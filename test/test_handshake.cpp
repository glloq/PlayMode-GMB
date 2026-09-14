#include "test_framework.h"
#include "gmb_sysex.h"
#include "gmb_sysex_service.h"
#include "gmb_instance_id.h"
#include "gmb_fixture.h"
#include "gmb_test_source.h"

// ============================================================================
// Block 0x01 — handshake (docs/SYSEX_IDENTITY.md §2)
// ============================================================================

static const uint8_t HANDSHAKE_REQUEST[] = {0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7};

TEST(handshake_reply_is_exactly_24_bytes) {
    uint8_t out[64];
    memset(out, 0xAA, sizeof(out));
    uint16_t n = gmbEncodeHandshake(out, 0x12345678, 1, 2, 3, 1234, 42, 0x03);
    CHECK_EQ(n, 24);
    CHECK_EQ((int)GMB_HANDSHAKE_REPLY_LEN, 24);
}

TEST(handshake_header_and_terminator) {
    uint8_t out[GMB_HANDSHAKE_REPLY_LEN];
    gmbEncodeHandshake(out, 1, 0, 9, 0, 0, 0, 0);
    CHECK_EQ(out[0], 0xF0);
    CHECK_EQ(out[1], 0x7D);
    CHECK_EQ(out[2], 0x00);
    CHECK_EQ(out[3], 0x01);   // block
    CHECK_EQ(out[4], 0x01);   // direction = response
    CHECK_EQ(out[23], 0xF7);
}

TEST(handshake_declares_protocol_version_2) {
    uint8_t out[GMB_HANDSHAKE_REPLY_LEN];
    gmbEncodeHandshake(out, 1, 0, 9, 0, 0, 0, 0);
    CHECK_EQ(out[5], 0x02);
}

TEST(handshake_fields_round_trip) {
    uint8_t out[GMB_HANDSHAKE_REPLY_LEN];
    const uint32_t instance = 0xDEADBEEF;
    const uint32_t revision = 0x0BADF00D;
    const uint32_t size = 1999;
    gmbEncodeHandshake(out, instance, 4, 5, 6, size, revision, 0x03);

    CHECK_EQ(gmbDecode32(&out[6]), instance);
    CHECK_EQ(out[11], 4);
    CHECK_EQ(out[12], 5);
    CHECK_EQ(out[13], 6);
    CHECK_EQ(gmbDecode21(&out[14]), size);
    CHECK_EQ(gmbDecode32(&out[17]), revision);
    CHECK_EQ(out[22], 0x03);
}

TEST(handshake_preserves_full_32_bit_instance_id) {
    // Bit 31 must survive: GMB's parser reads the 5th byte with a 0x0f mask.
    uint8_t out[GMB_HANDSHAKE_REPLY_LEN];
    gmbEncodeHandshake(out, 0xFFFFFFFFu, 0, 0, 0, 0, 0xFFFFFFFFu, 0);
    CHECK_EQ(gmbDecode32(&out[6]), 0xFFFFFFFFu);
    CHECK_EQ(gmbDecode32(&out[17]), 0xFFFFFFFFu);
}

TEST(handshake_is_7_bit_safe) {
    uint8_t out[GMB_HANDSHAKE_REPLY_LEN];
    gmbEncodeHandshake(out, 0xFFFFFFFFu, 0x7F, 0x7F, 0x7F, 0x1FFFFF, 0xFFFFFFFFu, 0x7F);
    for (uint16_t i = 1; i < GMB_HANDSHAKE_REPLY_LEN - 1; i++) {
        CHECK((out[i] & 0x80) == 0);
    }
    CHECK_EQ(out[GMB_HANDSHAKE_REPLY_LEN - 1], 0xF7);
}

TEST(instance_id_is_stable_and_unique_per_chip) {
    const uint8_t mac_a[6] = {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33};
    const uint8_t mac_b[6] = {0x24, 0x6F, 0x28, 0x11, 0x22, 0x34};

    // Same chip, any number of reboots -> same id.
    CHECK_EQ(gmbInstanceIdFromMac(mac_a), gmbInstanceIdFromMac(mac_a));
    // Different chip -> different id.
    CHECK(gmbInstanceIdFromMac(mac_a) != gmbInstanceIdFromMac(mac_b));
    // Never the reserved 0.
    CHECK(gmbInstanceIdFromMac(mac_a) != 0u);

    // A one-bit MAC difference must not collide.
    const uint8_t mac_c[6] = {0x24, 0x6F, 0x28, 0x11, 0x23, 0x33};
    CHECK(gmbInstanceIdFromMac(mac_a) != gmbInstanceIdFromMac(mac_c));
}

TEST(instance_id_is_independent_of_configuration) {
    // The id derives from hardware only; nothing in the configuration can be an
    // input. Proven structurally: the function takes only the MAC.
    const uint8_t mac[6] = {0xA0, 0xB7, 0x65, 0x01, 0x02, 0x03};
    uint32_t before = gmbInstanceIdFromMac(mac);
    GmbFixture f = makeXylophone();
    f.device_name = "totally-different-name";
    f.instruments[0].midi_channel = 9;
    (void)f.descriptor();
    CHECK_EQ(gmbInstanceIdFromMac(mac), before);
}

TEST(service_answers_handshake_with_live_descriptor_state) {
    TestDescriptorSource src;
    src.setDescriptor("{\"gmb_descriptor\":2}", 7);
    src.flags = GMB_FLAG_HTTP_AVAILABLE | GMB_FLAG_PUSH_NOTIFY;

    GmbSysExService svc;
    GmbIdentity id = {0x0A0B0C0D, 0, 9, 0};
    svc.begin(id, &src);

    GmbSysExService::Reply reply;
    CHECK(svc.handleFrame(HANDSHAKE_REQUEST, sizeof(HANDSHAKE_REQUEST), 1000, reply));
    CHECK_EQ(reply.len, GMB_HANDSHAKE_REPLY_LEN);
    CHECK_EQ(gmbDecode32(&reply.data[6]), 0x0A0B0C0Du);
    CHECK_EQ(gmbDecode21(&reply.data[14]), src.descriptorSize());
    CHECK_EQ(gmbDecode32(&reply.data[17]), 7u);
    CHECK_EQ(reply.data[22], 0x03);
    CHECK_EQ(svc.stats().handshakes, 1u);
}

TEST(handshake_reports_level_zero_when_no_descriptor) {
    TestDescriptorSource src;   // nothing published
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    GmbSysExService::Reply reply;
    CHECK(svc.handleFrame(HANDSHAKE_REQUEST, sizeof(HANDSHAKE_REQUEST), 10, reply));
    CHECK_EQ(gmbDecode21(&reply.data[14]), 0u);   // descriptor_size 0 = level 0
}

TEST(handshake_flood_is_rate_limited) {
    TestDescriptorSource src;
    src.setDescriptor("{}", 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    GmbSysExService::Reply reply;
    uint16_t answered = 0;
    for (uint16_t i = 0; i < 200; i++) {
        if (svc.handleFrame(HANDSHAKE_REQUEST, sizeof(HANDSHAKE_REQUEST), 5000, reply)) {
            answered++;
        }
    }
    CHECK_EQ(answered, (uint16_t)GMB_RATE_MAX_HANDSHAKE);
    CHECK(svc.stats().rate_limited > 0u);

    // The next window lets traffic through again.
    CHECK(svc.handleFrame(HANDSHAKE_REQUEST, sizeof(HANDSHAKE_REQUEST),
                          5000 + GMB_RATE_WINDOW_MS, reply));
}
