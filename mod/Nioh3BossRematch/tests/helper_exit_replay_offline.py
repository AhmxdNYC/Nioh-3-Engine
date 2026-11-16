"""#108110: replay archived soul-core forms through the real SealEndOnHitTick with SealExitOnHelperMs off (-1) and on (150).

Forms are parsed from archived live logs (SealHitSource burst lines: press time, owner, new objects). The bound seal is actor 456,
every other source actor is helper 789. Expected: a core whose bursts all come from a helper (78A8D Kasha, no learned count) ends
at its first helper burst + 150 ms with the helper NOT ghosted and the hidden seal kept at form speed (no x2 tail, #108161); every other core (5129B helper burst 1 of 2, seal-only cores) ends
at the same time with the same ghost list as with the rule off. Usage: helper_exit_replay_offline.py [LOG ...]
"""
from pathlib import Path
import re, subprocess, sys
here = Path(__file__).resolve().parent
root = here.parent
DEFAULT_LOGS = [
    'D:/nioh3-re/temp-archive/dvhand87482/run1/Nioh3BossRematch.log',
    'D:/nioh3-re/temp-archive/dvhand87482/run2/Nioh3BossRematch.log',
    'D:/nioh3-re/temp-archive/hand94621/logs/Nioh3BossRematch.log',
    'D:/nioh3-re/temp-archive/hand94740/logs/Nioh3BossRematch.log',
    'D:/nioh3-re/temp-archive/live95418/Nioh3BossRematch.log',
    'C:/Users/Ahmad Hamza/nioh3-engine/Temp/live96829/r0-Nioh3BossRematch.log',
    'C:/Users/Ahmad Hamza/nioh3-engine/Temp/live96829/rA-Nioh3BossRematch.log',
]
BOUND = re.compile(r'SealEndOnHit seal=([0-9A-F]+) core ([0-9A-F]+) bound [\d.]+ s after the press, (\d+) bursts seen so far: (\d+) hit bursts known')
SRC = re.compile(r'SealHitSource core ([0-9A-F]+) burst (\d+) actor=([0-9A-F]+) new=(\d+)')
OPEN = re.compile(r'SealEndOnHit core ([0-9A-F]+) burst (\d+) opens .* ([\d.]+) s after the press')
IDLE = re.compile(r'SealIdleEnd seal=[0-9A-F]+ core ([0-9A-F]+): .* ([\d.]+) s after the press')


def forms(paths):
    out, skipped = [], 0
    for p in paths:
        p = Path(p)
        if not p.exists():
            print(f'missing log (skipped): {p}'); continue
        cur = None
        for line in p.read_text(encoding='utf-8', errors='replace').splitlines():
            m = BOUND.search(line)
            if m:
                if cur and cur['bursts']: out.append(cur)
                cur = None
                if m.group(3) != '0': skipped += 1; continue  # bursts before bind: core id unknown in their lines
                cur = dict(log=p.parent.name + '/' + p.name, seal=m.group(1), core=m.group(2), want=int(m.group(4)), bursts=[], pend={}, idle=None)
                continue
            if not cur: continue
            m = SRC.search(line)
            if m and m.group(1) == cur['core']:
                cur['pend'].setdefault(int(m.group(2)), []).append((m.group(3) == cur['seal'], int(m.group(4))))
                continue
            m = OPEN.search(line)
            if m and m.group(1) == cur['core']:
                n = int(m.group(2))
                if n == len(cur['bursts']) + 1: cur['bursts'].append((float(m.group(3)), cur['pend'].pop(n, [])))
                continue
            m = IDLE.search(line)
            if m and m.group(1) == cur['core'] and cur['idle'] is None: cur['idle'] = float(m.group(2))
        if cur and cur['bursts']: out.append(cur)
    return out, skipped


def scenario(i, f):
    # one C++ replay call per form: bursts as (t, seal?, count) triples, idle edge time or -1
    items = []
    for t, owners in f['bursts']:
        for is_seal, n in owners or [(True, 1)]:
            items.append(f'{{{t:.3f},{1 if is_seal else 0},{min(n, 40)}}}')
    return (f' {{std::vector<B> b={{{",".join(items)}}};R off=run(b,{f["want"]},{f["idle"] if f["idle"] is not None else -1},-1);'
            f'R on=run(b,{f["want"]},{f["idle"] if f["idle"] is not None else -1},150);'
            f'printf("FORM|{i}|{f["core"]}|{f["log"]}|%.3f|%d|%.3f|%d|%d|%d\\n",off.t,off.helperGhosted,on.t,on.helperGhosted,off.kept,on.kept);}}\n')


def main():
    found, skipped = forms(sys.argv[1:] or DEFAULT_LOGS)
    harness = (here / 'last_hit_exit_offline.py').read_text(encoding='utf-8')
    a = harness.index('int main(){'); b = harness.index("\n}\n'''", a)
    body = r'''struct B{double t;int seal;int n;};struct R{double t=-1;int helperGhosted=0;int kept=0;};
R run(const std::vector<B>& bursts,int want,double idle,int helperMs){
 g_exitOnHelperMs=helperMs;start(want);censusMode=true;g_sehKid[1]=789;g_sehKidT[1]=base;
 uint64_t next=0x100000;std::vector<std::pair<double,std::vector<uint64_t>>> live;R r;
 const double end=(idle>0?idle:bursts.back().t)+1.5;
 for(double t=0.016;t<=end&&r.t<0;t+=0.016){
  for(const auto& x:bursts) if(x.t>t-0.016&&x.t<=t){std::vector<uint64_t> ids;
   for(int k=0;k<x.n;++k){census[next]=x.seal?456:789;ids.push_back(next);next+=0x1000;}live.push_back({t,ids});}
  for(auto it=live.begin();it!=live.end();){if(t-it->first>=0.1){for(auto id:it->second)census.erase(id);it=live.erase(it);}else ++it;}
  if(idle>0&&t>=idle)g_sieFireSeal=456;
  ghostActors.clear();ghostKept=false;step(t,false);
  if(g_ghostReq==456){r.t=t;r.helperGhosted=std::count(ghostActors.begin(),ghostActors.end(),uint64_t(789))?1:0;r.kept=ghostKept?1:0;}
 }
 censusMode=false;census.clear();g_sehKid[1]=0;g_sehKidT[1]=0;g_exitOnHelperMs=-1;return r;
}
int main(){
 ini.clear();LoadExitSetting();check(g_exitOnHelperMs==150);
 ini[L"SealExitOnHelperMs"]=L"-1";LoadExitSetting();check(g_exitOnHelperMs==-1);
 ini[L"SealExitOnHelperMs"]=L"9000";LoadExitSetting();check(g_exitOnHelperMs==5000);ini.clear();
'''
    unit = harness[:a] + body + ''.join(scenario(i, f) for i, f in enumerate(found)) + harness[b:]
    unit = unit.replace("build/last-hit-test", "build/helper-exit-test").replace("last-hit.cpp", "helper-exit.cpp").replace("last-hit.obj", "helper-exit.obj").replace("last-hit.exe", "helper-exit.exe")
    unit = unit.replace('printf("last-hit tick, ghost safety, settings and menu: %d checks PASS (offline)\\n",checks);', '')
    unit = unit.replace("subprocess.run([str(exe)],check=True,timeout=30)", "res=subprocess.run([str(exe)],check=True,timeout=120,capture_output=True,text=True)")
    g = {'__file__': str(here / 'last_hit_exit_offline.py'), '__name__': 'helper_exit'}
    exec(compile(unit, 'helper_exit_unit', 'exec'), g)
    rows = [l.split('|') for l in g['res'].stdout.splitlines() if l.startswith('FORM|')]
    fails, per = 0, {}
    for _, i, core, log, toff, goff, ton, gon, koff, kon in rows:
        toff, ton, goff, gon = float(toff), float(ton), int(goff), int(gon)
        f = found[int(i)]
        helper_only = all(not s for _, owners in f['bursts'] for s, _ in owners)
        if helper_only and f['want'] == 0:
            first = f['bursts'][0][0]
            ok = ton > 0 and abs(ton - (first + 0.150)) <= 0.035 and gon == 0 and (toff < 0 or toff > ton + 1.0) and kon == '1' and koff == '0'
            kind = 'ends early'
        else:
            ok = abs(ton - toff) < 1e-6 and gon == goff and kon == koff == '0'
            kind = 'unchanged'
        per.setdefault((core, kind), [0, 0])[0 if ok else 1] += 1
        if not ok:
            fails += 1; print(f'FAIL form {i} core {core} {log}: off {toff:.3f}s ghost-helper={goff}, on {ton:.3f}s ghost-helper={gon}')
    for (core, kind), (p, q) in sorted(per.items()):
        print(f'core {core}: {kind}  {p} pass {q} fail')
    print(f'{len(rows)} archived forms replayed ({skipped} late-bind forms skipped): {"PASS" if not fails and rows else "FAIL"}')
    sys.exit(1 if fails or not rows else 0)


if __name__ == '__main__':
    main()
