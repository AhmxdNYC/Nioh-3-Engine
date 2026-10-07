# ld_verify.py - offline byte check of the load-end (VSM blackout) patch sites against re\Nioh3.dump.exe (raw == RVA)
import sys
D=open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe","rb").read()
SITES=[
 # id, rva, original bytes (with context), patched bytes, note
 ("VBF",  0x18756C, "44 8A E3 EB 03 45 32 E4", "45 32 E4 EB 03 45 32 E4", "VsmEnvFrameUpdate: blackout flag r12b := 0 (master switch: budget floor + lifespan)"),
 ("VLS",  0x1876E6, "41 0F 44 C7",             "41 8B C7 90",             "lifespan per clipmap level: always the gameplay value 120*(k+1)"),
 ("VAT",  0x5A999F, "B2 01",                   "B2 00",                   "VsmBlackoutAgeThresholds: keep 10/200-frame refresh thresholds in blackout"),
 ("VBB",  0x5EA125, "B8 40 00 00 00",          "B8 20 00 00 00",          "VsmTilesPerFrameBudget blackout floor 64 -> 32 (optional, value is the imm32)"),
 ("TSB0", 0x28A4B65,"",                        "",                        "info: [g+75A8] := r14d(0) at load start (texture tile request cap lifted)"),
 ("TSB1", 0x28A930D,"41 C7 82 A8 75 00 00 80 00 00 00","",              "info: [g+75A8] := 0x80 at load end"),
]
ok=True
for sid,rva,orig,new,note in SITES:
    if not orig: print(f"{sid:5} {rva:#09x} {D[rva:rva+8].hex(' ')}  ({note})"); continue
    ob=bytes.fromhex(orig); cur=D[rva:rva+len(ob)]
    st="OK" if cur==ob else "MISMATCH"; ok&=cur==ob
    print(f"{sid:5} {rva:#09x} {st:8} {cur.hex(' ')} -> {new}   {note}")
print("ALL SITES MATCH" if ok else "SITE MISMATCH")
