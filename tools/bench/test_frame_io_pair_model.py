"""Small offline ABI, counterexample and retirement checks; no native calls."""
import pathlib,sys,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from frame_io_pair_model import Case,CopyRecord,Wrapper,Machine,stock,eager_pair,eligibility,verdict

class PairModelTests(unittest.TestCase):
    def test_actual_record_layout(self):
        raw=bytes.fromhex('01000000000000000200000000000000030000000000000004000000000000000500000000000000')
        self.assertEqual(CopyRecord(1,2,3,4,5).pack(),raw)
        self.assertEqual(CopyRecord.unpack(raw),CopyRecord(1,2,3,4,5))
    def test_invalid_records(self):
        for raw in (b'',b'0'*39,b'0'*41):
            with self.assertRaises(ValueError):CopyRecord.unpack(raw)
        for record in (CopyRecord(1,0,2,0,-1),CopyRecord(1,2**64-1,2,0,2)):
            with self.assertRaises(ValueError):record.pack()
    def test_success_only_changes_submissions(self):
        c=Case();a,b=stock(c),eager_pair(c)
        self.assertEqual(a.observable,b.observable);self.assertEqual(a.records,b.records)
        self.assertEqual(verdict(c)['stock_submits'],2);self.assertEqual(verdict(c)['pair_submits'],1)
        for m in (a,b):self.assertFalse(m.pending);self.assertTrue(all(v==0 for v in m.refs.values()))
    def test_wrapper_alias_and_pointer_reread(self):
        for c in (Case(b=Wrapper(1,101,201)),Case(reread_b=Wrapper(3,103,203))):
            self.assertTrue(eligibility(c));self.assertEqual(stock(c).observable,eager_pair(c).observable)
            self.assertNotEqual(stock(c).observable,eager_pair(c,guard=False).observable)
    def test_count_reread(self):
        c=Case(reread_count=8)
        self.assertIn('count_reread_changes',eligibility(c))
        self.assertNotEqual(stock(c).records,eager_pair(c,guard=False).records)
    def test_range_aliases(self):
        cases=[(Case(b=Wrapper(2,101,202,source_offset=4)),'source_ranges_alias'),
               (Case(b=Wrapper(2,102,201,staging_offset=4)),'staging_ranges_alias'),
               (Case(b=Wrapper(2,201,202)),'copy_destination_aliases_source')]
        for c,reason in cases:self.assertIn(reason,eligibility(c))
        self.assertFalse(eligibility(Case(b=Wrapper(2,101,202,source_offset=16))))
    def test_guarantees_are_admission_requirements(self):
        self.assertIn('source_epoch_not_pinned',eligibility(Case(stable_epoch=False)))
        self.assertIn('lifetime_not_pinned',eligibility(Case(lifetime_pin=False)))
        self.assertIn('nonpositive_count',eligibility(Case(count=0)))
        self.assertEqual(stock(Case(count=0)).observable,[('recount',)])
        self.assertFalse(stock(Case(count=0)).records)
    def test_A_creation_failure_equal_and_no_refs(self):
        c=Case(failures=frozenset(['A_create']))
        self.assertEqual(stock(c).observable,eager_pair(c).observable)
        self.assertFalse(eager_pair(c).refs)
    def test_B_creation_failure_is_concrete_no_go(self):
        c=Case(failures=frozenset(['B_create']))
        self.assertFalse(eligibility(c)) # Cannot gate on unknown future allocation failure.
        self.assertEqual(verdict(c)['status'],'NO_GO_EAGER_PAIR')
        self.assertIn(('map','A',1,101,0,16),stock(c).observable)
        self.assertNotIn(('map','A',1,101,0,16),eager_pair(c).observable)
    def test_error_precedence_counterexample(self):
        c=Case(failures=frozenset(['A_map','B_create']))
        self.assertEqual(stock(c).observable[0],('fail','A','map'))
        self.assertEqual(eager_pair(c).observable[0],('fail','B','create'))
    def test_A_map_failure_extra_copy_is_explicit(self):
        c=Case(failures=frozenset(['A_map']));a,b=stock(c),eager_pair(c)
        self.assertEqual(a.observable,b.observable)
        self.assertEqual(len(a.records),1);self.assertEqual(len(b.records),2)
        self.assertTrue(all(v==0 for v in b.refs.values()))
    def test_B_map_failure_preserves_output_and_cleanup(self):
        c=Case(failures=frozenset(['B_map']));a,b=stock(c),eager_pair(c)
        self.assertEqual(a.observable,b.observable);self.assertEqual(a.records,b.records)
        self.assertTrue(all(v==0 for v in b.refs.values()))
    def test_separate_submit_failures_do_not_map_to_one_submit(self):
        for label in ('A_submit','B_submit','pair_submit'):
            self.assertEqual(verdict(Case(failures=frozenset([label])))['status'],'NO_GO_EAGER_PAIR')
    def test_pending_and_failed_fence_retain_references(self):
        for c in (Case(fence_pending=True),Case(failures=frozenset(['pair_fence']))):
            m=Machine(c);items=[m.prepare('A',c.a,c.count),m.prepare('B',c.b,c.count)]
            self.assertFalse(m.submit(items,'pair'));m.finish(items)
            self.assertTrue(m.pending);self.assertTrue(all(v==1 for v in m.refs.values()))
            for kind,obj in [('source',c.a.source),('source',c.b.source),('staging',c.a.staging),('staging',c.b.staging)]:
                with self.assertRaisesRegex(AssertionError,'before fence'):m.release(kind,obj)
            m.retire();m.finish(items);self.assertTrue(all(v==0 for v in m.refs.values()))
    def test_required_reference_trace(self):
        c=Case();m=eager_pair(c)
        first_copy=next(i for i,e in enumerate(m.trace) if e[0]=='copy')
        last_fence=max(i for i,e in enumerate(m.trace) if e[0]=='fence_complete')
        for w in (c.a,c.b):
            for kind,obj in [('wrapper',w.identity),('source',w.source),('staging',w.staging)]:
                self.assertLess(m.trace.index(('retain',kind,obj)),first_copy)
                self.assertGreater(m.trace.index(('release',kind,obj)),last_fence)

if __name__=='__main__':unittest.main()
