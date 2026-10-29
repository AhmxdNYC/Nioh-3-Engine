"""Package the combined offline candidate, then verify every archived checksum."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--loader', type=Path, required=True)
    args = ap.parse_args()
    files = {}
    for module in ('Nioh3EngineFix', 'Nioh3BossRematch', 'Nioh3Training'):
        base = ROOT/'mod'/module
        files[module+'.asi'] = base/('' if module=='Nioh3Training' else 'bin')/(module+'.asi')
        files[module+'.ini'] = base/(module+'.ini')
        for p in base.glob('*.tsv'):
            files[p.name] = p
    files.update({
        'dinput8.dll': args.loader,
        'Nioh3Updater.exe': ROOT/'updater-dist/Nioh3Updater.exe',
        'SaveSlots-after-exit.pyz': ROOT/'tools/saves/dist/SaveSlots-after-exit.pyz',
        'SAVE-MANAGER.md': ROOT/'tools/saves/README-41493.md',
        'FINAL-BUILD.md': ROOT/'notes/final-build.md',
        'diagnostics/Nioh3CamKiProbe.asi': ROOT/'mod/Nioh3CamKiProbe/bin/Nioh3CamKiProbe.asi',
        'update-linux.sh': ROOT/'tools/updater/update-linux.sh',
    })
    for name in ('Nioh3 Mod Website.url','install_mod.py','Install-Mod.cmd','install-mod.sh','USER_GUIDE.md',
                 'Restore Vanilla Game.cmd','Put the mod back.cmd','restore-vanilla.sh','put-mod-back.sh'):
        files[name] = ROOT/'release'/name
    ini = configparser.ConfigParser(strict=False)
    ini.read(files['Nioh3EngineFix.ini'])
    assert all(ini.getint('SaveFiles',k)==0 for k in ('Enabled','RealRoot','Probe'))
    assert all(not ini.get('SaveFiles',k).strip() for k in ('CopySandbox','Helper','Python','SaveRoot'))
    records = files['Nioh3Training_action_records.tsv'].read_bytes()
    assert records == (ROOT/'data/boss-action-record.tsv').read_bytes()
    assert len(records.splitlines()) == 11477
    version = re.search(rb'Nioh3EngineFix (\d+\.\d+\.\d+) ',files['Nioh3EngineFix.asi'].read_bytes()).group(1).decode()
    sums = {name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in files.items()}
    out = ROOT/'release/dist'
    out.mkdir(exist_ok=True)
    target = out/f'Nioh3-{version}-combined-rc.zip'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
        for name,path in files.items():
            z.write(path,name)
        z.writestr('SHA256SUMS.txt',''.join(f'{sha}  {name}\n' for name,sha in sorted(sums.items())))
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        for name,sha in sums.items():
            assert hashlib.sha256(z.read(name)).hexdigest()==sha
    (out/'final-hashes.json').write_text(json.dumps(sums,indent=2)+'\n')
    print(f'PASS package: {len(files)} payload files, action records present, save gates OFF, all hashes verified')
    print(target)
    print('zip SHA256',hashlib.sha256(target.read_bytes()).hexdigest())

if __name__=='__main__':
    main()
