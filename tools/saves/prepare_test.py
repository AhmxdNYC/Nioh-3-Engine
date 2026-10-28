"""Make synthetic saves and an Enabled=1/RealRoot=0 INI fragment; never installs anything."""
import argparse
from pathlib import Path
import sys
import saveslot as s

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sandbox',required=True);p.add_argument('--ini',required=True)
    p.add_argument('--helper',required=True);a=p.parse_args()
    root=Path(a.sandbox).absolute();s.init(root)
    account=root/'current/Savedata/76561198000000000'
    for rel in ('SAVEDATA00/SAVEDATA.BIN','SAVEDATA00/BACKUP.BIN','SYSTEMSAVEDATA00/SAVEDATA.BIN'):
        f=account/rel;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'A'*4096)
    s.Slots(root).snapshot('slot1')
    s.Slots(root).snapshot('Yoritomo Practice')
    for f in account.rglob('*.BIN'):f.write_bytes(b'B'*4096)
    s.Slots(root).snapshot('New Game')
    for f in account.rglob('*.BIN'):f.write_bytes(b'C'*4096)
    # Exclusive output prevents overwriting an installed INI by mistake.
    with Path(a.ini).open('x',encoding='utf-8') as f:
        f.write('[SaveFiles]\nEnabled=1\nRealRoot=0\nProbe=1\n'
                f'CopySandbox={root}\nHelper={Path(a.helper).absolute()}\nPython={sys.executable}\nSaveRoot=\n')
    print('Created 3 named synthetic slots and current=C with test INI fragment; nothing installed.')

if __name__=='__main__':main()
