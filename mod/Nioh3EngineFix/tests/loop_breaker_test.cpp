// GitHub #138: the #100 breaker called DXGI "directly", but Steam had detoured that DXGI function back into our hook,
// so the loop never ended. Simulates both loops and checks the nesting stays bounded and the outer call returns.
#include "../src/loop_breaker.h"
#include <cstdio>
#include <cstdlib>
static int g_depth = 0, g_maxDepth = 0, g_realCalls = 0, g_skips = 0;
static bool g_realLoopsBack = false;  // true = DXGI function itself detoured into a hook that calls ours again
static int Hook(int v);
static int Other(int v) { return Hook(v); }                                    // other overlay: calls back through the vtable
static int Real(int v) { ++g_realCalls; return g_realLoopsBack ? Other(v) : 0; }  // DXGI (maybe detoured)
static int Hook(int v) {
    struct G { G() { ++g_depth; if (g_depth > g_maxDepth) g_maxDepth = g_depth; } ~G() { --g_depth; } } g;
    switch (loopbrk::Decide(g_depth, 8)) {
        case loopbrk::Step::Direct: return Real(v);
        case loopbrk::Step::Skip: ++g_skips; return 0;
        default: return Other(v);  // "original" = the other overlay's hook
    }
}
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s line %d\n", #c, __LINE__); std::exit(1); } } while (0)
int main() {
    CHECK(loopbrk::Decide(1, 8) == loopbrk::Step::Hooked);
    CHECK(loopbrk::Decide(7, 8) == loopbrk::Step::Hooked);
    CHECK(loopbrk::Decide(8, 8) == loopbrk::Step::Direct);
    CHECK(loopbrk::Decide(9, 8) == loopbrk::Step::Skip);
    // #100: clean DXGI on disk -> one direct call, no skip
    g_realLoopsBack = false; g_maxDepth = g_realCalls = g_skips = 0;
    CHECK(Hook(0) == 0); CHECK(g_depth == 0); CHECK(g_maxDepth == 8); CHECK(g_realCalls == 1); CHECK(g_skips == 0);
    // #138: DXGI detoured back into the loop -> bounded at 9, one skip
    g_realLoopsBack = true; g_maxDepth = g_realCalls = g_skips = 0;
    CHECK(Hook(0) == 0); CHECK(g_depth == 0); CHECK(g_maxDepth == 9); CHECK(g_realCalls == 1); CHECK(g_skips == 1);
    std::printf("loop_breaker_test: PASS\n");
    return 0;
}
