"""Private PS05ad compiler admission only; no installation, draw or env edits."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
BINDING = '8bd0e44288a48d95568f8d8126a0c5e11e4f291e5d9639e88accdd6851da564e'
HELPER = 'bcd1e5395c2ac2a004b858931b9a92942f8e200eac72faeac08812ef42afee11'
CANDIDATE = 'efb7895ca386d3e0b43f4d6b61aa20195cf0c0d50c79efc4baedab313b56444c'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(root):
    manifest = json.loads((root / 'manifest.json').read_text())
    for name, digest in manifest['files'].items():
        if Path(name).name != name or sha(root / name) != digest:
            raise ValueError('fixture mismatch: ' + name)
    if sha(root / 'binding.json') != BINDING or sha(root / 'pixel_admit.exe') != HELPER:
        raise ValueError('wrong PS05ad binding/helper')
    binding = json.loads((root / 'binding.json').read_text())
    if sha(root / 'candidate.dxbc') != CANDIDATE:
        raise ValueError('wrong candidate')
    for name in ['original.dxbc', 'material.dxbc', 'candidate.dxbc', 'pixel_admit.cpp']:
        if sha(root / name) != binding['files']['native/' + name]:
            raise ValueError('bound input mismatch: ' + name)
    if len(binding['records']) != 4:
        raise ValueError('wrong record count')
    for i, record in enumerate(binding['records']):
        for name, field in [('root', 'root_sha256'), ('desc', 'descriptor_sha256')]:
            if sha(root / f'{name}{i}.bin') != record[field]:
                raise ValueError('root/descriptor mismatch')
    return manifest


def accepted_output(stdout, returncode, expected_device):
    lines = stdout.splitlines()
    required = [f'record={i} root_hr=00000000' for i in range(4)]
    required += [f'record={i} {kind} hr=00000000' for i in range(4)
                 for kind in ('original', 'candidate')]
    records = [line for line in lines if line.startswith('record=')]
    adapters = [line for line in lines if line.startswith('adapter=')]
    adapter_ok = len(adapters) == 1 and re.search(
        r'\bvendor=1002 device=' + re.escape(expected_device.lower()) + r'\b', adapters[0])
    return bool(returncode == 0 and adapter_ok and sorted(records) == sorted(required)
                and lines.count('admission failures=0; no draws executed') == 1
                and any(re.fullmatch(r'WaveOps hr=00000000 enabled=1 lanes=\d+\.\.\d+', x) for x in lines)
                and 'shader_model hr=00000000 supported=65' in lines)


def game_absent(proc=Path('/proc')):
    if not proc.is_dir():
        raise RuntimeError('Linux /proc required for native run')
    for path in proc.glob('[0-9]*/comm'):
        try:
            name = path.read_text().strip().lower()
        except FileNotFoundError:
            continue
        if name in ('nioh3.exe', 'nioh3'):
            raise RuntimeError('Nioh3 is running; root must coordinate an idle device')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--proton', type=Path)
    parser.add_argument('--vkd3d', type=Path, help='actual existing Proton d3d12.dll')
    parser.add_argument('--driver-inventory', type=Path, help='fresh existing-device Vulkan/Mesa inventory text')
    parser.add_argument('--expected-device', help='actual Ally AMD DXGI device ID, four hex digits')
    parser.add_argument('--receipt', type=Path, default=HERE / 'proton-material-b-admission.json')
    args = parser.parse_args(argv)
    verify(HERE)
    print('Exact PS05ad packet hashes PASS')
    if args.proton is None:
        return 0
    if sys.platform != 'linux':
        raise RuntimeError('native execution is Linux/Proton only')
    if not all((args.vkd3d, args.driver_inventory, args.expected_device)):
        raise ValueError('runtime identity and expected device are required')
    if not re.fullmatch('[0-9a-fA-F]{4}', args.expected_device):
        raise ValueError('expected-device must be four hexadecimal digits')
    proton = args.proton.resolve(strict=True)
    runtime = [proton, args.vkd3d.resolve(strict=True), args.driver_inventory.resolve(strict=True)]
    if not os.environ.get('STEAM_COMPAT_DATA_PATH') or not os.environ.get('STEAM_COMPAT_CLIENT_INSTALL_PATH'):
        raise RuntimeError('existing configured game Proton context required; do not invent environment variables')
    game_absent()
    # Reserve an immutable receipt before execution. Never overwrite a prior run.
    with args.receipt.open('x', encoding='utf-8') as receipt_file:
        receipt = dict(scope='EXACT_PS05AD_VS70D_PROTON_PSO_NO_DRAW',
                       started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                       manifest_sha256=sha(HERE / 'manifest.json'), binding_sha256=BINDING,
                       candidate_sha256=CANDIDATE, helper_exe_sha256=HELPER,
                       runtime_files={str(p): sha(p) for p in runtime},
                       runtime_identity_source='operator-supplied existing device paths/inventory',
                       expected_device=args.expected_device.lower(), accepted=False,
                       draws_executed=False, image_gate='UNRUN', performance_gate='UNRUN')
        for name in ('version', 'toolmanifest.vdf'):
            path = proton.parent / name
            if path.is_file():
                receipt['runtime_files'][str(path)] = sha(path)
        try:
            run = subprocess.run([str(proton), 'run', str(HERE / 'pixel_admit.exe')],
                                 cwd=HERE, capture_output=True, text=True, timeout=120)
            receipt.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
            receipt['accepted'] = accepted_output(run.stdout, run.returncode, args.expected_device)
        except subprocess.TimeoutExpired as exc:
            receipt.update(error='120s timeout; verify Proton child closure before any game',
                           stdout=str(exc.stdout), stderr=str(exc.stderr))
        except OSError as exc:
            receipt['error'] = str(exc)
        try:
            verify(HERE)
            receipt['fixtures_unchanged'] = True
            receipt['runtime_unchanged'] = all(sha(Path(p)) == h for p, h in receipt['runtime_files'].items())
        except (OSError, ValueError) as exc:
            receipt.update(fixtures_unchanged=False, error=str(exc))
        receipt['accepted'] &= receipt.get('fixtures_unchanged', False) and receipt.get('runtime_unchanged', False)
        json.dump(receipt, receipt_file, indent=2)
        receipt_file.write('\n')
    print(json.dumps(receipt, indent=2))
    return 0 if receipt['accepted'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
