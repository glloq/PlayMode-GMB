#ifndef GMB_PROTOCOL_H
#define GMB_PROTOCOL_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// PlayMode — General-Midi-Boop v2 : protocol constants
// ============================================================================
//
// Wire protocol as specified by General-Midi-Boop `docs/SYSEX_IDENTITY.md`
// (Instrument Recognition & Capability Protocol v2) and as consumed by
// `src/midi/devices/DeviceManager.js` / `src/midi/instrument/DescriptorProtocol.js`.
//
//     F0 7D 00 <block> <direction> ... F7
//
// Every multi-byte numeric field is 7-bit little-endian (least significant
// group first), matching DeviceManager.parseGmbHandshake().
//

// --- Frame delimiters / identity ---
#define GMB_SYSEX_START             0xF0
#define GMB_SYSEX_END               0xF7
#define GMB_MANUFACTURER_ID         0x7D   // non-commercial / educational
#define GMB_DEVICE_ID               0x00   // General-Midi-Boop
#define GMB_PROTOCOL_VERSION        0x02   // this document

// --- Blocks ---
#define GMB_BLOCK_HANDSHAKE         0x01
#define GMB_BLOCK_DESCRIPTOR        0x10
#define GMB_BLOCK_CHANGE_NOTIFY     0x11

// --- Directions ---
#define GMB_DIR_REQUEST             0x00
#define GMB_DIR_RESPONSE            0x01
#define GMB_DIR_NOTIFICATION        0x02

// --- Handshake flags (offset 22 of the reply) ---
#define GMB_FLAG_HTTP_AVAILABLE     0x01   // bit 0 — GET /gmb/descriptor.json
#define GMB_FLAG_PUSH_NOTIFY        0x02   // bit 1 — emits block 0x11

// --- Block 0x11 change flags ---
#define GMB_CHANGE_IDENTITY         0x01
#define GMB_CHANGE_INSTRUMENTS      0x02
#define GMB_CHANGE_TIMING           0x04
#define GMB_CHANGE_RESTART_REQUIRED 0x08

// --- Frame sizes ---
#define GMB_HANDSHAKE_REQUEST_LEN   6      // F0 7D 00 01 00 F7
#define GMB_HANDSHAKE_REPLY_LEN     24     // spec §2: exactly 24 bytes
#define GMB_CHUNK_REQUEST_LEN       8      // F0 7D 00 10 00 <idx[2]> F7
#define GMB_CHUNK_HEADER_LEN        9      // F0 7D 00 10 01 <total[2]> <idx[2]>
#define GMB_CHUNK_PAYLOAD_MAX       200    // spec §3: BLE-MIDI reassembly MTU
#define GMB_CHUNK_REPLY_MAX         (GMB_CHUNK_HEADER_LEN + GMB_CHUNK_PAYLOAD_MAX + 1) // 210
#define GMB_NOTIFY_LEN              12     // F0 7D 00 11 02 <rev[5]> <flags> F7

// Largest frame this firmware ever emits.
#define GMB_SYSEX_OUT_MAX           GMB_CHUNK_REPLY_MAX

// Largest inbound frame accepted. Every v2 request is <= 8 bytes; anything
// longer is a foreign SysEx and is discarded without a reply. Kept small on
// purpose so a flood cannot consume RAM.
#define GMB_SYSEX_IN_MAX            24

// --- Descriptor cache ---
// Upper bound for the rendered descriptor. A full 8-instrument controller with
// discrete note lists fits well inside this; oversized configurations degrade
// optional detail (see gmb_descriptor.h) rather than emit truncated JSON.
#define GMB_DESCRIPTOR_MAX_BYTES    6144

// Per-segment transfer timeout (spec §3: comm_timeout, default 5000 ms). A
// transfer older than this is abandoned and the next chunk request starts a
// fresh snapshot.
#define GMB_TRANSFER_TIMEOUT_MS     5000

// --- SysEx rate limiting (control plane only; note/CC traffic is untouched) ---
#define GMB_RATE_WINDOW_MS          1000
#define GMB_RATE_MAX_HANDSHAKE      8      // handshakes per window
#define GMB_RATE_MAX_CHUNK          64     // descriptor chunks per window
#define GMB_RATE_MAX_INVALID        8      // malformed/unknown frames per window

// --- Capability snapshot limits ---
#define GMB_MAX_NOTES               128    // distinct MIDI notes per entry
#define GMB_MAX_VOICES              16     // voices emitted before degrading
#define GMB_MAX_CONSTRAINTS         4
#define GMB_MAX_CCS                 16

// Sentinel: "no GM program configured".
#define GMB_GM_PROGRAM_NONE         0xFF

#endif // GMB_PROTOCOL_H
