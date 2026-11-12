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
        self.assertEqual(g.staged_chain(s)['boxes'],[1,3])

    def test_pre_final_is_the_sethp_before_the_last_request(self):
        names=['root','Function::Wait_InvadeBox_Online','WaveBattle::RequestID','Character::SetHpRate','Mission::SetMissionFlag']
        syms=[(0,10,0),(1,20,0),(2,30,0),(3,40,0),(4,50,0)]
        f25=struct.unpack('<I',struct.pack('<f',0.25))[0]
        ftiny=struct.unpack('<I',struct.pack('<f',0.0001))[0]
        s=types.SimpleNamespace(names=names,syms=syms,insns=[
            entry(0,10),lit(4,1),call(8,20,0x1b),lit(14,65218),call(18,30,0x1b),lit(24,573339),I(28,0x2d,(struct.unpack('<I',struct.pack('<f',0.75))[0],)),call(32,40,0x1b),
            entry(40,10),lit(44,2),call(48,20,0x1b),lit(54,16889),call(58,30,0x1b),
            entry(70,10),lit(74,3),call(78,20,0x1b),lit(84,24284),call(88,30,0x1b),lit(94,573339),I(98,0x2d,(f25,)),call(102,40,0x1b),lit(108,481),call(112,50,0x1b),
            entry(120,10),lit(124,4),call(128,20,0x1b),lit(134,63339),call(138,30,0x1b),lit(144,573339),I(148,0x2d,(ftiny,)),call(152,40,0x1b)])
        chain=g.staged_chain(s)
        self.assertEqual(chain['waves'],[65218,16889,24284,63339])
        self.assertEqual(chain['boxes'],[1,2,3,4])
        self.assertEqual((chain['hp_place'],chain['hp_thou']),(573339,250))
        one=types.SimpleNamespace(names=names,syms=syms,insns=[
            entry(0,10),lit(4,1),call(8,20,0x1b),lit(14,573339),I(18,0x2d,(f25,)),call(22,40,0x1b),lit(28,1956),call(32,30,0x1b)])
        self.assertEqual(g.staged_chain(one)['hp_thou'],0)

    def test_death_veil_stations_are_the_invade_requests_only(self):
        data=json.loads((ROOT/'notes/native-entry-gates-72263.json').read_text())
        rb=next(r for r in data['rows'] if r['mission']==0xB24D)
        lfm=ROOT/'data/extract/lfm'
        if not lfm.exists():lfm=Path(r'C:\Users\Ahmad Hamza\nioh3-engine\data\extract\lfm')
        chain=g.staged_chain(g.dis.load(lfm/rb['script']))
        self.assertEqual(chain['waves'],[65218,16889,24284,63339])
        self.assertEqual(chain['boxes'],[571326,574736,575383,578226])
        self.assertEqual((chain['hp_place'],chain['hp_thou']),(573339,250))
        text=(ROOT/'mod/Nioh3BossRematch/src/native_entry_waves_gen.h').read_text()
        self.assertIn('{0xB24Du, 4u, 250u, 573339u, {65218, 16889, 24284, 63339, 0, 0, 0, 0}, {571326u, 574736u, 575383u, 578226u, 0u, 0u, 0u, 0u}, {86221, 89905, 83393, 86040, 0, 0, 0, 0}, {20559, 21876, 21513, 26515, 0, 0, 0, 0}, {107281, 108334, 108672, 94238, 0, 0, 0, 0}},',text)
        self.assertIn('{0xB3FEu, 1u, 0u, 0u, {1956, 0, 0, 0, 0, 0, 0, 0}, {8061650u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}, {233432, 0, 0, 0, 0, 0, 0, 0}, {16346, 0, 0, 0, 0, 0, 0, 0}, {102155, 0, 0, 0, 0, 0, 0, 0}},',text)
        self.assertNotIn('RB24D',text)
        self.assertNotIn('SetMissionFlag',text)

    def test_staged_final_starts_at_the_last_wave(self):
        header=(ROOT/'mod/Nioh3BossRematch/src/native_entry_waves_gen.h').read_text()
        start=(ROOT/'mod/Nioh3BossRematch/src/staged_start.h').read_text()
        checks=[
            'uint16_t n=0; const uint16_t* w=NativeEntryWaves(0xB24D,&n); assert(n==4 && w && w[0]==65218 && w[3]==63339);',
            'assert(StagedStartIndex(n,0)==0 && StagedStartIndex(n,1)==3);',
            'uint16_t t=9; uint32_t p=9; assert(NativeEntryWaveFinal(0xB24D,&t,&p)==1 && t==250 && p==573339u);',
            'assert(NativeEntryWaveFinal(0xB3FE,&t,&p)==0 && t==0 && p==0);',
            'assert(StagedFinalHpPending(1,4,3,250,573339u,0)==1);',
            'assert(StagedFinalHpPending(0,4,3,250,573339u,0)==0);',
            'assert(StagedFinalHpPending(1,4,0,250,573339u,0)==0);',
            'assert(StagedFinalHpPending(1,4,3,250,573339u,1)==0);',
            'uint32_t box=0; int x=0,y=0,z=0; assert(NativeEntryWaveStand(0xB24D,0,&box,&x,&y,&z)==1 && box==571326u && x==86221 && y==20559 && z==107281);',
            'assert(NativeEntryWaveStand(0xB24D,3,&box,&x,&y,&z)==1 && box==578226u);',
            'assert(NativeEntryWaveStand(0xB3FE,0,&box,&x,&y,&z)==1);',
            'assert(NativeEntryWaveStand(0xB24D,4,&box,&x,&y,&z)==0);',
            'assert(StagedHpFromThou(10000,250)==2500); assert(StagedHpFromThou(51712,250)==12928);',
        ]
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);src=p/'start.cpp';exe=p/'start.exe'
            src.write_text('#include <cassert>\n'+header+'\n'+start+'\nint main(){'+''.join(checks)+'}')
            subprocess.run([sys.executable,'-m','ziglang','c++','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

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
