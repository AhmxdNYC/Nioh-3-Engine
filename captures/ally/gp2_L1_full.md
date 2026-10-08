# GP2 digest

34 ON window(s); mean GPU span 30.27 ms/frame, covered 25.10 ms (83%); table = top 120, 21.00 ms (69% of span)

| # | ms/frame | % frame | stage | pass/marker | where (shader_dx12 record / exe / runtime) | name | replacement | notes |
|---|---|---|---|---|---|---|---|---|
| 1 | 3.663 | 12.1 | cs | FFX_FSR3UPSCALER_PASS_ACCUMULATE_SHARPEN | cs exe-embedded @rva 0x3a0c940 (.rdata) | fsr/upscale | no |  |
| 2 | 1.319 | 4.4 | cs | - | cs replacement\0xa9910db8.file container #0 (39152 B) | VFSCAT-CllLpvSfbVsm | cs: OUR REPLACEMENT in 0xa9910db8.file (fog: VFSCAT #235 (volumetric fog froxel scattering): one merged early-exit) <- orig 02636fe088cc3c5674266f81e9337747 |  |
| 3 | 0.887 | 2.9 | cs | FFX_FSR3UPSCALER_PASS_PREPARE_REACTIVITY | cs exe-embedded @rva 0x3a09970 (.rdata) | fsr/upscale | no |  |
| 4 | 0.791 | 2.6 | cs | - | cs replacement\0xd070067d.file container #0 (56776 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | cs: OUR REPLACEMENT in 0xd070067d.file (gi: RefProbe #121 (diffuse probe GI gather) and #414 (reflection probe tra) <- orig dcbbd6ebfcf76369fa89bbcebfcf2285 |  |
| 5 | 0.770 | 2.5 | cs | FFX_FSR3UPSCALER_PASS_PREPARE_INPUTS | cs exe-embedded @rva 0x39f3ea0 (.rdata) | fsr/upscale | no |  |
| 6 | 0.712 | 2.4 | vs/ps | - | vs shader_dx12\0xfd4f418f.file record@490672 (32610 B); ps shader_dx12\0xdf46710f.file record@60536 (18552 B) | PB2-BlrGbiMm0sMshltvReSfbVl / PB2-BlrGbiMm00MshltPkSfbVl | no |  |
| 7 | 0.709 | 2.3 | cs | FFX_FSR3UPSCALER_PASS_LUMA_INSTABILITY | cs exe-embedded @rva 0x3a07a20 (.rdata) | fsr/upscale | no |  |
| 8 | 0.569 | 1.9 | - | - | - | - | no |  |
| 9 | 0.433 | 1.4 | cs | - | cs shader_dx12\0xd070067d.file record@12520830 (47632 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | cs: ORIGINAL replaced in shader_dx12\0xd070067d.file (gi: RefProbe #121 (diffuse probe GI gather) and #414 (reflection probe tra) |  |
| 10 | 0.411 | 1.4 | cs | DrawSky | cs shader_dx12\0x5f17ca9d.file record@356836 (72884 B) | VCLOUD | no |  |
| 11 | 0.344 | 1.1 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0xa60921c6.file record@1304136 (12659 B) | WDO_SST-PtSrbVsm / PB2MRLR-AmbSpecMapAngLimitCllLas | no |  |
| 12 | 0.340 | 1.1 | cs | FFX_FSR3UPSCALER_PASS_SHADING_CHANGE_PYRAMID | cs exe-embedded @rva 0x39f7750 (.rdata) | fsr/upscale | no |  |
| 13 | 0.339 | 1.1 | vs/ps | UltramarineRender | vs shader_dx12\0xef2fa7c0.file record@1536 (17875 B); ps shader_dx12\0xef2fa7c0.file record@19748 (17777 B) | PB2UMAH-GbVl | no |  |
| 14 | 0.298 | 1.0 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x1fd07cc0.file record@235888 (6215 B) | WDO_SST-PtSrbVsm / WDO-PtSrbVsm | no |  |
| 15 | 0.290 | 1.0 | vs/ps | - | vs shader_dx12\0xdd27eab2.file record@6620 (3866 B); ps shader_dx12\0xc5bc597a.file record@460856 (31836 B) | PB2RIPPLE-Cbbl / PB2DECAL-CdlRgbSfb | no |  |
| 16 | 0.290 | 1.0 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x04df8ee9.file record@6060 (23231 B) | WDO_SST-PtSrbVsm / PB2DS-AdmCaCllEmHsIribOfPlaSmSpl | no |  |
| 17 | 0.282 | 0.9 | vs/ps | - | vs shader_dx12\0xfd20cb1a.file record@21860 (3033 B); ps shader_dx12\0xd070067d.file record@708884 (8995 B) | SDFOCC-CllSdf / RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 18 | 0.279 | 0.9 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a24110 (.rdata) | - | no |  |
| 19 | 0.277 | 0.9 | cs | MeshletPreculling | cs shader_dx12\0x29ee0902.file record@382236 (8844 B) | MPC | no |  |
| 20 | 0.274 | 0.9 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a25640 (.rdata) | - | no |  |
| 21 | 0.258 | 0.9 | vs/ps | - | vs shader_dx12\0x0f33242a.file record@45984 (3878 B); ps shader_dx12\0x0f33242a.file record@862811 (10044 B) | PBRLR | no |  |
| 22 | 0.255 | 0.8 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3be7f90 (.rdata) | - | no |  |
| 23 | 0.233 | 0.8 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@35620 (5235 B) | GFM / HBAO | no |  |
| 24 | 0.223 | 0.7 | vs/ps/ds/hs | - | vs shader_dx12\0xdc52fd1c.file record@59224 (10671 B); ps shader_dx12\0xdc52fd1c.file record@8437664 (121851 B); ds shader_dx12\0xdc52fd1c.file record@13507187 (23887 B); hs shader_dx12\0xdc52fd1c.file record@13464058 (9854 B) | PB2LS-GbiSfbVl | no |  |
| 25 | 0.221 | 0.7 | vs/ps | - | vs shader_dx12\0x52c39db0.file record@151636 (47739 B); ps shader_dx12\0xd864e310.file record@42452 (14484 B) | PB2TREE2-BlrGbiMshltvOcReSfbVl / PB2TREE2-BlrDimGbiMshltOcPfnPkSf | no |  |
| 26 | 0.210 | 0.7 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x618b07e7.file record@5728 (6695 B) | WDO_SST-PtSrbVsm / PB2DUS-GbMasRefprobelt | no |  |
| 27 | 0.207 | 0.7 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3be7a70 (.rdata) | - | no |  |
| 28 | 0.204 | 0.7 | cs | - | cs shader_dx12\0xd070067d.file record@2050850 (23864 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 29 | 0.191 | 0.6 | vs/ps | Batch Tiles 8 | vs shader_dx12\0x9992f2ab.file record@410536 (32238 B); ps shader_dx12\0x9992f2ab.file record@549252 (6940 B) | SMTREE2-BlrMfvMshltvReSfb | no |  |
| 30 | 0.191 | 0.6 | cs | FFX_FSR3UPSCALER_PASS_RCAS | cs exe-embedded @rva 0x39e0de0 (.rdata) | fsr/upscale | no |  |
| 31 | 0.184 | 0.6 | cs | FFX_FSR3UPSCALER_PASS_LUMA_PYRAMID | cs exe-embedded @rva 0x39ee2e0 (.rdata) | fsr/upscale | no |  |
| 32 | 0.170 | 0.6 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x1667d3a0.file record@333372 (29431 B) | WDO_SST-PtSrbVsm / WDO-PtSrbSsmVsm | no |  |
| 33 | 0.165 | 0.5 | cs | - | cs replacement\0x9c481f79.file container #0 (10628 B) | TOONEDGE2 | cs: OUR REPLACEMENT in 0x9c481f79.file (toonedge2: + 0x47bc5cb4.file  TOONEDGE2 #2: outline pass, 18x18 LDS tile) <- orig 6703ebfcffac0acbcf45a0f8859c5338 | 8986d40f=gpu-shader-sweep.md:84 |
| 34 | 0.158 | 0.5 | cs | FFX_FSR3UPSCALER_PASS_GENERATE_REACTIVE | cs exe-embedded @rva 0x39e4d40 (.rdata) | taa | no |  |
| 35 | 0.157 | 0.5 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x1fd07cc0.file record@242324 (6175 B) | WDO_SST-PtSrbVsm / WDO-PtSrbVsm | ps: ORIGINAL replaced in shader_dx12\0x1fd07cc0.file (rewrite) |  |
| 36 | 0.152 | 0.5 | vs/ps | - | vs shader_dx12\0x6cb9f85a.file record@11552 (3811 B); ps shader_dx12\0x6cb9f85a.file record@392916 (13664 B) | SSFOG-CllFdHfMpfS2aVf | no |  |
| 37 | 0.152 | 0.5 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps replacement\0x1fd07cc0.file container #0 (7183 B) | WDO_SST-PtSrbVsm / WDO-PtSrbVsm | ps: OUR REPLACEMENT in 0x1fd07cc0.file (rewrite) <- orig 6a0f7c689eacc711c9b291396c4c877f | 9945c063=invisible-gpu.md:210 |
| 38 | 0.151 | 0.5 | cs | UltramarineCaustics | cs shader_dx12\0x29dc0db1.file record@696 (12008 B) | CTS | no |  |
| 39 | 0.150 | 0.5 | vs/ps | - | vs shader_dx12\0xadb3a8f3.file record@238116 (5530 B); ps shader_dx12\0xadb3a8f3.file record@285112 (5740 B) | AMBSL | no |  |
| 40 | 0.144 | 0.5 | vs/ps | DebugFSR3CopyUpscaledDepth? | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a25640 (.rdata) | - | no |  |
| 41 | 0.134 | 0.4 | cs | - | cs shader_dx12\0x2801580a.file record@448 (9412 B) | TREE2WIND | no |  |
| 42 | 0.129 | 0.4 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@992078 (19706 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 43 | 0.127 | 0.4 | vs/ps | - | vs shader_dx12\0x5f066893.file record@1506024 (10547 B); ps shader_dx12\0x5f066893.file record@67266416 (32671 B) | G1E-CllCsDDomEmeEtmFoHHfoMMvNOcP | no |  |
| 44 | 0.126 | 0.4 | cs | - | cs shader_dx12\0xd070067d.file record@2238474 (26404 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 45 | 0.117 | 0.4 | vs/ps | - | vs replacement\0xe0b2c239.file container #1 (12529 B); ps shader_dx12\0xe0b2c239.file record@105904 (53847 B) | replacement / PB2-AlepmeAtDiDt2l3GbiPfnRiaexSf | vs: OUR REPLACEMENT in 0xe0b2c239.file (rewrite) |  |
| 46 | 0.116 | 0.4 | cs | UltramarineWet | cs shader_dx12\0x4f68362d.file record@608 (9772 B) | WET | no |  |
| 47 | 0.115 | 0.4 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a3d140 (.rdata) | - | no |  |
| 48 | 0.115 | 0.4 | vs/ps | UltramarineRenderHeight | vs shader_dx12\0xcae4c6d9.file record@1464 (17483 B); ps shader_dx12\0xcae4c6d9.file record@19312 (18229 B) | UMTV-Gb | no |  |
| 49 | 0.113 | 0.4 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a3e030 (.rdata) | tonemap/compose | no |  |
| 50 | 0.110 | 0.4 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@784798 (5090 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 51 | 0.107 | 0.4 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@764398 (12390 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 52 | 0.106 | 0.4 | vs/ps | - | vs shader_dx12\0xadb3a8f3.file record@124560 (5310 B); ps shader_dx12\0xadb3a8f3.file record@296140 (5104 B) | AMBSL | no |  |
| 53 | 0.101 | 0.3 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0xec36dc74.file record@25760 (5003 B) | GFM | no |  |
| 54 | 0.089 | 0.3 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@5329 (3657 B); ps shader_dx12\0xec36dc74.file record@30763 (5055 B) | GFM | no |  |
| 55 | 0.089 | 0.3 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x1fd07cc0.file record@472660 (27575 B) | WDO_SST-PtSrbVsm / WDO-PtSrbVsm | no |  |
| 56 | 0.088 | 0.3 | vs/ps | - | vs shader_dx12\0x4aaa34e4.file record@150888 (47494 B); ps shader_dx12\0x96f6e36d.file record@38040 (13087 B) | PB2TREE2-BlrGbiMshltvReSfbVl / PB2TREE2-AtBlrGbiMshltPfnPkSfbTs | no |  |
| 57 | 0.086 | 0.3 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0x3de2e18a.file record@19716 (4134 B) | RefProbe-CllPtSfbSplsSslsSsmVsm / HBAO | no |  |
| 58 | 0.082 | 0.3 | vs/ps | VideoOut | vs runtime-compiled (dump); ps runtime-compiled (dump) | - | no |  |
| 59 | 0.077 | 0.3 | - | VideoOut | - | - | no |  |
| 60 | 0.075 | 0.2 | vs/ps | - | vs shader_dx12\0x52c39db0.file record@151636 (47739 B); ps shader_dx12\0xdcc48475.file record@43164 (15192 B) | PB2TREE2-BlrGbiMshltvOcReSfbVl / PB2TREE2-BlrDimGbiMshltOcPfnSfbT | no |  |
| 61 | 0.075 | 0.2 | vs/ps | - | vs runtime-compiled (dump); ps runtime-compiled (dump) | post VS / compose (r5 47us) | no |  |
| 62 | 0.072 | 0.2 | - | FSR3 | - | - | no |  |
| 63 | 0.069 | 0.2 | vs/ps | - | vs shader_dx12\0x193fcd66.file record@17088 (5813 B); ps shader_dx12\0xf999073d.file record@33872 (3249 B) | SM-Sfb / SM-B0I2mIptrSfb | no |  |
| 64 | 0.068 | 0.2 | vs/ps | - | vs shader_dx12\0xf00d5c8c.file record@298564 (29866 B); ps shader_dx12\0xdf8c9879.file record@1948 (25852 B) | PB2-BlrGbiMshltvReSfbVl / PB2-BlrGbiMshltPkSfbT0Vl | no |  |
| 65 | 0.067 | 0.2 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@776788 (8010 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 66 | 0.066 | 0.2 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@5329 (3657 B); ps shader_dx12\0x98327bb0.file record@12305 (4855 B) | GFM / GFM-D | no |  |
| 67 | 0.062 | 0.2 | vs/ps | - | vs shader_dx12\0xadb3a8f3.file record@226972 (5180 B); ps shader_dx12\0xadb3a8f3.file record@268540 (3903 B) | AMBSL | no |  |
| 68 | 0.062 | 0.2 | cs | - | cs shader_dx12\0xd070067d.file record@12425302 (47632 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 69 | 0.062 | 0.2 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@187228 (8375 B) | GFM / HBAO | no |  |
| 70 | 0.061 | 0.2 | cs | - | cs exe-embedded @rva 0x3a2f530 (.rdata) | - | no |  |
| 71 | 0.060 | 0.2 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x98327bb0.file record@7506 (4799 B) | GFM / GFM-D | no |  |
| 72 | 0.059 | 0.2 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@945752 (8014 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 73 | 0.058 | 0.2 | vs | Batch Tiles 8 | vs shader_dx12\0xc9a18f12.file record@536648 (17892 B) | SM-BlrMfvMshltvSfb | no |  |
| 74 | 0.056 | 0.2 | - | App_CopyMRLRScene | - | - | no |  |
| 75 | 0.054 | 0.2 | vs/ps | - | vs shader_dx12\0xadb3a8f3.file record@74876 (4463 B); ps shader_dx12\0xadb3a8f3.file record@291060 (4911 B) | AMBSL | no |  |
| 76 | 0.053 | 0.2 | - | FSR3:SaveOpaqueColor | - | - | no |  |
| 77 | 0.052 | 0.2 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0x3de2e18a.file record@23948 (3834 B) | RefProbe-CllPtSfbSplsSslsSsmVsm / HBAO | no |  |
| 78 | 0.051 | 0.2 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@937738 (8014 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 79 | 0.047 | 0.2 | vs/ps | - | vs shader_dx12\0xef68ca9a.file record@12324 (15467 B); ps shader_dx12\0xc513ce7a.file record@128828 (57008 B) | PB2FUR-AmtCllDinmDrLiOcOfPtS2aSf / PB2FUR-AmtAtCllDiDrLiOfPtS2aSfbS | no |  |
| 80 | 0.043 | 0.1 | vs/ps | - | vs shader_dx12\0x5f066893.file record@675428 (8734 B); ps shader_dx12\0x5f066893.file record@62506136 (16140 B) | G1E-CllCsDDomEmeEtmFoHHfoMMvNOcP | no |  |
| 81 | 0.042 | 0.1 | vs/ps | DrawSky | vs shader_dx12\0x42ed52ed.file record@20960 (5063 B); ps shader_dx12\0x42ed52ed.file record@116240 (7325 B) | VCLOUD | no |  |
| 82 | 0.042 | 0.1 | vs/ps | - | vs shader_dx12\0xd070067d.file record@654333 (2824 B); ps shader_dx12\0xd070067d.file record@698136 (4106 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 83 | 0.034 | 0.1 | vs/ps | - | vs shader_dx12\0x9992f2ab.file record@410536 (32238 B); ps shader_dx12\0x9992f2ab.file record@549252 (6940 B) | SMTREE2-BlrMfvMshltvReSfb | no |  |
| 84 | 0.034 | 0.1 | vs/ps | - | vs shader_dx12\0xdd27eab2.file record@6620 (3866 B); ps shader_dx12\0xc5bc597a.file record@237184 (39920 B) | PB2RIPPLE-Cbbl / PB2DECAL-CdlRgbSfb | no |  |
| 85 | 0.032 | 0.1 | vs/ps | - | vs runtime-compiled (dump); ps runtime-compiled (dump) | post VS / bloom | ps: psoswap file post\out\e6efc8ff49a9b254151466f9835e8fad.dxbc (ORIGINAL key it replaces) | e6efc8ff=gpu-post-runtime.md:102 |
| 86 | 0.030 | 0.1 | cs | FFX_FSR3UPSCALER_PASS_SHADING_CHANGE | cs exe-embedded @rva 0x39f60b0 (.rdata) | fsr/upscale | no |  |
| 87 | 0.029 | 0.1 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x5ad383c1.file record@6173 (30191 B) | WDO_SST-PtSrbVsm / PB2DS-AdmAmbSpecMapAngLimitCllCr | no |  |
| 88 | 0.028 | 0.1 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@41024 (5235 B) | GFM / HBAO | no |  |
| 89 | 0.028 | 0.1 | vs/ps/ds/hs | - | vs shader_dx12\0xdc52fd1c.file record@59224 (10671 B); ps shader_dx12\0xdc52fd1c.file record@10629462 (236183 B); ds shader_dx12\0xdc52fd1c.file record@13507187 (23887 B); hs shader_dx12\0xdc52fd1c.file record@13464058 (9854 B) | PB2LS-GbiSfbVl | no |  |
| 90 | 0.026 | 0.1 | vs/ps | - | vs runtime-compiled (dump); ps runtime-compiled (dump) | DOF VS / dof | ps: psoswap file post\out\cf792d56ae14e49c093cb5f27a66f871.dxbc (ORIGINAL key it replaces) | cf792d56=gpu-post-runtime.md:100 |
| 91 | 0.026 | 0.1 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@170112 (8295 B) | GFM / HBAO | no |  |
| 92 | 0.023 | 0.1 | vs/ps | - | vs shader_dx12\0x0f33242a.file record@45984 (3878 B); ps shader_dx12\0x0f33242a.file record@883211 (10700 B) | PBRLR | no |  |
| 93 | 0.022 | 0.1 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@161652 (8195 B) | GFM / HBAO | no |  |
| 94 | 0.022 | 0.1 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a245c0 (.rdata) | - | no |  |
| 95 | 0.020 | 0.1 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x4921783e.file record@1302588 (12903 B) | WDO_SST-PtSrbVsm / PB2MRLR-CllLasVf | no |  |
| 96 | 0.015 | 0.1 | vs/ps | - | vs shader_dx12\0x5f066893.file record@675428 (8734 B); ps shader_dx12\0x5f066893.file record@9380852 (23056 B) | G1E-CllCsDDomEmeEtmFoHHfoMMvNOcP | no |  |
| 97 | 0.015 | 0.1 | vs | Batch Tiles 8 | vs shader_dx12\0xdd61dd92.file record@661588 (21004 B) | SM-BlrMfvMm00MshltvSfb | no |  |
| 98 | 0.015 | 0.0 | vs | Batch Tiles 8 | vs shader_dx12\0x9992f2ab.file record@476032 (30952 B) | SMTREE2-BlrMfvMshltvReSfb | no |  |
| 99 | 0.015 | 0.0 | vs/ps | - | vs shader_dx12\0x288f69a4.file record@413973 (22713 B); ps shader_dx12\0x288f69a4.file record@1535544 (20330 B) | SRIVER2-GbNRrVl | no |  |
| 100 | 0.014 | 0.0 | vs/ps | - | vs shader_dx12\0x52c39db0.file record@151636 (47739 B); ps shader_dx12\0xebe5ee46.file record@34528 (14345 B) | PB2TREE2-BlrGbiMshltvOcReSfbVl / PB2-AlepmeAtBlrDiGbiMshltOcSfbT0 | no |  |
| 101 | 0.013 | 0.0 | vs/ps | - | vs shader_dx12\0xfd20cb1a.file record@21860 (3033 B); ps shader_dx12\0xd070067d.file record@737753 (8843 B) | SDFOCC-CllSdf / RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 102 | 0.011 | 0.0 | vs/ps | - | vs shader_dx12\0x6cb9f85a.file record@7860 (3598 B); ps shader_dx12\0x6cb9f85a.file record@22684 (6931 B) | SSFOG-CllFdHfMpfS2aVf | no |  |
| 103 | 0.011 | 0.0 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0xa60921c6.file record@1400572 (11539 B) | WDO_SST-PtSrbVsm / PB2MRLR-AmbSpecMapAngLimitCllLas | no |  |
| 104 | 0.011 | 0.0 | vs/ps | - | vs shader_dx12\0xf00d5c8c.file record@298564 (29866 B); ps shader_dx12\0xf00d5c8c.file record@1324884 (26616 B) | PB2-BlrGbiMshltvReSfbVl | no |  |
| 105 | 0.009 | 0.0 | vs/ps | - | vs shader_dx12\0x52c39db0.file record@151636 (47739 B); ps shader_dx12\0xd864e310.file record@2472 (39516 B) | PB2TREE2-BlrGbiMshltvOcReSfbVl / PB2TREE2-BlrDimGbiMshltOcPfnPkSf | no |  |
| 106 | 0.009 | 0.0 | cs | - | cs shader_dx12\0xd070067d.file record@13687854 (47628 B) | RefProbe-CllPtSfbSplsSslsSsmVsm | no |  |
| 107 | 0.008 | 0.0 | vs/ps | - | vs runtime-compiled (dump); ps runtime-compiled (dump) | - | no |  |
| 108 | 0.008 | 0.0 | vs/ps | - | vs shader_dx12\0xf00d5c8c.file record@298564 (29866 B); ps shader_dx12\0xdf8c9879.file record@28056 (11084 B) | PB2-BlrGbiMshltvReSfbVl / PB2-BlrGbiMshltPkSfbT0Vl | no |  |
| 109 | 0.007 | 0.0 | vs/ps | - | vs runtime-compiled (dump); ps runtime-compiled (dump) | motion blur | ps: psoswap file post\out\4e13e236319c9df7c81203eca95c4df4.dxbc (ORIGINAL key it replaces) | 4e13e236=gpu-post-runtime.md:99 |
| 110 | 0.007 | 0.0 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a56590 (.rdata) | - | no |  |
| 111 | 0.006 | 0.0 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a3c6e0 (.rdata) | - | no |  |
| 112 | 0.006 | 0.0 | vs/ps | - | vs shader_dx12\0x0f33242a.file record@49862 (3661 B); ps shader_dx12\0x0f33242a.file record@182033 (4991 B) | PBRLR | no |  |
| 113 | 0.005 | 0.0 | vs/ps | - | vs shader_dx12\0x0f33242a.file record@49862 (3661 B); ps shader_dx12\0x0f33242a.file record@187024 (4991 B) | PBRLR | no |  |
| 114 | 0.005 | 0.0 | vs/ps | - | vs shader_dx12\0xec36dc74.file record@2288 (3041 B); ps shader_dx12\0x3de2e18a.file record@46428 (6995 B) | GFM / HBAO | no |  |
| 115 | 0.005 | 0.0 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3ff2680 (.rdata) | - | no |  |
| 116 | 0.005 | 0.0 | vs/ps | - | vs exe-embedded @rva 0x399ee90 (.rdata); ps exe-embedded @rva 0x3a2daa0 (.rdata) | - | no |  |
| 117 | 0.004 | 0.0 | vs/ps | - | vs shader_dx12\0xee5a444b.file record@41752 (3033 B); ps shader_dx12\0x80ec5766.file record@6200 (27171 B) | WDO_SST-PtSrbVsm / PB2DS-AdmCaCllEmHsIribOfPlaSmSpl | no |  |
| 118 | 0.004 | 0.0 | vs/ps/ds/hs | - | vs shader_dx12\0xdc52fd1c.file record@59224 (10671 B); ps shader_dx12\0xdc52fd1c.file record@3778660 (389243 B); ds shader_dx12\0xdc52fd1c.file record@13507187 (23887 B); hs shader_dx12\0xdc52fd1c.file record@13464058 (9854 B) | PB2LS-GbiSfbVl | no |  |
| 119 | 0.003 | 0.0 | cs | - | cs exe-embedded @rva 0x3a340f0 (.rdata) | - | no |  |
| 120 | 0.002 | 0.0 | - | - | - | - | no |  |

Per-PSO numbers are GPU-pipelined intervals (ranking, not isolated cost). Hashes:

1. `cs=e73868403411fc81d6c238cf52f018c8`
2. `cs=4d6740bf811e14127bc9ab8cefc885a5 (async compute)`
3. `cs=8b86a434076ec26bd7fb223ccd1b2374`
4. `cs=3afc8fd74d54ef75ec0a3f70482123ab`
5. `cs=756aff8f97f1ffb1e7919785f5789ac0`
6. `vs=70d0ee9feee590f9c48c40b808858e27 ps=db2a9be26f6c7405af29bb9bffca1e9b`
7. `cs=3010e631808d16089228a3684b3fcfb8`
8. `(copy / resolve / clear)`
9. `cs=6ae59bd0ddf1220dc8907954b1f215b4`
10. `cs=b76874bb17bdfc10a0739b74191c68a9 (async compute)`
11. `vs=26bcdc9dad88df680cc28cc877c71093 ps=2ec48e3f381677b6e3ad0aa136cb7b24`
12. `cs=8e8e36dc91c2d2713dd500af0566dbff`
13. `vs=355ad1ce80cc96fe9e4c58ebb41b71ec ps=98683c8f6b0f7e2390e84767823fefff`
14. `vs=26bcdc9dad88df680cc28cc877c71093 ps=0b17198d656715ff449183941912bea1`
15. `vs=563edb9d654bec6875beb60220e6bf6e ps=c013d8118112cb209196fc7ba5d2f92d`
