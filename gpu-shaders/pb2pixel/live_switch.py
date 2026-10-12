"""Private one-shader transaction. Operator must hold the device game lease.
Never launches/stops a game, changes settings, or restores saves.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

ORIGINAL='c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a'
CANDIDATE='d8511aa0bea0d778de9e8d29342470cec644fe907559415ec0bafdfc9f0844d5'
RELATIVE='shader_dx12/0xb4751b0f.file'


def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def plain(p):
    p=Path(p).absolute()
    for q in (p,*p.parents):
        if q.exists() and (q.is_symlink() or getattr(q.stat(),'st_file_attributes',0)&0x400):
            raise RuntimeError('reparse/symlink path refused: '+str(q))
    return p


def running():
    if os.name!='nt':raise RuntimeError('Windows test switch only; Proton needs separate admission')
    r=subprocess.run(['tasklist','/FI','IMAGENAME eq Nioh3.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    return 'nioh3.exe' in r.stdout.lower()


def write_json(p,v):
    t=p.with_name(p.name+'.'+uuid.uuid4().hex+'.tmp')
    with t.open('x',encoding='utf-8') as f:
        json.dump(v,f,indent=2);f.flush();os.fsync(f.fileno())
    os.replace(t,p)


def copy_new(src,dst):
    with Path(src).open('rb') as a,Path(dst).open('xb') as b:
        shutil.copyfileobj(a,b);b.flush();os.fsync(b.fileno())


def swap(target,source,before,after,is_running=running):
    if is_running() or sha(target)!=before or sha(source)!=after:raise RuntimeError('running/input ownership mismatch')
    temp=target.with_name('.pb2pixel-'+uuid.uuid4().hex+'.tmp')
    try:
        copy_new(source,temp)
        if sha(temp)!=after or sha(target)!=before or is_running():raise RuntimeError('ownership/process changed before publication')
        os.replace(temp,target)
        if sha(target)!=after:raise RuntimeError('post-publication verification failed; retain receipt')
    finally:
        if temp.exists():temp.unlink()


def activate(game,package,run,is_running=running):
    game,package,run=map(plain,(game,package,run));target=plain(game/RELATIVE)
    plain(package/'candidate.file')
    if is_running() or sha(target)!=ORIGINAL or sha(package/'candidate.file')!=CANDIDATE:raise RuntimeError('activation preflight failed')
    if run==game or game in run.parents:raise RuntimeError('receipt/backup must be outside game directory')
    run.mkdir(parents=True,exist_ok=False)
    receipt=run/'transaction.json';lock=game/'.pb2pixel-test.lock'
    state=dict(target=str(target),lock=str(lock),receipt=str(receipt),original=ORIGINAL,candidate=CANDIDATE,status='PREPARING')
    # Never remove an existing unresolved transaction automatically.
    with lock.open('x',encoding='utf-8') as f:f.write(str(receipt));f.flush();os.fsync(f.fileno())
    write_json(receipt,state)
    copy_new(target,run/'entry.file')
    if sha(run/'entry.file')!=ORIGINAL:raise RuntimeError('entry backup mismatch')
    state['status']='BACKUP_VERIFIED_ACTIVATION_PENDING';write_json(receipt,state)
    swap(target,package/'candidate.file',ORIGINAL,CANDIDATE,is_running)
    state['status']='ACTIVE';write_json(receipt,state)
    return receipt


def restore(receipt,is_running=running):
    receipt=plain(receipt);state=json.loads(receipt.read_text());target=plain(state['target']);lock=plain(state['lock'])
    if state['original']!=ORIGINAL or state['candidate']!=CANDIDATE or state['receipt']!=str(receipt):raise RuntimeError('receipt identity mismatch')
    if target.name!='0xb4751b0f.file' or target.parent.name!='shader_dx12' or lock!=target.parent.parent/'.pb2pixel-test.lock':raise RuntimeError('target scope mismatch')
    if not lock.is_file() or lock.read_text()!=str(receipt):raise RuntimeError('transaction lock not owned')
    backup=plain(receipt.parent/'entry.file')
    if is_running() or sha(backup)!=ORIGINAL:raise RuntimeError('running/bad backup')
    current=sha(target)
    if current==CANDIDATE:swap(target,backup,CANDIDATE,ORIGINAL,is_running)
    elif current!=ORIGINAL:raise RuntimeError('target changed externally; keep receipt and backup')
    if is_running() or sha(target)!=ORIGINAL:raise RuntimeError('restore verification failed')
    state['status']='RESTORED_VERIFIED';write_json(receipt,state)
    # Durable verified-restoration receipt precedes lock release.
    if lock.read_text()!=str(receipt):raise RuntimeError('lock owner changed')
    lock.unlink()


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['status','activate','restore'])
    p.add_argument('--game',type=Path);p.add_argument('--package',type=Path,default=Path(__file__).resolve().parent)
    p.add_argument('--run',type=Path);p.add_argument('--receipt',type=Path);a=p.parse_args()
    if a.action=='restore':restore(a.receipt)
    elif a.action=='status':print(json.dumps({'target_sha256':sha(plain(a.game/RELATIVE)),'game_running':running(),'lock_exists':(a.game/'.pb2pixel-test.lock').exists()}))
    else:print(activate(a.game,a.package,a.run))


if __name__=='__main__':main()
