#include "gmb_gm_program.h"

// ============================================================================
// PlayMode — GM program -> General-Midi-Boop type/subtype keys
// ============================================================================
//
// Mirrors INSTRUMENT_TYPE_HIERARCHY in General-Midi-Boop
// `src/midi/adaptation/InstrumentTypeConfig.js`. Categories hold exactly eight
// consecutive programs, so the category index is simply `program >> 3` and the
// subtype index is `program` itself.
//

const char* const GMB_GENERIC_TYPE    = "chromatic_percussion";
const char* const GMB_PHYSICAL_FAMILY = "percussion";

static const char* const GMB_CATEGORY[16] = {
    "piano",                // 0-7
    "chromatic_percussion", // 8-15
    "organ",                // 16-23
    "guitar",               // 24-31
    "bass",                 // 32-39
    "strings",              // 40-47
    "ensemble",             // 48-55
    "brass",                // 56-63
    "reed",                 // 64-71
    "pipe",                 // 72-79
    "synth_lead",           // 80-87
    "synth_pad",            // 88-95
    "synth_effects",        // 96-103
    "ethnic",               // 104-111
    "drums",                // 112-119
    "sound_effects"         // 120-127
};

static const char* const GMB_SUBTYPE[128] = {
    // 0-7 piano
    "acoustic_grand", "bright_acoustic", "electric_grand", "honky_tonk",
    "electric_piano_1", "electric_piano_2", "harpsichord", "clavinet",
    // 8-15 chromatic_percussion
    "celesta", "glockenspiel", "music_box", "vibraphone",
    "marimba", "xylophone", "tubular_bells", "dulcimer",
    // 16-23 organ
    "drawbar", "percussive_organ", "rock_organ", "church_organ",
    "reed_organ", "accordion", "harmonica", "tango_accordion",
    // 24-31 guitar
    "nylon", "steel", "jazz", "clean",
    "muted", "overdrive", "distortion", "harmonics",
    // 32-39 bass
    "acoustic", "finger", "pick", "fretless",
    "slap_1", "slap_2", "synth_bass_1", "synth_bass_2",
    // 40-47 strings
    "violin", "viola", "cello", "contrabass",
    "tremolo", "pizzicato", "harp", "timpani",
    // 48-55 ensemble
    "string_ensemble_1", "string_ensemble_2", "synth_strings_1", "synth_strings_2",
    "choir_aahs", "voice_oohs", "synth_voice", "orchestra_hit",
    // 56-63 brass
    "trumpet", "trombone", "tuba", "muted_trumpet",
    "french_horn", "brass_section", "synth_brass_1", "synth_brass_2",
    // 64-71 reed
    "soprano_sax", "alto_sax", "tenor_sax", "baritone_sax",
    "oboe", "english_horn", "bassoon", "clarinet",
    // 72-79 pipe
    "piccolo", "flute", "recorder", "pan_flute",
    "bottle", "shakuhachi", "whistle", "ocarina",
    // 80-87 synth_lead
    "square", "sawtooth", "calliope", "chiff",
    "charang", "voice_lead", "fifths", "bass_lead",
    // 88-95 synth_pad
    "new_age", "warm", "polysynth", "choir",
    "bowed", "metallic", "halo", "sweep",
    // 96-103 synth_effects
    "rain", "soundtrack", "crystal", "atmosphere",
    "brightness", "goblins", "echoes", "sci_fi",
    // 104-111 ethnic
    "sitar", "banjo", "shamisen", "koto",
    "kalimba", "bagpipe", "fiddle", "shanai",
    // 112-119 drums
    "tinkle_bell", "agogo", "steel_drums", "woodblock",
    "taiko", "melodic_tom", "synth_drum", "reverse_cymbal",
    // 120-127 sound_effects
    "guitar_fret", "breath", "seashore", "bird",
    "telephone", "helicopter", "applause", "gunshot"
};

const char* gmbTypeForProgram(uint8_t program) {
    if (program > 127) return GMB_GENERIC_TYPE;
    return GMB_CATEGORY[program >> 3];
}

const char* gmbSubtypeForProgram(uint8_t program) {
    if (program > 127) return "";
    return GMB_SUBTYPE[program];
}
