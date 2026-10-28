#pragma once
// In-process x64 ABI. No game operations or UI-model calls occur in callbacks.
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace rematch_menu_bridge {
constexpr uint32_t kVersion = 2;
constexpr uint32_t kMaxRows = 24;
constexpr const char* kExport = "Nioh3RematchMenuGetApi";
enum Result : int { Accepted = 0, Busy = 1, Unavailable = 2, BadVersion = 3, StaleSession = 4, StaleGeneration = 5, InvalidArgument = 6 };
enum Flags : uint32_t { HostedReady = 1u, Open = 2u, CloseRequested = 4u, CoverActive = 8u, HostMenu = 16u, Editing = 32u }; // Editing: modal text/slider editor owns input; host must not close with Y  // HostMenu (#38077): page closed by its 'Mod menu' row = show the host's first page, do not close the host menu
enum Capability : uint32_t { SnapshotV1 = 1u, InputV1 = 2u, ExclusiveHost = 4u };
constexpr uint32_t kRequiredCapabilities = SnapshotV1 | InputV1 | ExclusiveHost;
// user #13223: OpenRematch / OpenPractice open the add-on directly on its Boss Rematch / Practice page (the host's first page lists them);
// an older provider answers InvalidArgument and the host falls back to OpenMenu.
enum Action : uint32_t { OpenMenu = 0, CloseMenu, Up, Down, Left, Right, PageUp, PageDown, Accept, Back, OpenRematch, OpenPractice };
struct Row { char text[112]; uint32_t color; uint32_t selected; }; // color is Windows COLORREF (0x00BBGGRR), host adds alpha
struct Snapshot {
    uint32_t size;
    uint32_t version;
    uint64_t generation; // actionable model generation, NOT the draw count
    uint64_t session;
    uint64_t workerTickMs; // last worker publication, GetTickCount64 domain; query does not refresh it
    uint32_t flags;
    uint32_t rowCount;
    Row rows[kMaxRows];
    char footer[240];
    char toast[160];
    uint32_t toastColor;
    uint32_t coverPhase;
    uint64_t coverToken, coverDeadline;
    char coverReason[160];
};
struct Api {
    uint32_t size;
    uint32_t version;
    uint32_t capabilities;
    uint32_t reserved;
    // Accepted attach reserves a session; wait for HostedReady before use/masking.
    // Repeat same hostToken returns same session; another owner receives Busy.
    int (*Attach)(uint64_t hostToken, uint64_t* session);
    // Detach/Close must not be starved behind normal actions. Invalidates pending actions.
    int (*Detach)(uint64_t session);
    // Accepted means queued only. Worker rechecks session/generation before dispatch.
    // CloseMenu ignores generation; other actions require the displayed generation.
    int (*Post)(uint64_t session, uint64_t expectedGeneration, uint32_t action);
    // Caller initializes size/version. Never partial-copy; Busy preserves caller buffer.
    int (*CopySnapshot)(uint64_t session, Snapshot* out, uint32_t bytes);
    int (*PresentCover)(uint64_t session, uint64_t token);
};
using GetApi = int (*)(uint32_t version, uint32_t bytes, Api* out);
// All function pointers use platform x64 C calling convention. Query exported with
// extern "C" __declspec(dllexport). Modules remain loaded for the process lifetime.
// Every callback is try-only/nonblocking; worker owns every Ui* call. No pipe I/O.
inline bool ValidAction(uint32_t action) { return action <= OpenPractice; }
inline bool ValidApi(const Api& a) {
    return a.size == sizeof(Api) && a.version == kVersion && a.reserved == 0 &&
        (a.capabilities & kRequiredCapabilities) == kRequiredCapabilities && a.Attach && a.Detach && a.Post && a.CopySnapshot && a.PresentCover;
}
template<size_t N> inline bool Terminated(const char (&text)[N]) {
    for (size_t i = 0; i < N; ++i) if (!text[i]) return true;
    return false;
}
inline bool ValidSnapshot(const Snapshot& s, uint64_t session) {
    if (!session || s.size != sizeof(Snapshot) || s.version != kVersion || s.session != session ||
        !s.generation || s.rowCount > kMaxRows || (s.flags & ~(HostedReady | Open | CloseRequested | CoverActive | HostMenu)) ||
        !Terminated(s.footer) || !Terminated(s.toast) || !Terminated(s.coverReason) || s.coverPhase>6) return false;
    if ((s.flags & HostedReady) && !s.workerTickMs) return false;
    if ((s.flags & Open) && !(s.flags & HostedReady)) return false;
    if (s.coverPhase >= 1 && s.coverPhase <= 3 && !(s.flags & CoverActive)) return false;
    if ((s.flags & CoverActive) && (!(s.flags & HostedReady) || !s.coverToken || !s.coverDeadline || s.coverPhase<1 || s.coverPhase>3)) return false;
    for (uint32_t i = 0; i < s.rowCount; ++i)
        if (!Terminated(s.rows[i].text) || s.rows[i].selected > 1) return false;
    return true;
}
// Caller serializes every access (e.g. TryAcquireSRWLockExclusive); this helper is
// NOT a lock-free/concurrent queue. Close/detach use a separate priority mailbox.
struct QueuedAction { uint64_t session, generation; uint32_t action; };
template<size_t Capacity = 32> struct Queue {
    static_assert(Capacity > 0, "nonempty queue");
    QueuedAction entries[Capacity]{};
    size_t head = 0, count = 0;
    bool Push(const QueuedAction& a) {
        if (!a.session || !ValidAction(a.action) || a.action == CloseMenu || count == Capacity) return false;
        entries[(head + count) % Capacity] = a; ++count; return true;
    }
    bool Pop(QueuedAction& a) {
        if (!count) return false;
        a = entries[head]; head = (head + 1) % Capacity; --count; return true;
    }
    void Clear() { head = count = 0; }
};
static_assert(std::is_standard_layout<Row>::value && std::is_standard_layout<Snapshot>::value && std::is_standard_layout<Api>::value, "POD ABI");
static_assert(sizeof(Row) == 120 && offsetof(Snapshot, rows) == 40, "fixed row layout");
static_assert(sizeof(Snapshot) == 3504, "x64 snapshot size");
#if INTPTR_MAX == INT64_MAX
static_assert(sizeof(Api) == 56, "x64 API size");
#endif
} // namespace rematch_menu_bridge
