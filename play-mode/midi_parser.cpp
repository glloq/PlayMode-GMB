#include "midi_parser.h"

// ============================================================================
// PlayMode — MIDI Parser (implementation)
// ============================================================================

MidiParser::MidiParser()
    : _running_status(0),
      _data_index(0),
      _expected_length(0),
      _in_sysex(false),
      _ready(false),
      _sysex_len(0),
      _sysex_ready(false),
      _sysex_overflow(false),
      _sysex_oversized(0) {
    memset(&_message, 0, sizeof(_message));
    memset(_sysex_buf, 0, sizeof(_sysex_buf));
}

void MidiParser::reset() {
    _running_status = 0;
    _data_index = 0;
    _expected_length = 0;
    _in_sysex = false;
    _ready = false;
    _sysex_len = 0;
    _sysex_ready = false;
    _sysex_overflow = false;
    memset(&_message, 0, sizeof(_message));
}

bool MidiParser::feed(uint8_t byte) {
    _ready = false;

    // --- Real-time messages (0xF8-0xFF): always processed, do not affect running status ---
    if (byte >= 0xF8) {
        // Ignore system real-time messages (clock, start, stop, etc.)
        return false;
    }

    // --- Status byte (bit 7 = 1) ---
    if (byte & 0x80) {
        handleStatusByte(byte);
        return false;
    }

    // --- Data byte (bit 7 = 0) ---
    if (_in_sysex) {
        // Capture the payload while it still fits. Past the buffer the frame is
        // marked oversized: it is consumed to the end and then discarded, never
        // truncated into something that could be mistaken for a valid request.
        if (!_sysex_overflow) {
            if (_sysex_len < GMB_SYSEX_IN_MAX - 1) {
                _sysex_buf[_sysex_len++] = byte;
            } else {
                _sysex_overflow = true;
            }
        }
        return false;
    }

    return handleDataByte(byte);
}

MidiMessage MidiParser::getMessage() const {
    return _message;
}

// ============================================================================
// Status byte processing
// ============================================================================
void MidiParser::handleStatusByte(uint8_t byte) {
    // SysEx start
    if (byte == 0xF0) {
        _in_sysex = true;
        _running_status = 0;
        // A new F0 abandons whatever was being captured (and any frame not yet
        // consumed by the caller), so a truncated stream cannot be spliced.
        _sysex_len = 0;
        _sysex_ready = false;
        _sysex_overflow = false;
        _sysex_buf[_sysex_len++] = byte;
        return;
    }

    // SysEx end
    if (byte == 0xF7) {
        if (_in_sysex) {
            if (_sysex_overflow || _sysex_len >= GMB_SYSEX_IN_MAX) {
                _sysex_oversized++;
                _sysex_len = 0;
            } else {
                _sysex_buf[_sysex_len++] = byte;
                _sysex_ready = true;
            }
        }
        _in_sysex = false;
        _sysex_overflow = false;
        return;
    }

    // Ignore common system messages (0xF1-0xF6). Per the MIDI spec any status
    // byte aborts an in-flight SysEx, so drop the partial capture too.
    if (byte >= 0xF1 && byte <= 0xF6) {
        if (_in_sysex) { _in_sysex = false; _sysex_len = 0; _sysex_overflow = false; }
        // Tune Request (0xF6) has no data bytes
        // The others (MTC Quarter Frame, Song Position, Song Select) have 1-2 data bytes
        // We ignore all of them for simplicity
        _running_status = 0;
        return;
    }

    // Channel messages (0x80-0xEF) — these also abort an in-flight SysEx.
    if (_in_sysex) { _sysex_len = 0; _sysex_overflow = false; }
    _in_sysex = false;
    _running_status = byte;
    _data_index = 0;
    _expected_length = dataLengthForStatus(byte);

    // Extract type and channel
    _message.type = (MidiMessageType)(byte & 0xF0);
    _message.channel = byte & 0x0F;
}

// ============================================================================
// Data byte processing
// ============================================================================
bool MidiParser::handleDataByte(uint8_t byte) {
    // No active status -> ignore
    if (_running_status == 0) {
        return false;
    }

    // Running status: if we receive a data byte without a new status,
    // reuse the last status
    if (_data_index == 0) {
        _message.type = (MidiMessageType)(_running_status & 0xF0);
        _message.channel = _running_status & 0x0F;
    }

    if (_data_index == 0) {
        // AUDIT FIX: capture the timestamp at the first data byte so that
        // jitter buffer ageing reflects the wire-arrival time of the
        // earliest byte, not the byte that completes the message.
        _message.timestamp_us = (uint32_t)esp_timer_get_time();
        _message.data1 = byte;
        _data_index = 1;

        // Messages with only 1 data byte -> complete
        if (_expected_length == 1) {
            _message.data2 = 0;
            _data_index = 0;  // Ready for next message (running status)
            _ready = true;
            return true;
        }
    } else if (_data_index == 1) {
        _message.data2 = byte;
        _data_index = 0;  // Ready for next message (running status)
        _ready = true;
        return true;
    }

    return false;
}

// ============================================================================
// Data byte length per message type
// ============================================================================
uint8_t MidiParser::dataLengthForStatus(uint8_t status) {
    switch (status & 0xF0) {
        case 0x80: return 2;  // Note Off
        case 0x90: return 2;  // Note On
        case 0xA0: return 2;  // Polyphonic Aftertouch
        case 0xB0: return 2;  // Control Change
        case 0xC0: return 1;  // Program Change
        case 0xD0: return 1;  // Channel Pressure
        case 0xE0: return 2;  // Pitch Bend
        default:   return 0;
    }
}
