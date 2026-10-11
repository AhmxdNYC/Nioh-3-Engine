r"""derive_sites.py - derive the add-on's patch-resilient site table from the v2.02 dump.

    py -I tools/derive_sites.py [path\to\Nioh3.dump.exe]          # writes sites.json + src/sites_gen.h
    py -I tools/derive_sites.py --check                             # exit 1 if sites.json / sites_gen.h are out of date

Every site gets a byte pattern with wildcards ('??') over everything a patch moves: rip-relative displacements and call /
jmp / jcc rel32 targets. Literal bytes keep struct offsets and immediates, so a pattern that still matches after a game patch
also proves the offsets the code relies on. The window grows (16 -> 96 bytes, then starting up to 24 bytes earlier) until the
pattern matches exactly once in the executable sections. Kinds:
  hook    function entry hooked with a relocated prologue of `head` bytes (no rip-relative / branch inside the head)
  fn      function the add-on calls (entry)
  call    an E8 rel32 call site the add-on retargets (its original callee is read from the rel32 at run time)
  global  a .data pointer / array: resolved from a rip-relative reference (anchor found automatically), target = ip + len + disp + addend
  check   a code reference whose literal bytes prove offsets (resolved, never patched)
Groups self-disable independently at run time (src/sites.cpp).  Needs capstone (py -m pip install capstone) and numpy.
"""
import json
import pathlib
import re
import struct
import sys

import capstone
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
DUMP = pathlib.Path(r"re\Nioh3.dump.exe")

# (name, group, kind, rva, head)  head: hook prologue bytes / 0
H = "hook"
SPEC = [
    # PUMP: the per-frame game-thread tick (PerFrameUpdater 5651D0 -> FastTravelSeqTickCall 3B29E4; the perf ASI's Tour used it as TT1)
    ("PerFrameTickCall", "PUMP", "call", 0x56529C, 0),
    # FIGHT: fight window + placement ring (rematch gates, verify, BossLog)
    ("BossBattleMode", "FIGHT", H, 0x23A2DCC, 5),
    ("BossBattleModeEnd", "FIGHT", H, 0x23A2F70, 9),
    ("Character::Create", "FIGHT", H, 0x1180740, 5),
    # BOSSLOG
    ("SetMissionFlag", "BOSSLOG", H, 0x70275C, 5),
    ("ShowBossGauge", "BOSSLOG", H, 0x23B8678, 9),
    ("SetMissionClearFlag", "BOSSLOG", H, 0x1C1DB88, 7),
    ("Character::AddDropTable", "BOSSLOG", H, 0x238B3EC, 5),
    ("Character::AddDropFixItem", "BOSSLOG", H, 0x238B194, 5),
    ("Character::AddDropKeyItemDirectGet", "BOSSLOG", H, 0x238B2C0, 5),
    ("Character::ForceDropAmurita", "BOSSLOG", H, 0x238DD00, 9),
    ("Character::ForceDropItem", "BOSSLOG", H, 0x238DED0, 9),
    ("Character::ForceDropItemAndGet", "BOSSLOG", H, 0x238DF64, 9),
    ("Character::EnemyForceKillWithItemDrop", "BOSSLOG", H, 0x238DAA0, 9),
    ("Item::AddItem", "BOSSLOG", H, 0x23A0A98, 5),
    ("Item::AddFixItem", "BOSSLOG", H, 0x23A07D4, 7),
    ("Item::AddMagicItem", "BOSSLOG", H, 0x23A0D48, 5),
    ("Item::AddItemNoNotice", "BOSSLOG", H, 0x23A0C64, 5),
    ("Item::AddEquipment", "BOSSLOG", H, 0x23A0208, 7),
    ("Item::AddEquipmentBySlot", "BOSSLOG", H, 0x23A0414, 7),
    ("Item::AddEquipmentPreset", "BOSSLOG", H, 0x23A0650, 7),
    ("Player::AddGuardianSpirit", "BOSSLOG", H, 0x23A7714, 9),
    ("Player::AddGuardianSpiritNoNotice", "BOSSLOG", H, 0x23A77AC, 9),
    ("Player::AddGuardianSpiritWithTimer", "BOSSLOG", H, 0x23A783C, 9),
    ("Player::AddSkill", "BOSSLOG", H, 0x23A7A20, 5),
    ("Player::AddSkillDojo", "BOSSLOG", H, 0x23A7B68, 5),
    ("Player::AddMoney", "BOSSLOG", H, 0x23A79A4, 9),
    ("Mission::SetReward", "BOSSLOG", H, 0x23A5C5C, 9),
    ("Mission::GetGameClearRewardNormal", "BOSSLOG", H, 0x23A444C, 9),
    ("Mission::GetGameClearRewardHard", "BOSSLOG", H, 0x23A43EC, 9),
    ("GameClearRewardGrant", "BOSSLOG", H, 0x2280FA0, 7),
    # RLOG (RematchLog, Phase 0 logger)
    ("MissionLoadStep", "RLOG", H, 0x223AF50, 10),
    ("EmakiMissionRequest", "RLOG", H, 0x10DBED0, 5),
    ("MissionSetupEmaki", "RLOG", H, 0x1C2794C, 5),
    ("MissionRequestById", "RLOG", H, 0x2239600, 7),
    ("MissionRequestEnqueue", "RLOG", H, 0x223953C, 5),
    ("MissionStartFromSave", "RLOG", H, 0x2237244, 14),
    ("MissionReplayMenu_Setup", "RLOG", H, 0x1DC8424, 5),
    ("MissionReplayMenuOpen_1DDA258", "RLOG", H, 0x1DDA258, 5),
    ("UI::OpenMissionReplayMenu", "RLOG", H, 0x23F90CC, 7),
    ("MissionSelectListBuild", "RLOG", H, 0x378674, 7),
    ("BattleScrollTop_Select", "RLOG", H, 0x1DAFE64, 5),
    ("EmakiItemUse_MenuHandler", "RLOG", H, 0xF1F060, 10),
    ("MissionReplayMenu_Decide", "RLOG", H, 0xF1FD80, 5),
    ("MissionRecordGet", "RLOG", H, 0x377B38, 9),
    ("MissionClearedGetSlot", "RLOG", H, 0x702B64, 10),
    ("MissionClearedGet_37AFC0", "RLOG", H, 0x37AFC0, 13),
    ("MissionUnlockedGet", "RLOG", H, 0x909218, 14),
    ("EmakiUniqueDefeatFlag_Serialize", "RLOG", H, 0x10C6E70, 12),
    ("ClearMapSetEntry", "RLOG", H, 0x217E884, 5),
    ("TrophyUnlock_body", "RLOG", H, 0x297F2E4, 9),
    ("MissionResultUpdate", "RLOG", H, 0x1C3413C, 7),
    ("RetryGadget_ResurrectPlayers", "RLOG", H, 0x1CA7468, 11),
    ("SetDecideRetry_impl", "RLOG", H, 0x1C251C8, 6),
    ("Mission::SetMissionResultClearType", "RLOG", H, 0x23A59BC, 9),
    ("UI::SetGameOverUIDeadType", "RLOG", H, 0x23B815C, 9),
    ("UI::SetGameOverUIFailType", "RLOG", H, 0x23B81C8, 9),
    ("Player::SetRetryPointIDAndSave", "RLOG", H, 0x23AB7C4, 5),
    ("Player::NoGameOver", "RLOG", H, 0x23AAA64, 9),
    ("BuildSaveFilePath", "RLOG", H, 0x1FD6DD0, 7),
    # INV
    ("InventoryAddItem", "INV", H, 0x54D324, 5),
    # CORE (every rematch route)
    ("fn:MissionClearedGetSlot", "CORE", "fn", 0x702B64, 0),
    ("fn:MissionRequestById", "CORE", "fn", 0x2239600, 0),
    ("g:DataDb", "CORE", "global", 0x45B9E30, 0),
    ("g:ClearDb", "CORE", "global", 0x4751848, 0),
    ("g:MissionMgr", "CORE", "global", 0x47514D0, 0),
    ("g:MissionQueue", "CORE", "global", 0x45BC420, 0),
    ("g:PlayMode", "CORE", "global", 0x4709B50, 0),
    ("g:NetMgr", "CORE", "global", 0x45C84F8, 0),
    ("g:CharMgr", "CORE", "global", 0x45B9E40, 0),
    ("g:GameMain", "CORE", "global", 0x4759910, 0),
    ("g:PlayerTab", "CORE", "global", 0x4745348, 0),
    ("chk:ListFilter_0x4000", "CORE", "check", 0x378B83, 0),
    ("chk:RecordId_0x158", "CORE", "check", 0x1D9FDD5, 0),
    # RA (R-a: the game's replay menu)
    ("fn:MenuOpenMissionReplay", "RA", "fn", 0x1DE28E8, 0),
    ("g:MenuMgr", "RA", "global", 0x45C84A8, 0),
    # EMAKI (E0 / E1)
    ("fn:EmakiMissionRequest", "EMAKI", "fn", 0x10DBED0, 0),
    ("chk:EmakiTable_A88_map20", "EMAKI", "check", 0x10DBF21, 0),
    ("chk:EmakiRecordGet_0x34", "EMAKI", "check", 0x1C1425C, 0),
    ("g:Inventory", "EMAKI", "global", 0x4751530, 0),
    # FNOW (Fight now = E1 straight to the boss): MissionRequestById's third argument is the request +0xC start point (0 = the
    # mission default via 1C17D8C), and the mission state +28h / +14h are the mission id / retry point MissionStartFromSave resumes
    ("chk:ReqById_StartArg_r8", "FNOW", "check", 0x2239628, 0),
    ("chk:ReqById_Start_to_0xC", "FNOW", "check", 0x22396A3, 0),
    ("chk:MissionState_28_14", "FNOW", "check", 0x22372BF, 0),
    # WARP (Fight now in the arena): the tail of the script native Player::SetPos 23AB4DC = the game's own player warp
    # (CharacterSetPositionYaw 1C27010, physics vcall +D0h, actor manager +48h/+D0h, 1C2A3CC, scene cfg +10B0h/+10C0h/+500h, 3A083C,
    # StagePathInitStep 3B35B4). The add-on replays it with its own x y z yaw; the call targets are read from these two matches.
    # head = minimum pattern length for a check (80: both windows cover every call / offset the add-on uses).
    ("chk:SetPos_tail_a", "WARP", "check", 0x23AB658, 80),
    ("chk:SetPos_tail_b", "WARP", "check", 0x23AB6A7, 80),
    # EXIT (pipe 'exit', the self-test): MissionRequestReturn(params, 1) = what the mission menu's leave does (MissionMenu_OnClose
    # 1DB70B0, params from 1BF7D68 = {101h, 1000000h, 10100h, 0, 0})
    ("fn:MissionRequestReturn", "EXIT", "fn", 0x22396D8, 0),
    # GATE (Fight now for gate bosses): the boss start function waits in Function::WaitWarpStart(gate) =
    # Online::IsExclusiveRightsValue -> the check core 3B2354(slot, key{u8, u32 mission, u32 gate, u32 value}, 1), called at 3B2323;
    # the door / box opens it with Online::RequestExclusiveRights -> 1CA52C4(slot, key) (23A6F03). The add-on watches the checks of
    # the Fight now mission for the row's gate id and requests exactly that key, as the door interaction / StartWarp would.
    ("call:RightsCheck", "GATE", "call", 0x3B2323, 0),
    ("fn:RightsRequest", "GATE", "fn", 0x1CA52C4, 0),
]
# preferred functions to anchor a global's reference in (first hit there wins; else any unique hit in .text)
PREFER = {"g:DataDb": 0x223963C, "g:ClearDb": 0x378C93, "g:MissionQueue": 0x10DC07A, "g:MenuMgr": 0x1DE28EC}

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True


def exec_sections(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    sh = pe + 24 + struct.unpack_from("<H", img, pe + 20)[0]
    out = []
    for i in range(nsec):
        vs, va = struct.unpack_from("<II", img, sh + 40 * i + 8)
        if struct.unpack_from("<I", img, sh + 40 * i + 36)[0] & 0x20000000:
            out.append((va, vs))
    return out


def wild_mask(img, start, n):
    """bytes img[start:start+n] with relocation fields wildcarded; None if the window cuts an undecodable run"""
    pat = []
    a = start
    while a < start + n:
        ins = next(md.disasm(img[a:a + 15], a), None)
        if ins is None:
            return None
        b = list(img[a:a + ins.size])
        m = [False] * ins.size
        if ins.disp_size and any(op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP for op in ins.operands):
            for k in range(ins.disp_offset, ins.disp_offset + ins.disp_size):
                m[k] = True
        if ins.group(capstone.CS_GRP_CALL) or ins.group(capstone.CS_GRP_JUMP):
            if ins.imm_size == 4 and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                for k in range(ins.imm_offset, ins.imm_offset + 4):
                    m[k] = True
        pat += [None if mm else bb for bb, mm in zip(b, m)]
        a += ins.size
    return pat[:n] if len(pat) >= n else pat


def to_str(pat):
    return " ".join("??" if b is None else "%02X" % b for b in pat)


def regex(pat):
    return re.compile(b"".join(b"." if b is None else re.escape(bytes([b])) for b in pat), re.DOTALL)


def count(img, secs, pat, limit=3):
    rx = regex(pat)
    hits = []
    for va, vs in secs:
        for m in rx.finditer(img, va, va + vs):
            hits.append(m.start())
            if len(hits) >= limit:
                return hits
    return hits


def derive(img, secs, rva, min_len=16):
    for back in (0, 8, 16, 24):
        for n in range(min_len, 97, 8):
            pat = wild_mask(img, rva - back, n + back) if back == 0 else None
            if back:
                # start on an instruction boundary `back` bytes before: find one by decoding from rva-back..rva-1
                for s in range(rva - back, rva):
                    ok = True
                    a = s
                    while a < rva:
                        ins = next(md.disasm(img[a:a + 15], a), None)
                        if ins is None:
                            ok = False
                            break
                        a += ins.size
                    if ok and a == rva:
                        pat = wild_mask(img, s, n + (rva - s))
                        back = rva - s
                        break
            if not pat:
                continue
            hits = count(img, secs, pat)
            if len(hits) == 1 and hits[0] == rva - back:
                return pat, back
    raise SystemExit("no unique pattern for 0x%X" % rva)


def find_refs(img, secs, target):
    """code positions p of a rip-relative disp32 that resolves to `target` (instruction ends right after the disp)"""
    out = []
    for va, vs in secs:
        code = np.frombuffer(img, dtype=np.uint8, count=vs, offset=va)
        for k in range(4):
            m = (vs - k) // 4
            a = np.frombuffer(code[k:k + 4 * m].tobytes(), dtype="<i4")
            pos = va + k + 4 * np.arange(m, dtype=np.int64)
            hit = np.nonzero(pos + 4 + a.astype(np.int64) == target)[0]
            out += [int(pos[i]) for i in hit]
    return sorted(out)


def insn_at_disp(img, p):
    for back in (3, 4, 2, 5, 6):
        a = p - back
        ins = next(md.disasm(img[a:a + 15], a), None)
        if ins and ins.disp_size == 4 and ins.disp_offset == back and a + ins.size == p + 4:
            return ins
    return None


def native_table(img):
    """script native table {char* name, fn*} x n (0x397E770 on v2.02): fn rva -> name"""
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    ib = struct.unpack_from("<Q", img, pe + 24 + 24)[0]
    i = img.find(b"\0Mission::BossBattleMode\0") + 1
    p = img.find(struct.pack("<Q", ib + i))
    while True:  # walk back to the table start
        q = struct.unpack_from("<Q", img, p - 16)[0]
        if not (ib < q < ib + len(img)) or not img[q - ib:q - ib + 40].split(b"\0")[0].replace(b"::", b"").replace(b"_", b"").isalnum():
            break
        p -= 16
    out = {}
    while True:
        n, f = struct.unpack_from("<QQ", img, p)
        if not (ib < n < ib + len(img)) or not (ib < f < ib + len(img)):
            break
        nm = img[n - ib:img.index(b"\0", n - ib)].decode("latin1")
        if "::" not in nm:
            break
        out[f - ib] = nm
        p += 16
    return out


def build(img):
    secs = exec_sections(img)
    natives = native_table(img)
    sites = []
    for name, group, kind, rva, head in SPEC:
        e = {"name": name, "group": group, "kind": kind, "rva": rva, "head": head}
        if kind == "global":
            refs = find_refs(img, secs, rva)
            cands = []
            for p in refs:
                ins = insn_at_disp(img, p)
                if ins:
                    cands.append(ins)
            pref = PREFER.get(name)
            cands.sort(key=lambda i: (0 if pref and i.address == pref else 1, i.address))
            done = False
            for ins in cands[:40]:
                try:
                    pat, back = derive(img, secs, ins.address, 12)
                except SystemExit:
                    continue
                e.update(anchor=ins.address, pattern=to_str(pat), offset=back, disp_off=ins.disp_offset, insn_len=ins.size, addend=0,
                         refs=len(refs), what="%s %s" % (ins.mnemonic, ins.op_str))
                done = True
                break
            if not done:
                raise SystemExit("no anchor for %s" % name)
        else:
            if kind == "hook":
                a = rva
                while a < rva + head:
                    ins = next(md.disasm(img[a:a + 15], a))
                    if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL) or "rip" in ins.op_str:
                        raise SystemExit("%s: head %d not relocatable (%s %s)" % (name, head, ins.mnemonic, ins.op_str))
                    a += ins.size
                if a != rva + head:
                    raise SystemExit("%s: head %d is not on an instruction boundary" % (name, head))
            if kind == "hook" and rva in natives:
                # script native: resolved by NAME through the native table (any patch keeps the names), head bytes checked
                e.update(kind="native", native=natives[rva], pattern=to_str(list(img[rva:rva + head])), offset=0)
                sites.append(e)
                continue
            pat, back = derive(img, secs, rva, (head or 16) if kind == "check" else 16 if kind != "call" else 12)
            e.update(pattern=to_str(pat), offset=back)
            if kind == "call":
                if img[rva] != 0xE8:
                    raise SystemExit("%s: not an E8 call" % name)
                e["callee"] = rva + 5 + struct.unpack_from("<i", img, rva + 1)[0]
        sites.append(e)
    return sites


def render_h(sites, ts, soi):
    o = ["// GENERATED by tools/derive_sites.py from the v2.02 dump - do not edit. tests/test_sites.py verifies it.",
         "#pragma once", "#include <cstdint>", "namespace sites {",
         "constexpr uint32_t kKnownTimestamp = 0x%08X, kKnownSizeOfImage = 0x%X;" % (ts, soi),
         "enum Kind : uint8_t { kHook = 0, kFn = 1, kCall = 2, kGlobal = 3, kCheck = 4, kNative = 5 };  // kNative: hook resolved by script-native name",
         "struct Def { const char* name; const char* group; Kind kind; uint32_t rva; uint8_t head; const char* pattern; uint8_t offset;"
         " uint8_t dispOff; uint8_t insnLen; int32_t addend; uint32_t anchor; const char* native; };",
         "static const Def kDefs[] = {"]
    kmap = {"hook": "kHook", "fn": "kFn", "call": "kCall", "global": "kGlobal", "check": "kCheck", "native": "kNative"}
    for s in sites:
        o.append('    {"%s", "%s", %s, 0x%X, %d, "%s", %d, %d, %d, %d, 0x%X},' % (
            s["name"], s["group"], kmap[s["kind"]], s["rva"], s["head"], s["pattern"], s["offset"], s.get("disp_off", 0),
            s.get("insn_len", 0), s.get("addend", 0), s.get("anchor", 0)) + "")
        o[-1] = o[-1][:-2] + ', %s},' % ('"%s"' % s["native"] if s.get("native") else "nullptr")
    o += ["};", "constexpr int kCount = (int)(sizeof(kDefs) / sizeof(kDefs[0]));", "}  // namespace sites", ""]
    return "\r\n".join(o)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dump = pathlib.Path(args[0]) if args else DUMP
    img = dump.read_bytes()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    ts = struct.unpack_from("<I", img, pe + 8)[0]
    soi = struct.unpack_from("<I", img, pe + 24 + 56)[0]
    sites = build(img)
    js = json.dumps({"build": {"timestamp": "0x%08X" % ts, "size_of_image": "0x%X" % soi}, "sites": sites}, indent=1)
    hdr = render_h(sites, ts, soi)
    jp, hp = ROOT / "sites.json", ROOT / "src" / "sites_gen.h"
    if "--check" in sys.argv:
        ok = jp.exists() and hp.exists() and jp.read_text() == js and hp.read_bytes() == hdr.encode()
        print("sites.json / sites_gen.h: %s" % ("in sync" if ok else "OUT OF DATE"))
        return 0 if ok else 1
    jp.write_text(js)
    hp.write_bytes(hdr.encode())
    print("wrote %s and %s: %d sites" % (jp, hp, len(sites)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
