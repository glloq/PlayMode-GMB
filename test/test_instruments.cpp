#include "test_framework.h"
#include "gmb_fixture.h"
#include "gmb_gm_program.h"

// ============================================================================
// Logical instruments: one descriptor entry per logical PlayMode instrument
// ============================================================================

TEST(single_instrument_produces_one_entry) {
    GmbFixture f = makeXylophone();
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK(snap.instruments[0].configured);
    CHECK_EQ(snap.instruments[0].channel, 0);
}

TEST(four_instruments_on_one_esp32_produce_four_entries) {
    // The headline PlayMode use case: one controller, four logical instruments.
    GmbFixture f;
    const char* names[4] = {"Bells", "Mini xylo", "Piano section", "Kalimba"};
    const uint8_t programs[4] = {14, 13, 0, 108};
    uint8_t next_actuator = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t inst = f.addInstrument(names[i], (uint8_t)(i + 1), true, programs[i]);
        for (uint8_t n = 0; n < 3; n++) f.addSolenoid(next_actuator + n);
        f.mapRange(inst, (uint8_t)(60 + i * 4), (uint8_t)(62 + i * 4), next_actuator);
        next_actuator += 3;
    }

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 4);
    for (uint8_t i = 0; i < 4; i++) {
        CHECK(snap.instruments[i].configured);
        CHECK_EQ(snap.instruments[i].channel, (uint8_t)(i + 1));
    }

    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"subtype\":\"tubular_bells\"");
    CHECK_CONTAINS(json, "\"subtype\":\"xylophone\"");
    CHECK_CONTAINS(json, "\"type\":\"piano\"");
    CHECK_CONTAINS(json, "\"subtype\":\"kalimba\"");
    CHECK_CONTAINS(json, "\"type\":\"ethnic\"");
}

TEST(disabled_instrument_is_reported_but_not_configured) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Off", 3, /*enabled=*/false, 13);
    f.addSolenoid(1);
    f.map(inst, 60, 1);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    // The slot survives so GMB recognises it without overwriting prior setup.
    CHECK_EQ(snap.instrument_count, 1);
    CHECK(snap.instruments[0].present);
    CHECK(!snap.instruments[0].configured);
    CHECK_EQ(snap.instruments[0].note_count, 0);

    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"configured\":false");
    CHECK_NOT_CONTAINS(json, "\"notes\"");
}

TEST(instrument_without_any_mapping_is_unconfigured) {
    GmbFixture f;
    f.addInstrument("Empty slot", 0, true);
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK(!snap.instruments[0].configured);
    CHECK_CONTAINS(f.descriptor(), "\"configured\":false");
}

TEST(invalid_midi_channel_makes_the_instrument_unconfigured) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Bad channel", 99, true, 13);
    f.addSolenoid(1);
    f.map(inst, 60, 1);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK(!snap.instruments[0].configured);
    // Still a legal descriptor channel so GMB's validator accepts the document.
    CHECK(snap.instruments[0].channel <= 15);
}

TEST(failed_configuration_validation_marks_everything_unconfigured) {
    GmbFixture f = makeXylophone();
    f.config_valid = false;
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK(!snap.instruments[0].configured);
}

TEST(two_instruments_on_the_same_channel_merge_into_one_entry) {
    // Routing cannot tell them apart — a note on that channel reaches BOTH — so
    // a duplicate-channel descriptor would be both ambiguous and rejected by
    // GMB's own validator.
    GmbFixture f;
    uint8_t a = f.addInstrument("Low half", 5, true, 13);
    uint8_t b = f.addInstrument("High half", 5, true, 13);
    f.addSolenoid(1); f.addSolenoid(2); f.addSolenoid(3); f.addSolenoid(4);
    f.map(a, 60, 1); f.map(a, 61, 2);
    f.map(b, 62, 3); f.map(b, 63, 4);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK_EQ(snap.instruments[0].channel, 5);
    CHECK_EQ(snap.instruments[0].note_count, 4);
    CHECK_EQ(snap.instruments[0].source_count, 2);
    CHECK(snap.instruments[0].contiguous);
    CHECK_CONTAINS(f.descriptor(), "\"min\":60,\"max\":63");
}

TEST(descriptor_channels_are_unique) {
    GmbFixture f;
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t inst = f.addInstrument("Inst", 2, true, 13);   // all on channel 2
        f.addSolenoid((uint8_t)(30 + i));
        f.map(inst, (uint8_t)(40 + i), (uint8_t)(30 + i));
    }
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    for (uint8_t i = 0; i < snap.instrument_count; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < snap.instrument_count; j++) {
            CHECK(snap.instruments[i].channel != snap.instruments[j].channel);
        }
    }
}

TEST(omni_instrument_is_advertised_on_channel_zero) {
    GmbFixture f;
    uint8_t inst = f.addInstrument("Omni", MIDI_CHANNEL_OMNI_INTERNAL, true, 13);
    f.addSolenoid(1);
    f.map(inst, 60, 1);

    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, 1);
    CHECK_EQ(snap.instruments[0].channel, 0);
    CHECK(snap.instruments[0].omni);
    CHECK(snap.instruments[0].configured);
}

TEST(no_musical_profile_yields_a_generic_supported_type) {
    // Never inferred from actuator topology: a solenoid does not imply a
    // xylophone. A generic, supported GMB key is declared instead, and
    // gm_program / subtype stay absent (absent == unknown).
    GmbFixture f;
    uint8_t inst = f.addInstrument("Unnamed rig", 0, true);   // no gm_program
    f.addServo(1);
    f.map(inst, 60, 1);

    std::string json = f.descriptor();
    CHECK_CONTAINS(json, "\"type\":\"chromatic_percussion\"");
    CHECK_NOT_CONTAINS(json, "\"gm_program\"");
    CHECK_NOT_CONTAINS(json, "\"subtype\"");
}

TEST(gm_program_vocabulary_matches_general_midi_boop) {
    // Keys of InstrumentTypeConfig.js, not invented labels.
    CHECK_STR_EQ(gmbTypeForProgram(0), "piano");
    CHECK_STR_EQ(gmbSubtypeForProgram(0), "acoustic_grand");
    CHECK_STR_EQ(gmbTypeForProgram(8), "chromatic_percussion");
    CHECK_STR_EQ(gmbSubtypeForProgram(8), "celesta");
    CHECK_STR_EQ(gmbSubtypeForProgram(9), "glockenspiel");
    CHECK_STR_EQ(gmbSubtypeForProgram(12), "marimba");
    CHECK_STR_EQ(gmbSubtypeForProgram(13), "xylophone");
    CHECK_STR_EQ(gmbSubtypeForProgram(14), "tubular_bells");
    CHECK_STR_EQ(gmbTypeForProgram(46), "strings");
    CHECK_STR_EQ(gmbSubtypeForProgram(46), "harp");
    CHECK_STR_EQ(gmbTypeForProgram(108), "ethnic");
    CHECK_STR_EQ(gmbSubtypeForProgram(108), "kalimba");
    CHECK_STR_EQ(gmbTypeForProgram(114), "drums");
    CHECK_STR_EQ(gmbSubtypeForProgram(114), "steel_drums");
    CHECK_STR_EQ(gmbTypeForProgram(127), "sound_effects");
}

TEST(descriptor_declares_device_and_version) {
    GmbFixture f = makeXylophone();
    f.device_name = "atelier-01";
    std::string json = f.descriptor(9);
    CHECK_CONTAINS(json, "\"gmb_descriptor\":2");
    CHECK_CONTAINS(json, "\"revision\":9");
    CHECK_CONTAINS(json, "\"model\":\"PlayMode-GMB\"");
    CHECK_CONTAINS(json, "\"name\":\"atelier-01\"");
}

TEST(descriptor_is_pure_ascii) {
    GmbFixture f = makeXylophone();
    // A non-ASCII instrument name must be escaped, never emitted raw: the
    // payload has to stay 7-bit safe for the SysEx wire.
    strlcpy(f.instruments[0].name, "Carillon \xC3\xA9t\xC3\xA9",
            sizeof(f.instruments[0].name));
    std::string json = f.descriptor();
    CHECK(!json.empty());
    for (size_t i = 0; i < json.size(); i++) {
        CHECK(((unsigned char)json[i] & 0x80) == 0);
    }
    CHECK_CONTAINS(json, "\\u00");
}
