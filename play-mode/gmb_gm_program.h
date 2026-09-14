#ifndef GMB_GM_PROGRAM_H
#define GMB_GM_PROGRAM_H

#include <Arduino.h>

// ============================================================================
// PlayMode — General-Midi-Boop instrument vocabulary
// ============================================================================
//
// `type` / `subtype` in a GMB descriptor are the textual keys of
// General-Midi-Boop's `src/midi/adaptation/InstrumentTypeConfig.js` — NOT free
// text and NOT numeric ids. That hierarchy is strictly General MIDI aligned:
// sixteen categories of exactly eight consecutive programs, and within a
// category one subtype per program. So a GM program fully determines both keys.
//
// PlayMode therefore stores ONE musical field per logical instrument
// (`InstrumentConfig::gm_program`) and derives type/subtype from it; there is
// no second, hand-maintained vocabulary to keep in sync.
//

// Category key for a GM program (0-127). Never null.
const char* gmbTypeForProgram(uint8_t program);

// Subtype key for a GM program (0-127). Never null.
const char* gmbSubtypeForProgram(uint8_t program);

// Generic category used when the user has not chosen a musical profile.
// `chromatic_percussion` is the General MIDI category for pitched struck
// instruments (celesta, glockenspiel, vibraphone, marimba, xylophone, tubular
// bells) — i.e. PlayMode's core use case. It is a declared fallback, never an
// inference from actuator topology (a solenoid does not imply a xylophone).
extern const char* const GMB_GENERIC_TYPE;

// `physical.family` namespace (spec §5.9). PlayMode is a direct note ->
// actuator engine: one MIDI note strikes / presses one mechanism.
extern const char* const GMB_PHYSICAL_FAMILY;

#endif // GMB_GM_PROGRAM_H
