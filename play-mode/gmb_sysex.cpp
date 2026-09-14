#include "gmb_sysex.h"

// ============================================================================
// PlayMode — GMB SysEx wire codec
// ============================================================================

void gmbEncode32(uint32_t value, uint8_t* out) {
    out[0] = (uint8_t)( value        & 0x7F);
    out[1] = (uint8_t)((value >>  7) & 0x7F);
    out[2] = (uint8_t)((value >> 14) & 0x7F);
    out[3] = (uint8_t)((value >> 21) & 0x7F);
    out[4] = (uint8_t)((value >> 28) & 0x0F);
}

uint32_t gmbDecode32(const uint8_t* in) {
    return ((uint32_t)(in[0] & 0x7F))        |
           ((uint32_t)(in[1] & 0x7F) <<  7)  |
           ((uint32_t)(in[2] & 0x7F) << 14)  |
           ((uint32_t)(in[3] & 0x7F) << 21)  |
           ((uint32_t)(in[4] & 0x0F) << 28);
}

void gmbEncode21(uint32_t value, uint8_t* out) {
    out[0] = (uint8_t)( value        & 0x7F);
    out[1] = (uint8_t)((value >>  7) & 0x7F);
    out[2] = (uint8_t)((value >> 14) & 0x7F);
}

uint32_t gmbDecode21(const uint8_t* in) {
    return ((uint32_t)(in[0] & 0x7F))       |
           ((uint32_t)(in[1] & 0x7F) <<  7) |
           ((uint32_t)(in[2] & 0x7F) << 14);
}

void gmbEncode14(uint16_t value, uint8_t* out) {
    out[0] = (uint8_t)( value       & 0x7F);
    out[1] = (uint8_t)((value >> 7) & 0x7F);
}

uint16_t gmbDecode14(const uint8_t* in) {
    return (uint16_t)((in[0] & 0x7F) | ((uint16_t)(in[1] & 0x7F) << 7));
}

// ----------------------------------------------------------------------------
// Request decoding
// ----------------------------------------------------------------------------

GmbRequest gmbDecodeRequest(const uint8_t* frame, uint16_t len) {
    GmbRequest req = {GMB_REQ_NONE, 0};
    if (frame == nullptr) return req;

    // Shortest possible GMB frame: F0 7D 00 <block> <dir> F7.
    if (len < 6 || len > GMB_SYSEX_IN_MAX) return req;
    if (frame[0] != GMB_SYSEX_START || frame[len - 1] != GMB_SYSEX_END) return req;
    if (frame[1] != GMB_MANUFACTURER_ID) return req;
    if (frame[2] != GMB_DEVICE_ID) return req;

    // Every byte between the delimiters must be 7-bit. An 8-bit payload is a
    // corrupt or foreign frame; refuse it rather than interpreting it.
    for (uint16_t i = 1; i + 1 < len; i++) {
        if (frame[i] & 0x80) return req;
    }

    uint8_t block     = frame[3];
    uint8_t direction = frame[4];

    // We only ever answer requests. A response or notification on the wire is
    // another device talking; stay silent.
    if (direction != GMB_DIR_REQUEST) return req;

    if (block == GMB_BLOCK_HANDSHAKE) {
        if (len != GMB_HANDSHAKE_REQUEST_LEN) return req;
        req.kind = GMB_REQ_HANDSHAKE;
        return req;
    }

    if (block == GMB_BLOCK_DESCRIPTOR) {
        if (len != GMB_CHUNK_REQUEST_LEN) return req;
        req.kind = GMB_REQ_DESCRIPTOR_CHUNK;
        req.chunk_index = gmbDecode14(&frame[5]);
        return req;
    }

    // A well-formed GMB request for a block this firmware does not implement.
    // The caller counts it and stays silent (unknown fields/blocks are ignored
    // silently by design — spec §1 extensibility rule).
    req.kind = GMB_REQ_UNKNOWN_BLOCK;
    return req;
}

// ----------------------------------------------------------------------------
// Response encoding
// ----------------------------------------------------------------------------

uint16_t gmbEncodeHandshake(uint8_t* out, uint32_t instance_id,
                            uint8_t fw_major, uint8_t fw_minor, uint8_t fw_patch,
                            uint32_t descriptor_size, uint32_t revision,
                            uint8_t flags) {
    uint16_t i = 0;
    out[i++] = GMB_SYSEX_START;
    out[i++] = GMB_MANUFACTURER_ID;
    out[i++] = GMB_DEVICE_ID;
    out[i++] = GMB_BLOCK_HANDSHAKE;
    out[i++] = GMB_DIR_RESPONSE;
    out[i++] = GMB_PROTOCOL_VERSION;
    gmbEncode32(instance_id, &out[i]); i += 5;
    out[i++] = (uint8_t)(fw_major & 0x7F);
    out[i++] = (uint8_t)(fw_minor & 0x7F);
    out[i++] = (uint8_t)(fw_patch & 0x7F);
    gmbEncode21(descriptor_size, &out[i]); i += 3;
    gmbEncode32(revision, &out[i]); i += 5;
    out[i++] = (uint8_t)(flags & 0x7F);
    out[i++] = GMB_SYSEX_END;
    return i;   // exactly GMB_HANDSHAKE_REPLY_LEN
}

uint16_t gmbEncodeChunk(uint8_t* out, uint16_t out_cap,
                        uint16_t total_chunks, uint16_t chunk_index,
                        const char* payload, uint16_t payload_len) {
    if (payload_len > GMB_CHUNK_PAYLOAD_MAX) return 0;
    uint16_t need = (uint16_t)(GMB_CHUNK_HEADER_LEN + payload_len + 1);
    if (out_cap < need) return 0;

    uint16_t i = 0;
    out[i++] = GMB_SYSEX_START;
    out[i++] = GMB_MANUFACTURER_ID;
    out[i++] = GMB_DEVICE_ID;
    out[i++] = GMB_BLOCK_DESCRIPTOR;
    out[i++] = GMB_DIR_RESPONSE;
    gmbEncode14(total_chunks, &out[i]); i += 2;
    gmbEncode14(chunk_index,  &out[i]); i += 2;
    for (uint16_t p = 0; p < payload_len; p++) {
        uint8_t b = (uint8_t)payload[p];
        // The descriptor is ASCII by construction; refuse to emit a frame that
        // would break SysEx framing rather than silently masking the byte.
        if (b & 0x80) return 0;
        out[i++] = b;
    }
    out[i++] = GMB_SYSEX_END;
    return i;
}

uint16_t gmbEncodeNotification(uint8_t* out, uint32_t revision, uint8_t change_flags) {
    uint16_t i = 0;
    out[i++] = GMB_SYSEX_START;
    out[i++] = GMB_MANUFACTURER_ID;
    out[i++] = GMB_DEVICE_ID;
    out[i++] = GMB_BLOCK_CHANGE_NOTIFY;
    out[i++] = GMB_DIR_NOTIFICATION;
    gmbEncode32(revision, &out[i]); i += 5;
    out[i++] = (uint8_t)(change_flags & 0x7F);
    out[i++] = GMB_SYSEX_END;
    return i;   // exactly GMB_NOTIFY_LEN
}
