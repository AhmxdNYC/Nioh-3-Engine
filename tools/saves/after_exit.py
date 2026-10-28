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

def no_other_game(origin_pid=None):
    import csv,subprocess
    p=subprocess.run(['tasklist','/FI','IMAGENAME eq Nioh3.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True,
                     creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    return not any(row and row[0].lower()=='nioh3.exe' and (origin_pid is None or int(row[1])!=origin_pid)
                   for row in csv.reader(p.stdout.splitlines()))

def no_game():return no_other_game()

def run(root,pid,created,slot,token,action,*,real_root=False,save_root=None,game_factory=Game,clear=no_game,others=no_other_game,clock=time.monotonic,sleep=time.sleep):
    manager=s.Slots(root);s.name_ok(slot);s.name_ok(token)
    if action not in ('switch','backup'):raise ValueError('Unsupported request')
    expected=manager.verify(slot) if action=='switch' else None
    if action=='backup' and (manager.root/'slots'/slot).exists():raise ValueError('Backup slot exists')
    request=manager.root/('request-'+token);request.mkdir() # nonce consumed, even on failure
    cancel=manager.root/('cancel-'+token)
    game=None
    try:
        game=game_factory(pid,created);next_process_check=0
        s.write(request/'receipt.json',{'state':'waiting','pid':pid,'created':created,'slot':slot,'action':action})
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
        s.write(request/'receipt.json',{'state':'complete','result':result,'copy_only':not real_root})
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
    p.add_argument('--action',choices=['backup','switch'],required=True);a=p.parse_args()
    try:print(json.dumps(run(a.sandbox,a.pid,a.created,a.slot,a.token,a.action,real_root=a.real_root,save_root=a.save_root)));return 0
    except Exception as e:print(json.dumps({'error':str(e)}));return 1
if __name__=='__main__':raise SystemExit(main())
