"""Offline original/candidate codegen, all output and scratch confined here."""
import json,pathlib,sys,tempfile
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
OUT=HERE/'out'
tempfile.tempdir=str(OUT)
sys.path.insert(0,str(HERE.parent/'meshvs'))
import rga_vs

results={}
for stem in ('original','guarded'):
    isa=rga_vs.compile_vs(str(OUT/(stem+'.dxbc')),'gfx1103')
    (OUT/(stem+'.gfx1103.isa.txt')).write_text(isa)
    results[stem]=rga_vs.summary(isa)
    print(stem,json.dumps(results[stem]),flush=True)
(OUT/'isa-summary.json').write_text(json.dumps(results,indent=2))
