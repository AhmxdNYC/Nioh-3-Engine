"""#106727 (reports #77 / #80): the registry Post research trace (one synchronous log open+write+close per script->placement message,
up to 20000, on the game thread) is off by default and only [Debug] PostTrace=1 turns it on. Static check, no game."""
import pathlib, re, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
src = (ROOT / "src/training.c").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3Training.ini").read_text(encoding="utf-8", errors="replace")
fail = []
if not re.search(r"static volatile LONG g_post_trace = 0;", src): fail.append("g_post_trace must default to 0")
if 'GetPrivateProfileIntA("Debug", "PostTrace", 0, ini)' not in src: fail.append("[Debug] PostTrace (default 0) must be read")
dbg = ini.split("[Debug]", 1)[1].split("\n[", 1)[0] if "[Debug]" in ini else ""
if not re.search(r"^PostTrace = 0\s*$", dbg, re.M): fail.append("shipped ini [Debug] must have PostTrace = 0")
if "if (g_post_trace &&" not in src: fail.append("h_post must check g_post_trace before logging")
for f in fail: print("FAIL:", f)
if not fail: print("PASS: Post trace off by default ([Debug] PostTrace = 0), opt-in only")
sys.exit(1 if fail else 0)
