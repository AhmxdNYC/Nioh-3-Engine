"""Offline regression: generated literal gate dependencies and runtime admission."""
from pathlib import Path
import importlib.util,types,unittest,subprocess,sys,tempfile,json
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('gates',ROOT/'tools/gen_native_entry_gates.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
I=g.dis.Insn

def script(ins):
    names=['root','Function::WaitWarpStart','Character::Create','child','Script::Create','WaveBattle::RequestID']
    # Deliberately shuffled index/id to catch the old disassembler mistake.
    syms=[(2,30,0),(0,10,0),(5,60,0),(1,20,0),(3,40,0),(4,50,0)]
    return types.SimpleNamespace(names=names,syms=syms,insns=ins)
def entry(off,n):return I(off,0x3b,(n,))
def lit(off,n):return I(off,0x29,(n,))
def call(off,n,op=0x1f):return I(off,op,(n,))
class GateTests(unittest.TestCase):
    def test_direct_selected_create(self):
        s=script([entry(0,10),lit(4,90),call(10,20),lit(14,100),call(20,30,0x1b)])
        self.assertEqual(g.gate_paths(s,100,pool=[])[0]['gate'],90)
        self.assertEqual(g.gate_paths(s,101,pool=[]),[])
    def test_create_before_wait_is_not_link(self):
        s=script([entry(0,10),lit(4,100),call(10,30),lit(14,90),call(20,20)])
        self.assertEqual(g.gate_paths(s,100,pool=[]),[])
    def test_script_and_named_call_graph(self):
        for callsite in ([call(14,40)],[I(14,0x2f,(0,)),call(18,50,0x1b)]):
            s=script([entry(0,10),lit(4,90),call(10,20)]+callsite+[entry(30,40),lit(34,100),call(40,30)])
            self.assertEqual(g.gate_paths(s,100,pool=['child'])[0]['proof'][0]['function'],'child')
    def test_wave_matches_selected_enemy_record_only(self):
        s=script([entry(0,10),lit(4,90),call(10,20),lit(14,77),call(20,60,0x1b)])
        self.assertEqual(g.gate_paths(s,100,wave=77,pool=[])[0]['proof'][0]['kind'],'enemy-record-wave')
        self.assertEqual(g.gate_paths(s,100,wave=78,pool=[]),[])
    def test_ambiguous_is_visible_for_rejection(self):
        s=script([entry(0,10),lit(4,90),call(10,20),lit(14,100),call(20,30),entry(30,40),lit(34,91),call(40,20),lit(44,100),call(50,30)])
        self.assertEqual({p['gate'] for p in g.gate_paths(s,100,pool=[])},{90,91})
    def test_real_provenance_and_runtime_selection(self):
        data=json.loads((ROOT/'notes/native-entry-gates-72263.json').read_text())
        rows=data['rows'];self.assertEqual((ROOT/'mod/Nioh3BossRematch/src/native_entry_gates_gen.h').read_text(),g.render(rows))
        expected={'R25E4':1047905,'RB24D':573194,'RFCCB':1089694,'R2068':1072821,'R9A25':262638326}
        self.assertEqual({r['id']:r['gate'] for r in rows if r['id'] in expected},expected)
        s=(ROOT/'mod/Nioh3BossRematch/src/rematch.cpp').read_text();a=s.index('static uint32_t EntryGateChoice(');b=s.index('\nbool g_exitOk',a)
        body=s[a:b];header=(ROOT/'mod/Nioh3BossRematch/src/native_entry_gates_gen.h').read_text()
        checks=[]
        for r in rows:
            checks.append('assert(EntryGateChoice(%d,%d,0,false,true,true,true,true)==%d);'%(r['mission'],r['placement'],r['gate']))
        checks += ['assert(!EntryGateChoice(0,0,99,false,false,true,true,true));',
                   'assert(EntryGateChoice(0,0,99,true,false,true,true,true)==99);',
                   'assert(!EntryGateChoice(0,0,99,true,true,false,true,true));',
                   'assert(!EntryGateChoice(0,0,99,true,false,false,false,true));',
                   'assert(!EntryGateChoice(0,0,99,true,false,false,true,false));']
        self.assertIn('if (!g_gateSeen.load())',s[s.index('void GateTick('):s.index('void GateTick(')+4000])
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);src=p/'gate.cpp';exe=p/'gate.exe';src.write_text('#include <cassert>\n'+header+body+'\nint main(){'+''.join(checks)+'}')
            subprocess.run([sys.executable,'-m','ziglang','c++','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()
