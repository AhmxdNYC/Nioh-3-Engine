#pragma once
// Hook-loop breaker for the DXGI swapchain hooks (Present / Present1 / SetColorSpace1).
// GitHub #38/#100: another overlay re-hooks the call around ours, so the call nests through both hooks. At depth kLimit
// we call the DXGI function read from disk once ("Direct"). GitHub #138: when that DXGI function is itself patched in
// memory (Steam overlay inline detour), the direct call re-enters our hook again. Any deeper call is now dropped
// ("Skip", returns S_OK), so the stack is bounded at about kLimit + one more pass through the other overlay.
namespace loopbrk {
enum class Step { Hooked, Direct, Skip };
// depth = nesting depth on this thread INCLUDING the current call (1 = outermost).
constexpr Step Decide(int depth, int limit) {
    return depth < limit ? Step::Hooked : (depth == limit ? Step::Direct : Step::Skip);
}
}  // namespace loopbrk
