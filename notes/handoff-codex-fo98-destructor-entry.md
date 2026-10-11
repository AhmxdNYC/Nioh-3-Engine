# FO98: null-current direct destructor entry

2026-10-11, OpenAI Codex, finite inert C/native follow-up to reset-boundary.
No game/Ghidra/bridge/source/public operations. Exact benchmark sentinel
`%LOCALAPPDATA%/nioh3-codex/pc-benchmark-active` was absent at the check.
FO remains NO-GO; producer closure remains another owner's obligation.

## Verified earlier wait on primary destruction and factory failure

The conditional null-current gap is closed for the observed full graphics-owner
destruction path. Null C+580 alone is not a completion predicate.

- Deleting destructor2EE0D58 calls2EE0694 at2EE0D67. 2EE0694 unconditionally
  calls2EEC4C8(owner,0,0,0) at2EE06C5, before its tailjump at2EE09F3 to
  base destructor2EE09FC. 2EE09FC calls2EFB020(owner+10) at2EE0A09.
  Thus the direct C destructor follows the owner's previously proved GPU wait.
- Inside2EEC4C8,2EEC501 calls2EEDDEC before2EEDEF8/context teardown.
  The prior note proves both branches of its2F00A74 cleanup wait on the
  primary fence. Context teardown2F00BC4 later clears C+580 at2F00F3A.
  The second direct destructor's null-current observation is therefore after
  the earlier wait and slot teardown, not evidence that GPU waiting was omitted.
- Factory constructor2EDFB40 calls base ctor2EE0144 at2EDFB52, then installs
  derived vtable3B99288 at2EDFB67. That vtable's first entry is2EE0D58.
  CreateD3D12Device2EE300C calls that constructor at2EE3488, then initializer
  2EE9424 at2EE3532. On false,2EE3544 calls the owner's vtable+0. Therefore
  the explicit factory initialization-failure path also uses the full destructor
  and earlier wait, including failures with no current list yet.

## Verified writes; remaining scope

- Context ctor2EFA840 zeroes R14 at2EFA864 and writes C+580=0 at2EFAB48.
  SynBeginList49349C writes C+580=its list argument at4934AF; it subsequently
  dereferences that list. Primary ring creation checks each created list nonnull
  before success; reset492220 passes its slot list to49349C.
- A bounded destination-write check over pdata functions in2EF8000..2F12000,
  plus49349C, found only the constructor zero, teardown zero, and begin-list
  assignment above. This is not a full binary alias/bulk-write proof.
- Base-owner deleting wrapper2EE0D90 ->2EE09FC and context deleting wrapper
  2EFB19C ->2EFB020 still add no earlier wait. Their vtable entries are
  409A020+0 and409A680+0 respectively; exported direct caller lists are empty.
  The factory overwrites the base-owner vtable before initialization. Independent
  invocation of either wrapper on a live submitted primary context is unproved.

No demonstrated unsafe primary destruction path was found. Completion of the
known prefix still assumes producer exclusion after the earlier wait. Next exact
proof, if a universal destructor claim is needed: resolve independent vtable+0
invocations on C or the base owner, or close all alias writes that could clear
C+580 while its submitted sequence is incomplete. Do not treat absent direct
callers as proof of absent virtual callers.

## Native provenance

`re/Nioh3.dump.exe`, flat RVA offsets; matching inert C exports under
`D:/nioh3-re/decomp`. SHA256 of RVA/hex-byte-count windows:

```text
2EE0694/36 baaf7eb1f6ac34927f2517725ad0b42a7092166546d3485617fff50f2f99df0b
2EE09D4/24 40b4362ddb96f43fc15d05e03ec29e6f8278f8c00499bd6c51e74ccb73553a03
2EE09FC/22 26f0f99440c5a8b06bbc3e82047204666e1049c3fe7d7f6a07653c2af6501246
2EEC4C8/3E bef3daabf4f36d78f288f0098be9dc77bfb840ca74e84212cd0fcb8df721829c
2EE0D58/37 feebb49f30768cef0d3f6d3b6a417b505f958f107024fc035c245eee39af59c1
2EE0D90/37 3dc675cc0ce4b4f57f4862bef32eda195643cfb73a0a5e81c2d1d11391d9368e
2EFB19C/34 d48370cb2dd131b855aba53dec1ba5dc7083ffafa718c08b98481eb51ba9f3a6
2F00F27/1A 15974273c396ddca470f65e6eb21bc88ab2ec8c0e7839a68c15af7359f558581
3B99288/8  975fcebe67542d819aef866eb061302f038f499e6146595412e3c3c99e446b7e
2EDFB4F/1B 787195fd3097064298a556ac716bd220f11f5364662253284d8b04d07005a713
2EE351B/3B c7b26c8fc7e92221a2b4485f4458d48edaec29a37b788231670599c1bc3a9321
2EFAB48/7  521fb557478410b17d544bb6db8371239e83522365ebe757c7af0825bbe11d9b
4934A6/10  f846596dfdc469724002ebeef5d505f58bdfc83b3c0f9f7b076d88e089be9c1f
```
