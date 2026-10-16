"""Read a scene-only GP2 log slice; identify submitted draws, never pixel execution."""
import argparse
import json
from pathlib import Path
import re

DIGEST='e9e2d0b05b89a2d262f0e59bf39f1841'
ROW=re.compile(r'GP2:\s+\d+\s+([^|]+)\|\s*([\d.]+) ms\s*\|\s*([\d.]+)\s*\|\s*([\d.]+)\s*\|\s*\[([^]]*)\]')


def inspect(text):
    rows=[]
    for line in text.splitlines():
        m=ROW.search(line)
        if m and re.search(r'\bps='+DIGEST+r'\b',m[1]):
            rows.append(dict(line=line,ms_per_sampled_frame=float(m[2]),draw_calls_per_sampled_frame=float(m[3]),intervals_per_sampled_frame=float(m[4]),marker=m[5]))
    positive=any(r['draw_calls_per_sampled_frame']>0 and r['intervals_per_sampled_frame']>0 for r in rows)
    return dict(verdict='CANDIDATE_DRAW_INTERVAL_OBSERVED' if positive else 'EXERCISE_UNCONFIRMED',
        shader_digest=DIGEST,rows=rows,pixel_invocations_proven=False,visual_equivalence_proven=False,
        conditions='Scene-only current-boot slice; GPUProf2_MinRun=1; ShaderSwap/ShaderHot/SkipFx disabled; original INI and ASI identity retained separately.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path,nargs='?');p.add_argument('--self-test',action='store_true');a=p.parse_args()
    if a.self_test:
        row='GP2:   1 vs=1559714ca496fffe6ddd3c827e1d962e ps='+DIGEST+' | 0.123 ms | 2.0 | 1.0 | [GBuffer]'
        assert inspect(row)['verdict']=='CANDIDATE_DRAW_INTERVAL_OBSERVED'
        for bad in (row.replace(DIGEST,'7dcfe3de3f3fcbc199be405eae1596b4'),row.replace('2.0','0.0'),row.replace('1.0','0.0'), 'PSOSWAP: ps='+DIGEST, ''):
            assert inspect(bad)['verdict']=='EXERCISE_UNCONFIRMED'
        print('PASS 6 log admission cases; no pixel-execution claim');return
    p.error('log path required') if a.log is None else None
    print(json.dumps(inspect(a.log.read_text(encoding='utf-8',errors='replace')),indent=2))


if __name__=='__main__':main()
