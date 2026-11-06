"""Check script-derived path selection, especially shifted serialized symbol IDs."""
import sys, unittest
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
if __name__=='__main__':unittest.main()
