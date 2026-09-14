#include "test_framework.h"
#include "gmb_fixture.h"

// ============================================================================
// Servo / solenoid are implementation details, never a reason to split
// ============================================================================

TEST(alternating_servo_and_solenoid_stay_one_logical_instrument) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Hybrid", 0, true, 13);
    f.addSolenoid(1);
    f.addServo(2);
    f.addSolenoid(3);
    f.addServo(4);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2);
    f.map(inst, 62, 3);
    f.map(inst, 63, 4);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    // ONE instrument, not four, and not one per actuator family.
    CHECK_EQ(snap.instrument_count, 1);
    CHECK_EQ(snap.instruments[0].note_count, 4);
    CHECK(snap.instruments[0].contiguous);

    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"min\":60,\"max\":63");
    // No hardware inventory leaks into the descriptor.
    CHECK_NOT_CONTAINS(json, "servo");
    CHECK_NOT_CONTAINS(json, "solenoid");
    CHECK_NOT_CONTAINS(json, "pca");
}

TEST(mechanical_piano_with_solenoid_keys_and_a_servo_pedal_is_one_instrument) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Mechanical piano", 2, true, 0);
    for (uint8_t i = 0; i < 8; i++) f.addSolenoid((uint8_t)(10 + i));
    f.addServo(50, SERVO_TOUCHE, 25, 120, 0);   // sustain pedal
    f.mapRange(inst, 21, 28, 10);
    f.map(inst, 29, 50);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK_EQ(snap.instruments[0].note_count, 9);
    CHECK(snap.instruments[0].has_attack_notes);
    CHECK(snap.instruments[0].has_hold_notes);
    CHECK_STR_EQ(snap.instruments[0].type, "piano");
}

TEST(actuator_technology_only_reaches_the_descriptor_through_capabilities) {
    // Same notes, same counts; only the technology differs. The descriptors may
    // differ in velocity/timing/polyphony — never in instrument structure.
    GmbFixture servos;
    uint8_t a = servos.addInstrument("S", 0, true, 13);
    for (uint8_t i = 0; i < 4; i++) servos.addServo((uint8_t)(1 + i));
    servos.mapRange(a, 60, 63, 1);

    GmbFixture solenoids;
    uint8_t b = solenoids.addInstrument("S", 0, true, 13);
    for (uint8_t i = 0; i < 4; i++) solenoids.addSolenoid((uint8_t)(1 + i));
    solenoids.mapRange(b, 60, 63, 1);

    GmbCapabilitySnapshot sa, sb;
    servos.build(sa);
    solenoids.build(sb);
    CHECK_EQ(sa.instrument_count, sb.instrument_count);
    CHECK_EQ(sa.instruments[0].note_count, sb.instruments[0].note_count);
    CHECK_EQ(sa.instruments[0].channel, sb.instruments[0].channel);

    CHECK_CONTAINS(servos.descriptor(),    "\"min\":60,\"max\":63");
    CHECK_CONTAINS(solenoids.descriptor(), "\"min\":60,\"max\":63");
}

TEST(physical_family_is_percussion) {
    CHECK_CONTAINS(makeXylophone().descriptor(), "\"family\":\"percussion\"");
}
