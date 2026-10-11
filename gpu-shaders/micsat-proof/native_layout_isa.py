"""Compile against native-derived default compute layout, explicit root version1.0."""
import json,pathlib,subprocess,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parent;OUT=HERE/'out'
sys.path[:0]=[str(ROOT/'materials'),str(ROOT/'gi')]
import dxcom,rga_stats
ROOT_HLSL='#define RS "RootFlags(0), DescriptorTable(CBV(b0, numDescriptors=14)), DescriptorTable(SRV(t0, numDescriptors=48)), DescriptorTable(UAV(u0, numDescriptors=16)), DescriptorTable(Sampler(s0, numDescriptors=16))"\n'
def main():
    work=OUT/'native-layout';work.mkdir(exist_ok=True)
    hlsl=work/'root.hlsl';hlsl.write_text(ROOT_HLSL)
    blob=work/'root.bin'
    subprocess.run([dxcom.DXC,'-T','rootsig_1_0','-E','RS',str(hlsl),'-Fo',str(blob)],check=True,capture_output=True)
    report={}
    for stem in ('original','current'):
        target=work/stem;target.mkdir(exist_ok=True)
        run=subprocess.run([rga_stats.RGA,'-s','dx12','--offline','-c','gfx1103','--cs-blob',str(OUT/(stem+'.dxbc')),
                            '--rs-bin',str(blob),'--isa',str(target/'isa.txt')],capture_output=True,text=True)
        matches=list(target.glob('*comp*'))
        if not matches:raise RuntimeError(run.stdout[-1500:]+run.stderr[-500:])
        isa=matches[0].read_text();report[stem]=rga_stats.analyse(isa)
        print(stem,json.dumps(report[stem]),flush=True)
    (OUT/'native-layout-metrics.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':main()
