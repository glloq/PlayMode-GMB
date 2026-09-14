#include "gmb_capabilities.h"
#include "gmb_gm_program.h"

// ============================================================================
// PlayMode — GMB capability classification + snapshot builder
// ============================================================================

// ----------------------------------------------------------------------------
// Behaviour classification
// ----------------------------------------------------------------------------

GmbBehaviourCaps gmbClassify(ActuatorType type, uint8_t behaviour) {
    GmbBehaviourCaps c = {GmbGesture::INVALID, GmbActionRole::AUXILIARY,
                          false, false, false};
    if (type == ACT_SERVO) {
        switch (behaviour) {
            case SERVO_FRAPPE:
                c = {GmbGesture::STRIKE,    GmbActionRole::NOTE_ATTACK, true,  true,  false};
                break;
            case SERVO_ALTERNE:
                // Discrete A/B move: the note is produced by the movement, the
                // servo then simply stays where it is. No NoteOff semantics.
                c = {GmbGesture::ALTERNATE, GmbActionRole::NOTE_ATTACK, false, true,  false};
                break;
            case SERVO_GRATTER:
                c = {GmbGesture::STRUM,     GmbActionRole::NOTE_ATTACK, true,  true,  false};
                break;
            case SERVO_TOUCHE:
                c = {GmbGesture::KEY,       GmbActionRole::NOTE_HOLD,   false, false, true};
                break;
            default: break;
        }
    } else if (type == ACT_SOLENOID) {
        switch (behaviour) {
            case SOL_FRAPPE:
                c = {GmbGesture::IMPULSE,      GmbActionRole::NOTE_ATTACK, true,  true,  false};
                break;
            case SOL_HIT_AND_HOLD:
                c = {GmbGesture::HIT_AND_HOLD, GmbActionRole::NOTE_HOLD,   false, false, true};
                break;
            default: break;
        }
    }
    return c;
}

bool gmbBehaviourValid(ActuatorType type, uint8_t behaviour) {
    return gmbClassify(type, behaviour).gesture != GmbGesture::INVALID;
}

bool gmbVelocityEffective(const ActuatorConfig& act, uint8_t behaviour) {
    GmbBehaviourCaps c = gmbClassify(act.type, behaviour);
    if (!c.velocity_capable) return false;

    if (act.type == ACT_SERVO) {
        // velocityToAmplitude() maps 0..127 onto 0..amplitude: a zero amplitude
        // makes every velocity produce the same (null) movement.
        return act.amplitude > 0;
    }

    // SOL_FRAPPE maps velocity onto the pulse duration. Mirror the engine's own
    // fallbacks and min/max swap so the answer matches what really happens.
    uint16_t min_pulse = (act.pulse_min_ms > 0) ? act.pulse_min_ms : SOLENOID_MIN_PULSE_MS;
    uint16_t max_pulse = (act.pulse_ms     > 0) ? act.pulse_ms     : SOLENOID_MAX_PULSE_MS;
    return min_pulse != max_pulse;
}

uint16_t gmbBusyWindowMs(const ActuatorConfig& act, uint8_t behaviour) {
    GmbBehaviourCaps c = gmbClassify(act.type, behaviour);
    switch (c.gesture) {
        case GmbGesture::STRIKE:
        case GmbGesture::ALTERNATE:
        case GmbGesture::STRUM:
            // The engine commands the return `speed_ms` after the attack; a new
            // attack before then cancels it. This is a CONFIGURED movement
            // duration, not a measured acoustic re-strike interval.
            return act.speed_ms;
        case GmbGesture::IMPULSE: {
            uint16_t max_pulse = (act.pulse_ms > 0) ? act.pulse_ms : SOLENOID_MAX_PULSE_MS;
            return max_pulse;   // coil is energised for at most this long
        }
        default:
            // KEY / HIT_AND_HOLD have no self-terminating cycle: their duration
            // is decided by the NoteOff, so no busy window can be derived.
            return 0;
    }
}

uint16_t gmbReleaseMs(const ActuatorConfig& act, uint8_t behaviour) {
    GmbBehaviourCaps c = gmbClassify(act.type, behaviour);
    if (c.gesture == GmbGesture::KEY) {
        // The key travels back over the configured servo movement duration.
        return act.speed_ms;
    }
    // HIT_AND_HOLD de-energises the coil with no configured release ramp — the
    // mechanical fall time is unmeasured, so it stays UNKNOWN (never 0).
    return 0;
}

// ----------------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------------

static const ActuatorConfig* findActuator(const GmbBuildInput& in, uint8_t id) {
    for (uint8_t i = 0; i < in.actuator_count && i < MAX_ACTUATORS; i++) {
        if (in.actuators[i].id == id) return &in.actuators[i];
    }
    return nullptr;
}

// The dispatcher resolves a routing by its instrument_index FIELD, not by array
// position — mirror that exactly.
static const MidiRoutingConfig* findRouting(const GmbBuildInput& in, uint8_t inst_idx) {
    for (uint8_t i = 0; i < in.routing_count && i < MAX_INSTRUMENTS; i++) {
        if (in.routings[i].instrument_index == inst_idx) return &in.routings[i];
    }
    return nullptr;
}

// Worst-case admission current for one simultaneous activation of `act`,
// using the same model as ResourceManager::estimateCurrent(assume_active).
static uint16_t worstCaseMa(const ActuatorConfig& act) {
    if (act.type == ACT_SERVO) return POWER_SERVO_ACTIVE_MA;
    return POWER_SOLENOID_FULL_MA;   // full power at velocity 127, and for hold
}

// Does this instrument's velocity curve flatten every input onto one output?
// (A single point always returns that point; several identical outputs do too.)
static bool velocityCurveIsFlat(const MidiRoutingConfig* routing) {
    if (routing == nullptr || routing->velocity_curve_count == 0) return false;
    uint8_t first = routing->velocity_curve[0].output;
    for (uint8_t i = 1; i < routing->velocity_curve_count &&
                        i < VELOCITY_CURVE_POINTS; i++) {
        if (routing->velocity_curve[i].output != first) return false;
    }
    return true;   // count == 1 lands here too, which is correct
}

static void addConstraint(GmbInstrumentCaps& e, const char* group, uint8_t max) {
    if (e.constraint_count >= GMB_MAX_CONSTRAINTS) return;
    GmbConstraint& c = e.constraints[e.constraint_count++];
    c.per_group = (group != nullptr);
    c.max = max;
    c.group[0] = '\0';
    if (group != nullptr) strlcpy(c.group, group, sizeof(c.group));
}

// Insert `note` (played by `actuator_id`) keeping the list sorted and unique.
// Returns true when the note was newly added.
static bool insertNote(GmbInstrumentCaps& e, uint8_t note, uint8_t actuator_id) {
    uint8_t pos = 0;
    while (pos < e.note_count && e.notes[pos] < note) pos++;
    if (pos < e.note_count && e.notes[pos] == note) return false;  // duplicate
    if (e.note_count >= GMB_MAX_NOTES) return false;
    for (uint8_t i = e.note_count; i > pos; i--) {
        e.notes[i] = e.notes[i - 1];
        e.note_actuator[i] = e.note_actuator[i - 1];
    }
    e.notes[pos] = note;
    e.note_actuator[pos] = actuator_id;
    e.note_count++;
    return true;
}

static void addCC(GmbInstrumentCaps& e, uint8_t cc) {
    for (uint8_t i = 0; i < e.cc_count; i++) if (e.ccs[i] == cc) return;
    if (e.cc_count >= GMB_MAX_CCS) return;
    e.ccs[e.cc_count++] = cc;
}

// ----------------------------------------------------------------------------
// Snapshot construction
// ----------------------------------------------------------------------------

// Per-contributor accumulators that must not leak into the merged entry.
struct ContribTotals {
    bool     has_polyphony_cap;      // the power budget declares caps at all
    uint16_t polyphony_cap_sum;      // sum of the contributors' per-instrument caps
    uint16_t worst_ma;               // worst per-activation draw, any bus
    uint16_t worst_ma_bus[2];
    uint8_t  actuators_on_bus[2];
};

void gmbBuildSnapshot(const GmbBuildInput& in, GmbCapabilitySnapshot& out) {
    memset(&out, 0, sizeof(out));
    strlcpy(out.device_name,
            (in.device_name != nullptr && in.device_name[0] != '\0')
                ? in.device_name : "PlayMode",
            sizeof(out.device_name));

    // Distinct actuator ids contributed to each descriptor entry, so a merge
    // never double-counts a shared actuator.
    static uint8_t entry_actuators[MAX_INSTRUMENTS][MAX_ACTUATORS];
    static uint8_t entry_actuator_count[MAX_INSTRUMENTS];
    static ContribTotals totals[MAX_INSTRUMENTS];
    memset(entry_actuator_count, 0, sizeof(entry_actuator_count));
    memset(totals, 0, sizeof(totals));

    uint8_t inst_count = in.instrument_count;
    if (inst_count > MAX_INSTRUMENTS) inst_count = MAX_INSTRUMENTS;

    for (uint8_t i = 0; i < inst_count; i++) {
        const InstrumentConfig& inst = in.instruments[i];

        // A MIDI channel PlayMode cannot listen on makes the slot unplayable,
        // but the slot is still reported so GMB recognises it and keeps any
        // previous manual configuration (spec §5.1).
        bool omni = (inst.midi_channel == MIDI_CHANNEL_OMNI_INTERNAL);
        bool channel_valid = omni || inst.midi_channel < MIDI_CHANNEL_COUNT;

        // An Omni instrument answers on every channel; GMB has no Omni
        // representation, so it is advertised on channel 0 (where it really
        // does answer). Explicit channels are used as-is.
        uint8_t desc_channel = omni ? 0 : (channel_valid ? inst.midi_channel : 0);

        // Two PlayMode instruments on one MIDI channel cannot be told apart by
        // routing — a note on that channel reaches BOTH — so they must become
        // ONE descriptor entry rather than a duplicate-channel descriptor that
        // GMB would reject.
        int8_t slot = -1;
        for (uint8_t s = 0; s < out.instrument_count; s++) {
            if (out.instruments[s].channel == desc_channel) { slot = (int8_t)s; break; }
        }
        if (slot < 0) {
            if (out.instrument_count >= MAX_INSTRUMENTS) continue;
            slot = (int8_t)out.instrument_count++;
            GmbInstrumentCaps& fresh = out.instruments[slot];
            memset(&fresh, 0, sizeof(fresh));
            fresh.present = true;
            fresh.channel = desc_channel;
            fresh.gm_program = GMB_GM_PROGRAM_NONE;
            fresh.type = GMB_GENERIC_TYPE;
            fresh.subtype = nullptr;
        }
        GmbInstrumentCaps& e = out.instruments[slot];
        ContribTotals& t = totals[slot];

        if (e.source_count < MAX_INSTRUMENTS) e.sources[e.source_count++] = i;
        if (omni) e.omni = true;
        if (e.name[0] == '\0' && inst.name[0] != '\0') {
            strlcpy(e.name, inst.name, sizeof(e.name));
        }

        const MidiRoutingConfig* routing = findRouting(in, i);

        // ---- playable notes -------------------------------------------------
        // A note is playable only when the instrument is enabled, the mapping is
        // valid and enabled, the actuator exists and is enabled, and the
        // effective behaviour can actually produce/control a note.
        uint8_t  contributed = 0;
        bool     any_velocity = false;
        bool     any_attack = false, any_hold = false;
        uint16_t max_latency = 0;
        uint16_t max_busy = 0;
        uint16_t max_release = 0;
        bool     curve_flat = velocityCurveIsFlat(routing);

        if (inst.enabled && channel_valid && routing != nullptr) {
            uint8_t map_count = routing->note_map_count;
            if (map_count > MAX_NOTE_MAPPINGS) map_count = MAX_NOTE_MAPPINGS;
            for (uint8_t m = 0; m < map_count; m++) {
                const NoteMapping& nm = routing->note_map[m];
                if (!nm.enabled) { out.rejected_mapping_total++; continue; }
                if (nm.midi_note > 127) { out.rejected_mapping_total++; continue; }

                const ActuatorConfig* act = findActuator(in, nm.actuator_id);
                if (act == nullptr || !act->enabled) {
                    out.rejected_mapping_total++;
                    continue;
                }
                uint8_t behaviour = (nm.behavior_override != 0xFF)
                                  ? nm.behavior_override : act->behavior;
                if (!gmbBehaviourValid(act->type, behaviour)) {
                    // The engine's behaviour switch would match no case: the
                    // note would silently do nothing.
                    out.rejected_mapping_total++;
                    continue;
                }

                bool added = insertNote(e, nm.midi_note, act->id);
                if (added) contributed++;

                GmbBehaviourCaps caps = gmbClassify(act->type, behaviour);
                if (caps.role == GmbActionRole::NOTE_HOLD) any_hold = true;
                else                                       any_attack = true;
                if (!curve_flat && gmbVelocityEffective(*act, behaviour)) any_velocity = true;

                if (act->latency_ms > max_latency) max_latency = act->latency_ms;
                uint16_t busy = gmbBusyWindowMs(*act, behaviour);
                if (busy > max_busy) max_busy = busy;
                uint16_t rel = gmbReleaseMs(*act, behaviour);
                if (rel > max_release) max_release = rel;

                // Distinct actuators (for voices / polyphony / power groups).
                bool known = false;
                for (uint8_t k = 0; k < entry_actuator_count[slot]; k++) {
                    if (entry_actuators[slot][k] == act->id) { known = true; break; }
                }
                if (!known && entry_actuator_count[slot] < MAX_ACTUATORS) {
                    entry_actuators[slot][entry_actuator_count[slot]++] = act->id;
                    uint16_t ma = worstCaseMa(*act);
                    if (ma > t.worst_ma) t.worst_ma = ma;
                    if (act->bus_id < 2) {
                        t.actuators_on_bus[act->bus_id]++;
                        if (ma > t.worst_ma_bus[act->bus_id]) t.worst_ma_bus[act->bus_id] = ma;
                    }
                }
            }
        }

        if (contributed > 0) {
            e.configured = true;
            if (any_velocity) e.velocity = true;
            if (any_attack)   e.has_attack_notes = true;
            if (any_hold)     e.has_hold_notes = true;
            if (max_latency > 0 &&
                (!e.has_excite_latency || max_latency > e.excite_latency_ms)) {
                e.has_excite_latency = true;
                e.excite_latency_ms = max_latency;
            }
            if (max_busy > e.rearticulation_ms) e.rearticulation_ms = max_busy;
            if (max_release > 0 &&
                (!e.has_release || max_release > e.release_ms)) {
                e.has_release = true;
                e.release_ms = max_release;
            }

            // Per-instrument polyphony cap. A cap of 0 makes the ResourceManager
            // reject EVERY activation for that instrument, so it contributes no
            // simultaneous voice at all.
            if (in.power != nullptr) {
                t.has_polyphony_cap = true;
                t.polyphony_cap_sum += in.power->instrument_max_polyphony[i];
            }

            // A configured instrument declares the musical profile for the
            // entry. The first contributor that has one wins.
            if (e.gm_program == GMB_GM_PROGRAM_NONE && inst.gm_program <= 127) {
                e.gm_program = inst.gm_program;
                e.type    = gmbTypeForProgram(inst.gm_program);
                e.subtype = gmbSubtypeForProgram(inst.gm_program);
            }
            if (e.name[0] == '\0' && inst.name[0] != '\0') {
                strlcpy(e.name, inst.name, sizeof(e.name));
            }
        }

        // ---- declared controls ---------------------------------------------
        if (inst.enabled && channel_valid && routing != nullptr) {
            uint8_t cc_count = routing->cc_map_count;
            if (cc_count > MAX_CC_MAPPINGS) cc_count = MAX_CC_MAPPINGS;
            for (uint8_t c = 0; c < cc_count; c++) {
                const CCMapping& cm = routing->cc_map[c];
                if (!cm.enabled || cm.cc_number > 127) continue;
                const ActuatorConfig* act = findActuator(in, cm.actuator_id);
                if (act == nullptr || !act->enabled) continue;
                // CC_TARGET_POSITION is servo-only in the dispatcher; a solenoid
                // target is a configuration error the runtime ignores.
                if (cm.target == CC_TARGET_POSITION && act->type != ACT_SERVO) continue;
                addCC(e, cm.cc_number);
            }
        }
    }

    // ---- per-entry derived values -------------------------------------------
    uint32_t servo_idle_total = 0;
    uint32_t servo_idle_bus[2] = {0, 0};
    for (uint8_t a = 0; a < in.actuator_count && a < MAX_ACTUATORS; a++) {
        const ActuatorConfig& act = in.actuators[a];
        if (!act.enabled || act.type != ACT_SERVO) continue;
        servo_idle_total += POWER_SERVO_IDLE_MA;
        if (act.bus_id < 2) servo_idle_bus[act.bus_id] += POWER_SERVO_IDLE_MA;
    }

    uint8_t configured_entries = 0;
    for (uint8_t s = 0; s < out.instrument_count; s++) {
        if (out.instruments[s].configured) configured_entries++;
    }

    for (uint8_t s = 0; s < out.instrument_count; s++) {
        GmbInstrumentCaps& e = out.instruments[s];
        ContribTotals& t = totals[s];

        // The configuration as a whole failed validation: nothing is trusted as
        // playable, but the slots stay so GMB does not overwrite prior setup.
        if (!in.config_valid) e.configured = false;

        // Every contributor is capped at zero simultaneous voices: the runtime
        // admission gate rejects every note, so the entry is not playable.
        if (e.configured && t.has_polyphony_cap && t.polyphony_cap_sum == 0) {
            e.configured = false;
        }

        if (!e.configured) {
            e.note_count = 0;
            e.velocity = false;
            e.polyphony_max = 0;
            e.distinct_actuators = 0;
            e.shared_actuators = false;
            e.contiguous = false;
            e.has_attack_notes = e.has_hold_notes = false;
            e.has_excite_latency = e.has_release = e.has_rearticulation = false;
            e.excite_latency_ms = e.release_ms = e.rearticulation_ms = 0;
            e.cc_count = 0;
            e.constraint_count = 0;
            continue;
        }

        out.playable_note_total += e.note_count;
        e.distinct_actuators = entry_actuator_count[s];
        e.shared_actuators = (e.distinct_actuators < e.note_count);

        // Contiguity decides range vs discrete: only a fully chromatic span may
        // be announced as a range.
        e.contiguous = true;
        for (uint8_t i = 1; i < e.note_count; i++) {
            if (e.notes[i] != (uint8_t)(e.notes[i - 1] + 1)) { e.contiguous = false; break; }
        }

        // ---- polyphony ------------------------------------------------------
        // Never the number of note mappings: it is the number of simultaneous
        // notes the CONFIGURED machine can really sustain.
        uint16_t poly = e.distinct_actuators;
        if (t.has_polyphony_cap && t.polyphony_cap_sum < poly) poly = t.polyphony_cap_sum;
        if (in.power != nullptr) {
            if (in.power->global_max_polyphony > 0 &&
                in.power->global_max_polyphony < poly) {
                poly = in.power->global_max_polyphony;
            }
            // Global current budget, using the runtime's own admission model:
            // parked servos draw idle current permanently, the rest is headroom.
            if (t.worst_ma > 0 && in.power->global_max_ma > servo_idle_total) {
                uint32_t headroom = in.power->global_max_ma - servo_idle_total;
                uint32_t by_current = headroom / t.worst_ma;
                if (by_current < poly) poly = (uint16_t)by_current;
            }
        }
        if (in.safety != nullptr && in.safety->max_polyphony > 0 &&
            in.safety->max_polyphony < poly) {
            poly = in.safety->max_polyphony;
        }
        if (poly > 255) poly = 255;
        e.polyphony_max = (uint8_t)poly;

        // A playable instrument always sustains at least one note.
        if (e.polyphony_max == 0) e.polyphony_max = 1;

        // ---- constraints ----------------------------------------------------
        if (e.shared_actuators) {
            // Several notes share one mechanism: it cannot voice them at once.
            addConstraint(e, nullptr, 0);
        }
        if (in.power != nullptr) {
            for (uint8_t b = 0; b < 2; b++) {
                if (t.actuators_on_bus[b] == 0 || t.worst_ma_bus[b] == 0) continue;
                uint32_t bus_cap = (b == 0) ? in.power->servo_bus_max_ma
                                            : in.power->solenoid_bus_max_ma;
                if (bus_cap <= servo_idle_bus[b]) continue;
                uint32_t by_current = (bus_cap - servo_idle_bus[b]) / t.worst_ma_bus[b];
                if (by_current < t.actuators_on_bus[b] && by_current < e.polyphony_max) {
                    char group[16];
                    snprintf(group, sizeof(group), "power_supply_%u", (unsigned)b);
                    addConstraint(e, group, (uint8_t)(by_current > 255 ? 255 : by_current));
                }
            }
            // Several logical instruments share one controller budget: GMB must
            // know the ceiling is global, not per instrument.
            if (configured_entries > 1 && in.power->global_max_polyphony > 0) {
                addConstraint(e, "controller", in.power->global_max_polyphony);
            }
        }

        // ---- timing ---------------------------------------------------------
        // The per-actuator rate limiter is a hard admission gate: beyond
        // max_freq_hz triggers in a rolling second the note is REJECTED. That
        // is a real sustained re-articulation bound.
        uint16_t rate_floor = 0;
        if (in.safety != nullptr && in.safety->max_freq_hz > 0) {
            rate_floor = (uint16_t)(1000U / in.safety->max_freq_hz);
        }
        if (rate_floor > e.rearticulation_ms) e.rearticulation_ms = rate_floor;
        e.has_rearticulation = (e.rearticulation_ms > 0);
    }
}

// ----------------------------------------------------------------------------
// Change classification (block 0x11 change_flags)
// ----------------------------------------------------------------------------

static uint32_t fnv1a(const uint8_t* data, size_t len, uint32_t h) {
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h;
}

void gmbDigest(const GmbCapabilitySnapshot& snap, GmbSnapshotDigest& out) {
    memset(&out, 0, sizeof(out));
    out.device_name_hash = fnv1a((const uint8_t*)snap.device_name,
                                 strlen(snap.device_name), 2166136261UL);
    out.instrument_count = snap.instrument_count;
    for (uint8_t i = 0; i < snap.instrument_count && i < MAX_INSTRUMENTS; i++) {
        const GmbInstrumentCaps& e = snap.instruments[i];
        GmbInstrumentDigest& d = out.instruments[i];
        d.channel            = e.channel;
        d.configured         = e.configured ? 1 : 0;
        d.velocity           = e.velocity ? 1 : 0;
        d.polyphony_max      = e.polyphony_max;
        d.gm_program         = e.gm_program;
        d.cc_count           = e.cc_count;
        d.constraint_count   = e.constraint_count;
        d.has_excite_latency = e.has_excite_latency ? 1 : 0;
        d.has_rearticulation = e.has_rearticulation ? 1 : 0;
        d.has_release        = e.has_release ? 1 : 0;
        d.excite_latency_ms  = e.excite_latency_ms;
        d.rearticulation_ms  = e.rearticulation_ms;
        d.release_ms         = e.release_ms;
        d.notes_hash         = fnv1a(e.notes, e.note_count, 2166136261UL);
    }
}

uint8_t gmbChangeFlags(const GmbSnapshotDigest& before, const GmbSnapshotDigest& after) {
    uint8_t flags = 0;
    if (before.device_name_hash != after.device_name_hash) flags |= GMB_CHANGE_IDENTITY;
    if (before.instrument_count != after.instrument_count) flags |= GMB_CHANGE_INSTRUMENTS;

    uint8_t n = (before.instrument_count < after.instrument_count)
              ? before.instrument_count : after.instrument_count;
    for (uint8_t i = 0; i < n && i < MAX_INSTRUMENTS; i++) {
        const GmbInstrumentDigest& a = before.instruments[i];
        const GmbInstrumentDigest& b = after.instruments[i];
        if (a.channel != b.channel || a.configured != b.configured ||
            a.velocity != b.velocity || a.polyphony_max != b.polyphony_max ||
            a.gm_program != b.gm_program || a.notes_hash != b.notes_hash ||
            a.cc_count != b.cc_count || a.constraint_count != b.constraint_count) {
            flags |= GMB_CHANGE_INSTRUMENTS;
        }
        if (a.has_excite_latency != b.has_excite_latency ||
            a.excite_latency_ms  != b.excite_latency_ms  ||
            a.has_rearticulation != b.has_rearticulation ||
            a.rearticulation_ms  != b.rearticulation_ms  ||
            a.has_release        != b.has_release        ||
            a.release_ms         != b.release_ms) {
            flags |= GMB_CHANGE_TIMING;
        }
    }
    return flags;
}
