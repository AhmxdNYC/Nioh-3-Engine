#include "../src/meshletwait.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

using meshletwait::State;
static State* enrolling;
static meshletwait::WaitFn nativeWait;
static volatile uint8_t* expectedAddress;
static BOOL WINAPI ClearDuringEnrollment(volatile VOID* p, PVOID value, SIZE_T size, DWORD timeout) {
    assert(p == expectedAddress && size == 1 && timeout == 1);
    assert(*static_cast<uint8_t*>(value) == 1);
    enrolling->Complete(expectedAddress); // clear + wake before kernel enrollment
    return nativeWait(p, value, size, timeout);
}
int main() {
    const DWORD threads[] = {11,22,33};
    assert(meshletwait::KnownThread(11,44,threads,3));
    assert(meshletwait::KnownThread(44,44,threads,3));
    assert(!meshletwait::KnownThread(55,44,threads,3)); // newly created thread
    assert(!meshletwait::KnownThread(0,44,threads,3)); // query failure
    assert(!meshletwait::KnownThread(11,44,nullptr,0)); // absent enumeration
    // Publication failure injection uses the exact allocation-free production helper.
    uint8_t a[] = {1,2}, b[] = {3,4};
    const uint8_t ao[] = {1,2}, bo[] = {3,4}, an[] = {5,6}, bn[] = {7,8};
    meshletwait::Site pair[] = {{a,ao,an,2}, {b,bo,bn,2}};
    int flushes = 0;
    auto result = meshletwait::Publish(pair,2,[&] { ++flushes; if (flushes==1) b[0]=99; return true; });
    assert(result == meshletwait::Publication::Unchanged && flushes==2);
    assert(!memcmp(a,ao,2) && !memcmp(b,bo,2)); // both restored before return
    a[0] = 99; flushes = 0;
    result = meshletwait::Publish(pair,2,[&] { ++flushes; return true; });
    assert(result == meshletwait::Publication::Unchanged && !flushes && b[0]==3);
    a[0] = 1;
    result = meshletwait::Publish(pair,2,[] { return true; });
    assert(result == meshletwait::Publication::Applied && a[0]==5 && b[0]==7);
    memcpy(a,ao,2); memcpy(b,bo,2);
    result = meshletwait::Publish(pair,2,[] { return false; });
    assert(result == meshletwait::Publication::Uncertain && a[0]==1 && b[0]==3);
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    nativeWait = reinterpret_cast<meshletwait::WaitFn>(GetProcAddress(kb, "WaitOnAddress"));
    auto wake = reinterpret_cast<meshletwait::WakeFn>(GetProcAddress(kb, "WakeByAddressAll"));
    assert(nativeWait && wake);
    State s; s.wait = nativeWait; s.wake = wake; s.enabled = true;
    volatile uint8_t f = 0;
    s.Round(&f); assert(s.rounds == 0); // already complete
    f = 1; s.enabled = false; s.Round(&f); assert(f == 1 && s.rounds == 0);
    s.Complete(&f); assert(!f && !s.wakes); // disabled writer is still original clear
    s.enabled = true;
    s.wake = nullptr; f = 1; s.Round(&f); assert(f == 1 && !s.rounds);
    s.wake = wake;
    enrolling = &s; expectedAddress = &f; s.wait = ClearDuringEnrollment;
    s.Round(&f); assert(!f && s.rounds == 1 && s.wakes == 1);
    s.wait = nativeWait;
    // A timeout returns to the native recheck, never declares completion itself.
    f = 1; s.Round(&f); assert(f == 1 && s.timeouts > 0);
    for (int run = 0; run < 50; ++run) {
        f = 1; volatile uint8_t other = 1;
        std::atomic<bool> entered{false}, returned{false};
        std::thread reader([&] {
            entered = true;
            while (f) s.Round(&f); // actual preserved native recheck contract
            returned = true;
        });
        while (!entered) Sleep(0);
        wake(const_cast<uint8_t*>(&other)); // unrelated address cannot complete us
        Sleep(2);
        assert(!returned && f == 1);
        // Even a wake of the right address without a clear cannot complete us.
        wake(const_cast<uint8_t*>(&f)); Sleep(1); assert(!returned && f == 1);
        s.Complete(&f);
        reader.join(); assert(returned && !f);
    }
    assert(s.errors == 0);
    std::printf("PASS: pre-completion, disabled, missing API, enrollment race, timeout recheck, 50 concurrent exact-address wake/clear cycles; rounds=%ld timeouts=%ld wakes=%ld\n", s.rounds, s.timeouts, s.wakes);
}
