#ifndef GMB_TEST_SOURCE_H
#define GMB_TEST_SOURCE_H

// ============================================================================
// PlayMode — in-memory descriptor source + revision store for the tests
// ============================================================================

#include <string>
#include "gmb_sysex_service.h"
#include "gmb_revision.h"

// Mirrors GmbRuntime's two-buffer publish/pin behaviour without the firmware.
class TestDescriptorSource : public GmbDescriptorSource {
public:
    TestDescriptorSource() : flags(0), _published(-1), _pinned(-1) {
        _rev[0] = _rev[1] = 0;
    }

    void setDescriptor(const std::string& json, uint32_t revision) {
        int8_t slot;
        if (_pinned >= 0)         slot = (int8_t)(1 - _pinned);
        else if (_published >= 0) slot = (int8_t)(1 - _published);
        else                      slot = 0;
        _buf[slot] = json;
        _rev[slot] = revision;
        _published = slot;
    }

    const char* descriptorData() const override {
        return _published < 0 ? nullptr : _buf[_published].c_str();
    }
    uint16_t descriptorSize() const override {
        return _published < 0 ? 0 : (uint16_t)_buf[_published].size();
    }
    uint32_t descriptorRevision() const override {
        return _published < 0 ? 0 : _rev[_published];
    }
    uint8_t handshakeFlags() const override { return flags; }

    int8_t pinPublished() override {
        if (_published < 0 || _buf[_published].empty()) return -1;
        _pinned = _published;
        return _pinned;
    }
    void unpin(int8_t handle) override {
        if (_pinned == handle) _pinned = -1;
    }
    const char* pinnedData(int8_t handle, uint16_t& len, uint32_t& revision) const override {
        if (handle < 0 || handle > 1) { len = 0; revision = 0; return nullptr; }
        len = (uint16_t)_buf[handle].size();
        revision = _rev[handle];
        return _buf[handle].c_str();
    }

    uint8_t flags;

private:
    std::string _buf[2];
    uint32_t    _rev[2];
    int8_t      _published;
    int8_t      _pinned;
};

// In-memory revision persistence, with a switch to simulate a failed save.
class TestRevisionStore : public GmbRevisionStore {
public:
    TestRevisionStore() : revision(0), hash(0), stored(false),
                          fail_saves(false), saves(0) {}

    bool load(uint32_t& r, uint32_t& h) override {
        if (!stored) return false;
        r = revision;
        h = hash;
        return true;
    }
    bool save(uint32_t r, uint32_t h) override {
        saves++;
        if (fail_saves) return false;
        revision = r;
        hash = h;
        stored = true;
        return true;
    }

    uint32_t revision;
    uint32_t hash;
    bool     stored;
    bool     fail_saves;
    uint32_t saves;
};

#endif // GMB_TEST_SOURCE_H
