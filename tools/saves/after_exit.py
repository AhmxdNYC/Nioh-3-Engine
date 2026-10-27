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

def no_game():
    import csv,subprocess
    p=subprocess.run(['tasklist','/FI','IMAGENAME eq Nioh3.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    return not any(row and row[0].lower()=='nioh3.exe' for row in csv.reader(p.stdout.splitlines()))

def run(root,pid,created,slot,token,action,*,game_factory=Game,clear=no_game,clock=time.monotonic,sleep=time.sleep):
    manager=s.Slots(root);s.name_ok(slot);s.name_ok(token)
    if action not in ('switch','backup'):raise ValueError('Unsupported request')
    expected=manager.verify(slot) if action=='switch' else None
    if action=='backup' and (manager.root/'slots'/slot).exists():raise ValueError('Backup slot exists')
    request=manager.root/('request-'+token);request.mkdir() # nonce consumed, even on failure
    cancel=manager.root/('cancel-'+token)
    game=None
    try:
        game=game_factory(pid,created);deadline=clock()+120
        s.write(request/'receipt.json',{'state':'waiting','pid':pid,'created':created,'slot':slot,'action':action})
        while True:
            if cancel.exists():raise ValueError('Cancelled after leaving main menu')
            if clock()>=deadline:raise ValueError('Timed out waiting for normal game exit')
            code=game.exited()
            if code is not None:
                if code!=0:raise ValueError('Abnormal exit; copy switch refused')
                break
            sleep(.1)
        if not clear():raise ValueError('Another game process exists; copy switch refused')
        if cancel.exists():raise ValueError('Cancelled')
        if action=='switch' and manager.verify(slot)!=expected:raise ValueError('Slot changed while waiting')
        # Slots owns the filesystem transaction lock; no live save paths are supported.
        result=manager.switch(slot) if action=='switch' else manager.snapshot(slot)
        s.write(request/'receipt.json',{'state':'complete','result':result,'copy_only':True})
        return result
    except Exception as e:
        s.write(request/'receipt.json',{'state':'refused','error':str(e),'copy_only':True});raise
    finally:
        if game:game.close()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['sandbox','slot','token']:p.add_argument('--'+key,required=True)
    p.add_argument('--pid',type=int,required=True);p.add_argument('--created',type=int,required=True)
    p.add_argument('--action',choices=['backup','switch'],required=True);a=p.parse_args()
    try:print(json.dumps(run(a.sandbox,a.pid,a.created,a.slot,a.token,a.action)));return 0
    except Exception as e:print(json.dumps({'error':str(e)}));return 1
if __name__=='__main__':raise SystemExit(main())
