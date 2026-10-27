import sys,json,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent/'tools/updater'))
import updater as u
stage=Path(__file__).parent/'staged-site'
m=u.validate(json.loads((stage/'updates/stable.json').read_text()))
assert m['version']=='1.7.85'
payloads={f['url']:(stage/'releases/1.7.85'/f['name']).read_bytes() for f in m['files']}
for f in m['files']: assert u.digest(payloads[f['url']])==f['sha256']
u.fetch=lambda url,limit=u.MAX_FILE: (stage/'updates/stable.json').read_bytes() if url==u.MANIFEST_URL else payloads[url]
with tempfile.TemporaryDirectory(prefix='nioh3-hourly-copy-') as td:
    root=Path(td)
    old={f['name']:b'old fixture '+f['name'].encode() for f in m['files']}
    for name,data in old.items(): (root/name).write_bytes(data)
    ini=root/'Nioh3EngineFix.ini'; ini.write_bytes(b'[Player]\r\nKeep=1\r\n')
    u.game_running=lambda:False
    result=u.update(root)
    assert result['changed']==3
    for f in m['files']: assert u.digest((root/f['name']).read_bytes())==f['sha256']
    assert ini.read_bytes()==b'[Player]\r\nKeep=1\r\n'
    assert u.update(root)['changed']==0
    u.rollback(root,result['backup'])
    for name,data in old.items(): assert (root/name).read_bytes()==data
    assert ini.read_bytes()==b'[Player]\r\nKeep=1\r\n'
    first=m['files'][0]; payloads[first['url']]=b'corrupted'
    try: u.update(root)
    except ValueError as e: assert 'SHA256 mismatch' in str(e)
    else: raise AssertionError('corruption accepted')
    for name,data in old.items(): assert (root/name).read_bytes()==data
print('PASS offline staged payloads: 3 installs/hash verification, settings preservation, no-op repeat, rollback, corrupt download rejection; no network or real game writes.')
