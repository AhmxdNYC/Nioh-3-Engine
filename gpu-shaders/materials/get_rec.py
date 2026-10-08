r"""get_rec.py - write one record of a staged replacement shader_dx12 file (gpu-shaders\replacement\<file>) as work\<file>_<k>.new.dxbc/.ll
    py -I get_rec.py 0xdf46710f.file:1 [...]"""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import build_sfb, dxcom  # noqa: E402
for a in sys.argv[1:]:
    f, _, k = a.partition(':')
    g = build_sfb.Group(open(os.path.join(build_sfb.REPL, f), 'rb').read())
    base = os.path.join(HERE, 'work', '%s_%s.new' % (f.replace('.file', ''), k))
    open(base + '.dxbc', 'wb').write(g.dxbc(int(k)))
    open(base + '.ll', 'w', encoding='utf-8').write(dxcom.dumpbin(base + '.dxbc'))
    print('wrote', base + '.dxbc')
