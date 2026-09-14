#include "test_framework.h"
#include "gmb_fixture.h"

// ============================================================================
// Playable-note derivation (range vs discrete, and what is NOT announced)
// ============================================================================

static GmbInstrumentCaps firstEntry(const GmbFixture& f) {
    GmbCapabilitySnapshot snap;
    f.build(snap);
    return snap.instruments[0];
}

TEST(contiguous_notes_become_a_range) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Chromatic", 0, true, 13);
    for (uint8_t i = 0; i < 4; i++) f.addSolenoid((uint8_t)(1 + i));
    f.mapRange(inst, 60, 63, 1);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK(e.contiguous);
    CHECK_EQ(e.note_count, 4);
    CHECK_CONTAINS(f.descriptor(), "\"mode\":\"range\",\"min\":60,\"max\":63");
}

TEST(sparse_notes_become_a_discrete_list) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Sparse", 0, true, 14);
    f.addSolenoid(1); f.addSolenoid(2); f.addSolenoid(3);
    f.map(inst, 60, 1);
    f.map(inst, 62, 2);
    f.map(inst, 64, 3);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK(!e.contiguous);
    CHECK_CONTAINS(f.descriptor(), "\"mode\":\"discrete\",\"list\":[60,62,64]");
}

TEST(one_missing_semitone_forces_discrete) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Gap", 0, true, 13);
    for (uint8_t i = 0; i < 4; i++) f.addSolenoid((uint8_t)(1 + i));
    f.map(inst, 60, 1);
    f.map(inst, 61, 2);
    f.map(inst, 63, 3);   // 62 missing
    f.map(inst, 64, 4);

    CHECK(!firstEntry(f).contiguous);
    CHECK_CONTAINS(f.descriptor(), "\"list\":[60,61,63,64]");
}

TEST(notes_are_sorted_and_deduplicated) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Jumbled", 0, true, 13);
    f.addSolenoid(1); f.addSolenoid(2); f.addSolenoid(3);
    f.map(inst, 64, 3);
    f.map(inst, 60, 1);
    f.map(inst, 64, 2);   // duplicate MIDI note, different actuator
    f.map(inst, 62, 2);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 3);
    CHECK_EQ(e.notes[0], 60);
    CHECK_EQ(e.notes[1], 62);
    CHECK_EQ(e.notes[2], 64);
    CHECK_CONTAINS(f.descriptor(), "\"list\":[60,62,64]");
}

TEST(disabled_mapping_is_not_announced) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Partly off", 0, true, 13);
    f.addSolenoid(1); f.addSolenoid(2);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2, /*enabled=*/false);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 1);
    CHECK_EQ(e.notes[0], 60);
}

TEST(disabled_actuator_removes_its_note) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("One dead key", 0, true, 13);
    f.addSolenoid(1);
    f.addSolenoid(2, SOL_FRAPPE, 5, 40, 0, /*enabled=*/false);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 1);
    CHECK_EQ(e.notes[0], 60);
}

TEST(mapping_to_a_missing_actuator_is_dropped) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Orphan", 0, true, 13);
    f.addSolenoid(1);
    f.map(inst, 60, 1);
    f.map(inst, 61, 77);   // no such actuator

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instruments[0].note_count, 1);
    CHECK(snap.rejected_mapping_total >= 1u);
}

TEST(behaviour_override_invalid_for_the_actuator_type_is_dropped) {
    // A solenoid routed with SERVO_TOUCHE matches no branch of the engine's
    // behaviour switch: the note would silently do nothing, so it must not be
    // announced as playable.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Bad override", 0, true, 13);
    f.addSolenoid(1);
    f.addSolenoid(2);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2, true, SERVO_TOUCHE);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 1);
    CHECK_EQ(e.notes[0], 60);
}

TEST(valid_behaviour_override_keeps_the_note) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Good override", 0, true, 13);
    f.addSolenoid(1);
    f.addSolenoid(2);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2, true, SOL_HIT_AND_HOLD);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 2);
    CHECK(e.has_hold_notes);
    CHECK(e.has_attack_notes);
}

TEST(shared_actuator_is_declared_as_a_voice_constraint) {
    // Two notes on one mechanism: it can only sound one of them at a time.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Shared", 0, true, 13);
    f.addSolenoid(1);
    f.addSolenoid(2);
    f.map(inst, 60, 1);
    f.map(inst, 61, 1);   // same actuator
    f.map(inst, 62, 2);

    GmbInstrumentCaps e = firstEntry(f);
    CHECK_EQ(e.note_count, 3);
    CHECK_EQ(e.distinct_actuators, 2);
    CHECK(e.shared_actuators);

    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"type\":\"one_note_per_voice\"");
    CHECK_CONTAINS(json, "\"voices\"");
    CHECK_CONTAINS(json, "\"id\":\"a1\"");
}

TEST(independent_actuators_emit_no_voices_block) {
    // One actuator per note: voices carry no information, so they are omitted.
    std::string json = makeXylophone().descriptor();
    CHECK_NOT_CONTAINS(json, "\"voices\"");
    CHECK_NOT_CONTAINS(json, "one_note_per_voice");
}

TEST(playable_note_total_is_reported_for_diagnostics) {
    GmbFixture f = makeXylophone();
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.playable_note_total, 12);
}
