"""Run from an x64 MSVC developer prompt. No game process or installation."""
import pathlib, subprocess
r=pathlib.Path(__file__).resolve().parents[1]
b=r/'build'; b.mkdir(exist_ok=True)
s=(r/'src/caves.asm').read_text()
a=s.index('caveRDWake PROC FRAME');z=s.index('caveRDWake ENDP')+len('caveRDWake ENDP')
(b/'rd_unwind.asm').write_text('EXTERN RDComplete:PROC\nPUBLIC caveRDWake,rd_resume\n.data\nrd_resume DQ 0\n.code\n'+s[a:z]+'\nEND\n')
subprocess.run(['ml64','/nologo','/c','/Fo'+str(b/'rd_unwind.obj'),str(b/'rd_unwind.asm')],check=True)
for name in ('drainwait_harness','drainwait_unwind'):
 cmd=['cl','/nologo','/std:c++17','/O2','/EHsc','/MT',str(r/'tests'/f'{name}.cpp'),'/Fe'+str(b/f'{name}.exe'),'/Fo'+str(b/f'{name}.obj')]
 if name.endswith('unwind'):cmd.append(str(b/'rd_unwind.obj'))
 subprocess.run(cmd,check=True); subprocess.run([str(b/f'{name}.exe')],check=True)
