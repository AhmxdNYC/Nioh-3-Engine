r"""rga_vs.py - compile a meshlet VS (with a sink PS) through RGA's offline DX12 driver and summarise the VS ISA:
VMEM loads (buffer_load*), SMEM loads (s_load*/s_buffer_load*), full vmcnt waits, branches, VGPR/SGPR, waves/SIMD.
    py -I rga_vs.py VS.dxbc [VS2.dxbc ...] [--chips gfx1102,gfx1103,gfx1201] [--isa DIR]
RGA 2026-05 has no RDNA2 target; gfx1102 (RX 7600, 32 CU mid-range) is the closest proxy for the RX 6650 XT."""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'materials')); sys.path.insert(0, os.path.join(HERE, '..', 'gi'))
import rga_ps, rga_stats  # noqa: E402
SINK = os.path.join(HERE, 'work', os.environ.get('MSV_SINK', 'sink_ps.dxbc'))   # MSV_SINK=sink_pos.dxbc for shadow VSs
if not os.path.exists(SINK):   # compile the sink pixel shader (sink_ps.hlsl / sink_pos.hlsl next to this script)
    os.makedirs(os.path.dirname(SINK), exist_ok=True)
    subprocess.run([rga_ps.dxcom.DXC, '-T', 'ps_6_5', '-E', 'main', os.path.join(HERE, os.path.basename(SINK)[:-5] + '.hlsl'), '-Fo', SINK], check=True)
GPSO = ('# schemaVersion\n1.0\n\n# InputLayoutNumElements\n0\n\n# InputLayout\n\n\n# PrimitiveTopologyType\n'
        'D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE\n\n# NumRenderTargets\n1\n\n# RTVFormats\n{ DXGI_FORMAT_R16G16B16A16_FLOAT }\n')


def compile_vs(vs, chip):
    d = tempfile.mkdtemp(prefix='rgavs_')
    try:
        shutil.copy(vs, os.path.join(d, 'vs.dxbc')); shutil.copy(SINK, os.path.join(d, 'ps.dxbc'))
        open(os.path.join(d, 'p.gpso'), 'w').write(GPSO)
        rs = rga_ps.root_sig(rga_ps.dxcom.dumpbin(vs)).replace('ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT', '0')
        open(os.path.join(d, 'rs.hlsl'), 'w').write(rs)
        r = subprocess.run([rga_ps.RGA, '-s', 'dx12', '--offline', '-c', chip, '--vs-blob', 'vs.dxbc', '--ps-blob', 'ps.dxbc',
                            '--gpso', 'p.gpso', '--rs-hlsl', 'rs.hlsl', '--rs-macro', 'RS', '--isa', 'o.txt'],
                           capture_output=True, text=True, cwd=d)
        f = [x for x in os.listdir(d) if x.endswith('vert.txt')]
        if not f:
            raise RuntimeError(r.stdout[-1500:] + r.stderr[-800:])
        return open(os.path.join(d, f[0]), encoding='utf-8', errors='replace').read()
    finally:
        shutil.rmtree(d, ignore_errors=True)


def summary(isa):
    st = rga_stats.analyse(isa)
    op = [l.split()[0] for l in isa.splitlines() if re.match(r'^\s+[a-z_][a-z0-9_]*\s', l)]
    st['vmem'] = sum(1 for o in op if re.match(r'^(buffer_load|tbuffer_load|global_load)', o))
    st['smem'] = sum(1 for o in op if re.match(r'^s_(buffer_)?load', o))
    st['rfl'] = sum(1 for o in op if o == 'v_readfirstlane_b32')
    return st


if __name__ == '__main__':
    a = sys.argv[1:]
    chips = a[a.index('--chips') + 1].split(',') if '--chips' in a else ['gfx1102', 'gfx1103', 'gfx1201']
    isadir = a[a.index('--isa') + 1] if '--isa' in a else None
    vs = [x for i, x in enumerate(a) if not x.startswith('--') and (i == 0 or a[i - 1] not in ('--chips', '--isa'))]
    for c in chips:
        for v in vs:
            isa = compile_vs(v, c)
            if isadir:
                os.makedirs(isadir, exist_ok=True)
                open(os.path.join(isadir, '%s_%s.isa.txt' % (os.path.basename(v).split('.')[0], c)), 'w').write(isa)
            s = summary(isa)
            print('%-8s %-24s inst=%5d vmem=%3d smem=%3d rfl=%3d br=%3d wait0=%3d vgpr=%3d sgpr=%3d wave%d' % (
                c, os.path.basename(v), s['instructions'], s['vmem'], s['smem'], s['rfl'], s['branches'],
                s['mem_waits_full'], s['vgprs'], s['sgprs'], s['wave']))
