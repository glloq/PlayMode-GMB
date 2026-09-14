#ifndef GMB_REVISION_H
#define GMB_REVISION_H

#include <Arduino.h>

// ============================================================================
// PlayMode — GMB capability revision (spec §2: an ETag for the descriptor)
// ============================================================================
//
//     boot without configuration change -> same revision
//     effective capability change       -> revision + 1
//     failed / invalid save             -> no revision change
//     cosmetic-only setting             -> no revision change
//
// The last two fall out of the mechanism rather than needing special cases: the
// revision advances only when the CANONICAL DESCRIPTOR HASH changes, so a
// setting that does not reach the descriptor cannot move it, and a save that
// never took effect cannot either.
//
// Persistence is deliberately behind an interface: the device stores the pair
// in LittleFS, tests store it in memory.
//

class GmbRevisionStore {
public:
    virtual ~GmbRevisionStore() {}
    // Returns false when nothing has been persisted yet.
    virtual bool load(uint32_t& revision, uint32_t& hash) = 0;
    virtual bool save(uint32_t revision, uint32_t hash) = 0;
};

class GmbRevision {
public:
    explicit GmbRevision(GmbRevisionStore* store = nullptr);

    // Read the persisted pair (if any). Safe to call once at boot.
    void begin();

    // Offer a freshly computed descriptor hash.
    //   - identical to the stored hash  -> revision unchanged, nothing written;
    //   - different                     -> revision incremented and persisted.
    // Returns true when the revision changed.
    bool update(uint32_t descriptor_hash);

    uint32_t value() const { return _revision; }
    uint32_t hash()  const { return _hash; }
    bool     persisted() const { return _persisted; }
    uint32_t writeCount() const { return _writes; }

private:
    GmbRevisionStore* _store;
    uint32_t _revision;
    uint32_t _hash;
    bool     _loaded;
    bool     _persisted;   // last save attempt succeeded
    uint32_t _writes;      // flash writes performed (diagnostics)
};

#endif // GMB_REVISION_H
