#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <Arduino.h>
#include "midi_types.h"
#include "gmb_protocol.h"

// ============================================================================
// PlayMode — Byte-by-byte MIDI Parser (Phase 3)
// ============================================================================
//
// Stateful MIDI parser that consumes one byte at a time.
// Supports running status.
// Used by each transport (Serial, UDP) independently.
//
// SysEx: complete frames short enough to be a General-Midi-Boop request are
// captured into a small fixed buffer and offered to the caller; everything else
// (a longer or corrupt frame) is still consumed and discarded. Capturing does
// not touch the note path — SysEx already clears running status, exactly as
// before.
//

class MidiParser {
public:
    MidiParser();

    // Resets the parser state
    void reset();

    // Feeds the parser with a byte.
    // Returns true when a complete message is ready.
    bool feed(uint8_t byte);

    // Retrieves the last parsed message.
    // Valid only after feed() has returned true.
    MidiMessage getMessage() const;

    // --- SysEx capture (control plane) ---

    // True when a complete SysEx frame (F0 ... F7) small enough to be a GMB
    // request is waiting. Cleared by clearSysEx().
    bool hasSysEx() const { return _sysex_ready; }

    // The captured frame, F0 and F7 included. Valid until clearSysEx().
    const uint8_t* sysExData() const { return _sysex_buf; }
    uint16_t       sysExLength() const { return _sysex_len; }
    void           clearSysEx() { _sysex_ready = false; _sysex_len = 0; }

    // SysEx frames dropped because they exceeded the capture buffer
    // (diagnostics only — an oversized frame is never acted on).
    uint32_t oversizedSysExCount() const { return _sysex_oversized; }

private:
    MidiMessage _message;        // Message being constructed
    uint8_t _running_status;     // Last status byte (running status)
    uint8_t _data_index;         // Expected data byte index (0 or 1)
    uint8_t _expected_length;    // Number of expected data bytes (1 or 2)
    bool _in_sysex;              // Currently receiving SysEx
    bool _ready;                 // Message ready to be read

    // SysEx capture buffer. Deliberately tiny: every v2 request fits, and a
    // flood of long foreign frames can never consume RAM.
    uint8_t  _sysex_buf[GMB_SYSEX_IN_MAX];
    uint16_t _sysex_len;
    bool     _sysex_ready;
    bool     _sysex_overflow;
    uint32_t _sysex_oversized;

    // Determines the number of data bytes for a status byte
    uint8_t dataLengthForStatus(uint8_t status);

    // Processes a status byte
    void handleStatusByte(uint8_t byte);

    // Processes a data byte
    bool handleDataByte(uint8_t byte);
};

#endif // MIDI_PARSER_H
