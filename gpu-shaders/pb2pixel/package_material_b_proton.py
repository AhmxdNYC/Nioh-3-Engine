"""Package retained PS05ad inputs for private Proton compiler admission; never runs GPU."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source, out = args.source, args.out
    binding = json.loads((source / 'binding.json').read_text())
    prior = json.loads((source / 'native-admission.json').read_text())
    if sha(source / 'binding.json') != '8bd0e44288a48d95568f8d8126a0c5e11e4f291e5d9639e88accdd6851da564e':
        raise ValueError('wrong retained binding')
    for name, digest in binding['files'].items():
        if sha(source / name) != digest:
            raise ValueError('changed retained input: ' + name)
    if prior['exit_code'] or prior['binding_sha256'] != sha(source / 'binding.json'):
        raise ValueError('Windows receipt mismatch')
    if sha(source / 'native/pixel_admit.exe') != prior['helper_exe_sha256']:
        raise ValueError('changed native helper')
    out.mkdir(parents=True, exist_ok=False)
    names = ['pixel_admit.exe', 'pixel_admit.cpp', 'original.dxbc', 'material.dxbc', 'candidate.dxbc']
    names += [f'{kind}{i}.bin' for kind in ('root', 'desc') for i in range(4)]
    for name in names:
        shutil.copyfile(source / 'native' / name, out / name)
    shutil.copyfile(source / 'binding.json', out / 'binding.json')
    shutil.copyfile(source / 'native-admission.json', out / 'windows-admission.json')
    shutil.copyfile(Path(__file__).with_name('proton_material_b_admit.py'), out / 'proton_material_b_admit.py')
    (out / 'README.txt').write_text('''PRIVATE PS05ad / VS70d Proton compiler-only packet. Do not publish game fixtures.
This exact helper creates four captured root signatures and eight PSOs: each
original/candidate PS pair keeps the exact original VS/root/descriptor. It has
no draw, queue, window, installation or game-start path. No shader pack or
installer is included. Never use the unrelated fixed-pin PS7dc live_switch.py.

1. Root coordinates an idle Ally, no game/timing capture, with its device lease.
2. Extract outside game and prefix installations; verify the received zip hash.
3. python3 proton_material_b_admit.py
   This verifies hashes only and is safe without a GPU or Proton context.
4. Through the ALREADY approved device wrapper in its EXISTING configured game
   Proton context, using the SAME Proton/prefix/renderer settings, execute:
   python3 proton_material_b_admit.py --proton /actual/current/Proton/proton 
     --vkd3d /actual/current/Proton/files/lib64/wine/vkd3d-proton/d3d12.dll
     --driver-inventory /actual/fresh-vulkan-mesa-inventory.txt
     --expected-device ACTUAL_FOUR_HEX_DIGIT_ALLY_ID
     --receipt /private/new-unique-ps05ad-receipt.json
   Paths above are placeholders, not a new runtime selection. Obtain actual
   device ID and current driver/Mesa identity from the existing Ally inventory.
   Plain-shell absence of compatibility context REFUSES. Do not invent env vars,
   change Proton, add a prefix, install dependencies or toggle driver options.
   If the approved context wrapper/identity evidence is unavailable, stop here.
5. Retain the immutable JSON, exact stdout/stderr and fresh driver inventory.
   PASS requires all FOUR root + EIGHT PSO calls S_OK, WaveOps and SM6.5, expected
   AMD device, unchanged inputs/runtime, and exit0. Original failure invalidates
   the baseline; it is not a candidate-only rejection. A timeout fails; root
   must verify helper/Proton child closure before permitting the next game.

Included RX9070 Windows receipt is provenance only, never an Ally PASS.
Helper imports only d3d12.dll, dxgi.dll, KERNEL32.dll (retained static-CRT build).
No draws, image equivalence, runtime participation or FPS gain follows from PSO
acceptance. Follow-on requires independently admitted PS05ad slot1 staging,
actual draw participation/image/control smoke, exact restore and matched
profiler-off Ally A/B. No such live adapter/admission is included here.
''', encoding='utf-8')
    manifest = dict(scope='PRIVATE_EXACT_PS05AD_VS70D_PROTON_PSO_NO_DRAW',
                    candidate_checkpoint='d65c050', main_equivalent='758e98a',
                    native_preparation_checkpoint='15ee2f4',
                    binding_sha256=sha(source / 'binding.json'),
                    journal_sha256=binding['journal_sha256'], records=binding['records'],
                    candidate_shader_sha256=binding['candidate_shader_sha256'],
                    helper_exe_sha256=prior['helper_exe_sha256'],
                    files={p.name: sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    archive = out.with_suffix('.zip')
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(out.iterdir()):
            z.write(path, path.name)
    print(json.dumps(dict(directory=str(out), archive=str(archive), sha256=sha(archive),
                         manifest_sha256=sha(out / 'manifest.json'), bytes=archive.stat().st_size), indent=2))


if __name__ == '__main__':
    main()
