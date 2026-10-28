"""Import one validated user save at idle title, then choose it through native Load.

Never overwrite a game slot, replace system data, call game functions or quit.
Atomic publication into a free SAVEDATAnn is the mutation boundary. A final
gate sample is not a game-thread lock; no existing profile is removed even if
the title changes immediately afterwards. Native acceptance remains required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import uuid
import saveslot as s


def validate(blob, account):
    import savecrypt
    if len(blob) != 0x900058+0x158: raise ValueError('Unexpected user-save file size')
    header = savecrypt.xor(blob[:0x158], savecrypt.hdr_stream())
    if header[:6] != b'RNNUSR': raise ValueError('Not a user save')
    if struct.unpack_from('<Q', header, 0x10)[0] != int(account): raise ValueError('Header Steam account mismatch')
    if struct.unpack_from('<I', header, 0x1C)[0] != 0x900058: raise ValueError('Invalid body size')
    plain = savecrypt.decrypt(blob)
    if struct.unpack_from('<I', plain, 0x158)[0] != 0x25111400:  # body version at plain+0x158 (real saves; +0x15C is the game build)
        raise ValueError('Unsupported user-save version')
    if savecrypt.checksum(plain) != struct.unpack_from('<I', plain, 0x900194)[0]:
        raise ValueError('User-save checksum mismatch')
    return {'savedAt': struct.unpack_from('<Q', header, 0x20)[0], 'sha256': hashlib.sha256(blob).hexdigest()}


def restamp(blob, floor):
    """Return blob with header savedAt (+0x20) strictly above floor so native newest-slot load picks it."""
    import savecrypt
    header = bytearray(savecrypt.xor(blob[:0x158], savecrypt.hdr_stream()))
    if struct.unpack_from('<Q', header, 0x20)[0] > floor: return blob
    struct.pack_into('<Q', header, 0x20, floor + 1)
    return savecrypt.xor(bytes(header), savecrypt.hdr_stream()) + blob[0x158:]


def newest_stamp(account_dir):
    import savecrypt
    best = 0
    for i in range(50):
        f = account_dir/f'SAVEDATA{i:02d}'/'SAVEDATA.BIN'
        if f.is_file():
            h = savecrypt.xor(f.read_bytes()[:0x158], savecrypt.hdr_stream())
            if h[:6] == b'RNNUSR': best = max(best, struct.unpack_from('<Q', h, 0x20)[0])
    return best


def publish(stage, target):
    # Windows MoveFileW has no replace flag. POSIX rename can replace an empty
    # directory, so the production publication is deliberately Windows-only.
    if os.name != 'nt': raise ValueError('Windows publication only; Proton acceptance pending')
    import ctypes
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.MoveFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
    k.MoveFileW.restype = ctypes.c_int
    if not k.MoveFileW(str(stage), str(target)): raise ctypes.WinError(ctypes.get_last_error())


def run(workspace, root, slot, token, gate, *, source_slot=None, validator=validate, publisher=publish, make_newest=False):
    manager = s.Slots(workspace); s.name_ok(slot); s.name_ok(token)
    root = s.plain(root)
    if root == root.parent or root == manager.root or root in manager.root.parents or manager.root in root.parents:
        raise ValueError('Save root and library must be disjoint, non-root directories')
    # Nonce is consumed even on failure. The receipt says imported, never loaded.
    request = manager.root/('request-'+token); request.mkdir()
    receipt = {'state':'preparing', 'slot':slot, 'loaded':False}
    def record(): s.write(request/'receipt.json', receipt)
    def guard():
        if (manager.root/('cancel-'+token)).exists(): raise ValueError('Cancelled')
        s.plain(root); gate.check()
    record()
    try:
        with manager.lock():
            manager.idle(); gate.settle(); guard()
            manifest = manager.verify(slot); account = s.identity(root)
            if manifest['account'] != account: raise ValueError('Cross-account import refused')
            source = manager.root/'slots'/slot/'data'/'Savedata'/account
            candidates = sorted(p.name for p in source.iterdir() if re.fullmatch(r'SAVEDATA(?:[0-4][0-9])', p.name) and p.is_dir())
            if source_slot is None:
                if len(candidates) != 1: raise ValueError('Select explicit source slot for a multi-profile snapshot')
                source_slot = candidates[0]
            if source_slot not in candidates: raise ValueError('Invalid source SAVEDATAnn')
            blob = (source/source_slot/'SAVEDATA.BIN').read_bytes()
            metadata = validator(blob, account); guard()
            if make_newest:
                blob = restamp(blob, newest_stamp(root/'Savedata'/account)); metadata = dict(metadata, restamped=True)
            if manager.verify(slot) != manifest: raise ValueError('Source snapshot changed')
            before = s.inventory(root)
            tx = manager.root/'transactions'/('live-'+uuid.uuid4().hex); tx.mkdir()
            backup = s.copy_verified(root, tx/'backup')
            if backup != before: raise ValueError('Root changed during mandatory backup')
            s.write(tx/'backup.json', {'root':str(root), 'tree':backup, 'verified':True})
            guard()
            account_dir = root/'Savedata'/account
            target = next((account_dir/f'SAVEDATA{i:02d}' for i in range(50)
                           if not (account_dir/f'SAVEDATA{i:02d}').exists()), None)
            if target is None: raise ValueError('All 50 game slots occupied; nothing overwritten')
            # Stage beside root, outside the game enumerator, on the same volume.
            stage = root.with_name(root.name+'.live-incoming-'+uuid.uuid4().hex); stage.mkdir()
            for filename in ('SAVEDATA.BIN', 'BACKUP.BIN'):
                with (stage/filename).open('xb') as f: f.write(blob); f.flush(); os.fsync(f.fileno())
            expected = s.inventory(stage)
            if any(v['sha256'] != hashlib.sha256(blob).hexdigest() for v in expected['files'].values()):
                raise ValueError('Staged profile verification failed')
            receipt.update(state='prepared', target=str(target), backup=str(tx/'backup'), stage=str(stage), metadata=metadata)
            record(); s.write(tx/'receipt.json', receipt)
            # Long checks precede the fresh final sample; target race handled by
            # no-replace OS publication, never an overwrite or two-file swap.
            if s.inventory(root) != before or s.inventory(tx/'backup') != before:
                raise ValueError('Save root or backup changed before import')
            if s.inventory(stage) != expected: raise ValueError('Stage changed before import')
            guard(); s.plain(target); publisher(stage, target)
            receipt.update(state='imported', instruction='Choose Load Game and select '+target.name+'; Continue selects newest timestamp.')
            try: guard(); receipt['gate_after']='idle'
            except Exception as e: receipt['gate_after']='changed: '+str(e)
            # Never remove a published slot after gate loss: the native loader
            # may already be reading it. Existing profiles remain untouched.
            record(); s.write(tx/'receipt.json', receipt)
            return receipt
    except Exception as e:
        if receipt['state'] != 'imported': receipt.update(state='refused', error=str(e))
        else: receipt['receipt_error'] = str(e)
        record(); raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('sandbox', 'save-root', 'slot', 'token'): p.add_argument('--'+key, required=True)
    for key in ('pid', 'created'): p.add_argument('--'+key, type=int, required=True)
    p.add_argument('--source-slot'); p.add_argument('--make-newest', action='store_true'); a = p.parse_args()
    from title_gate import Gate
    gate = None
    try:
        gate = Gate(a.pid, a.created)
        print(json.dumps(run(a.sandbox, a.save_root, a.slot, a.token, gate, source_slot=a.source_slot, make_newest=a.make_newest))); return 0
    except Exception as e: print(json.dumps({'error':str(e)})); return 1
    finally:
        if gate: gate.close()


if __name__ == '__main__': raise SystemExit(main())
