// Included around verbatim production ApplyBatch by run_applybatch_failure_audit.py.
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
namespace gen { enum class Kind { Bytes, VPtr }; struct Site { const char* id; size_t len; Kind kind; }; }
struct Item { const gen::Site* s; uint8_t* addr; std::vector<uint8_t> repl; };
alignas(8) static uint8_t storage[32];
static uint8_t* g_base = storage;
static int fault, cursor, opens, closes, suspends, resumes, frozen, flushes, writes, protections[2], failures;
static DWORD error;
static uintptr_t ip;
static void require(bool b) { if (!b) { std::puts("internal harness contract failed"); std::exit(2); } }
static void Log(const char*, ...) { require(frozen == 0); }
static bool BuildRepl(Item& it) { require(frozen == 0); it.repl.assign(it.s->len, 0xCC); return fault != 10; }
static DWORD FakeError() { return error; }
static DWORD FakeSelf() { return 1; }
static DWORD FakePid() { return 42; }
static HANDLE FakeProcess() { return (HANDLE)99; }
static HANDLE FakeSnapshot(DWORD, DWORD) { require(frozen == 0); if (fault == 1) { error = 5; return INVALID_HANDLE_VALUE; } ++opens; return (HANDLE)100; }
static BOOL entry(THREADENTRY32* te) {
    if (cursor == 4) { error = ERROR_NO_MORE_FILES; return FALSE; }
    te->th32ThreadID = DWORD(cursor + 1); te->th32OwnerProcessID = cursor == 3 ? 77 : 42;
    return TRUE;
}
static BOOL FakeFirst(HANDLE, THREADENTRY32* te) { cursor = 0; if (fault == 2) { error = 5; return FALSE; } return entry(te); }
static BOOL FakeNext(HANDLE, THREADENTRY32* te) { ++cursor; if (fault == 3 && cursor == 2) { error = 5; return FALSE; } return entry(te); }
static HANDLE FakeOpen(DWORD, BOOL, DWORD tid) { require(tid == 2 || tid == 3); if ((fault == 4 && tid == 2) || (fault == 5 && tid == 3)) { error = 5; return nullptr; } ++opens; return (HANDLE)(uintptr_t)tid; }
static BOOL FakeClose(HANDLE) { require(frozen == 0); ++closes; return TRUE; }
static DWORD FakeSuspend(HANDLE h) { if ((fault == 6 && h == (HANDLE)2) || (fault == 7 && h == (HANDLE)3)) { error = 5; return DWORD(-1); } ++suspends; ++frozen; return 0; }
static DWORD FakeResume(HANDLE) { require(frozen > 0); --frozen; ++resumes; return 1; }
static BOOL FakeContext(HANDLE h, CONTEXT* c) { require(frozen > 0); if ((fault == 8 && h == (HANDLE)2) || (fault == 9 && h == (HANDLE)3)) { error = 5; return FALSE; } c->Rip = ip; return TRUE; }
static BOOL FakeProtect(void* p, SIZE_T, DWORD prot, DWORD* old) { require(frozen == 0); int index = p == storage ? 0 : 1; if (fault == 11 && index == 1 && prot == PAGE_EXECUTE_READWRITE) { error = 5; return FALSE; } *old = DWORD(protections[index]); protections[index] = int(prot); return TRUE; }
static BOOL FakeFlush(HANDLE, const void*, SIZE_T) { ++flushes; return TRUE; }
static void FakeSleep(DWORD) { require(frozen == 0); }
static void* FakeCopy(void* p, const void* q, size_t n) { ++writes; return std::memcpy(p, q, n); }
static LONG64 FakeExchange(volatile LONG64* p, LONG64 v) { ++writes; LONG64 old = *p; *p = v; return old; }
#define GetLastError FakeError
#define GetCurrentThreadId FakeSelf
#define GetCurrentProcessId FakePid
#define GetCurrentProcess FakeProcess
#define CreateToolhelp32Snapshot FakeSnapshot
#define Thread32First FakeFirst
#define Thread32Next FakeNext
#define OpenThread FakeOpen
#define CloseHandle FakeClose
#define SuspendThread FakeSuspend
#define ResumeThread FakeResume
#define GetThreadContext FakeContext
#define VirtualProtect FakeProtect
#define FlushInstructionCache FakeFlush
#define Sleep FakeSleep
#define memcpy FakeCopy
#undef InterlockedExchange64
#define InterlockedExchange64 FakeExchange
// PRODUCTION_FUNCTION
#undef memcpy
int main() {
    const gen::Site s1{"one", 8, gen::Kind::Bytes}, s2{"two", 8, gen::Kind::VPtr};
    const char* labels[] = {"success", "snapshot", "first", "partial-next", "open-first", "open-second", "suspend-first", "suspend-second", "context-first", "context-second", "encoding", "protect-second", "persistent-clash", "at-start", "at-end", "outside"};
    for (int c = 0; c < 16; ++c) {
        fault = c; opens = closes = suspends = resumes = frozen = flushes = writes = 0; error = 0;
        protections[0] = PAGE_EXECUTE_READ; protections[1] = PAGE_READONLY;
        std::memset(storage, 0xAA, sizeof(storage));
        ip = c == 12 ? uintptr_t(storage + 1) : c == 13 ? uintptr_t(storage) : c == 14 ? uintptr_t(storage + 8) : uintptr_t(storage + 31);
        Item a{&s1, storage, {}}, b{&s2, storage + 16, {}}; std::vector<Item*> batch{&a, &b};
        bool result = ApplyBatch("test", batch), expected = c == 0 || c >= 13;
        bool bytes = true;
        for (int k = 0; k < 32; ++k) if (storage[k] != ((expected && (k < 8 || (k >= 16 && k < 24))) ? 0xCC : 0xAA)) bytes = false;
        bool ok = result == expected && bytes && writes == (expected ? 2 : 0) && flushes == (expected ? 1 : 0) && opens == closes && suspends == resumes && frozen == 0 && protections[0] == PAGE_EXECUTE_READ && protections[1] == PAGE_READONLY;
        if (!ok) ++failures;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", labels[c]);
        if (!ok && c >= 1 && c <= 9) {
            bool cleanup = opens == closes && suspends == resumes && frozen == 0 && protections[0] == PAGE_EXECUTE_READ && protections[1] == PAGE_READONLY;
            std::printf("WITNESS %s result=%d writes=%d flushes=%d unchanged=%d cleanup=%d\n", labels[c], int(result), writes, flushes, int(bytes), int(cleanup));
        }
    }
    std::vector<Item*> empty; require(ApplyBatch("empty", empty));
    std::printf("17 cases, %d failures\n", failures);
    return failures ? 1 : 0;
}
