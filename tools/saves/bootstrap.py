"""Create a verified, separate save library; never write to the source save root."""
import argparse
import json
from pathlib import Path
import re
import shutil
import uuid

import saveslot as s
from live_title import validate, publish


def existing_library(root):
    manager = s.Slots(root)
    manager.idle()
    if not (root/'current').is_dir() or not (root/'slots').is_dir() or not (root/'transactions').is_dir():
        raise ValueError('Incomplete library')
    names = []
    for slot in sorted((root/'slots').iterdir()):
        if slot.name.startswith('.'):
            raise ValueError('Unfinished library snapshot')
        if not slot.is_dir():
            raise ValueError('Unexpected library entry')
        manager.verify(slot.name)
        names.append(slot.name)
    return {'state':'existing', 'slots':names, 'changed':False}


def run(workspace, source, *, validator=validate, publisher=publish):
    root = s.safe_root(workspace)
    source = s.plain(source)
    if root == root.parent or root == source or root in source.parents or source in root.parents:
        raise ValueError('Library and source must be separate non-root directories')
    if root.exists():
        return existing_library(root)
    before = s.inventory(source)
    accounts = source/'Savedata'
    ids = [p.name for p in accounts.iterdir() if p.is_dir()] if accounts.is_dir() else []
    if len(ids) != 1 or not re.fullmatch(r'\d{17}', ids[0]):
        raise ValueError('Require exactly one Steam account')
    account = ids[0]
    profiles = sorted(p.name for p in (accounts/account).iterdir()
                      if p.is_dir() and re.fullmatch(r'SAVEDATA\d{2}', p.name))
    if not profiles:
        raise ValueError('No user saves to copy')
    system = accounts/account/'SYSTEMSAVEDATA00'
    if not (system/'SAVEDATA.BIN').is_file():
        raise ValueError('Missing system save')
    root.parent.mkdir(parents=True, exist_ok=True)
    s.plain(root.parent)
    stage = root.with_name(root.name+'.bootstrap-'+uuid.uuid4().hex)
    s.init(stage)
    backup = stage/'backups'; backup.mkdir()
    copied = s.copy_verified(source, backup/'initial')
    if copied != before:
        raise ValueError('Source changed before backup')
    snapshot_root = backup/'initial'/'Savedata'/account
    metadata = {}
    for profile in profiles:
        blob = (snapshot_root/profile/'SAVEDATA.BIN').read_bytes()
        metadata[profile] = validator(blob, account)
    names = []
    for profile in profiles:
        name = 'My current save' if len(profiles) == 1 else 'My save '+profile[-2:]
        slot = stage/'slots'/name; slot.mkdir()
        data = slot/'data'; destination = data/'Savedata'/account
        destination.mkdir(parents=True)
        # Normalize the selected profile to slot 00 so each snapshot is unambiguous.
        (destination/'SAVEDATA00').mkdir()
        blob = (snapshot_root/profile/'SAVEDATA.BIN').read_bytes()
        for filename in ('SAVEDATA.BIN','BACKUP.BIN'):
            (destination/'SAVEDATA00'/filename).write_bytes(blob)
        s.copy_verified(snapshot_root/'SYSTEMSAVEDATA00', destination/'SYSTEMSAVEDATA00')
        manifest = {'account':account, 'tree':s.inventory(data)}
        s.write(slot/'manifest.json', manifest)
        s.Slots(stage).verify(name)
        names.append(name)
    newest = max(profiles, key=lambda p: metadata[p]['savedAt'])
    selected_name = 'My current save' if len(profiles) == 1 else 'My save '+newest[-2:]
    # current is a separate normalized copy; the full original tree is retained in backups.
    shutil.copytree(stage/'slots'/selected_name/'data', stage/'current', dirs_exist_ok=True)
    s.identity(stage/'current')
    s.write(stage/'bootstrap.json', {'source':str(source), 'backup':'backups/initial',
                                   'tree':before, 'profiles':metadata, 'slots':names})
    if s.inventory(source) != before or s.inventory(backup/'initial') != before:
        raise ValueError('Source or backup changed; refusing publication')
    if root.exists():
        raise ValueError('Library appeared during preparation; nothing overwritten')
    publisher(stage, root)
    return {'state':'created', 'slots':names, 'backup':str(root/'backups/initial'),
            'source_files':len(before['files']), 'changed':True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap', action='store_true')
    parser.add_argument('--sandbox', required=True)
    parser.add_argument('--save-root', required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(run(args.sandbox, args.save_root)))
        return 0
    except Exception as error:
        print(json.dumps({'state':'refused', 'error':str(error)}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
