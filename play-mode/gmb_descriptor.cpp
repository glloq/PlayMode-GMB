#include "gmb_descriptor.h"
#include "gmb_gm_program.h"

// ============================================================================
// PlayMode — GMB descriptor serialization
// ============================================================================

namespace {

// Bounded, allocation-free JSON writer. Once it overflows it stops writing and
// stays overflowed, so a caller can never publish a truncated document.
class Writer {
public:
    Writer(char* buf, size_t cap) : _buf(buf), _cap(cap), _len(0), _over(false) {}

    void raw(const char* s) {
        while (*s) put(*s++);
    }

    // ASCII-only JSON string. Anything outside printable ASCII is escaped as
    // \u00XX, so the payload is 7-bit safe by construction.
    void str(const char* s) {
        put('"');
        if (s != nullptr) {
            for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
                unsigned char c = *p;
                if (c == '"')       raw("\\\"");
                else if (c == '\\') raw("\\\\");
                else if (c == '\n') raw("\\n");
                else if (c == '\r') raw("\\r");
                else if (c == '\t') raw("\\t");
                else if (c < 0x20 || c >= 0x7F) {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04X", (unsigned)c);
                    raw(esc);
                } else put((char)c);
            }
        }
        put('"');
    }

    void num(uint32_t v) {
        char tmp[11];
        uint8_t n = 0;
        if (v == 0) tmp[n++] = '0';
        while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
        while (n > 0) put(tmp[--n]);
    }

    void key(const char* k) { str(k); put(':'); }
    void boolean(bool b)    { raw(b ? "true" : "false"); }

    bool   ok()  const { return !_over; }
    size_t len() const { return _len; }

    bool finish() {
        if (_over) return false;
        if (_len >= _cap) { _over = true; return false; }
        _buf[_len] = '\0';
        return true;
    }

private:
    void put(char c) {
        // Keep one byte for the terminating NUL.
        if (_over || _len + 1 >= _cap) { _over = true; return; }
        _buf[_len++] = c;
    }
    char*  _buf;
    size_t _cap;
    size_t _len;
    bool   _over;
};

void writeNotes(Writer& w, const GmbInstrumentCaps& e) {
    w.key("notes");
    w.raw("{");
    if (e.contiguous) {
        // Only a fully chromatic span may be announced as a range.
        w.key("mode"); w.str("range");
        w.raw(","); w.key("min"); w.num(e.notes[0]);
        w.raw(","); w.key("max"); w.num(e.notes[e.note_count - 1]);
    } else {
        w.key("mode"); w.str("discrete");
        w.raw(","); w.key("list"); w.raw("[");
        for (uint8_t i = 0; i < e.note_count; i++) {
            if (i) w.raw(",");
            w.num(e.notes[i]);
        }
        w.raw("]");
    }
    w.raw("}");
}

// One voice per distinct actuator: a mechanism that can only sound one of its
// notes at a time (spec §5.4). Emitted only when an actuator really is shared —
// a chromatic instrument with one actuator per note has independent voices and
// the block would be pure noise.
void writeVoices(Writer& w, const GmbInstrumentCaps& e) {
    w.key("voices");
    w.raw("[");
    uint8_t emitted = 0;
    for (uint8_t i = 0; i < e.note_count; i++) {
        uint8_t act = e.note_actuator[i];
        bool first = true;
        for (uint8_t j = 0; j < i; j++) {
            if (e.note_actuator[j] == act) { first = false; break; }
        }
        if (!first) continue;

        if (emitted) w.raw(",");
        emitted++;
        w.raw("{");
        char id[12];
        snprintf(id, sizeof(id), "a%u", (unsigned)act);
        w.key("id"); w.str(id);
        w.raw(","); w.key("notes"); w.raw("{");
        w.key("mode"); w.str("discrete");
        w.raw(","); w.key("list"); w.raw("[");
        uint8_t n = 0;
        for (uint8_t j = 0; j < e.note_count; j++) {
            if (e.note_actuator[j] != act) continue;
            if (n++) w.raw(",");
            w.num(e.notes[j]);
        }
        w.raw("]}}");
    }
    w.raw("]");
}

void writePolyphony(Writer& w, const GmbInstrumentCaps& e, bool with_constraints) {
    w.key("polyphony");
    w.raw("{");
    w.key("max"); w.num(e.polyphony_max);
    if (with_constraints && e.constraint_count > 0) {
        w.raw(","); w.key("constraints"); w.raw("[");
        for (uint8_t i = 0; i < e.constraint_count; i++) {
            if (i) w.raw(",");
            const GmbConstraint& c = e.constraints[i];
            w.raw("{");
            if (c.per_group) {
                w.key("type");  w.str("max_simultaneous_per_group");
                w.raw(","); w.key("group"); w.str(c.group);
                w.raw(","); w.key("max");   w.num(c.max);
            } else {
                w.key("type"); w.str("one_note_per_voice");
            }
            w.raw("}");
        }
        w.raw("]");
    }
    w.raw("}");
}

// Spec §5.6 two-phase timing. PlayMode has NO preparation phase: a note is one
// commanded gesture that is itself the excitation, so `prepare` is omitted
// rather than invented (this is exactly the "— / frappe" row the spec's
// conformity table gives PlayMode-GMB). Anything unmeasured is omitted, never
// emitted as 0.
void writeTiming(Writer& w, const GmbInstrumentCaps& e) {
    w.key("timing");
    w.raw("{");
    bool first = true;
    if (e.has_excite_latency) {
        w.key("excite"); w.raw("{");
        w.key("latency_ms"); w.num(e.excite_latency_ms);
        w.raw("}");
        first = false;
    }
    if (e.has_rearticulation) {
        if (!first) w.raw(",");
        w.key("rearticulation_ms"); w.num(e.rearticulation_ms);
        first = false;
    }
    if (e.has_release) {
        if (!first) w.raw(",");
        w.key("release_ms"); w.num(e.release_ms);
    }
    w.raw("}");
}

void writeExpression(Writer& w, const GmbInstrumentCaps& e, bool with_cc) {
    w.key("expression");
    w.raw("{");
    w.key("velocity"); w.boolean(e.velocity);
    // PlayMode's dispatcher handles NoteOn/NoteOff/CC only: pitch bend and
    // aftertouch are known-absent, not unknown.
    w.raw(","); w.key("pitch_bend"); w.raw("{"); w.key("supported"); w.boolean(false); w.raw("}");
    w.raw(","); w.key("channel_aftertouch"); w.boolean(false);
    w.raw(","); w.key("poly_aftertouch");    w.boolean(false);
    if (with_cc && e.cc_count > 0) {
        w.raw(","); w.key("cc"); w.raw("[");
        for (uint8_t i = 0; i < e.cc_count; i++) {
            if (i) w.raw(",");
            w.num(e.ccs[i]);
        }
        w.raw("]");
    }
    w.raw("}");
}

void writeInstrument(Writer& w, const GmbInstrumentCaps& e, uint8_t level) {
    w.raw("{");
    w.key("channel"); w.num(e.channel);
    w.raw(","); w.key("configured"); w.boolean(e.configured);

    if (e.name[0] != '\0') { w.raw(","); w.key("name"); w.str(e.name); }

    // `type` is always declared (a generic, supported key when the user has not
    // chosen a musical profile); `subtype` and `gm_program` only when known.
    w.raw(","); w.key("type"); w.str(e.type != nullptr ? e.type : GMB_GENERIC_TYPE);
    if (e.subtype != nullptr && e.subtype[0] != '\0') {
        w.raw(","); w.key("subtype"); w.str(e.subtype);
    }
    if (e.gm_program <= 127) {
        w.raw(","); w.key("gm_program"); w.num(e.gm_program);
    }

    if (!e.configured || e.note_count == 0) {
        // Spec §5.1: the slot exists but is not defined. GMB falls back to
        // manual entry WITHOUT overwriting a previous configuration.
        w.raw("}");
        return;
    }

    w.raw(","); writeNotes(w, e);

    // Voices only make sense when one mechanism really serves several notes;
    // one actuator per note means independent voices and the block is noise
    // (spec §5.4 makes it optional for exactly that case).
    if (level == GMB_DETAIL_FULL && e.shared_actuators &&
        e.distinct_actuators > 0 && e.distinct_actuators <= GMB_MAX_VOICES) {
        w.raw(","); writeVoices(w, e);
    }

    w.raw(","); writePolyphony(w, e, level < GMB_DETAIL_MINIMAL);

    if (level < GMB_DETAIL_MINIMAL &&
        (e.has_excite_latency || e.has_rearticulation || e.has_release)) {
        w.raw(",");
        writeTiming(w, e);
    }

    w.raw(","); writeExpression(w, e, level < GMB_DETAIL_CORE);

    // Velocity is also surfaced at instrument level: consumers that only look
    // at expression keep working, and simple ones get it directly.
    w.raw(","); w.key("velocity"); w.boolean(e.velocity);

    if (level < GMB_DETAIL_CORE) {
        w.raw(","); w.key("physical"); w.raw("{");
        w.key("family"); w.str(GMB_PHYSICAL_FAMILY);
        // `physical` is a free extension namespace (spec §5.9), so this is where
        // PlayMode says whether NoteOff means anything musically. Not every note
        // is percussion: a key or a hit-and-hold coil sustains until released,
        // while a strike is over the moment it lands.
        w.raw(","); w.key("note_off");
        if (e.has_hold_notes && e.has_attack_notes) w.str("mixed");
        else if (e.has_hold_notes)                  w.str("releases");
        else                                        w.str("ignored");
        w.raw("}");
    }

    w.raw("}");
}

} // namespace

size_t gmbRenderDescriptor(const GmbCapabilitySnapshot& snap, uint32_t revision,
                           uint8_t level, char* out, size_t cap) {
    if (out == nullptr || cap == 0) return 0;
    Writer w(out, cap);
    w.raw("{");
    w.key("gmb_descriptor"); w.num(2);
    w.raw(","); w.key("revision"); w.num(revision);
    w.raw(","); w.key("device"); w.raw("{");
    w.key("name"); w.str(snap.device_name);
    w.raw(","); w.key("model"); w.str("PlayMode-GMB");
    w.raw("}");
    w.raw(","); w.key("instruments"); w.raw("[");
    for (uint8_t i = 0; i < snap.instrument_count; i++) {
        if (i) w.raw(",");
        writeInstrument(w, snap.instruments[i], level);
    }
    w.raw("]}");
    if (!w.finish()) return 0;
    return w.len();
}

size_t gmbBuildDescriptor(const GmbCapabilitySnapshot& snap, uint32_t revision,
                          char* out, size_t cap, uint8_t* used_level) {
    for (uint8_t level = GMB_DETAIL_FULL; level <= GMB_DETAIL_MINIMAL; level++) {
        size_t n = gmbRenderDescriptor(snap, revision, level, out, cap);
        if (n > 0) {
            if (used_level != nullptr) *used_level = level;
            return n;
        }
    }
    if (used_level != nullptr) *used_level = GMB_DETAIL_MINIMAL;
    return 0;
}

uint32_t gmbHash(const char* data, size_t len) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)data[i];
        h *= 16777619UL;
    }
    return h;
}
