"""Real temporary-file failure checks, never an actual game/save path."""
import argparse
from pathlib import Path
import tempfile
import live_switch as L


def main():
    p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);a=p.parse_args()
    checks=0
    with tempfile.TemporaryDirectory(prefix='pb2-switch-test-') as temp:
        base=Path(temp)
        def fixture(name):
            game=base/name/'game';(game/'shader_dx12').mkdir(parents=True)
            target=game/L.RELATIVE;target.write_bytes((a.package/'original.file').read_bytes())
            return game,target,base/name/'run'
        def refuses(fn):
            try:fn()
            except (RuntimeError,FileExistsError):return
            raise AssertionError('failure accepted')
        game,target,run=fixture('normal');r=L.activate(game,a.package,run,lambda:False)
        assert L.sha(target)==L.CANDIDATE;L.restore(r,lambda:False);assert L.sha(target)==L.ORIGINAL;checks+=1
        game,target,run=fixture('running');refuses(lambda:L.activate(game,a.package,run,lambda:True));assert L.sha(target)==L.ORIGINAL;checks+=1
        game,target,run=fixture('changed');r=L.activate(game,a.package,run,lambda:False);target.write_bytes(b'newer external contents')
        refuses(lambda:L.restore(r,lambda:False));assert target.read_bytes()==b'newer external contents' and (game/'.pb2pixel-test.lock').exists();checks+=1
        game,target,run=fixture('midcopy');old=L.copy_new
        def changed_copy(src,dst):
            old(src,dst)
            if Path(dst).name.startswith('.pb2pixel-'):target.write_bytes(b'external during staging')
        L.copy_new=changed_copy
        try:refuses(lambda:L.activate(game,a.package,run,lambda:False))
        finally:L.copy_new=old
        assert target.read_bytes()==b'external during staging' and (game/'.pb2pixel-test.lock').exists();checks+=1
        game,target,run=fixture('start');calls=[0]
        def starts():calls[0]+=1;return calls[0]>=3
        refuses(lambda:L.activate(game,a.package,run,starts));assert L.sha(target)==L.ORIGINAL;checks+=1
        game,target,run=fixture('journal');r=L.activate(game,a.package,run,lambda:False);old=L.write_json
        def fail(*args):raise RuntimeError('injected durable receipt failure')
        L.write_json=fail
        try:refuses(lambda:L.restore(r,lambda:False))
        finally:L.write_json=old
        assert L.sha(target)==L.ORIGINAL and (game/'.pb2pixel-test.lock').exists()
        L.restore(r,lambda:False);assert not (game/'.pb2pixel-test.lock').exists();checks+=1
        game,target,run=fixture('duplicate');r=L.activate(game,a.package,run,lambda:False)
        refuses(lambda:L.activate(game,a.package,run.parent/'other',lambda:False));L.restore(r,lambda:False);checks+=1
    print(f'PASS {checks} temporary-file activation/restoration/failure checks; no actual game/save paths mutated')


if __name__=='__main__':main()
