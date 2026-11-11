"""Offline regression: generated literal gate dependencies and runtime admission."""
from pathlib import Path
import importlib.util,types,unittest,subprocess,sys,tempfile,json,struct
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
    def test_prerequisites_exact_postgate_mission_join(self):
        s=script([entry(0,10),lit(4,200),call(10,30),lit(14,90),call(20,20),
                  lit(24,201),call(30,30),lit(34,202),call(40,30),lit(44,203),call(50,30),
                  lit(54,100),call(60,30)])
        paths=g.gate_paths(s,100,pool=[])
        def enemy(pid,chr):
            b=bytearray(0x138);struct.pack_into('<II',b,0,pid,chr);return bytes(b)
        members=g.sequence_members(paths,[enemy(200,1),enemy(201,2),enemy(202,3),enemy(202,4),enemy(100,9),enemy(999,2)])
        self.assertEqual([(m['placement'],m['chr']) for m in members],[(100,9),(201,2)])
        # Before-wait, absent, duplicate and same-character unrelated placements excluded.

    def test_end_warp_rights_create_is_link(self):
        names=['root','Function::WaitWarpStart','Character::Create','Function::EndWarpRights','waitfn','endfn']
        # ident 20/30/40 match the shuffled helper: WaitWarpStart, Create, EndWarpRights
        syms=[(4,10,0),(1,20,0),(2,30,0),(3,40,0),(5,50,0),(0,60,0)]
        s=types.SimpleNamespace(names=names,syms=syms,insns=[
            entry(0,10),lit(4,90),call(10,20),
            entry(20,50),lit(24,90),call(30,40,0x1b),lit(34,100),call(40,30,0x1b)])
        self.assertEqual(g.gate_paths(s,100,pool=[])[0]['gate'],90)
        self.assertEqual(g.gate_paths(s,101,pool=[]),[])

    def test_staged_wave_follows_invade_and_skips_a_bare_request(self):
        names=['root','Function::Wait_InvadeBox_Online','WaveBattle::RequestID','Function::Wait_InvadeBox_HostOnly','noise']
        syms=[(0,10,0),(1,20,0),(2,30,0),(3,40,0),(4,50,0)]
        s=types.SimpleNamespace(names=names,syms=syms,insns=[
            entry(0,10),lit(4,34449),call(8,30,0x1b),
            entry(20,10),lit(24,1),call(28,20,0x1b),lit(34,65218),call(38,30,0x1b),
            entry(50,10),lit(54,16889),call(58,30,0x1b),lit(64,2),call(68,40,0x1b),
            entry(80,10),lit(84,3),call(88,40,0x1b),lit(94,24284),call(98,30,0x1b)])
        self.assertEqual(g.staged_wave_ids(s),[65218,24284])

    def test_death_veil_stations_are_the_invade_requests_only(self):
        data=json.loads((ROOT/'notes/native-entry-gates-72263.json').read_text())
        rb=next(r for r in data['rows'] if r['mission']==0xB24D)
        lfm=ROOT/'data/extract/lfm'
        if not lfm.exists():lfm=Path(r'C:\Users\Ahmad Hamza\nioh3-engine\data\extract\lfm')
        waves=g.staged_wave_ids(g.dis.load(lfm/rb['script']))
        self.assertEqual(waves,[65218,16889,24284,63339])
        text=(ROOT/'mod/Nioh3BossRematch/src/native_entry_waves_gen.h').read_text()
        self.assertIn('{0xB24Du, 4u, {65218, 16889, 24284, 63339, 0, 0, 0, 0}},',text)
        self.assertNotIn('RB24D',text)

    def test_create_before_end_warp_is_not_link(self):
        names=['root','Function::WaitWarpStart','Character::Create','Function::EndWarpRights','waitfn','endfn']
        syms=[(4,10,0),(1,20,0),(2,30,0),(3,40,0),(5,50,0),(0,60,0)]
        s=types.SimpleNamespace(names=names,syms=syms,insns=[
            entry(0,10),lit(4,90),call(10,20),
            entry(20,50),lit(24,100),call(30,30,0x1b),lit(34,90),call(40,40,0x1b)])
        self.assertEqual(g.gate_paths(s,100,pool=[]),[])

    def test_real_provenance_and_runtime_selection(self):
        data=json.loads((ROOT/'notes/native-entry-gates-72263.json').read_text())
        rows=data['rows'];self.assertEqual((ROOT/'mod/Nioh3Training/src/native_entry_sequences_gen.h').read_text(),g.render_sequences(rows))
        rb=next(r for r in rows if r['id']=='RB24D');members={m['placement']:m['chr'] for m in rb['members']}
        self.assertEqual(members[573339],720656)
        self.assertNotIn(572110,members)  # Hiruko is an interact Create, not the door spawn
        self.assertEqual((ROOT/'mod/Nioh3BossRematch/src/native_entry_gates_gen.h').read_text(),g.render(rows))
        expected={'R25E4':1047905,'RB24D':573194,'R2068':1072821,'R04DF':1051105,'R2EE9':348369,'RE7BD':4070877}
        self.assertEqual({r['id']:r['gate'] for r in rows},expected)
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
