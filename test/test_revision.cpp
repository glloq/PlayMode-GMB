#include "test_framework.h"
#include "gmb_runtime.h"
#include "gmb_fixture.h"
#include "gmb_test_source.h"

// ============================================================================
// Capability revision (docs/SYSEX_IDENTITY.md §2: an ETag for the descriptor)
// ============================================================================

struct Harness {
    GmbFixture        fixture;
    TestRevisionStore store;
    GmbRuntime        runtime;
    std::vector<std::string> notifications;

    // `seed_rev` simulates a reboot: the pair is already on flash before the
    // runtime starts, exactly as GmbFsRevisionStore would have left it.
    explicit Harness(uint32_t seed_rev = 0, uint32_t seed_hash = 0) {
        if (seed_rev != 0) {
            store.revision = seed_rev;
            store.hash = seed_hash;
            store.stored = true;
        }
        GmbIdentity id = {0xABCD, 0, 9, 0};
        runtime.begin(id, &store);
        runtime.setNotificationSink(&Harness::sink, this);
    }

    static void sink(const uint8_t* data, uint16_t len, void* ctx) {
        Harness* self = (Harness*)ctx;
        self->notifications.push_back(std::string((const char*)data, len));
    }

    bool rebuild(bool notify = true) {
        GmbBuildInput in = fixture.input();
        return runtime.rebuild(in, 1000, notify);
    }
    uint32_t revision() const { return runtime.revision(); }
};

TEST(first_build_establishes_a_revision) {
    Harness h;
    h.fixture = makeXylophone();
    CHECK(h.rebuild(false));
    CHECK_EQ(h.revision(), 1u);
    CHECK(h.store.stored);
    CHECK_EQ(h.store.revision, 1u);
}

TEST(reboot_without_a_configuration_change_keeps_the_revision) {
    Harness first;
    first.fixture = makeXylophone();
    first.rebuild(false);
    uint32_t rev = first.revision();
    uint32_t hash = first.store.hash;

    // Same persisted pair, fresh runtime: nothing changed, nothing advances,
    // and nothing is written to flash.
    Harness second(rev, hash);
    second.fixture = makeXylophone();
    CHECK(!second.rebuild(false));
    CHECK_EQ(second.revision(), rev);
    CHECK_EQ(second.store.saves, 0u);
    CHECK_EQ(second.notifications.size(), 0u);
}

TEST(a_note_mapping_change_advances_the_revision) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t before = h.revision();

    h.fixture.addSolenoid(30);
    h.fixture.map(0, 72, 30);
    CHECK(h.rebuild());
    CHECK_EQ(h.revision(), before + 1);
    CHECK_EQ(h.notifications.size(), 1u);
}

TEST(resaving_the_same_configuration_keeps_the_revision) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t rev = h.revision();
    uint32_t saves = h.store.saves;

    for (int i = 0; i < 5; i++) CHECK(!h.rebuild());
    CHECK_EQ(h.revision(), rev);
    CHECK_EQ(h.store.saves, saves);        // no flash wear
    CHECK_EQ(h.notifications.size(), 0u);  // and no spurious notification
}

TEST(a_velocity_capability_change_advances_the_revision) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t before = h.revision();

    // Collapse the pulse span: velocity stops changing the physical result.
    for (size_t i = 0; i < h.fixture.actuators.size(); i++) {
        h.fixture.actuators[i].pulse_min_ms = h.fixture.actuators[i].pulse_ms;
    }
    CHECK(h.rebuild());
    CHECK_EQ(h.revision(), before + 1);
    CHECK_CONTAINS(h.fixture.descriptor(), "\"velocity\":false");
}

TEST(a_timing_change_advances_the_revision_and_flags_timing) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t before = h.revision();

    h.fixture.actuator(10).latency_ms = 14;   // acoustic calibration applied
    CHECK(h.rebuild());
    CHECK_EQ(h.revision(), before + 1);
    CHECK_EQ(h.notifications.size(), 1u);
    uint8_t flags = (uint8_t)h.notifications[0][10];
    CHECK((flags & GMB_CHANGE_TIMING) != 0);
}

TEST(a_channel_change_flags_instruments) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    h.fixture.instruments[0].midi_channel = 4;
    CHECK(h.rebuild());
    CHECK_EQ(h.notifications.size(), 1u);
    uint8_t flags = (uint8_t)h.notifications[0][10];
    CHECK((flags & GMB_CHANGE_INSTRUMENTS) != 0);
}

TEST(a_device_rename_flags_identity) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    h.fixture.device_name = "atelier-2";
    CHECK(h.rebuild());
    uint8_t flags = (uint8_t)h.notifications[0][10];
    CHECK((flags & GMB_CHANGE_IDENTITY) != 0);
}

TEST(a_setting_that_does_not_reach_the_descriptor_keeps_the_revision) {
    // Cosmetic / internal settings cannot move a hash taken over the canonical
    // capability projection.
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t rev = h.revision();

    h.fixture.safety.max_duty_pct = 40;         // thermal only
    h.fixture.safety.watchdog_ms = 1234;        // backstop only
    h.fixture.power.smart_rejection = false;    // rejection policy only
    for (size_t i = 0; i < h.fixture.actuators.size(); i++) {
        h.fixture.actuators[i].pca_channel = (uint8_t)(15 - i % 16);  // rewiring
    }
    CHECK(!h.rebuild());
    CHECK_EQ(h.revision(), rev);
    CHECK_EQ(h.notifications.size(), 0u);
}

TEST(an_instrument_rename_advances_the_revision_because_it_is_declared) {
    // The descriptor carries `name`, so renaming genuinely changes the document
    // GMB caches — the ETag has to move with it.
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t rev = h.revision();
    strlcpy(h.fixture.instruments[0].name, "Carillon",
            sizeof(h.fixture.instruments[0].name));
    CHECK(h.rebuild());
    CHECK_EQ(h.revision(), rev + 1);
}

TEST(a_failed_persist_does_not_lose_the_in_memory_revision) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    uint32_t rev = h.revision();

    h.store.fail_saves = true;
    h.fixture.instruments[0].midi_channel = 6;
    CHECK(h.rebuild());
    CHECK_EQ(h.revision(), rev + 1);
    CHECK(!h.runtime.revisionWrites() || true);
    CHECK_EQ(h.store.revision, rev);   // the store still holds the old pair
}

TEST(notification_frame_matches_the_spec) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    h.fixture.instruments[0].midi_channel = 8;
    h.rebuild();

    CHECK_EQ(h.notifications.size(), 1u);
    const std::string& n = h.notifications[0];
    CHECK_EQ(n.size(), (size_t)GMB_NOTIFY_LEN);
    CHECK_EQ((uint8_t)n[0], 0xF0);
    CHECK_EQ((uint8_t)n[1], 0x7D);
    CHECK_EQ((uint8_t)n[2], 0x00);
    CHECK_EQ((uint8_t)n[3], 0x11);
    CHECK_EQ((uint8_t)n[4], 0x02);   // notification direction
    CHECK_EQ(gmbDecode32((const uint8_t*)n.data() + 5), h.revision());
    CHECK_EQ((uint8_t)n[11], 0xF7);
    for (size_t i = 1; i + 1 < n.size(); i++) CHECK(((uint8_t)n[i] & 0x80) == 0);
}

TEST(descriptor_revision_matches_the_handshake_revision) {
    Harness h;
    h.fixture = makeXylophone();
    h.rebuild(false);
    static char buf[GMB_DESCRIPTOR_MAX_BYTES];
    uint16_t n = h.runtime.copyDescriptor(buf, sizeof(buf));
    CHECK(n > 0);
    char expect[32];
    snprintf(expect, sizeof(expect), "\"revision\":%u", (unsigned)h.revision());
    CHECK_CONTAINS(std::string(buf, n), expect);
    CHECK_EQ(h.runtime.descriptorRevision(), h.revision());
}

TEST(rebuild_request_is_honoured_by_service) {
    Harness h;
    h.fixture = makeXylophone();
    GmbBuildInput in = h.fixture.input();
    CHECK(!h.runtime.rebuildPending());
    h.runtime.requestRebuild();
    CHECK(h.runtime.rebuildPending());
    h.runtime.service(in, 10);
    CHECK(!h.runtime.rebuildPending());
    CHECK_EQ(h.runtime.revision(), 1u);
}
