"""Audit retained Ally archive receipts and rounded launch statistics.
Does not reconstruct missing exact measurement slices or prove native equivalence.
"""
from pathlib import Path
import hashlib,json,re,statistics,zipfile
root=Path(__file__).resolve().parents[2]
p=root/'scratch/ally-fog-native-236.zip'
assert hashlib.sha256(p.read_bytes()).hexdigest()=='d0b541c858490064d78c01d5c5cbe38221542a4a6fcf2cb0850f4f752ce706c0'
with zipfile.ZipFile(p) as z:
    assert z.testzip() is None
    read=lambda n:z.read('up/'+n).decode()
    def hashes(t): return dict((line.split(maxsplit=1)[1],line.split()[0]) for line in t.splitlines() if line.strip())
    entry=hashes(read('hash_before.txt'));saves=hashes(read('saves_before.txt'))
    means={};p99={};missing=[]
    for arm in ('1A','2B','3B','4A','5A','6B'):
        prefix='run/'+arm+'/'
        assert read(prefix+'restore.txt').strip()=='restore_ok=1'
        assert 'exit_pid=[]' in read(prefix+'windows.txt')
        assert hashes(read(prefix+'saves_after_restore.txt'))==saves
        # Local/time metadata may change; only claimed remote-time/content fields.
        cloud_fields=lambda t:re.findall(r'"(?:sha|remotetime)"\s+"[^"]*"',t)
        assert cloud_fields(read(prefix+'remotecache_after_exit.vdf'))==cloud_fields(read('remotecache_entry.vdf'))
        current=hashes(read(prefix+'shader_hash.txt'))
        for path,digest in current.items():
            if path.endswith('0xa9910db8.file'):
                expected='4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993' if arm.endswith('A') else '81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e'
            elif path.endswith('Nioh3EngineFix.asi'):
                expected=next(v for k,v in entry.items() if k.endswith('Nioh3EngineFix.asi'))
            else: expected=entry[path]
            assert digest==expected,(arm,path)
        rows=re.findall(r'w[12] n (\d+) fps ([\d.]+) ft_p99 ([\d.]+)',read(prefix+'summary.txt'))
        assert len(rows)==2 and all(int(n)==200 for n,_,_ in rows)
        means[arm]=statistics.mean(float(f) for _,f,_ in rows)
        p99[arm]=statistics.mean(float(t) for _,_,t in rows)
        missing += [prefix+f'w{i}.csv' for i in (1,2) if 'up/'+prefix+f'w{i}.csv' not in z.namelist()]
    deltas=[100*(means[b]/means[a]-1) for a,b in [('1A','2B'),('4A','3B'),('5A','6B')]]
    print(json.dumps(dict(receipts='PASS',rounded_launch_means=means,paired_percent=deltas,
        mean_percent=statistics.mean(deltas),se_percent=statistics.stdev(deltas)/(3**.5),
        missing_exact_slices=len(missing),scope='Rounded-summary reproduction; sampled-window p99, not pooled per-frame; exact CSV slices needed'),indent=2))
p=root/'scratch/pc2-recovery-235.zip'
assert hashlib.sha256(p.read_bytes()).hexdigest()=='69f10a7455f0ef964fb0d79c754b700a339ff128a624cd16c3629d47700e1bdd'
with zipfile.ZipFile(p) as z:
    assert z.testzip() is None
    d=json.loads(z.read('pc2-recovery-codex-20261011/current-state.json').decode('utf-8-sig'))
    assert d['original_pid_currently_absent'] and not d['game_capture_processes']
    assert all(r['entry_sha256']==r['backup_sha256']==r['current_sha256'] for r in d['entry_files'])
    assert all(r['backup_sha256']==r['current_sha256'] for r in d['savedata'])
    assert all(r['archived_installed_sha256']==r['current_sha256'] for r in d['loader_shaders'])
    print('pc2 current-state receipt consistency PASS at',d['observed_utc'],'; historical recovery time / old independent clock anchor unresolved')
