#ifndef GMB_TEST_ARDUINO_SHIM_H
#define GMB_TEST_ARDUINO_SHIM_H

// ============================================================================
// PlayMode — minimal Arduino shim for the native (host) test build
// ============================================================================
//
// The GMB layer is deliberately written against plain C++ and the PlayMode
// configuration structs, so it compiles and runs on a host. This shim provides
// only what config.h / midi_types.h / types.h / gmb_*.{h,cpp} actually use.
// It is NOT a general Arduino emulation and is never compiled into firmware.
//

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#if defined(__GLIBC__)
#include <features.h>
#endif

// glibc only gained strlcpy in 2.38; provide it elsewhere.
#if !defined(__GLIBC_PREREQ) || !__GLIBC_PREREQ(2, 38)
static inline size_t strlcpy(char* dst, const char* src, size_t size) {
    size_t len = strlen(src);
    if (size > 0) {
        size_t n = (len >= size) ? size - 1 : len;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
#endif

typedef uint8_t byte;

// Deliberately functions, never macros: a `min`/`max` macro would break
// <algorithm> and the standard containers the tests use.
template <typename T> static inline T min(T a, T b) { return a < b ? a : b; }
template <typename T> static inline T max(T a, T b) { return a > b ? a : b; }

// Monotonic microsecond clock. The host build only needs it to be increasing;
// the parser uses it to timestamp a message's first data byte.
static inline int64_t esp_timer_get_time() {
    static int64_t t = 0;
    return (t += 100);
}

static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif // GMB_TEST_ARDUINO_SHIM_H
