#include "test_framework.h"
#include "gmb_fixture.h"

// ============================================================================
// Velocity: announced only when it changes the PHYSICAL result
// ============================================================================

static bool velocityOf(const GmbFixture& f) {
    GmbCapabilitySnapshot snap;
    f.build(snap);
    return snap.instruments[0].velocity;
}

TEST(solenoid_strike_with_a_pulse_span_is_velocity_sensitive) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Solenoid", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, /*min*/5, /*max*/40);
    f.map(inst, 60, 1);
    CHECK(velocityOf(f));
    CHECK_CONTAINS(f.descriptor(), "\"velocity\":true");
}

TEST(solenoid_strike_with_equal_min_and_max_pulse_is_not) {
    // Every velocity produces the same pulse: the byte exists, the physical
    // result does not change.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Fixed pulse", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, /*min*/20, /*max*/20);
    f.map(inst, 60, 1);
    CHECK(!velocityOf(f));
    CHECK_CONTAINS(f.descriptor(), "\"velocity\":false");
}

TEST(hit_and_hold_solenoid_ignores_velocity) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Hold", 0, true, 0);
    f.addSolenoid(1, SOL_HIT_AND_HOLD, 5, 40);
    f.map(inst, 60, 1);
    CHECK(!velocityOf(f));
}

TEST(servo_strike_with_amplitude_is_velocity_sensitive) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Servo strike", 0, true, 13);
    f.addServo(1, SERVO_FRAPPE, /*amplitude*/30);
    f.map(inst, 60, 1);
    CHECK(velocityOf(f));
}

TEST(servo_strike_with_zero_amplitude_is_not) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Servo no amp", 0, true, 13);
    f.addServo(1, SERVO_FRAPPE, /*amplitude*/0);
    f.map(inst, 60, 1);
    CHECK(!velocityOf(f));
}

TEST(servo_key_and_alternate_ignore_velocity) {
    GmbFixture key;
    uint8_t a = key.addInstrument("Key", 0, true, 0);
    key.addServo(1, SERVO_TOUCHE, 25, 80);
    key.map(a, 60, 1);
    CHECK(!velocityOf(key));

    GmbFixture alt;
    uint8_t b = alt.addInstrument("Alternate", 0, true, 13);
    alt.addServo(1, SERVO_ALTERNE, 25, 80);
    alt.map(b, 60, 1);
    CHECK(!velocityOf(alt));
}

TEST(servo_strum_is_velocity_sensitive) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Strum", 0, true, 46);
    f.addServo(1, SERVO_GRATTER, 40, 100);
    f.map(inst, 60, 1);
    CHECK(velocityOf(f));
}

TEST(one_velocity_sensitive_note_makes_the_instrument_velocity_sensitive) {
    // Per-instrument accuracy: the instrument genuinely responds to velocity as
    // soon as any of its notes does.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Mixed", 0, true, 13);
    f.addSolenoid(1, SOL_HIT_AND_HOLD);            // ignores velocity
    f.addSolenoid(2, SOL_FRAPPE, 5, 40);           // uses velocity
    f.map(inst, 60, 1);
    f.map(inst, 61, 2);
    CHECK(velocityOf(f));
}

TEST(velocity_is_per_instrument_not_per_controller) {
    GmbFixture f;
    uint8_t a = f.addInstrument("Velocity", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40);
    f.map(a, 60, 1);

    uint8_t b = f.addInstrument("No velocity", 1, true, 0);
    f.addServo(2, SERVO_TOUCHE, 25, 80);
    f.map(b, 61, 2);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 2);
    CHECK(snap.instruments[0].velocity);
    CHECK(!snap.instruments[1].velocity);
}

TEST(behaviour_override_decides_velocity_not_the_actuator_default) {
    // The actuator defaults to a velocity-sensitive strike, but the routing
    // overrides the note to hit-and-hold, which ignores velocity.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Override", 0, true, 0);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40);
    f.map(inst, 60, 1, true, SOL_HIT_AND_HOLD);
    CHECK(!velocityOf(f));
}

TEST(flat_velocity_curve_cancels_velocity_response) {
    // A curve mapping every input to one output leaves the mechanism with a
    // constant velocity, whatever the actuator could have done.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Flattened", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40);
    f.map(inst, 60, 1);
    CHECK(velocityOf(f));
    f.setVelocityCurve(inst, {0, 64, 127}, {100, 100, 100});
    CHECK(!velocityOf(f));
}

TEST(shaped_velocity_curve_keeps_velocity_response) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Shaped", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40);
    f.map(inst, 60, 1);
    f.setVelocityCurve(inst, {0, 64, 127}, {10, 80, 127});
    CHECK(velocityOf(f));
}

TEST(pitch_bend_and_aftertouch_are_declared_absent) {
    // PlayMode's dispatcher handles NoteOn/NoteOff/CC only: these are known
    // falses, not unknowns.
    std::string json = makeXylophone().descriptor();
    CHECK_CONTAINS(json, "\"pitch_bend\":{\"supported\":false}");
    CHECK_CONTAINS(json, "\"channel_aftertouch\":false");
    CHECK_CONTAINS(json, "\"poly_aftertouch\":false");
}

TEST(configured_cc_mappings_are_declared) {
    GmbFixture f = makeXylophone();
    f.mapCC(0, 11, 10, CC_TARGET_AMPLITUDE);
    f.mapCC(0, 1, 11, CC_TARGET_SPEED);
    f.mapCC(0, 7, 99, CC_TARGET_AMPLITUDE);        // no such actuator -> dropped
    f.mapCC(0, 20, 12, CC_TARGET_AMPLITUDE, false); // disabled -> dropped
    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"cc\":[11,1]");
}

TEST(cc_position_on_a_solenoid_is_not_declared) {
    // The dispatcher refuses CC_TARGET_POSITION on a solenoid, so announcing
    // the control would be a false claim.
    GmbFixture f = makeXylophone();
    f.mapCC(0, 21, 10, CC_TARGET_POSITION);
    CHECK_NOT_CONTAINS(f.descriptor(), "\"cc\":[21]");
}
