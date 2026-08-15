# Web Interface

> See also: [REST API](api-rest.md) — [Architecture](architecture.md) — [Calibration & Tests](calibration-tests.md)

## 1. Main UI Goals

* **Immediate clarity** → user understands what they can do from the main page
* **No-code / adaptable** → easily add/modify actuators, instruments, CC
* **Real-time monitoring** → active notes, latencies, buses, PCA, safety state
* **First-use guidance** → Welcome page with 3-step guide

---

# 2. Interface Structure (implemented)

The UI is a single-page application embedded in PROGMEM (~3000 lines of vanilla HTML/CSS/JS).
Dark GitHub-style theme (#0d1117 bg, #58a6ff accent, #3fb950 success, #f85149 error).

### 2.1 Navigation

4 main tabs + Calibration (conditional) + Settings (gear icon):

| Page | ID | Visible by default | Description |
|------|----|--------------------|-------------|
| **Welcome** | `page-welcome` | If 0 instruments | 3-step guide: Create → Connect → Play |
| **Instrument** | `page-instrument` | Yes (default) | Instrument management + virtual pianos |
| **MIDI** | `page-midi` | Yes | MIDI transports + real-time messages |
| **Actuators** | `page-actuators` | Yes | Actuator table + CC routing |
| **Wiring** | `page-wiring` | Yes | Electrical diagram generated from the config |
| **Calibration** | `page-calibration` | No (conditional) | Acoustic calibration + tests |
| **Settings** | `page-settings` | Via gear icon | Monitoring, safety, logs, WiFi, config |

### 2.2 Welcome Page (first-run)

* Automatically shown if no instruments exist
* Midi B∞p logo centered in the header
* 3 illustrated steps: Create → Connect → Play
* Main button "Create my first instrument" → launches 4-step wizard
* Secondary link "skip to manual configuration"
* Disappears once an instrument is created

### 2.3 Instrument Page

* **Instrument list**: table (name, channel, type, actuators, state, actions)
* **Creation**:
  * "Wizard" button → 4-step assistant (identity, type, MIDI notes, summary)
  * "+ Manual" button → full modal
* **Editing**: modal with all parameters
* **Deletion**: confirmation via themed `appConfirm()`
* **Virtual piano(s)**: one interactive keyboard per instrument
  * Visual MIDI notes, touch scrolling
  * Real-time feedback on active notes

### 2.4 MIDI Page

* **MIDI transports**: 3 dedicated cards with toggle + status:
  * MIDI Cable (DIN / TRS) — Serial 31250 baud, GPIO 4
  * WiFi — raw UDP (port 5004)
  * WiFi — Apple / RTP-MIDI (AppleMIDI, synchronized)
* **Jitter buffer**: slider 10–80 ms (default 30 ms)
  * Help text: "Anti-jitter buffer for network MIDI"
* **Received MIDI messages**: real-time table (type, channel, note, velocity, source, timestamp)
  * Pause / clear buttons

### 2.5 Actuators Page

* **Complete table**: ID, type (servo/solenoid), MIDI note, bus, PCA, channel, mode, state
* **Editing**: modal with dynamic parameters based on type and behavior
  * Servo: initial angle, amplitude, speed, angle B, strike direction
  * Solenoid: min/max duration, initial PWM, hold PWM, ramp
* **CC Routing**: Control Changes table per instrument
  * CC number, target actuator, target parameter (position/amplitude/speed/PWM hold)

### 2.6 Wiring Page

Everything on this page is **generated from the live configuration** — there is no
static picture to keep in sync. It reads `GET /api/buses`, `GET /api/actuators` and
`GET /api/power`, then draws the machine as it is actually configured.

* **SVG diagram** (built in JS, no library):
  * ESP32 block with the exact GPIO of every signal it drives
  * Actuator supply block (one V+ rail per bus + common GND)
  * One band per I²C bus: SDA / SCL / /OE / V+ / GND rails and the PCA9685 boards
    hanging off them, each board showing its address and its 16 channels
    (blue = servo, amber = solenoid, grey = free, red = conflict, faded = disabled).
    Hovering a channel shows the actuator it drives.
  * Inputs band: MIDI IN (opto-coupler → RX), optional INMP441 I²S mic, status LED
  * **Download SVG** button — colours are written as SVG attributes, so the exported
    file renders identically outside the UI (print it for the workbench)
* **Summary cards**: actuator count, boards and channels used, worst-case peak
  current vs. the energy budget, idle draw
* **Wiring checks** — live validation, each with the fix to apply:
  * two actuators on the same PCA channel
  * a board used by an actuator but not declared on its bus
  * servos on a bus running above ~130 Hz, solenoids on a 50 Hz bus
  * servos and solenoids mixed on one bus (a PCA9685 has a single PWM frequency)
  * actuators on a disabled or unknown bus, more than 4 boards per bus
  * worst-case draw above the configured energy budget
* **Pinout table**: bus pins read from the device + the compile-time pins
  (MIDI RX, status LED, I²S mic) with their wiring notes
* **Power distribution**: supply sizing computed from the actuator mix, plus the
  fixed rules (separate rails, star ground, bulk capacitors, fuses, flyback diodes,
  /OE as hardware kill switch)
* **Commissioning checklist**: staged power-up before the first note

### 2.7 Calibration Page (conditional)

* Tab hidden by default (`#nav-cal` with `display:none`)
* **Acoustic calibration**: I²S mic INMP441, progress, latency results
* **Actuator tests**:
  * Sweep: sequential scan of all actuators
  * Burst: rapid fire on a specific actuator
  * Stress: simultaneous maximum load
* Test event log (64 entries)

### 2.8 Settings Page

* **Monitoring**: 4 real-time cards
  * MIDI: received/routed/rejected messages
  * Polyphony: progress bar + active/rejected
  * Scheduler: queued/processed events
  * WiFi: IP, mode (STA/AP), signal
* **Polyphony & Safety**:
  * Max polyphony (single shared input)
  * Kill Switch (toggle)
  * Graceful degradation
  * Advanced limits (collapsible): duty cycle, frequency, watchdog, current
* **System Log**:
  * Logs filterable by level (DEBUG, INFO, WARN, ERROR, CRITICAL)
  * Filter by category (System, MIDI, Scheduler, Safety, Power, Calibration, Test)
  * Auto-scroll, clear button
* **WiFi Connection**: SSID, password, hostname, AP fallback
* **I²C Bus** (advanced, collapsible): configurable PWM frequency per bus
* **Configuration**: LittleFS flash save, reset to defaults

---

# 3. Modals and Dialogs

* **`appConfirm()`**: replaces all native `confirm()`
  * Integrated dark theme design
  * Contextual icons, danger/primary support
  * Customizable button text
* **`appAlert()`**: replaces all native `alert()`
  * Same themed style
* **Edit modals**: instrument, actuator, CC mapping
* **Wizard**: multi-step modal with visual progress

---

# 4. Real-time Communication

* **WebSocket** (`/ws`): full state broadcast every 200 ms
  * Scheduler: queue, processed events
  * MIDI: serial/UDP/RTP counters, routed/rejected messages
  * Safety: estimated current, active count, kill switch, degradation
  * Power: used budget (%), servo/solenoid bus
  * Actuators: active states
* **Toasts**: temporary notifications (green success, red error, yellow warning)
* **Piano feedback**: active notes highlighted

---

# 5. Backend REST API

38+ endpoints organized in 4 HTTP verbs (GET, POST, DELETE) + WebSocket.
See [api-rest.md](api-rest.md) for the full endpoint list.

Backend unchanged by UI refactoring — all API routes are identical.

---

# 6. UX / Ergonomics

* **Clean**: 4 main tabs, no nested menus
* **Real-time feedback**: virtual piano + actuator indicators + monitoring cards
* **Guidance**: automatic Welcome page on first boot
* **Modularity**: add instruments / actuators via UI, no recompilation needed
* **Responsiveness**: adaptive interface for mobile / tablet / desktop
* **Colors and visual codes**: active buses, running actuators, safety alerts
* **Dark theme**: comfortable viewing, consistent GitHub-style palette
