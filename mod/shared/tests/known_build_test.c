/* known_build_test.c - offline test of mod/shared/known_build.h (#77).
 * usage: known_build_test <decrypted v2.02 dump (file offset = RVA)> [Steam Nioh3.exe]
 * Builds synthetic mapped images: the stripped layout with the dump's .text window must classify as N3KB_STRIPPED;
 * every broken variant must be N3KB_UNKNOWN; the Steam layout is N3KB_STEAM. With a Steam exe, it is also mapped as
 * an image (no code runs) and must classify as N3KB_STEAM. */
#include "../known_build.h"
#include <stdio.h>
#include <stdlib.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

static uint8_t* g_win; /* 64 KB .text window from the dump */

/* header page + .text window in one allocation; the classifier reads nothing else */
static uintptr_t Make(uint32_t stamp, uint32_t size, int nsec, int bindLast, int moveSec, int flipByte) {
    uint8_t* b = (uint8_t*)VirtualAlloc(NULL, 0x1000 + N3KB_HASH_LEN, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)b;
    IMAGE_NT_HEADERS64* nt;
    IMAGE_SECTION_HEADER* sh;
    int i;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    nt = (IMAGE_NT_HEADERS64*)(b + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.NumberOfSections = (WORD)nsec;
    nt->FileHeader.TimeDateStamp = stamp;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage = size;
    sh = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nsec && i < 7; ++i) {
        memcpy(sh[i].Name, n3kb_secs[i].name, 8);
        sh[i].VirtualAddress = n3kb_secs[i].va + (i == moveSec ? 0x1000 : 0);
        sh[i].Misc.VirtualSize = i == 0 ? 0x38E028B : 0x1000;
    }
    if (bindLast && nsec == 8) { memcpy(sh[7].Name, ".bind\0\0", 8); sh[7].VirtualAddress = N3KB_SIZE_STRIPPED; }
    memcpy(b + N3KB_HASH_RVA, g_win, N3KB_HASH_LEN);
    if (flipByte >= 0) b[N3KB_HASH_RVA + flipByte] ^= 1;
    return (uintptr_t)b;
}
static void Free(uintptr_t b) { VirtualFree((void*)b, 0, MEM_RELEASE); }

static int Classify(uint32_t stamp, uint32_t size, int nsec, int bindLast, int moveSec, int flipByte) {
    uintptr_t b = Make(stamp, size, nsec, bindLast, moveSec, flipByte);
    int k = n3kb_classify(b);
    Free(b);
    return k;
}

int main(int argc, char** argv) {
    FILE* f;
    uint8_t d[32], z[64];
    static const uint8_t zero64[32] = {0xf5, 0xa5, 0xfd, 0x42, 0xd1, 0x6a, 0x20, 0x30, 0x27, 0x98, 0xef, 0x6e, 0xd3, 0x09, 0x97, 0x9b,
                                       0x43, 0x00, 0x3d, 0x23, 0x20, 0xd9, 0xf0, 0xe8, 0xea, 0x98, 0x31, 0xa9, 0x27, 0x59, 0xfb, 0x4b};
    if (argc < 2) { printf("usage: known_build_test <dump> [steam exe]\n"); return 2; }
    memset(z, 0, sizeof z);
    n3kb_sha256(z, 64, d);
    CHECK(memcmp(d, zero64, 32) == 0); /* sha256 of 64 zero bytes */

    g_win = (uint8_t*)malloc(N3KB_HASH_LEN);
    f = fopen(argv[1], "rb");
    if (!f || fseek(f, N3KB_HASH_RVA, SEEK_SET) || fread(g_win, 1, N3KB_HASH_LEN, f) != N3KB_HASH_LEN) { printf("FAIL: cannot read dump\n"); return 1; }
    fclose(f);

    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 7, 0, -1, -1) == N3KB_STRIPPED);    /* the stripped copy */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STEAM, 8, 1, -1, 5) == N3KB_STEAM);           /* Steam: identity, bytes not read */
    CHECK(Classify(N3KB_STAMP + 1, N3KB_SIZE_STRIPPED, 7, 0, -1, -1) == N3KB_UNKNOWN); /* other stamp */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED + 0x1000, 7, 0, -1, -1) == N3KB_UNKNOWN); /* other size */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 8, 1, -1, -1) == N3KB_UNKNOWN);     /* still has .bind */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 6, 0, -1, -1) == N3KB_UNKNOWN);     /* a section missing */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 7, 0, 3, -1) == N3KB_UNKNOWN);      /* .pdata moved */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 7, 0, -1, 0) == N3KB_UNKNOWN);      /* first code byte differs */
    CHECK(Classify(N3KB_STAMP, N3KB_SIZE_STRIPPED, 7, 0, -1, N3KB_HASH_LEN - 1) == N3KB_UNKNOWN); /* last byte differs */
    {
        LARGE_INTEGER q0, q1, fq;
        uintptr_t b = Make(N3KB_STAMP, N3KB_SIZE_STRIPPED, 7, 0, -1, -1);
        QueryPerformanceFrequency(&fq);
        QueryPerformanceCounter(&q0);
        CHECK(n3kb_classify(b) == N3KB_STRIPPED);
        QueryPerformanceCounter(&q1);
        printf("stripped classify: %.3f ms\n", (double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)fq.QuadPart);
        Free(b);
    }
    if (argc > 2) {
        wchar_t w[MAX_PATH];
        HMODULE m;
        MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, w, MAX_PATH);
        m = LoadLibraryExW(w, NULL, DONT_RESOLVE_DLL_REFERENCES); /* mapped as an image; no entry point runs */
        CHECK(m != NULL);
        if (m) {
            int k = n3kb_classify((uintptr_t)m & ~(uintptr_t)3);
            printf("steam exe mapped: %s\n", n3kb_name(k));
            CHECK(k == N3KB_STEAM);
            FreeLibrary(m);
        }
    }
    printf(g_fail ? "known_build_test: %d FAILED\n" : "known_build_test: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
