#ifndef GMB_INSTANCE_ID_H
#define GMB_INSTANCE_ID_H

#include <Arduino.h>

// ============================================================================
// PlayMode — GMB instance_id (spec §2)
// ============================================================================
//
// `instance_id` is the pivot of the whole protocol: it is what binds a stored
// General-Midi-Boop configuration to one physical exemplar. It MUST therefore
// be derived from hardware identity only:
//
//     same ESP32 + reboot              -> same id
//     same ESP32 + configuration change-> same id
//     different ESP32                  -> different id
//
// It is deliberately NOT derived from the instrument name, MIDI channel,
// configured notes, Wi-Fi SSID or firmware profile — all of those change while
// the exemplar stays the same, which would silently unbind the saved setup.
//

// Fold the 6-byte factory MAC (ESP32 eFuse) into a stable 32-bit id.
// Pure function: no hardware access, so it is directly testable.
uint32_t gmbInstanceIdFromMac(const uint8_t mac[6]);

// The running chip's instance id, read once from the eFuse MAC and cached.
uint32_t gmbInstanceId();

#endif // GMB_INSTANCE_ID_H
