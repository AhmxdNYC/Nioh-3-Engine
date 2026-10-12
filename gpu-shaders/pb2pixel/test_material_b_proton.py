"""Exercise packet integrity and strict native-result admission without GPU/Proton."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('packet', type=Path)
    packet = parser.parse_args().packet
    spec = importlib.util.spec_from_file_location('admit', packet / 'proton_material_b_admit.py')
    admit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(admit)
    admit.verify(packet)
    output = json.loads((packet / 'windows-admission.json').read_text())['output']
    assert admit.accepted_output(output, 0, '7550')
    checks = 2
    # Every individual root/PSO success is mandatory exactly once.
    for line in output.splitlines():
        if line.startswith('record='):
            assert not admit.accepted_output(output.replace(line + '\n', '', 1), 0, '7550')
            assert not admit.accepted_output(output + '\n' + line, 0, '7550')
            checks += 2
    assert not admit.accepted_output(output, 1, '7550')
    assert not admit.accepted_output(output, 0, '150e')
    assert not admit.accepted_output(output.replace('enabled=1', 'enabled=0'), 0, '7550')
    assert not admit.accepted_output(output.replace('supported=65', 'supported=64'), 0, '7550')
    checks += 4
    with tempfile.TemporaryDirectory(prefix='ps05ad-admission-test-') as directory:
        scratch = Path(directory)
        shutil.copytree(packet, scratch / 'packet')
        copied = scratch / 'packet'
        for name in ('root0.bin', 'desc3.bin', 'original.dxbc', 'material.dxbc',
                     'candidate.dxbc', 'pixel_admit.exe', 'binding.json'):
            target = copied / name
            original = target.read_bytes()
            target.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
            try:
                admit.verify(copied)
            except ValueError:
                pass
            else:
                raise AssertionError('corruption admitted: ' + name)
            target.write_bytes(original)
            checks += 1
        proc = scratch / 'proc'
        (proc / '123').mkdir(parents=True)
        (proc / '123/comm').write_text('Nioh3.exe\n')
        try:
            admit.game_absent(proc)
        except RuntimeError:
            pass
        else:
            raise AssertionError('live game admitted')
        (proc / '123/comm').write_text('harmless\n')
        admit.game_absent(proc)
        checks += 2
    print(f'PASS {checks} offline checks; no native executable invoked')


if __name__ == '__main__':
    main()
