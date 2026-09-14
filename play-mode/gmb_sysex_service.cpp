#include "gmb_sysex_service.h"

// ============================================================================
// PlayMode — GMB SysEx service
// ============================================================================

static const uint8_t BUCKET_HANDSHAKE = 0;
static const uint8_t BUCKET_CHUNK     = 1;
static const uint8_t BUCKET_INVALID   = 2;

GmbSysExService::GmbSysExService()
    : _source(nullptr),
      _transfer_active(false),
      _transfer_handle(-1),
      _transfer_revision(0),
      _transfer_len(0),
      _transfer_chunks(0),
      _transfer_last_ms(0) {
    memset(&_identity, 0, sizeof(_identity));
    memset(&_stats, 0, sizeof(_stats));
    memset(_bucket_start_ms, 0, sizeof(_bucket_start_ms));
    memset(_bucket_count, 0, sizeof(_bucket_count));
}

void GmbSysExService::begin(const GmbIdentity& identity, GmbDescriptorSource* source) {
    _identity = identity;
    _source = source;
}

bool GmbSysExService::rateAllow(uint8_t bucket, uint32_t now_ms, uint16_t limit) {
    if (bucket >= BUCKET_COUNT) return false;
    // Unsigned difference is wrap-safe across the millis() rollover.
    if ((uint32_t)(now_ms - _bucket_start_ms[bucket]) >= GMB_RATE_WINDOW_MS) {
        _bucket_start_ms[bucket] = now_ms;
        _bucket_count[bucket] = 0;
    }
    if (_bucket_count[bucket] >= limit) {
        _stats.rate_limited++;
        return false;
    }
    _bucket_count[bucket]++;
    return true;
}

void GmbSysExService::endTransfer(uint32_t now_ms, bool completed) {
    (void)now_ms;
    if (!_transfer_active) return;
    if (_source != nullptr && _transfer_handle >= 0) _source->unpin(_transfer_handle);
    _transfer_active = false;
    _transfer_handle = -1;
    _transfer_len = 0;
    _transfer_chunks = 0;
    if (completed) _stats.transfers_completed++;
    else           _stats.transfers_timed_out++;
}

bool GmbSysExService::startTransfer(uint32_t now_ms) {
    if (_source == nullptr) return false;
    int8_t handle = _source->pinPublished();
    if (handle < 0) return false;

    uint16_t len = 0;
    uint32_t rev = 0;
    const char* data = _source->pinnedData(handle, len, rev);
    if (data == nullptr || len == 0) {
        _source->unpin(handle);
        return false;
    }
    _transfer_active   = true;
    _transfer_handle   = handle;
    _transfer_len      = len;
    _transfer_revision = rev;
    _transfer_chunks   = (uint16_t)((len + GMB_CHUNK_PAYLOAD_MAX - 1) / GMB_CHUNK_PAYLOAD_MAX);
    _transfer_last_ms  = now_ms;
    _stats.transfers_started++;
    return true;
}

void GmbSysExService::tick(uint32_t now_ms) {
    if (!_transfer_active) return;
    if ((uint32_t)(now_ms - _transfer_last_ms) >= GMB_TRANSFER_TIMEOUT_MS) {
        endTransfer(now_ms, false);
    }
}

bool GmbSysExService::handleFrame(const uint8_t* frame, uint16_t len,
                                  uint32_t now_ms, Reply& reply) {
    reply.len = 0;

    // Drop a stale transfer BEFORE deciding what this frame does, so a retry
    // arriving after the timeout starts a fresh snapshot.
    tick(now_ms);

    GmbRequest req = gmbDecodeRequest(frame, len);

    if (req.kind == GMB_REQ_NONE || req.kind == GMB_REQ_UNKNOWN_BLOCK) {
        // Not ours, malformed, or a block we do not implement: count it, answer
        // nothing. Nothing downstream of here can move an actuator.
        _stats.invalid++;
        rateAllow(BUCKET_INVALID, now_ms, GMB_RATE_MAX_INVALID);
        return false;
    }

    if (req.kind == GMB_REQ_HANDSHAKE) {
        if (!rateAllow(BUCKET_HANDSHAKE, now_ms, GMB_RATE_MAX_HANDSHAKE)) return false;

        uint32_t size = 0, revision = 0;
        uint8_t  flags = 0;
        if (_source != nullptr) {
            size     = _source->descriptorSize();
            revision = _source->descriptorRevision();
            flags    = _source->handshakeFlags();
        }
        reply.len = gmbEncodeHandshake(reply.data, _identity.instance_id,
                                       _identity.fw_major, _identity.fw_minor,
                                       _identity.fw_patch, size, revision, flags);
        _stats.handshakes++;
        _stats.last_handshake_ms = now_ms;
        return true;
    }

    // --- block 0x10: descriptor chunk ---------------------------------------
    if (!rateAllow(BUCKET_CHUNK, now_ms, GMB_RATE_MAX_CHUNK)) return false;

    if (!_transfer_active && !startTransfer(now_ms)) {
        // Level 0 (no descriptor published): nothing to serve.
        _stats.invalid++;
        return false;
    }

    // A retry of ANY chunk — chunk 0 included — keeps serving the snapshot this
    // transfer started with. A configuration change mid-transfer publishes a new
    // descriptor for the NEXT transfer only.
    _transfer_last_ms = now_ms;
    _stats.chunk_requests++;
    _stats.last_chunk_ms = now_ms;

    if (req.chunk_index >= _transfer_chunks) {
        // Out-of-range index: stay silent rather than answer nonsense. GMB
        // retries, then falls back to level 0.
        _stats.invalid++;
        return false;
    }

    uint16_t plen = 0;
    uint32_t rev = 0;
    const char* data = _source->pinnedData(_transfer_handle, plen, rev);
    if (data == nullptr) {
        endTransfer(now_ms, false);
        _stats.invalid++;
        return false;
    }

    uint32_t offset = (uint32_t)req.chunk_index * GMB_CHUNK_PAYLOAD_MAX;
    uint16_t chunk_len = GMB_CHUNK_PAYLOAD_MAX;
    if (offset + chunk_len > _transfer_len) {
        chunk_len = (uint16_t)(_transfer_len - offset);
    }

    reply.len = gmbEncodeChunk(reply.data, (uint16_t)sizeof(reply.data),
                               _transfer_chunks, req.chunk_index,
                               data + offset, chunk_len);
    if (reply.len == 0) {
        _stats.invalid++;
        return false;
    }

    // The last segment completes the transfer and releases the snapshot.
    if (req.chunk_index == (uint16_t)(_transfer_chunks - 1)) {
        endTransfer(now_ms, true);
    }
    return true;
}

bool GmbSysExService::buildNotification(uint32_t revision, uint8_t change_flags,
                                        uint32_t now_ms, Reply& reply) {
    reply.len = gmbEncodeNotification(reply.data, revision, change_flags);
    _stats.notifications++;
    _stats.last_notification_ms = now_ms;
    return reply.len > 0;
}
