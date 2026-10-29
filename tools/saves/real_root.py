"""Explicit opt-in full-root adapter. Caller must prove clean exit before construction/use."""
from datetime import datetime
from pathlib import Path
import uuid
import saveslot as s


class RealRoot:
    def __init__(self, workspace, root, clear):
        self.slots = s.Slots(workspace)
        self.root = s.plain(root)
        self.clear = clear
        if not self.root.name or self.root == self.root.parent:
            raise ValueError('Filesystem root forbidden')
        if self.root == self.slots.root or self.root in self.slots.root.parents or self.slots.root in self.root.parents:
            raise ValueError('Save root and workspace must be disjoint')
        self.guard()

    def guard(self):
        s.plain(self.root)
        if not self.clear(): raise ValueError('Game process present or process check failed')

    def snapshot(self, name):
        s.name_ok(name)
        with self.slots.lock():
            self.slots.idle(); self.guard()
            account = s.identity(self.root)
            target = self.slots.root/'slots'/name
            if target.exists(): raise ValueError('Slot exists; never overwrite')
            stage = self.slots.root/'slots'/('.stage-'+uuid.uuid4().hex)
            stage.mkdir()
            tree = s.copy_verified(self.root, stage/'data')
            self.guard()
            s.write(stage/'manifest.json', {'account':account,'tree':tree})
            stage.rename(target)
        return {'saved':name,'files':len(tree['files']),'real_root':True}

    def switch(self, name, fail_at=None):
        with self.slots.lock():
            self.slots.idle(); self.guard()
            slot = self.slots.verify(name)
            if s.identity(self.root) != slot['account']: raise ValueError('Cross-account switch forbidden')
            stamp = datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:16]
            tx = self.slots.root/'transactions'/('real-'+stamp); tx.mkdir()
            before = s.inventory(self.root)
            backup = s.copy_verified(self.root, tx/'backup')
            if backup != before: raise ValueError('Full-root backup mismatch')
            s.write(tx/'backup.json', {'verified':True,'tree':backup,'root':str(self.root)})
            if fail_at == 'backed-up': raise RuntimeError('Injected after backup')
            # Siblings ensure atomic same-volume renames. Never remove old trees.
            incoming = self.root.with_name(self.root.name+'.incoming-'+stamp)
            previous = self.root.with_name(self.root.name+'.backup-'+stamp)
            after = s.copy_verified(self.slots.root/'slots'/name/'data', incoming)
            if after != slot['tree']: raise ValueError('Incoming slot mismatch')
            journal = {'kind':'real-root-v1','root':str(self.root),'previous':str(previous),
                       'incoming':str(incoming),'transaction':tx.name,'before':before,'after':after}
            s.write(tx/'receipt.json', journal)
            s.write(self.slots.root/'pending.json', journal)
            self.guard()
            if s.inventory(self.root)!=before or s.inventory(tx/'backup')!=before:
                raise ValueError('Source/backup changed; switch refused')
            if previous.exists(): raise ValueError('Backup destination exists')
            self.root.rename(previous)
            if fail_at == 'detached': raise RuntimeError('Injected after detach')
            self.guard()
            incoming.rename(self.root)
            if fail_at == 'installed': raise RuntimeError('Injected after install')
            self.guard()
            if s.inventory(self.root)!=after: raise ValueError('Installed root mismatch; recovery required')
            s.write(tx/'complete.json', {'verified':True})
            (self.slots.root/'pending.json').rename(tx/'committed-journal.json')
        return {'switched':name,'backup':str(tx/'backup'),'previous':str(previous),'real_root':True}

    def recover(self):
        with self.slots.lock():
            self.guard(); pending=self.slots.root/'pending.json'; j=s.read(pending)
            if j.get('kind')!='real-root-v1' or j.get('root')!=str(self.root):
                raise ValueError('Recovery root mismatch')
            tx=self.slots.root/'transactions'/s.name_ok(j['transaction'])
            previous=s.plain(j['previous'])
            if previous.parent!=self.root.parent or not previous.name.startswith(self.root.name+'.backup-'):
                raise ValueError('Invalid previous-root path')
            if s.inventory(tx/'backup')!=j['before']: raise ValueError('Full backup changed')
            if previous.exists():
                if s.inventory(previous)!=j['before']: raise ValueError('Previous root changed')
                if self.root.exists():
                    self.root.rename(self.root.with_name(self.root.name+'.rejected-'+uuid.uuid4().hex))
                self.guard(); previous.rename(self.root)
            if s.inventory(self.root)!=j['before']: raise ValueError('Recovery mismatch')
            s.write(tx/'recovered.json', {'verified':True})
            pending.rename(tx/('recovered-journal-'+uuid.uuid4().hex+'.json'))
        return {'recovered':str(self.root)}
