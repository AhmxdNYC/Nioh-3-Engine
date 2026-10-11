"""Synthetic parser/invalidation checks, not measured performance samples."""
import struct
import unittest
from cbv_emit_opportunities import analyze, TraceError


def call(payload=b'abcd', tag=1, cmdlist=7, ctx=9, root=0, stage=0, dirty=True):
    return (struct.pack('<6I3Q', 0x4C4C4143, stage, int(dirty), 1, 1, root, ctx, tag, cmdlist)
            + struct.pack('<2I', len(payload), 0) + (payload if dirty else b''))


class TraceTests(unittest.TestCase):
    def test_actual_format_and_missing_proof(self):
        result = analyze(call() + call())
        self.assertEqual(result['equal_prior_payload_bytes_same_observed_identity'], 4)
        self.assertEqual(result['emitter_correlated_calls'], 0)
        self.assertIsNone(result['candidate_eliminated_emitter_bytes'])

    def test_changed_payload_is_not_equal(self):
        self.assertEqual(analyze(call() + call(b'abce'))['equal_prior_payload_records_same_observed_identity'], 0)

    def test_list_tag_root_stage_and_context_separation(self):
        for change in [dict(tag=2), dict(cmdlist=8), dict(root=2), dict(stage=1), dict(ctx=8)]:
            with self.subTest(change=change):
                self.assertEqual(analyze(call() + call(**change))['equal_prior_payload_records_same_observed_identity'], 0)

    def test_aba_after_observed_transition_does_not_reuse_old(self):
        result = analyze(call() + call(tag=2) + call())
        self.assertEqual(result['equal_prior_payload_records_same_observed_identity'], 0)

    def test_root_change_overwrites_prior_slot_observation(self):
        result = analyze(call() + call(root=2) + call())
        self.assertEqual(result['equal_prior_payload_records_same_observed_identity'], 0)

    def test_untouched_slot_has_no_serialized_payload(self):
        result = analyze(call(dirty=False))
        self.assertEqual(result['dirty_payload_records'], 0)

    def test_truncation_at_every_position_and_trailing_bytes(self):
        data = call()
        for cut in range(len(data)):
            with self.subTest(cut=cut), self.assertRaises(TraceError):
                analyze(data[:cut])
        with self.assertRaises(TraceError):
            analyze(data + b'x')

    def test_stale_documented_header_rejected(self):
        with self.assertRaises(TraceError):
            analyze(b'CBUTRACE' + struct.pack('<I', 1) + call())

    def test_bad_count_stage_slot_and_duplicate_slot(self):
        for position, value in [(12, 0xffffffff), (4, 7), (52, 14)]:
            data = bytearray(call())
            struct.pack_into('<I', data, position, value)
            with self.subTest(position=position), self.assertRaises(TraceError):
                analyze(data)
        data = struct.pack('<6I3Q', 0x4C4C4143, 0, 0, 2, 2, 0, 1, 1, 1) + struct.pack('<4I', 4, 0, 4, 0)
        with self.assertRaises(TraceError):
            analyze(data)


if __name__ == '__main__':
    unittest.main()
