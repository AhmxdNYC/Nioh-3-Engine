r"""rga_ps.py - compile a material PS (with its VS) through RGA's offline DX12 driver for gfx1103 / gfx1201 and summarise
the pixel-shader ISA (instructions, VMEM loads, samples, branches, full memory waits, VGPRs, waves/SIMD).

    py -I rga_ps.py VS.dxbc PS.dxbc [PS2.dxbc ...] [--chips gfx1103,gfx1201]
The .gpso (input layout from the VS input signature, 8 x RGBA16F targets) is generated; RGA runs in a no-space temp dir.
"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'gi')); sys.path.insert(0, HERE)
from rga_stats import analyse, waves_per_simd  # noqa: E402  (GI agent's ISA summariser, read-only use)
import dxcom  # noqa: E402

RGA = os.path.join(HERE, '..', '..', 'tools', 'rdts', 'RadeonDeveloperToolSuite-2026-05-28-1806', 'rga.exe')
FMT = {1: 'R32_FLOAT', 2: 'R32G32_FLOAT', 3: 'R32G32B32_FLOAT', 4: 'R32G32B32A32_FLOAT'}
IFMT = {1: 'R32_UINT', 2: 'R32G32_UINT', 3: 'R32G32B32_UINT', 4: 'R32G32B32A32_UINT'}


def gpso(vs_ll):
    els, off = [], 0
    sec = vs_ll.split('; Input signature:', 1)[1].split('; Output signature:', 1)[0]
    for l in sec.splitlines():
        m = re.match(r'^; (\w+)\s+(\d+)\s+([xyzw]+)\s+(\d+)\s+(\w+)\s+(\w+)', l)
        if m and m.group(5) == 'NONE':
            n = len(m.group(3)); f = (IFMT if m.group(6) in ('uint', 'int') else FMT)[n]
            els.append('{ "%s", %s, DXGI_FORMAT_%s, 0, %d, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }'
                       % (m.group(1), m.group(2), f, off)); off += 4 * n
    return ('# schemaVersion\n1.0\n\n# InputLayoutNumElements\n%d\n\n# InputLayout\n%s\n\n# PrimitiveTopologyType\n'
            'D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE\n\n# NumRenderTargets\n8\n\n# RTVFormats\n{ %s }\n'
            % (len(els), ',\n'.join(els), ', '.join(['DXGI_FORMAT_R16G16B16A16_FLOAT'] * 8)))


def root_sig(*lls):
    """root signature (HLSL macro RS) covering the PS's resource bindings table: one CBV/SRV/UAV table + one sampler
    table; unbounded (bindless) ranges stay unbounded. The shipped containers carry no RTS0."""
    regs, unb = set(), []
    lines = []
    for ll in lls:
        lines += ll.split('; Resource Bindings:', 1)[1].splitlines()[3:] + ['']
    for l in lines:
        if not l.startswith(';') or not l[1:].strip():
            continue
        m = re.match(r'^;\s+(?:\S+\s+)?(cbuffer|texture|sampler|UAV)\s+\S+\s+\S+\s+\S+\s+([a-z]+)(\d+)(?:,space(\d+))?\s*(\d+|unbounded)', l)
        if not m:
            continue
        kind = {'cbuffer': 'CBV', 'texture': 'SRV', 'UAV': 'UAV', 'sampler': 'Sampler'}[m.group(1)]
        sp, r0 = int(m.group(4) or 0), int(m.group(3))
        if m.group(5) == 'unbounded':
            e = (kind, m.group(2)[-1], r0, sp)
            if e not in unb:
                unb.append(e)
        else:
            regs.update((kind, m.group(2)[-1], r, sp) for r in range(r0, r0 + int(m.group(5))))
    runs = []                   # merge registers of the shaders (they may overlap) into contiguous ranges
    for k in sorted(regs, key=lambda x: (x[0], x[3], x[2])):
        if runs and runs[-1][0] == k[0] and runs[-1][3] == k[3] and runs[-1][2] + runs[-1][4] == k[2]:
            runs[-1][4] += 1
        else:
            runs.append([k[0], k[1], k[2], k[3], 1])
    parts = []
    for samp in (False, True):  # bounded ranges share a table; every unbounded range gets its own
        b = ['%s(%s%d, space=%d, numDescriptors=%d)' % (k, c, r, s_, n) for k, c, r, s_, n in runs if (k == 'Sampler') == samp]
        if b:
            parts.append('DescriptorTable(%s)' % ', '.join(b))
        parts += ['DescriptorTable(%s(%s%d, space=%d, numDescriptors=unbounded))' % u for u in unb if (u[0] == 'Sampler') == samp]
    return '#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), %s"\n' % ', '.join(parts)


def compile_ps(vs, ps, chip):
    d = tempfile.mkdtemp(prefix='rgaps_')
    try:
        shutil.copy(vs, os.path.join(d, 'vs.dxbc')); shutil.copy(ps, os.path.join(d, 'ps.dxbc'))
        open(os.path.join(d, 'p.gpso'), 'w').write(gpso(dxcom.dumpbin(vs)))
        open(os.path.join(d, 'rs.hlsl'), 'w').write(root_sig(dxcom.dumpbin(ps), dxcom.dumpbin(vs)))
        r = subprocess.run([RGA, '-s', 'dx12', '--offline', '-c', chip, '--vs-blob', os.path.join(d, 'vs.dxbc'),
                            '--ps-blob', os.path.join(d, 'ps.dxbc'), '--gpso', os.path.join(d, 'p.gpso'),
                            '--rs-hlsl', os.path.join(d, 'rs.hlsl'), '--rs-macro', 'RS',
                            '--isa', os.path.join(d, 'o.txt')], capture_output=True, text=True, cwd=d)
        f = [x for x in os.listdir(d) if x.endswith(STAGE + '.txt')]
        if not f:
            raise RuntimeError(r.stdout[-1500:] + r.stderr[-800:])
        return open(os.path.join(d, f[0]), encoding='utf-8', errors='replace').read()
    finally:
        shutil.rmtree(d, ignore_errors=True)


STAGE = 'pixel'   # '--stage vert': report the vertex shader instead (then the fixed first argument is the PS)


def stats(vs, ps, chip):
    isa = compile_ps(vs, ps, chip)
    st = analyse(isa)
    st['buffer_loads'] = len(re.findall(r'^\s*(buffer_load|tbuffer_load)', isa, re.M))
    st['waves_simd'] = waves_per_simd(chip, st['vgprs'], st['wave'])
    return st, isa


if __name__ == '__main__':
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    chips = sys.argv[sys.argv.index('--chips') + 1].split(',') if '--chips' in sys.argv else ['gfx1103', 'gfx1201']
    a = [x for x in a if x != ','.join(chips)]
    if '--stage' in sys.argv:
        STAGE = sys.argv[sys.argv.index('--stage') + 1]
        a = [x for x in a if x != STAGE]
    for c in chips:
        for x in a[1:]:
            st, _ = stats(x, a[0], c) if STAGE == 'vert' else stats(a[0], x, c)
            ps = x
            print('%-8s %-28s inst=%5d vmem=%4d bufld=%4d smp=%3d br=%3d wait0=%3d vgpr=%3d waves/SIMD=%2d' % (
                c, os.path.basename(ps), st['instructions'], st['vmem_loads'], st['buffer_loads'], st['samples'],
                st['branches'], st['mem_waits_full'], st['vgprs'], st['waves_simd']))
