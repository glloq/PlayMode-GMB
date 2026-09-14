#ifndef GMB_TEST_FIXTURE_H
#define GMB_TEST_FIXTURE_H

// ============================================================================
// PlayMode — configuration fixtures for the native GMB tests
// ============================================================================
//
// Builds ACTIVE-shaped PlayMode configuration (actuators + logical instruments
// + routing + power/safety limits), exactly as ConfigManager holds it, so the
// capability builder is exercised against the real structures.
//

#include <string>
#include <vector>
#include "config.h"
#include "types.h"
#include "gmb_capabilities.h"
#include "gmb_descriptor.h"

struct GmbFixture {
    std::vector<ActuatorConfig>    actuators;
    std::vector<InstrumentConfig>  instruments;
    std::vector<MidiRoutingConfig> routings;
    PowerBudget   power;
    SafetyLimits  safety;
    std::string   device_name;
    bool          config_valid;

    GmbFixture() : device_name("play-mode"), config_valid(true) {
        memset(&power, 0, sizeof(power));
        memset(&safety, 0, sizeof(safety));
        // PlayMode's own defaults (ConfigManager::loadDefaults).
        power.global_max_ma        = POWER_GLOBAL_MAX_MA;
        power.servo_bus_max_ma     = POWER_SERVO_BUS_MAX_MA;
        power.solenoid_bus_max_ma  = POWER_SOLENOID_BUS_MAX_MA;
        power.global_max_polyphony = POWER_MAX_POLYPHONY;
        power.smart_rejection      = true;
        for (uint8_t i = 0; i < MAX_INSTRUMENTS; i++) power.instrument_max_polyphony[i] = 4;

        safety.max_duty_pct     = SAFETY_MAX_DUTY_CYCLE;
        safety.max_freq_hz      = SAFETY_MAX_FREQ_HZ;
        safety.watchdog_ms      = SAFETY_WATCHDOG_MS;
        safety.max_polyphony    = SAFETY_MAX_POLYPHONY;
        safety.max_current_ma   = SAFETY_MAX_TOTAL_CURRENT_MA;
        safety.solenoid_hold_ms = SAFETY_SOLENOID_HOLD_MS;
        safety.servo_hold_ms    = SAFETY_SERVO_HOLD_MS;
    }

    // --- actuators ---

    uint8_t addServo(uint8_t id, uint8_t behaviour = SERVO_FRAPPE,
                     uint16_t amplitude = 30, uint16_t speed_ms = 60,
                     uint16_t latency_ms = 0, bool enabled = true,
                     uint8_t bus_id = 0) {
        ActuatorConfig a;
        memset(&a, 0, sizeof(a));
        a.id = id;
        a.type = ACT_SERVO;
        a.bus_id = bus_id;
        a.pca_address = PCA_BASE_ADDRESS;
        a.pca_channel = (uint8_t)(id % PCA_CHANNELS);
        a.behavior = behaviour;
        a.angle_initial = 90;
        a.amplitude = amplitude;
        a.speed_ms = speed_ms;
        a.angle_b = 120;
        a.latency_ms = latency_ms;
        a.enabled = enabled;
        actuators.push_back(a);
        return id;
    }

    uint8_t addSolenoid(uint8_t id, uint8_t behaviour = SOL_FRAPPE,
                        uint16_t pulse_min_ms = 5, uint16_t pulse_ms = 40,
                        uint16_t latency_ms = 0, bool enabled = true,
                        uint8_t bus_id = 1) {
        ActuatorConfig a;
        memset(&a, 0, sizeof(a));
        a.id = id;
        a.type = ACT_SOLENOID;
        a.bus_id = bus_id;
        a.pca_address = PCA_BASE_ADDRESS;
        a.pca_channel = (uint8_t)(id % PCA_CHANNELS);
        a.behavior = behaviour;
        a.pulse_min_ms = pulse_min_ms;
        a.pulse_ms = pulse_ms;
        a.pwm_initial = SOLENOID_PWM_MAX;
        a.pwm_hold = 2048;
        a.ramp_ms = 40;
        a.latency_ms = latency_ms;
        a.enabled = enabled;
        actuators.push_back(a);
        return id;
    }

    ActuatorConfig& actuator(uint8_t id) {
        for (size_t i = 0; i < actuators.size(); i++) {
            if (actuators[i].id == id) return actuators[i];
        }
        static ActuatorConfig dummy;
        return dummy;
    }

    // --- instruments ---

    uint8_t addInstrument(const char* name, uint8_t channel, bool enabled = true,
                          uint8_t gm_program = GMB_GM_PROGRAM_NONE) {
        InstrumentConfig inst;
        memset(&inst, 0, sizeof(inst));
        strlcpy(inst.name, name, sizeof(inst.name));
        inst.gm_program = gm_program;
        inst.midi_channel = channel;
        inst.bus_id = 0;
        inst.default_latency_ms = 10;
        inst.enabled = enabled;

        MidiRoutingConfig routing;
        memset(&routing, 0, sizeof(routing));
        routing.instrument_index = (uint8_t)instruments.size();

        instruments.push_back(inst);
        routings.push_back(routing);
        return (uint8_t)(instruments.size() - 1);
    }

    void map(uint8_t inst_idx, uint8_t note, uint8_t actuator_id,
             bool enabled = true, uint8_t behaviour_override = 0xFF) {
        MidiRoutingConfig& r = routings[inst_idx];
        NoteMapping& m = r.note_map[r.note_map_count++];
        m.midi_note = note;
        m.actuator_id = actuator_id;
        m.behavior_override = behaviour_override;
        m.enabled = enabled;
    }

    void mapRange(uint8_t inst_idx, uint8_t first_note, uint8_t last_note,
                  uint8_t first_actuator) {
        uint8_t act = first_actuator;
        for (uint8_t n = first_note; n <= last_note; n++) map(inst_idx, n, act++);
    }

    void mapCC(uint8_t inst_idx, uint8_t cc, uint8_t actuator_id,
               CCTarget target = CC_TARGET_AMPLITUDE, bool enabled = true) {
        MidiRoutingConfig& r = routings[inst_idx];
        CCMapping& c = r.cc_map[r.cc_map_count++];
        c.cc_number = cc;
        c.actuator_id = actuator_id;
        c.target = target;
        c.range_min = 0;
        c.range_max = 90;
        c.enabled = enabled;
    }

    void setVelocityCurve(uint8_t inst_idx, const std::vector<uint8_t>& in,
                          const std::vector<uint8_t>& out) {
        MidiRoutingConfig& r = routings[inst_idx];
        r.velocity_curve_count = 0;
        for (size_t i = 0; i < in.size() && i < VELOCITY_CURVE_POINTS; i++) {
            r.velocity_curve[r.velocity_curve_count].input = in[i];
            r.velocity_curve[r.velocity_curve_count].output = out[i];
            r.velocity_curve_count++;
        }
    }

    // --- build ---

    GmbBuildInput input() const {
        GmbBuildInput in;
        in.actuators = actuators.empty() ? nullptr : actuators.data();
        in.actuator_count = (uint8_t)actuators.size();
        in.instruments = instruments.empty() ? nullptr : instruments.data();
        in.instrument_count = (uint8_t)instruments.size();
        in.routings = routings.empty() ? nullptr : routings.data();
        in.routing_count = (uint8_t)routings.size();
        in.power = &power;
        in.safety = &safety;
        in.device_name = device_name.c_str();
        in.config_valid = config_valid;
        return in;
    }

    void build(GmbCapabilitySnapshot& snap) const {
        GmbBuildInput in = input();
        gmbBuildSnapshot(in, snap);
    }

    std::string descriptor(uint32_t revision = 1) const {
        GmbCapabilitySnapshot snap;
        build(snap);
        static char buf[GMB_DESCRIPTOR_MAX_BYTES];
        uint8_t level = 0;
        size_t n = gmbBuildDescriptor(snap, revision, buf, sizeof(buf), &level);
        return (n == 0) ? std::string() : std::string(buf, n);
    }
};

// A plain chromatic solenoid xylophone: one solenoid per note, notes 60..71.
inline GmbFixture makeXylophone() {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Xylo", 0, true, 13 /* GM xylophone */);
    for (uint8_t i = 0; i < 12; i++) f.addSolenoid((uint8_t)(10 + i));
    f.mapRange(inst, 60, 71, 10);
    return f;
}

#endif // GMB_TEST_FIXTURE_H
