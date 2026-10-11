// mpcs_harness_stubs.cpp - what src/mpccheck.cpp links against in the game build (gpuopt.cpp / gpuopt_thunks.asm), for the harness.
#include <cstdint>
namespace gpuopt {
bool Batching() { return false; }
}  // namespace gpuopt
extern "C" {
void* gpuopt_orig0[96] = {};
void* gpuopt_orig1[96] = {};
}
