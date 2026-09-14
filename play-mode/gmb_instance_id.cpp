#include "gmb_instance_id.h"

// ============================================================================
// PlayMode — GMB instance_id
// ============================================================================

uint32_t gmbInstanceIdFromMac(const uint8_t mac[6]) {
    // FNV-1a 32 over the factory MAC. The MAC's low bytes carry most of the
    // per-unit entropy; a plain truncation would collide across boards from the
    // same batch, so the bytes are mixed instead.
    uint32_t h = 2166136261UL;
    for (uint8_t i = 0; i < 6; i++) {
        h ^= mac[i];
        h *= 16777619UL;
    }
    // 0 is reserved as "unset" by several GMB consumers; never hand it out.
    if (h == 0) h = 0x0000002AUL;
    return h;
}

#if defined(ARDUINO)

uint32_t gmbInstanceId() {
    static uint32_t cached = 0;
    if (cached != 0) return cached;

    // Factory-burned eFuse MAC: identical across reboots, unique per chip, and
    // unaffected by which Wi-Fi interface happens to be up. Read through the
    // Arduino core API so no ESP-IDF header layout is assumed.
    uint64_t efuse = ESP.getEfuseMac();
    uint8_t mac[6];
    for (uint8_t i = 0; i < 6; i++) {
        mac[i] = (uint8_t)((efuse >> (8 * i)) & 0xFF);
    }
    cached = gmbInstanceIdFromMac(mac);
    return cached;
}
#endif
