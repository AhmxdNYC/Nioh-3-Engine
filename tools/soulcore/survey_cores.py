# One launch, regular player: times EVERY owned soul core in one pass (human #134924: universal timings, not one by one).
# Equip each owned core with the shrine path, use it twice, keep the second use (first use learns hitboxes, #134293).
# Discord one line per core (#134331). Writes OUT/cores.jsonl (resumable) and OUT/progress.txt; table.py makes the table.
# Needs a Boss Rematch ASI with the "spirit scequip/scstate/gauge" pipe (pc2/sc-survey-134412 e93268d3 or later).
# Usage: py -I survey_cores.py OUT_DIR   (normally started by run_survey.ps1 once the game is in the field)
import json, os, re, subprocess, sys, time

GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
LOG = os.path.join(GAME, "Nioh3BossRematch.log")
VPAD = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix\vpad.py"
NOTIFY = r"C:\Users\Ahmad Hamza\universal-optimizer\tools\notify\notify.py"
PIPE = r"\\.\pipe\nioh3bossrematch"
if len(sys.argv) < 2:
    raise SystemExit("usage: survey_cores.py OUT_DIR")
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
JSONL = os.path.join(OUT, "cores.jsonl")
PROG = os.path.join(OUT, "progress.txt")

def say(s):
    print(s, flush=True)

def pipe(cmd, timeout=8):
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateFileW.restype = ctypes.c_void_p
    k.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    k.WriteFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    k.ReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    k.CloseHandle.argtypes = [ctypes.c_void_p]
    k.WaitNamedPipeW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
    k.WaitNamedPipeW(PIPE, 5000)
    h = k.CreateFileW(PIPE, 0xC0000000, 0, None, 3, 0, None)
    bad = ctypes.c_void_p(-1).value
    if not h or h == bad:
        raise OSError("no rematch pipe err %s" % ctypes.get_last_error())
    try:
        raw = (cmd.strip() + "\n").encode("ascii")
        src = ctypes.create_string_buffer(raw)
        wr = wintypes.DWORD()
        if not k.WriteFile(h, src, len(raw), ctypes.byref(wr), None):
            raise OSError("write %s" % ctypes.get_last_error())
        acc = b""
        tmp = ctypes.create_string_buffer(2048)
        n = wintypes.DWORD()
        t0 = time.time()
        while b"\n" not in acc:
            if time.time() - t0 > timeout:
                raise TimeoutError(cmd)
            if not k.ReadFile(h, tmp, 2048, ctypes.byref(n), None):
                raise OSError("read %s" % ctypes.get_last_error())
            if not n.value:
                break
            acc += tmp.raw[:n.value]
        return acc.decode("utf-8", "replace").strip()
    finally:
        k.CloseHandle(h)

def log_size():
    try:
        return os.path.getsize(LOG)
    except OSError:
        return 0

def read_from(pos):
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateFileW.restype = ctypes.c_void_p
    k.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    k.SetFilePointerEx.argtypes = [ctypes.c_void_p, ctypes.c_longlong, ctypes.c_void_p, wintypes.DWORD]
    k.ReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    k.CloseHandle.argtypes = [ctypes.c_void_p]
    h = k.CreateFileW(LOG, 0x80000000, 7, None, 3, 0, None)
    if not h or h == ctypes.c_void_p(-1).value:
        return "", pos
    try:
        k.SetFilePointerEx(h, pos, None, 0)
        acc = b""
        tmp = ctypes.create_string_buffer(65536)
        n = wintypes.DWORD()
        while k.ReadFile(h, tmp, 65536, ctypes.byref(n), None) and n.value:
            acc += tmp.raw[:n.value]
            if len(acc) > 2000000:
                break
        return acc.decode("utf-8", "replace"), pos + len(acc)
    finally:
        k.CloseHandle(h)

def press(btn, ms=450):
    r = subprocess.run([sys.executable, "-I", VPAD, "press", btn, str(ms)], capture_output=True, text=True)
    say("press %s -> %s" % (btn, (r.stdout or r.stderr).strip()[:120]))
    return r.returncode == 0

# vpad DOWN lights game button id 0x8. The item scan stops on that button.
# Shrine equip writes the soul-core use id into shortcut word 1 (button id 0xD), which DOWN does not press.
# Word 3 stays 0, so the press never reaches HasSkill. Copy word 1 onto word 3 before each press.
_ARM = {}

def use_from(text):
    hits = re.findall(r"use [0-9A-Fa-f]+/([0-9A-Fa-f]+)", text or "")
    return int(hits[-1], 16) if hits and int(hits[-1], 16) else 0

def latest_use():
    try:
        data = open(LOG, "rb").read()[-250000:].decode("utf-8", "replace")
    except OSError:
        return 0
    return use_from(data)

def arm_down(use=0):
    import ctypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = ctypes.c_void_p
    k.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    k.WriteProcessMemory.argtypes = k.ReadProcessMemory.argtypes
    ps = ctypes.WinDLL("psapi")
    ps.EnumProcessModules.restype = ctypes.c_int
    ps.EnumProcessModules.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong)]
    if not _ARM.get("pid"):
        out = subprocess.check_output(["powershell", "-NoProfile", "(Get-Process Nioh3 -ErrorAction Stop).Id"], text=True)
        _ARM["pid"] = int(out.strip().split()[0])
    h = _ARM.get("h")
    if not h:
        h = k.OpenProcess(0x0438, False, _ARM["pid"])  # QUERY | VM_READ | VM_WRITE | VM_OP
        if not h:
            say("arm open failed %s" % ctypes.get_last_error())
            return 0
        _ARM["h"] = h
        _ARM["k"] = k
    k = _ARM["k"]

    def ru(addr, n):
        buf = (ctypes.c_ubyte * n)()
        got = ctypes.c_size_t()
        if not k.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)) or got.value != n:
            raise OSError(hex(addr))
        return bytes(buf)

    def u64(a):
        return int.from_bytes(ru(a, 8), "little")

    def u32(a):
        return int.from_bytes(ru(a, 4), "little")

    def u16(a):
        return int.from_bytes(ru(a, 2), "little")

    try:
        if "node" not in _ARM:
            arr = (ctypes.c_uint64 * 4)()
            need = ctypes.c_ulong()
            if not ps.EnumProcessModules(h, ctypes.byref(arr), ctypes.sizeof(arr), ctypes.byref(need)) or not arr[0]:
                say("arm no module")
                return 0
            base = arr[0]
            if ru(base, 2) != b"MZ":
                say("arm base not MZ")
                return 0
            inv = u64(u64(base + 0x4751530))
            mask = u64(inv + 0x23B810)
            buckets = u64(inv + 0x23B7F8)
            found = []
            for b in range(int(mask) + 1):
                found.append(u64(buckets + b * 16 + 8))
            sent = max(set(found), key=found.count)
            node = 0
            for p in found:
                if p and p != sent and u32(p + 0x10) == 0:
                    node = p
                    break
            if not node:
                say("arm no shortcut node")
                return 0
            _ARM["node"] = node
            say("arm node %X" % node)
        node = _ARM["node"]
        if not use:
            use = u16(node + 0x16)
        if not use:
            say("arm no use id")
            return 0
        if u16(node + 0x1A) != use:
            raw = (ctypes.c_ubyte * 2)(use & 0xFF, (use >> 8) & 0xFF)
            got = ctypes.c_size_t()
            if not k.WriteProcessMemory(h, ctypes.c_void_p(node + 0x1A), raw, 2, ctypes.byref(got)) or got.value != 2:
                say("arm write failed")
                return 0
        say("arm use %X" % use)
        return use
    except OSError as e:
        say("arm read failed %s" % e)
        _ARM.pop("node", None)
        return 0

def alert(title, text):
    r = subprocess.run([sys.executable, "-I", NOTIFY, "result", "--project", "nioh3", "--device", "pc",
                        "--verdict", "neutral", "--title", title, "--text", text], capture_output=True, text=True)
    say("discord %s %s" % (r.returncode, (r.stdout or r.stderr).strip()[:160]))

def owned():
    rows, off, n = [], 0, None
    while True:
        line = pipe("spirit scstate %d" % off)
        say("scstate " + line[:220])
        if not line.startswith("ok"):
            raise SystemExit("scstate failed: " + line)
        m = re.search(r"n=(\d+)", line)
        n = int(m.group(1)) if m else 0
        pairs = re.findall(r"(\d+):([0-9A-Fa-f]+)", line.split("slots=", 1)[-1] if "slots=" in line else line)
        # the slots= field is hex indexes, not idx:item. Pairs are only the ' idx:ITEM' tail.
        tail = line.split(" slots=", 1)[-1]
        # drop the slot prefix "HEX,HEX" then read pairs
        pairs = re.findall(r" (\d+):([0-9A-Fa-f]+)", line)
        for idx, item in pairs:
            rows.append((int(idx), item.upper()))
        off += 8
        if off >= n or not pairs:
            break
    # unique by inventory index
    seen, out = set(), []
    for idx, item in rows:
        if idx in seen:
            continue
        seen.add(idx)
        out.append((idx, item))
    return n, out

TS = re.compile(r"\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]")

def stamp(line):
    m = TS.search(line)
    if not m:
        return None
    h, mi, s, ms = (int(x) for x in m.groups())
    return h * 3600 + mi * 60 + s + ms / 1000.0

def measure(text):
    opens = [(int(a), float(b), int(c)) for a, c, b in re.findall(
        r"burst (\d+) opens \((\d+) NEW objects.*?([0-9.]+) s after the press", text)]
    closes = [(int(a), float(b)) for a, b in re.findall(
        r"burst (\d+) closes ([0-9.]+) s after the press", text)]
    ends = re.findall(r"FormHide=4 end (\S+)", text)
    cores = re.findall(r"press dir=L core=([0-9A-F]+)", text) or re.findall(r"SealEndOnHit core ([0-9A-F]+)", text)
    press_t, end_t = None, None
    for line in text.splitlines():
        if press_t is None and "after the press" in line:
            sm = re.search(r"([0-9.]+) s after the press", line)
            ts = stamp(line)
            if sm and ts is not None:
                press_t = ts - float(sm.group(1))
        if "FormHide=4 end " in line:
            end_t = stamp(line)
    dur = (end_t - press_t) if press_t is not None and end_t is not None else None
    if dur is not None and dur < 0:
        dur += 24 * 3600
    last_open = max((b for _, b, _ in opens), default=None)
    last_close = max((b for _, b in closes), default=None)
    first_open = min((b for _, b, _ in opens), default=None)
    post = "post-exit burst" in text
    early = bool(post or (dur is not None and last_open is not None and dur + 0.05 < last_open))
    if "FOLLOW (player rides the seal)" in text:
        move = "follow"
    elif "-> pin" in text or "PIN" in text and "FormFollowSeal" in text:
        move = "pin"
    elif "seal moved" in text:
        move = "moved"
    else:
        move = "no move line"
    return {
        "yokai": cores[-1] if cores else "",
        "end_s": round(dur, 3) if dur is not None else None,
        "end_why": ends[-1] if ends else "",
        "first_hit_s": first_open,
        "last_hit_s": last_open,
        "last_close_s": last_close,
        "after_s": round(dur - last_close, 3) if dur is not None and last_close is not None else None,
        "bursts": max((a for a, _, _ in opens), default=0),
        "new_objects": [c for _, _, c in opens],
        "early": early,
        "post_exit": post,
        "move": move,
    }

def capture(btn, timeout=18):
    pos = log_size()
    if not press(btn):
        return "", False
    t0 = time.time()
    started = False
    while time.time() - t0 < timeout:
        text, _ = read_from(pos)
        if any(s in text for s in ("press dir=L", "burst 1 opens", "FormHide=4 press", "SealMotion core=")):
            started = True
        if "FormHide=4 end " in text and started:
            return text, True
        if not started and time.time() - t0 > 8:
            return text, False
        time.sleep(0.25)
    text, _ = read_from(pos)
    return text, started

def thrown_core(text):
    cores = re.findall(r"SealMotion core=([0-9A-Fa-f]+)", text)
    if not cores:
        cores = re.findall(r"SealEndOnHit core ([0-9A-Fa-f]+)", text)
    return cores[-1].upper() if cores else ""

def y2_after(pos, timeout=3.0):
    t0 = time.time()
    last = ""
    while time.time() - t0 < timeout:
        text, _ = read_from(pos)
        hits = re.findall(r"FormCore equipped: Y1=[0-9A-Fa-f]+ Y2=([0-9A-Fa-f]+)", text)
        if hits:
            last = hits[-1].upper()
            if time.time() - t0 > 0.25:
                return last
        time.sleep(0.1)
    return last

def down_throws(expect, want=2, use=0):
    # vpad DOWN hits shortcut word 3. Write the equipped use id there before each press.
    got = []
    misses = 0
    for _ in range(6):
        if len(got) >= want:
            break
        if not arm_down(use):
            return got, "no shortcut"
        try:
            pipe("spirit gauge max", timeout=6)
        except Exception:
            pass
        pos = log_size()
        if not press("DOWN", 500):
            return got, "press failed"
        t0 = time.time()
        text = ""
        started = False
        while time.time() - t0 < 22:
            text, _ = read_from(pos)
            if "press dir=D" in text or "burst 1 opens" in text or "SealMotion core=" in text or "FormHide=4 press" in text:
                started = True
            if "FormHide=4 end " in text and thrown_core(text):
                break
            if started and "burst 1 opens" not in text and "SealEndOnHit" not in text and time.time() - t0 > 8:
                break
            if not started and time.time() - t0 > 6:
                break
            time.sleep(0.2)
        else:
            text, _ = read_from(pos)
        core = thrown_core(text)
        if not core:
            misses += 1
            say("DOWN no throw miss %d expect %s" % (misses, expect))
            if misses >= 3:
                return got, "no throw"
            time.sleep(0.8)
            continue
        misses = 0
        if expect and core != expect:
            say("DOWN threw %s wanted Y2 %s" % (core, expect))
            return got, "wrong %s" % core
        got.append(text)
        time.sleep(1.0)
    if len(got) >= want:
        return got, "ok"
    return got, "short"

def main():
    n, rows = owned()
    say("owned %d listed %d" % (n, len(rows)))
    kept_rows = []
    if os.path.isfile(JSONL):
        for line in open(JSONL, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            try:
                kept_rows.append(json.loads(line))
            except Exception:
                continue
    have = {int(r["idx"]) for r in kept_rows if "idx" in r and r.get("use2")}
    done = have
    open(PROG, "w", encoding="utf-8").write("owned %d listed %d already %d resume\n" % (n, len(rows), len(done)))
    if not rows:
        alert("Soul cores 0", "No owned soul cores in this save. Survey stopped.")
        return 2
    last_y1 = ""
    same_y1 = 0
    prev_y1 = ""
    # 4:37 proved idx 13 throws 30744. A boot that cannot throw that core must not walk the list.
    if os.environ.get("SURVEY_SKIP_PROBE") == "1":
        say("probe skipped")
    else:
        say("probe idx 13")
        pos = log_size()
        eq = pipe("spirit scequip 1 13", timeout=12)
        say("probe equip %s" % eq)
        try:
            pipe("spirit gauge max", timeout=6)
        except Exception as ge:
            say("probe gauge skip %s" % ge)
        y = y2_after(pos)
        uses, why = down_throws(y, 1, use_from(read_from(pos)[0]) or latest_use())
        if not uses:
            alert("Soul core survey stopped", "Fresh boot did not throw the known core idx 13 (yokai %s). Stopped." % (y or "?"))
            open(PROG, "w", encoding="utf-8").write("STOP probe no throw idx 13\n")
            say("STOP probe no throw")
            return 4
        say("probe ok %s" % (y or why))
    streak = 0
    for i, (idx, item) in enumerate(rows, 1):
        if idx in done:
            say("skip idx %d already kept" % idx)
            continue
        pos = log_size()
        eq = pipe("spirit scequip 1 %d" % idx, timeout=12)
        say("equip %d %s -> %s" % (idx, item, eq))
        same = "result=-4" in eq
        ok_eq = eq.startswith("ok") and ("result=8" in eq or same)
        if not ok_eq:
            rec = {"i": i, "n": len(rows), "idx": idx, "item": item, "equip": eq, "error": "equip failed"}
            open(JSONL, "a", encoding="utf-8").write(json.dumps(rec) + "\n")
            alert("Soul core %d/%d %s" % (i, len(rows), item), "Equip failed. %s" % eq[:180])
            open(PROG, "w", encoding="utf-8").write("fail equip %d/%d %s\n" % (i, len(rows), item))
            continue
        try:
            say("gauge " + pipe("spirit gauge max", timeout=6))
        except Exception as ge:
            say("gauge skip %s" % ge)
        y1 = y2_after(pos) if not same else last_y1
        if y1:
            last_y1 = y1
        if not last_y1:
            say("no Y2 line yet for idx %d, pressing DOWN anyway" % idx)
            last_y1 = ""
        if "result=8" in eq:
            st = pipe("spirit scstate 0")
            slot_now = ""
            for _try in range(6):
                sm = re.search(r"slots=[0-9A-Fa-f]+,([0-9A-Fa-f]+)", st)
                slot_now = sm.group(1) if sm else ""
                if slot_now and int(slot_now, 16) == idx:
                    break
                time.sleep(0.3)
                st = pipe("spirit scstate 0")
            if slot_now and int(slot_now, 16) != idx:
                say("STOP Yang 2 stayed %s after equip idx %d" % (slot_now, idx))
                alert("Soul core survey stopped", "Shrine equip said done but Yang 2 stayed index %s, not %d. Stopped." % (slot_now, idx))
                return 3
        if last_y1 == prev_y1 and "result=8" in eq:
            same_y1 += 1
        else:
            same_y1 = 0
        prev_y1 = last_y1
        if same_y1 >= 3:
            say("STOP three equips still published Y1 %s" % last_y1)
            alert("Soul core survey stopped", "Three equips still published left core %s. The equipped core is not changing. Stopped." % last_y1)
            return 3
        chunk, _ = read_from(pos)
        use = use_from(chunk) or latest_use()
        say("use %X" % use)
        uses, why = down_throws(last_y1, 2, use)
        if why == "no throw" or (why == "short" and len(uses) == 0):
            streak += 1
            rec = {"i": i, "n": len(rows), "idx": idx, "item": item, "y2": last_y1, "use2": False, "press": why, "button": "DOWN", "bursts": 0, "end_s": None}
            open(JSONL, "a", encoding="utf-8").write(json.dumps(rec) + "\n")
            alert("Soul core %d/%d %s" % (i, len(rows), item), "Item %s, yokai %s. No seal on the down press. Not kept." % (item, last_y1 or "?"))
            open(PROG, "w", encoding="utf-8").write("miss %d/%d item %s streak %d\n" % (i, len(rows), item, streak))
            say("MISS %d/%d %s streak %d" % (i, len(rows), item, streak))
            continue
        streak = 0
        if why == "no shortcut":
            alert("Soul core survey stopped", "Could not copy the equipped use id onto the Down button. Stopped.")
            open(PROG, "a", encoding="utf-8").write("STOP no shortcut\n")
            return 5
        if why.startswith("wrong"):
            alert("Soul core survey stopped", "Down press threw %s after equip published %s. Stopped so the other slot is not measured." % (why.split()[-1], last_y1))
            return 3
        second = uses[1] if len(uses) > 1 else (uses[0] if uses else "")
        m = measure(second)
        m.update({"i": i, "n": len(rows), "idx": idx, "item": item, "equip": eq, "button": "DOWN",
                  "y2": last_y1, "use2": len(uses) > 1, "use1": len(uses) > 0, "press": why, "button": "DOWN"})
        open(JSONL, "a", encoding="utf-8").write(json.dumps(m) + "\n")
        yokai = m.get("yokai") or last_y1
        early = "YES" if m["early"] else "no"
        end = "no end" if m["end_s"] is None else "%.2f s" % m["end_s"]
        if len(uses) < 2:
            text = "Item %s, left core %s. Only %d real throw(s) (%s). Not kept." % (item, last_y1, len(uses), why)
        else:
            text = "Item %s, yokai %s. Second use ends %s (%s). First hit %s, last hit %s, %d bursts, new objects %s. After last close %s. Ends before last hitbox: %s. Move: %s. Button DOWN." % (
                item, yokai, end, m["end_why"] or "?",
                m["first_hit_s"] if m["first_hit_s"] is not None else "?",
                m["last_hit_s"] if m["last_hit_s"] is not None else "?",
                m["bursts"], m["new_objects"], m["after_s"] if m["after_s"] is not None else "?",
                early, m["move"])
        alert("Soul core %d/%d %s" % (i, len(rows), item), text)
        open(PROG, "w", encoding="utf-8").write("done %d/%d item %s yokai %s end %s early %s\n" % (
            i, len(rows), item, yokai, end, early))
        say("KEEP " + text)
        time.sleep(0.4)
    say("SURVEY DONE %d" % len(rows))
    open(PROG, "a", encoding="utf-8").write("SURVEY DONE %d\n" % len(rows))
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        say("SURVEY ERROR %s" % e)
        open(PROG, "a", encoding="utf-8").write("ERROR %s\n" % e)
        raise
