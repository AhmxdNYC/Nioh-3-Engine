# Exact PS05ad Ally compiler admission packet

Private artifact: `D:/nioh3-re/candidates/ps05ad-ally-admission-20261012.zip`
(181665 bytes), SHA256
`52d4add36fa1c466631d7e73a8f69261dc501bea52a46d0741555044b3eac80f`.
Manifest SHA256
`a7dc9d1de589e8de2b8109b91232967f84ddbde309e400cd07aa3d9913af35bd`.
No game, native helper, Proton, bridge, installation or environment mutation ran
during preparation. Extracted shader fixtures stay private.

Provenance: candidate codegen `d65c050` / main `758e98a`; native preparation
`15ee2f4`; retained source packet
`D:/nioh3-re/deliveries/pb2-material-b-native-20261012`.
Its RX9070 receipt records four roots/eight original/candidate PSOs S_OK with
zero draws. That is Windows compiler evidence, not an Ally result.

Exact pins:

| Input | SHA256 |
|---|---|
| Original binding | `8bd0e44288a48d95568f8d8126a0c5e11e4f291e5d9639e88accdd6851da564e` |
| Candidate PS | `efb7895ca386d3e0b43f4d6b61aa20195cf0c0d50c79efc4baedab313b56444c` |
| Original PS | `4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b` |
| Unchanged companion VS | `83d64677954ab9c2368b032ea022659353a701bf6a1daf07c37eeaf93a6d55bf` |
| Root (each of four) | `dc65a1fc5ca7cb3fbcd100895637c8eab667904e6f613625597bad8890d0031c` |
| Retained helper executable | `bcd1e5395c2ac2a004b858931b9a92942f8e200eac72faeac08812ef42afee11` |
| Recorded journal | `f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d` |

The manifest/binding retain all four descriptor hashes and journal offsets
16116818, 16190406, 16195298, 16198120. Original VS70d0ee9f and PS05ad1715
are preserved, only the candidate PS is substituted. Six RT formats
91/11/87/26/87/34, DSV20 and samples1/0 remain exact. The original helper
is not rebuilt: import inspection shows only d3d12.dll, dxgi.dll and KERNEL32.dll,
with no separate MSVC-runtime dependency. Source travels beside executable.

Reproduce packaging with
`gpu-shaders/pb2pixel/package_material_b_proton.py --source <retained-packet> --out <new-private-directory>`.
Before a device action, root verifies the archive hash and coordinates an idle
Ally/device lease. In the extracted folder, `python3 proton_material_b_admit.py`
verifies hashes only. Full instructions are in its README.txt. Native execution
requires the actual existing configured game Proton context and launch wrapper:

```text
python3 proton_material_b_admit.py --proton <actual-current-proton-launcher>
  --vkd3d <actual-runtime-d3d12.dll> --driver-inventory <fresh-vulkan-mesa-inventory>
  --expected-device <actual-four-hex-digit-Ally-device-id> --receipt <new-private-json>
```

These are placeholders: this static task did not access the Ally to establish
the current wrapper, device ID, Proton/vkd3d paths or fresh Mesa inventory.
Those are remaining device-side prerequisites. Do not manufacture environment
variables, create a prefix, select another Proton or install dependencies.
The runner inherits context unchanged and refuses missing context or a detected
Nioh3 process. Root still owns exclusion against concurrent test/game launches.

Acceptance requires exit0, expected AMD device, WaveOps/SM6.5, exactly four
root/eight PSO successes, and unchanged fixture/runtime hashes. Runtime paths
and driver inventory are explicitly operator-supplied evidence, not automatic
proof of what Proton loaded. Retain the fresh inventory and stdout/stderr with
the immutable receipt. Timeout fails after120s and requires root verification
that Proton children have exited before any next game.

Validation: hash-only packet PASS;39 offline checks passed for exact and missing/
duplicate PSO/root results, wrong device, nonzero exit, missing capabilities,
altered root/descriptor/VS/PS/candidate/executable/binding, and game-presence guard.
No GPU executable was invoked. Existing PS7dc fixed-pin live_switch.py is absent
and must not be used. Original/candidate .file containers are deliberately not
included: this packet authorizes no staging. Follow-on needs an independently
admitted PS05ad slot1 adapter, actual draw participation and image/control smoke,
verified exact restoration and matched profiler-off Ally A/B. Compiler PASS
alone proves neither image equivalence nor performance; all these gates remain
UNRUN on Ally.
