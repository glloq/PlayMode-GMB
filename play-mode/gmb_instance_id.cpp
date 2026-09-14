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

#if defined(ARDUINO) && defined(ESP32)
#include <esp_system.h>

uint32_t gmbInstanceId() {
    static uint32_t cached = 0;
    if (cached != 0) return cached;

    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    // Factory-burned eFuse MAC: identical across reboots, unique per chip, and
    // unaffected by the Wi-Fi interface actually in use.
    esp_efuse_mac_get_default(mac);
    cached = gmbInstanceIdFromMac(mac);
    return cached;
}
#endif
