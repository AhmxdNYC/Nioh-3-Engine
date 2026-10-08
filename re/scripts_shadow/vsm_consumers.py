"""vsm_consumers.py -- dumpbin every ps/cs in shader groups named *Vsm* that binds sVirtualShadow and check it loads
tVirtualShadowMapFlags and tests bit 0 of a loaded flag (tile valid). Result 2026-10-08: 356/356 gated."""
import sys,os,glob,subprocess,tempfile,re,collections
sys.path.insert(0,r'C:\Users\Ahmad Hamza\nioh3-engine\gpu-shaders')
import idrk
DXC=r'C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806\utils\dx12\dxc\dxc.exe'
sd=r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12'
import tempfile
tmp=os.path.join(tempfile.gettempdir(),'vsm_consumers_tmp.dxbc')
res=collections.Counter();bad=[]
for fn in glob.glob(sd+r'\*.file'):
    try: p=idrk.unpack(open(fn,'rb').read())
    except Exception: continue
    g=idrk.group_name(p)
    if 'Vsm' not in g: continue
    for i,(off,size,dig,hdig,kind,parts) in enumerate(idrk.iter_dxbc(p)):
        if kind!='ps' and kind!='cs': continue
        open(tmp,'wb').write(p[off:off+size])
        t=subprocess.run([DXC,'-dumpbin',tmp],capture_output=True,text=True,errors='replace').stdout
        if ' sVirtualShadow ' not in t: continue
        hasF='tVirtualShadowMapFlags' in t
        # find flags handle id and check an 'and i32 ..., 1' after a bufferLoad
        chk=bool(re.search(r'bufferLoad\.i32\(i32 68, %dx\.types\.Handle %\d+, i32 %\d+, i32 undef\)[^\n]*\n\s*%(\d+) = extractvalue[^\n]*\n\s*%\d+ = and i32 %\1, 1\b',t))
        res[(g,hasF,chk)]+=1
        if not (hasF and chk): bad.append((g,i,kind,hasF,chk))
for k,v in sorted(res.items()): print(v,k)
print('not flag-gated:',bad[:40])
