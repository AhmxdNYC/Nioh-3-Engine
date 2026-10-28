"""Copy-only save slot prototype. Never resolves or opens installed game saves."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import uuid

MARKER = 'NIOH3_COPY_SANDBOX_V1'

def plain(path):
    p = Path(path).absolute()
    for part in [p, *p.parents]:
        if part.exists() or part.is_symlink():
            s = part.lstat()
            if stat.S_ISLNK(s.st_mode) or getattr(s, 'st_file_attributes', 0) & 0x400:
                raise ValueError('Links/reparse points forbidden: ' + str(part))
    return p

def safe_root(path):
    p = plain(path)
    normalized = str(p).replace('\\', '/').lower()
    if '/koeitecmo/' in normalized or normalized.endswith('/koeitecmo') or '/userdata/' in normalized:
        raise ValueError('Installed save/cloud paths forbidden; use a new disposable sandbox')
    return p

def read(path):
    plain(path)
    return json.loads(path.read_text(encoding='utf-8'))

def write(path, obj):
    plain(path)
    tmp = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    with tmp.open('x', encoding='utf-8') as f:
        json.dump(obj, f, indent=2); f.flush(); os.fsync(f.fileno())
    os.replace(tmp, path)

def inventory(root):
    plain(root)
    if not root.is_dir(): raise ValueError('Missing directory: ' + str(root))
    files, dirs = {}, []
    for folder, subdirs, names in os.walk(root, followlinks=False, onerror=lambda e: (_ for _ in ()).throw(e)):
        for name in subdirs + names: plain(Path(folder) / name)
        dirs.extend((Path(folder)/d).relative_to(root).as_posix() for d in subdirs)
        for name in sorted(names):
            p = Path(folder)/name
            if not p.is_file() or p.stat().st_nlink != 1: raise ValueError('Only ordinary single-link files allowed')
            h = hashlib.sha256()
            with p.open('rb') as f:
                for block in iter(lambda: f.read(1024*1024), b''): h.update(block)
            files[p.relative_to(root).as_posix()] = {'size':p.stat().st_size, 'sha256':h.hexdigest()}
    return {'files':files, 'dirs':sorted(dirs)}

def identity(root):
    data = root/'Savedata'; plain(data)
    accounts = list(data.iterdir()) if data.is_dir() else []
    ids = [p.name for p in accounts if re.fullmatch(r'\d{17}', p.name) and p.is_dir()]
    if len(ids) != 1: raise ValueError('Require exactly one 17-digit account under Savedata')
    for p in accounts:
        if p.is_dir() and p.name != ids[0]: raise ValueError('Unexpected account directory')
    account = data/ids[0]
    for rel in ['SAVEDATA00/SAVEDATA.BIN', 'SAVEDATA00/BACKUP.BIN', 'SYSTEMSAVEDATA00/SAVEDATA.BIN']:
        p = account/rel; plain(p)
        if not p.is_file() or not p.stat().st_size: raise ValueError('Incomplete save copy: '+rel)
    return ids[0]

def copy_verified(src, dst):
    expected = inventory(src)
    if not expected['files']: raise ValueError('Empty save copy')
    dst.mkdir()  # exclusive destination, never overwrite
    for rel in expected['dirs']: (dst/rel).mkdir(parents=True, exist_ok=True)
    for rel in expected['files']:
        out=dst/rel;out.parent.mkdir(parents=True, exist_ok=True)
        with (src/rel).open('rb') as a, out.open('xb') as b:
            shutil.copyfileobj(a,b);b.flush();os.fsync(b.fileno())
    if inventory(src)!=expected or inventory(dst)!=expected: raise ValueError('Copy changed during transfer')
    return expected

def name_ok(name):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9 _-]{0,47}',name) or name.endswith(' '): raise ValueError('Invalid slot/transaction name')
    if name.upper() in {'CON','PRN','AUX','NUL',*(f'COM{i}' for i in range(1,10)),*(f'LPT{i}' for i in range(1,10))}:
        raise ValueError('Reserved Windows name')
    return name

class Slots:
    def __init__(self, root):
        self.root=safe_root(root)
        if read(self.root/'sandbox.json') != {'format':MARKER}: raise ValueError('Not a copy sandbox')
        # Refuse redirected descendants before taking a lock or writing anything.
        inventory(self.root)
    @contextlib.contextmanager
    def lock(self):
        p=self.root/'operation.lock'
        with p.open('x') as f: f.write(str(os.getpid()))
        try: yield
        finally: p.unlink()
    def idle(self):
        if (self.root/'pending.json').exists(): raise ValueError('Unfinished transaction; run recover first')
    def snapshot(self, name):
        name_ok(name)
        with self.lock():
            self.idle();src=self.root/'current';account=identity(src)
            stage=self.root/'slots'/('.stage-'+uuid.uuid4().hex);stage.mkdir()
            manifest=copy_verified(src,stage/'data')
            write(stage/'manifest.json',{'account':account,'tree':manifest})
            target=self.root/'slots'/name
            if target.exists(): raise ValueError('Slot exists; immutable snapshots cannot be overwritten')
            stage.rename(target)
        return {'saved':name,'files':len(manifest['files'])}
    def verify(self, name):
        p=self.root/'slots'/name_ok(name);manifest=read(p/'manifest.json')
        if inventory(p/'data') != manifest['tree'] or identity(p/'data')!=manifest['account']:
            raise ValueError('Slot checksum/account mismatch')
        return manifest
    def switch(self, name, fail_at=None):
        with self.lock():
            self.idle();slot=self.verify(name);current=self.root/'current'
            if identity(current)!=slot['account']: raise ValueError('Cross-account switch forbidden')
            before=inventory(current);txid=uuid.uuid4().hex;tx=self.root/'transactions'/txid;tx.mkdir()
            after=copy_verified(self.root/'slots'/name/'data',tx/'incoming')
            if after!=slot['tree']: raise ValueError('Slot changed before switch')
            # A separate verified copy must exist BEFORE detaching current.
            # The retained previous directory is an additional undo source,
            # not a substitute for the mandatory pre-mutation backup.
            backup=copy_verified(current,tx/'backup')
            if backup!=before: raise ValueError('Current changed during mandatory backup')
            write(tx/'backup.json',{'verified':True,'tree':backup})
            if fail_at=='backed-up': raise RuntimeError('Injected interruption: backed-up')
            journal={'id':txid,'slot':name,'before':before,'after':after}
            write(tx/'receipt.json',journal);write(self.root/'pending.json',journal)
            if fail_at=='prepared': raise RuntimeError('Injected interruption: prepared')
            if inventory(current)!=before: raise ValueError('Current copy changed before swap')
            if inventory(tx/'backup')!=before: raise ValueError('Mandatory backup verification failed')
            current.rename(tx/'previous')
            if fail_at=='detached': raise RuntimeError('Injected interruption: detached')
            (tx/'incoming').rename(current)
            if fail_at=='installed': raise RuntimeError('Injected interruption: installed')
            if inventory(current)!=after: raise ValueError('Installed copy verification failed; recover required')
            write(tx/'complete.json',{'verified':True})
            (self.root/'pending.json').unlink()
        return {'switched':name,'undo':txid,'files':len(after['files'])}
    def recover(self):
        with self.lock():
            pending=self.root/'pending.json'
            if not pending.exists(): return {'recovery':'none'}
            j=read(pending);tx=self.root/'transactions'/name_ok(j['id']);cur=self.root/'current'
            if (tx/'previous').exists():
                if inventory(tx/'previous')!=j['before']: raise ValueError('Original backup changed; retain all evidence')
                if cur.exists(): cur.rename(tx/('rejected-'+uuid.uuid4().hex))
                (tx/'previous').rename(cur)
            if inventory(cur)!=j['before']: raise ValueError('Cannot verify original copy')
            write(tx/'recovered.json',{'verified':True});pending.unlink()
        return {'recovery':'original restored','transaction':j['id']}
    def restore(self, txid):
        # Publish the retained pre-switch tree as another immutable slot, then use
        # the same switch transaction so undo itself also has an undo backup.
        name='undo-'+name_ok(txid)+'-'+uuid.uuid4().hex[:8]
        with self.lock():
            self.idle();tx=self.root/'transactions'/txid;j=read(tx/'receipt.json')
            if inventory(tx/'previous')!=j['before']: raise ValueError('Undo backup changed')
            slot=self.root/'slots'/name;slot.mkdir()
            tree=copy_verified(tx/'previous',slot/'data')
            if tree!=j['before']: raise ValueError('Undo backup changed during copy')
            write(slot/'manifest.json',{'account':identity(slot/'data'),'tree':tree})
        return self.switch(name)

def init(root):
    root=safe_root(root);root.mkdir() # must be NEW; cannot designate existing real saves
    for name in ['current','slots','transactions']: (root/name).mkdir()
    write(root/'sandbox.json',{'format':MARKER})
    return {'sandbox':str(root),'mode':'COPY ONLY','next':'Put an existing COPY of the NIOH3 tree under current, then backup NAME'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sandbox',required=True,help='New disposable workspace, never an installed save root')
    p.add_argument('command',choices=['init','list','backup','verify','switch','restore','recover'])
    p.add_argument('name',nargs='?');a=p.parse_args()
    try:
        if a.command=='init': result=init(a.sandbox)
        else:
            s=Slots(a.sandbox)
            if a.command=='list': result=sorted(x.name for x in (s.root/'slots').iterdir() if not x.name.startswith('.'))
            elif a.command=='recover': result=s.recover()
            else:
                if not a.name: raise ValueError('NAME required')
                result=getattr(s,{'backup':'snapshot'}.get(a.command,a.command))(a.name)
        print(json.dumps(result,indent=2));return 0
    except (OSError,ValueError,KeyError,RuntimeError) as e:
        print(json.dumps({'error':str(e),'mode':'COPY ONLY'}));return 1
if __name__=='__main__': raise SystemExit(main())
