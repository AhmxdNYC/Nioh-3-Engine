"""Check script-derived path selection, especially shifted serialized symbol IDs."""
import sys, unittest, struct
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools'))
import gen_native_entry_boxes as g
I=g.dis.Insn
class Paths(unittest.TestCase):
 def script(self,door=False,wrong=False):
  names=['entry','Function::Wait_InvadeBox_Online','Mission::GatherWarp','Character::CreateAndWait','Function::WaitWarpStart']
  ins=[I(0,0x3b,(10,)),I(10,0x2c,(4,77,0,0,2)),I(20,0x1f,(11,)),I(30,0x29,(78 if wrong else 77,)),I(40,0x2e,(4,0,0,0,0)),I(50,0x1b,(12,))]
  if door:ins.append(I(55,0x1f,(14,)))
  ins += [I(60,0x29,(99,)),I(70,0x1f,(13,))]
  return SimpleNamespace(names=names,syms=[(i,i+10,0) for i in range(5)],insns=ins)
 def test_exact_path(self):
  r=g.paths(self.script(),99);self.assertEqual(len(r),1);self.assertEqual(r[0]['box'],77)
 def test_door(self):self.assertFalse(g.paths(self.script(door=True),99))
 def test_wrong_gather(self):self.assertFalse(g.paths(self.script(wrong=True),99))
 def test_wrong_actor(self):self.assertFalse(g.paths(self.script(),98))
 def test_other_function(self):
  s=self.script();s.insns.insert(-2,I(56,0x3b,(10,)));self.assertFalse(g.paths(s,99))
class WavePaths(unittest.TestCase):
 def script(self,wave=42,host=True):
  names=['entry','Function::Wait_InvadeBox_HostOnly' if host else 'Function::Wait_InvadeBox_Online','Mission::GatherWarp','WaveBattle::RequestID']
  return SimpleNamespace(names=names,syms=[(i,i+10,0) for i in range(4)],insns=[I(0,0x3b,(10,)),I(10,0x2c,(4,77,78,0,0)),I(20,0x1f,(11,)),I(30,0x29,(88,)),I(40,0x2e,(4,0,0,0,0)),I(50,0x1b,(12,)),I(60,0x29,(wave,)),I(70,0x1b,(13,))])
 def enemy(self):
  b=bytearray(8+3*0x138);struct.pack_into('<II',b,0,0x20042200,3)
  for j,(pid,chr,wave,ordinal) in enumerate([(99,100,42,3),(98,101,42,1),(97,102,43,1)]):
   o=8+j*0x138;struct.pack_into('<II',b,o,pid,chr);struct.pack_into('<H',b,o+0x1a,wave);struct.pack_into('<I',b,o+0x1c,ordinal)
  return bytes(b)
 def test_host_wave_different_gather_box(self):
  r=g.paths(self.script(),99,42);self.assertEqual(len(r),1);self.assertEqual(r[0]['wait_boxes'],[77,78]);self.assertEqual(r[0]['wave'],42)
 def test_wrong_or_unlinked_wave_rejected(self):
  self.assertFalse(g.paths(self.script(),99,43));self.assertFalse(g.paths(self.script(),99))
 def test_online_path_still_requires_matching_box(self):self.assertFalse(g.paths(self.script(host=False),99,42))
 def test_enemy_wave_members_join_exact_group(self):
  r=g.wave_members(self.script(),self.enemy(),99);self.assertEqual([m['placement'] for m in r],[99,98]);self.assertEqual([m['ordinal'] for m in r],[3,1])
 def test_wave_link_rejects_wrong_selected_or_request(self):
  self.assertFalse(g.wave_members(self.script(),self.enemy(),97));self.assertFalse(g.wave_members(self.script(),self.enemy(),96));self.assertFalse(g.wave_members(self.script(43),self.enemy(),99))
 def test_wave_path_does_not_cross_function(self):
  s=self.script();s.insns.insert(-2,I(55,0x3b,(10,)));self.assertFalse(g.paths(s,99,42))
 def test_new_wait_invalidates_old_gather(self):
  s=self.script();s.insns[-2:-2]=[I(55,0x2c,(4,79,0,0,0)),I(56,0x1f,(11,))];self.assertFalse(g.paths(s,99,42))
 def test_oriented_box_contains_native_pose(self):
  q=bytearray(0xa0);struct.pack_into('<5f',q,0x10,206038.34375,4439.40185,302049.34375,0,-135.60321);struct.pack_into('<3f',q,0x28,3000.1987,609.3196,2084.7197)
  self.assertTrue(g.inside_xz(q,205000,302620));self.assertFalse(g.inside_xz(q,203290,301270))
if __name__=='__main__':unittest.main()
