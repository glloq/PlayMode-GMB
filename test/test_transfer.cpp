#include "test_framework.h"
#include "gmb_sysex.h"
#include "gmb_sysex_service.h"
#include "gmb_runtime.h"
#include "gmb_fixture.h"
#include "gmb_test_source.h"

// ============================================================================
// Block 0x10 — segmented descriptor transfer (docs/SYSEX_IDENTITY.md §3)
// ============================================================================

static void chunkRequest(uint8_t* out, uint16_t index) {
    out[0] = 0xF0; out[1] = 0x7D; out[2] = 0x00;
    out[3] = 0x10; out[4] = 0x00;
    gmbEncode14(index, &out[5]);
    out[7] = 0xF7;
}

// Fetch one chunk; returns the payload, or "" when nothing was answered.
static std::string fetch(GmbSysExService& svc, uint16_t index, uint32_t now_ms,
                         uint16_t* total_out = nullptr) {
    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, index);
    GmbSysExService::Reply reply;
    if (!svc.handleFrame(req, sizeof(req), now_ms, reply)) return std::string();
    if (total_out) *total_out = gmbDecode14(&reply.data[5]);
    return std::string((const char*)&reply.data[GMB_CHUNK_HEADER_LEN],
                       reply.len - GMB_CHUNK_HEADER_LEN - 1);
}

static std::string makeJson(size_t len, char fill) {
    return std::string(len, fill);
}

TEST(chunk_reply_header_is_well_formed) {
    TestDescriptorSource src;
    src.setDescriptor(makeJson(50, 'x'), 3);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, 0);
    GmbSysExService::Reply reply;
    CHECK(svc.handleFrame(req, sizeof(req), 100, reply));
    CHECK_EQ(reply.data[0], 0xF0);
    CHECK_EQ(reply.data[1], 0x7D);
    CHECK_EQ(reply.data[2], 0x00);
    CHECK_EQ(reply.data[3], 0x10);
    CHECK_EQ(reply.data[4], 0x01);
    CHECK_EQ(gmbDecode14(&reply.data[5]), 1u);   // total chunks
    CHECK_EQ(gmbDecode14(&reply.data[7]), 0u);   // index
    CHECK_EQ(reply.data[reply.len - 1], 0xF7);
    CHECK_EQ(reply.len, GMB_CHUNK_HEADER_LEN + 50 + 1);
}

TEST(chunk_payload_never_exceeds_the_spec_maximum) {
    TestDescriptorSource src;
    src.setDescriptor(makeJson(1000, 'a'), 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, 0);
    GmbSysExService::Reply reply;
    CHECK(svc.handleFrame(req, sizeof(req), 1, reply));
    CHECK_EQ(reply.len - GMB_CHUNK_HEADER_LEN - 1, GMB_CHUNK_PAYLOAD_MAX);
    CHECK(reply.len <= 210);
}

TEST(single_chunk_descriptor) {
    TestDescriptorSource src;
    std::string doc = makeJson(120, 'q');
    src.setDescriptor(doc, 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint16_t total = 0;
    std::string got = fetch(svc, 0, 10, &total);
    CHECK_EQ(total, 1);
    CHECK_STR_EQ(got, doc);
    CHECK_EQ(svc.stats().transfers_completed, 1u);
}

TEST(multi_chunk_descriptor_reconstructs_exactly) {
    TestDescriptorSource src;
    std::string doc;
    for (int i = 0; i < 1234; i++) doc += (char)('!' + (i % 60));
    src.setDescriptor(doc, 5);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint16_t total = 0;
    fetch(svc, 0, 1, &total);
    CHECK_EQ(total, (uint16_t)((doc.size() + 199) / 200));

    std::string rebuilt;
    for (uint16_t i = 0; i < total; i++) rebuilt += fetch(svc, i, 1);
    CHECK_STR_EQ(rebuilt, doc);
    CHECK_EQ(rebuilt.size(), doc.size());
}

TEST(arbitrary_chunk_order_and_retries_are_served) {
    TestDescriptorSource src;
    std::string doc;
    for (int i = 0; i < 700; i++) doc += (char)('A' + (i % 26));
    src.setDescriptor(doc, 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint16_t total = 0;
    fetch(svc, 2, 1, &total);
    CHECK_EQ(total, 4);

    // Middle, first, last, then a retry of the middle.
    std::string c1 = fetch(svc, 1, 1);
    std::string c0 = fetch(svc, 0, 1);
    std::string c1_again = fetch(svc, 1, 1);
    CHECK_STR_EQ(c1, c1_again);
    CHECK_STR_EQ(c0, doc.substr(0, 200));
    CHECK_STR_EQ(c1, doc.substr(200, 200));

    std::string last = fetch(svc, 3, 1);
    CHECK_EQ(last.size(), doc.size() - 600);
    CHECK_STR_EQ(last, doc.substr(600));
}

TEST(invalid_chunk_index_is_answered_with_silence) {
    TestDescriptorSource src;
    src.setDescriptor(makeJson(100, 'z'), 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, 99);
    GmbSysExService::Reply reply;
    CHECK(!svc.handleFrame(req, sizeof(req), 1, reply));
    CHECK_EQ(reply.len, 0);
}

TEST(chunk_request_with_no_descriptor_is_silent) {
    TestDescriptorSource src;   // level 0
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, 0);
    GmbSysExService::Reply reply;
    CHECK(!svc.handleFrame(req, sizeof(req), 1, reply));
}

TEST(a_transfer_keeps_serving_its_snapshot_across_a_configuration_change) {
    // Descriptor A is being transferred when the configuration changes and
    // descriptor B is published. Every remaining segment — including a RETRY of
    // chunk 0 — must still come from A.
    TestDescriptorSource src;
    std::string docA(700, 'A');
    std::string docB(700, 'B');
    src.setDescriptor(docA, 1);

    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint16_t total = 0;
    std::string first = fetch(svc, 0, 100, &total);
    CHECK_EQ(total, 4);
    CHECK_EQ(first[0], 'A');
    CHECK_EQ(svc.transferRevision(), 1u);

    // Configuration changes mid-transfer.
    src.setDescriptor(docB, 2);
    CHECK_EQ(src.descriptorRevision(), 2u);

    std::string retry0 = fetch(svc, 0, 150);
    CHECK_EQ(retry0[0], 'A');          // NOT switched to B
    CHECK_STR_EQ(retry0, first);
    CHECK_EQ(fetch(svc, 1, 160)[0], 'A');
    CHECK_EQ(fetch(svc, 2, 170)[0], 'A');
    CHECK_EQ(fetch(svc, 3, 180)[0], 'A');
    CHECK_EQ(svc.stats().transfers_completed, 1u);

    // The NEXT transfer serves B.
    CHECK_EQ(fetch(svc, 0, 190)[0], 'B');
    CHECK_EQ(svc.transferRevision(), 2u);
}

TEST(a_timed_out_transfer_releases_the_snapshot_and_the_next_serves_the_new_one) {
    TestDescriptorSource src;
    src.setDescriptor(std::string(700, 'A'), 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    CHECK_EQ(fetch(svc, 0, 1000)[0], 'A');
    CHECK(svc.transferActive());

    src.setDescriptor(std::string(700, 'B'), 2);

    // Still inside the window: the frozen snapshot is used.
    CHECK_EQ(fetch(svc, 1, 1000 + GMB_TRANSFER_TIMEOUT_MS - 1)[0], 'A');
    // Past the window: the transfer is abandoned and a fresh one starts.
    CHECK_EQ(fetch(svc, 0, 1000 + 3 * GMB_TRANSFER_TIMEOUT_MS)[0], 'B');
    CHECK_EQ(svc.stats().transfers_timed_out, 1u);
}

TEST(tick_ages_out_an_idle_transfer) {
    TestDescriptorSource src;
    src.setDescriptor(std::string(700, 'A'), 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    fetch(svc, 0, 500);
    CHECK(svc.transferActive());
    svc.tick(500 + GMB_TRANSFER_TIMEOUT_MS / 2);
    CHECK(svc.transferActive());
    svc.tick(500 + GMB_TRANSFER_TIMEOUT_MS);
    CHECK(!svc.transferActive());
}

TEST(chunk_flood_is_rate_limited) {
    TestDescriptorSource src;
    src.setDescriptor(makeJson(4000, 'x'), 1);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint8_t req[GMB_CHUNK_REQUEST_LEN];
    chunkRequest(req, 0);
    GmbSysExService::Reply reply;
    uint16_t answered = 0;
    for (uint16_t i = 0; i < 500; i++) {
        if (svc.handleFrame(req, sizeof(req), 2000, reply)) answered++;
    }
    CHECK_EQ(answered, (uint16_t)GMB_RATE_MAX_CHUNK);
    CHECK(svc.stats().rate_limited > 0u);
}

TEST(every_chunk_frame_is_7_bit_safe) {
    TestDescriptorSource src;
    src.setDescriptor(makeXylophone().descriptor(4), 4);
    GmbSysExService svc;
    GmbIdentity id = {1, 0, 9, 0};
    svc.begin(id, &src);

    uint16_t total = 0;
    fetch(svc, 0, 1, &total);
    for (uint16_t i = 0; i < total; i++) {
        uint8_t req[GMB_CHUNK_REQUEST_LEN];
        chunkRequest(req, i);
        GmbSysExService::Reply reply;
        CHECK(svc.handleFrame(req, sizeof(req), 1, reply));
        for (uint16_t b = 1; b < reply.len - 1; b++) CHECK((reply.data[b] & 0x80) == 0);
        CHECK_EQ(reply.data[reply.len - 1], 0xF7);
    }
}

TEST(runtime_serves_the_same_document_over_sysex_and_http) {
    // One cache, one serializer: the HTTP body and the reassembled SysEx
    // transfer must be byte-identical.
    GmbFixture f = makeXylophone();
    TestRevisionStore store;
    GmbRuntime rt;
    GmbIdentity id = {0x1234, 0, 9, 0};
    rt.begin(id, &store);
    GmbBuildInput in = f.input();
    rt.rebuild(in, 0, false);

    static char http[GMB_DESCRIPTOR_MAX_BYTES];
    uint16_t http_len = rt.copyDescriptor(http, sizeof(http));
    CHECK(http_len > 0);

    uint16_t total = 0;
    fetch(rt.sysex(), 0, 10, &total);
    std::string rebuilt;
    for (uint16_t i = 0; i < total; i++) rebuilt += fetch(rt.sysex(), i, 10);
    CHECK_STR_EQ(rebuilt, std::string(http, http_len));
    CHECK_EQ(rt.descriptorBytes(), http_len);
    CHECK_EQ(rt.chunkCount(), total);
}

TEST(runtime_transfer_is_immune_to_a_rebuild_mid_flight) {
    GmbFixture f = makeXylophone();
    TestRevisionStore store;
    GmbRuntime rt;
    GmbIdentity id = {0x1234, 0, 9, 0};
    rt.begin(id, &store);
    GmbBuildInput in = f.input();
    rt.rebuild(in, 0, false);

    uint16_t total = 0;
    std::string first = fetch(rt.sysex(), 0, 100, &total);
    uint32_t rev_during = rt.sysex().transferRevision();

    // The user changes the configuration while the transfer is in flight.
    f.instruments[0].midi_channel = 7;
    GmbBuildInput in2 = f.input();
    rt.rebuild(in2, 110, false);
    CHECK(rt.revision() > rev_during);

    std::string rebuilt;
    for (uint16_t i = 0; i < total; i++) rebuilt += fetch(rt.sysex(), i, 120);
    CHECK_CONTAINS(rebuilt, "\"channel\":0");
    CHECK_NOT_CONTAINS(rebuilt, "\"channel\":7");

    // The next transfer sees the new descriptor.
    uint16_t total2 = 0;
    fetch(rt.sysex(), 0, 200, &total2);
    std::string next;
    for (uint16_t i = 0; i < total2; i++) next += fetch(rt.sysex(), i, 200);
    CHECK_CONTAINS(next, "\"channel\":7");
}
