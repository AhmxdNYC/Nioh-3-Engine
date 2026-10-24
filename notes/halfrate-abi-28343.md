# Half-rate entry ABI fix (#28343)

PC2 reported artifact1792 startup access violation with all keys OFF, address7FF689C4A26B minus base7FF689A60000 = RVA1EA26B. In the retained v2.02 dump that RVA is inside the displacement of JE at1EA268, not a half-rate patched entry. Exact fault-site bytes/registers are still needed to prove the native crash chain.

A separate definite defect was found in EmitCam: it saved RAX/RDX/R8-R11 but not RCX before calling HrCamera. The resumed native instruction at24426B immediately dereferences camera RCX+10, followed by+20/+30. A compiled helper may destroy RCX. It is active even when HRS/HRG/HRC are OFF. Independent companion review confirmed the omission (Opus unavailable in supported models).

Both entry cave types now preserve flags, RAX/RCX/RDX/R8-R11 and XMM0-5. Eight pushes plus0x88 bytes keep the Windows call aligned and provide0x20 home space plus six SIMD slots. Total saved depth0xC8 changes shadow arguments from entry+100/+108 to cave+1C8/+1D0. Stack arithmetic uses imm32; both forward/skip exits restore state before replay/return. Camera replays original mov rax,rsp; sub rsp,78 only after restoring original entry state. Stolen spans remain complete instructions (shadow5, GI5, cloud5, camera7 bytes).

Validation: existing policy and telemetry harnesses;3000 actual executable camera/pass cave calls (OFF/ON forward/skip), and hostile helper destroying volatile GPR/XMM state. No native test or claim that the reported1EA26B crash is conclusively resolved. Base remains artifact1780/1792 legacy1.7.61, version suffix hrabi; this fixes the cave defect without silently introducing a1.7.73 port. New artifact must be tested by pc2/pc. Supersedes1792 for testing; all keys remain default OFF.
