#ifndef GMB_RUNTIME_H
#define GMB_RUNTIME_H

#include <Arduino.h>
#include "gmb_protocol.h"
#include "gmb_capabilities.h"
#include "gmb_descriptor.h"
#include "gmb_revision.h"
#include "gmb_sysex_service.h"

// ============================================================================
// PlayMode — GMB runtime (capability snapshot + descriptor cache + revision)
// ============================================================================
//
//     Active PlayMode configuration
//                 |
//           CapabilityBuilder
//                 |
//          CapabilitySnapshot
//                 |
//        cached GMB descriptor
//              |
//        SysEx + HTTP
//
// The cache holds TWO buffers. A descriptor transfer pins the buffer it is
// serving, so a configuration change during the transfer publishes into the
// other one: GMB keeps receiving a consistent document and only the NEXT
// transfer sees the new descriptor (spec §3).
//
// Everything here is control plane. It runs from the main loop, never from a
// MIDI callback or the real-time scheduler task.
//

// Mutual exclusion between the loop (rebuild) and the async HTTP task (read).
// A no-op off-device so the same code is unit-testable on a host.
class GmbLockable {
public:
    GmbLockable();
    ~GmbLockable();
    void lock();
    void unlock();
private:
    void* _handle;
};

class GmbDescriptorCache {
public:
    GmbDescriptorCache();

    // Buffer that may be written right now (never the pinned one).
    char*    writeBuffer(int8_t& slot);
    size_t   capacity() const { return GMB_DESCRIPTOR_MAX_BYTES; }
    void     commit(int8_t slot, uint16_t len, uint32_t revision);

    const char* published(uint16_t& len, uint32_t& revision) const;
    uint16_t    publishedSize() const;
    uint32_t    publishedRevision() const;

    int8_t      pin();
    void        unpin(int8_t slot);
    const char* pinned(int8_t slot, uint16_t& len, uint32_t& revision) const;

private:
    char     _buf[2][GMB_DESCRIPTOR_MAX_BYTES];
    uint16_t _len[2];
    uint32_t _rev[2];
    int8_t   _published;
    int8_t   _pinned;
};

class GmbRuntime : public GmbDescriptorSource {
public:
    GmbRuntime();

    void begin(const GmbIdentity& identity, GmbRevisionStore* store);

    // Ask for a rebuild from the active configuration. Cheap and safe to call
    // from an async web handler: the work happens in service().
    void requestRebuild();
    bool rebuildPending() const { return _rebuild_requested; }

    // Rebuild now from `in`. `notify` emits a block 0x11 when the capability
    // set effectively changed. Returns true when the revision advanced.
    bool rebuild(const GmbBuildInput& in, uint32_t now_ms, bool notify);

    // Main-loop entry point: honours a pending rebuild and ages out a stale
    // descriptor transfer.
    void service(const GmbBuildInput& in, uint32_t now_ms);

    // Handshake flags (spec §2).
    void setHttpAvailable(bool on);
    void setPushAvailable(bool on);

    // Where a block 0x11 notification is written out (one transport fan-out
    // function supplied by the firmware). May be null.
    typedef void (*NotifySink)(const uint8_t* data, uint16_t len, void* ctx);
    void setNotificationSink(NotifySink sink, void* ctx);

    GmbSysExService& sysex() { return _sysex; }
    const GmbCapabilitySnapshot& snapshot() const { return _snapshot; }
    const GmbSysExStats& stats() const { return _sysex.stats(); }

    uint32_t revision() const { return _revision.value(); }
    uint16_t descriptorBytes() const { return _cache.publishedSize(); }
    uint16_t chunkCount() const;
    uint8_t  detailLevel() const { return _detail_level; }
    bool     degraded() const { return _detail_level != GMB_DETAIL_FULL; }
    bool     overflowed() const { return _overflowed; }
    uint32_t rebuildCount() const { return _rebuilds; }
    uint32_t revisionWrites() const { return _revision.writeCount(); }

    // Copy the published descriptor out under the cache lock (HTTP path).
    // Returns the number of bytes copied, 0 when nothing is published.
    uint16_t copyDescriptor(char* out, uint16_t cap);

    // --- GmbDescriptorSource ------------------------------------------------
    const char* descriptorData() const override;
    uint16_t    descriptorSize() const override;
    uint32_t    descriptorRevision() const override;
    uint8_t     handshakeFlags() const override;
    int8_t      pinPublished() override;
    void        unpin(int8_t handle) override;
    const char* pinnedData(int8_t handle, uint16_t& len, uint32_t& revision) const override;

private:
    GmbDescriptorCache    _cache;
    GmbLockable           _lock;
    GmbRevision           _revision;
    GmbSysExService       _sysex;
    GmbCapabilitySnapshot _snapshot;
    GmbSnapshotDigest     _digest;
    bool                  _has_digest;

    volatile bool _rebuild_requested;
    bool     _http_available;
    bool     _push_available;
    uint8_t  _detail_level;
    bool     _overflowed;
    uint32_t _rebuilds;

    NotifySink _sink;
    void*      _sink_ctx;
};

#if defined(ARDUINO)
// LittleFS-backed revision store. Writes only when the revision actually
// advances, so a quiet device never touches flash.
class GmbFsRevisionStore : public GmbRevisionStore {
public:
    bool load(uint32_t& revision, uint32_t& hash) override;
    bool save(uint32_t revision, uint32_t hash) override;
};
#endif

#endif // GMB_RUNTIME_H
