"""SelfTest phase 2 uses [Rematch] SelfTestHoldMs. No game."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
rematch = (ROOT / "src/rematch.cpp").read_text(encoding="utf-8")
ini = (ROOT / "Nioh3BossRematch.ini").read_text(encoding="utf-8")


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def phase2(tick):
    start = tick.index("if (st.phase == 2)")
    opening = tick.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (tick[end] == "{") - (tick[end] == "}")
        end += 1
    return tick[start:end]


tick = function(rematch, "void SelfTestTick(const Snap& s, ULONGLONG t)")
hold = phase2(tick)
assert "int holdMs = 4000;" in rematch
assert 'L"SelfTestHoldMs", 4000' in rematch
assert "if (hold < 0) hold = 4000;" in rematch
assert "const ULONGLONG hold = (ULONGLONG)st.holdMs;" in hold
assert "if (t - st.since < hold) return;" in hold
assert "st.since = t - hold;" in hold
assert "< 4000" not in hold and "t - 4000" not in hold
assert "SelfTestHoldMs = 4000" in ini
assert "40000 stays 40 seconds" in ini


def still_holding(now, since, hold_ms):
    return now - since < hold_ms


def retry_since(now, hold_ms):
    return now - hold_ms


# Missing key uses 4000: 3 s stays, 4 s leaves.
assert still_holding(3000, 0, 4000)
assert not still_holding(4000, 0, 4000)
# 40000 stays through the old 4 s window and leaves at 40 s.
assert still_holding(4000, 0, 40000)
assert still_holding(39999, 0, 40000)
assert not still_holding(40000, 0, 40000)
# A 120 s retry must subtract the same hold, or a 40 s stay waits another 36 s.
now = 130000
assert not still_holding(now, retry_since(now, 40000), 40000)
assert still_holding(now, retry_since(now, 4000), 40000)
print("selftest hold ok")
