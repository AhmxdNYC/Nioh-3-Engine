#pragma once
#include <cstdint>

// Pure admission rules. These values are copied snapshots; callers must provide
// coherent identities and atomic publication/invalidations across threads.
// An accepted DXGI Present is not evidence of physical scanout.
namespace cover_present {
struct Identity {
    uint64_t token = 0, session = 0, swapchain = 0, contextGeneration = 0;
};
struct Ticket {
    Identity id{};
    bool submitted = false, outermost = false, testPresent = false;
};
inline bool Valid(const Identity& id) {
    return id.token && id.session && id.swapchain && id.contextGeneration;
}
inline bool Same(const Identity& a, const Identity& b) {
    return a.token == b.token && a.session == b.session &&
        a.swapchain == b.swapchain && a.contextGeneration == b.contextGeneration;
}
inline bool Accept(const Ticket& ticket, const Identity& stillCurrent,
                   int32_t finalHr, bool cancelled) {
    // S_OK only: SUCCEEDED also accepts DXGI_STATUS_OCCLUDED. Call only after
    // the final original Present/Present1, including any INVALID_CALL fallback.
    return !cancelled && ticket.submitted && ticket.outermost && !ticket.testPresent &&
        finalHr == 0 && Valid(ticket.id) && Valid(stillCurrent) && Same(ticket.id, stillCurrent);
}
struct InputLease {
    uint64_t token = 0, session = 0, workerMs = 0, presentedMs = 0, deadlineMs = 0;
    bool enabled = false, cancelled = false;
};
inline bool Fresh(uint64_t stamp, uint64_t now, uint64_t freshnessMs) {
    return stamp && freshnessMs && now >= stamp && now - stamp < freshnessMs;
}
inline bool Captures(const InputLease& lease, uint64_t now, uint64_t freshnessMs = 1500) {
    // Re-evaluate from every input path, even if renderer or provider has died.
    // No synthetic heartbeat and no capture before first accepted cover frame.
    return lease.enabled && !lease.cancelled && lease.token && lease.session &&
        lease.deadlineMs && now < lease.deadlineMs &&
        Fresh(lease.workerMs, now, freshnessMs) && Fresh(lease.presentedMs, now, freshnessMs);
}
} // namespace cover_present
