"""Build helper pyz and an offline acceptance bundle; does not access any save root."""
from pathlib import Path
import zipfile

root=Path(__file__).resolve().parent
dist=root/'dist';dist.mkdir(exist_ok=True)
helper=dist/'SaveSlots-after-exit.pyz'
with zipfile.ZipFile(helper,'w',zipfile.ZIP_DEFLATED) as z:
    for name in ('saveslot.py','after_exit.py','real_root.py'):z.write(root/name,name)
    z.writestr('__main__.py','from after_exit import main\nraise SystemExit(main())\n')
with zipfile.ZipFile(dist/'SaveGate-41985.zip','w',zipfile.ZIP_DEFLATED) as z:
    z.write(helper,helper.name)
    for name in ('saveslot.py','after_exit.py','real_root.py','prepare_test.py','README-41493.md','README-41985.md'):
        z.write(root/name,name)
    z.writestr('sandbox-test.ini','[SaveFiles]\nEnabled=1\nRealRoot=0\nProbe=1\nCopySandbox=\nHelper=\nPython=\nSaveRoot=\n')
    z.writestr('copied-root-test.ini','[SaveFiles]\nEnabled=1\nRealRoot=1\nProbe=1\nCopySandbox=\nHelper=\nPython=\n; Set to a disposable copied root only for acceptance.\nSaveRoot=\n')
print(helper)
print(dist/'SaveGate-41985.zip')
