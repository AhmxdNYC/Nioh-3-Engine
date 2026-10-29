"""Package the live-title helper; no save/game access."""
from pathlib import Path
import zipfile

root=Path(__file__).resolve().parent
dist=root/'dist';dist.mkdir(exist_ok=True)
out=dist/'SaveSlots-live-title.pyz'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for name in ('saveslot.py','after_exit.py','title_gate.py','live_title.py','bootstrap.py'):
        z.write(root/name,name)
    z.write(root.parents[1]/'re/scripts_re/savecrypt.py','savecrypt.py')
    z.writestr('__main__.py','import sys\nif \"--bootstrap\" in sys.argv:\n    from bootstrap import main\nelse:\n    from live_title import main\nraise SystemExit(main())\n')
print(out)
