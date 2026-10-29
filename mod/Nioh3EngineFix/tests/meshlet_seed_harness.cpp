// Own-process execution of the two exact v2.02 completion seed/CAS regions.
// No game access. Run with the decrypted image path. Ordering rationale is in notes.
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <stdexcept>
using Fn = uint32_t(*)(volatile LONG*, uint32_t*);
struct Code {
    void* mem;
    Fn fn;
    Code(const std::vector<unsigned char>& b) {
        mem = VirtualAlloc(nullptr, b.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!mem) throw std::runtime_error("VirtualAlloc");
        memcpy(mem, b.data(), b.size());
        DWORD old;
        if (!VirtualProtect(mem, b.size(), PAGE_EXECUTE_READ, &old)) throw std::runtime_error("VirtualProtect");
        FlushInstructionCache(GetCurrentProcess(), mem, b.size());
        fn = reinterpret_cast<Fn>(mem);
    }
    ~Code() { VirtualFree(mem, 0, MEM_RELEASE); }
};
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<unsigned char> image((std::istreambuf_iterator<char>(f)), {});
    if (image.size() < 0x49431) return 3;
    unsigned checks = 0;
    for (int path = 0; path < 2; ++path) {
        const size_t begin = path ? 0x4940A : 0x493DA;
        const size_t end = path ? 0x49431 : 0x493F2;
        const size_t seed = path ? 0x49412 : 0x493DC;
        std::vector<unsigned char> prefix = path
            ? std::vector<unsigned char>{0x41,0x55,0x49,0x89,0xC9,0x48,0x89,0xD0} // save r13; r9=rcx; rax=rdx
            : std::vector<unsigned char>{0x41,0x55,0x45,0x33,0xED,0x49,0x89,0xC8}; // save/zero r13; r8=rcx
        auto original = prefix;
        original.insert(original.end(), image.begin()+begin, image.begin()+end);
        original.insert(original.end(), {0x41,0x5D,0xC3}); // restore r13; ret
        auto patched = original;
        const size_t off = prefix.size() + seed - begin;
        const unsigned char expected[] = {0xF0,0x45,0x0F,0xB1,static_cast<unsigned char>(path ? 0x29 : 0x28)};
        if (memcmp(patched.data()+off, expected, 5)) return 4;
        const unsigned char replacement[] = {0x41,0x8B,static_cast<unsigned char>(path ? 0x01 : 0x00),0x66,0x90};
        memcpy(patched.data()+off, replacement, 5);
        Code a(original), b(patched);
        uint32_t x=0x12345678;
        for (unsigned i=0; i<20000; ++i) {
            x ^= x<<13; x ^= x>>17; x ^= x<<5;
            const uint32_t input = i<4 ? (i==0 ? 0u : i==1 ? 1u : i==2 ? 0x200u : ~0u) : x;
            volatile LONG wa=static_cast<LONG>(input), wb=wa;
            uint32_t oa=123, ob=123;
            auto ra=a.fn(&wa,&oa), rb=b.fn(&wb,&ob);
            if (wa!=wb || uint32_t(wb)!=(input|0x200u) || ra!=rb || oa!=ob || (path && ob!=0)) return 5;
            ++checks;
        }
        // Actual copied CAS instructions race with other bit setters; no lost bits.
        volatile LONG shared=0;
        std::atomic<bool> go{false};
        std::thread setter([&] { while (!go.load()) {} for (unsigned i=0;i<100000;++i) InterlockedOr(&shared,LONG(1u<<(i%31))); });
        go.store(true);
        for (unsigned i=0;i<100000;++i) { uint32_t out=123; b.fn(&shared,&out); if (path && out!=0) return 6; }
        setter.join();
        if (uint32_t(shared)!=0x7FFFFFFFu) return 7;
        ++checks;
    }
    std::cout << "PASS " << checks << " equivalence/contention cases; exact original and patched machine code; no game access\n";
}
