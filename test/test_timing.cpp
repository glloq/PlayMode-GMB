#include "test_framework.h"
#include "gmb_fixture.h"

// ============================================================================
// Timing: unknown != 0. Anything unmeasured is OMITTED, never emitted as zero.
// ============================================================================

static GmbInstrumentCaps entry(const GmbFixture& f) {
    GmbCapabilitySnapshot snap;
    f.build(snap);
    return snap.instruments[0];
}

TEST(unmeasured_acoustic_latency_is_omitted_not_zero) {
    // Software scheduling delay being zero says nothing about acoustic onset.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Uncalibrated", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40, /*latency_ms=*/0);
    f.map(inst, 60, 1);

    CHECK(!entry(f).has_excite_latency);
    std::string json = f.descriptor();
    CHECK_NOT_CONTAINS(json, "\"latency_ms\":0");
    CHECK_NOT_CONTAINS(json, "\"excite\"");
}

TEST(measured_latency_is_declared) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Calibrated", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40, /*latency_ms=*/12);
    f.map(inst, 60, 1);

    GmbInstrumentCaps e = entry(f);
    CHECK(e.has_excite_latency);
    CHECK_EQ(e.excite_latency_ms, 12);
    CHECK_CONTAINS(f.descriptor(), "\"excite\":{\"latency_ms\":12}");
}

TEST(excite_latency_is_the_slowest_note_because_playmode_aligns_the_rest) {
    // The dispatcher delays faster actuators to the instrument's slowest one,
    // so the residual latency GMB must compensate is that maximum.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Mixed latency", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 5, 40, 4);
    f.addSolenoid(2, SOL_FRAPPE, 5, 40, 17);
    f.addSolenoid(3, SOL_FRAPPE, 5, 40, 9);
    f.map(inst, 60, 1);
    f.map(inst, 61, 2);
    f.map(inst, 62, 3);
    CHECK_EQ(entry(f).excite_latency_ms, 17);
}

TEST(a_configured_pulse_duration_never_becomes_acoustic_latency) {
    // Electrical activation time, mechanical travel and acoustic onset are
    // three different things.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Long pulse", 0, true, 13);
    f.addSolenoid(1, SOL_FRAPPE, 30, 30, /*latency_ms=*/0);
    f.map(inst, 60, 1);

    GmbInstrumentCaps e = entry(f);
    CHECK(!e.has_excite_latency);
    // The pulse duration is reported where it belongs: re-articulation.
    CHECK(e.has_rearticulation);
    CHECK_EQ(e.rearticulation_ms, 30);
    CHECK_NOT_CONTAINS(f.descriptor(), "\"excite\"");
}

TEST(a_configured_servo_movement_never_becomes_acoustic_latency) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Servo", 0, true, 13);
    f.addServo(1, SERVO_FRAPPE, 30, /*speed_ms=*/150, /*latency_ms=*/0);
    f.map(inst, 60, 1);

    GmbInstrumentCaps e = entry(f);
    CHECK(!e.has_excite_latency);
    CHECK_EQ(e.rearticulation_ms, 150);
}

TEST(playmode_declares_no_prepare_phase) {
    // A PlayMode note is one commanded gesture that IS the excitation: there is
    // no separable silent preparation to anticipate.
    std::string json = makeXylophone().descriptor();
    CHECK_NOT_CONTAINS(json, "\"prepare\"");
    CHECK_NOT_CONTAINS(json, "per_semitone_ms");
}

TEST(rearticulation_falls_back_to_the_enforced_rate_limit) {
    // Even a mechanism with no configured busy window is bounded by the
    // per-actuator rate limiter, which REJECTS notes above max_freq_hz.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Hold", 0, true, 0);
    f.addSolenoid(1, SOL_HIT_AND_HOLD);
    f.map(inst, 60, 1);
    f.safety.max_freq_hz = 25;   // 40 ms floor

    GmbInstrumentCaps e = entry(f);
    CHECK(e.has_rearticulation);
    CHECK_EQ(e.rearticulation_ms, 40);
}

TEST(rearticulation_takes_the_slower_of_busy_window_and_rate_limit) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Slow servo", 0, true, 13);
    f.addServo(1, SERVO_FRAPPE, 30, /*speed_ms=*/200);
    f.map(inst, 60, 1);
    f.safety.max_freq_hz = 50;   // 20 ms floor, well below the movement
    CHECK_EQ(entry(f).rearticulation_ms, 200);
}

TEST(rearticulation_is_omitted_when_nothing_bounds_it) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Unbounded", 0, true, 0);
    f.addSolenoid(1, SOL_HIT_AND_HOLD);
    f.map(inst, 60, 1);
    f.safety.max_freq_hz = 0;    // rate limiting disabled

    GmbInstrumentCaps e = entry(f);
    CHECK(!e.has_rearticulation);
    CHECK_NOT_CONTAINS(f.descriptor(), "rearticulation_ms");
}

TEST(key_release_duration_is_declared_from_the_configured_movement) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Keys", 0, true, 0);
    f.addServo(1, SERVO_TOUCHE, 25, /*speed_ms=*/90);
    f.map(inst, 60, 1);

    GmbInstrumentCaps e = entry(f);
    CHECK(e.has_release);
    CHECK_EQ(e.release_ms, 90);
    CHECK_CONTAINS(f.descriptor(), "\"release_ms\":90");
}

TEST(strike_only_instruments_declare_no_release) {
    // NoteOff is irrelevant to a strike, so a release time would be a fiction.
    GmbFixture f = makeXylophone();
    CHECK(!entry(f).has_release);
    CHECK_NOT_CONTAINS(f.descriptor(), "release_ms");
}

TEST(hit_and_hold_release_stays_unknown) {
    // The coil is de-energised with no configured ramp; the mechanical fall
    // time is unmeasured.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Hold", 0, true, 0);
    f.addSolenoid(1, SOL_HIT_AND_HOLD);
    f.map(inst, 60, 1);
    CHECK(!entry(f).has_release);
}

TEST(timing_block_is_omitted_entirely_when_nothing_is_known) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Nothing known", 0, true, 0);
    f.addSolenoid(1, SOL_HIT_AND_HOLD);
    f.map(inst, 60, 1);
    f.safety.max_freq_hz = 0;
    std::string json = f.descriptor();
    CHECK(!json.empty());
    CHECK_NOT_CONTAINS(json, "\"timing\"");
}

TEST(note_off_semantics_are_declared) {
    // Strike-only: NoteOff is musically irrelevant.
    CHECK_CONTAINS(makeXylophone().descriptor(), "\"note_off\":\"ignored\"");

    // Key-only: NoteOff releases the note.
    GmbFixture keys;
    uint8_t k = keys.addInstrument("Keys", 0, true, 0);
    keys.addServo(1, SERVO_TOUCHE, 25, 80);
    keys.map(k, 60, 1);
    CHECK_CONTAINS(keys.descriptor(), "\"note_off\":\"releases\"");

    // A mechanical piano with struck keys and a held pedal is both.
    GmbFixture mixed;
    uint8_t m = mixed.addInstrument("Piano", 0, true, 0);
    mixed.addSolenoid(1);
    mixed.addServo(2, SERVO_TOUCHE, 25, 80);
    mixed.map(m, 60, 1);
    mixed.map(m, 61, 2);
    CHECK_CONTAINS(mixed.descriptor(), "\"note_off\":\"mixed\"");
}
