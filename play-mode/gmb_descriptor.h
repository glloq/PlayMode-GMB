#ifndef GMB_DESCRIPTOR_H
#define GMB_DESCRIPTOR_H

#include <Arduino.h>
#include "gmb_capabilities.h"

// ============================================================================
// PlayMode — GMB descriptor serialization (spec §5)
// ============================================================================
//
// The capability snapshot is the single source; this file only renders it.
// Output is ASCII-only JSON (every byte < 0x80), so it is already 7-bit safe
// and needs no packing before it goes onto the SysEx wire (spec §3).
//
// Descriptor size is bounded. When a configuration would overflow the buffer,
// OPTIONAL detail is dropped in a fixed order rather than producing truncated,
// invalid JSON.
//

enum GmbDetailLevel : uint8_t {
    GMB_DETAIL_FULL      = 0,   // everything
    GMB_DETAIL_NO_VOICES = 1,   // drop per-voice breakdown
    GMB_DETAIL_CORE      = 2,   // + drop expression CC list and physical block
    GMB_DETAIL_MINIMAL   = 3    // + drop timing and polyphony constraints
};

// Render at exactly `level`. Returns the byte count written (excluding the
// terminating NUL, which is always written when it fits), or 0 if the snapshot
// does not fit — in which case nothing usable was produced.
size_t gmbRenderDescriptor(const GmbCapabilitySnapshot& snap, uint32_t revision,
                           uint8_t level, char* out, size_t cap);

// Render at the richest level that fits, degrading optional detail as needed.
// `used_level` receives the level actually emitted. Returns 0 only when even
// GMB_DETAIL_MINIMAL overflows.
size_t gmbBuildDescriptor(const GmbCapabilitySnapshot& snap, uint32_t revision,
                          char* out, size_t cap, uint8_t* used_level);

// FNV-1a 32 over `len` bytes — used for capability change detection.
uint32_t gmbHash(const char* data, size_t len);

#endif // GMB_DESCRIPTOR_H
