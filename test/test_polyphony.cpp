#include "test_framework.h"
#include "gmb_fixture.h"

// ============================================================================
// Polyphony: simultaneous MUSICAL notes, not the number of note mappings
// ============================================================================

static GmbInstrumentCaps entry0(const GmbFixture& f) {
    GmbCapabilitySnapshot snap;
    f.build(snap);
    return snap.instruments[0];
}

// 8 independent solenoids, generous budget and no per-instrument cap.
static GmbFixture makeEightIndependent() {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Eight", 0, true, 13);
    for (uint8_t i = 0; i < 8; i++) f.addSolenoid((uint8_t)(1 + i));
    f.mapRange(inst, 60, 67, 1);
    f.power.global_max_polyphony = 32;
    f.power.global_max_ma = 100000;
    f.power.solenoid_bus_max_ma = 100000;
    f.safety.max_polyphony = 32;
    for (uint8_t i = 0; i < MAX_INSTRUMENTS; i++) f.power.instrument_max_polyphony[i] = 32;
    return f;
}

TEST(independent_actuators_give_polyphony_equal_to_their_count) {
    GmbFixture f = makeEightIndependent();
    CHECK_EQ(entry0(f).polyphony_max, 8);
}

TEST(global_max_concurrent_caps_polyphony) {
    // 8 independent actuators, maxConcurrent = 4 -> polyphony.max <= 4.
    GmbFixture f = makeEightIndependent();
    f.power.global_max_polyphony = 4;
    GmbInstrumentCaps e = entry0(f);
    CHECK(e.polyphony_max <= 4);
    CHECK_EQ(e.polyphony_max, 4);
    CHECK_CONTAINS(f.descriptor(), "\"polyphony\":{\"max\":4");
}

TEST(per_instrument_cap_caps_polyphony) {
    GmbFixture f = makeEightIndependent();
    f.power.instrument_max_polyphony[0] = 3;
    CHECK_EQ(entry0(f).polyphony_max, 3);
}

TEST(safety_polyphony_limit_caps_polyphony) {
    GmbFixture f = makeEightIndependent();
    f.safety.max_polyphony = 2;
    CHECK_EQ(entry0(f).polyphony_max, 2);
}

TEST(current_budget_caps_polyphony) {
    // Room for two solenoids at POWER_SOLENOID_FULL_MA and no more.
    GmbFixture f = makeEightIndependent();
    f.power.global_max_ma = 2 * POWER_SOLENOID_FULL_MA + 10;
    CHECK_EQ(entry0(f).polyphony_max, 2);
}

TEST(polyphony_is_never_the_note_count_when_actuators_are_shared) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Shared bank", 0, true, 13);
    f.addSolenoid(1);
    f.addSolenoid(2);
    for (uint8_t n = 60; n < 66; n++) f.map(inst, n, (uint8_t)((n % 2) ? 1 : 2));
    f.power.global_max_polyphony = 32;
    for (uint8_t i = 0; i < MAX_INSTRUMENTS; i++) f.power.instrument_max_polyphony[i] = 32;

    GmbInstrumentCaps e = entry0(f);
    CHECK_EQ(e.note_count, 6);
    CHECK_EQ(e.distinct_actuators, 2);
    CHECK_EQ(e.polyphony_max, 2);     // NOT 6
}

TEST(a_playable_instrument_never_advertises_zero_polyphony) {
    GmbFixture f = makeEightIndependent();
    f.power.global_max_ma = 1;   // not even one activation fits
    GmbInstrumentCaps e = entry0(f);
    CHECK(e.configured);
    CHECK_EQ(e.polyphony_max, 1);
}

TEST(instrument_capped_at_zero_voices_is_not_configured) {
    // The runtime admission gate rejects every note for such an instrument.
    GmbFixture f = makeEightIndependent();
    for (uint8_t i = 0; i < MAX_INSTRUMENTS; i++) f.power.instrument_max_polyphony[i] = 0;
    CHECK(!entry0(f).configured);
}

TEST(bus_power_group_constraint_is_declared_when_it_binds) {
    GmbFixture f = makeEightIndependent();
    // Bus 1 can only carry three solenoids at full power.
    f.power.solenoid_bus_max_ma = 3 * POWER_SOLENOID_FULL_MA;
    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"type\":\"max_simultaneous_per_group\"");
    CHECK_CONTAINS(json, "\"group\":\"power_supply_1\"");
    CHECK_CONTAINS(json, "\"max\":3");
}

TEST(bus_power_group_constraint_is_omitted_when_it_does_not_bind) {
    GmbFixture f = makeEightIndependent();   // 100 A of headroom
    CHECK_NOT_CONTAINS(f.descriptor(), "power_supply_");
}

TEST(shared_controller_budget_is_declared_when_several_instruments_exist) {
    GmbFixture f;
    uint8_t a = f.addInstrument("A", 0, true, 13);
    uint8_t b = f.addInstrument("B", 1, true, 13);
    f.addSolenoid(1); f.addSolenoid(2);
    f.map(a, 60, 1);
    f.map(b, 61, 2);
    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"group\":\"controller\"");
}

TEST(single_instrument_has_no_controller_group_constraint) {
    CHECK_NOT_CONTAINS(makeXylophone().descriptor(), "\"group\":\"controller\"");
}

TEST(merged_same_channel_instruments_sum_their_voice_caps) {
    GmbFixture f;
    uint8_t a = f.addInstrument("Half A", 4, true, 13);
    uint8_t b = f.addInstrument("Half B", 4, true, 13);
    for (uint8_t i = 0; i < 8; i++) f.addSolenoid((uint8_t)(1 + i));
    f.mapRange(a, 60, 63, 1);
    f.mapRange(b, 64, 67, 5);
    f.power.instrument_max_polyphony[0] = 2;
    f.power.instrument_max_polyphony[1] = 3;
    f.power.global_max_polyphony = 32;
    f.safety.max_polyphony = 32;

    GmbInstrumentCaps e = entry0(f);
    CHECK_EQ(e.distinct_actuators, 8);
    CHECK_EQ(e.polyphony_max, 5);   // 2 + 3, both instruments really do sound
}
