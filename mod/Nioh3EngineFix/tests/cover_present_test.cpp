#include "../src/cover_present.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace cover_present;
static unsigned checks = 0;
static void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
int main() {
    const Identity current{11, 22, 33, 44};
    const Ticket good{current, true, true, false};
    Check(Accept(good, current, 0, false), "successful exact frame");
    Check(!Accept(good, current, 0x087a0001, false), "occluded is not visible admission");
    Check(!Accept(good, current, static_cast<int32_t>(0x887a0001u), false), "invalid-call frame rejected");
    Check(!Accept(good, current, static_cast<int32_t>(0x887a000au), false), "still-drawing frame rejected");
    Check(!Accept(good, current, 1, false), "other successful status rejected");
    Check(!Accept(good, current, 0, true), "cancel between draw and Present return");
    for (unsigned field = 0; field < 4; ++field) {
        Identity changed = current;
        uint64_t* ids[] = {&changed.token, &changed.session, &changed.swapchain, &changed.contextGeneration};
        ++*ids[field];
        Check(!Accept(good, changed, 0, false), "identity changed after submission");
        *ids[field] = 0;
        Ticket invalid = good; invalid.id = changed;
        Check(!Accept(invalid, changed, 0, false), "zero identity rejected even when matching");
    }
    Ticket bad = good; bad.submitted = false;
    Check(!Accept(bad, current, 0, false), "quad built but GPU submission skipped");
    bad = good; bad.outermost = false;
    Check(!Accept(bad, current, 0, false), "nested Present cannot admit outer ticket");
    bad = good; bad.testPresent = true;
    Check(!Accept(bad, current, 0, false), "Present TEST cannot admit");
    Check(!Accept(Ticket{}, current, 0, false), "new invocation cannot reuse prior ticket");
    Check(Accept(good, current, 0, false), "successful final fallback may admit exact ticket");

    const InputLease live{11, 22, 10000, 10000, 16000, true, false};
    Check(Captures(live, 10000), "fresh worker and actual covered Present");
    Check(Captures(live, 11499), "last fresh millisecond");
    Check(!Captures(live, 11500), "freshness expires at boundary");
    Check(!Captures(live, 9999), "future publication cannot mask");
    Check(!Captures(live, 10000, 0), "zero lease interval");
    InputLease l = live; l.presentedMs = 0;
    Check(!Captures(l, 10000), "no input capture before first real covered Present");
    l = live; l.workerMs = 8500;
    Check(!Captures(l, 10000), "stopped provider releases despite active renderer");
    l = live; l.presentedMs = 8500;
    Check(!Captures(l, 10000), "stopped renderer releases despite active provider");
    l = live; l.deadlineMs = 10000;
    Check(!Captures(l, 10000), "absolute deadline releases");
    l = live; l.workerMs = l.presentedMs = 16000;
    Check(!Captures(l, 16000), "heartbeats cannot extend absolute deadline");
    l = live; l.cancelled = true;
    Check(!Captures(l, 10000), "local Escape or B cancellation releases");
    l = live; l.enabled = false;
    Check(!Captures(l, 10000), "disable or teardown releases");
    for (unsigned field = 0; field < 5; ++field) {
        l = live;
        uint64_t* fields[] = {&l.token, &l.session, &l.workerMs, &l.presentedMs, &l.deadlineMs};
        *fields[field] = 0;
        Check(!Captures(l, 10000), "incomplete publication cannot capture");
    }
    l = live; l.workerMs = std::numeric_limits<uint64_t>::max();
    Check(!Captures(l, 10000), "future stamp rejects without unsigned-wrap freshness");
    std::printf("PASS: %u cover Present/lease checks (pure model; no runtime claim)\n", checks);
}
