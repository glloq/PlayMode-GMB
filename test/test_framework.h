#ifndef GMB_TEST_FRAMEWORK_H
#define GMB_TEST_FRAMEWORK_H

// ============================================================================
// PlayMode — tiny assertion framework for the native GMB tests
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

struct GmbTestState {
    int checks = 0;
    int failures = 0;
    const char* current = "";
};

inline GmbTestState& gmbTestState() {
    static GmbTestState s;
    return s;
}

inline void gmbTestBegin(const char* name) {
    gmbTestState().current = name;
}

inline void gmbTestFail(const char* file, int line, const std::string& what) {
    GmbTestState& s = gmbTestState();
    s.failures++;
    fprintf(stderr, "  FAIL  [%s] %s:%d\n        %s\n",
            s.current, file, line, what.c_str());
}

#define CHECK(cond)                                                            \
    do {                                                                       \
        gmbTestState().checks++;                                               \
        if (!(cond)) gmbTestFail(__FILE__, __LINE__, "expected: " #cond);      \
    } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        gmbTestState().checks++;                                               \
        auto _a = (actual);                                                    \
        auto _e = (expected);                                                  \
        if (!(_a == _e)) {                                                     \
            char _b[256];                                                      \
            snprintf(_b, sizeof(_b), "%s == %s  (got %lld, want %lld)",        \
                     #actual, #expected, (long long)_a, (long long)_e);        \
            gmbTestFail(__FILE__, __LINE__, _b);                               \
        }                                                                      \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                         \
    do {                                                                       \
        gmbTestState().checks++;                                               \
        std::string _a(actual);                                                \
        std::string _e(expected);                                              \
        if (_a != _e) {                                                        \
            gmbTestFail(__FILE__, __LINE__,                                    \
                        "got \"" + _a + "\", want \"" + _e + "\"");            \
        }                                                                      \
    } while (0)

// Substring assertions against the rendered descriptor.
#define CHECK_CONTAINS(haystack, needle)                                       \
    do {                                                                       \
        gmbTestState().checks++;                                               \
        std::string _h(haystack);                                              \
        std::string _n(needle);                                                \
        if (_h.find(_n) == std::string::npos) {                                \
            gmbTestFail(__FILE__, __LINE__, "missing \"" + _n + "\"");         \
        }                                                                      \
    } while (0)

#define CHECK_NOT_CONTAINS(haystack, needle)                                   \
    do {                                                                       \
        gmbTestState().checks++;                                               \
        std::string _h(haystack);                                              \
        std::string _n(needle);                                                \
        if (_h.find(_n) != std::string::npos) {                                \
            gmbTestFail(__FILE__, __LINE__, "unexpected \"" + _n + "\"");      \
        }                                                                      \
    } while (0)

typedef void (*GmbTestFn)();

struct GmbTestCase {
    const char* name;
    GmbTestFn   fn;
};

inline std::vector<GmbTestCase>& gmbTestRegistry() {
    static std::vector<GmbTestCase> r;
    return r;
}

struct GmbTestRegistrar {
    GmbTestRegistrar(const char* name, GmbTestFn fn) {
        gmbTestRegistry().push_back({name, fn});
    }
};

#define TEST(name)                                                             \
    static void name();                                                        \
    static GmbTestRegistrar _reg_##name(#name, name);                          \
    static void name()

inline int gmbRunAllTests() {
    GmbTestState& s = gmbTestState();
    for (const GmbTestCase& tc : gmbTestRegistry()) {
        int before = s.failures;
        gmbTestBegin(tc.name);
        tc.fn();
        printf("%-52s %s\n", tc.name, (s.failures == before) ? "ok" : "FAILED");
    }
    printf("\n%d checks, %d failure(s), %zu test(s)\n",
           s.checks, s.failures, gmbTestRegistry().size());
    return s.failures == 0 ? 0 : 1;
}

#endif // GMB_TEST_FRAMEWORK_H
