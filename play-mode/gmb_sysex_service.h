#ifndef GMB_SYSEX_SERVICE_H
#define GMB_SYSEX_SERVICE_H

#include <Arduino.h>
#include "gmb_protocol.h"
#include "gmb_sysex.h"

// ============================================================================
// PlayMode — GMB SysEx service (handshake / chunk transfer / notification)
// ============================================================================
//
// Transport-independent: it consumes a COMPLETE SysEx frame and produces at
// most one reply frame. Whoever received the frame sends the reply back on the
// same transport. Nothing here touches actuators, the scheduler or the
// filesystem, so a malformed or flooding peer cannot disturb musical timing.
//

// Read-only view of the published descriptor. The runtime implements it; the
// service pins the buffer for the duration of one transfer so a configuration
// change mid-transfer cannot swap the document under GMB's feet (spec §3).
class GmbDescriptorSource {
public:
    virtual ~GmbDescriptorSource() {}
    virtual const char* descriptorData() const = 0;
    virtual uint16_t    descriptorSize() const = 0;
    virtual uint32_t    descriptorRevision() const = 0;
    virtual uint8_t     handshakeFlags() const = 0;

    // Pin the currently published buffer and return an opaque handle, or -1 if
    // nothing is published. A pinned buffer is never overwritten by a rebuild.
    virtual int8_t      pinPublished() = 0;
    virtual void        unpin(int8_t handle) = 0;
    virtual const char* pinnedData(int8_t handle, uint16_t& len, uint32_t& revision) const = 0;
};

struct GmbIdentity {
    uint32_t instance_id;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
};

struct GmbSysExStats {
    uint32_t handshakes;
    uint32_t chunk_requests;
    uint32_t notifications;
    uint32_t invalid;          // malformed / foreign / unknown-block frames
    uint32_t rate_limited;
    uint32_t transfers_started;
    uint32_t transfers_completed;
    uint32_t transfers_timed_out;
    uint32_t last_handshake_ms;
    uint32_t last_chunk_ms;
    uint32_t last_notification_ms;
};

class GmbSysExService {
public:
    struct Reply {
        uint8_t  data[GMB_SYSEX_OUT_MAX];
        uint16_t len;
    };

    GmbSysExService();

    void begin(const GmbIdentity& identity, GmbDescriptorSource* source);

    // Handle one complete inbound SysEx frame (F0 ... F7). Returns true and
    // fills `reply` when a response must be sent back on the same transport.
    // Returns false for every frame that is not ours, is malformed, or is
    // rate-limited — in which case nothing at all is sent.
    bool handleFrame(const uint8_t* frame, uint16_t len, uint32_t now_ms, Reply& reply);

    // Build a block 0x11 capability-change notification.
    bool buildNotification(uint32_t revision, uint8_t change_flags,
                           uint32_t now_ms, Reply& reply);

    // Abandon a transfer that has gone quiet, so the next request serves the
    // current descriptor. Safe to call from the main loop every iteration.
    void tick(uint32_t now_ms);

    const GmbSysExStats& stats() const { return _stats; }
    bool     transferActive() const { return _transfer_active; }
    uint32_t transferRevision() const { return _transfer_revision; }
    uint16_t transferChunks() const { return _transfer_chunks; }

private:
    bool rateAllow(uint8_t bucket, uint32_t now_ms, uint16_t limit);
    void endTransfer(uint32_t now_ms, bool completed);
    bool startTransfer(uint32_t now_ms);

    GmbIdentity          _identity;
    GmbDescriptorSource* _source;
    GmbSysExStats        _stats;

    // Active segmented transfer (spec §3: segments are served from a frozen
    // snapshot so two versions of the profile are never mixed).
    bool     _transfer_active;
    int8_t   _transfer_handle;
    uint32_t _transfer_revision;
    uint16_t _transfer_len;
    uint16_t _transfer_chunks;
    uint32_t _transfer_last_ms;

    // Rolling-window rate limiters (control plane only).
    static const uint8_t BUCKET_COUNT = 3;
    uint32_t _bucket_start_ms[BUCKET_COUNT];
    uint16_t _bucket_count[BUCKET_COUNT];
};

#endif // GMB_SYSEX_SERVICE_H
