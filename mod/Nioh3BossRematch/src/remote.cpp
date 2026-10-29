// remote.cpp - \\.\pipe\nioh3bossrematch ([Remote] Pipe = 1, default 0): one command per line, one reply line each.
// Used by tools\rematch\verify_all.ps1 (local only: PIPE_REJECT_REMOTE_CLIENTS). Commands: ping, status, scan, row <id>,
// start <id>, abort, open, scroll (rematch.cpp Command()).
#include "addon.h"

#include <cstdio>
#include <cstring>

namespace addon {
namespace remote {
namespace {

DWORD WINAPI Client(void* p) {
    HANDLE h = (HANDLE)p;
    char buf[512];
    size_t have = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, buf + have, (DWORD)(sizeof(buf) - 1 - have), &got, nullptr) || !got) break;
        have += got;
        buf[have] = 0;
        char* nl;
        while ((nl = strchr(buf, '\n')) != nullptr) {
            *nl = 0;
            if (nl > buf && nl[-1] == '\r') nl[-1] = 0;
            char reply[1024];
            if(!strcmp(buf,"hostfight") || !strncmp(buf,"hostfight ",10))
                ui::HostedFightCommand(buf[9]?buf+10:"",reply,sizeof(reply)-2);
            else rematch::Command(buf, reply, sizeof(reply) - 2);
            strcat_s(reply, "\n");
            DWORD wr = 0;
            WriteFile(h, reply, (DWORD)strlen(reply), &wr, nullptr);
            const size_t used = (size_t)(nl - buf) + 1;
            memmove(buf, nl + 1, have - used + 1);
            have -= used;
        }
        if (have >= sizeof(buf) - 1) have = 0;
    }
    DisconnectNamedPipe(h);
    CloseHandle(h);
    return 0;
}

DWORD WINAPI Server(void*) {
    for (;;) {
        HANDLE h = CreateNamedPipeW(L"\\\\.\\pipe\\nioh3bossrematch", PIPE_ACCESS_DUPLEX,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 4, 4096, 4096, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(h);
            continue;
        }
        if (HANDLE t = CreateThread(nullptr, 0, Client, h, 0, nullptr)) CloseHandle(t);
        else CloseHandle(h);
    }
}

}  // namespace

bool Start() {
    if (!IniInt(L"Remote", L"Pipe", 0)) return false;
    if (HANDLE t = CreateThread(nullptr, 0, Server, nullptr, 0, nullptr)) CloseHandle(t);
    Log("Remote: pipe \\\\.\\pipe\\nioh3bossrematch on (local only)");
    return true;
}

}  // namespace remote
}  // namespace addon
