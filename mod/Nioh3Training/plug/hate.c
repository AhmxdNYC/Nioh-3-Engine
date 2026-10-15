// trplug probe (bridge 4616): force aggro through the native SetInitAccumulatedHate body (1FF23C8(aiBoss, aiPlayer, amount)).
// Commands: hate <bossActorPtr> [amount]   (actor pointer from the 'actors' command)   hdump <bossActorPtr> (log hate list size)
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef void (*LogFn)(const char*, ...);
static int Rd64(uintptr_t a, uint64_t* out) { __try { *out = *(volatile uint64_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static int ListN(uint64_t ai) { uint64_t p50 = 0, p10 = 0, b = 0, e = 0; if (!Rd64(ai + 0x50, &p50) || !p50 || !Rd64(p50 + 0x10, &p10) || !p10 || !Rd64(p10 + 0x518, &b) || !Rd64(p10 + 0x520, &e)) return -1; return (int)((e - b) / 16); }
__declspec(dllexport) void Cmd(uintptr_t B, const char* line, LogFn Log) {
    char op[16] = {0}; unsigned long long a = 0; float amt = 1000.0f;
    int n = sscanf(line, "%15s %llx %f", op, &a, &amt);
    if (n < 2) return;
    uint64_t pt = 0, pl = 0; Rd64(B + 74732360, &pt); Rd64(pt + 0xCD * 0x18, &pl);
    uint64_t aiB = 0, aiP = 0; Rd64(a + 0x3b0, &aiB); Rd64(pl + 0x3b0, &aiP);
    Log("PLUG %s boss %llx player %llx aiBoss %llx aiPlayer %llx hateList %d", op, a, (unsigned long long)pl, (unsigned long long)aiB, (unsigned long long)aiP, aiB ? ListN(aiB) : -9);
    if (!strcmp(op, "hate") && aiB && aiP) {
        __try { ((void (*)(uint64_t, uint64_t, float))(B + 0x1FF23C8))(aiB, aiP, amt); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PLUG hate: exception"); return; }
        Log("PLUG hate applied amount %.0f, hateList now %d", amt, ListN(aiB));
    }
}
