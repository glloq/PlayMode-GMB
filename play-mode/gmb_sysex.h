#ifndef GMB_SYSEX_H
#define GMB_SYSEX_H

#include <Arduino.h>
#include "gmb_protocol.h"

// ============================================================================
// PlayMode — GMB SysEx wire codec (spec §2/§3/§4)
// ============================================================================
//
// Pure framing: encode/decode only, no state, no I/O. Every numeric field is
// 7-bit little-endian, matching General-Midi-Boop's decoder.
//

// --- 7-bit numeric encoding --------------------------------------------------

// 32-bit value over 5 bytes (bits 28-31 land in the 5th byte's low nibble).
void gmbEncode32(uint32_t value, uint8_t* out);
uint32_t gmbDecode32(const uint8_t* in);

// 21-bit value over 3 bytes.
void gmbEncode21(uint32_t value, uint8_t* out);
uint32_t gmbDecode21(const uint8_t* in);

// 14-bit value over 2 bytes.
void gmbEncode14(uint16_t value, uint8_t* out);
uint16_t gmbDecode14(const uint8_t* in);

// --- Request decoding --------------------------------------------------------

enum GmbRequestKind : uint8_t {
    GMB_REQ_NONE = 0,        // not addressed to us / malformed — ignore silently
    GMB_REQ_HANDSHAKE,
    GMB_REQ_DESCRIPTOR_CHUNK,
    GMB_REQ_UNKNOWN_BLOCK    // well-formed GMB frame, block we do not implement
};

struct GmbRequest {
    GmbRequestKind kind;
    uint16_t       chunk_index;
};

// Decode one COMPLETE SysEx frame (F0 ... F7 inclusive). Anything that is not a
// well-formed General-Midi-Boop v2 request — wrong manufacturer, wrong device
// id, wrong direction, truncated, oversized, or carrying an 8-bit payload byte
// — decodes to GMB_REQ_NONE so the caller stays silent.
GmbRequest gmbDecodeRequest(const uint8_t* frame, uint16_t len);

// --- Response encoding -------------------------------------------------------

// Block 0x01 response. `out` must hold GMB_HANDSHAKE_REPLY_LEN bytes.
// Always writes exactly 24 bytes and returns that count.
uint16_t gmbEncodeHandshake(uint8_t* out, uint32_t instance_id,
                            uint8_t fw_major, uint8_t fw_minor, uint8_t fw_patch,
                            uint32_t descriptor_size, uint32_t revision,
                            uint8_t flags);

// Block 0x10 response. Returns the frame length, or 0 if the payload does not
// fit (payload_len > GMB_CHUNK_PAYLOAD_MAX) or contains a non-ASCII byte.
uint16_t gmbEncodeChunk(uint8_t* out, uint16_t out_cap,
                        uint16_t total_chunks, uint16_t chunk_index,
                        const char* payload, uint16_t payload_len);

// Block 0x11 notification. `out` must hold GMB_NOTIFY_LEN bytes.
uint16_t gmbEncodeNotification(uint8_t* out, uint32_t revision, uint8_t change_flags);

#endif // GMB_SYSEX_H
