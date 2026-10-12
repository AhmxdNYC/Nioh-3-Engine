# Private Proton admission bundle

Prepared after Windows recorded-pipeline admission passed. No bridge, upload,
device operation or environment change was performed during packaging.

Artifact: `D:/nioh3-re/candidates/pb2-native-admission-1e1f9f7.zip`
(174276 bytes), SHA256
`222cb00d1abd96a61b9a58148ac01e252eafa2930ebf80a13dfc917e698507bb`.
Manifest SHA256:
`eb39cea9cb6d8176d2f5e6bb32084496d5f0fcbb4d32262f203709e2c848dc97`.

The manifest binds the exact helper, nineteen shader/root/descriptor fixtures,
Windows receipt, verifier and short README. It records the journal and selected
record hashes without the original user's filesystem path. Extracted game
shader fixtures remain private. No game files are installed by this bundle.

After root coordinates an idle device and private extraction, first run:

```
python3 proton_admit.py
```

Then, through the existing configured game Proton launch context:

```
python3 proton_admit.py --proton /actual/current/Proton/proton
```

Use the device's actual existing Proton; the path above is a placeholder, not
a new Proton selection. The runner inherits the environment unchanged and
refuses absent compatibility context. Root must use the existing approved
device wrapper, rather than invent a prefix or renderer variables. It records
the Proton launcher/version/manifest hashes when available, exact stdout and
stderr, a 120s bound, and post-run fixture identity in `proton-admission.json`.
Timeout is failure and requires checking child closure before another game.

Local hash-only validation PASS. Two actual-verifier negatives, one altered
root-signature byte and one altered topology byte, both fail before invoking
Proton or creating a device. No extra shader symbolic checks were repeated.

## Next performance gates

1. Require original AND candidate S_OK for all eight actual state variants on
   the intended device. Retain installed Proton/vkd3d/Mesa identity beside the
   receipt. A broken original baseline cannot reject the candidate alone.
2. Where retained target compiler artifacts permit it, inspect actual VS
   subgroup/all-equal/first-lane and mixed nonuniform descriptor lowering.
   Pipeline acceptance alone does not measure uniform-branch frequency or
   establish that RADV removes the Windows-observed waterfall overhead.
3. Resolve the selected packed material permutation before root stages one
   private record. The two duplicate VS containers remain alternatives; the
   journal identifies shader bytes/state, not which container supplied them.
4. After root admission, use a short same-scene native movement/geometry/motion
   smoke, restore, then matched profiler-off A/B on the GPU-bound Ally. Report
   frame-time distributions and repeats, keeping settings/camera/path fixed.
   Use separate targeted PSO profiling only for attribution. The historical
   pair interval is not isolated VS time and is not a promised savings budget.

The bundle is an executable compiler gate, not a default install, picture
acceptance or performance promotion.
