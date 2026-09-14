#include "gmb_runtime.h"

#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <LittleFS.h>
#include "config.h"
#endif

// ============================================================================
// PlayMode — GMB runtime
// ============================================================================

// ----------------------------------------------------------------------------
// Lock
// ----------------------------------------------------------------------------

#if defined(ARDUINO)
GmbLockable::GmbLockable() : _handle(nullptr) {
    _handle = (void*)xSemaphoreCreateMutex();
}
GmbLockable::~GmbLockable() {
    if (_handle != nullptr) vSemaphoreDelete((SemaphoreHandle_t)_handle);
}
void GmbLockable::lock() {
    if (_handle != nullptr) xSemaphoreTake((SemaphoreHandle_t)_handle, portMAX_DELAY);
}
void GmbLockable::unlock() {
    if (_handle != nullptr) xSemaphoreGive((SemaphoreHandle_t)_handle);
}
#else
GmbLockable::GmbLockable()  : _handle(nullptr) {}
GmbLockable::~GmbLockable() {}
void GmbLockable::lock()    {}
void GmbLockable::unlock()  {}
#endif

// ----------------------------------------------------------------------------
// Descriptor cache
// ----------------------------------------------------------------------------

GmbDescriptorCache::GmbDescriptorCache() : _published(-1), _pinned(-1) {
    _len[0] = _len[1] = 0;
    _rev[0] = _rev[1] = 0;
    _buf[0][0] = '\0';
    _buf[1][0] = '\0';
}

char* GmbDescriptorCache::writeBuffer(int8_t& slot) {
    // Never write the buffer an in-flight transfer is serving.
    if (_pinned >= 0)          slot = (int8_t)(1 - _pinned);
    else if (_published >= 0)  slot = (int8_t)(1 - _published);
    else                       slot = 0;
    return _buf[slot];
}

void GmbDescriptorCache::commit(int8_t slot, uint16_t len, uint32_t revision) {
    if (slot < 0 || slot > 1) return;
    _len[slot] = len;
    _rev[slot] = revision;
    _published = slot;
}

const char* GmbDescriptorCache::published(uint16_t& len, uint32_t& revision) const {
    if (_published < 0) { len = 0; revision = 0; return nullptr; }
    len = _len[_published];
    revision = _rev[_published];
    return _buf[_published];
}

uint16_t GmbDescriptorCache::publishedSize() const {
    return (_published < 0) ? 0 : _len[_published];
}

uint32_t GmbDescriptorCache::publishedRevision() const {
    return (_published < 0) ? 0 : _rev[_published];
}

int8_t GmbDescriptorCache::pin() {
    if (_published < 0 || _len[_published] == 0) return -1;
    _pinned = _published;
    return _pinned;
}

void GmbDescriptorCache::unpin(int8_t slot) {
    if (_pinned == slot) _pinned = -1;
}

const char* GmbDescriptorCache::pinned(int8_t slot, uint16_t& len, uint32_t& revision) const {
    if (slot < 0 || slot > 1) { len = 0; revision = 0; return nullptr; }
    len = _len[slot];
    revision = _rev[slot];
    return _buf[slot];
}

// ----------------------------------------------------------------------------
// Runtime
// ----------------------------------------------------------------------------

GmbRuntime::GmbRuntime()
    : _revision(nullptr),
      _has_digest(false),
      _rebuild_requested(false),
      _http_available(false),
      _push_available(false),
      _detail_level(GMB_DETAIL_FULL),
      _overflowed(false),
      _rebuilds(0),
      _sink(nullptr),
      _sink_ctx(nullptr) {
    memset(&_snapshot, 0, sizeof(_snapshot));
    memset(&_digest, 0, sizeof(_digest));
}

void GmbRuntime::begin(const GmbIdentity& identity, GmbRevisionStore* store) {
    _revision = GmbRevision(store);
    _revision.begin();
    _sysex.begin(identity, this);
}

void GmbRuntime::requestRebuild() {
    _rebuild_requested = true;
}

void GmbRuntime::setHttpAvailable(bool on) {
    if (_http_available != on) _http_available = on;
}

void GmbRuntime::setPushAvailable(bool on) {
    if (_push_available != on) _push_available = on;
}

void GmbRuntime::setNotificationSink(NotifySink sink, void* ctx) {
    _sink = sink;
    _sink_ctx = ctx;
}

uint16_t GmbRuntime::chunkCount() const {
    uint16_t len = _cache.publishedSize();
    if (len == 0) return 0;
    return (uint16_t)((len + GMB_CHUNK_PAYLOAD_MAX - 1) / GMB_CHUNK_PAYLOAD_MAX);
}

bool GmbRuntime::rebuild(const GmbBuildInput& in, uint32_t now_ms, bool notify) {
    _rebuild_requested = false;
    _rebuilds++;

    GmbSnapshotDigest next_digest;
    bool changed = false;
    uint8_t change_flags = 0;
    uint32_t revision = 0;

    _lock.lock();

    gmbBuildSnapshot(in, _snapshot);

    int8_t slot = -1;
    char*  buf  = _cache.writeBuffer(slot);
    size_t cap  = _cache.capacity();

    // Render once with revision 0: that is the CANONICAL capability projection,
    // so hashing it cannot be perturbed by the revision counter itself. A
    // setting that never reaches the descriptor therefore cannot move the hash,
    // which is exactly the "cosmetic change -> same revision" rule.
    uint8_t level = GMB_DETAIL_FULL;
    size_t probe_len = gmbBuildDescriptor(_snapshot, 0, buf, cap, &level);
    if (probe_len == 0) {
        // Even the minimal form overflows: keep serving the previous descriptor
        // rather than publishing truncated, invalid JSON.
        _overflowed = true;
        _lock.unlock();
        return false;
    }
    _overflowed = false;
    _detail_level = level;

    uint32_t hash = gmbHash(buf, probe_len);
    changed = _revision.update(hash);
    revision = _revision.value();

    // Re-render with the real revision at the level we know fits. The revision
    // digits can push it over the edge on a pathological configuration, so fall
    // back to the degrading builder if that happens.
    size_t final_len = gmbRenderDescriptor(_snapshot, revision, level, buf, cap);
    if (final_len == 0) {
        final_len = gmbBuildDescriptor(_snapshot, revision, buf, cap, &level);
        _detail_level = level;
    }
    if (final_len == 0) {
        _overflowed = true;
        _lock.unlock();
        return false;
    }

    _cache.commit(slot, (uint16_t)final_len, revision);

    gmbDigest(_snapshot, next_digest);
    change_flags = _has_digest ? gmbChangeFlags(_digest, next_digest) : 0;
    _digest = next_digest;
    _has_digest = true;

    _lock.unlock();

    // Spec §4: the notification is an optimisation, never a dependency — it is
    // emitted only for an EFFECTIVE change, never for a cosmetic edit.
    if (changed && notify && _sink != nullptr) {
        if (change_flags == 0) change_flags = GMB_CHANGE_INSTRUMENTS;
        GmbSysExService::Reply reply;
        if (_sysex.buildNotification(revision, change_flags, now_ms, reply)) {
            _sink(reply.data, reply.len, _sink_ctx);
        }
    }
    return changed;
}

void GmbRuntime::service(const GmbBuildInput& in, uint32_t now_ms) {
    if (_rebuild_requested) rebuild(in, now_ms, true);
    _sysex.tick(now_ms);
}

uint16_t GmbRuntime::copyDescriptor(char* out, uint16_t cap) {
    if (out == nullptr || cap == 0) return 0;
    // Taken under the cache lock: this runs on the async HTTP task while the
    // main loop may be republishing into the very same buffer.
    _lock.lock();
    uint16_t len = 0;
    uint32_t rev = 0;
    const char* data = _cache.published(len, rev);
    if (data == nullptr || len == 0 || len >= cap) {
        _lock.unlock();
        return 0;
    }
    memcpy(out, data, len);
    out[len] = '\0';
    _lock.unlock();
    return len;
}

// --- GmbDescriptorSource ----------------------------------------------------

const char* GmbRuntime::descriptorData() const {
    uint16_t len = 0;
    uint32_t rev = 0;
    return _cache.published(len, rev);
}

uint16_t GmbRuntime::descriptorSize() const      { return _cache.publishedSize(); }
uint32_t GmbRuntime::descriptorRevision() const  { return _cache.publishedRevision(); }

uint8_t GmbRuntime::handshakeFlags() const {
    uint8_t flags = 0;
    if (_http_available) flags |= GMB_FLAG_HTTP_AVAILABLE;
    if (_push_available) flags |= GMB_FLAG_PUSH_NOTIFY;
    return flags;
}

int8_t GmbRuntime::pinPublished() {
    _lock.lock();
    int8_t slot = _cache.pin();
    _lock.unlock();
    return slot;
}

void GmbRuntime::unpin(int8_t handle) {
    _lock.lock();
    _cache.unpin(handle);
    _lock.unlock();
}

const char* GmbRuntime::pinnedData(int8_t handle, uint16_t& len, uint32_t& revision) const {
    return _cache.pinned(handle, len, revision);
}

// ----------------------------------------------------------------------------
// LittleFS revision store (device only)
// ----------------------------------------------------------------------------

#if defined(ARDUINO)

bool GmbFsRevisionStore::load(uint32_t& revision, uint32_t& hash) {
    if (!LittleFS.exists(GMB_REVISION_PATH)) return false;
    File f = LittleFS.open(GMB_REVISION_PATH, "r");
    if (!f) return false;
    char line[48];
    size_t n = f.readBytes(line, sizeof(line) - 1);
    f.close();
    line[n] = '\0';
    unsigned long rev = 0, h = 0;
    if (sscanf(line, "%lu %lu", &rev, &h) != 2) return false;
    revision = (uint32_t)rev;
    hash = (uint32_t)h;
    return true;
}

bool GmbFsRevisionStore::save(uint32_t revision, uint32_t hash) {
    File f = LittleFS.open(GMB_REVISION_TMP_PATH, "w");
    if (!f) return false;
    char line[48];
    int n = snprintf(line, sizeof(line), "%lu %lu",
                     (unsigned long)revision, (unsigned long)hash);
    bool ok = (n > 0) && (f.write((const uint8_t*)line, (size_t)n) == (size_t)n);
    f.close();
    if (!ok) { LittleFS.remove(GMB_REVISION_TMP_PATH); return false; }
    // Atomic replace so a power cut cannot leave a half-written counter.
    LittleFS.remove(GMB_REVISION_PATH);
    return LittleFS.rename(GMB_REVISION_TMP_PATH, GMB_REVISION_PATH);
}

#endif
