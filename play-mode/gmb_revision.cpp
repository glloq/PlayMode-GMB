#include "gmb_revision.h"

// ============================================================================
// PlayMode — GMB capability revision
// ============================================================================

GmbRevision::GmbRevision(GmbRevisionStore* store)
    : _store(store), _revision(0), _hash(0), _loaded(false),
      _persisted(true), _writes(0) {}

void GmbRevision::begin() {
    if (_loaded) return;
    _loaded = true;
    if (_store == nullptr) return;
    uint32_t rev = 0, hash = 0;
    if (_store->load(rev, hash)) {
        _revision = rev;
        _hash = hash;
    }
}

bool GmbRevision::update(uint32_t descriptor_hash) {
    begin();
    if (_revision != 0 && _hash == descriptor_hash) {
        // Nothing effective changed: no increment, and crucially no flash write.
        return false;
    }
    _revision++;
    _hash = descriptor_hash;
    if (_store != nullptr) {
        _persisted = _store->save(_revision, _hash);
        if (_persisted) _writes++;
    }
    return true;
}
