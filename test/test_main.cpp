#include "test_framework.h"

// ============================================================================
// PlayMode — native GMB test runner
// ============================================================================

int main() {
    printf("PlayMode — General-Midi-Boop v2 native tests\n");
    printf("--------------------------------------------------------\n");
    return gmbRunAllTests();
}
