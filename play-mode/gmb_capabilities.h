#ifndef GMB_CAPABILITIES_H
#define GMB_CAPABILITIES_H

#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "gmb_protocol.h"

// ============================================================================
// PlayMode — GMB capability classification + snapshot builder
// ============================================================================
//
// The architectural rule PlayMode exposes to General-Midi-Boop:
//
//     MIDI note -> logical note -> configured actuator -> SERVO or SOLENOID
//
// GMB describes the MUSICAL capability. Servo vs solenoid is an implementation
// detail and is NEVER a reason to split a logical instrument: a mechanical
// piano whose keys are solenoids and whose pedal is a servo is one instrument.
// Actuator technology only reaches the descriptor where it genuinely changes a
// musical capability (velocity response, note-off semantics, timing,
// polyphony, power concurrency) — and then it does so through those fields,
// not as a hardware inventory.
//
// This header is the capability-classification layer the serializer needs, so
// no behaviour switch-statement is duplicated inside the JSON writer.
//

// --- Musical classification of a PlayMode behaviour -------------------------

// What the gesture does to a note, independent of the actuator technology.
enum class GmbActionRole : uint8_t {
    NOTE_ATTACK,    // produces the note; self-terminating, NoteOff irrelevant
    NOTE_HOLD,      // produces AND sustains it; NoteOff releases
    NOTE_RELEASE,   // release-only gesture (no PlayMode behaviour maps here yet)
    AUXILIARY       // not a note-producing action (CC positioning, tests)
};

// PlayMode behaviour, named musically.
enum class GmbGesture : uint8_t {
    STRIKE,        // SERVO_FRAPPE      — servo swings out and returns
    ALTERNATE,     // SERVO_ALTERNE     — servo toggles A <-> B
    STRUM,         // SERVO_GRATTER     — servo sweeps +/- alternately
    KEY,           // SERVO_TOUCHE      — servo presses on NoteOn, releases on NoteOff
    IMPULSE,       // SOL_FRAPPE        — solenoid pulse, self-terminating
    HIT_AND_HOLD,  // SOL_HIT_AND_HOLD  — solenoid attack then sustained hold
    INVALID        // behaviour is not valid for this actuator type
};

struct GmbBehaviourCaps {
    GmbGesture    gesture;
    GmbActionRole role;
    bool          velocity_capable;   // the gesture reads NoteOn velocity at all
    bool          self_terminating;   // returns/releases without a NoteOff
    bool          note_off_releases;  // a NoteOff genuinely releases the note
};

// Classify (actuator type, effective behaviour). An out-of-range behaviour for
// the type yields GmbGesture::INVALID — the engine's behaviour switch would not
// match it either, so such a mapping is not playable.
GmbBehaviourCaps gmbClassify(ActuatorType type, uint8_t behaviour);

// True when the behaviour is one the engine can actually execute for this type.
bool gmbBehaviourValid(ActuatorType type, uint8_t behaviour);

// True when velocity changes the PHYSICAL result for this actuator, not merely
// because MIDI NoteOn carries a velocity byte:
//   SERVO_FRAPPE / SERVO_GRATTER — velocity scales the strike amplitude, so it
//                                  is effective only when amplitude > 0;
//   SOL_FRAPPE                   — velocity scales the pulse duration, so it is
//                                  effective only when min and max differ;
//   everything else              — the handler ignores event.velocity.
bool gmbVelocityEffective(const ActuatorConfig& act, uint8_t behaviour);

// Configured "busy window": how long the mechanism is committed after an
// attack before the same note can be re-articulated. 0 = not derivable.
uint16_t gmbBusyWindowMs(const ActuatorConfig& act, uint8_t behaviour);

// Configured release duration for a held note. 0 = unknown (NOT "instant").
uint16_t gmbReleaseMs(const ActuatorConfig& act, uint8_t behaviour);

// --- Capability snapshot ----------------------------------------------------

struct GmbConstraint {
    // Only `max_simultaneous_per_group` and `one_note_per_voice` are produced;
    // both are spec §5.5 constraint types.
    bool     per_group;          // false => one_note_per_voice
    char     group[16];
    uint8_t  max;
};

// One GMB descriptor instrument entry, derived from one or more PlayMode
// logical instruments (they merge only when MIDI routing genuinely cannot tell
// them apart — see gmbBuildSnapshot).
struct GmbInstrumentCaps {
    bool     present;
    bool     configured;
    uint8_t  channel;                 // descriptor channel 0..15
    bool     omni;                    // a contributor listened on every channel
    char     name[32];

    uint8_t  gm_program;              // GMB_GM_PROGRAM_NONE = unknown
    const char* type;                 // never null
    const char* subtype;              // null/"" = unknown

    uint8_t  note_count;
    uint8_t  notes[GMB_MAX_NOTES];          // sorted, deduplicated
    uint8_t  note_actuator[GMB_MAX_NOTES];  // voice grouping key
    bool     contiguous;                    // every semitone min..max playable

    bool     velocity;
    bool     has_attack_notes;
    bool     has_hold_notes;

    uint8_t  distinct_actuators;
    bool     shared_actuators;        // one actuator serves several notes
    uint8_t  polyphony_max;

    uint8_t  constraint_count;
    GmbConstraint constraints[GMB_MAX_CONSTRAINTS];

    bool     has_excite_latency;  uint16_t excite_latency_ms;
    bool     has_rearticulation;  uint16_t rearticulation_ms;
    bool     has_release;         uint16_t release_ms;

    uint8_t  cc_count;
    uint8_t  ccs[GMB_MAX_CCS];

    uint8_t  source_count;            // PlayMode instruments merged here
    uint8_t  sources[MAX_INSTRUMENTS];
};

struct GmbCapabilitySnapshot {
    char     device_name[32];
    uint8_t  instrument_count;
    GmbInstrumentCaps instruments[MAX_INSTRUMENTS];

    // Diagnostics (not serialized as capabilities).
    uint16_t playable_note_total;
    uint16_t rejected_mapping_total;  // mappings dropped as unplayable
};

// Everything the builder reads. Deliberately plain pointers into the ACTIVE
// validated configuration so the snapshot is always derived from it — there is
// no separate, hand-maintained GMB profile anywhere.
struct GmbBuildInput {
    const ActuatorConfig*    actuators;
    uint8_t                  actuator_count;
    const InstrumentConfig*  instruments;
    uint8_t                  instrument_count;
    const MidiRoutingConfig* routings;
    uint8_t                  routing_count;
    const PowerBudget*       power;       // may be null
    const SafetyLimits*      safety;      // may be null
    const char*              device_name; // may be null
    bool                     config_valid; // active configuration passed validation
};

// Build an immutable capability snapshot from the active configuration.
void gmbBuildSnapshot(const GmbBuildInput& in, GmbCapabilitySnapshot& out);

// Compact fingerprint of a snapshot's musically meaningful fields. Only used
// to classify a change into block 0x11 change_flags; the revision itself is
// driven by the canonical descriptor hash. Keeping a digest instead of a whole
// previous snapshot keeps the runtime's static footprint small.
struct GmbInstrumentDigest {
    uint8_t  channel;
    uint8_t  configured;
    uint8_t  velocity;
    uint8_t  polyphony_max;
    uint8_t  gm_program;
    uint8_t  cc_count;
    uint8_t  constraint_count;
    uint8_t  has_excite_latency;
    uint8_t  has_rearticulation;
    uint8_t  has_release;
    uint16_t excite_latency_ms;
    uint16_t rearticulation_ms;
    uint16_t release_ms;
    uint32_t notes_hash;
};

struct GmbSnapshotDigest {
    uint32_t            device_name_hash;
    uint8_t             instrument_count;
    GmbInstrumentDigest instruments[MAX_INSTRUMENTS];
};

void gmbDigest(const GmbCapabilitySnapshot& snap, GmbSnapshotDigest& out);

uint8_t gmbChangeFlags(const GmbSnapshotDigest& before, const GmbSnapshotDigest& after);

#endif // GMB_CAPABILITIES_H
