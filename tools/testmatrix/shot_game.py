"""shot_game.py PID OUT.png - minimize Steam windows, bring game to front, grab only the game window (#95578)."""
import ctypes, ctypes.wintypes as w, subprocess, sys, time, csv, io
from PIL import ImageGrab
u = ctypes.windll.user32; gp = int(sys.argv[1]); out = sys.argv[2]
names = {int(r[1]): r[0].lower() for r in csv.reader(io.StringIO(subprocess.run(['tasklist', '/fo', 'csv', '/nh'], capture_output=True, text=True).stdout)) if len(r) > 1 and r[1].isdigit()}
hw, mini = [], 0
@ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
def cb(h, _):
    global mini
    pid = w.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
    if u.IsWindowVisible(h):
        n = names.get(pid.value, '')
        if n.startswith('steam'): u.ShowWindow(h, 6); mini += 1
        elif pid.value == gp: hw.append(h)
    return True
u.EnumWindows(cb, 0); time.sleep(1.5)
if hw:
    u.SetForegroundWindow(hw[0]); time.sleep(1.5)
    r = w.RECT(); u.GetWindowRect(hw[0], ctypes.byref(r)); ImageGrab.grab((r.left, r.top, r.right, r.bottom)).save(out)
else:
    ImageGrab.grab().save(out)
print(f"steam windows minimized={mini} game_window={bool(hw)}")
