r"""mkvs.py - synthesize a pass-through vertex shader whose outputs match a pixel shader's input signature, for RGA's
DX12 offline pipeline (the game's meshlet VSs have no input layout and crash RGA).  py -I mkvs.py PS.ll out_vs.dxbc"""
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import dxcom  # noqa: E402
ll = open(sys.argv[1], encoding='utf-8').read()
sec = ll.split('; Input signature:', 1)[1].split('; Output signature:', 1)[0]
fields, body = [], []
for l in sec.splitlines():
    m = re.match(r'^; (\w+)\s+(\d+)\s+([xyzw ]+?)\s+(\d+)\s+(\w+)\s+(\w+)', l)
    if not m:
        continue
    name, idx, mask, fmt = m.group(1), m.group(2), m.group(3).replace(' ', ''), m.group(6)
    if m.group(5) not in ('NONE', 'POS'):
        continue          # PS-only system values (SV_IsFrontFace, SV_SampleIndex...) are not VS outputs
    n = len(mask); ty = {'float': 'float', 'uint': 'uint', 'int': 'int'}[fmt] + ('' if n == 1 else str(n))
    sem = 'SV_Position' if name == 'SV_Position' else '%s%s' % (name, idx)
    interp = 'nointerpolation ' if fmt != 'float' else ''
    fields.append('%s%s f%d : %s;' % (interp, ty, len(fields), sem))
    body.append('o.f%d = (%s)p.%s;' % (len(fields) - 1, ty, 'xyzw'[:n] if n > 1 else 'x'))
src = 'struct O { %s };\nO main(float4 p : POSITION) { O o; %s return o; }\n' % (' '.join(fields), ' '.join(body))
hl = sys.argv[2] + '.hlsl'; open(hl, 'w').write(src)
subprocess.run([dxcom.DXC, '-T', 'vs_6_5', '-E', 'main', '-Fo', sys.argv[2], hl], check=True)
print(src)
