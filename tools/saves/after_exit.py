"""Copy-only helper: bind to one game lifetime, wait for exit 0, then apply a slot operation."""
import argparse
import ctypes
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import time
import saveslot as s

class Game:
    def __init__(self,pid,created):
        if os.name!='nt':raise ValueError('Windows helper only; Proton validation pending')
        self.k=ctypes.WinDLL('kernel32',use_last_error=True)
        k=self.k;k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
        k.CloseHandle.argtypes=[w.HANDLE];k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]
        k.GetExitCodeProcess.argtypes=[w.HANDLE,ctypes.POINTER(w.DWORD)]
        k.GetProcessTimes.argtypes=[w.HANDLE,*([ctypes.POINTER(w.FILETIME)]*4)]
        k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,ctypes.POINTER(w.DWORD)]
        self.h=k.OpenProcess(0x100000|0x1000,False,pid)
        if not self.h:raise OSError('Cannot open originating game process')
        try:
            values=[w.FILETIME() for _ in range(4)]
            if not k.GetProcessTimes(self.h,*[ctypes.byref(x) for x in values]):raise OSError('Cannot identify process lifetime')
            actual=(values[0].dwHighDateTime<<32)|values[0].dwLowDateTime
            n=w.DWORD(32768);buf=ctypes.create_unicode_buffer(n.value)
            if actual!=created or not k.QueryFullProcessImageNameW(self.h,0,buf,ctypes.byref(n)) or Path(buf.value).name.lower()!='nioh3.exe':
                raise ValueError('Origin PID/lifetime/image mismatch')
        except Exception:self.close();raise
    def exited(self):
        result=self.k.WaitForSingleObject(self.h,0)
        if result==0x102:return None
        if result!=0:raise OSError('Process wait failed')
        code=w.DWORD()
        if not self.k.GetExitCodeProcess(self.h,ctypes.byref(code)):raise OSError('Cannot read exit status')
        return code.value
    def close(self):
        if self.h:self.k.CloseHandle(self.h);self.h=None

def request_quit(game,pid):
    """Ask the validated originating window to close; never kill or click confirmation dialogs."""
    if game.exited() is not None:return
    user=ctypes.WinDLL('user32',use_last_error=True)
    callback=ctypes.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
    user.EnumWindows.argtypes=[callback,w.LPARAM];user.EnumWindows.restype=w.BOOL
    user.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)]
    user.IsWindowVisible.argtypes=[w.HWND];user.GetWindow.argtypes=[w.HWND,w.UINT];user.GetWindow.restype=w.HWND
    user.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];user.PostMessageW.restype=w.BOOL
    windows=[]
    @callback
    def visit(hwnd,_):
        owner=w.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid and user.IsWindowVisible(hwnd) and not user.GetWindow(hwnd,4):windows.append(hwnd)
        return True
    if not user.EnumWindows(visit,0):raise OSError('Cannot enumerate originating game window')
    if len(windows)!=1:raise ValueError('No unique game window; quit normally or cancel and retry')
    # Recheck PID and original lifetime immediately before posting WM_CLOSE.
    owner=w.DWORD();user.GetWindowThreadProcessId(windows[0],ctypes.byref(owner))
    if game.exited() is not None:return
    if owner.value!=pid or not user.PostMessageW(windows[0],0x10,0,0):raise OSError('Graceful close request failed')

def launch_game():
    # Fixed game identity, no configurable shell command or interpolated slot name.
    if os.name!='nt':raise ValueError('Windows/Proton Steam launch requires native acceptance')
    os.startfile('steam://rungameid/3681010')

def no_other_game(origin_pid=None):
    import csv,subprocess
    p=subprocess.run(['tasklist','/FI','IMAGENAME eq Nioh3.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True,
                     creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    return not any(row and row[0].lower()=='nioh3.exe' and (origin_pid is None or int(row[1])!=origin_pid)
                   for row in csv.reader(p.stdout.splitlines()))

def no_game():return no_other_game()

def run(root,pid,created,slot,token,action,*,real_root=False,save_root=None,relaunch=False,quit_game=False,
        game_factory=Game,clear=no_game,others=no_other_game,clock=time.monotonic,sleep=time.sleep,
        quitter=request_quit,launcher=launch_game):
    manager=s.Slots(root);s.name_ok(slot);s.name_ok(token)
    if action not in ('switch','backup'):raise ValueError('Unsupported request')
    if (relaunch or quit_game) and action!='switch':raise ValueError('Relaunch/quit requires a switch request')
    if quit_game and not relaunch:raise ValueError('Automatic quit requires relaunch')
    expected=manager.verify(slot) if action=='switch' else None
    if action=='backup' and (manager.root/'slots'/slot).exists():raise ValueError('Backup slot exists')
    request=manager.root/('request-'+token);request.mkdir() # nonce consumed, even on failure
    cancel=manager.root/('cancel-'+token)
    game=None
    try:
        game=game_factory(pid,created);next_process_check=0
        s.write(request/'receipt.json',{'state':'waiting','pid':pid,'created':created,'slot':slot,'action':action})
        if quit_game:
            if cancel.exists() or not others(pid):raise ValueError('Cancelled or different game PID before close')
            quitter(game,pid)
            s.write(request/'receipt.json',{'state':'waiting','slot':slot,'quit_requested':True,'relaunch':relaunch})
        while True:
            if cancel.exists():raise ValueError('Explicit user cancellation')
            now=clock()
            if now>=next_process_check:
                if not others(pid):raise ValueError('Different game PID detected; request cancelled')
                next_process_check=now+1
            code=game.exited()
            if code is not None:
                if code!=0:raise ValueError('Abnormal exit; copy switch refused')
                break
            sleep(.1)
        if not clear():raise ValueError('Another game process exists; copy switch refused')
        if cancel.exists():raise ValueError('Cancelled')
        if action=='switch' and manager.verify(slot)!=expected:raise ValueError('Slot changed while waiting')
        # No target-root access until the originating process exits cleanly.
        if real_root:
            if not save_root:raise ValueError('Explicit save root required')
            from real_root import RealRoot
            adapter=RealRoot(root,save_root,clear)
            result=adapter.switch(slot) if action=='switch' else adapter.snapshot(slot)
        else:
            result=manager.switch(slot) if action=='switch' else manager.snapshot(slot)
        # Save commit and relaunch have separate outcomes: launch failure never hides a completed switch.
        receipt={'state':'complete','result':result,'copy_only':not real_root,'relaunch':'not-requested'}
        s.write(request/'receipt.json',receipt)
        if relaunch:
            try:
                if cancel.exists():raise ValueError('Cancelled before relaunch; switched save retained')
                if not clear():raise ValueError('Game already running; relaunch suppressed')
                launcher();receipt['relaunch']='requested'
            except Exception as e:receipt.update(relaunch='failed',launch_error=str(e))
            s.write(request/'receipt.json',receipt)
            result=dict(result,relaunch=receipt['relaunch'])
        return result
    except Exception as e:
        s.write(request/'receipt.json',{'state':'refused','error':str(e),'copy_only':not real_root});raise
    finally:
        if game:game.close()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['sandbox','slot','token']:p.add_argument('--'+key,required=True)
    p.add_argument('--pid',type=int,required=True);p.add_argument('--created',type=int,required=True)
    p.add_argument('--real-root',action='store_true');p.add_argument('--save-root')
    p.add_argument('--relaunch',action='store_true');p.add_argument('--quit',dest='quit_game',action='store_true')
    p.add_argument('--action',choices=['backup','switch'],required=True);a=p.parse_args()
    try:print(json.dumps(run(a.sandbox,a.pid,a.created,a.slot,a.token,a.action,real_root=a.real_root,save_root=a.save_root,relaunch=a.relaunch,quit_game=a.quit_game)));return 0
    except Exception as e:print(json.dumps({'error':str(e)}));return 1
if __name__=='__main__':raise SystemExit(main())
