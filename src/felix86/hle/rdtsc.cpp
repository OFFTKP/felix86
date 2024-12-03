#include "felix86/hle/rdtsc.hpp"

void felix86_rdtsc(ThreadState* state) {
    WARN("Rdtsc called, ignoring...");
    state->SetGpr(X86_REF_RAX, 0);
    state->SetGpr(X86_REF_RDX, 0);
}