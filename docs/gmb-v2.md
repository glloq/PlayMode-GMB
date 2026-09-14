# General-Midi-Boop v2 — automatic recognition

PlayMode answers General-Midi-Boop's discovery handshake and publishes a
capability descriptor describing what it can actually play. A Raspberry Pi
running General-Midi-Boop therefore needs **no manual instrument setup**: plug
the controller in, and the channels, playable notes, velocity response,
polyphony and timing arrive on their own.

The protocol is specified in General-Midi-Boop's `docs/SYSEX_IDENTITY.md`
(Instrument Recognition & Capability Protocol v2). PlayMode implements **level
1**: handshake *and* descriptor.

---

## 1. The one rule

```
MIDI note
    ↓
logical note
    ↓
configured actuator
    ↓
SERVO or SOLENOID
```

The descriptor describes the **musical** capability. Servo versus solenoid is an
implementation detail and never splits an instrument: a mechanical piano whose
keys are solenoids and whose sustain pedal is a servo is **one** instrument.
Actuator technology reaches the descriptor only where it genuinely changes a
musical capability — velocity response, note-off semantics, timing,
polyphony and power concurrency — and then through those fields, never as a
hardware inventory.

Everything is derived from the **active validated configuration**. There is no
separate GMB profile to maintain: edit a mapping in the web UI and the
descriptor follows.

---

## 2. Architecture

```
Active PlayMode configuration
            ↓
      CapabilityBuilder          gmb_capabilities.*
            ↓
     CapabilitySnapshot
            ↓
   cached GMB descriptor         gmb_descriptor.*, gmb_runtime.*
        ┌───┴────┐
        ↓        ↓
     SysEx      HTTP             gmb_sysex*.*, web_server.cpp
        ↓
General-Midi-Boop
```

| File | Responsibility |
|------|----------------|
| `gmb_protocol.h` | Wire constants, frame sizes, limits |
| `gmb_capabilities.*` | Behaviour classification + snapshot builder |
| `gmb_descriptor.*` | Snapshot → ASCII JSON, with graceful degradation |
| `gmb_gm_program.*` | GM program → GMB `type`/`subtype` vocabulary |
| `gmb_instance_id.*` | Stable per-chip identity |
| `gmb_revision.*` | Persistent revision + change detection |
| `gmb_sysex.*` | Frame codec (encode / decode only) |
| `gmb_sysex_service.*` | Handshake, chunked transfer, rate limiting |
| `gmb_runtime.*` | Descriptor cache, publish/pin, notifications |

The control plane runs entirely on the loop task. It never generates JSON in a
MIDI callback, never writes the filesystem from one, never allocates per MIDI
event, and never touches an actuator — a malformed or flooding SysEx peer
cannot disturb musical timing.

---

## 3. Block 0x01 — handshake

```
request   F0 7D 00 01 00 F7
response  F0 7D 00 01 01 02 <instance_id[5]> <firmware[3]>
          <descriptor_size[3]> <revision[5]> <flags> F7      (exactly 24 bytes)
```

`flags` bit 0 = `GET /gmb/descriptor.json` is reachable, bit 1 = the device
emits block 0x11. Both reflect what the device can do *right now* and are
refreshed as transports and the web server come and go. They are **not** part of
the descriptor, so they never churn the revision.

### instance_id

The pivot of the whole protocol: it binds a stored General-Midi-Boop
configuration to one physical exemplar. PlayMode folds the ESP32's factory
eFuse MAC into 32 bits (FNV-1a).

```
same ESP32 + reboot               → same id
same ESP32 + configuration change → same id
different ESP32                   → different id
```

It is deliberately **not** derived from the instrument name, MIDI channel,
configured notes, Wi-Fi SSID or firmware profile: all of those change while the
exemplar stays the same, which would silently unbind the saved setup.

---

## 4. Block 0x10 — descriptor transfer

```
request   F0 7D 00 10 00 <chunk_index[2]> F7
response  F0 7D 00 10 01 <total_chunks[2]> <chunk_index[2]> <payload…> F7
```

Payload is at most 200 bytes (210 per frame), ASCII-only JSON — already 7-bit
safe, so no packing. Segments may be requested in any order and retried.

**Snapshot consistency.** A transfer pins the buffer it is serving. If the
configuration changes mid-flight, the new descriptor is published into the other
buffer and the transfer — including a *retry of chunk 0* — keeps serving the
document it started with. Only the next transfer sees the new one. A transfer
that goes quiet for 5 s (the spec's `comm_timeout`) is abandoned and released.

An out-of-range chunk index, or a request while nothing is published, is
answered with silence rather than nonsense.

---

## 5. Block 0x11 — capability-change notification

```
F0 7D 00 11 02 <revision[5]> <change_flags> F7
```

Emitted on every transport that has a return path, and **only** when the
canonical descriptor hash actually moved. Flags: identity, instruments, timing.

---

## 6. Revision

The revision is an ETag. It advances only when the descriptor rendered from the
snapshot differs from the one on flash:

| Event | Revision |
|---|---|
| boot without a configuration change | unchanged, no flash write |
| effective capability change | +1, persisted |
| failed / invalid save | unchanged |
| setting that does not reach the descriptor | unchanged |

The last two need no special case: a setting that cannot reach the descriptor
cannot move its hash, and a save that never took effect cannot either. Duty
cycle, watchdog timeouts, rejection policy and PCA channel assignments are
invisible to the revision; a rename is not, because the descriptor declares it.

---

## 7. What is derived, and how

### Logical instruments

One descriptor entry per logical PlayMode instrument. Four instruments on one
ESP32 stay four entries. Two instruments **on the same MIDI channel** merge into
one entry — routing genuinely cannot tell them apart (a note on that channel
reaches both), and duplicate channels are rejected by GMB's own validator.

An Omni instrument is advertised on channel 0, where it really does answer.

### Playable notes

A note is announced only when **all** of these hold:

```
instrument enabled
+ valid MIDI channel
+ mapping enabled and in range
+ actuator exists
+ actuator enabled
+ effective behaviour can produce/control a note
+ configuration validation passed
```

The last one matters: a behaviour override that is invalid for the actuator type
(say `SERVO_TOUCHE` on a solenoid) matches no branch of the engine's behaviour
switch, so the note would silently do nothing — it is not announced.

Notes are deduplicated and sorted. A fully chromatic span becomes a range;
anything else a discrete list.

```
60 61 62 63 → {"mode":"range","min":60,"max":63}
60 62 64    → {"mode":"discrete","list":[60,62,64]}
```

### Behaviour classification

| PlayMode behaviour | Gesture | Role | NoteOff |
|---|---|---|---|
| `SERVO_FRAPPE` | strike | attack | irrelevant |
| `SERVO_ALTERNE` | alternate | attack | irrelevant |
| `SERVO_GRATTER` | strum | attack | irrelevant |
| `SERVO_TOUCHE` | key | hold | releases |
| `SOL_FRAPPE` | impulse | attack | self-terminating |
| `SOL_HIT_AND_HOLD` | hit-and-hold | hold | releases |

### Velocity

Announced only when it changes the **physical** result, not because NoteOn
carries a velocity byte:

* servo strike / strum — scales the amplitude, so it needs `amplitude > 0`;
* solenoid strike — scales the pulse duration, so it needs `pulse_min ≠ pulse_max`;
* alternate, key, hit-and-hold — the handlers ignore velocity;
* a velocity curve that flattens every input onto one output cancels it.

An instrument responds to velocity as soon as any of its notes does.

### Polyphony

The number of simultaneous notes the configured machine can really sustain —
never the number of note mappings. It is the count of independent actuators,
capped by whichever of these binds first:

* actuator sharing (two notes on one mechanism);
* the per-instrument concurrency cap;
* the global concurrency cap;
* the safety polyphony limit;
* the current budget, using the same per-actuator model as the runtime
  admission gate.

Constraints are declared alongside: `one_note_per_voice` when a mechanism is
shared (with the `voices` block naming which notes share it), and
`max_simultaneous_per_group` for a per-supply ceiling (`power_supply_0`,
`power_supply_1`) or the shared controller budget.

### Timing

**Unknown is omitted, never emitted as zero.**

PlayMode declares **no `prepare` phase**: a note is one commanded gesture that
*is* the excitation, with no separable silent preparation to anticipate. This is
exactly the `— / frappe` row the spec's conformity table gives PlayMode-GMB.

* `excite.latency_ms` — the largest measured actuator latency in the instrument.
  The dispatcher already delays faster actuators to match the slowest, so that
  maximum *is* the residual latency GMB must compensate. Omitted entirely while
  no latency has been measured or entered. A configured pulse duration or servo
  movement time is **never** promoted to acoustic latency.
* `rearticulation_ms` — the configured busy window (servo movement, solenoid
  pulse), floored by the rate limiter, which really does reject notes above
  `max_freq_hz`. Omitted when neither bounds it.
* `release_ms` — the configured key return movement. Hit-and-hold has no
  configured release ramp, so it stays unknown.

### Type, subtype and GM program

`type` / `subtype` are the textual keys of General-Midi-Boop's
`InstrumentTypeConfig.js`, which is strictly General MIDI aligned. PlayMode
therefore stores **one** musical field per logical instrument — a GM program —
and derives both keys from it. Pick "Xylophone" in the instrument dialog and the
descriptor declares `chromatic_percussion` / `xylophone` / program 13.

With no profile chosen, the descriptor declares the generic supported type
`chromatic_percussion` and omits `subtype` and `gm_program` entirely. Nothing is
ever inferred from actuator topology: a solenoid does not imply a xylophone.

### Configured state

`"configured": false` keeps the slot visible so GMB recognises the instrument
without overwriting a previous manual configuration, for a slot that is
disabled, unmapped, on an invalid channel, capped at zero voices, or part of a
configuration that failed validation.

---

## 8. Transports

| Transport | Automatic recognition | Why |
|---|---|---|
| RTP-MIDI (AppleMIDI) | yes | bidirectional session |
| WiFi UDP | yes | the reply goes back to the datagram's sender |
| DIN with MIDI OUT | yes | set the TX GPIO on the MIDI page |
| DIN, IN only | no | no return path — select the instrument by hand |

GMB is transport-independent: a complete SysEx frame reaches the same service
whatever carried it, and the reply leaves on the same transport. Normal
NoteOn / NoteOff / CC handling is untouched.

The spec marks RTP-MIDI discovery as degraded on the GMB side and recommends the
HTTP flag; PlayMode raises that flag whenever its web server is up.

---

## 9. HTTP

| Endpoint | Purpose |
|---|---|
| `GET /gmb/descriptor.json` | The descriptor — byte-identical to the SysEx transfer |
| `GET /api/gmb/status` | Diagnostics (not required for the protocol) |

`/gmb/descriptor.json` is read-only and unauthenticated: it is the path GMB
follows when the handshake raises flag bit 0. It carries `X-GMB-Revision`.

---

## 10. Limits and protection

* Descriptor cache: 6 KiB, two buffers (publish + pinned transfer).
* Oversized configurations **degrade optional detail** — voices, then the CC
  list and `physical`, then timing and constraints — and never emit truncated
  JSON. `/api/gmb/status` reports the degradation.
* SysEx rate limiting per rolling second: 8 handshakes, 64 chunks, 8 invalid
  frames. Notes and CC are never rate-limited.
* A malformed frame — wrong manufacturer, wrong device id, wrong direction,
  unknown block, truncated, oversized, or carrying an 8-bit payload byte —
  produces no reply and no actuator movement.

---

## 11. Tests

`make -C test` builds and runs the native suite (no ESP32 toolchain needed; it
also runs in CI). It covers the handshake byte layout, instance-id stability,
logical instrument grouping, note derivation, servo/solenoid mixing, velocity,
polyphony, timing, chunked transfer and snapshot consistency, revision
behaviour, SysEx capture in the MIDI parser, and malformed-frame robustness.
