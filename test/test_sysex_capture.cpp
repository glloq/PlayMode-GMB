#include "test_framework.h"
#include "midi_parser.h"
#include "gmb_sysex.h"

// ============================================================================
// SysEx capture in the byte-by-byte MIDI parser
// ============================================================================
//
// The note path must be untouched: SysEx bytes never fabricate a note event,
// and a note stream interleaved with SysEx still parses correctly.
//

static void feedAll(MidiParser& p, const std::vector<uint8_t>& bytes,
                    std::vector<MidiMessage>* messages = nullptr) {
    for (size_t i = 0; i < bytes.size(); i++) {
        if (p.feed(bytes[i]) && messages) messages->push_back(p.getMessage());
    }
}

TEST(a_complete_gmb_request_is_captured_with_its_boundaries) {
    MidiParser p;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7});
    CHECK(p.hasSysEx());
    CHECK_EQ(p.sysExLength(), 6);
    CHECK_EQ(p.sysExData()[0], 0xF0);
    CHECK_EQ(p.sysExData()[5], 0xF7);

    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_HANDSHAKE);

    p.clearSysEx();
    CHECK(!p.hasSysEx());
}

TEST(a_chunk_request_is_captured_and_decoded) {
    MidiParser p;
    uint8_t idx[2];
    gmbEncode14(300, idx);
    feedAll(p, {0xF0, 0x7D, 0x00, 0x10, 0x00, idx[0], idx[1], 0xF7});
    CHECK(p.hasSysEx());
    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_DESCRIPTOR_CHUNK);
    CHECK_EQ(req.chunk_index, 300);
}

TEST(sysex_never_produces_a_note_event) {
    MidiParser p;
    std::vector<MidiMessage> msgs;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7}, &msgs);
    CHECK_EQ(msgs.size(), 0u);
}

TEST(notes_around_a_sysex_frame_still_parse) {
    MidiParser p;
    std::vector<MidiMessage> msgs;
    feedAll(p, {
        0x90, 60, 100,                          // NoteOn ch0
        0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7,     // GMB handshake request
        0x80, 60, 0                             // NoteOff ch0
    }, &msgs);
    CHECK_EQ(msgs.size(), 2u);
    CHECK_EQ((int)msgs[0].type, (int)MIDI_NOTE_ON);
    CHECK_EQ(msgs[0].data1, 60);
    CHECK_EQ((int)msgs[1].type, (int)MIDI_NOTE_OFF);
    CHECK(p.hasSysEx());
}

TEST(sysex_clears_running_status_as_the_midi_spec_requires) {
    MidiParser p;
    std::vector<MidiMessage> msgs;
    feedAll(p, {
        0x90, 60, 100,                       // NoteOn, establishes running status
        0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7,  // SysEx
        62, 90                               // bare data bytes: must NOT become a note
    }, &msgs);
    CHECK_EQ(msgs.size(), 1u);
}

TEST(realtime_bytes_inside_sysex_do_not_corrupt_the_capture) {
    MidiParser p;
    // 0xF8 (clock) may be interleaved anywhere, including inside a SysEx frame.
    feedAll(p, {0xF0, 0x7D, 0xF8, 0x00, 0x01, 0xF8, 0x00, 0xF7});
    CHECK(p.hasSysEx());
    CHECK_EQ(p.sysExLength(), 6);
    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_HANDSHAKE);
}

TEST(an_oversized_frame_is_discarded_not_truncated) {
    MidiParser p;
    std::vector<uint8_t> big = {0xF0, 0x7D, 0x00, 0x01, 0x00};
    for (int i = 0; i < 200; i++) big.push_back(0x01);
    big.push_back(0xF7);
    feedAll(p, big);
    CHECK(!p.hasSysEx());
    CHECK_EQ(p.oversizedSysExCount(), 1u);
}

TEST(a_frame_cut_short_by_a_new_status_byte_is_dropped) {
    MidiParser p;
    std::vector<MidiMessage> msgs;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x90, 60, 100}, &msgs);
    CHECK(!p.hasSysEx());
    CHECK_EQ(msgs.size(), 1u);          // the NoteOn still lands
    CHECK_EQ(msgs[0].data1, 60);
}

TEST(a_restarted_frame_replaces_the_partial_one) {
    MidiParser p;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x01,               // truncated
                0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7}); // fresh, complete
    CHECK(p.hasSysEx());
    CHECK_EQ(p.sysExLength(), 6);
    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_HANDSHAKE);
}

TEST(reset_clears_a_pending_frame) {
    MidiParser p;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7});
    CHECK(p.hasSysEx());
    p.reset();
    CHECK(!p.hasSysEx());
    CHECK_EQ(p.sysExLength(), 0);
}

TEST(a_foreign_sysex_is_captured_but_decodes_to_nothing) {
    MidiParser p;
    feedAll(p, {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7});   // Universal Identity Request
    CHECK(p.hasSysEx());
    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_NONE);
}

TEST(back_to_back_frames_are_each_delivered) {
    MidiParser p;
    feedAll(p, {0xF0, 0x7D, 0x00, 0x01, 0x00, 0xF7});
    CHECK(p.hasSysEx());
    p.clearSysEx();
    feedAll(p, {0xF0, 0x7D, 0x00, 0x10, 0x00, 0x02, 0x00, 0xF7});
    CHECK(p.hasSysEx());
    GmbRequest req = gmbDecodeRequest(p.sysExData(), p.sysExLength());
    CHECK_EQ((int)req.kind, (int)GMB_REQ_DESCRIPTOR_CHUNK);
    CHECK_EQ(req.chunk_index, 2);
}
