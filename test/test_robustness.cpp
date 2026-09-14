#include "test_framework.h"
#include "gmb_sysex.h"
#include "gmb_sysex_service.h"
#include "gmb_fixture.h"
#include "gmb_test_source.h"

// ============================================================================
// Robustness: a malformed or hostile SysEx frame must produce no reply, no
// crash, and above all must never reach an actuator.
// ============================================================================

namespace {

struct Fixture {
    TestDescriptorSource src;
    GmbSysExService      svc;
    Fixture() {
        src.setDescriptor(makeXylophone().descriptor(1), 1);
        GmbIdentity id = {0x2222, 0, 9, 0};
        svc.begin(id, &src);
    }
    // Returns true when the service answered.
    bool feed(const std::vector<uint8_t>& frame, uint32_t now_ms = 100) {
        GmbSysExService::Reply reply;
        reply.len = 0xFFFF;
        bool answered = svc.handleFrame(frame.empty() ? nullptr : frame.data(),
                                        (uint16_t)frame.size(), now_ms, reply);
        if (!answered) {
            // A refused frame must leave nothing to transmit.
            gmbTestState().checks++;
            if (reply.len != 0) {
                gmbTestFail(__FILE__, __LINE__, "refused frame produced bytes");
            }
        }
        return answered;
    }
};

} // namespace

TEST(a_well_formed_handshake_is_the_control_case) {
    Fixture f;
    CHECK(f.feed({0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7}));
}

TEST(wrong_manufacturer_id_is_ignored) {
    Fixture f;
    CHECK(!f.feed({0xF0, 0x7E, 0x00, 0x01, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x43, 0x00, 0x01, 0x00, 0xF7}));
    // The MIDI Universal Identity Request must not be answered as a GMB frame.
    CHECK(!f.feed({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
}

TEST(wrong_gmb_device_id_is_ignored) {
    Fixture f;
    CHECK(!f.feed({0xF0, 0x7D, 0x01, 0x01, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x7F, 0x01, 0x00, 0xF7}));
}

TEST(wrong_direction_is_ignored) {
    Fixture f;
    // A response or a notification on the wire is another device talking.
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x01, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x02, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x10, 0x01, 0x00, 0x00, 0xF7}));
}

TEST(unknown_block_is_ignored) {
    Fixture f;
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x42, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x00, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x7F, 0x00, 0xF7}));
    CHECK(f.svc.stats().invalid >= 3u);
}

TEST(truncated_frames_are_ignored) {
    Fixture f;
    CHECK(!f.feed({}));
    CHECK(!f.feed({0xF0}));
    CHECK(!f.feed({0xF0, 0x7D}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x00}));            // no F7
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x10, 0x00, 0x00, 0xF7})); // chunk idx short
}

TEST(missing_terminator_is_ignored) {
    Fixture f;
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x00, 0x00}));
    CHECK(!f.feed({0x00, 0x7D, 0x00, 0x01, 0x00, 0xF7}));   // no F0 either
}

TEST(oversized_frames_are_ignored) {
    Fixture f;
    std::vector<uint8_t> big;
    big.push_back(0xF0); big.push_back(0x7D); big.push_back(0x00);
    big.push_back(0x01); big.push_back(0x00);
    for (int i = 0; i < 512; i++) big.push_back(0x00);
    big.push_back(0xF7);
    CHECK(!f.feed(big));
}

TEST(a_handshake_padded_to_the_wrong_length_is_ignored) {
    Fixture f;
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x00, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x01, 0x00, 0x00, 0x00, 0xF7}));
}

TEST(an_8_bit_payload_byte_is_ignored) {
    Fixture f;
    // Real SysEx payload is 7-bit; an 8-bit byte means corruption or a foreign
    // frame, never something to interpret.
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x10, 0x00, 0x80, 0x00, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xF7}));
    CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x81, 0x00, 0xF7}));
}

TEST(a_null_frame_is_ignored) {
    Fixture f;
    GmbSysExService::Reply reply;
    CHECK(!f.svc.handleFrame(nullptr, 6, 10, reply));
    GmbRequest req = gmbDecodeRequest(nullptr, 6);
    CHECK_EQ((int)req.kind, (int)GMB_REQ_NONE);
}

TEST(a_flood_of_invalid_frames_never_answers) {
    Fixture f;
    for (int i = 0; i < 500; i++) {
        CHECK(!f.feed({0xF0, 0x7D, 0x00, 0x55, 0x00, 0xF7}, 3000));
    }
    CHECK(f.svc.stats().handshakes == 0u);
    CHECK(f.svc.stats().chunk_requests == 0u);
}

TEST(a_hostile_stream_never_starts_a_transfer) {
    Fixture f;
    f.feed({0xF0, 0x7D, 0x00, 0x10, 0x01, 0x00, 0x00, 0xF7});  // wrong direction
    f.feed({0xF0, 0x7D, 0x00, 0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7});
    CHECK(!f.svc.transferActive());
}

TEST(decoder_never_reports_a_request_for_a_response_frame) {
    // Our own handshake reply fed back in must not be treated as a request.
    uint8_t reply[GMB_HANDSHAKE_REPLY_LEN];
    gmbEncodeHandshake(reply, 0x1234, 0, 9, 0, 100, 1, 3);
    GmbRequest req = gmbDecodeRequest(reply, sizeof(reply));
    CHECK_EQ((int)req.kind, (int)GMB_REQ_NONE);

    uint8_t notify[GMB_NOTIFY_LEN];
    gmbEncodeNotification(notify, 7, GMB_CHANGE_INSTRUMENTS);
    GmbRequest req2 = gmbDecodeRequest(notify, sizeof(notify));
    CHECK_EQ((int)req2.kind, (int)GMB_REQ_NONE);
}

TEST(chunk_encoder_refuses_a_non_ascii_payload) {
    uint8_t out[GMB_SYSEX_OUT_MAX];
    const char bad[] = {'a', (char)0xC3, 'b', '\0'};
    CHECK_EQ(gmbEncodeChunk(out, sizeof(out), 1, 0, bad, 3), 0);
}

TEST(chunk_encoder_refuses_an_oversized_payload) {
    uint8_t out[GMB_SYSEX_OUT_MAX];
    std::string big(GMB_CHUNK_PAYLOAD_MAX + 1, 'a');
    CHECK_EQ(gmbEncodeChunk(out, sizeof(out), 1, 0, big.c_str(),
                            (uint16_t)big.size()), 0);
}

TEST(chunk_encoder_refuses_a_short_output_buffer) {
    uint8_t out[16];
    std::string payload(100, 'a');
    CHECK_EQ(gmbEncodeChunk(out, sizeof(out), 1, 0, payload.c_str(), 100), 0);
}

TEST(descriptor_render_into_a_short_buffer_never_truncates) {
    GmbFixture f = makeXylophone();
    GmbCapabilitySnapshot snap;
    f.build(snap);
    char small[64];
    memset(small, 'Z', sizeof(small));
    uint8_t level = 0;
    size_t n = gmbBuildDescriptor(snap, 1, small, sizeof(small), &level);
    CHECK_EQ(n, 0u);   // refused, not truncated
}

TEST(descriptor_degrades_optional_detail_before_it_overflows) {
    // Shared actuators make the voices block the biggest optional element.
    GmbFixture f;
    uint8_t inst = f.addInstrument("Dense", 0, true, 13);
    for (uint8_t i = 0; i < 8; i++) f.addSolenoid((uint8_t)(1 + i));
    for (uint8_t n = 0; n < 40; n++) f.map(inst, (uint8_t)(40 + n), (uint8_t)(1 + (n % 8)));

    GmbCapabilitySnapshot snap;
    f.build(snap);

    static char buf[GMB_DESCRIPTOR_MAX_BYTES];
    uint8_t full_level = 0;
    size_t full = gmbBuildDescriptor(snap, 1, buf, sizeof(buf), &full_level);
    CHECK(full > 0);
    CHECK_EQ((int)full_level, (int)GMB_DETAIL_FULL);
    CHECK_CONTAINS(std::string(buf, full), "\"voices\"");

    // Give it just under what the full form needs: it degrades, stays valid
    // JSON, and still carries the notes.
    uint8_t level = 0;
    size_t n = gmbBuildDescriptor(snap, 1, buf, full - 10, &level);
    CHECK(n > 0);
    CHECK(level > GMB_DETAIL_FULL);
    std::string json(buf, n);
    CHECK_NOT_CONTAINS(json, "\"voices\"");
    CHECK_CONTAINS(json, "\"notes\"");
    CHECK_EQ(json[json.size() - 1], '}');
}

TEST(a_full_controller_fits_the_descriptor_budget) {
    // Eight logical instruments spanning the whole actuator budget.
    GmbFixture f;
    uint8_t next = 0;
    for (uint8_t i = 0; i < MAX_INSTRUMENTS; i++) {
        uint8_t inst = f.addInstrument("Instrument slot", i, true, (uint8_t)(i * 8));
        for (uint8_t n = 0; n < 16; n++) f.addSolenoid((uint8_t)(next + n), SOL_FRAPPE,
                                                       5, 40, (uint16_t)(3 + i));
        // Sparse notes: the discrete form, i.e. the bulky one.
        for (uint8_t n = 0; n < 16; n++) {
            f.map(inst, (uint8_t)(2 * n + i * 2), (uint8_t)(next + n));
        }
        next += 16;
    }
    GmbCapabilitySnapshot snap;
    f.build(snap);
    CHECK_EQ(snap.instrument_count, MAX_INSTRUMENTS);

    static char buf[GMB_DESCRIPTOR_MAX_BYTES];
    uint8_t level = 0;
    size_t n = gmbBuildDescriptor(snap, 4294967295u, buf, sizeof(buf), &level);
    CHECK(n > 0);
    CHECK_EQ((int)level, (int)GMB_DETAIL_FULL);
    CHECK(n < GMB_DESCRIPTOR_MAX_BYTES);
}
