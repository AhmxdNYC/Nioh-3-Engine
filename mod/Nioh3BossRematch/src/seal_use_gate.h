#pragma once
#include <cstdint>
namespace sealuse {
// Player seal-use animations, not yokai/core IDs. Never accept a whole 0x28xx category.
inline bool ThrowAction(uint32_t action) {
    return action == 0x28FD || action == 0x28B6 || action == 0x28B2 || action == 0x28FB;
}
enum class Decision { Wait, Confirm, Cancel };
inline Decision Check(double now, double requested, uint32_t original, uint32_t current, int timeoutMs) {
    if (requested < 0 || now < requested || (now - requested) * 1000.0 >= timeoutMs) return Decision::Cancel;
    if (ThrowAction(current)) return Decision::Confirm;
    if (current != original && current != 0xFFFF && current != 0xFFFFFFFF) return Decision::Cancel;
    return Decision::Wait;
}
}
