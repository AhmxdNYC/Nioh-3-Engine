;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
; shader debug name: 02636fe088cc3c5674266f81e9337747.pdb
; shader hash: 02636fe088cc3c5674266f81e9337747
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Compute Shader
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 0
; SigOutputElements: 0
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 0
; SigOutputVectors[0]: 0
; SigOutputVectors[1]: 0
; SigOutputVectors[2]: 0
; SigOutputVectors[3]: 0
;
;
; Buffer Definitions:
;
; cbuffer $Globals
; {
;
;   struct dx.alignment.legacy.$Globals
;   {
;
;       float4 vEye;                                  ; Offset:    0
;       int nClipLevelMin;                            ; Offset:   16
;       int nClstPtch[2];                             ; Offset:   32
;       float4 vClstZPrm;                             ; Offset:   64
;       float4 vCllPkPrm[2];                          ; Offset:   80
;       row_major float4x4 mTrsW2LPV[4];              ; Offset:  112
;       row_major float4x4 mTrsW2LPVSR[4];            ; Offset:  368
;       int nLPV3DDepth;                              ; Offset:  624
;       float fDiffuseScale;                          ; Offset:  628
;       float fNormalDir;                             ; Offset:  632
;       float4 fSHBlendRegion;                        ; Offset:  640
;       int nArraySize_VirtualSM;                     ; Offset:  656
;       float4 scMinMipMapVirtualSM;                  ; Offset:  672
;       row_major float4x4 mW2S_VirtualSM[16];        ; Offset:  688
;       float2 vClipOriginOffset[16];                 ; Offset: 1712
;       int nValidClipmapFlag;                        ; Offset: 1960
;       int nClipmapLevelBias;                        ; Offset: 1964
;       float4 g_vScrnSize;                           ; Offset: 1968
;       float3 g_vFltOrgOfs;                          ; Offset: 1984
;   
;   } $Globals;                                       ; Offset:    0 Size:  1996
;
; }
;
; cbuffer VolumetricFogConstBuf
; {
;
;   struct dx.alignment.legacy.VolumetricFogConstBuf
;   {
;
;       row_major float4x4 gV2W;                      ; Offset:    0
;       row_major float4x4 gPrevW2P;                  ; Offset:   64
;       float4 gAngleAndNearFar;                      ; Offset:  128
;       float4 gCalcVolumeFactor;                     ; Offset:  144
;       float4 gEyePosAndFogFar;                      ; Offset:  160
;       float4 gInjectionDesc;                        ; Offset:  176
;       int4 gLightInfo;                              ; Offset:  192
;       float4 gFogNoiseInfo;                         ; Offset:  208
;       float4 gScatteringInfo;                       ; Offset:  224
;       float4 gAccParams;                            ; Offset:  240
;       float4 gAttParam;                             ; Offset:  256
;       row_major float4x4 gW2P;                      ; Offset:  272
;   
;   } VolumetricFogConstBuf;                          ; Offset:    0 Size:   336
;
; }
;
; Resource bind info for sParaLightInfo
; {
;
;   struct dx.alignment.legacy.struct.slVFParallelLightInfo
;   {
;
;       struct struct.slVFCommonLightInfo
;       {
;
;           float4 vMapDesc;                          ; Offset:    0
;           float4 vColorScale;                       ; Offset:   16
;           float4 vEcc;                              ; Offset:   32
;       
;       } sCommon;                                    ; Offset:    0
;
;       struct struct.slSVFParallelLight
;       {
;
;           float4 vDir;                              ; Offset:   48
;           float4 vDiffuse;                          ; Offset:   64
;       
;       } sLightInfo;                                 ; Offset:   48
;
;       float4 vAttr;                                 ; Offset:   80
;       row_major float4x4 mW2S;                      ; Offset:   96
;       row_major float4x4 mW2S2;                     ; Offset:  160
;       row_major float4x4 mW2S3;                     ; Offset:  224
;       row_major float4x4 mW2S4;                     ; Offset:  288
;       float4 vSegs;                                 ; Offset:  352
;       row_major float4x4 mUVS;                      ; Offset:  368
;   
;   } $Element;                                       ; Offset:    0 Size:   432
;
; }
;
; Resource bind info for sPointLightInfo
; {
;
;   struct struct.slVFPointBasicLightInfo
;   {
;
;       struct struct.slVFCommonLightInfo
;       {
;
;           float4 vMapDesc;                          ; Offset:    0
;           float4 vColorScale;                       ; Offset:   16
;           float4 vEcc;                              ; Offset:   32
;       
;       } sCommon;                                    ; Offset:    0
;
;       float4 vW2S;                                  ; Offset:   48
;   
;   } $Element;                                       ; Offset:    0 Size:    64
;
; }
;
; Resource bind info for sSpotLightInfo
; {
;
;   struct dx.alignment.legacy.struct.slVFSpotBasicLightInfo
;   {
;
;       struct struct.slVFCommonLightInfo
;       {
;
;           float4 vMapDesc;                          ; Offset:    0
;           float4 vColorScale;                       ; Offset:   16
;           float4 vEcc;                              ; Offset:   32
;       
;       } sCommon;                                    ; Offset:    0
;
;       row_major float4x4 vW2S;                      ; Offset:   48
;   
;   } $Element;                                       ; Offset:    0 Size:   112
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; VolumetricFogConstBuf             cbuffer      NA          NA     CB1            cb1     1
; __smpsIES                         sampler      NA          NA      S0             s0     1
; __smpsLPVBuffer                   sampler      NA          NA      S1             s1     1
; __smpsVirtualShadow               sampler      NA          NA      S2             s2     1
; __smpsPermutation                 sampler      NA          NA      S3             s8     1
; __smpsVolumeSrc                   sampler      NA          NA      S4             s9     1
; __smpsParallelShadowMap           sampler      NA          NA      S5            s10     1
; __smpsSpotShadowMap               sampler      NA          NA      S6            s12     1
; __smpsPointShadowMapArray         sampler      NA          NA      S7            s13     1
; sIES                              texture     f32     2darray      T0             t0     1
; tCllLightIndices                  texture     u32         buf      T1            t16     1
; tCllLightPositions                texture     f32         buf      T2            t17     1
; tCllLightAttributes               texture     f32         buf      T3            t18     1
; tCllLightExAttributes             texture     u32         buf      T4            t19     1
; tCllSptLightAttributes            texture     f32         buf      T5            t20     1
; tCllSptLightExAttributes          texture     u32         buf      T6            t21     1
; sLPVBuffer                        texture     f32     2darray      T7             t1     1
; sVirtualShadow                    texture     f32     2darray      T8             t2     1
; tVirtualShadowMapFlags            texture     u32         buf      T9            t32     1
; sPermutation                      texture     f32          2d     T10             t8     1
; sVolumeSrc                        texture     f32          3d     T11             t9     1
; sParallelShadowMap                texture     f32          2d     T12            t10     1
; sSpotShadowMap                    texture     f32     2darray     T13            t12     1
; sPointShadowMapArray              texture     f32   cubearray     T14            t13     1
; sParaLightInfo                    texture  struct         r/o     T15            t25     1
; sPointLightInfo                   texture  struct         r/o     T16            t26     1
; sSpotLightInfo                    texture  struct         r/o     T17            t27     1
; sPointScatterAttr                 texture     f32         buf     T18            t28     1
; sSpotScatterAttr                  texture     f32         buf     T19            t29     1
; uavTarget                             UAV     f32          3d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.Dimensions = type { i32, i32, i32, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"class.Buffer<unsigned int>" = type { i32 }
%"class.Buffer<vector<float, 4> >" = type { <4 x float> }
%"class.Buffer<vector<float, 3> >" = type { <3 x float> }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture3D<vector<float, 4> >" = type { <4 x float>, %"class.Texture3D<vector<float, 4> >::mips_type" }
%"class.Texture3D<vector<float, 4> >::mips_type" = type { i32 }
%"class.TextureCubeArray<vector<float, 4> >" = type { <4 x float> }
%"dx.alignment.legacy.class.StructuredBuffer<slVFParallelLightInfo>" = type { %dx.alignment.legacy.struct.slVFParallelLightInfo }
%dx.alignment.legacy.struct.slVFParallelLightInfo = type { %struct.slVFCommonLightInfo, %struct.slSVFParallelLight, <4 x float>, [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <4 x float>, [4 x <4 x float>] }
%struct.slVFCommonLightInfo = type { <4 x float>, <4 x float>, <4 x float> }
%struct.slSVFParallelLight = type { <4 x float>, <4 x float> }
%"class.StructuredBuffer<slVFPointBasicLightInfo>" = type { %struct.slVFPointBasicLightInfo }
%struct.slVFPointBasicLightInfo = type { %struct.slVFCommonLightInfo, <4 x float> }
%"dx.alignment.legacy.class.StructuredBuffer<slVFSpotBasicLightInfo>" = type { %dx.alignment.legacy.struct.slVFSpotBasicLightInfo }
%dx.alignment.legacy.struct.slVFSpotBasicLightInfo = type { %struct.slVFCommonLightInfo, [4 x <4 x float>] }
%"class.Buffer<float>" = type { float }
%"class.RWTexture3D<vector<float, 4> >" = type { <4 x float> }
%"dx.alignment.legacy.$Globals" = type { <4 x float>, i32, [2 x i32], <4 x float>, [2 x <4 x float>], [4 x [4 x <4 x float>]], [4 x [4 x <4 x float>]], i32, float, float, <4 x float>, i32, <4 x float>, [16 x [4 x <4 x float>]], [16 x <2 x float>], i32, i32, <4 x float>, <3 x float> }
%dx.alignment.legacy.VolumetricFogConstBuf = type { [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x i32>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, [4 x <4 x float>] }
%struct.SamplerState = type { i32 }
%struct.SamplerComparisonState = type { i32 }

@"\01?ldsAppliedLightIndices@@3PAHA" = external addrspace(3) global [4096 x i32], align 4
@POISSON_SAMPLE.0 = internal unnamed_addr constant [16 x float] [float 0x3FE9B951C0000000, float 0x3FD470E2C0000000, float 0x3FEA8E8A80000000, float 0x3FD50157E0000000, float 0x3FE1EFE500000000, float 0x3FB0A94780000000, float 0x3FE21CC540000000, float 0x3FB28815E0000000, float 0x3FEA560EA0000000, float 0x3FB39C4120000000, float 0x3FB18A7620000000, float 0x3FE1FD3920000000, float 0x3FD52EF4E0000000, float 0x3FD3B12900000000, float 0x3FE9AF5340000000, float 0x3FE3616500000000]
@POISSON_SAMPLE.1 = internal unnamed_addr constant [16 x float] [float 0x3FC341B760000000, float 0x3FE5B45D00000000, float 0x3FE46A5F80000000, float 0x3FC3B445E0000000, float 0x3FDBBDE400000000, float 0x3FEC696600000000, float 0x3FEC7253E0000000, float 0x3FD7C9DE00000000, float 0x3FED82D380000000, float 0x3FE3F01DA0000000, float 0x3FC0634120000000, float 0x3FC4CD0FE0000000, float 0x3FD9C1BDA0000000, float 0x3FED04DFC0000000, float 0x3FD917B100000000, float 0x3FE50531A0000000]

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 19, i32 29, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 18, i32 28, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 17, i32 27, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 16, i32 26, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 15, i32 25, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 14, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 13, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 12, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 11, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 10, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 9, i32 32, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 8, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 21, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 20, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %17 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 19, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %18 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 18, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %19 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 17, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %20 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 16, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %21 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %22 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 7, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %23 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 6, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %24 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 5, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %25 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 4, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %26 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %27 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %28 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %29 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %30 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %31 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %32 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %33 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %34 = call i32 @dx.op.threadId.i32(i32 93, i32 2)  ; ThreadId(component)
  %35 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  %36 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %37 = extractvalue %dx.types.CBufRet.f32 %36, 3
  %38 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %1, i32 0)  ; GetDimensions(handle,mipLevel)
  %39 = extractvalue %dx.types.Dimensions %38, 0
  %40 = extractvalue %dx.types.Dimensions %38, 1
  %41 = extractvalue %dx.types.Dimensions %38, 2
  %42 = uitofp i32 %32 to float
  %43 = uitofp i32 %33 to float
  %44 = uitofp i32 %34 to float
  %45 = fadd fast float %42, 5.000000e-01
  %46 = fadd fast float %43, 5.000000e-01
  %47 = fadd fast float %44, 5.000000e-01
  %48 = uitofp i32 %39 to float
  %49 = uitofp i32 %40 to float
  %50 = uitofp i32 %41 to float
  %51 = fdiv fast float %45, %48
  %52 = fdiv fast float %46, %49
  %53 = fdiv fast float %47, %50
  %54 = fmul fast float %51, 2.000000e+00
  %55 = fmul fast float %52, 2.000000e+00
  %56 = fadd fast float %54, -1.000000e+00
  %57 = fsub fast float 1.000000e+00, %55
  %58 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %59 = extractvalue %dx.types.CBufRet.f32 %58, 2
  %60 = fcmp fast ogt float %59, 0.000000e+00
  %61 = extractvalue %dx.types.CBufRet.f32 %58, 0
  %62 = fcmp fast ogt float %61, 0.000000e+00
  %63 = and i1 %60, %62
  br i1 %63, label %64, label %89

; <label>:64                                      ; preds = %0
  %65 = shl i32 %33, 1
  %66 = uitofp i32 %65 to float
  %67 = fmul fast float %61, %44
  %68 = fadd fast float %66, %42
  %69 = fadd fast float %68, %61
  %70 = fadd fast float %69, %67
  %71 = fptoui float %70 to i32
  %72 = and i32 %71, 15
  %73 = getelementptr [16 x float], [16 x float]* @POISSON_SAMPLE.0, i32 0, i32 %72
  %74 = getelementptr [16 x float], [16 x float]* @POISSON_SAMPLE.1, i32 0, i32 %72
  %75 = load float, float* %73, align 4
  %76 = load float, float* %74, align 4
  %77 = fadd fast float %75, -5.000000e-01
  %78 = fadd fast float %76, -5.000000e-01
  %79 = fdiv fast float %77, %48
  %80 = fdiv fast float %78, %49
  %81 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %82 = extractvalue %dx.types.CBufRet.f32 %81, 2
  %83 = fmul fast float %79, 2.000000e+00
  %84 = fmul fast float %83, %82
  %85 = fadd fast float %84, %56
  %86 = fmul fast float %80, 2.000000e+00
  %87 = fmul fast float %86, %82
  %88 = fadd fast float %87, %57
  br label %89

; <label>:89                                      ; preds = %64, %0
  %90 = phi float [ %85, %64 ], [ %56, %0 ]
  %91 = phi float [ %88, %64 ], [ %57, %0 ]
  %92 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %93 = extractvalue %dx.types.CBufRet.f32 %92, 3
  %94 = fmul fast float %53, 0x3FF7154760000000
  %95 = fmul fast float %94, %93
  %96 = call float @dx.op.unary.f32(i32 21, float %95)  ; Exp(value)
  %97 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %98 = extractvalue %dx.types.CBufRet.f32 %97, 2
  %99 = fmul fast float %98, %96
  %100 = fmul fast float %90, 5.000000e-01
  %101 = fmul fast float %91, 5.000000e-01
  %102 = fadd fast float %100, 5.000000e-01
  %103 = fsub fast float 5.000000e-01, %101
  %104 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 123)  ; CBufferLoadLegacy(handle,regIndex)
  %105 = extractvalue %dx.types.CBufRet.f32 %104, 0
  %106 = extractvalue %dx.types.CBufRet.f32 %104, 1
  %107 = fmul fast float %105, %102
  %108 = fmul fast float %106, %103
  %109 = fsub fast float -0.000000e+00, %99
  %110 = extractvalue %dx.types.CBufRet.f32 %97, 0
  %111 = extractvalue %dx.types.CBufRet.f32 %97, 1
  %112 = fmul fast float %110, %90
  %113 = fmul fast float %111, %91
  %114 = fmul fast float %112, %99
  %115 = fmul fast float %113, %99
  %116 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %117 = extractvalue %dx.types.CBufRet.f32 %116, 0
  %118 = extractvalue %dx.types.CBufRet.f32 %116, 1
  %119 = extractvalue %dx.types.CBufRet.f32 %116, 2
  %120 = extractvalue %dx.types.CBufRet.f32 %116, 3
  %121 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %122 = extractvalue %dx.types.CBufRet.f32 %121, 0
  %123 = extractvalue %dx.types.CBufRet.f32 %121, 1
  %124 = extractvalue %dx.types.CBufRet.f32 %121, 2
  %125 = extractvalue %dx.types.CBufRet.f32 %121, 3
  %126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %127 = extractvalue %dx.types.CBufRet.f32 %126, 0
  %128 = extractvalue %dx.types.CBufRet.f32 %126, 1
  %129 = extractvalue %dx.types.CBufRet.f32 %126, 2
  %130 = extractvalue %dx.types.CBufRet.f32 %126, 3
  %131 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %132 = extractvalue %dx.types.CBufRet.f32 %131, 0
  %133 = extractvalue %dx.types.CBufRet.f32 %131, 1
  %134 = extractvalue %dx.types.CBufRet.f32 %131, 2
  %135 = extractvalue %dx.types.CBufRet.f32 %131, 3
  %136 = fmul fast float %114, %117
  %137 = call float @dx.op.tertiary.f32(i32 46, float %115, float %122, float %136)  ; FMad(a,b,c)
  %138 = call float @dx.op.tertiary.f32(i32 46, float %109, float %127, float %137)  ; FMad(a,b,c)
  %139 = fadd fast float %138, %132
  %140 = fmul fast float %114, %118
  %141 = call float @dx.op.tertiary.f32(i32 46, float %115, float %123, float %140)  ; FMad(a,b,c)
  %142 = call float @dx.op.tertiary.f32(i32 46, float %109, float %128, float %141)  ; FMad(a,b,c)
  %143 = fadd fast float %142, %133
  %144 = fmul fast float %114, %119
  %145 = call float @dx.op.tertiary.f32(i32 46, float %115, float %124, float %144)  ; FMad(a,b,c)
  %146 = call float @dx.op.tertiary.f32(i32 46, float %109, float %129, float %145)  ; FMad(a,b,c)
  %147 = fadd fast float %146, %134
  %148 = fmul fast float %114, %120
  %149 = call float @dx.op.tertiary.f32(i32 46, float %115, float %125, float %148)  ; FMad(a,b,c)
  %150 = call float @dx.op.tertiary.f32(i32 46, float %109, float %130, float %149)  ; FMad(a,b,c)
  %151 = fadd fast float %150, %135
  %152 = fmul fast float %110, %56
  %153 = fmul fast float %111, %57
  %154 = fmul fast float %152, %99
  %155 = fmul fast float %153, %99
  %156 = fmul fast float %154, %117
  %157 = call float @dx.op.tertiary.f32(i32 46, float %155, float %122, float %156)  ; FMad(a,b,c)
  %158 = call float @dx.op.tertiary.f32(i32 46, float %109, float %127, float %157)  ; FMad(a,b,c)
  %159 = fadd fast float %158, %132
  %160 = fmul fast float %154, %118
  %161 = call float @dx.op.tertiary.f32(i32 46, float %155, float %123, float %160)  ; FMad(a,b,c)
  %162 = call float @dx.op.tertiary.f32(i32 46, float %109, float %128, float %161)  ; FMad(a,b,c)
  %163 = fadd fast float %162, %133
  %164 = fmul fast float %154, %119
  %165 = call float @dx.op.tertiary.f32(i32 46, float %155, float %124, float %164)  ; FMad(a,b,c)
  %166 = call float @dx.op.tertiary.f32(i32 46, float %109, float %129, float %165)  ; FMad(a,b,c)
  %167 = fadd fast float %166, %134
  %168 = fmul fast float %154, %120
  %169 = call float @dx.op.tertiary.f32(i32 46, float %155, float %125, float %168)  ; FMad(a,b,c)
  %170 = call float @dx.op.tertiary.f32(i32 46, float %109, float %130, float %169)  ; FMad(a,b,c)
  %171 = fadd fast float %170, %135
  %172 = fadd fast float %44, 1.000000e+00
  %173 = fdiv fast float %172, %50
  %174 = fmul fast float %173, 0x3FF7154760000000
  %175 = fmul fast float %174, %93
  %176 = call float @dx.op.unary.f32(i32 21, float %175)  ; Exp(value)
  %177 = fmul fast float %98, %176
  %178 = fsub fast float -0.000000e+00, %177
  %179 = fmul fast float %112, %177
  %180 = fmul fast float %113, %177
  %181 = fmul fast float %179, %117
  %182 = call float @dx.op.tertiary.f32(i32 46, float %180, float %122, float %181)  ; FMad(a,b,c)
  %183 = call float @dx.op.tertiary.f32(i32 46, float %178, float %127, float %182)  ; FMad(a,b,c)
  %184 = fadd fast float %183, %132
  %185 = fmul fast float %179, %118
  %186 = call float @dx.op.tertiary.f32(i32 46, float %180, float %123, float %185)  ; FMad(a,b,c)
  %187 = call float @dx.op.tertiary.f32(i32 46, float %178, float %128, float %186)  ; FMad(a,b,c)
  %188 = fadd fast float %187, %133
  %189 = fmul fast float %179, %119
  %190 = call float @dx.op.tertiary.f32(i32 46, float %180, float %124, float %189)  ; FMad(a,b,c)
  %191 = call float @dx.op.tertiary.f32(i32 46, float %178, float %129, float %190)  ; FMad(a,b,c)
  %192 = fadd fast float %191, %134
  %193 = fmul fast float %179, %120
  %194 = call float @dx.op.tertiary.f32(i32 46, float %180, float %125, float %193)  ; FMad(a,b,c)
  %195 = call float @dx.op.tertiary.f32(i32 46, float %178, float %130, float %194)  ; FMad(a,b,c)
  %196 = fadd fast float %195, %135
  %197 = fdiv fast float %44, %50
  %198 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %199 = extractvalue %dx.types.CBufRet.f32 %198, 3
  %200 = fmul fast float %197, 0x3FF7154760000000
  %201 = fmul fast float %200, %199
  %202 = call float @dx.op.unary.f32(i32 21, float %201)  ; Exp(value)
  %203 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %204 = extractvalue %dx.types.CBufRet.f32 %203, 2
  %205 = fmul fast float %204, %202
  %206 = fsub fast float -0.000000e+00, %205
  %207 = extractvalue %dx.types.CBufRet.f32 %203, 0
  %208 = extractvalue %dx.types.CBufRet.f32 %203, 1
  %209 = fmul fast float %207, %90
  %210 = fmul fast float %208, %91
  %211 = fmul fast float %209, %205
  %212 = fmul fast float %210, %205
  %213 = fmul fast float %211, %117
  %214 = call float @dx.op.tertiary.f32(i32 46, float %212, float %122, float %213)  ; FMad(a,b,c)
  %215 = call float @dx.op.tertiary.f32(i32 46, float %206, float %127, float %214)  ; FMad(a,b,c)
  %216 = fadd fast float %215, %132
  %217 = fmul fast float %211, %118
  %218 = call float @dx.op.tertiary.f32(i32 46, float %212, float %123, float %217)  ; FMad(a,b,c)
  %219 = call float @dx.op.tertiary.f32(i32 46, float %206, float %128, float %218)  ; FMad(a,b,c)
  %220 = fadd fast float %219, %133
  %221 = fmul fast float %211, %119
  %222 = call float @dx.op.tertiary.f32(i32 46, float %212, float %124, float %221)  ; FMad(a,b,c)
  %223 = call float @dx.op.tertiary.f32(i32 46, float %206, float %129, float %222)  ; FMad(a,b,c)
  %224 = fadd fast float %223, %134
  %225 = fmul fast float %211, %120
  %226 = call float @dx.op.tertiary.f32(i32 46, float %212, float %125, float %225)  ; FMad(a,b,c)
  %227 = call float @dx.op.tertiary.f32(i32 46, float %206, float %130, float %226)  ; FMad(a,b,c)
  %228 = fadd fast float %227, %135
  %229 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %230 = extractvalue %dx.types.CBufRet.f32 %229, 2
  %231 = extractvalue %dx.types.CBufRet.f32 %229, 3
  %232 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %233 = extractvalue %dx.types.CBufRet.f32 %232, 2
  %234 = extractvalue %dx.types.CBufRet.f32 %232, 3
  %235 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %236 = extractvalue %dx.types.CBufRet.f32 %235, 2
  %237 = extractvalue %dx.types.CBufRet.f32 %235, 3
  %238 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %239 = extractvalue %dx.types.CBufRet.f32 %238, 2
  %240 = extractvalue %dx.types.CBufRet.f32 %238, 3
  %241 = fmul fast float %230, %139
  %242 = call float @dx.op.tertiary.f32(i32 46, float %143, float %233, float %241)  ; FMad(a,b,c)
  %243 = call float @dx.op.tertiary.f32(i32 46, float %147, float %236, float %242)  ; FMad(a,b,c)
  %244 = call float @dx.op.tertiary.f32(i32 46, float %151, float %239, float %243)  ; FMad(a,b,c)
  %245 = fmul fast float %231, %139
  %246 = call float @dx.op.tertiary.f32(i32 46, float %143, float %234, float %245)  ; FMad(a,b,c)
  %247 = call float @dx.op.tertiary.f32(i32 46, float %147, float %237, float %246)  ; FMad(a,b,c)
  %248 = call float @dx.op.tertiary.f32(i32 46, float %151, float %240, float %247)  ; FMad(a,b,c)
  %249 = fdiv fast float %244, %248
  %250 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %251 = extractvalue %dx.types.CBufRet.f32 %250, 0
  %252 = extractvalue %dx.types.CBufRet.f32 %250, 1
  %253 = extractvalue %dx.types.CBufRet.f32 %250, 2
  %254 = fsub fast float %139, %251
  %255 = fsub fast float %143, %252
  %256 = fsub fast float %147, %253
  %257 = call float @dx.op.dot3.f32(i32 55, float %254, float %255, float %256, float %254, float %255, float %256)  ; Dot3(ax,ay,az,bx,by,bz)
  %258 = call float @dx.op.unary.f32(i32 25, float %257)  ; Rsqrt(value)
  %259 = fmul fast float %254, %258
  %260 = fmul fast float %255, %258
  %261 = fmul fast float %256, %258
  %262 = fadd fast float %44, 0x3FC5604180000000
  %263 = fdiv fast float %262, %50
  %264 = fmul fast float %263, 0x3FF7154760000000
  %265 = fmul fast float %264, %199
  %266 = call float @dx.op.unary.f32(i32 21, float %265)  ; Exp(value)
  %267 = fmul fast float %204, %266
  %268 = fsub fast float -0.000000e+00, %267
  %269 = fmul fast float %207, %56
  %270 = fmul fast float %208, %57
  %271 = fmul fast float %269, %267
  %272 = fmul fast float %270, %267
  %273 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %274 = extractvalue %dx.types.CBufRet.f32 %273, 0
  %275 = extractvalue %dx.types.CBufRet.f32 %273, 1
  %276 = extractvalue %dx.types.CBufRet.f32 %273, 2
  %277 = extractvalue %dx.types.CBufRet.f32 %273, 3
  %278 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %279 = extractvalue %dx.types.CBufRet.f32 %278, 0
  %280 = extractvalue %dx.types.CBufRet.f32 %278, 1
  %281 = extractvalue %dx.types.CBufRet.f32 %278, 2
  %282 = extractvalue %dx.types.CBufRet.f32 %278, 3
  %283 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %284 = extractvalue %dx.types.CBufRet.f32 %283, 0
  %285 = extractvalue %dx.types.CBufRet.f32 %283, 1
  %286 = extractvalue %dx.types.CBufRet.f32 %283, 2
  %287 = extractvalue %dx.types.CBufRet.f32 %283, 3
  %288 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %289 = extractvalue %dx.types.CBufRet.f32 %288, 0
  %290 = extractvalue %dx.types.CBufRet.f32 %288, 1
  %291 = extractvalue %dx.types.CBufRet.f32 %288, 2
  %292 = extractvalue %dx.types.CBufRet.f32 %288, 3
  %293 = fmul fast float %271, %274
  %294 = call float @dx.op.tertiary.f32(i32 46, float %272, float %279, float %293)  ; FMad(a,b,c)
  %295 = call float @dx.op.tertiary.f32(i32 46, float %268, float %284, float %294)  ; FMad(a,b,c)
  %296 = fadd fast float %295, %289
  %297 = fmul fast float %271, %275
  %298 = call float @dx.op.tertiary.f32(i32 46, float %272, float %280, float %297)  ; FMad(a,b,c)
  %299 = call float @dx.op.tertiary.f32(i32 46, float %268, float %285, float %298)  ; FMad(a,b,c)
  %300 = fadd fast float %299, %290
  %301 = fmul fast float %271, %276
  %302 = call float @dx.op.tertiary.f32(i32 46, float %272, float %281, float %301)  ; FMad(a,b,c)
  %303 = call float @dx.op.tertiary.f32(i32 46, float %268, float %286, float %302)  ; FMad(a,b,c)
  %304 = fadd fast float %303, %291
  %305 = fmul fast float %271, %277
  %306 = call float @dx.op.tertiary.f32(i32 46, float %272, float %282, float %305)  ; FMad(a,b,c)
  %307 = call float @dx.op.tertiary.f32(i32 46, float %268, float %287, float %306)  ; FMad(a,b,c)
  %308 = fadd fast float %307, %292
  %309 = fadd fast float %44, 0x3FEAA7EFA0000000
  %310 = fdiv fast float %309, %50
  %311 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %312 = extractvalue %dx.types.CBufRet.f32 %311, 3
  %313 = fmul fast float %310, 0x3FF7154760000000
  %314 = fmul fast float %313, %312
  %315 = call float @dx.op.unary.f32(i32 21, float %314)  ; Exp(value)
  %316 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %317 = extractvalue %dx.types.CBufRet.f32 %316, 2
  %318 = fmul fast float %317, %315
  %319 = fsub fast float -0.000000e+00, %318
  %320 = extractvalue %dx.types.CBufRet.f32 %316, 0
  %321 = extractvalue %dx.types.CBufRet.f32 %316, 1
  %322 = fmul fast float %320, %56
  %323 = fmul fast float %321, %57
  %324 = fmul fast float %322, %318
  %325 = fmul fast float %323, %318
  %326 = fmul fast float %324, %274
  %327 = call float @dx.op.tertiary.f32(i32 46, float %325, float %279, float %326)  ; FMad(a,b,c)
  %328 = call float @dx.op.tertiary.f32(i32 46, float %319, float %284, float %327)  ; FMad(a,b,c)
  %329 = fadd fast float %328, %289
  %330 = fmul fast float %324, %275
  %331 = call float @dx.op.tertiary.f32(i32 46, float %325, float %280, float %330)  ; FMad(a,b,c)
  %332 = call float @dx.op.tertiary.f32(i32 46, float %319, float %285, float %331)  ; FMad(a,b,c)
  %333 = fadd fast float %332, %290
  %334 = fmul fast float %324, %276
  %335 = call float @dx.op.tertiary.f32(i32 46, float %325, float %281, float %334)  ; FMad(a,b,c)
  %336 = call float @dx.op.tertiary.f32(i32 46, float %319, float %286, float %335)  ; FMad(a,b,c)
  %337 = fadd fast float %336, %291
  %338 = fmul fast float %324, %277
  %339 = call float @dx.op.tertiary.f32(i32 46, float %325, float %282, float %338)  ; FMad(a,b,c)
  %340 = call float @dx.op.tertiary.f32(i32 46, float %319, float %287, float %339)  ; FMad(a,b,c)
  %341 = fadd fast float %340, %292
  %342 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %343 = extractvalue %dx.types.CBufRet.i32 %342, 0
  %344 = icmp eq i32 %343, 0
  br i1 %344, label %1290, label %345

; <label>:345                                     ; preds = %89
  %346 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 41)  ; CBufferLoadLegacy(handle,regIndex)
  %347 = extractvalue %dx.types.CBufRet.i32 %346, 0
  br label %348

; <label>:348                                     ; preds = %1274, %345
  %349 = phi float [ %1282, %1274 ], [ 0.000000e+00, %345 ]
  %350 = phi float [ %1283, %1274 ], [ 0.000000e+00, %345 ]
  %351 = phi float [ %1284, %1274 ], [ 0.000000e+00, %345 ]
  %352 = phi i32 [ %1281, %1274 ], [ %347, %345 ]
  %353 = phi float [ %1278, %1274 ], [ 0.000000e+00, %345 ]
  %354 = phi float [ %1279, %1274 ], [ 0.000000e+00, %345 ]
  %355 = phi float [ %1280, %1274 ], [ 0.000000e+00, %345 ]
  %356 = phi i32 [ %1285, %1274 ], [ 0, %345 ]
  %357 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 0)  ; BufferLoad(srv,index,wot)
  %358 = extractvalue %dx.types.ResRet.f32 %357, 0
  %359 = extractvalue %dx.types.ResRet.f32 %357, 2
  %360 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 16)  ; BufferLoad(srv,index,wot)
  %361 = extractvalue %dx.types.ResRet.f32 %360, 0
  %362 = extractvalue %dx.types.ResRet.f32 %360, 1
  %363 = extractvalue %dx.types.ResRet.f32 %360, 2
  %364 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 32)  ; BufferLoad(srv,index,wot)
  %365 = extractvalue %dx.types.ResRet.f32 %364, 0
  %366 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 48)  ; BufferLoad(srv,index,wot)
  %367 = extractvalue %dx.types.ResRet.f32 %366, 0
  %368 = extractvalue %dx.types.ResRet.f32 %366, 1
  %369 = extractvalue %dx.types.ResRet.f32 %366, 2
  %370 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 64)  ; BufferLoad(srv,index,wot)
  %371 = extractvalue %dx.types.ResRet.f32 %370, 0
  %372 = extractvalue %dx.types.ResRet.f32 %370, 1
  %373 = extractvalue %dx.types.ResRet.f32 %370, 2
  %374 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 96)  ; BufferLoad(srv,index,wot)
  %375 = extractvalue %dx.types.ResRet.f32 %374, 0
  %376 = extractvalue %dx.types.ResRet.f32 %374, 1
  %377 = extractvalue %dx.types.ResRet.f32 %374, 2
  %378 = extractvalue %dx.types.ResRet.f32 %374, 3
  %379 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 112)  ; BufferLoad(srv,index,wot)
  %380 = extractvalue %dx.types.ResRet.f32 %379, 0
  %381 = extractvalue %dx.types.ResRet.f32 %379, 1
  %382 = extractvalue %dx.types.ResRet.f32 %379, 2
  %383 = extractvalue %dx.types.ResRet.f32 %379, 3
  %384 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 128)  ; BufferLoad(srv,index,wot)
  %385 = extractvalue %dx.types.ResRet.f32 %384, 0
  %386 = extractvalue %dx.types.ResRet.f32 %384, 1
  %387 = extractvalue %dx.types.ResRet.f32 %384, 2
  %388 = extractvalue %dx.types.ResRet.f32 %384, 3
  %389 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 144)  ; BufferLoad(srv,index,wot)
  %390 = extractvalue %dx.types.ResRet.f32 %389, 0
  %391 = extractvalue %dx.types.ResRet.f32 %389, 1
  %392 = extractvalue %dx.types.ResRet.f32 %389, 2
  %393 = extractvalue %dx.types.ResRet.f32 %389, 3
  %394 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 160)  ; BufferLoad(srv,index,wot)
  %395 = extractvalue %dx.types.ResRet.f32 %394, 0
  %396 = extractvalue %dx.types.ResRet.f32 %394, 1
  %397 = extractvalue %dx.types.ResRet.f32 %394, 2
  %398 = extractvalue %dx.types.ResRet.f32 %394, 3
  %399 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 176)  ; BufferLoad(srv,index,wot)
  %400 = extractvalue %dx.types.ResRet.f32 %399, 0
  %401 = extractvalue %dx.types.ResRet.f32 %399, 1
  %402 = extractvalue %dx.types.ResRet.f32 %399, 2
  %403 = extractvalue %dx.types.ResRet.f32 %399, 3
  %404 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 192)  ; BufferLoad(srv,index,wot)
  %405 = extractvalue %dx.types.ResRet.f32 %404, 0
  %406 = extractvalue %dx.types.ResRet.f32 %404, 1
  %407 = extractvalue %dx.types.ResRet.f32 %404, 2
  %408 = extractvalue %dx.types.ResRet.f32 %404, 3
  %409 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 208)  ; BufferLoad(srv,index,wot)
  %410 = extractvalue %dx.types.ResRet.f32 %409, 0
  %411 = extractvalue %dx.types.ResRet.f32 %409, 1
  %412 = extractvalue %dx.types.ResRet.f32 %409, 2
  %413 = extractvalue %dx.types.ResRet.f32 %409, 3
  %414 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 224)  ; BufferLoad(srv,index,wot)
  %415 = extractvalue %dx.types.ResRet.f32 %414, 0
  %416 = extractvalue %dx.types.ResRet.f32 %414, 1
  %417 = extractvalue %dx.types.ResRet.f32 %414, 2
  %418 = extractvalue %dx.types.ResRet.f32 %414, 3
  %419 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 240)  ; BufferLoad(srv,index,wot)
  %420 = extractvalue %dx.types.ResRet.f32 %419, 0
  %421 = extractvalue %dx.types.ResRet.f32 %419, 1
  %422 = extractvalue %dx.types.ResRet.f32 %419, 2
  %423 = extractvalue %dx.types.ResRet.f32 %419, 3
  %424 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 256)  ; BufferLoad(srv,index,wot)
  %425 = extractvalue %dx.types.ResRet.f32 %424, 0
  %426 = extractvalue %dx.types.ResRet.f32 %424, 1
  %427 = extractvalue %dx.types.ResRet.f32 %424, 2
  %428 = extractvalue %dx.types.ResRet.f32 %424, 3
  %429 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 272)  ; BufferLoad(srv,index,wot)
  %430 = extractvalue %dx.types.ResRet.f32 %429, 0
  %431 = extractvalue %dx.types.ResRet.f32 %429, 1
  %432 = extractvalue %dx.types.ResRet.f32 %429, 2
  %433 = extractvalue %dx.types.ResRet.f32 %429, 3
  %434 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 288)  ; BufferLoad(srv,index,wot)
  %435 = extractvalue %dx.types.ResRet.f32 %434, 0
  %436 = extractvalue %dx.types.ResRet.f32 %434, 1
  %437 = extractvalue %dx.types.ResRet.f32 %434, 2
  %438 = extractvalue %dx.types.ResRet.f32 %434, 3
  %439 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 304)  ; BufferLoad(srv,index,wot)
  %440 = extractvalue %dx.types.ResRet.f32 %439, 0
  %441 = extractvalue %dx.types.ResRet.f32 %439, 1
  %442 = extractvalue %dx.types.ResRet.f32 %439, 2
  %443 = extractvalue %dx.types.ResRet.f32 %439, 3
  %444 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 320)  ; BufferLoad(srv,index,wot)
  %445 = extractvalue %dx.types.ResRet.f32 %444, 0
  %446 = extractvalue %dx.types.ResRet.f32 %444, 1
  %447 = extractvalue %dx.types.ResRet.f32 %444, 2
  %448 = extractvalue %dx.types.ResRet.f32 %444, 3
  %449 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 336)  ; BufferLoad(srv,index,wot)
  %450 = extractvalue %dx.types.ResRet.f32 %449, 0
  %451 = extractvalue %dx.types.ResRet.f32 %449, 1
  %452 = extractvalue %dx.types.ResRet.f32 %449, 2
  %453 = extractvalue %dx.types.ResRet.f32 %449, 3
  %454 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 352)  ; BufferLoad(srv,index,wot)
  %455 = extractvalue %dx.types.ResRet.f32 %454, 0
  %456 = extractvalue %dx.types.ResRet.f32 %454, 1
  %457 = extractvalue %dx.types.ResRet.f32 %454, 2
  %458 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 368)  ; BufferLoad(srv,index,wot)
  %459 = extractvalue %dx.types.ResRet.f32 %458, 0
  %460 = extractvalue %dx.types.ResRet.f32 %458, 1
  %461 = extractvalue %dx.types.ResRet.f32 %458, 2
  %462 = extractvalue %dx.types.ResRet.f32 %458, 3
  %463 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 384)  ; BufferLoad(srv,index,wot)
  %464 = extractvalue %dx.types.ResRet.f32 %463, 0
  %465 = extractvalue %dx.types.ResRet.f32 %463, 1
  %466 = extractvalue %dx.types.ResRet.f32 %463, 2
  %467 = extractvalue %dx.types.ResRet.f32 %463, 3
  %468 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 400)  ; BufferLoad(srv,index,wot)
  %469 = extractvalue %dx.types.ResRet.f32 %468, 0
  %470 = extractvalue %dx.types.ResRet.f32 %468, 1
  %471 = extractvalue %dx.types.ResRet.f32 %468, 2
  %472 = extractvalue %dx.types.ResRet.f32 %468, 3
  %473 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 416)  ; BufferLoad(srv,index,wot)
  %474 = extractvalue %dx.types.ResRet.f32 %473, 0
  %475 = extractvalue %dx.types.ResRet.f32 %473, 1
  %476 = extractvalue %dx.types.ResRet.f32 %473, 2
  %477 = extractvalue %dx.types.ResRet.f32 %473, 3
  %478 = fcmp fast oeq float %358, 2.000000e+00
  br i1 %478, label %479, label %499

; <label>:479                                     ; preds = %348
  %480 = fmul fast float %371, %361
  %481 = fmul fast float %372, %362
  %482 = fmul fast float %373, %363
  %483 = call float @dx.op.dot3.f32(i32 55, float %367, float %368, float %369, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %484 = fmul fast float %365, %365
  %485 = fsub fast float 1.000000e+00, %484
  %486 = fadd fast float %484, 1.000000e+00
  %487 = fmul fast float %365, 2.000000e+00
  %488 = fmul fast float %487, %483
  %489 = fsub fast float %486, %488
  %490 = call float @dx.op.unary.f32(i32 6, float %489)  ; FAbs(value)
  %491 = call float @dx.op.unary.f32(i32 23, float %490)  ; Log(value)
  %492 = fmul fast float %491, 1.500000e+00
  %493 = call float @dx.op.unary.f32(i32 21, float %492)  ; Exp(value)
  %494 = fdiv fast float %485, %493
  %495 = fmul fast float %494, 0x3FE921FB00000000
  %496 = fmul fast float %480, %495
  %497 = fmul fast float %481, %495
  %498 = fmul fast float %482, %495
  br label %1274

; <label>:499                                     ; preds = %348
  %500 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %6, i32 %356, i32 80)  ; BufferLoad(srv,index,wot)
  %501 = extractvalue %dx.types.ResRet.f32 %500, 0
  %502 = fcmp fast oeq float %358, 1.000000e+00
  %503 = fcmp fast ogt float %501, 0.000000e+00
  br i1 %503, label %504, label %511

; <label>:504                                     ; preds = %499
  %505 = fcmp fast ogt float %99, %455
  br i1 %505, label %506, label %511

; <label>:506                                     ; preds = %504
  %507 = fcmp fast ogt float %99, %456
  br i1 %507, label %508, label %511

; <label>:508                                     ; preds = %506
  %509 = fcmp fast ogt float %99, %457
  br i1 %509, label %510, label %511

; <label>:510                                     ; preds = %508
  br label %511

; <label>:511                                     ; preds = %510, %508, %506, %504, %499
  %512 = phi float [ %435, %510 ], [ %415, %508 ], [ %395, %506 ], [ %375, %504 ], [ %375, %499 ]
  %513 = phi float [ %436, %510 ], [ %416, %508 ], [ %396, %506 ], [ %376, %504 ], [ %376, %499 ]
  %514 = phi float [ %437, %510 ], [ %417, %508 ], [ %397, %506 ], [ %377, %504 ], [ %377, %499 ]
  %515 = phi float [ %438, %510 ], [ %418, %508 ], [ %398, %506 ], [ %378, %504 ], [ %378, %499 ]
  %516 = phi float [ %440, %510 ], [ %420, %508 ], [ %400, %506 ], [ %380, %504 ], [ %380, %499 ]
  %517 = phi float [ %441, %510 ], [ %421, %508 ], [ %401, %506 ], [ %381, %504 ], [ %381, %499 ]
  %518 = phi float [ %442, %510 ], [ %422, %508 ], [ %402, %506 ], [ %382, %504 ], [ %382, %499 ]
  %519 = phi float [ %443, %510 ], [ %423, %508 ], [ %403, %506 ], [ %383, %504 ], [ %383, %499 ]
  %520 = phi float [ %445, %510 ], [ %425, %508 ], [ %405, %506 ], [ %385, %504 ], [ %385, %499 ]
  %521 = phi float [ %446, %510 ], [ %426, %508 ], [ %406, %506 ], [ %386, %504 ], [ %386, %499 ]
  %522 = phi float [ %447, %510 ], [ %427, %508 ], [ %407, %506 ], [ %387, %504 ], [ %387, %499 ]
  %523 = phi float [ %448, %510 ], [ %428, %508 ], [ %408, %506 ], [ %388, %504 ], [ %388, %499 ]
  %524 = phi float [ %450, %510 ], [ %430, %508 ], [ %410, %506 ], [ %390, %504 ], [ %390, %499 ]
  %525 = phi float [ %451, %510 ], [ %431, %508 ], [ %411, %506 ], [ %391, %504 ], [ %391, %499 ]
  %526 = phi float [ %452, %510 ], [ %432, %508 ], [ %412, %506 ], [ %392, %504 ], [ %392, %499 ]
  %527 = phi float [ %453, %510 ], [ %433, %508 ], [ %413, %506 ], [ %393, %504 ], [ %393, %499 ]
  %528 = phi float [ %474, %510 ], [ %469, %508 ], [ %464, %506 ], [ %459, %504 ], [ 1.000000e+00, %499 ]
  %529 = phi float [ %475, %510 ], [ %470, %508 ], [ %465, %506 ], [ %460, %504 ], [ 1.000000e+00, %499 ]
  %530 = phi float [ %476, %510 ], [ %471, %508 ], [ %466, %506 ], [ %461, %504 ], [ 0.000000e+00, %499 ]
  %531 = phi float [ %477, %510 ], [ %472, %508 ], [ %467, %506 ], [ %462, %504 ], [ 0.000000e+00, %499 ]
  %532 = fmul fast float %512, %139
  %533 = call float @dx.op.tertiary.f32(i32 46, float %143, float %516, float %532)  ; FMad(a,b,c)
  %534 = call float @dx.op.tertiary.f32(i32 46, float %147, float %520, float %533)  ; FMad(a,b,c)
  %535 = call float @dx.op.tertiary.f32(i32 46, float %151, float %524, float %534)  ; FMad(a,b,c)
  %536 = fmul fast float %513, %139
  %537 = call float @dx.op.tertiary.f32(i32 46, float %143, float %517, float %536)  ; FMad(a,b,c)
  %538 = call float @dx.op.tertiary.f32(i32 46, float %147, float %521, float %537)  ; FMad(a,b,c)
  %539 = call float @dx.op.tertiary.f32(i32 46, float %151, float %525, float %538)  ; FMad(a,b,c)
  %540 = fmul fast float %514, %139
  %541 = call float @dx.op.tertiary.f32(i32 46, float %143, float %518, float %540)  ; FMad(a,b,c)
  %542 = call float @dx.op.tertiary.f32(i32 46, float %147, float %522, float %541)  ; FMad(a,b,c)
  %543 = call float @dx.op.tertiary.f32(i32 46, float %151, float %526, float %542)  ; FMad(a,b,c)
  %544 = fmul fast float %515, %139
  %545 = call float @dx.op.tertiary.f32(i32 46, float %143, float %519, float %544)  ; FMad(a,b,c)
  %546 = call float @dx.op.tertiary.f32(i32 46, float %147, float %523, float %545)  ; FMad(a,b,c)
  %547 = call float @dx.op.tertiary.f32(i32 46, float %151, float %527, float %546)  ; FMad(a,b,c)
  %548 = fdiv fast float %535, %547
  %549 = fdiv fast float %539, %547
  %550 = fdiv fast float %543, %547
  %551 = fcmp fast olt float %548, 0.000000e+00
  %552 = fcmp fast ogt float %548, 1.000000e+00
  %553 = or i1 %551, %552
  %554 = fcmp fast olt float %549, 0.000000e+00
  %555 = or i1 %553, %554
  %556 = fcmp fast ogt float %549, 1.000000e+00
  %557 = or i1 %556, %555
  br i1 %557, label %576, label %558

; <label>:558                                     ; preds = %511
  %559 = fmul fast float %548, %528
  %560 = fmul fast float %549, %529
  %561 = fadd fast float %559, %530
  %562 = fadd fast float %560, %531
  %563 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %9, %dx.types.Handle %24, float %561, float %562, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %564 = extractvalue %dx.types.ResRet.f32 %563, 0
  br i1 %502, label %565, label %573

; <label>:565                                     ; preds = %558
  %566 = fmul fast float %359, 0xBFF7154760000000
  %567 = fmul fast float %566, %550
  %568 = call float @dx.op.unary.f32(i32 21, float %567)  ; Exp(value)
  %569 = fmul fast float %564, 0x3FF3333340000000
  %570 = fmul fast float %569, %568
  %571 = fadd fast float %570, 0xBFC99999A0000000
  %572 = call float @dx.op.unary.f32(i32 7, float %571)  ; Saturate(value)
  br label %576

; <label>:573                                     ; preds = %558
  %574 = fcmp fast ogt float %564, %550
  %575 = select i1 %574, float 1.000000e+00, float 0.000000e+00
  br label %576

; <label>:576                                     ; preds = %573, %565, %511
  %577 = phi float [ %572, %565 ], [ %575, %573 ], [ 1.000000e+00, %511 ]
  %578 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 41)  ; CBufferLoadLegacy(handle,regIndex)
  %579 = extractvalue %dx.types.CBufRet.i32 %578, 0
  %580 = icmp sgt i32 %579, 0
  br i1 %580, label %581, label %766

; <label>:581                                     ; preds = %576
  %582 = fcmp fast oeq float %249, 0.000000e+00
  %583 = fcmp fast oeq float %249, 1.000000e+00
  %584 = or i1 %582, %583
  br i1 %584, label %766, label %585

; <label>:585                                     ; preds = %581
  %586 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %587 = extractvalue %dx.types.CBufRet.f32 %586, 0
  %588 = extractvalue %dx.types.CBufRet.f32 %586, 1
  %589 = extractvalue %dx.types.CBufRet.f32 %586, 2
  %590 = fsub fast float %139, %587
  %591 = fsub fast float %143, %588
  %592 = fsub fast float %147, %589
  %593 = fmul fast float %590, %590
  %594 = fmul fast float %591, %591
  %595 = fadd fast float %593, %594
  %596 = fmul fast float %592, %592
  %597 = fadd fast float %595, %596
  %598 = call float @dx.op.unary.f32(i32 24, float %597)  ; Sqrt(value)
  %599 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %600 = extractvalue %dx.types.CBufRet.i32 %599, 0
  %601 = and i32 %600, 31
  %602 = shl i32 1, %601
  %603 = uitofp i32 %602 to float
  %604 = fmul fast float %603, 5.000000e-01
  %605 = fdiv fast float %598, %604
  %606 = fmul fast float %605, 2.000000e+00
  %607 = call float @dx.op.unary.f32(i32 6, float %606)  ; FAbs(value)
  %608 = call float @dx.op.binary.f32(i32 35, float %607, float 1.000000e+00)  ; FMax(a,b)
  %609 = call float @dx.op.unary.f32(i32 23, float %608)  ; Log(value)
  %610 = fptoui float %609 to i32
  %611 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %612 = extractvalue %dx.types.CBufRet.i32 %611, 3
  %613 = call i32 @dx.op.binary.i32(i32 37, i32 %610, i32 %612)  ; IMax(a,b)
  %614 = icmp ult i32 %613, %579
  br i1 %614, label %615, label %727

; <label>:615                                     ; preds = %585
  br label %616

; <label>:616                                     ; preds = %616, %615
  %617 = phi i32 [ %688, %616 ], [ %579, %615 ]
  %618 = phi i32 [ %689, %616 ], [ %613, %615 ]
  %619 = shl i32 %618, 2
  %620 = add i32 %619, 43
  %621 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %620)  ; CBufferLoadLegacy(handle,regIndex)
  %622 = extractvalue %dx.types.CBufRet.f32 %621, 0
  %623 = extractvalue %dx.types.CBufRet.f32 %621, 1
  %624 = add i32 %619, 44
  %625 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %624)  ; CBufferLoadLegacy(handle,regIndex)
  %626 = extractvalue %dx.types.CBufRet.f32 %625, 0
  %627 = extractvalue %dx.types.CBufRet.f32 %625, 1
  %628 = add i32 %619, 45
  %629 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %628)  ; CBufferLoadLegacy(handle,regIndex)
  %630 = extractvalue %dx.types.CBufRet.f32 %629, 0
  %631 = extractvalue %dx.types.CBufRet.f32 %629, 1
  %632 = add i32 %619, 46
  %633 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %632)  ; CBufferLoadLegacy(handle,regIndex)
  %634 = extractvalue %dx.types.CBufRet.f32 %633, 0
  %635 = extractvalue %dx.types.CBufRet.f32 %633, 1
  %636 = fmul fast float %622, %139
  %637 = call float @dx.op.tertiary.f32(i32 46, float %143, float %626, float %636)  ; FMad(a,b,c)
  %638 = call float @dx.op.tertiary.f32(i32 46, float %147, float %630, float %637)  ; FMad(a,b,c)
  %639 = call float @dx.op.tertiary.f32(i32 46, float %151, float %634, float %638)  ; FMad(a,b,c)
  %640 = fmul fast float %623, %139
  %641 = call float @dx.op.tertiary.f32(i32 46, float %143, float %627, float %640)  ; FMad(a,b,c)
  %642 = call float @dx.op.tertiary.f32(i32 46, float %147, float %631, float %641)  ; FMad(a,b,c)
  %643 = call float @dx.op.tertiary.f32(i32 46, float %151, float %635, float %642)  ; FMad(a,b,c)
  %644 = call float @dx.op.binary.f32(i32 35, float %639, float 0.000000e+00)  ; FMax(a,b)
  %645 = call float @dx.op.binary.f32(i32 35, float %643, float 0.000000e+00)  ; FMax(a,b)
  %646 = call float @dx.op.binary.f32(i32 36, float %644, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %647 = call float @dx.op.binary.f32(i32 36, float %645, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %648 = add i32 %618, 107
  %649 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %648)  ; CBufferLoadLegacy(handle,regIndex)
  %650 = extractvalue %dx.types.CBufRet.f32 %649, 0
  %651 = extractvalue %dx.types.CBufRet.f32 %649, 1
  %652 = fadd fast float %650, %646
  %653 = fadd fast float %651, %647
  %654 = call float @dx.op.unary.f32(i32 22, float %652)  ; Frc(value)
  %655 = call float @dx.op.unary.f32(i32 22, float %653)  ; Frc(value)
  %656 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %657 = extractvalue %dx.types.CBufRet.i32 %656, 2
  %658 = and i32 %618, 31
  %659 = shl i32 1, %658
  %660 = and i32 %657, %659
  %661 = icmp ne i32 %660, 0
  %662 = fcmp fast ogt float %639, 0.000000e+00
  %663 = fcmp fast ogt float %643, 0.000000e+00
  %664 = and i1 %662, %663
  %665 = fcmp fast olt float %639, 1.000000e+00
  %666 = fcmp fast olt float %643, 1.000000e+00
  %667 = and i1 %665, %666
  %668 = and i1 %664, %667
  %669 = and i1 %668, %661
  %670 = uitofp i32 %618 to float
  %671 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %672 = extractvalue %dx.types.CBufRet.f32 %671, 0
  %673 = extractvalue %dx.types.CBufRet.f32 %671, 1
  %674 = fmul fast float %673, %655
  %675 = extractvalue %dx.types.CBufRet.f32 %671, 2
  %676 = fmul fast float %675, %670
  %677 = call float @dx.op.unary.f32(i32 27, float %674)  ; Round_ni(value)
  %678 = fadd fast float %677, %654
  %679 = fmul fast float %678, %672
  %680 = fadd fast float %679, %676
  %681 = fptoui float %680 to i32
  %682 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %681, i32 undef)  ; BufferLoad(srv,index,wot)
  %683 = extractvalue %dx.types.ResRet.i32 %682, 0
  %684 = and i32 %683, 1
  %685 = icmp ne i32 %684, 0
  %686 = and i1 %669, %685
  %687 = select i1 %686, i32 %618, i32 %579
  %688 = call i32 @dx.op.binary.i32(i32 40, i32 %617, i32 %687)  ; UMin(a,b)
  %689 = add nuw i32 %618, 1
  %690 = icmp eq i32 %689, %579
  br i1 %690, label %691, label %616

; <label>:691                                     ; preds = %616
  %692 = icmp ult i32 %688, %579
  br i1 %692, label %693, label %727

; <label>:693                                     ; preds = %691
  %694 = shl i32 %688, 2
  %695 = add i32 %694, 43
  %696 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %695)  ; CBufferLoadLegacy(handle,regIndex)
  %697 = extractvalue %dx.types.CBufRet.f32 %696, 0
  %698 = extractvalue %dx.types.CBufRet.f32 %696, 1
  %699 = extractvalue %dx.types.CBufRet.f32 %696, 2
  %700 = add i32 %694, 44
  %701 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %700)  ; CBufferLoadLegacy(handle,regIndex)
  %702 = extractvalue %dx.types.CBufRet.f32 %701, 0
  %703 = extractvalue %dx.types.CBufRet.f32 %701, 1
  %704 = extractvalue %dx.types.CBufRet.f32 %701, 2
  %705 = add i32 %694, 45
  %706 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %705)  ; CBufferLoadLegacy(handle,regIndex)
  %707 = extractvalue %dx.types.CBufRet.f32 %706, 0
  %708 = extractvalue %dx.types.CBufRet.f32 %706, 1
  %709 = extractvalue %dx.types.CBufRet.f32 %706, 2
  %710 = add i32 %694, 46
  %711 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %710)  ; CBufferLoadLegacy(handle,regIndex)
  %712 = extractvalue %dx.types.CBufRet.f32 %711, 0
  %713 = extractvalue %dx.types.CBufRet.f32 %711, 1
  %714 = extractvalue %dx.types.CBufRet.f32 %711, 2
  %715 = fmul fast float %697, %139
  %716 = call float @dx.op.tertiary.f32(i32 46, float %143, float %702, float %715)  ; FMad(a,b,c)
  %717 = call float @dx.op.tertiary.f32(i32 46, float %147, float %707, float %716)  ; FMad(a,b,c)
  %718 = call float @dx.op.tertiary.f32(i32 46, float %151, float %712, float %717)  ; FMad(a,b,c)
  %719 = fmul fast float %698, %139
  %720 = call float @dx.op.tertiary.f32(i32 46, float %143, float %703, float %719)  ; FMad(a,b,c)
  %721 = call float @dx.op.tertiary.f32(i32 46, float %147, float %708, float %720)  ; FMad(a,b,c)
  %722 = call float @dx.op.tertiary.f32(i32 46, float %151, float %713, float %721)  ; FMad(a,b,c)
  %723 = fmul fast float %699, %139
  %724 = call float @dx.op.tertiary.f32(i32 46, float %143, float %704, float %723)  ; FMad(a,b,c)
  %725 = call float @dx.op.tertiary.f32(i32 46, float %147, float %709, float %724)  ; FMad(a,b,c)
  %726 = call float @dx.op.tertiary.f32(i32 46, float %151, float %714, float %725)  ; FMad(a,b,c)
  br label %727

; <label>:727                                     ; preds = %693, %691, %585
  %728 = phi float [ %353, %585 ], [ %353, %691 ], [ %718, %693 ]
  %729 = phi float [ %354, %585 ], [ %354, %691 ], [ %722, %693 ]
  %730 = phi float [ %355, %585 ], [ %355, %691 ], [ %726, %693 ]
  %731 = phi i32 [ %352, %585 ], [ %688, %691 ], [ %688, %693 ]
  %732 = icmp slt i32 %731, %579
  br i1 %732, label %733, label %766

; <label>:733                                     ; preds = %727
  %734 = call float @dx.op.binary.f32(i32 35, float %728, float 0.000000e+00)  ; FMax(a,b)
  %735 = call float @dx.op.binary.f32(i32 35, float %729, float 0.000000e+00)  ; FMax(a,b)
  %736 = call float @dx.op.binary.f32(i32 36, float %734, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %737 = call float @dx.op.binary.f32(i32 36, float %735, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %738 = add i32 %731, 107
  %739 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %738)  ; CBufferLoadLegacy(handle,regIndex)
  %740 = extractvalue %dx.types.CBufRet.f32 %739, 0
  %741 = extractvalue %dx.types.CBufRet.f32 %739, 1
  %742 = fadd fast float %740, %736
  %743 = fadd fast float %741, %737
  %744 = call float @dx.op.unary.f32(i32 22, float %742)  ; Frc(value)
  %745 = call float @dx.op.unary.f32(i32 22, float %743)  ; Frc(value)
  %746 = uitofp i32 %731 to float
  %747 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %748 = extractvalue %dx.types.CBufRet.f32 %747, 0
  %749 = extractvalue %dx.types.CBufRet.f32 %747, 1
  %750 = fmul fast float %749, %745
  %751 = extractvalue %dx.types.CBufRet.f32 %747, 2
  %752 = fmul fast float %751, %746
  %753 = call float @dx.op.unary.f32(i32 27, float %750)  ; Round_ni(value)
  %754 = fadd fast float %753, %744
  %755 = fmul fast float %754, %748
  %756 = fadd fast float %755, %752
  %757 = fptoui float %756 to i32
  %758 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %757, i32 undef)  ; BufferLoad(srv,index,wot)
  %759 = extractvalue %dx.types.ResRet.i32 %758, 0
  %760 = and i32 %759, 1
  %761 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %13, %dx.types.Handle %27, float %744, float %745, float %746, float undef, i32 0, i32 0, i32 undef, float %730)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %762 = extractvalue %dx.types.ResRet.f32 %761, 0
  %763 = icmp ne i32 %760, 0
  %764 = select i1 %763, float %762, float 1.000000e+00
  %765 = call float @dx.op.binary.f32(i32 36, float 1.000000e+00, float %764)  ; FMin(a,b)
  br label %766

; <label>:766                                     ; preds = %733, %727, %581, %576
  %767 = phi float [ 1.000000e+00, %581 ], [ 1.000000e+00, %727 ], [ %765, %733 ], [ 1.000000e+00, %576 ]
  %768 = phi float [ %353, %581 ], [ %728, %727 ], [ %728, %733 ], [ %353, %576 ]
  %769 = phi float [ %354, %581 ], [ %729, %727 ], [ %729, %733 ], [ %354, %576 ]
  %770 = phi float [ %355, %581 ], [ %730, %727 ], [ %730, %733 ], [ %355, %576 ]
  %771 = phi i32 [ %352, %581 ], [ %731, %727 ], [ %731, %733 ], [ %352, %576 ]
  %772 = call float @dx.op.binary.f32(i32 36, float %577, float %767)  ; FMin(a,b)
  %773 = fmul fast float %512, %296
  %774 = call float @dx.op.tertiary.f32(i32 46, float %300, float %516, float %773)  ; FMad(a,b,c)
  %775 = call float @dx.op.tertiary.f32(i32 46, float %304, float %520, float %774)  ; FMad(a,b,c)
  %776 = call float @dx.op.tertiary.f32(i32 46, float %308, float %524, float %775)  ; FMad(a,b,c)
  %777 = fmul fast float %513, %296
  %778 = call float @dx.op.tertiary.f32(i32 46, float %300, float %517, float %777)  ; FMad(a,b,c)
  %779 = call float @dx.op.tertiary.f32(i32 46, float %304, float %521, float %778)  ; FMad(a,b,c)
  %780 = call float @dx.op.tertiary.f32(i32 46, float %308, float %525, float %779)  ; FMad(a,b,c)
  %781 = fmul fast float %514, %296
  %782 = call float @dx.op.tertiary.f32(i32 46, float %300, float %518, float %781)  ; FMad(a,b,c)
  %783 = call float @dx.op.tertiary.f32(i32 46, float %304, float %522, float %782)  ; FMad(a,b,c)
  %784 = call float @dx.op.tertiary.f32(i32 46, float %308, float %526, float %783)  ; FMad(a,b,c)
  %785 = fmul fast float %515, %296
  %786 = call float @dx.op.tertiary.f32(i32 46, float %300, float %519, float %785)  ; FMad(a,b,c)
  %787 = call float @dx.op.tertiary.f32(i32 46, float %304, float %523, float %786)  ; FMad(a,b,c)
  %788 = call float @dx.op.tertiary.f32(i32 46, float %308, float %527, float %787)  ; FMad(a,b,c)
  %789 = fdiv fast float %776, %788
  %790 = fdiv fast float %780, %788
  %791 = fdiv fast float %784, %788
  %792 = fcmp fast olt float %789, 0.000000e+00
  %793 = fcmp fast ogt float %789, 1.000000e+00
  %794 = or i1 %792, %793
  %795 = fcmp fast olt float %790, 0.000000e+00
  %796 = or i1 %794, %795
  %797 = fcmp fast ogt float %790, 1.000000e+00
  %798 = or i1 %797, %796
  br i1 %798, label %817, label %799

; <label>:799                                     ; preds = %766
  %800 = fmul fast float %789, %528
  %801 = fmul fast float %790, %529
  %802 = fadd fast float %800, %530
  %803 = fadd fast float %801, %531
  %804 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %9, %dx.types.Handle %24, float %802, float %803, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %805 = extractvalue %dx.types.ResRet.f32 %804, 0
  br i1 %502, label %806, label %814

; <label>:806                                     ; preds = %799
  %807 = fmul fast float %359, 0xBFF7154760000000
  %808 = fmul fast float %807, %791
  %809 = call float @dx.op.unary.f32(i32 21, float %808)  ; Exp(value)
  %810 = fmul fast float %805, 0x3FF3333340000000
  %811 = fmul fast float %810, %809
  %812 = fadd fast float %811, 0xBFC99999A0000000
  %813 = call float @dx.op.unary.f32(i32 7, float %812)  ; Saturate(value)
  br label %817

; <label>:814                                     ; preds = %799
  %815 = fcmp fast ogt float %805, %791
  %816 = select i1 %815, float 1.000000e+00, float 0.000000e+00
  br label %817

; <label>:817                                     ; preds = %814, %806, %766
  %818 = phi float [ %813, %806 ], [ %816, %814 ], [ 1.000000e+00, %766 ]
  br i1 %580, label %819, label %1004

; <label>:819                                     ; preds = %817
  %820 = fcmp fast oeq float %249, 0.000000e+00
  %821 = fcmp fast oeq float %249, 1.000000e+00
  %822 = or i1 %820, %821
  br i1 %822, label %1004, label %823

; <label>:823                                     ; preds = %819
  %824 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %825 = extractvalue %dx.types.CBufRet.f32 %824, 0
  %826 = extractvalue %dx.types.CBufRet.f32 %824, 1
  %827 = extractvalue %dx.types.CBufRet.f32 %824, 2
  %828 = fsub fast float %296, %825
  %829 = fsub fast float %300, %826
  %830 = fsub fast float %304, %827
  %831 = fmul fast float %828, %828
  %832 = fmul fast float %829, %829
  %833 = fadd fast float %831, %832
  %834 = fmul fast float %830, %830
  %835 = fadd fast float %833, %834
  %836 = call float @dx.op.unary.f32(i32 24, float %835)  ; Sqrt(value)
  %837 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %838 = extractvalue %dx.types.CBufRet.i32 %837, 0
  %839 = and i32 %838, 31
  %840 = shl i32 1, %839
  %841 = uitofp i32 %840 to float
  %842 = fmul fast float %841, 5.000000e-01
  %843 = fdiv fast float %836, %842
  %844 = fmul fast float %843, 2.000000e+00
  %845 = call float @dx.op.unary.f32(i32 6, float %844)  ; FAbs(value)
  %846 = call float @dx.op.binary.f32(i32 35, float %845, float 1.000000e+00)  ; FMax(a,b)
  %847 = call float @dx.op.unary.f32(i32 23, float %846)  ; Log(value)
  %848 = fptoui float %847 to i32
  %849 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %850 = extractvalue %dx.types.CBufRet.i32 %849, 3
  %851 = call i32 @dx.op.binary.i32(i32 37, i32 %848, i32 %850)  ; IMax(a,b)
  %852 = icmp ult i32 %851, %579
  br i1 %852, label %853, label %965

; <label>:853                                     ; preds = %823
  br label %854

; <label>:854                                     ; preds = %854, %853
  %855 = phi i32 [ %926, %854 ], [ %579, %853 ]
  %856 = phi i32 [ %927, %854 ], [ %851, %853 ]
  %857 = shl i32 %856, 2
  %858 = add i32 %857, 43
  %859 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %858)  ; CBufferLoadLegacy(handle,regIndex)
  %860 = extractvalue %dx.types.CBufRet.f32 %859, 0
  %861 = extractvalue %dx.types.CBufRet.f32 %859, 1
  %862 = add i32 %857, 44
  %863 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %862)  ; CBufferLoadLegacy(handle,regIndex)
  %864 = extractvalue %dx.types.CBufRet.f32 %863, 0
  %865 = extractvalue %dx.types.CBufRet.f32 %863, 1
  %866 = add i32 %857, 45
  %867 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %866)  ; CBufferLoadLegacy(handle,regIndex)
  %868 = extractvalue %dx.types.CBufRet.f32 %867, 0
  %869 = extractvalue %dx.types.CBufRet.f32 %867, 1
  %870 = add i32 %857, 46
  %871 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %870)  ; CBufferLoadLegacy(handle,regIndex)
  %872 = extractvalue %dx.types.CBufRet.f32 %871, 0
  %873 = extractvalue %dx.types.CBufRet.f32 %871, 1
  %874 = fmul fast float %860, %296
  %875 = call float @dx.op.tertiary.f32(i32 46, float %300, float %864, float %874)  ; FMad(a,b,c)
  %876 = call float @dx.op.tertiary.f32(i32 46, float %304, float %868, float %875)  ; FMad(a,b,c)
  %877 = call float @dx.op.tertiary.f32(i32 46, float %308, float %872, float %876)  ; FMad(a,b,c)
  %878 = fmul fast float %861, %296
  %879 = call float @dx.op.tertiary.f32(i32 46, float %300, float %865, float %878)  ; FMad(a,b,c)
  %880 = call float @dx.op.tertiary.f32(i32 46, float %304, float %869, float %879)  ; FMad(a,b,c)
  %881 = call float @dx.op.tertiary.f32(i32 46, float %308, float %873, float %880)  ; FMad(a,b,c)
  %882 = call float @dx.op.binary.f32(i32 35, float %877, float 0.000000e+00)  ; FMax(a,b)
  %883 = call float @dx.op.binary.f32(i32 35, float %881, float 0.000000e+00)  ; FMax(a,b)
  %884 = call float @dx.op.binary.f32(i32 36, float %882, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %885 = call float @dx.op.binary.f32(i32 36, float %883, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %886 = add i32 %856, 107
  %887 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %886)  ; CBufferLoadLegacy(handle,regIndex)
  %888 = extractvalue %dx.types.CBufRet.f32 %887, 0
  %889 = extractvalue %dx.types.CBufRet.f32 %887, 1
  %890 = fadd fast float %888, %884
  %891 = fadd fast float %889, %885
  %892 = call float @dx.op.unary.f32(i32 22, float %890)  ; Frc(value)
  %893 = call float @dx.op.unary.f32(i32 22, float %891)  ; Frc(value)
  %894 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %895 = extractvalue %dx.types.CBufRet.i32 %894, 2
  %896 = and i32 %856, 31
  %897 = shl i32 1, %896
  %898 = and i32 %895, %897
  %899 = icmp ne i32 %898, 0
  %900 = fcmp fast ogt float %877, 0.000000e+00
  %901 = fcmp fast ogt float %881, 0.000000e+00
  %902 = and i1 %900, %901
  %903 = fcmp fast olt float %877, 1.000000e+00
  %904 = fcmp fast olt float %881, 1.000000e+00
  %905 = and i1 %903, %904
  %906 = and i1 %902, %905
  %907 = and i1 %906, %899
  %908 = uitofp i32 %856 to float
  %909 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %910 = extractvalue %dx.types.CBufRet.f32 %909, 0
  %911 = extractvalue %dx.types.CBufRet.f32 %909, 1
  %912 = fmul fast float %911, %893
  %913 = extractvalue %dx.types.CBufRet.f32 %909, 2
  %914 = fmul fast float %913, %908
  %915 = call float @dx.op.unary.f32(i32 27, float %912)  ; Round_ni(value)
  %916 = fadd fast float %915, %892
  %917 = fmul fast float %916, %910
  %918 = fadd fast float %917, %914
  %919 = fptoui float %918 to i32
  %920 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %919, i32 undef)  ; BufferLoad(srv,index,wot)
  %921 = extractvalue %dx.types.ResRet.i32 %920, 0
  %922 = and i32 %921, 1
  %923 = icmp ne i32 %922, 0
  %924 = and i1 %907, %923
  %925 = select i1 %924, i32 %856, i32 %579
  %926 = call i32 @dx.op.binary.i32(i32 40, i32 %855, i32 %925)  ; UMin(a,b)
  %927 = add nuw i32 %856, 1
  %928 = icmp eq i32 %927, %579
  br i1 %928, label %929, label %854

; <label>:929                                     ; preds = %854
  %930 = icmp ult i32 %926, %579
  br i1 %930, label %931, label %965

; <label>:931                                     ; preds = %929
  %932 = shl i32 %926, 2
  %933 = add i32 %932, 43
  %934 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %933)  ; CBufferLoadLegacy(handle,regIndex)
  %935 = extractvalue %dx.types.CBufRet.f32 %934, 0
  %936 = extractvalue %dx.types.CBufRet.f32 %934, 1
  %937 = extractvalue %dx.types.CBufRet.f32 %934, 2
  %938 = add i32 %932, 44
  %939 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %938)  ; CBufferLoadLegacy(handle,regIndex)
  %940 = extractvalue %dx.types.CBufRet.f32 %939, 0
  %941 = extractvalue %dx.types.CBufRet.f32 %939, 1
  %942 = extractvalue %dx.types.CBufRet.f32 %939, 2
  %943 = add i32 %932, 45
  %944 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %943)  ; CBufferLoadLegacy(handle,regIndex)
  %945 = extractvalue %dx.types.CBufRet.f32 %944, 0
  %946 = extractvalue %dx.types.CBufRet.f32 %944, 1
  %947 = extractvalue %dx.types.CBufRet.f32 %944, 2
  %948 = add i32 %932, 46
  %949 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %948)  ; CBufferLoadLegacy(handle,regIndex)
  %950 = extractvalue %dx.types.CBufRet.f32 %949, 0
  %951 = extractvalue %dx.types.CBufRet.f32 %949, 1
  %952 = extractvalue %dx.types.CBufRet.f32 %949, 2
  %953 = fmul fast float %935, %296
  %954 = call float @dx.op.tertiary.f32(i32 46, float %300, float %940, float %953)  ; FMad(a,b,c)
  %955 = call float @dx.op.tertiary.f32(i32 46, float %304, float %945, float %954)  ; FMad(a,b,c)
  %956 = call float @dx.op.tertiary.f32(i32 46, float %308, float %950, float %955)  ; FMad(a,b,c)
  %957 = fmul fast float %936, %296
  %958 = call float @dx.op.tertiary.f32(i32 46, float %300, float %941, float %957)  ; FMad(a,b,c)
  %959 = call float @dx.op.tertiary.f32(i32 46, float %304, float %946, float %958)  ; FMad(a,b,c)
  %960 = call float @dx.op.tertiary.f32(i32 46, float %308, float %951, float %959)  ; FMad(a,b,c)
  %961 = fmul fast float %937, %296
  %962 = call float @dx.op.tertiary.f32(i32 46, float %300, float %942, float %961)  ; FMad(a,b,c)
  %963 = call float @dx.op.tertiary.f32(i32 46, float %304, float %947, float %962)  ; FMad(a,b,c)
  %964 = call float @dx.op.tertiary.f32(i32 46, float %308, float %952, float %963)  ; FMad(a,b,c)
  br label %965

; <label>:965                                     ; preds = %931, %929, %823
  %966 = phi float [ %768, %823 ], [ %768, %929 ], [ %956, %931 ]
  %967 = phi float [ %769, %823 ], [ %769, %929 ], [ %960, %931 ]
  %968 = phi float [ %770, %823 ], [ %770, %929 ], [ %964, %931 ]
  %969 = phi i32 [ %771, %823 ], [ %926, %929 ], [ %926, %931 ]
  %970 = icmp slt i32 %969, %579
  br i1 %970, label %971, label %1004

; <label>:971                                     ; preds = %965
  %972 = call float @dx.op.binary.f32(i32 35, float %966, float 0.000000e+00)  ; FMax(a,b)
  %973 = call float @dx.op.binary.f32(i32 35, float %967, float 0.000000e+00)  ; FMax(a,b)
  %974 = call float @dx.op.binary.f32(i32 36, float %972, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %975 = call float @dx.op.binary.f32(i32 36, float %973, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %976 = add i32 %969, 107
  %977 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %976)  ; CBufferLoadLegacy(handle,regIndex)
  %978 = extractvalue %dx.types.CBufRet.f32 %977, 0
  %979 = extractvalue %dx.types.CBufRet.f32 %977, 1
  %980 = fadd fast float %978, %974
  %981 = fadd fast float %979, %975
  %982 = call float @dx.op.unary.f32(i32 22, float %980)  ; Frc(value)
  %983 = call float @dx.op.unary.f32(i32 22, float %981)  ; Frc(value)
  %984 = uitofp i32 %969 to float
  %985 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %986 = extractvalue %dx.types.CBufRet.f32 %985, 0
  %987 = extractvalue %dx.types.CBufRet.f32 %985, 1
  %988 = fmul fast float %987, %983
  %989 = extractvalue %dx.types.CBufRet.f32 %985, 2
  %990 = fmul fast float %989, %984
  %991 = call float @dx.op.unary.f32(i32 27, float %988)  ; Round_ni(value)
  %992 = fadd fast float %991, %982
  %993 = fmul fast float %992, %986
  %994 = fadd fast float %993, %990
  %995 = fptoui float %994 to i32
  %996 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %995, i32 undef)  ; BufferLoad(srv,index,wot)
  %997 = extractvalue %dx.types.ResRet.i32 %996, 0
  %998 = and i32 %997, 1
  %999 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %13, %dx.types.Handle %27, float %982, float %983, float %984, float undef, i32 0, i32 0, i32 undef, float %968)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1000 = extractvalue %dx.types.ResRet.f32 %999, 0
  %1001 = icmp ne i32 %998, 0
  %1002 = select i1 %1001, float %1000, float 1.000000e+00
  %1003 = call float @dx.op.binary.f32(i32 36, float 1.000000e+00, float %1002)  ; FMin(a,b)
  br label %1004

; <label>:1004                                    ; preds = %971, %965, %819, %817
  %1005 = phi float [ 1.000000e+00, %819 ], [ 1.000000e+00, %965 ], [ %1003, %971 ], [ 1.000000e+00, %817 ]
  %1006 = phi float [ %768, %819 ], [ %966, %965 ], [ %966, %971 ], [ %768, %817 ]
  %1007 = phi float [ %769, %819 ], [ %967, %965 ], [ %967, %971 ], [ %769, %817 ]
  %1008 = phi float [ %770, %819 ], [ %968, %965 ], [ %968, %971 ], [ %770, %817 ]
  %1009 = phi i32 [ %771, %819 ], [ %969, %965 ], [ %969, %971 ], [ %771, %817 ]
  %1010 = call float @dx.op.binary.f32(i32 36, float %818, float %1005)  ; FMin(a,b)
  %1011 = fadd fast float %1010, %772
  %1012 = fmul fast float %512, %329
  %1013 = call float @dx.op.tertiary.f32(i32 46, float %333, float %516, float %1012)  ; FMad(a,b,c)
  %1014 = call float @dx.op.tertiary.f32(i32 46, float %337, float %520, float %1013)  ; FMad(a,b,c)
  %1015 = call float @dx.op.tertiary.f32(i32 46, float %341, float %524, float %1014)  ; FMad(a,b,c)
  %1016 = fmul fast float %513, %329
  %1017 = call float @dx.op.tertiary.f32(i32 46, float %333, float %517, float %1016)  ; FMad(a,b,c)
  %1018 = call float @dx.op.tertiary.f32(i32 46, float %337, float %521, float %1017)  ; FMad(a,b,c)
  %1019 = call float @dx.op.tertiary.f32(i32 46, float %341, float %525, float %1018)  ; FMad(a,b,c)
  %1020 = fmul fast float %514, %329
  %1021 = call float @dx.op.tertiary.f32(i32 46, float %333, float %518, float %1020)  ; FMad(a,b,c)
  %1022 = call float @dx.op.tertiary.f32(i32 46, float %337, float %522, float %1021)  ; FMad(a,b,c)
  %1023 = call float @dx.op.tertiary.f32(i32 46, float %341, float %526, float %1022)  ; FMad(a,b,c)
  %1024 = fmul fast float %515, %329
  %1025 = call float @dx.op.tertiary.f32(i32 46, float %333, float %519, float %1024)  ; FMad(a,b,c)
  %1026 = call float @dx.op.tertiary.f32(i32 46, float %337, float %523, float %1025)  ; FMad(a,b,c)
  %1027 = call float @dx.op.tertiary.f32(i32 46, float %341, float %527, float %1026)  ; FMad(a,b,c)
  %1028 = fdiv fast float %1015, %1027
  %1029 = fdiv fast float %1019, %1027
  %1030 = fdiv fast float %1023, %1027
  %1031 = fcmp fast olt float %1028, 0.000000e+00
  %1032 = fcmp fast ogt float %1028, 1.000000e+00
  %1033 = or i1 %1031, %1032
  %1034 = fcmp fast olt float %1029, 0.000000e+00
  %1035 = or i1 %1033, %1034
  %1036 = fcmp fast ogt float %1029, 1.000000e+00
  %1037 = or i1 %1036, %1035
  br i1 %1037, label %1056, label %1038

; <label>:1038                                    ; preds = %1004
  %1039 = fmul fast float %1028, %528
  %1040 = fmul fast float %1029, %529
  %1041 = fadd fast float %1039, %530
  %1042 = fadd fast float %1040, %531
  %1043 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %9, %dx.types.Handle %24, float %1041, float %1042, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1044 = extractvalue %dx.types.ResRet.f32 %1043, 0
  br i1 %502, label %1045, label %1053

; <label>:1045                                    ; preds = %1038
  %1046 = fmul fast float %359, 0xBFF7154760000000
  %1047 = fmul fast float %1046, %1030
  %1048 = call float @dx.op.unary.f32(i32 21, float %1047)  ; Exp(value)
  %1049 = fmul fast float %1044, 0x3FF3333340000000
  %1050 = fmul fast float %1049, %1048
  %1051 = fadd fast float %1050, 0xBFC99999A0000000
  %1052 = call float @dx.op.unary.f32(i32 7, float %1051)  ; Saturate(value)
  br label %1056

; <label>:1053                                    ; preds = %1038
  %1054 = fcmp fast ogt float %1044, %1030
  %1055 = select i1 %1054, float 1.000000e+00, float 0.000000e+00
  br label %1056

; <label>:1056                                    ; preds = %1053, %1045, %1004
  %1057 = phi float [ %1052, %1045 ], [ %1055, %1053 ], [ 1.000000e+00, %1004 ]
  br i1 %580, label %1058, label %1243

; <label>:1058                                    ; preds = %1056
  %1059 = fcmp fast oeq float %249, 0.000000e+00
  %1060 = fcmp fast oeq float %249, 1.000000e+00
  %1061 = or i1 %1059, %1060
  br i1 %1061, label %1243, label %1062

; <label>:1062                                    ; preds = %1058
  %1063 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %1064 = extractvalue %dx.types.CBufRet.f32 %1063, 0
  %1065 = extractvalue %dx.types.CBufRet.f32 %1063, 1
  %1066 = extractvalue %dx.types.CBufRet.f32 %1063, 2
  %1067 = fsub fast float %329, %1064
  %1068 = fsub fast float %333, %1065
  %1069 = fsub fast float %337, %1066
  %1070 = fmul fast float %1067, %1067
  %1071 = fmul fast float %1068, %1068
  %1072 = fadd fast float %1070, %1071
  %1073 = fmul fast float %1069, %1069
  %1074 = fadd fast float %1072, %1073
  %1075 = call float @dx.op.unary.f32(i32 24, float %1074)  ; Sqrt(value)
  %1076 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %1077 = extractvalue %dx.types.CBufRet.i32 %1076, 0
  %1078 = and i32 %1077, 31
  %1079 = shl i32 1, %1078
  %1080 = uitofp i32 %1079 to float
  %1081 = fmul fast float %1080, 5.000000e-01
  %1082 = fdiv fast float %1075, %1081
  %1083 = fmul fast float %1082, 2.000000e+00
  %1084 = call float @dx.op.unary.f32(i32 6, float %1083)  ; FAbs(value)
  %1085 = call float @dx.op.binary.f32(i32 35, float %1084, float 1.000000e+00)  ; FMax(a,b)
  %1086 = call float @dx.op.unary.f32(i32 23, float %1085)  ; Log(value)
  %1087 = fptoui float %1086 to i32
  %1088 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %1089 = extractvalue %dx.types.CBufRet.i32 %1088, 3
  %1090 = call i32 @dx.op.binary.i32(i32 37, i32 %1087, i32 %1089)  ; IMax(a,b)
  %1091 = icmp ult i32 %1090, %579
  br i1 %1091, label %1092, label %1204

; <label>:1092                                    ; preds = %1062
  br label %1093

; <label>:1093                                    ; preds = %1093, %1092
  %1094 = phi i32 [ %1165, %1093 ], [ %579, %1092 ]
  %1095 = phi i32 [ %1166, %1093 ], [ %1090, %1092 ]
  %1096 = shl i32 %1095, 2
  %1097 = add i32 %1096, 43
  %1098 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1097)  ; CBufferLoadLegacy(handle,regIndex)
  %1099 = extractvalue %dx.types.CBufRet.f32 %1098, 0
  %1100 = extractvalue %dx.types.CBufRet.f32 %1098, 1
  %1101 = add i32 %1096, 44
  %1102 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1101)  ; CBufferLoadLegacy(handle,regIndex)
  %1103 = extractvalue %dx.types.CBufRet.f32 %1102, 0
  %1104 = extractvalue %dx.types.CBufRet.f32 %1102, 1
  %1105 = add i32 %1096, 45
  %1106 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1105)  ; CBufferLoadLegacy(handle,regIndex)
  %1107 = extractvalue %dx.types.CBufRet.f32 %1106, 0
  %1108 = extractvalue %dx.types.CBufRet.f32 %1106, 1
  %1109 = add i32 %1096, 46
  %1110 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1109)  ; CBufferLoadLegacy(handle,regIndex)
  %1111 = extractvalue %dx.types.CBufRet.f32 %1110, 0
  %1112 = extractvalue %dx.types.CBufRet.f32 %1110, 1
  %1113 = fmul fast float %1099, %329
  %1114 = call float @dx.op.tertiary.f32(i32 46, float %333, float %1103, float %1113)  ; FMad(a,b,c)
  %1115 = call float @dx.op.tertiary.f32(i32 46, float %337, float %1107, float %1114)  ; FMad(a,b,c)
  %1116 = call float @dx.op.tertiary.f32(i32 46, float %341, float %1111, float %1115)  ; FMad(a,b,c)
  %1117 = fmul fast float %1100, %329
  %1118 = call float @dx.op.tertiary.f32(i32 46, float %333, float %1104, float %1117)  ; FMad(a,b,c)
  %1119 = call float @dx.op.tertiary.f32(i32 46, float %337, float %1108, float %1118)  ; FMad(a,b,c)
  %1120 = call float @dx.op.tertiary.f32(i32 46, float %341, float %1112, float %1119)  ; FMad(a,b,c)
  %1121 = call float @dx.op.binary.f32(i32 35, float %1116, float 0.000000e+00)  ; FMax(a,b)
  %1122 = call float @dx.op.binary.f32(i32 35, float %1120, float 0.000000e+00)  ; FMax(a,b)
  %1123 = call float @dx.op.binary.f32(i32 36, float %1121, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1124 = call float @dx.op.binary.f32(i32 36, float %1122, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1125 = add i32 %1095, 107
  %1126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1125)  ; CBufferLoadLegacy(handle,regIndex)
  %1127 = extractvalue %dx.types.CBufRet.f32 %1126, 0
  %1128 = extractvalue %dx.types.CBufRet.f32 %1126, 1
  %1129 = fadd fast float %1127, %1123
  %1130 = fadd fast float %1128, %1124
  %1131 = call float @dx.op.unary.f32(i32 22, float %1129)  ; Frc(value)
  %1132 = call float @dx.op.unary.f32(i32 22, float %1130)  ; Frc(value)
  %1133 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 122)  ; CBufferLoadLegacy(handle,regIndex)
  %1134 = extractvalue %dx.types.CBufRet.i32 %1133, 2
  %1135 = and i32 %1095, 31
  %1136 = shl i32 1, %1135
  %1137 = and i32 %1134, %1136
  %1138 = icmp ne i32 %1137, 0
  %1139 = fcmp fast ogt float %1116, 0.000000e+00
  %1140 = fcmp fast ogt float %1120, 0.000000e+00
  %1141 = and i1 %1139, %1140
  %1142 = fcmp fast olt float %1116, 1.000000e+00
  %1143 = fcmp fast olt float %1120, 1.000000e+00
  %1144 = and i1 %1142, %1143
  %1145 = and i1 %1141, %1144
  %1146 = and i1 %1145, %1138
  %1147 = uitofp i32 %1095 to float
  %1148 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %1149 = extractvalue %dx.types.CBufRet.f32 %1148, 0
  %1150 = extractvalue %dx.types.CBufRet.f32 %1148, 1
  %1151 = fmul fast float %1150, %1132
  %1152 = extractvalue %dx.types.CBufRet.f32 %1148, 2
  %1153 = fmul fast float %1152, %1147
  %1154 = call float @dx.op.unary.f32(i32 27, float %1151)  ; Round_ni(value)
  %1155 = fadd fast float %1154, %1131
  %1156 = fmul fast float %1155, %1149
  %1157 = fadd fast float %1156, %1153
  %1158 = fptoui float %1157 to i32
  %1159 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %1158, i32 undef)  ; BufferLoad(srv,index,wot)
  %1160 = extractvalue %dx.types.ResRet.i32 %1159, 0
  %1161 = and i32 %1160, 1
  %1162 = icmp ne i32 %1161, 0
  %1163 = and i1 %1146, %1162
  %1164 = select i1 %1163, i32 %1095, i32 %579
  %1165 = call i32 @dx.op.binary.i32(i32 40, i32 %1094, i32 %1164)  ; UMin(a,b)
  %1166 = add nuw i32 %1095, 1
  %1167 = icmp eq i32 %1166, %579
  br i1 %1167, label %1168, label %1093

; <label>:1168                                    ; preds = %1093
  %1169 = icmp ult i32 %1165, %579
  br i1 %1169, label %1170, label %1204

; <label>:1170                                    ; preds = %1168
  %1171 = shl i32 %1165, 2
  %1172 = add i32 %1171, 43
  %1173 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1172)  ; CBufferLoadLegacy(handle,regIndex)
  %1174 = extractvalue %dx.types.CBufRet.f32 %1173, 0
  %1175 = extractvalue %dx.types.CBufRet.f32 %1173, 1
  %1176 = extractvalue %dx.types.CBufRet.f32 %1173, 2
  %1177 = add i32 %1171, 44
  %1178 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1177)  ; CBufferLoadLegacy(handle,regIndex)
  %1179 = extractvalue %dx.types.CBufRet.f32 %1178, 0
  %1180 = extractvalue %dx.types.CBufRet.f32 %1178, 1
  %1181 = extractvalue %dx.types.CBufRet.f32 %1178, 2
  %1182 = add i32 %1171, 45
  %1183 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1182)  ; CBufferLoadLegacy(handle,regIndex)
  %1184 = extractvalue %dx.types.CBufRet.f32 %1183, 0
  %1185 = extractvalue %dx.types.CBufRet.f32 %1183, 1
  %1186 = extractvalue %dx.types.CBufRet.f32 %1183, 2
  %1187 = add i32 %1171, 46
  %1188 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1187)  ; CBufferLoadLegacy(handle,regIndex)
  %1189 = extractvalue %dx.types.CBufRet.f32 %1188, 0
  %1190 = extractvalue %dx.types.CBufRet.f32 %1188, 1
  %1191 = extractvalue %dx.types.CBufRet.f32 %1188, 2
  %1192 = fmul fast float %1174, %329
  %1193 = call float @dx.op.tertiary.f32(i32 46, float %333, float %1179, float %1192)  ; FMad(a,b,c)
  %1194 = call float @dx.op.tertiary.f32(i32 46, float %337, float %1184, float %1193)  ; FMad(a,b,c)
  %1195 = call float @dx.op.tertiary.f32(i32 46, float %341, float %1189, float %1194)  ; FMad(a,b,c)
  %1196 = fmul fast float %1175, %329
  %1197 = call float @dx.op.tertiary.f32(i32 46, float %333, float %1180, float %1196)  ; FMad(a,b,c)
  %1198 = call float @dx.op.tertiary.f32(i32 46, float %337, float %1185, float %1197)  ; FMad(a,b,c)
  %1199 = call float @dx.op.tertiary.f32(i32 46, float %341, float %1190, float %1198)  ; FMad(a,b,c)
  %1200 = fmul fast float %1176, %329
  %1201 = call float @dx.op.tertiary.f32(i32 46, float %333, float %1181, float %1200)  ; FMad(a,b,c)
  %1202 = call float @dx.op.tertiary.f32(i32 46, float %337, float %1186, float %1201)  ; FMad(a,b,c)
  %1203 = call float @dx.op.tertiary.f32(i32 46, float %341, float %1191, float %1202)  ; FMad(a,b,c)
  br label %1204

; <label>:1204                                    ; preds = %1170, %1168, %1062
  %1205 = phi float [ %1006, %1062 ], [ %1006, %1168 ], [ %1195, %1170 ]
  %1206 = phi float [ %1007, %1062 ], [ %1007, %1168 ], [ %1199, %1170 ]
  %1207 = phi float [ %1008, %1062 ], [ %1008, %1168 ], [ %1203, %1170 ]
  %1208 = phi i32 [ %1009, %1062 ], [ %1165, %1168 ], [ %1165, %1170 ]
  %1209 = icmp slt i32 %1208, %579
  br i1 %1209, label %1210, label %1243

; <label>:1210                                    ; preds = %1204
  %1211 = call float @dx.op.binary.f32(i32 35, float %1205, float 0.000000e+00)  ; FMax(a,b)
  %1212 = call float @dx.op.binary.f32(i32 35, float %1206, float 0.000000e+00)  ; FMax(a,b)
  %1213 = call float @dx.op.binary.f32(i32 36, float %1211, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1214 = call float @dx.op.binary.f32(i32 36, float %1212, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1215 = add i32 %1208, 107
  %1216 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 %1215)  ; CBufferLoadLegacy(handle,regIndex)
  %1217 = extractvalue %dx.types.CBufRet.f32 %1216, 0
  %1218 = extractvalue %dx.types.CBufRet.f32 %1216, 1
  %1219 = fadd fast float %1217, %1213
  %1220 = fadd fast float %1218, %1214
  %1221 = call float @dx.op.unary.f32(i32 22, float %1219)  ; Frc(value)
  %1222 = call float @dx.op.unary.f32(i32 22, float %1220)  ; Frc(value)
  %1223 = uitofp i32 %1208 to float
  %1224 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 42)  ; CBufferLoadLegacy(handle,regIndex)
  %1225 = extractvalue %dx.types.CBufRet.f32 %1224, 0
  %1226 = extractvalue %dx.types.CBufRet.f32 %1224, 1
  %1227 = fmul fast float %1226, %1222
  %1228 = extractvalue %dx.types.CBufRet.f32 %1224, 2
  %1229 = fmul fast float %1228, %1223
  %1230 = call float @dx.op.unary.f32(i32 27, float %1227)  ; Round_ni(value)
  %1231 = fadd fast float %1230, %1221
  %1232 = fmul fast float %1231, %1225
  %1233 = fadd fast float %1232, %1229
  %1234 = fptoui float %1233 to i32
  %1235 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %12, i32 %1234, i32 undef)  ; BufferLoad(srv,index,wot)
  %1236 = extractvalue %dx.types.ResRet.i32 %1235, 0
  %1237 = and i32 %1236, 1
  %1238 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %13, %dx.types.Handle %27, float %1221, float %1222, float %1223, float undef, i32 0, i32 0, i32 undef, float %1207)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1239 = extractvalue %dx.types.ResRet.f32 %1238, 0
  %1240 = icmp ne i32 %1237, 0
  %1241 = select i1 %1240, float %1239, float 1.000000e+00
  %1242 = call float @dx.op.binary.f32(i32 36, float 1.000000e+00, float %1241)  ; FMin(a,b)
  br label %1243

; <label>:1243                                    ; preds = %1210, %1204, %1058, %1056
  %1244 = phi float [ 1.000000e+00, %1058 ], [ 1.000000e+00, %1204 ], [ %1242, %1210 ], [ 1.000000e+00, %1056 ]
  %1245 = phi float [ %1006, %1058 ], [ %1205, %1204 ], [ %1205, %1210 ], [ %1006, %1056 ]
  %1246 = phi float [ %1007, %1058 ], [ %1206, %1204 ], [ %1206, %1210 ], [ %1007, %1056 ]
  %1247 = phi float [ %1008, %1058 ], [ %1207, %1204 ], [ %1207, %1210 ], [ %1008, %1056 ]
  %1248 = phi i32 [ %1009, %1058 ], [ %1208, %1204 ], [ %1208, %1210 ], [ %1009, %1056 ]
  %1249 = call float @dx.op.binary.f32(i32 36, float %1057, float %1244)  ; FMin(a,b)
  %1250 = fadd fast float %1011, %1249
  %1251 = fmul fast float %1250, 0x3FD5555560000000
  %1252 = call float @dx.op.dot3.f32(i32 55, float %367, float %368, float %369, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %1253 = fmul fast float %365, %365
  %1254 = fsub fast float 1.000000e+00, %1253
  %1255 = fadd fast float %1253, 1.000000e+00
  %1256 = fmul fast float %365, 2.000000e+00
  %1257 = fmul fast float %1256, %1252
  %1258 = fsub fast float %1255, %1257
  %1259 = call float @dx.op.unary.f32(i32 6, float %1258)  ; FAbs(value)
  %1260 = call float @dx.op.unary.f32(i32 23, float %1259)  ; Log(value)
  %1261 = fmul fast float %1260, 1.500000e+00
  %1262 = call float @dx.op.unary.f32(i32 21, float %1261)  ; Exp(value)
  %1263 = fdiv fast float %1254, %1262
  %1264 = fmul fast float %1263, 0x3FE921FB00000000
  %1265 = fmul fast float %371, %361
  %1266 = fmul fast float %1265, %1251
  %1267 = fmul fast float %1266, %1264
  %1268 = fmul fast float %372, %362
  %1269 = fmul fast float %1268, %1251
  %1270 = fmul fast float %1269, %1264
  %1271 = fmul fast float %373, %363
  %1272 = fmul fast float %1271, %1251
  %1273 = fmul fast float %1272, %1264
  br label %1274

; <label>:1274                                    ; preds = %1243, %479
  %1275 = phi float [ %496, %479 ], [ %1267, %1243 ]
  %1276 = phi float [ %497, %479 ], [ %1270, %1243 ]
  %1277 = phi float [ %498, %479 ], [ %1273, %1243 ]
  %1278 = phi float [ %353, %479 ], [ %1245, %1243 ]
  %1279 = phi float [ %354, %479 ], [ %1246, %1243 ]
  %1280 = phi float [ %355, %479 ], [ %1247, %1243 ]
  %1281 = phi i32 [ %352, %479 ], [ %1248, %1243 ]
  %1282 = fadd fast float %1275, %349
  %1283 = fadd fast float %1276, %350
  %1284 = fadd fast float %1277, %351
  %1285 = add i32 %356, 1
  %1286 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1287 = extractvalue %dx.types.CBufRet.i32 %1286, 0
  %1288 = icmp ult i32 %1285, %1287
  br i1 %1288, label %348, label %1289

; <label>:1289                                    ; preds = %1274
  br label %1290

; <label>:1290                                    ; preds = %1289, %89
  %1291 = phi float [ 0.000000e+00, %89 ], [ %1282, %1289 ]
  %1292 = phi float [ 0.000000e+00, %89 ], [ %1283, %1289 ]
  %1293 = phi float [ 0.000000e+00, %89 ], [ %1284, %1289 ]
  %1294 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1295 = extractvalue %dx.types.CBufRet.f32 %1294, 2
  %1296 = extractvalue %dx.types.CBufRet.f32 %1294, 3
  %1297 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1298 = extractvalue %dx.types.CBufRet.f32 %1297, 2
  %1299 = extractvalue %dx.types.CBufRet.f32 %1297, 3
  %1300 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1301 = extractvalue %dx.types.CBufRet.f32 %1300, 2
  %1302 = extractvalue %dx.types.CBufRet.f32 %1300, 3
  %1303 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %1304 = extractvalue %dx.types.CBufRet.f32 %1303, 2
  %1305 = extractvalue %dx.types.CBufRet.f32 %1303, 3
  %1306 = fmul fast float %1295, %216
  %1307 = call float @dx.op.tertiary.f32(i32 46, float %220, float %1298, float %1306)  ; FMad(a,b,c)
  %1308 = call float @dx.op.tertiary.f32(i32 46, float %224, float %1301, float %1307)  ; FMad(a,b,c)
  %1309 = call float @dx.op.tertiary.f32(i32 46, float %228, float %1304, float %1308)  ; FMad(a,b,c)
  %1310 = fmul fast float %1296, %216
  %1311 = call float @dx.op.tertiary.f32(i32 46, float %220, float %1299, float %1310)  ; FMad(a,b,c)
  %1312 = call float @dx.op.tertiary.f32(i32 46, float %224, float %1302, float %1311)  ; FMad(a,b,c)
  %1313 = call float @dx.op.tertiary.f32(i32 46, float %228, float %1305, float %1312)  ; FMad(a,b,c)
  %1314 = fdiv fast float %1309, %1313
  %1315 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %1316 = extractvalue %dx.types.CBufRet.f32 %1315, 0
  %1317 = fmul fast float %1316, %1314
  %1318 = extractvalue %dx.types.CBufRet.f32 %1315, 1
  %1319 = fadd fast float %1317, %1318
  %1320 = call float @dx.op.unary.f32(i32 23, float %1319)  ; Log(value)
  %1321 = extractvalue %dx.types.CBufRet.f32 %1315, 2
  %1322 = fmul fast float %1321, %1320
  %1323 = extractvalue %dx.types.CBufRet.f32 %1315, 3
  %1324 = fadd fast float %1322, %1323
  %1325 = call float @dx.op.binary.f32(i32 35, float %1324, float 0.000000e+00)  ; FMax(a,b)
  %1326 = fptoui float %1325 to i32
  %1327 = fmul fast float %1295, %184
  %1328 = call float @dx.op.tertiary.f32(i32 46, float %188, float %1298, float %1327)  ; FMad(a,b,c)
  %1329 = call float @dx.op.tertiary.f32(i32 46, float %192, float %1301, float %1328)  ; FMad(a,b,c)
  %1330 = call float @dx.op.tertiary.f32(i32 46, float %196, float %1304, float %1329)  ; FMad(a,b,c)
  %1331 = fmul fast float %1296, %184
  %1332 = call float @dx.op.tertiary.f32(i32 46, float %188, float %1299, float %1331)  ; FMad(a,b,c)
  %1333 = call float @dx.op.tertiary.f32(i32 46, float %192, float %1302, float %1332)  ; FMad(a,b,c)
  %1334 = call float @dx.op.tertiary.f32(i32 46, float %196, float %1305, float %1333)  ; FMad(a,b,c)
  %1335 = fdiv fast float %1330, %1334
  %1336 = fmul fast float %1316, %1335
  %1337 = fadd fast float %1336, %1318
  %1338 = call float @dx.op.unary.f32(i32 23, float %1337)  ; Log(value)
  %1339 = fmul fast float %1321, %1338
  %1340 = fadd fast float %1339, %1323
  %1341 = call float @dx.op.binary.f32(i32 35, float %1340, float 0.000000e+00)  ; FMax(a,b)
  %1342 = fptoui float %1341 to i32
  %1343 = shl i32 %35, 4
  br label %1344

; <label>:1344                                    ; preds = %2354, %1290
  %1345 = phi i32 [ 0, %1290 ], [ %2355, %2354 ]
  %1346 = phi i32 [ 0, %1290 ], [ %2359, %2354 ]
  %1347 = phi float [ %1291, %1290 ], [ %2356, %2354 ]
  %1348 = phi float [ %1292, %1290 ], [ %2357, %2354 ]
  %1349 = phi float [ %1293, %1290 ], [ %2358, %2354 ]
  %1350 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1343
  store i32 -1, i32 addrspace(3)* %1350, align 4, !tbaa !48, !noalias !52
  %1351 = or i32 %1343, 1
  %1352 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1351
  store i32 -1, i32 addrspace(3)* %1352, align 4, !tbaa !48, !noalias !52
  %1353 = or i32 %1343, 2
  %1354 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1353
  store i32 -1, i32 addrspace(3)* %1354, align 4, !tbaa !48, !noalias !52
  %1355 = or i32 %1343, 3
  %1356 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1355
  store i32 -1, i32 addrspace(3)* %1356, align 4, !tbaa !48, !noalias !52
  %1357 = or i32 %1343, 4
  %1358 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1357
  store i32 -1, i32 addrspace(3)* %1358, align 4, !tbaa !48, !noalias !52
  %1359 = or i32 %1343, 5
  %1360 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1359
  store i32 -1, i32 addrspace(3)* %1360, align 4, !tbaa !48, !noalias !52
  %1361 = or i32 %1343, 6
  %1362 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1361
  store i32 -1, i32 addrspace(3)* %1362, align 4, !tbaa !48, !noalias !52
  %1363 = or i32 %1343, 7
  %1364 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1363
  store i32 -1, i32 addrspace(3)* %1364, align 4, !tbaa !48, !noalias !52
  %1365 = or i32 %1343, 8
  %1366 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1365
  store i32 -1, i32 addrspace(3)* %1366, align 4, !tbaa !48, !noalias !52
  %1367 = or i32 %1343, 9
  %1368 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1367
  store i32 -1, i32 addrspace(3)* %1368, align 4, !tbaa !48, !noalias !52
  %1369 = or i32 %1343, 10
  %1370 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1369
  store i32 -1, i32 addrspace(3)* %1370, align 4, !tbaa !48, !noalias !52
  %1371 = or i32 %1343, 11
  %1372 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1371
  store i32 -1, i32 addrspace(3)* %1372, align 4, !tbaa !48, !noalias !52
  %1373 = or i32 %1343, 12
  %1374 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1373
  store i32 -1, i32 addrspace(3)* %1374, align 4, !tbaa !48, !noalias !52
  %1375 = or i32 %1343, 13
  %1376 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1375
  store i32 -1, i32 addrspace(3)* %1376, align 4, !tbaa !48, !noalias !52
  %1377 = or i32 %1343, 14
  %1378 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1377
  store i32 -1, i32 addrspace(3)* %1378, align 4, !tbaa !48, !noalias !52
  %1379 = or i32 %1343, 15
  %1380 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %1379
  store i32 -1, i32 addrspace(3)* %1380, align 4, !tbaa !48, !noalias !52
  %1381 = icmp ugt i32 %1326, %1342
  br i1 %1381, label %2354, label %1382

; <label>:1382                                    ; preds = %1344
  br label %1383

; <label>:1383                                    ; preds = %2346, %1382
  %1384 = phi i32 [ %2347, %2346 ], [ %1345, %1382 ]
  %1385 = phi i32 [ %2351, %2346 ], [ %1326, %1382 ]
  %1386 = phi float [ %2348, %2346 ], [ %1347, %1382 ]
  %1387 = phi float [ %2349, %2346 ], [ %1348, %1382 ]
  %1388 = phi float [ %2350, %2346 ], [ %1349, %1382 ]
  %1389 = icmp ult i32 %1385, 16
  br i1 %1389, label %1390, label %2346

; <label>:1390                                    ; preds = %1383
  %1391 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1392 = extractvalue %dx.types.CBufRet.i32 %1391, 0
  %1393 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %1394 = extractvalue %dx.types.CBufRet.i32 %1393, 0
  %1395 = fadd fast float %107, 5.000000e-01
  %1396 = fadd fast float %108, 5.000000e-01
  %1397 = fmul fast float %1395, 1.562500e-02
  %1398 = fptoui float %1397 to i32
  %1399 = fmul fast float %1396, 1.562500e-02
  %1400 = fptoui float %1399 to i32
  %1401 = mul i32 %1394, %1385
  %1402 = mul i32 %1392, %1400
  %1403 = add i32 %1402, %1398
  %1404 = add i32 %1403, %1401
  %1405 = shl i32 %1404, 2
  %1406 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %20, i32 %1405, i32 undef)  ; BufferLoad(srv,index,wot)
  %1407 = extractvalue %dx.types.ResRet.i32 %1406, 0
  %1408 = or i32 %1405, 1
  %1409 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %20, i32 %1408, i32 undef)  ; BufferLoad(srv,index,wot)
  %1410 = extractvalue %dx.types.ResRet.i32 %1409, 0
  %1411 = shl i32 %1410, 16
  %1412 = or i32 %1411, %1407
  %1413 = or i32 %1405, 2
  %1414 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %20, i32 %1413, i32 undef)  ; BufferLoad(srv,index,wot)
  %1415 = extractvalue %dx.types.ResRet.i32 %1414, 0
  %1416 = or i32 %1405, 3
  %1417 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %20, i32 %1416, i32 undef)  ; BufferLoad(srv,index,wot)
  %1418 = extractvalue %dx.types.ResRet.i32 %1417, 0
  %1419 = icmp eq i32 %1346, 0
  %1420 = select i1 %1419, i32 %1415, i32 %1418
  %1421 = select i1 %1419, i32 0, i32 %1415
  %1422 = add i32 %1421, %1412
  %1423 = icmp eq i32 %1420, 0
  br i1 %1423, label %2346, label %1424

; <label>:1424                                    ; preds = %1390
  br label %1425

; <label>:1425                                    ; preds = %2338, %1424
  %1426 = phi i32 [ %2339, %2338 ], [ %1384, %1424 ]
  %1427 = phi i32 [ %2343, %2338 ], [ 0, %1424 ]
  %1428 = phi float [ %2340, %2338 ], [ %1386, %1424 ]
  %1429 = phi float [ %2341, %2338 ], [ %1387, %1424 ]
  %1430 = phi float [ %2342, %2338 ], [ %1388, %1424 ]
  %1431 = add i32 %1422, %1427
  %1432 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %20, i32 %1431, i32 undef)  ; BufferLoad(srv,index,wot)
  %1433 = extractvalue %dx.types.ResRet.i32 %1432, 0
  %1434 = icmp eq i32 %1385, %1326
  br i1 %1434, label %1484, label %1435

; <label>:1435                                    ; preds = %1425
  %1436 = load i32, i32 addrspace(3)* %1350, align 4, !tbaa !48, !noalias !52
  %1437 = icmp eq i32 %1436, %1433
  %1438 = zext i1 %1437 to i32
  %1439 = load i32, i32 addrspace(3)* %1352, align 4, !tbaa !48, !noalias !52
  %1440 = icmp eq i32 %1439, %1433
  %1441 = load i32, i32 addrspace(3)* %1354, align 4, !tbaa !48, !noalias !52
  %1442 = icmp eq i32 %1441, %1433
  %1443 = or i1 %1440, %1442
  %1444 = load i32, i32 addrspace(3)* %1356, align 4, !tbaa !48, !noalias !52
  %1445 = icmp eq i32 %1444, %1433
  %1446 = or i1 %1445, %1443
  %1447 = load i32, i32 addrspace(3)* %1358, align 4, !tbaa !48, !noalias !52
  %1448 = icmp eq i32 %1447, %1433
  %1449 = or i1 %1448, %1446
  %1450 = load i32, i32 addrspace(3)* %1360, align 4, !tbaa !48, !noalias !52
  %1451 = icmp eq i32 %1450, %1433
  %1452 = or i1 %1451, %1449
  %1453 = load i32, i32 addrspace(3)* %1362, align 4, !tbaa !48, !noalias !52
  %1454 = icmp eq i32 %1453, %1433
  %1455 = or i1 %1454, %1452
  %1456 = load i32, i32 addrspace(3)* %1364, align 4, !tbaa !48, !noalias !52
  %1457 = icmp eq i32 %1456, %1433
  %1458 = or i1 %1457, %1455
  %1459 = load i32, i32 addrspace(3)* %1366, align 4, !tbaa !48, !noalias !52
  %1460 = icmp eq i32 %1459, %1433
  %1461 = or i1 %1460, %1458
  %1462 = load i32, i32 addrspace(3)* %1368, align 4, !tbaa !48, !noalias !52
  %1463 = icmp eq i32 %1462, %1433
  %1464 = or i1 %1463, %1461
  %1465 = load i32, i32 addrspace(3)* %1370, align 4, !tbaa !48, !noalias !52
  %1466 = icmp eq i32 %1465, %1433
  %1467 = or i1 %1466, %1464
  %1468 = load i32, i32 addrspace(3)* %1372, align 4, !tbaa !48, !noalias !52
  %1469 = icmp eq i32 %1468, %1433
  %1470 = or i1 %1469, %1467
  %1471 = load i32, i32 addrspace(3)* %1374, align 4, !tbaa !48, !noalias !52
  %1472 = icmp eq i32 %1471, %1433
  %1473 = or i1 %1472, %1470
  %1474 = load i32, i32 addrspace(3)* %1376, align 4, !tbaa !48, !noalias !52
  %1475 = icmp eq i32 %1474, %1433
  %1476 = or i1 %1475, %1473
  %1477 = load i32, i32 addrspace(3)* %1378, align 4, !tbaa !48, !noalias !52
  %1478 = icmp eq i32 %1477, %1433
  %1479 = or i1 %1478, %1476
  %1480 = load i32, i32 addrspace(3)* %1380, align 4, !tbaa !48, !noalias !52
  %1481 = icmp eq i32 %1480, %1433
  %1482 = or i1 %1481, %1479
  %1483 = select i1 %1482, i32 1, i32 %1438
  br label %1484

; <label>:1484                                    ; preds = %1435, %1425
  %1485 = phi i32 [ 0, %1425 ], [ %1483, %1435 ]
  %1486 = icmp eq i32 %1485, 0
  br i1 %1486, label %1487, label %2338

; <label>:1487                                    ; preds = %1484
  br i1 %1419, label %1488, label %1939

; <label>:1488                                    ; preds = %1487
  %1489 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %19, i32 %1433, i32 undef)  ; BufferLoad(srv,index,wot)
  %1490 = extractvalue %dx.types.ResRet.f32 %1489, 0
  %1491 = extractvalue %dx.types.ResRet.f32 %1489, 1
  %1492 = extractvalue %dx.types.ResRet.f32 %1489, 2
  %1493 = extractvalue %dx.types.ResRet.f32 %1489, 3
  %1494 = fcmp fast ogt float %1493, 0.000000e+00
  %1495 = fmul fast float %1493, %1493
  %1496 = select i1 %1494, float %1495, float -1.000000e+00
  %1497 = fcmp fast ogt float %1496, 0.000000e+00
  br i1 %1497, label %1498, label %2327

; <label>:1498                                    ; preds = %1488
  %1499 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %1500 = extractvalue %dx.types.CBufRet.f32 %1499, 0
  %1501 = extractvalue %dx.types.CBufRet.f32 %1499, 1
  %1502 = extractvalue %dx.types.CBufRet.f32 %1499, 2
  %1503 = fsub fast float %184, %1500
  %1504 = fsub fast float %188, %1501
  %1505 = fsub fast float %192, %1502
  %1506 = call float @dx.op.dot3.f32(i32 55, float %1503, float %1504, float %1505, float %1503, float %1504, float %1505)  ; Dot3(ax,ay,az,bx,by,bz)
  %1507 = call float @dx.op.unary.f32(i32 25, float %1506)  ; Rsqrt(value)
  %1508 = fmul fast float %1503, %1507
  %1509 = fmul fast float %1504, %1507
  %1510 = fmul fast float %1505, %1507
  %1511 = call float @dx.op.dot3.f32(i32 55, float %1508, float %1509, float %1510, float %1508, float %1509, float %1510)  ; Dot3(ax,ay,az,bx,by,bz)
  %1512 = fsub fast float %1500, %1490
  %1513 = fsub fast float %1501, %1491
  %1514 = fsub fast float %1502, %1492
  %1515 = call float @dx.op.dot3.f32(i32 55, float %1512, float %1513, float %1514, float %1508, float %1509, float %1510)  ; Dot3(ax,ay,az,bx,by,bz)
  %1516 = fmul fast float %1515, 2.000000e+00
  %1517 = call float @dx.op.dot3.f32(i32 55, float %1512, float %1513, float %1514, float %1512, float %1513, float %1514)  ; Dot3(ax,ay,az,bx,by,bz)
  %1518 = fsub fast float %1517, %1496
  %1519 = fmul fast float %1516, %1516
  %1520 = fmul fast float %1511, 4.000000e+00
  %1521 = fmul fast float %1520, %1518
  %1522 = fsub fast float %1519, %1521
  %1523 = fcmp fast ugt float %1522, 0.000000e+00
  br i1 %1523, label %1524, label %1559

; <label>:1524                                    ; preds = %1498
  %1525 = fcmp fast ogt float %1516, 0.000000e+00
  %1526 = call float @dx.op.unary.f32(i32 24, float %1522)  ; Sqrt(value)
  %1527 = fsub fast float -0.000000e+00, %1526
  %1528 = select i1 %1525, float %1526, float %1527
  %1529 = fadd fast float %1528, %1516
  %1530 = fmul float %1529, -5.000000e-01
  %1531 = fdiv fast float %1530, %1511
  %1532 = fdiv fast float %1518, %1530
  %1533 = call float @dx.op.binary.f32(i32 36, float %1531, float %1532)  ; FMin(a,b)
  %1534 = call float @dx.op.binary.f32(i32 35, float %1531, float %1532)  ; FMax(a,b)
  %1535 = fsub fast float %216, %1500
  %1536 = fsub fast float %220, %1501
  %1537 = fsub fast float %224, %1502
  %1538 = fmul fast float %1535, %1535
  %1539 = fmul fast float %1536, %1536
  %1540 = fadd fast float %1538, %1539
  %1541 = fmul fast float %1537, %1537
  %1542 = fadd fast float %1540, %1541
  %1543 = call float @dx.op.unary.f32(i32 24, float %1542)  ; Sqrt(value)
  %1544 = fmul fast float %1503, %1503
  %1545 = fmul fast float %1504, %1504
  %1546 = fadd fast float %1544, %1545
  %1547 = fmul fast float %1505, %1505
  %1548 = fadd fast float %1546, %1547
  %1549 = call float @dx.op.unary.f32(i32 24, float %1548)  ; Sqrt(value)
  %1550 = call float @dx.op.binary.f32(i32 36, float %1549, float %1533)  ; FMin(a,b)
  %1551 = call float @dx.op.binary.f32(i32 35, float %1543, float %1550)  ; FMax(a,b)
  %1552 = call float @dx.op.binary.f32(i32 35, float %1543, float %1534)  ; FMax(a,b)
  %1553 = call float @dx.op.binary.f32(i32 36, float %1549, float %1552)  ; FMin(a,b)
  %1554 = fsub fast float %1551, %1543
  %1555 = fsub fast float %1553, %1543
  %1556 = fsub fast float %1553, %1551
  %1557 = fsub fast float %1549, %1543
  %1558 = fdiv fast float %1556, %1557
  br label %1559

; <label>:1559                                    ; preds = %1524, %1498
  %1560 = phi float [ %1554, %1524 ], [ 0.000000e+00, %1498 ]
  %1561 = phi float [ %1555, %1524 ], [ 0.000000e+00, %1498 ]
  %1562 = phi float [ %1558, %1524 ], [ 0.000000e+00, %1498 ]
  %1563 = fcmp fast ogt float %1562, 0.000000e+00
  br i1 %1563, label %1564, label %2327

; <label>:1564                                    ; preds = %1559
  %1565 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1433, i32 16)  ; BufferLoad(srv,index,wot)
  %1566 = extractvalue %dx.types.ResRet.f32 %1565, 0
  %1567 = extractvalue %dx.types.ResRet.f32 %1565, 1
  %1568 = extractvalue %dx.types.ResRet.f32 %1565, 2
  %1569 = shl i32 %1433, 1
  %1570 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %3, i32 %1569, i32 undef)  ; BufferLoad(srv,index,wot)
  %1571 = extractvalue %dx.types.ResRet.f32 %1570, 0
  %1572 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1433, i32 0)  ; BufferLoad(srv,index,wot)
  %1573 = extractvalue %dx.types.ResRet.f32 %1572, 0
  %1574 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1433, i32 48)  ; BufferLoad(srv,index,wot)
  %1575 = extractvalue %dx.types.ResRet.f32 %1574, 0
  %1576 = extractvalue %dx.types.ResRet.f32 %1574, 1
  %1577 = uitofp i32 %1433 to float
  %1578 = extractvalue %dx.types.ResRet.f32 %1572, 2
  %1579 = or i32 %1569, 1
  %1580 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %3, i32 %1579, i32 undef)  ; BufferLoad(srv,index,wot)
  %1581 = extractvalue %dx.types.ResRet.f32 %1580, 0
  %1582 = mul i32 %1433, 3
  %1583 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %18, i32 %1582, i32 undef)  ; BufferLoad(srv,index,wot)
  %1584 = extractvalue %dx.types.ResRet.f32 %1583, 0
  %1585 = extractvalue %dx.types.ResRet.f32 %1583, 1
  %1586 = extractvalue %dx.types.ResRet.f32 %1583, 2
  %1587 = add i32 %1582, 1
  %1588 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %18, i32 %1587, i32 undef)  ; BufferLoad(srv,index,wot)
  %1589 = extractvalue %dx.types.ResRet.f32 %1588, 0
  %1590 = extractvalue %dx.types.ResRet.f32 %1588, 1
  %1591 = extractvalue %dx.types.ResRet.f32 %1588, 2
  %1592 = add i32 %1582, 2
  %1593 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %17, i32 %1592, i32 undef)  ; BufferLoad(srv,index,wot)
  %1594 = extractvalue %dx.types.ResRet.i32 %1593, 0
  %1595 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %18, i32 %1592, i32 undef)  ; BufferLoad(srv,index,wot)
  %1596 = extractvalue %dx.types.ResRet.f32 %1595, 0
  %1597 = extractvalue %dx.types.ResRet.f32 %1595, 1
  %1598 = extractvalue %dx.types.ResRet.f32 %1595, 2
  %1599 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %17, i32 %1582, i32 undef)  ; BufferLoad(srv,index,wot)
  %1600 = extractvalue %dx.types.ResRet.i32 %1599, 0
  %1601 = fsub fast float %183, %215
  %1602 = fsub fast float %187, %219
  %1603 = fsub fast float %191, %223
  %1604 = call float @dx.op.dot3.f32(i32 55, float %1601, float %1602, float %1603, float %1601, float %1602, float %1603)  ; Dot3(ax,ay,az,bx,by,bz)
  %1605 = call float @dx.op.unary.f32(i32 25, float %1604)  ; Rsqrt(value)
  %1606 = fmul fast float %1605, %1601
  %1607 = fmul fast float %1605, %1602
  %1608 = fmul fast float %1605, %1603
  %1609 = fmul fast float %1606, %1560
  %1610 = fmul fast float %1607, %1560
  %1611 = fmul fast float %1608, %1560
  %1612 = fsub fast float %1490, %216
  %1613 = fsub fast float %1612, %1609
  %1614 = fsub fast float %1491, %220
  %1615 = fsub fast float %1614, %1610
  %1616 = fsub fast float %1492, %224
  %1617 = fsub fast float %1616, %1611
  %1618 = call float @dx.op.dot3.f32(i32 55, float %1613, float %1615, float %1617, float %1613, float %1615, float %1617)  ; Dot3(ax,ay,az,bx,by,bz)
  %1619 = fadd fast float %1618, 0x3F10000020000000
  %1620 = icmp eq i32 %1600, 3
  br i1 %1620, label %1621, label %1624

; <label>:1621                                    ; preds = %1564
  %1622 = fmul fast float %1496, 0x3FB99999A0000000
  %1623 = call float @dx.op.binary.f32(i32 35, float %1619, float %1622)  ; FMax(a,b)
  br label %1624

; <label>:1624                                    ; preds = %1621, %1564
  %1625 = phi float [ %1623, %1621 ], [ %1619, %1564 ]
  %1626 = call float @dx.op.unary.f32(i32 25, float %1625)  ; Rsqrt(value)
  br i1 %1620, label %1627, label %1670

; <label>:1627                                    ; preds = %1624
  %1628 = fsub fast float -0.000000e+00, %1613
  %1629 = fsub fast float -0.000000e+00, %1615
  %1630 = fsub fast float -0.000000e+00, %1617
  %1631 = call float @dx.op.dot3.f32(i32 55, float %1628, float %1629, float %1630, float %1628, float %1629, float %1630)  ; Dot3(ax,ay,az,bx,by,bz)
  %1632 = call float @dx.op.unary.f32(i32 25, float %1631)  ; Rsqrt(value)
  %1633 = fmul fast float %1632, %1628
  %1634 = fmul fast float %1632, %1629
  %1635 = fmul fast float %1632, %1630
  %1636 = call float @dx.op.dot3.f32(i32 55, float %1633, float %1634, float %1635, float %1596, float %1597, float %1598)  ; Dot3(ax,ay,az,bx,by,bz)
  %1637 = fmul fast float %1636, 5.000000e-01
  %1638 = fsub fast float 5.000000e-01, %1637
  %1639 = call float @dx.op.dot3.f32(i32 55, float %1633, float %1634, float %1635, float %1589, float %1590, float %1591)  ; Dot3(ax,ay,az,bx,by,bz)
  %1640 = fmul fast float %1636, %1636
  %1641 = fsub fast float 1.000000e+00, %1640
  %1642 = call float @dx.op.unary.f32(i32 25, float %1641)  ; Rsqrt(value)
  %1643 = fmul fast float %1642, %1639
  %1644 = fmul fast float %1598, %1590
  %1645 = fmul fast float %1597, %1591
  %1646 = fsub fast float %1644, %1645
  %1647 = fmul fast float %1596, %1591
  %1648 = fmul fast float %1598, %1589
  %1649 = fsub fast float %1647, %1648
  %1650 = fmul fast float %1597, %1589
  %1651 = fmul fast float %1596, %1590
  %1652 = fsub fast float %1650, %1651
  %1653 = call float @dx.op.dot3.f32(i32 55, float %1646, float %1649, float %1652, float %1633, float %1634, float %1635)  ; Dot3(ax,ay,az,bx,by,bz)
  %1654 = fcmp fast oge float %1653, 0.000000e+00
  %1655 = fmul fast float %1643, 2.500000e-01
  %1656 = fsub fast float 2.500000e-01, %1655
  %1657 = fadd fast float %1655, 7.500000e-01
  %1658 = select i1 %1654, float %1656, float %1657
  %1659 = uitofp i32 %1594 to float
  %1660 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %21, %dx.types.Handle %29, float %1638, float %1658, float %1659, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1661 = extractvalue %dx.types.ResRet.f32 %1660, 0
  %1662 = fmul fast float %1661, %1496
  %1663 = fmul fast float %1496, %37
  %1664 = fadd fast float %1625, %1663
  %1665 = fdiv fast float %1662, %1664
  %1666 = fdiv fast float %1625, %1496
  %1667 = fsub fast float 1.000000e+00, %1666
  %1668 = call float @dx.op.unary.f32(i32 7, float %1667)  ; Saturate(value)
  %1669 = fmul fast float %1668, %1665
  br label %1682

; <label>:1670                                    ; preds = %1624
  %1671 = icmp eq i32 %1600, 2
  br i1 %1671, label %1672, label %1678

; <label>:1672                                    ; preds = %1670
  %1673 = fmul fast float %1589, 0xBFF7154760000000
  %1674 = fmul fast float %1673, %1625
  %1675 = call float @dx.op.unary.f32(i32 21, float %1674)  ; Exp(value)
  %1676 = fmul fast float %1675, %1590
  %1677 = call float @dx.op.unary.f32(i32 7, float %1676)  ; Saturate(value)
  br label %1682

; <label>:1678                                    ; preds = %1670
  %1679 = fmul fast float %1626, %1625
  %1680 = call float @dx.op.dot3.f32(i32 55, float %1589, float %1590, float %1591, float 1.000000e+00, float %1679, float %1625)  ; Dot3(ax,ay,az,bx,by,bz)
  %1681 = fdiv fast float 1.000000e+00, %1680
  br label %1682

; <label>:1682                                    ; preds = %1678, %1672, %1627
  %1683 = phi float [ %1669, %1627 ], [ %1677, %1672 ], [ %1681, %1678 ]
  %1684 = fmul fast float %1626, %1613
  %1685 = fmul fast float %1626, %1615
  %1686 = fmul fast float %1626, %1617
  %1687 = call float @dx.op.dot3.f32(i32 55, float %1684, float %1685, float %1686, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %1688 = fmul fast float %1581, %1581
  %1689 = fsub fast float 1.000000e+00, %1688
  %1690 = fadd fast float %1688, 1.000000e+00
  %1691 = fmul fast float %1581, 2.000000e+00
  %1692 = fmul fast float %1691, %1687
  %1693 = fsub fast float %1690, %1692
  %1694 = call float @dx.op.unary.f32(i32 6, float %1693)  ; FAbs(value)
  %1695 = call float @dx.op.unary.f32(i32 23, float %1694)  ; Log(value)
  %1696 = fmul fast float %1695, 1.500000e+00
  %1697 = call float @dx.op.unary.f32(i32 21, float %1696)  ; Exp(value)
  %1698 = fdiv fast float %1689, %1697
  %1699 = fmul fast float %1606, %1561
  %1700 = fmul fast float %1607, %1561
  %1701 = fmul fast float %1608, %1561
  %1702 = fsub fast float %1612, %1699
  %1703 = fsub fast float %1614, %1700
  %1704 = fsub fast float %1616, %1701
  %1705 = call float @dx.op.dot3.f32(i32 55, float %1702, float %1703, float %1704, float %1702, float %1703, float %1704)  ; Dot3(ax,ay,az,bx,by,bz)
  %1706 = fadd fast float %1705, 0x3F10000020000000
  br i1 %1620, label %1707, label %1710

; <label>:1707                                    ; preds = %1682
  %1708 = fmul fast float %1496, 0x3FB99999A0000000
  %1709 = call float @dx.op.binary.f32(i32 35, float %1706, float %1708)  ; FMax(a,b)
  br label %1710

; <label>:1710                                    ; preds = %1707, %1682
  %1711 = phi float [ %1709, %1707 ], [ %1706, %1682 ]
  %1712 = call float @dx.op.unary.f32(i32 25, float %1711)  ; Rsqrt(value)
  br i1 %1620, label %1713, label %1756

; <label>:1713                                    ; preds = %1710
  %1714 = fsub fast float -0.000000e+00, %1702
  %1715 = fsub fast float -0.000000e+00, %1703
  %1716 = fsub fast float -0.000000e+00, %1704
  %1717 = call float @dx.op.dot3.f32(i32 55, float %1714, float %1715, float %1716, float %1714, float %1715, float %1716)  ; Dot3(ax,ay,az,bx,by,bz)
  %1718 = call float @dx.op.unary.f32(i32 25, float %1717)  ; Rsqrt(value)
  %1719 = fmul fast float %1718, %1714
  %1720 = fmul fast float %1718, %1715
  %1721 = fmul fast float %1718, %1716
  %1722 = call float @dx.op.dot3.f32(i32 55, float %1719, float %1720, float %1721, float %1596, float %1597, float %1598)  ; Dot3(ax,ay,az,bx,by,bz)
  %1723 = fmul fast float %1722, 5.000000e-01
  %1724 = fsub fast float 5.000000e-01, %1723
  %1725 = call float @dx.op.dot3.f32(i32 55, float %1719, float %1720, float %1721, float %1589, float %1590, float %1591)  ; Dot3(ax,ay,az,bx,by,bz)
  %1726 = fmul fast float %1722, %1722
  %1727 = fsub fast float 1.000000e+00, %1726
  %1728 = call float @dx.op.unary.f32(i32 25, float %1727)  ; Rsqrt(value)
  %1729 = fmul fast float %1728, %1725
  %1730 = fmul fast float %1598, %1590
  %1731 = fmul fast float %1597, %1591
  %1732 = fsub fast float %1730, %1731
  %1733 = fmul fast float %1596, %1591
  %1734 = fmul fast float %1598, %1589
  %1735 = fsub fast float %1733, %1734
  %1736 = fmul fast float %1597, %1589
  %1737 = fmul fast float %1596, %1590
  %1738 = fsub fast float %1736, %1737
  %1739 = call float @dx.op.dot3.f32(i32 55, float %1732, float %1735, float %1738, float %1719, float %1720, float %1721)  ; Dot3(ax,ay,az,bx,by,bz)
  %1740 = fcmp fast oge float %1739, 0.000000e+00
  %1741 = fmul fast float %1729, 2.500000e-01
  %1742 = fsub fast float 2.500000e-01, %1741
  %1743 = fadd fast float %1741, 7.500000e-01
  %1744 = select i1 %1740, float %1742, float %1743
  %1745 = uitofp i32 %1594 to float
  %1746 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %21, %dx.types.Handle %29, float %1724, float %1744, float %1745, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1747 = extractvalue %dx.types.ResRet.f32 %1746, 0
  %1748 = fmul fast float %1747, %1496
  %1749 = fmul fast float %1496, %37
  %1750 = fadd fast float %1711, %1749
  %1751 = fdiv fast float %1748, %1750
  %1752 = fdiv fast float %1711, %1496
  %1753 = fsub fast float 1.000000e+00, %1752
  %1754 = call float @dx.op.unary.f32(i32 7, float %1753)  ; Saturate(value)
  %1755 = fmul fast float %1754, %1751
  br label %1768

; <label>:1756                                    ; preds = %1710
  %1757 = icmp eq i32 %1600, 2
  br i1 %1757, label %1758, label %1764

; <label>:1758                                    ; preds = %1756
  %1759 = fmul fast float %1589, 0xBFF7154760000000
  %1760 = fmul fast float %1759, %1711
  %1761 = call float @dx.op.unary.f32(i32 21, float %1760)  ; Exp(value)
  %1762 = fmul fast float %1761, %1590
  %1763 = call float @dx.op.unary.f32(i32 7, float %1762)  ; Saturate(value)
  br label %1768

; <label>:1764                                    ; preds = %1756
  %1765 = fmul fast float %1712, %1711
  %1766 = call float @dx.op.dot3.f32(i32 55, float %1589, float %1590, float %1591, float 1.000000e+00, float %1765, float %1711)  ; Dot3(ax,ay,az,bx,by,bz)
  %1767 = fdiv fast float 1.000000e+00, %1766
  br label %1768

; <label>:1768                                    ; preds = %1764, %1758, %1713
  %1769 = phi float [ %1755, %1713 ], [ %1763, %1758 ], [ %1767, %1764 ]
  %1770 = fmul fast float %1712, %1702
  %1771 = fmul fast float %1712, %1703
  %1772 = fmul fast float %1712, %1704
  %1773 = call float @dx.op.dot3.f32(i32 55, float %1770, float %1771, float %1772, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %1774 = fmul fast float %1691, %1773
  %1775 = fsub fast float %1690, %1774
  %1776 = call float @dx.op.unary.f32(i32 6, float %1775)  ; FAbs(value)
  %1777 = call float @dx.op.unary.f32(i32 23, float %1776)  ; Log(value)
  %1778 = fmul fast float %1777, 1.500000e+00
  %1779 = call float @dx.op.unary.f32(i32 21, float %1778)  ; Exp(value)
  %1780 = fdiv fast float %1689, %1779
  %1781 = fadd fast float %1561, %1560
  %1782 = fmul fast float %1781, 5.000000e-01
  %1783 = fmul fast float %1782, %1606
  %1784 = fmul fast float %1782, %1607
  %1785 = fmul fast float %1782, %1608
  %1786 = fadd fast float %1783, %216
  %1787 = fadd fast float %1784, %220
  %1788 = fadd fast float %1785, %224
  %1789 = fsub fast float %1490, %1786
  %1790 = fsub fast float %1491, %1787
  %1791 = fsub fast float %1492, %1788
  %1792 = call float @dx.op.dot3.f32(i32 55, float %1789, float %1790, float %1791, float %1789, float %1790, float %1791)  ; Dot3(ax,ay,az,bx,by,bz)
  %1793 = fadd fast float %1792, 0x3F10000020000000
  br i1 %1620, label %1794, label %1797

; <label>:1794                                    ; preds = %1768
  %1795 = fmul fast float %1496, 0x3FB99999A0000000
  %1796 = call float @dx.op.binary.f32(i32 35, float %1793, float %1795)  ; FMax(a,b)
  br label %1797

; <label>:1797                                    ; preds = %1794, %1768
  %1798 = phi float [ %1796, %1794 ], [ %1793, %1768 ]
  %1799 = call float @dx.op.unary.f32(i32 25, float %1798)  ; Rsqrt(value)
  br i1 %1620, label %1800, label %1843

; <label>:1800                                    ; preds = %1797
  %1801 = fsub fast float -0.000000e+00, %1789
  %1802 = fsub fast float -0.000000e+00, %1790
  %1803 = fsub fast float -0.000000e+00, %1791
  %1804 = call float @dx.op.dot3.f32(i32 55, float %1801, float %1802, float %1803, float %1801, float %1802, float %1803)  ; Dot3(ax,ay,az,bx,by,bz)
  %1805 = call float @dx.op.unary.f32(i32 25, float %1804)  ; Rsqrt(value)
  %1806 = fmul fast float %1805, %1801
  %1807 = fmul fast float %1805, %1802
  %1808 = fmul fast float %1805, %1803
  %1809 = call float @dx.op.dot3.f32(i32 55, float %1806, float %1807, float %1808, float %1596, float %1597, float %1598)  ; Dot3(ax,ay,az,bx,by,bz)
  %1810 = fmul fast float %1809, 5.000000e-01
  %1811 = fsub fast float 5.000000e-01, %1810
  %1812 = call float @dx.op.dot3.f32(i32 55, float %1806, float %1807, float %1808, float %1589, float %1590, float %1591)  ; Dot3(ax,ay,az,bx,by,bz)
  %1813 = fmul fast float %1809, %1809
  %1814 = fsub fast float 1.000000e+00, %1813
  %1815 = call float @dx.op.unary.f32(i32 25, float %1814)  ; Rsqrt(value)
  %1816 = fmul fast float %1815, %1812
  %1817 = fmul fast float %1598, %1590
  %1818 = fmul fast float %1597, %1591
  %1819 = fsub fast float %1817, %1818
  %1820 = fmul fast float %1596, %1591
  %1821 = fmul fast float %1598, %1589
  %1822 = fsub fast float %1820, %1821
  %1823 = fmul fast float %1597, %1589
  %1824 = fmul fast float %1596, %1590
  %1825 = fsub fast float %1823, %1824
  %1826 = call float @dx.op.dot3.f32(i32 55, float %1819, float %1822, float %1825, float %1806, float %1807, float %1808)  ; Dot3(ax,ay,az,bx,by,bz)
  %1827 = fcmp fast oge float %1826, 0.000000e+00
  %1828 = fmul fast float %1816, 2.500000e-01
  %1829 = fsub fast float 2.500000e-01, %1828
  %1830 = fadd fast float %1828, 7.500000e-01
  %1831 = select i1 %1827, float %1829, float %1830
  %1832 = uitofp i32 %1594 to float
  %1833 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %21, %dx.types.Handle %29, float %1811, float %1831, float %1832, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1834 = extractvalue %dx.types.ResRet.f32 %1833, 0
  %1835 = fmul fast float %1834, %1496
  %1836 = fmul fast float %1496, %37
  %1837 = fadd fast float %1798, %1836
  %1838 = fdiv fast float %1835, %1837
  %1839 = fdiv fast float %1798, %1496
  %1840 = fsub fast float 1.000000e+00, %1839
  %1841 = call float @dx.op.unary.f32(i32 7, float %1840)  ; Saturate(value)
  %1842 = fmul fast float %1841, %1838
  br label %1855

; <label>:1843                                    ; preds = %1797
  %1844 = icmp eq i32 %1600, 2
  br i1 %1844, label %1845, label %1851

; <label>:1845                                    ; preds = %1843
  %1846 = fmul fast float %1589, 0xBFF7154760000000
  %1847 = fmul fast float %1846, %1798
  %1848 = call float @dx.op.unary.f32(i32 21, float %1847)  ; Exp(value)
  %1849 = fmul fast float %1848, %1590
  %1850 = call float @dx.op.unary.f32(i32 7, float %1849)  ; Saturate(value)
  br label %1855

; <label>:1851                                    ; preds = %1843
  %1852 = fmul fast float %1799, %1798
  %1853 = call float @dx.op.dot3.f32(i32 55, float %1589, float %1590, float %1591, float 1.000000e+00, float %1852, float %1798)  ; Dot3(ax,ay,az,bx,by,bz)
  %1854 = fdiv fast float 1.000000e+00, %1853
  br label %1855

; <label>:1855                                    ; preds = %1851, %1845, %1800
  %1856 = phi float [ %1842, %1800 ], [ %1850, %1845 ], [ %1854, %1851 ]
  %1857 = fmul fast float %1799, %1789
  %1858 = fmul fast float %1799, %1790
  %1859 = fmul fast float %1799, %1791
  %1860 = call float @dx.op.dot3.f32(i32 55, float %1857, float %1858, float %1859, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %1861 = fmul fast float %1691, %1860
  %1862 = fsub fast float %1690, %1861
  %1863 = call float @dx.op.unary.f32(i32 6, float %1862)  ; FAbs(value)
  %1864 = call float @dx.op.unary.f32(i32 23, float %1863)  ; Log(value)
  %1865 = fmul fast float %1864, 1.500000e+00
  %1866 = call float @dx.op.unary.f32(i32 21, float %1865)  ; Exp(value)
  %1867 = fdiv fast float %1689, %1866
  %1868 = fadd fast float %1769, %1683
  %1869 = fadd fast float %1780, %1698
  %1870 = fmul fast float %1869, 0x3FE921FB00000000
  %1871 = fmul fast float %1856, 2.000000e+00
  %1872 = fmul fast float %1867, 0x3FF921FB00000000
  %1873 = fadd fast float %1868, %1871
  %1874 = fadd fast float %1872, %1870
  %1875 = fmul fast float %1873, 2.500000e-01
  %1876 = fmul fast float %1874, 2.500000e-01
  %1877 = fmul fast float %1566, %1562
  %1878 = fmul fast float %1877, %1571
  %1879 = fmul fast float %1878, %1584
  %1880 = fmul fast float %1879, %1875
  %1881 = fmul fast float %1880, %1876
  %1882 = fmul fast float %1567, %1562
  %1883 = fmul fast float %1882, %1571
  %1884 = fmul fast float %1883, %1585
  %1885 = fmul fast float %1884, %1875
  %1886 = fmul fast float %1885, %1876
  %1887 = fmul fast float %1568, %1562
  %1888 = fmul fast float %1887, %1571
  %1889 = fmul fast float %1888, %1586
  %1890 = fmul fast float %1889, %1875
  %1891 = fmul fast float %1890, %1876
  %1892 = fcmp fast une float %1573, 2.000000e+00
  br i1 %1892, label %1893, label %1932

; <label>:1893                                    ; preds = %1855
  %1894 = fcmp fast oeq float %1573, 1.000000e+00
  %1895 = fptosi float %1577 to i32
  %1896 = fsub fast float %1786, %1490
  %1897 = fsub fast float %1787, %1491
  %1898 = fsub fast float %1788, %1492
  %1899 = fmul fast float %1896, %1575
  %1900 = fmul fast float %1897, %1575
  %1901 = fmul fast float %1898, %1575
  %1902 = fsub fast float -0.000000e+00, %1901
  %1903 = fmul fast float %1899, %1899
  %1904 = fmul fast float %1900, %1900
  %1905 = fadd fast float %1903, %1904
  %1906 = fmul fast float %1901, %1901
  %1907 = fadd fast float %1905, %1906
  %1908 = call float @dx.op.unary.f32(i32 24, float %1907)  ; Sqrt(value)
  %1909 = fadd fast float %1908, %1576
  %1910 = call float @dx.op.unary.f32(i32 7, float %1909)  ; Saturate(value)
  %1911 = sitofp i32 %1895 to float
  br i1 %1894, label %1912, label %1922

; <label>:1912                                    ; preds = %1893
  %1913 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %7, %dx.types.Handle %22, float %1899, float %1900, float %1902, float %1911, i32 undef, i32 undef, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1914 = extractvalue %dx.types.ResRet.f32 %1913, 0
  %1915 = fmul fast float %1578, 0xBFF7154760000000
  %1916 = fmul fast float %1915, %1910
  %1917 = call float @dx.op.unary.f32(i32 21, float %1916)  ; Exp(value)
  %1918 = fmul fast float %1917, 0x3FF3333340000000
  %1919 = fmul fast float %1918, %1914
  %1920 = fadd fast float %1919, 0xBFC99999A0000000
  %1921 = call float @dx.op.unary.f32(i32 7, float %1920)  ; Saturate(value)
  br label %1927

; <label>:1922                                    ; preds = %1893
  %1923 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %7, %dx.types.Handle %22, float %1899, float %1900, float %1902, float %1911, i32 undef, i32 undef, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1924 = extractvalue %dx.types.ResRet.f32 %1923, 0
  %1925 = fcmp fast ogt float %1924, %1910
  %1926 = select i1 %1925, float 1.000000e+00, float 0.000000e+00
  br label %1927

; <label>:1927                                    ; preds = %1922, %1912
  %1928 = phi float [ %1921, %1912 ], [ %1926, %1922 ]
  %1929 = fmul fast float %1928, %1881
  %1930 = fmul fast float %1928, %1886
  %1931 = fmul fast float %1928, %1891
  br label %1932

; <label>:1932                                    ; preds = %1927, %1855
  %1933 = phi float [ %1929, %1927 ], [ %1881, %1855 ]
  %1934 = phi float [ %1930, %1927 ], [ %1886, %1855 ]
  %1935 = phi float [ %1931, %1927 ], [ %1891, %1855 ]
  %1936 = fadd fast float %1933, %1428
  %1937 = fadd fast float %1934, %1429
  %1938 = fadd fast float %1935, %1430
  br label %2327

; <label>:1939                                    ; preds = %1487
  %1940 = mul i32 %1433, 5
  %1941 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %16, i32 %1940, i32 undef)  ; BufferLoad(srv,index,wot)
  %1942 = extractvalue %dx.types.ResRet.f32 %1941, 0
  %1943 = extractvalue %dx.types.ResRet.f32 %1941, 1
  %1944 = extractvalue %dx.types.ResRet.f32 %1941, 2
  %1945 = add i32 %1940, 4
  %1946 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %16, i32 %1945, i32 undef)  ; BufferLoad(srv,index,wot)
  %1947 = extractvalue %dx.types.ResRet.f32 %1946, 0
  %1948 = extractvalue %dx.types.ResRet.f32 %1946, 1
  %1949 = extractvalue %dx.types.ResRet.f32 %1946, 2
  %1950 = add i32 %1940, 1
  %1951 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %16, i32 %1950, i32 undef)  ; BufferLoad(srv,index,wot)
  %1952 = extractvalue %dx.types.ResRet.f32 %1951, 0
  %1953 = extractvalue %dx.types.ResRet.f32 %1951, 1
  %1954 = extractvalue %dx.types.ResRet.f32 %1951, 2
  %1955 = add i32 %1940, 2
  %1956 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %16, i32 %1955, i32 undef)  ; BufferLoad(srv,index,wot)
  %1957 = extractvalue %dx.types.ResRet.f32 %1956, 0
  %1958 = extractvalue %dx.types.ResRet.f32 %1956, 1
  %1959 = extractvalue %dx.types.ResRet.f32 %1956, 2
  %1960 = add i32 %1940, 3
  %1961 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %16, i32 %1960, i32 undef)  ; BufferLoad(srv,index,wot)
  %1962 = extractvalue %dx.types.ResRet.f32 %1961, 0
  %1963 = extractvalue %dx.types.ResRet.f32 %1961, 1
  %1964 = extractvalue %dx.types.ResRet.f32 %1961, 2
  %1965 = fcmp fast ogt float %1947, 0.000000e+00
  br i1 %1965, label %1966, label %2327

; <label>:1966                                    ; preds = %1939
  %1967 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %1968 = extractvalue %dx.types.CBufRet.f32 %1967, 0
  %1969 = extractvalue %dx.types.CBufRet.f32 %1967, 1
  %1970 = extractvalue %dx.types.CBufRet.f32 %1967, 2
  %1971 = fsub fast float %184, %1968
  %1972 = fsub fast float %188, %1969
  %1973 = fsub fast float %192, %1970
  %1974 = call float @dx.op.dot3.f32(i32 55, float %1971, float %1972, float %1973, float %1971, float %1972, float %1973)  ; Dot3(ax,ay,az,bx,by,bz)
  %1975 = call float @dx.op.unary.f32(i32 25, float %1974)  ; Rsqrt(value)
  %1976 = fmul fast float %1971, %1975
  %1977 = fmul fast float %1972, %1975
  %1978 = fmul fast float %1973, %1975
  %1979 = call float @dx.op.dot3.f32(i32 55, float %1976, float %1977, float %1978, float %1976, float %1977, float %1978)  ; Dot3(ax,ay,az,bx,by,bz)
  %1980 = fsub fast float %1968, %1942
  %1981 = fsub fast float %1969, %1943
  %1982 = fsub fast float %1970, %1944
  %1983 = call float @dx.op.dot3.f32(i32 55, float %1980, float %1981, float %1982, float %1976, float %1977, float %1978)  ; Dot3(ax,ay,az,bx,by,bz)
  %1984 = fmul fast float %1983, 2.000000e+00
  %1985 = call float @dx.op.dot3.f32(i32 55, float %1980, float %1981, float %1982, float %1980, float %1981, float %1982)  ; Dot3(ax,ay,az,bx,by,bz)
  %1986 = fsub fast float %1985, %1947
  %1987 = fmul fast float %1984, %1984
  %1988 = fmul fast float %1979, 4.000000e+00
  %1989 = fmul fast float %1988, %1986
  %1990 = fsub fast float %1987, %1989
  %1991 = fcmp fast ugt float %1990, 0.000000e+00
  br i1 %1991, label %1992, label %2025

; <label>:1992                                    ; preds = %1966
  %1993 = fcmp fast ogt float %1984, 0.000000e+00
  %1994 = call float @dx.op.unary.f32(i32 24, float %1990)  ; Sqrt(value)
  %1995 = fsub fast float -0.000000e+00, %1994
  %1996 = select i1 %1993, float %1994, float %1995
  %1997 = fadd fast float %1996, %1984
  %1998 = fmul float %1997, -5.000000e-01
  %1999 = fdiv fast float %1998, %1979
  %2000 = fdiv fast float %1986, %1998
  %2001 = call float @dx.op.binary.f32(i32 36, float %1999, float %2000)  ; FMin(a,b)
  %2002 = call float @dx.op.binary.f32(i32 35, float %1999, float %2000)  ; FMax(a,b)
  %2003 = fsub fast float %216, %1968
  %2004 = fsub fast float %220, %1969
  %2005 = fsub fast float %224, %1970
  %2006 = fmul fast float %2003, %2003
  %2007 = fmul fast float %2004, %2004
  %2008 = fadd fast float %2006, %2007
  %2009 = fmul fast float %2005, %2005
  %2010 = fadd fast float %2008, %2009
  %2011 = call float @dx.op.unary.f32(i32 24, float %2010)  ; Sqrt(value)
  %2012 = fmul fast float %1971, %1971
  %2013 = fmul fast float %1972, %1972
  %2014 = fadd fast float %2012, %2013
  %2015 = fmul fast float %1973, %1973
  %2016 = fadd fast float %2014, %2015
  %2017 = call float @dx.op.unary.f32(i32 24, float %2016)  ; Sqrt(value)
  %2018 = call float @dx.op.binary.f32(i32 36, float %2017, float %2001)  ; FMin(a,b)
  %2019 = call float @dx.op.binary.f32(i32 35, float %2011, float %2018)  ; FMax(a,b)
  %2020 = call float @dx.op.binary.f32(i32 35, float %2011, float %2002)  ; FMax(a,b)
  %2021 = call float @dx.op.binary.f32(i32 36, float %2017, float %2020)  ; FMin(a,b)
  %2022 = fsub fast float %2021, %2019
  %2023 = fsub fast float %2017, %2011
  %2024 = fdiv fast float %2022, %2023
  br label %2025

; <label>:2025                                    ; preds = %1992, %1966
  %2026 = phi float [ %2024, %1992 ], [ 0.000000e+00, %1966 ]
  %2027 = fcmp fast ogt float %2026, 0.000000e+00
  br i1 %2027, label %2028, label %2327

; <label>:2028                                    ; preds = %2025
  %2029 = mul nsw i32 %1433, 3
  %2030 = add nsw i32 %2029, 2
  %2031 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %15, i32 %2030, i32 undef)  ; BufferLoad(srv,index,wot)
  %2032 = extractvalue %dx.types.ResRet.i32 %2031, 0
  %2033 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 16)  ; BufferLoad(srv,index,wot)
  %2034 = extractvalue %dx.types.ResRet.f32 %2033, 0
  %2035 = extractvalue %dx.types.ResRet.f32 %2033, 1
  %2036 = extractvalue %dx.types.ResRet.f32 %2033, 2
  %2037 = shl i32 %1433, 1
  %2038 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2037, i32 undef)  ; BufferLoad(srv,index,wot)
  %2039 = extractvalue %dx.types.ResRet.f32 %2038, 0
  %2040 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 0)  ; BufferLoad(srv,index,wot)
  %2041 = extractvalue %dx.types.ResRet.f32 %2040, 0
  %2042 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 48)  ; BufferLoad(srv,index,wot)
  %2043 = extractvalue %dx.types.ResRet.f32 %2042, 0
  %2044 = extractvalue %dx.types.ResRet.f32 %2042, 1
  %2045 = extractvalue %dx.types.ResRet.f32 %2042, 2
  %2046 = extractvalue %dx.types.ResRet.f32 %2042, 3
  %2047 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 64)  ; BufferLoad(srv,index,wot)
  %2048 = extractvalue %dx.types.ResRet.f32 %2047, 0
  %2049 = extractvalue %dx.types.ResRet.f32 %2047, 1
  %2050 = extractvalue %dx.types.ResRet.f32 %2047, 2
  %2051 = extractvalue %dx.types.ResRet.f32 %2047, 3
  %2052 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 80)  ; BufferLoad(srv,index,wot)
  %2053 = extractvalue %dx.types.ResRet.f32 %2052, 0
  %2054 = extractvalue %dx.types.ResRet.f32 %2052, 1
  %2055 = extractvalue %dx.types.ResRet.f32 %2052, 2
  %2056 = extractvalue %dx.types.ResRet.f32 %2052, 3
  %2057 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %4, i32 %1433, i32 96)  ; BufferLoad(srv,index,wot)
  %2058 = extractvalue %dx.types.ResRet.f32 %2057, 0
  %2059 = extractvalue %dx.types.ResRet.f32 %2057, 1
  %2060 = extractvalue %dx.types.ResRet.f32 %2057, 2
  %2061 = extractvalue %dx.types.ResRet.f32 %2057, 3
  %2062 = uitofp i32 %1433 to float
  %2063 = extractvalue %dx.types.ResRet.f32 %2040, 2
  %2064 = or i32 %2037, 1
  %2065 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2064, i32 undef)  ; BufferLoad(srv,index,wot)
  %2066 = extractvalue %dx.types.ResRet.f32 %2065, 0
  %2067 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %15, i32 %2029, i32 undef)  ; BufferLoad(srv,index,wot)
  %2068 = extractvalue %dx.types.ResRet.i32 %2067, 0
  %2069 = fsub fast float %183, %215
  %2070 = fsub fast float %187, %219
  %2071 = fsub fast float %191, %223
  %2072 = call float @dx.op.dot3.f32(i32 55, float %2069, float %2070, float %2071, float %2069, float %2070, float %2071)  ; Dot3(ax,ay,az,bx,by,bz)
  %2073 = call float @dx.op.unary.f32(i32 25, float %2072)  ; Rsqrt(value)
  %2074 = fmul fast float %2073, %2069
  %2075 = fmul fast float %2073, %2070
  %2076 = fmul fast float %2073, %2071
  %2077 = fmul fast float %2069, %2069
  %2078 = fmul fast float %2070, %2070
  %2079 = fadd fast float %2078, %2077
  %2080 = fmul fast float %2071, %2071
  %2081 = fadd fast float %2079, %2080
  %2082 = call float @dx.op.unary.f32(i32 24, float %2081)  ; Sqrt(value)
  %2083 = fmul fast float %2076, %1953
  %2084 = fmul fast float %2075, %1954
  %2085 = fsub fast float %2083, %2084
  %2086 = fmul fast float %2074, %1954
  %2087 = fmul fast float %2076, %1952
  %2088 = fsub fast float %2086, %2087
  %2089 = fmul fast float %2075, %1952
  %2090 = fmul fast float %2074, %1953
  %2091 = fsub fast float %2089, %2090
  %2092 = call float @dx.op.dot3.f32(i32 55, float %2085, float %2088, float %2091, float %2085, float %2088, float %2091)  ; Dot3(ax,ay,az,bx,by,bz)
  %2093 = call float @dx.op.dot3.f32(i32 55, float %2074, float %2075, float %2076, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2094 = fcmp fast olt float %2092, 0x3EB0C6F7A0000000
  %2095 = fsub fast float %216, %1942
  %2096 = fsub fast float %220, %1943
  %2097 = fsub fast float %224, %1944
  br i1 %2094, label %2098, label %2118

; <label>:2098                                    ; preds = %2028
  %2099 = call float @dx.op.dot3.f32(i32 55, float %2095, float %2096, float %2097, float %2095, float %2096, float %2097)  ; Dot3(ax,ay,az,bx,by,bz)
  %2100 = call float @dx.op.unary.f32(i32 25, float %2099)  ; Rsqrt(value)
  %2101 = fmul fast float %2100, %2095
  %2102 = fmul fast float %2100, %2096
  %2103 = fmul fast float %2100, %2097
  %2104 = fsub fast float %184, %1942
  %2105 = fsub fast float %188, %1943
  %2106 = fsub fast float %192, %1944
  %2107 = call float @dx.op.dot3.f32(i32 55, float %2104, float %2105, float %2106, float %2104, float %2105, float %2106)  ; Dot3(ax,ay,az,bx,by,bz)
  %2108 = call float @dx.op.unary.f32(i32 25, float %2107)  ; Rsqrt(value)
  %2109 = fmul fast float %2108, %2104
  %2110 = fmul fast float %2108, %2105
  %2111 = fmul fast float %2108, %2106
  %2112 = call float @dx.op.dot3.f32(i32 55, float %2101, float %2102, float %2103, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2113 = call float @dx.op.dot3.f32(i32 55, float %2109, float %2110, float %2111, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2114 = fcmp fast ogt float %2112, %2113
  %2115 = select i1 %2114, float %216, float %184
  %2116 = select i1 %2114, float %220, float %188
  %2117 = select i1 %2114, float %224, float %192
  br label %2154

; <label>:2118                                    ; preds = %2028
  %2119 = fsub fast float %1942, %216
  %2120 = fsub fast float %1943, %220
  %2121 = fsub fast float %1944, %224
  %2122 = call float @dx.op.dot3.f32(i32 55, float %2119, float %2120, float %2121, float %2074, float %2075, float %2076)  ; Dot3(ax,ay,az,bx,by,bz)
  %2123 = call float @dx.op.dot3.f32(i32 55, float %2119, float %2120, float %2121, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2124 = fmul fast float %2123, %2093
  %2125 = fsub fast float %2122, %2124
  %2126 = fmul fast float %2093, %2093
  %2127 = fsub fast float 1.000000e+00, %2126
  %2128 = fdiv fast float %2125, %2127
  %2129 = call float @dx.op.binary.f32(i32 36, float %2128, float %2082)  ; FMin(a,b)
  %2130 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2129)  ; FMax(a,b)
  %2131 = fmul fast float %2130, %2074
  %2132 = fmul fast float %2130, %2075
  %2133 = fmul fast float %2130, %2076
  %2134 = fdiv fast float %1948, %1949
  %2135 = fmul fast float %2134, %2134
  %2136 = fsub fast float 1.000000e+00, %2135
  %2137 = call float @dx.op.unary.f32(i32 24, float %2136)  ; Sqrt(value)
  %2138 = fdiv fast float %2137, %2134
  %2139 = fadd fast float %2095, %2131
  %2140 = fadd fast float %2096, %2132
  %2141 = fadd fast float %2097, %2133
  %2142 = fmul fast float %2139, %2139
  %2143 = fmul fast float %2140, %2140
  %2144 = fadd fast float %2142, %2143
  %2145 = fmul fast float %2141, %2141
  %2146 = fadd fast float %2144, %2145
  %2147 = call float @dx.op.unary.f32(i32 24, float %2146)  ; Sqrt(value)
  %2148 = fmul fast float %2147, %2138
  %2149 = fdiv fast float %2148, %2082
  %2150 = call float @dx.op.unary.f32(i32 7, float %2149)  ; Saturate(value)
  %2151 = fadd fast float %2131, %216
  %2152 = fadd fast float %2132, %220
  %2153 = fadd fast float %2133, %224
  br label %2154

; <label>:2154                                    ; preds = %2118, %2098
  %2155 = phi float [ %2115, %2098 ], [ %2151, %2118 ]
  %2156 = phi float [ %2116, %2098 ], [ %2152, %2118 ]
  %2157 = phi float [ %2117, %2098 ], [ %2153, %2118 ]
  %2158 = phi float [ 1.000000e+00, %2098 ], [ %2150, %2118 ]
  %2159 = fsub fast float %1942, %2155
  %2160 = fsub fast float %1943, %2156
  %2161 = fsub fast float %1944, %2157
  %2162 = call float @dx.op.dot3.f32(i32 55, float %2159, float %2160, float %2161, float %2159, float %2160, float %2161)  ; Dot3(ax,ay,az,bx,by,bz)
  %2163 = fadd fast float %2162, 0x3F10000020000000
  %2164 = icmp eq i32 %2068, 3
  br i1 %2164, label %2165, label %2168

; <label>:2165                                    ; preds = %2154
  %2166 = fmul fast float %1947, 0x3FB99999A0000000
  %2167 = call float @dx.op.binary.f32(i32 35, float %2163, float %2166)  ; FMax(a,b)
  br label %2168

; <label>:2168                                    ; preds = %2165, %2154
  %2169 = phi float [ %2167, %2165 ], [ %2163, %2154 ]
  %2170 = call float @dx.op.unary.f32(i32 25, float %2169)  ; Rsqrt(value)
  %2171 = fmul fast float %2170, %2159
  %2172 = fmul fast float %2170, %2160
  %2173 = fmul fast float %2170, %2161
  %2174 = call float @dx.op.dot3.f32(i32 55, float %2171, float %2172, float %2173, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2175 = fmul fast float %2174, %1949
  %2176 = fadd fast float %2175, %1948
  %2177 = call float @dx.op.unary.f32(i32 7, float %2176)  ; Saturate(value)
  br i1 %2164, label %2178, label %2222

; <label>:2178                                    ; preds = %2168
  %2179 = fsub fast float -0.000000e+00, %2159
  %2180 = fsub fast float -0.000000e+00, %2160
  %2181 = fsub fast float -0.000000e+00, %2161
  %2182 = call float @dx.op.dot3.f32(i32 55, float %2179, float %2180, float %2181, float %2179, float %2180, float %2181)  ; Dot3(ax,ay,az,bx,by,bz)
  %2183 = call float @dx.op.unary.f32(i32 25, float %2182)  ; Rsqrt(value)
  %2184 = fmul fast float %2183, %2179
  %2185 = fmul fast float %2183, %2180
  %2186 = fmul fast float %2183, %2181
  %2187 = call float @dx.op.dot3.f32(i32 55, float %2184, float %2185, float %2186, float %1952, float %1953, float %1954)  ; Dot3(ax,ay,az,bx,by,bz)
  %2188 = fmul fast float %2187, 5.000000e-01
  %2189 = fsub fast float 5.000000e-01, %2188
  %2190 = call float @dx.op.dot3.f32(i32 55, float %2184, float %2185, float %2186, float %1962, float %1963, float %1964)  ; Dot3(ax,ay,az,bx,by,bz)
  %2191 = fmul fast float %2187, %2187
  %2192 = fsub fast float 1.000000e+00, %2191
  %2193 = call float @dx.op.unary.f32(i32 25, float %2192)  ; Rsqrt(value)
  %2194 = fmul fast float %2193, %2190
  %2195 = fmul fast float %1963, %1954
  %2196 = fmul fast float %1964, %1953
  %2197 = fsub fast float %2195, %2196
  %2198 = fmul fast float %1964, %1952
  %2199 = fmul fast float %1962, %1954
  %2200 = fsub fast float %2198, %2199
  %2201 = fmul fast float %1962, %1953
  %2202 = fmul fast float %1963, %1952
  %2203 = fsub fast float %2201, %2202
  %2204 = call float @dx.op.dot3.f32(i32 55, float %2197, float %2200, float %2203, float %2184, float %2185, float %2186)  ; Dot3(ax,ay,az,bx,by,bz)
  %2205 = fcmp fast oge float %2204, 0.000000e+00
  %2206 = fmul fast float %2194, 2.500000e-01
  %2207 = fsub fast float 2.500000e-01, %2206
  %2208 = fadd fast float %2206, 7.500000e-01
  %2209 = select i1 %2205, float %2207, float %2208
  %2210 = uitofp i32 %2032 to float
  %2211 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %21, %dx.types.Handle %29, float %2189, float %2209, float %2210, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2212 = extractvalue %dx.types.ResRet.f32 %2211, 0
  %2213 = fmul fast float %2212, %1947
  %2214 = fmul fast float %1947, %37
  %2215 = fadd fast float %2169, %2214
  %2216 = fdiv fast float %2213, %2215
  %2217 = fdiv fast float %2169, %1947
  %2218 = fsub fast float 1.000000e+00, %2217
  %2219 = call float @dx.op.unary.f32(i32 7, float %2218)  ; Saturate(value)
  %2220 = fmul fast float %2216, %2177
  %2221 = fmul fast float %2220, %2219
  br label %2235

; <label>:2222                                    ; preds = %2168
  %2223 = icmp eq i32 %2068, 2
  br i1 %2223, label %2224, label %2231

; <label>:2224                                    ; preds = %2222
  %2225 = fmul fast float %1962, 0xBFF7154760000000
  %2226 = fmul fast float %2225, %2169
  %2227 = call float @dx.op.unary.f32(i32 21, float %2226)  ; Exp(value)
  %2228 = fmul fast float %2227, %1963
  %2229 = call float @dx.op.unary.f32(i32 7, float %2228)  ; Saturate(value)
  %2230 = fmul fast float %2229, %2177
  br label %2235

; <label>:2231                                    ; preds = %2222
  %2232 = fmul fast float %2170, %2169
  %2233 = call float @dx.op.dot3.f32(i32 55, float 1.000000e+00, float %2232, float %2169, float %1962, float %1963, float %1964)  ; Dot3(ax,ay,az,bx,by,bz)
  %2234 = fdiv fast float %2177, %2233
  br label %2235

; <label>:2235                                    ; preds = %2231, %2224, %2178
  %2236 = phi float [ %2221, %2178 ], [ %2230, %2224 ], [ %2234, %2231 ]
  %2237 = call float @dx.op.dot3.f32(i32 55, float %2171, float %2172, float %2173, float %259, float %260, float %261)  ; Dot3(ax,ay,az,bx,by,bz)
  %2238 = fmul fast float %2066, %2066
  %2239 = fsub fast float 1.000000e+00, %2238
  %2240 = fadd fast float %2238, 1.000000e+00
  %2241 = fmul fast float %2066, 2.000000e+00
  %2242 = fmul fast float %2241, %2237
  %2243 = fsub fast float %2240, %2242
  %2244 = call float @dx.op.unary.f32(i32 6, float %2243)  ; FAbs(value)
  %2245 = call float @dx.op.unary.f32(i32 23, float %2244)  ; Log(value)
  %2246 = fmul fast float %2245, 1.500000e+00
  %2247 = call float @dx.op.unary.f32(i32 21, float %2246)  ; Exp(value)
  %2248 = fdiv fast float %2239, %2247
  %2249 = fmul fast float %2248, 0x3FE921FB00000000
  %2250 = call float @dx.op.binary.f32(i32 35, float %2176, float 0.000000e+00)  ; FMax(a,b)
  %2251 = call float @dx.op.binary.f32(i32 36, float %2250, float 1.000000e+00)  ; FMin(a,b)
  %2252 = fmul fast float %2034, %1957
  %2253 = fmul fast float %2252, %2039
  %2254 = fmul fast float %2253, %2158
  %2255 = fmul fast float %2254, %2236
  %2256 = fmul fast float %2255, %2249
  %2257 = fmul fast float %2256, %2251
  %2258 = fmul fast float %2035, %1958
  %2259 = fmul fast float %2258, %2039
  %2260 = fmul fast float %2259, %2158
  %2261 = fmul fast float %2260, %2236
  %2262 = fmul fast float %2261, %2249
  %2263 = fmul fast float %2262, %2251
  %2264 = fmul fast float %2036, %1959
  %2265 = fmul fast float %2264, %2039
  %2266 = fmul fast float %2265, %2158
  %2267 = fmul fast float %2266, %2236
  %2268 = fmul fast float %2267, %2249
  %2269 = fmul fast float %2268, %2251
  %2270 = fcmp fast une float %2041, 2.000000e+00
  br i1 %2270, label %2271, label %2320

; <label>:2271                                    ; preds = %2235
  %2272 = fmul fast float %2043, %139
  %2273 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2048, float %2272)  ; FMad(a,b,c)
  %2274 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2053, float %2273)  ; FMad(a,b,c)
  %2275 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2058, float %2274)  ; FMad(a,b,c)
  %2276 = fmul fast float %2044, %139
  %2277 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2049, float %2276)  ; FMad(a,b,c)
  %2278 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2054, float %2277)  ; FMad(a,b,c)
  %2279 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2059, float %2278)  ; FMad(a,b,c)
  %2280 = fmul fast float %2045, %139
  %2281 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2050, float %2280)  ; FMad(a,b,c)
  %2282 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2055, float %2281)  ; FMad(a,b,c)
  %2283 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2060, float %2282)  ; FMad(a,b,c)
  %2284 = fmul fast float %2046, %139
  %2285 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2051, float %2284)  ; FMad(a,b,c)
  %2286 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2056, float %2285)  ; FMad(a,b,c)
  %2287 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2061, float %2286)  ; FMad(a,b,c)
  %2288 = fdiv fast float %2275, %2287
  %2289 = fdiv fast float %2279, %2287
  %2290 = fdiv fast float %2283, %2287
  %2291 = fcmp fast olt float %2288, 0.000000e+00
  %2292 = fcmp fast ogt float %2288, 1.000000e+00
  %2293 = or i1 %2291, %2292
  %2294 = fcmp fast olt float %2289, 0.000000e+00
  %2295 = or i1 %2293, %2294
  %2296 = fcmp fast ogt float %2289, 1.000000e+00
  %2297 = or i1 %2296, %2295
  br i1 %2297, label %2315, label %2298

; <label>:2298                                    ; preds = %2271
  %2299 = fptosi float %2062 to i32
  %2300 = fcmp fast oeq float %2041, 1.000000e+00
  %2301 = sitofp i32 %2299 to float
  %2302 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %23, float %2288, float %2289, float %2301, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2303 = extractvalue %dx.types.ResRet.f32 %2302, 0
  br i1 %2300, label %2304, label %2312

; <label>:2304                                    ; preds = %2298
  %2305 = fmul fast float %2063, 0xBFF7154760000000
  %2306 = fmul fast float %2305, %2290
  %2307 = call float @dx.op.unary.f32(i32 21, float %2306)  ; Exp(value)
  %2308 = fmul fast float %2303, 0x3FF3333340000000
  %2309 = fmul fast float %2308, %2307
  %2310 = fadd fast float %2309, 0xBFC99999A0000000
  %2311 = call float @dx.op.unary.f32(i32 7, float %2310)  ; Saturate(value)
  br label %2315

; <label>:2312                                    ; preds = %2298
  %2313 = fcmp fast ogt float %2303, %2290
  %2314 = select i1 %2313, float 1.000000e+00, float 0.000000e+00
  br label %2315

; <label>:2315                                    ; preds = %2312, %2304, %2271
  %2316 = phi float [ %2311, %2304 ], [ %2314, %2312 ], [ 1.000000e+00, %2271 ]
  %2317 = fmul fast float %2316, %2257
  %2318 = fmul fast float %2316, %2263
  %2319 = fmul fast float %2316, %2269
  br label %2320

; <label>:2320                                    ; preds = %2315, %2235
  %2321 = phi float [ %2317, %2315 ], [ %2257, %2235 ]
  %2322 = phi float [ %2318, %2315 ], [ %2263, %2235 ]
  %2323 = phi float [ %2319, %2315 ], [ %2269, %2235 ]
  %2324 = fadd fast float %2321, %1428
  %2325 = fadd fast float %2322, %1429
  %2326 = fadd fast float %2323, %1430
  br label %2327

; <label>:2327                                    ; preds = %2320, %2025, %1939, %1932, %1559, %1488
  %2328 = phi i1 [ true, %2320 ], [ false, %2025 ], [ false, %1939 ], [ true, %1932 ], [ false, %1559 ], [ false, %1488 ]
  %2329 = phi float [ %2324, %2320 ], [ %1428, %2025 ], [ %1428, %1939 ], [ %1936, %1932 ], [ %1428, %1559 ], [ %1428, %1488 ]
  %2330 = phi float [ %2325, %2320 ], [ %1429, %2025 ], [ %1429, %1939 ], [ %1937, %1932 ], [ %1429, %1559 ], [ %1429, %1488 ]
  %2331 = phi float [ %2326, %2320 ], [ %1430, %2025 ], [ %1430, %1939 ], [ %1938, %1932 ], [ %1430, %1559 ], [ %1430, %1488 ]
  %2332 = icmp ult i32 %1426, 16
  %2333 = and i1 %2332, %2328
  br i1 %2333, label %2334, label %2338

; <label>:2334                                    ; preds = %2327
  %2335 = add i32 %1426, %1343
  %2336 = getelementptr [4096 x i32], [4096 x i32] addrspace(3)* @"\01?ldsAppliedLightIndices@@3PAHA", i32 0, i32 %2335
  store i32 %1433, i32 addrspace(3)* %2336, align 4, !tbaa !48, !noalias !52
  %2337 = add i32 %1426, 1
  br label %2338

; <label>:2338                                    ; preds = %2334, %2327, %1484
  %2339 = phi i32 [ %1426, %1484 ], [ %2337, %2334 ], [ %1426, %2327 ]
  %2340 = phi float [ %1428, %1484 ], [ %2329, %2334 ], [ %2329, %2327 ]
  %2341 = phi float [ %1429, %1484 ], [ %2330, %2334 ], [ %2330, %2327 ]
  %2342 = phi float [ %1430, %1484 ], [ %2331, %2334 ], [ %2331, %2327 ]
  %2343 = add nuw i32 %1427, 1
  %2344 = icmp eq i32 %2343, %1420
  br i1 %2344, label %2345, label %1425

; <label>:2345                                    ; preds = %2338
  br label %2346

; <label>:2346                                    ; preds = %2345, %1390, %1383
  %2347 = phi i32 [ %1384, %1383 ], [ %1384, %1390 ], [ %2339, %2345 ]
  %2348 = phi float [ %1386, %1383 ], [ %1386, %1390 ], [ %2340, %2345 ]
  %2349 = phi float [ %1387, %1383 ], [ %1387, %1390 ], [ %2341, %2345 ]
  %2350 = phi float [ %1388, %1383 ], [ %1388, %1390 ], [ %2342, %2345 ]
  %2351 = add i32 %1385, 1
  %2352 = icmp ugt i32 %2351, %1342
  br i1 %2352, label %2353, label %1383

; <label>:2353                                    ; preds = %2346
  br label %2354

; <label>:2354                                    ; preds = %2353, %1344
  %2355 = phi i32 [ %1345, %1344 ], [ %2347, %2353 ]
  %2356 = phi float [ %1347, %1344 ], [ %2348, %2353 ]
  %2357 = phi float [ %1348, %1344 ], [ %2349, %2353 ]
  %2358 = phi float [ %1349, %1344 ], [ %2350, %2353 ]
  %2359 = add nuw nsw i32 %1346, 1
  %2360 = icmp eq i32 %2359, 2
  br i1 %2360, label %2361, label %1344

; <label>:2361                                    ; preds = %2354
  %2362 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2363 = extractvalue %dx.types.CBufRet.f32 %2362, 0
  %2364 = extractvalue %dx.types.CBufRet.f32 %2362, 1
  %2365 = extractvalue %dx.types.CBufRet.f32 %2362, 2
  %2366 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2367 = extractvalue %dx.types.CBufRet.f32 %2366, 0
  %2368 = extractvalue %dx.types.CBufRet.f32 %2366, 1
  %2369 = extractvalue %dx.types.CBufRet.f32 %2366, 2
  %2370 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2371 = extractvalue %dx.types.CBufRet.f32 %2370, 0
  %2372 = extractvalue %dx.types.CBufRet.f32 %2370, 1
  %2373 = extractvalue %dx.types.CBufRet.f32 %2370, 2
  %2374 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2375 = extractvalue %dx.types.CBufRet.f32 %2374, 0
  %2376 = extractvalue %dx.types.CBufRet.f32 %2374, 1
  %2377 = extractvalue %dx.types.CBufRet.f32 %2374, 2
  %2378 = fmul fast float %2363, %139
  %2379 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2367, float %2378)  ; FMad(a,b,c)
  %2380 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2371, float %2379)  ; FMad(a,b,c)
  %2381 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2375, float %2380)  ; FMad(a,b,c)
  %2382 = fmul fast float %2364, %139
  %2383 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2368, float %2382)  ; FMad(a,b,c)
  %2384 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2372, float %2383)  ; FMad(a,b,c)
  %2385 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2376, float %2384)  ; FMad(a,b,c)
  %2386 = fmul fast float %2365, %139
  %2387 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2369, float %2386)  ; FMad(a,b,c)
  %2388 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2373, float %2387)  ; FMad(a,b,c)
  %2389 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2377, float %2388)  ; FMad(a,b,c)
  %2390 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2391 = extractvalue %dx.types.CBufRet.f32 %2390, 0
  %2392 = extractvalue %dx.types.CBufRet.f32 %2390, 1
  %2393 = extractvalue %dx.types.CBufRet.f32 %2390, 2
  %2394 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %2395 = extractvalue %dx.types.CBufRet.f32 %2394, 0
  %2396 = extractvalue %dx.types.CBufRet.f32 %2394, 1
  %2397 = extractvalue %dx.types.CBufRet.f32 %2394, 2
  %2398 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %2399 = extractvalue %dx.types.CBufRet.f32 %2398, 0
  %2400 = extractvalue %dx.types.CBufRet.f32 %2398, 1
  %2401 = extractvalue %dx.types.CBufRet.f32 %2398, 2
  %2402 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %2403 = extractvalue %dx.types.CBufRet.f32 %2402, 0
  %2404 = extractvalue %dx.types.CBufRet.f32 %2402, 1
  %2405 = extractvalue %dx.types.CBufRet.f32 %2402, 2
  %2406 = fmul fast float %2391, %139
  %2407 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2395, float %2406)  ; FMad(a,b,c)
  %2408 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2399, float %2407)  ; FMad(a,b,c)
  %2409 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2403, float %2408)  ; FMad(a,b,c)
  %2410 = fmul fast float %2392, %139
  %2411 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2396, float %2410)  ; FMad(a,b,c)
  %2412 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2400, float %2411)  ; FMad(a,b,c)
  %2413 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2404, float %2412)  ; FMad(a,b,c)
  %2414 = fmul fast float %2393, %139
  %2415 = call float @dx.op.tertiary.f32(i32 46, float %143, float %2397, float %2414)  ; FMad(a,b,c)
  %2416 = call float @dx.op.tertiary.f32(i32 46, float %147, float %2401, float %2415)  ; FMad(a,b,c)
  %2417 = call float @dx.op.tertiary.f32(i32 46, float %151, float %2405, float %2416)  ; FMad(a,b,c)
  %2418 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 39)  ; CBufferLoadLegacy(handle,regIndex)
  %2419 = extractvalue %dx.types.CBufRet.f32 %2418, 2
  %2420 = fmul fast float %259, 0xBFF05F8CA0000000
  %2421 = fmul fast float %2420, %2419
  %2422 = fmul fast float %260, 0xBFF05F8CA0000000
  %2423 = fmul fast float %2422, %2419
  %2424 = fmul fast float %261, 0x3FF05F8CA0000000
  %2425 = fmul fast float %2424, %2419
  %2426 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 40)  ; CBufferLoadLegacy(handle,regIndex)
  %2427 = extractvalue %dx.types.CBufRet.f32 %2426, 0
  %2428 = fcmp fast oeq float %2427, 0.000000e+00
  br i1 %2428, label %2429, label %2440

; <label>:2429                                    ; preds = %2361
  %2430 = fadd fast float %2409, -5.000000e-01
  %2431 = fadd fast float %2413, -5.000000e-01
  %2432 = fadd fast float %2417, -5.000000e-01
  %2433 = call float @dx.op.unary.f32(i32 6, float %2430)  ; FAbs(value)
  %2434 = call float @dx.op.unary.f32(i32 6, float %2431)  ; FAbs(value)
  %2435 = call float @dx.op.unary.f32(i32 6, float %2432)  ; FAbs(value)
  %2436 = call float @dx.op.binary.f32(i32 35, float %2434, float %2435)  ; FMax(a,b)
  %2437 = call float @dx.op.binary.f32(i32 35, float %2433, float %2436)  ; FMax(a,b)
  %2438 = fcmp fast ogt float %2437, 5.000000e-01
  %2439 = select i1 %2438, float 0.000000e+00, float 1.000000e+00
  br label %2457

; <label>:2440                                    ; preds = %2361
  %2441 = fsub fast float 1.000000e+00, %2409
  %2442 = call float @dx.op.binary.f32(i32 36, float %2427, float %2441)  ; FMin(a,b)
  %2443 = call float @dx.op.binary.f32(i32 36, float %2427, float %2409)  ; FMin(a,b)
  %2444 = call float @dx.op.binary.f32(i32 36, float %2443, float %2442)  ; FMin(a,b)
  %2445 = fsub fast float 1.000000e+00, %2413
  %2446 = call float @dx.op.binary.f32(i32 36, float %2427, float %2445)  ; FMin(a,b)
  %2447 = call float @dx.op.binary.f32(i32 36, float %2427, float %2413)  ; FMin(a,b)
  %2448 = call float @dx.op.binary.f32(i32 36, float %2447, float %2446)  ; FMin(a,b)
  %2449 = fsub fast float 1.000000e+00, %2417
  %2450 = call float @dx.op.binary.f32(i32 36, float %2427, float %2449)  ; FMin(a,b)
  %2451 = call float @dx.op.binary.f32(i32 36, float %2427, float %2417)  ; FMin(a,b)
  %2452 = call float @dx.op.binary.f32(i32 36, float %2451, float %2450)  ; FMin(a,b)
  %2453 = call float @dx.op.binary.f32(i32 36, float %2448, float %2452)  ; FMin(a,b)
  %2454 = call float @dx.op.binary.f32(i32 36, float %2444, float %2453)  ; FMin(a,b)
  %2455 = fdiv fast float %2454, %2427
  %2456 = call float @dx.op.unary.f32(i32 7, float %2455)  ; Saturate(value)
  br label %2457

; <label>:2457                                    ; preds = %2440, %2429
  %2458 = phi float [ %2439, %2429 ], [ %2456, %2440 ]
  %2459 = fcmp fast une float %2458, 0.000000e+00
  br i1 %2459, label %2460, label %2553

; <label>:2460                                    ; preds = %2457
  %2461 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 39)  ; CBufferLoadLegacy(handle,regIndex)
  %2462 = extractvalue %dx.types.CBufRet.i32 %2461, 0
  %2463 = sitofp i32 %2462 to float
  %2464 = fmul fast float %2463, %2389
  %2465 = call float @dx.op.unary.f32(i32 27, float %2464)  ; Round_ni(value)
  %2466 = fptosi float %2465 to i32
  %2467 = add nsw i32 %2462, -1
  %2468 = add nsw i32 %2466, 1
  %2469 = call i32 @dx.op.binary.i32(i32 38, i32 %2468, i32 %2467)  ; IMin(a,b)
  %2470 = sitofp i32 %2466 to float
  %2471 = fsub fast float %2464, %2470
  %2472 = fsub fast float 1.000000e+00, %2471
  %2473 = sitofp i32 %2469 to float
  %2474 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2470, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2475 = extractvalue %dx.types.ResRet.f32 %2474, 0
  %2476 = extractvalue %dx.types.ResRet.f32 %2474, 1
  %2477 = extractvalue %dx.types.ResRet.f32 %2474, 2
  %2478 = extractvalue %dx.types.ResRet.f32 %2474, 3
  %2479 = fmul fast float %2475, %2472
  %2480 = fmul fast float %2476, %2472
  %2481 = fmul fast float %2477, %2472
  %2482 = fmul fast float %2478, %2472
  %2483 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2473, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2484 = extractvalue %dx.types.ResRet.f32 %2483, 0
  %2485 = extractvalue %dx.types.ResRet.f32 %2483, 1
  %2486 = extractvalue %dx.types.ResRet.f32 %2483, 2
  %2487 = extractvalue %dx.types.ResRet.f32 %2483, 3
  %2488 = fmul fast float %2484, %2471
  %2489 = fmul fast float %2485, %2471
  %2490 = fmul fast float %2486, %2471
  %2491 = fmul fast float %2487, %2471
  %2492 = fadd fast float %2488, %2479
  %2493 = fadd fast float %2489, %2480
  %2494 = fadd fast float %2490, %2481
  %2495 = fadd fast float %2491, %2482
  %2496 = fadd fast float %2463, %2470
  %2497 = fadd fast float %2463, %2473
  %2498 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2496, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2499 = extractvalue %dx.types.ResRet.f32 %2498, 0
  %2500 = extractvalue %dx.types.ResRet.f32 %2498, 1
  %2501 = extractvalue %dx.types.ResRet.f32 %2498, 2
  %2502 = extractvalue %dx.types.ResRet.f32 %2498, 3
  %2503 = fmul fast float %2499, %2472
  %2504 = fmul fast float %2500, %2472
  %2505 = fmul fast float %2501, %2472
  %2506 = fmul fast float %2502, %2472
  %2507 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2497, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2508 = extractvalue %dx.types.ResRet.f32 %2507, 0
  %2509 = extractvalue %dx.types.ResRet.f32 %2507, 1
  %2510 = extractvalue %dx.types.ResRet.f32 %2507, 2
  %2511 = extractvalue %dx.types.ResRet.f32 %2507, 3
  %2512 = fmul fast float %2508, %2471
  %2513 = fmul fast float %2509, %2471
  %2514 = fmul fast float %2510, %2471
  %2515 = fmul fast float %2511, %2471
  %2516 = fadd fast float %2512, %2503
  %2517 = fadd fast float %2513, %2504
  %2518 = fadd fast float %2514, %2505
  %2519 = fadd fast float %2515, %2506
  %2520 = fadd fast float %2463, %2496
  %2521 = fadd fast float %2463, %2497
  %2522 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2520, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2523 = extractvalue %dx.types.ResRet.f32 %2522, 0
  %2524 = extractvalue %dx.types.ResRet.f32 %2522, 1
  %2525 = extractvalue %dx.types.ResRet.f32 %2522, 2
  %2526 = extractvalue %dx.types.ResRet.f32 %2522, 3
  %2527 = fmul fast float %2523, %2472
  %2528 = fmul fast float %2524, %2472
  %2529 = fmul fast float %2525, %2472
  %2530 = fmul fast float %2526, %2472
  %2531 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %28, float %2381, float %2385, float %2521, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2532 = extractvalue %dx.types.ResRet.f32 %2531, 0
  %2533 = extractvalue %dx.types.ResRet.f32 %2531, 1
  %2534 = extractvalue %dx.types.ResRet.f32 %2531, 2
  %2535 = extractvalue %dx.types.ResRet.f32 %2531, 3
  %2536 = fmul fast float %2532, %2471
  %2537 = fmul fast float %2533, %2471
  %2538 = fmul fast float %2534, %2471
  %2539 = fmul fast float %2535, %2471
  %2540 = fadd fast float %2536, %2527
  %2541 = fadd fast float %2537, %2528
  %2542 = fadd fast float %2538, %2529
  %2543 = fadd fast float %2539, %2530
  %2544 = call float @dx.op.dot4.f32(i32 56, float %2492, float %2493, float %2494, float %2495, float 0x3FEC5BF980000000, float %2423, float %2425, float %2421)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2545 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2544)  ; FMax(a,b)
  %2546 = fmul fast float %2545, %2458
  %2547 = call float @dx.op.dot4.f32(i32 56, float %2516, float %2517, float %2518, float %2519, float 0x3FEC5BF980000000, float %2423, float %2425, float %2421)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2548 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2547)  ; FMax(a,b)
  %2549 = fmul fast float %2548, %2458
  %2550 = call float @dx.op.dot4.f32(i32 56, float %2540, float %2541, float %2542, float %2543, float 0x3FEC5BF980000000, float %2423, float %2425, float %2421)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2551 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2550)  ; FMax(a,b)
  %2552 = fmul fast float %2551, %2458
  br label %2553

; <label>:2553                                    ; preds = %2460, %2457
  %2554 = phi float [ %2546, %2460 ], [ 0.000000e+00, %2457 ]
  %2555 = phi float [ %2549, %2460 ], [ 0.000000e+00, %2457 ]
  %2556 = phi float [ %2552, %2460 ], [ 0.000000e+00, %2457 ]
  %2557 = extractvalue %dx.types.CBufRet.f32 %2418, 1
  %2558 = fmul fast float %2557, %2554
  %2559 = fmul fast float %2557, %2555
  %2560 = fmul fast float %2557, %2556
  %2561 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %2562 = extractvalue %dx.types.CBufRet.f32 %2561, 0
  %2563 = fmul fast float %2558, %2562
  %2564 = fmul fast float %2559, %2562
  %2565 = fmul fast float %2560, %2562
  %2566 = fadd fast float %2563, %2356
  %2567 = fadd fast float %2564, %2357
  %2568 = fadd fast float %2565, %2358
  %2569 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %2570 = extractvalue %dx.types.CBufRet.f32 %2569, 3
  %2571 = fcmp fast une float %2570, 0.000000e+00
  br i1 %2571, label %2572, label %2758

; <label>:2572                                    ; preds = %2553
  %2573 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 124)  ; CBufferLoadLegacy(handle,regIndex)
  %2574 = extractvalue %dx.types.CBufRet.f32 %2573, 0
  %2575 = extractvalue %dx.types.CBufRet.f32 %2573, 1
  %2576 = extractvalue %dx.types.CBufRet.f32 %2573, 2
  %2577 = fadd fast float %2574, %139
  %2578 = fadd fast float %2575, %143
  %2579 = fadd fast float %2576, %147
  %2580 = fmul fast float %2577, %2570
  %2581 = fmul fast float %2578, %2570
  %2582 = fmul fast float %2579, %2570
  %2583 = extractvalue %dx.types.CBufRet.f32 %2569, 0
  %2584 = extractvalue %dx.types.CBufRet.f32 %2569, 1
  %2585 = extractvalue %dx.types.CBufRet.f32 %2569, 2
  %2586 = fadd fast float %2580, %2583
  %2587 = fadd fast float %2581, %2584
  %2588 = fadd fast float %2582, %2585
  %2589 = call float @dx.op.unary.f32(i32 27, float %2586)  ; Round_ni(value)
  %2590 = call float @dx.op.unary.f32(i32 27, float %2587)  ; Round_ni(value)
  %2591 = call float @dx.op.unary.f32(i32 27, float %2588)  ; Round_ni(value)
  %2592 = fptosi float %2589 to i32
  %2593 = fptosi float %2590 to i32
  %2594 = fptosi float %2591 to i32
  %2595 = and i32 %2592, 255
  %2596 = and i32 %2593, 255
  %2597 = and i32 %2594, 255
  %2598 = fsub fast float %2586, %2589
  %2599 = fsub fast float %2587, %2590
  %2600 = fsub fast float %2588, %2591
  %2601 = fmul fast float %2598, %2598
  %2602 = fmul fast float %2599, %2599
  %2603 = fmul fast float %2601, %2598
  %2604 = fmul fast float %2602, %2599
  %2605 = fmul fast float %2598, 6.000000e+00
  %2606 = fmul fast float %2599, 6.000000e+00
  %2607 = fadd fast float %2605, -1.500000e+01
  %2608 = fadd fast float %2606, -1.500000e+01
  %2609 = fmul fast float %2607, %2598
  %2610 = fmul fast float %2608, %2599
  %2611 = fadd fast float %2609, 1.000000e+01
  %2612 = fadd fast float %2610, 1.000000e+01
  %2613 = fmul fast float %2603, %2611
  %2614 = fmul fast float %2604, %2612
  %2615 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %11, i32 0, i32 %2595, i32 %2596, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2616 = extractvalue %dx.types.ResRet.f32 %2615, 0
  %2617 = extractvalue %dx.types.ResRet.f32 %2615, 1
  %2618 = extractvalue %dx.types.ResRet.f32 %2615, 2
  %2619 = extractvalue %dx.types.ResRet.f32 %2615, 3
  %2620 = fmul fast float %2616, 0x3FEFE00000000000
  %2621 = fmul fast float %2617, 0x3FEFE00000000000
  %2622 = fmul fast float %2618, 0x3FEFE00000000000
  %2623 = fmul fast float %2619, 0x3FEFE00000000000
  %2624 = sitofp i32 %2597 to float
  %2625 = fmul fast float %2624, 3.906250e-03
  %2626 = fadd fast float %2620, %2625
  %2627 = fadd fast float %2621, %2625
  %2628 = fadd fast float %2622, %2625
  %2629 = fadd fast float %2623, %2625
  %2630 = fadd fast float %2629, 0x3F77FF9B40000000
  %2631 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2630, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2632 = fadd fast float %2599, -1.000000e+00
  %2633 = fadd fast float %2600, -1.000000e+00
  %2634 = fadd fast float %2627, 0x3F77FF9B40000000
  %2635 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2634, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2636 = extractvalue %dx.types.ResRet.f32 %2635, 0
  %2637 = extractvalue %dx.types.ResRet.f32 %2635, 1
  %2638 = extractvalue %dx.types.ResRet.f32 %2635, 2
  %2639 = fmul fast float %2636, 2.000000e+00
  %2640 = fmul fast float %2637, 2.000000e+00
  %2641 = fmul fast float %2638, 2.000000e+00
  %2642 = fadd fast float %2639, -1.000000e+00
  %2643 = fadd fast float %2640, -1.000000e+00
  %2644 = fadd fast float %2641, -1.000000e+00
  %2645 = call float @dx.op.dot3.f32(i32 55, float %2642, float %2643, float %2644, float %2598, float %2632, float %2633)  ; Dot3(ax,ay,az,bx,by,bz)
  %2646 = fadd fast float %2598, -1.000000e+00
  %2647 = fadd fast float %2628, 0x3F77FF9B40000000
  %2648 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2647, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2649 = extractvalue %dx.types.ResRet.f32 %2648, 0
  %2650 = extractvalue %dx.types.ResRet.f32 %2648, 1
  %2651 = extractvalue %dx.types.ResRet.f32 %2648, 2
  %2652 = fmul fast float %2649, 2.000000e+00
  %2653 = fmul fast float %2650, 2.000000e+00
  %2654 = fmul fast float %2651, 2.000000e+00
  %2655 = fadd fast float %2652, -1.000000e+00
  %2656 = fadd fast float %2653, -1.000000e+00
  %2657 = fadd fast float %2654, -1.000000e+00
  %2658 = call float @dx.op.dot3.f32(i32 55, float %2655, float %2656, float %2657, float %2646, float %2599, float %2633)  ; Dot3(ax,ay,az,bx,by,bz)
  %2659 = fadd fast float %2626, 0x3F77FF9B40000000
  %2660 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2659, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2661 = extractvalue %dx.types.ResRet.f32 %2660, 0
  %2662 = extractvalue %dx.types.ResRet.f32 %2660, 1
  %2663 = extractvalue %dx.types.ResRet.f32 %2660, 2
  %2664 = fmul fast float %2661, 2.000000e+00
  %2665 = fmul fast float %2662, 2.000000e+00
  %2666 = fmul fast float %2663, 2.000000e+00
  %2667 = fadd fast float %2664, -1.000000e+00
  %2668 = fadd fast float %2665, -1.000000e+00
  %2669 = fadd fast float %2666, -1.000000e+00
  %2670 = call float @dx.op.dot3.f32(i32 55, float %2667, float %2668, float %2669, float %2598, float %2599, float %2633)  ; Dot3(ax,ay,az,bx,by,bz)
  %2671 = fsub fast float %2658, %2670
  %2672 = fmul fast float %2671, %2613
  %2673 = fadd fast float %2672, %2670
  %2674 = fadd fast float %2629, 0x3F5FFF79C0000000
  %2675 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2674, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2676 = extractvalue %dx.types.ResRet.f32 %2675, 0
  %2677 = extractvalue %dx.types.ResRet.f32 %2675, 1
  %2678 = extractvalue %dx.types.ResRet.f32 %2675, 2
  %2679 = fmul fast float %2676, 2.000000e+00
  %2680 = fmul fast float %2677, 2.000000e+00
  %2681 = fmul fast float %2678, 2.000000e+00
  %2682 = fadd fast float %2679, -1.000000e+00
  %2683 = fadd fast float %2680, -1.000000e+00
  %2684 = fadd fast float %2681, -1.000000e+00
  %2685 = call float @dx.op.dot3.f32(i32 55, float %2682, float %2683, float %2684, float %2646, float %2632, float %2600)  ; Dot3(ax,ay,az,bx,by,bz)
  %2686 = fadd fast float %2627, 0x3F5FFF79C0000000
  %2687 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2686, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2688 = extractvalue %dx.types.ResRet.f32 %2687, 0
  %2689 = extractvalue %dx.types.ResRet.f32 %2687, 1
  %2690 = extractvalue %dx.types.ResRet.f32 %2687, 2
  %2691 = fmul fast float %2688, 2.000000e+00
  %2692 = fmul fast float %2689, 2.000000e+00
  %2693 = fmul fast float %2690, 2.000000e+00
  %2694 = fadd fast float %2691, -1.000000e+00
  %2695 = fadd fast float %2692, -1.000000e+00
  %2696 = fadd fast float %2693, -1.000000e+00
  %2697 = call float @dx.op.dot3.f32(i32 55, float %2694, float %2695, float %2696, float %2598, float %2632, float %2600)  ; Dot3(ax,ay,az,bx,by,bz)
  %2698 = fsub fast float %2685, %2697
  %2699 = fmul fast float %2698, %2613
  %2700 = fadd fast float %2699, %2697
  %2701 = fadd fast float %2628, 0x3F5FFF79C0000000
  %2702 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2701, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2703 = extractvalue %dx.types.ResRet.f32 %2702, 0
  %2704 = extractvalue %dx.types.ResRet.f32 %2702, 1
  %2705 = extractvalue %dx.types.ResRet.f32 %2702, 2
  %2706 = fmul fast float %2703, 2.000000e+00
  %2707 = fmul fast float %2704, 2.000000e+00
  %2708 = fmul fast float %2705, 2.000000e+00
  %2709 = fadd fast float %2706, -1.000000e+00
  %2710 = fadd fast float %2707, -1.000000e+00
  %2711 = fadd fast float %2708, -1.000000e+00
  %2712 = call float @dx.op.dot3.f32(i32 55, float %2709, float %2710, float %2711, float %2646, float %2599, float %2600)  ; Dot3(ax,ay,az,bx,by,bz)
  %2713 = fadd fast float %2626, 0x3F5FFF79C0000000
  %2714 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %26, float %2713, float 2.565000e+02, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2715 = extractvalue %dx.types.ResRet.f32 %2714, 0
  %2716 = extractvalue %dx.types.ResRet.f32 %2714, 1
  %2717 = extractvalue %dx.types.ResRet.f32 %2714, 2
  %2718 = fmul fast float %2715, 2.000000e+00
  %2719 = fmul fast float %2716, 2.000000e+00
  %2720 = fmul fast float %2717, 2.000000e+00
  %2721 = fadd fast float %2718, -1.000000e+00
  %2722 = fadd fast float %2719, -1.000000e+00
  %2723 = fadd fast float %2720, -1.000000e+00
  %2724 = call float @dx.op.dot3.f32(i32 55, float %2721, float %2722, float %2723, float %2598, float %2599, float %2600)  ; Dot3(ax,ay,az,bx,by,bz)
  %2725 = fsub fast float %2712, %2724
  %2726 = fmul fast float %2725, %2613
  %2727 = fadd fast float %2726, %2724
  %2728 = fsub fast float %2700, %2727
  %2729 = fmul fast float %2728, %2614
  %2730 = fadd fast float %2729, %2727
  %2731 = fmul fast float %2600, %2600
  %2732 = fmul fast float %2731, %2600
  %2733 = fmul fast float %2600, 6.000000e+00
  %2734 = fadd fast float %2733, -1.500000e+01
  %2735 = fmul fast float %2734, %2600
  %2736 = fadd fast float %2735, 1.000000e+01
  %2737 = fmul fast float %2732, %2736
  %2738 = extractvalue %dx.types.ResRet.f32 %2631, 0
  %2739 = fmul fast float %2738, 2.000000e+00
  %2740 = fadd fast float %2739, -1.000000e+00
  %2741 = extractvalue %dx.types.ResRet.f32 %2631, 1
  %2742 = fmul fast float %2741, 2.000000e+00
  %2743 = fadd fast float %2742, -1.000000e+00
  %2744 = extractvalue %dx.types.ResRet.f32 %2631, 2
  %2745 = fmul fast float %2744, 2.000000e+00
  %2746 = fadd fast float %2745, -1.000000e+00
  %2747 = call float @dx.op.dot3.f32(i32 55, float %2740, float %2743, float %2746, float %2646, float %2632, float %2633)  ; Dot3(ax,ay,az,bx,by,bz)
  %2748 = fsub fast float %2747, %2645
  %2749 = fmul fast float %2748, %2613
  %2750 = fsub fast float %2645, %2673
  %2751 = fadd fast float %2750, %2749
  %2752 = fmul fast float %2751, %2614
  %2753 = fadd fast float %2752, %2673
  %2754 = fsub fast float %2753, %2730
  %2755 = fmul fast float %2737, %2754
  %2756 = fadd fast float %2730, 1.000000e+00
  %2757 = fadd fast float %2756, %2755
  br label %2758

; <label>:2758                                    ; preds = %2572, %2553
  %2759 = phi float [ %2757, %2572 ], [ 1.000000e+00, %2553 ]
  %2760 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2761 = extractvalue %dx.types.CBufRet.f32 %2760, 0
  %2762 = extractvalue %dx.types.CBufRet.f32 %2760, 1
  %2763 = fcmp fast ogt float %143, %2762
  br i1 %2763, label %2764, label %2771

; <label>:2764                                    ; preds = %2758
  %2765 = fsub fast float %143, %2762
  %2766 = fsub fast float -0.000000e+00, %2765
  %2767 = fdiv fast float %2766, %2762
  %2768 = fmul fast float %2767, 0x3FF7154760000000
  %2769 = call float @dx.op.unary.f32(i32 21, float %2768)  ; Exp(value)
  %2770 = fmul fast float %2769, %2761
  br label %2771

; <label>:2771                                    ; preds = %2764, %2758
  %2772 = phi float [ %2770, %2764 ], [ %2761, %2758 ]
  %2773 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2774 = extractvalue %dx.types.CBufRet.f32 %2773, 1
  %2775 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %2776 = extractvalue %dx.types.CBufRet.f32 %2775, 2
  %2777 = fsub fast float %143, %2774
  %2778 = fsub fast float %2777, %2776
  %2779 = extractvalue %dx.types.CBufRet.f32 %2775, 3
  %2780 = fmul fast float %2778, %2779
  %2781 = call float @dx.op.unary.f32(i32 7, float %2780)  ; Saturate(value)
  %2782 = fmul fast float %2781, %2781
  %2783 = fmul fast float %2782, 0xC042089FC0000000
  %2784 = call float @dx.op.unary.f32(i32 21, float %2783)  ; Exp(value)
  %2785 = fsub fast float %138, %183
  %2786 = fsub fast float %142, %187
  %2787 = fsub fast float %146, %191
  %2788 = fmul fast float %2785, %2785
  %2789 = fmul fast float %2786, %2786
  %2790 = fadd fast float %2789, %2788
  %2791 = fmul fast float %2787, %2787
  %2792 = fadd fast float %2790, %2791
  %2793 = call float @dx.op.unary.f32(i32 24, float %2792)  ; Sqrt(value)
  %2794 = fmul fast float %2772, %2759
  %2795 = fmul fast float %2794, %2784
  %2796 = fmul fast float %2795, %2793
  %2797 = fmul fast float %2796, %2566
  %2798 = fmul fast float %2796, %2567
  %2799 = fmul fast float %2796, %2568
  %2800 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2801 = extractvalue %dx.types.CBufRet.f32 %2800, 1
  %2802 = fcmp fast ogt float %2801, 0.000000e+00
  br i1 %2802, label %2803, label %2908

; <label>:2803                                    ; preds = %2771
  %2804 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %2805 = extractvalue %dx.types.CBufRet.f32 %2804, 0
  %2806 = extractvalue %dx.types.CBufRet.f32 %2804, 1
  %2807 = extractvalue %dx.types.CBufRet.f32 %2804, 2
  %2808 = extractvalue %dx.types.CBufRet.f32 %2804, 3
  %2809 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %2810 = extractvalue %dx.types.CBufRet.f32 %2809, 0
  %2811 = extractvalue %dx.types.CBufRet.f32 %2809, 1
  %2812 = extractvalue %dx.types.CBufRet.f32 %2809, 2
  %2813 = extractvalue %dx.types.CBufRet.f32 %2809, 3
  %2814 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %2815 = extractvalue %dx.types.CBufRet.f32 %2814, 0
  %2816 = extractvalue %dx.types.CBufRet.f32 %2814, 1
  %2817 = extractvalue %dx.types.CBufRet.f32 %2814, 2
  %2818 = extractvalue %dx.types.CBufRet.f32 %2814, 3
  %2819 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2820 = extractvalue %dx.types.CBufRet.f32 %2819, 0
  %2821 = extractvalue %dx.types.CBufRet.f32 %2819, 1
  %2822 = extractvalue %dx.types.CBufRet.f32 %2819, 2
  %2823 = extractvalue %dx.types.CBufRet.f32 %2819, 3
  %2824 = fmul fast float %2805, %159
  %2825 = call float @dx.op.tertiary.f32(i32 46, float %163, float %2810, float %2824)  ; FMad(a,b,c)
  %2826 = call float @dx.op.tertiary.f32(i32 46, float %167, float %2815, float %2825)  ; FMad(a,b,c)
  %2827 = call float @dx.op.tertiary.f32(i32 46, float %171, float %2820, float %2826)  ; FMad(a,b,c)
  %2828 = fmul fast float %2806, %159
  %2829 = call float @dx.op.tertiary.f32(i32 46, float %163, float %2811, float %2828)  ; FMad(a,b,c)
  %2830 = call float @dx.op.tertiary.f32(i32 46, float %167, float %2816, float %2829)  ; FMad(a,b,c)
  %2831 = call float @dx.op.tertiary.f32(i32 46, float %171, float %2821, float %2830)  ; FMad(a,b,c)
  %2832 = fmul fast float %2807, %159
  %2833 = call float @dx.op.tertiary.f32(i32 46, float %163, float %2812, float %2832)  ; FMad(a,b,c)
  %2834 = call float @dx.op.tertiary.f32(i32 46, float %167, float %2817, float %2833)  ; FMad(a,b,c)
  %2835 = call float @dx.op.tertiary.f32(i32 46, float %171, float %2822, float %2834)  ; FMad(a,b,c)
  %2836 = fmul fast float %2808, %159
  %2837 = call float @dx.op.tertiary.f32(i32 46, float %163, float %2813, float %2836)  ; FMad(a,b,c)
  %2838 = call float @dx.op.tertiary.f32(i32 46, float %167, float %2818, float %2837)  ; FMad(a,b,c)
  %2839 = call float @dx.op.tertiary.f32(i32 46, float %171, float %2823, float %2838)  ; FMad(a,b,c)
  %2840 = fdiv fast float %2827, %2839
  %2841 = fdiv fast float %2831, %2839
  %2842 = fdiv fast float %2835, %2839
  %2843 = call float @dx.op.unary.f32(i32 6, float %2840)  ; FAbs(value)
  %2844 = fcmp fast ogt float %2843, 1.000000e+00
  %2845 = call float @dx.op.unary.f32(i32 6, float %2841)  ; FAbs(value)
  %2846 = fcmp fast ogt float %2845, 1.000000e+00
  %2847 = or i1 %2844, %2846
  %2848 = fcmp fast olt float %2842, 0.000000e+00
  %2849 = or i1 %2848, %2847
  %2850 = fcmp fast ogt float %2842, 1.000000e+00
  %2851 = or i1 %2850, %2849
  br i1 %2851, label %2889, label %2852

; <label>:2852                                    ; preds = %2803
  %2853 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2854 = extractvalue %dx.types.CBufRet.f32 %2853, 0
  %2855 = fsub fast float -0.000000e+00, %2854
  %2856 = extractvalue %dx.types.CBufRet.f32 %2853, 1
  %2857 = fadd fast float %2856, %2842
  %2858 = fdiv fast float %2855, %2857
  %2859 = call float @dx.op.unary.f32(i32 23, float %2858)  ; Log(value)
  %2860 = fmul fast float %2859, 0x3FE62E4300000000
  %2861 = extractvalue %dx.types.CBufRet.f32 %2853, 2
  %2862 = call float @dx.op.unary.f32(i32 23, float %2861)  ; Log(value)
  %2863 = fmul fast float %2862, 0x3FE62E4300000000
  %2864 = fdiv fast float %2860, %2863
  %2865 = fmul fast float %2840, 5.000000e-01
  %2866 = fmul fast float %2841, 5.000000e-01
  %2867 = fadd fast float %2865, 5.000000e-01
  %2868 = fsub fast float 5.000000e-01, %2866
  %2869 = add i32 %39, -1
  %2870 = add i32 %40, -1
  %2871 = add i32 %41, -1
  %2872 = uitofp i32 %2869 to float
  %2873 = uitofp i32 %2870 to float
  %2874 = uitofp i32 %2871 to float
  %2875 = fmul fast float %2867, %2872
  %2876 = fmul fast float %2868, %2873
  %2877 = fmul fast float %2864, %2874
  %2878 = fadd fast float %2875, 5.000000e-01
  %2879 = fdiv fast float %2878, %48
  %2880 = fadd fast float %2876, 5.000000e-01
  %2881 = fdiv fast float %2880, %49
  %2882 = fadd fast float %2877, 5.000000e-01
  %2883 = fdiv fast float %2882, %50
  %2884 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %25, float %2879, float %2881, float %2883, float undef, i32 0, i32 0, i32 0, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2885 = extractvalue %dx.types.ResRet.f32 %2884, 0
  %2886 = extractvalue %dx.types.ResRet.f32 %2884, 1
  %2887 = extractvalue %dx.types.ResRet.f32 %2884, 2
  %2888 = extractvalue %dx.types.ResRet.f32 %2884, 3
  br label %2889

; <label>:2889                                    ; preds = %2852, %2803
  %2890 = phi float [ %2885, %2852 ], [ 0.000000e+00, %2803 ]
  %2891 = phi float [ %2886, %2852 ], [ 0.000000e+00, %2803 ]
  %2892 = phi float [ %2887, %2852 ], [ 0.000000e+00, %2803 ]
  %2893 = phi float [ %2888, %2852 ], [ -1.000000e+00, %2803 ]
  %2894 = fcmp fast ogt float %2893, 0.000000e+00
  br i1 %2894, label %2895, label %2908

; <label>:2895                                    ; preds = %2889
  %2896 = fsub fast float %2890, %2797
  %2897 = fsub fast float %2891, %2798
  %2898 = fsub fast float %2892, %2799
  %2899 = fsub fast float %2893, %2796
  %2900 = fmul fast float %2801, %2896
  %2901 = fmul fast float %2801, %2897
  %2902 = fmul fast float %2801, %2898
  %2903 = fmul fast float %2801, %2899
  %2904 = fadd fast float %2900, %2797
  %2905 = fadd fast float %2901, %2798
  %2906 = fadd fast float %2902, %2799
  %2907 = fadd fast float %2903, %2796
  br label %2908

; <label>:2908                                    ; preds = %2895, %2889, %2771
  %2909 = phi float [ %2904, %2895 ], [ %2797, %2889 ], [ %2797, %2771 ]
  %2910 = phi float [ %2905, %2895 ], [ %2798, %2889 ], [ %2798, %2771 ]
  %2911 = phi float [ %2906, %2895 ], [ %2799, %2889 ], [ %2799, %2771 ]
  %2912 = phi float [ %2907, %2895 ], [ %2796, %2889 ], [ %2796, %2771 ]
  %2913 = call float @dx.op.binary.f32(i32 35, float %2909, float 0.000000e+00)  ; FMax(a,b)
  %2914 = call float @dx.op.binary.f32(i32 35, float %2910, float 0.000000e+00)  ; FMax(a,b)
  %2915 = call float @dx.op.binary.f32(i32 35, float %2911, float 0.000000e+00)  ; FMax(a,b)
  %2916 = call float @dx.op.binary.f32(i32 35, float %2912, float 0.000000e+00)  ; FMax(a,b)
  %2917 = call float @dx.op.binary.f32(i32 36, float %2913, float 0x47EFFFFFA0000000)  ; FMin(a,b)
  %2918 = call float @dx.op.binary.f32(i32 36, float %2914, float 0x47EFFFFFA0000000)  ; FMin(a,b)
  %2919 = call float @dx.op.binary.f32(i32 36, float %2915, float 0x47EFFFFFA0000000)  ; FMin(a,b)
  %2920 = call float @dx.op.binary.f32(i32 36, float %2916, float 0x47EFFFFFA0000000)  ; FMin(a,b)
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %32, i32 %33, i32 %34, float %2917, float %2918, float %2919, float %2920, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!45}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !31, !33, !36}
!5 = !{!6, !8, !10, !11, !12, !13, !14, !15, !16, !17, !18, !19, !20, !21, !22, !23, !25, !27, !29, !30}
!6 = !{i32 0, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 7, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 16, i32 1, i32 10, i32 0, !9}
!9 = !{i32 0, i32 5}
!10 = !{i32 2, %"class.Buffer<vector<float, 4> >"* undef, !"", i32 0, i32 17, i32 1, i32 10, i32 0, !7}
!11 = !{i32 3, %"class.Buffer<vector<float, 3> >"* undef, !"", i32 0, i32 18, i32 1, i32 10, i32 0, !7}
!12 = !{i32 4, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 19, i32 1, i32 10, i32 0, !9}
!13 = !{i32 5, %"class.Buffer<vector<float, 3> >"* undef, !"", i32 0, i32 20, i32 1, i32 10, i32 0, !7}
!14 = !{i32 6, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 21, i32 1, i32 10, i32 0, !9}
!15 = !{i32 7, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 7, i32 0, !7}
!16 = !{i32 8, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 7, i32 0, !7}
!17 = !{i32 9, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 32, i32 1, i32 10, i32 0, !9}
!18 = !{i32 10, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!19 = !{i32 11, %"class.Texture3D<vector<float, 4> >"* undef, !"", i32 0, i32 9, i32 1, i32 4, i32 0, !7}
!20 = !{i32 12, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 10, i32 1, i32 2, i32 0, !7}
!21 = !{i32 13, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 7, i32 0, !7}
!22 = !{i32 14, %"class.TextureCubeArray<vector<float, 4> >"* undef, !"", i32 0, i32 13, i32 1, i32 9, i32 0, !7}
!23 = !{i32 15, %"dx.alignment.legacy.class.StructuredBuffer<slVFParallelLightInfo>"* undef, !"", i32 0, i32 25, i32 1, i32 12, i32 0, !24}
!24 = !{i32 1, i32 432}
!25 = !{i32 16, %"class.StructuredBuffer<slVFPointBasicLightInfo>"* undef, !"", i32 0, i32 26, i32 1, i32 12, i32 0, !26}
!26 = !{i32 1, i32 64}
!27 = !{i32 17, %"dx.alignment.legacy.class.StructuredBuffer<slVFSpotBasicLightInfo>"* undef, !"", i32 0, i32 27, i32 1, i32 12, i32 0, !28}
!28 = !{i32 1, i32 112}
!29 = !{i32 18, %"class.Buffer<float>"* undef, !"", i32 0, i32 28, i32 1, i32 10, i32 0, !7}
!30 = !{i32 19, %"class.Buffer<float>"* undef, !"", i32 0, i32 29, i32 1, i32 10, i32 0, !7}
!31 = !{!32}
!32 = !{i32 0, %"class.RWTexture3D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 4, i1 false, i1 false, i1 false, !7}
!33 = !{!34, !35}
!34 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 1996, null}
!35 = !{i32 1, %dx.alignment.legacy.VolumetricFogConstBuf* undef, !"", i32 0, i32 1, i32 1, i32 336, null}
!36 = !{!37, !38, !39, !40, !41, !42, !43, !44}
!37 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 0, i32 1, i32 0, null}
!38 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 1, i32 1, i32 0, null}
!39 = !{i32 2, %struct.SamplerComparisonState* undef, !"", i32 0, i32 2, i32 1, i32 1, null}
!40 = !{i32 3, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!41 = !{i32 4, %struct.SamplerState* undef, !"", i32 0, i32 9, i32 1, i32 0, null}
!42 = !{i32 5, %struct.SamplerState* undef, !"", i32 0, i32 10, i32 1, i32 0, null}
!43 = !{i32 6, %struct.SamplerState* undef, !"", i32 0, i32 12, i32 1, i32 0, null}
!44 = !{i32 7, %struct.SamplerState* undef, !"", i32 0, i32 13, i32 1, i32 0, null}
!45 = !{void ()* @main, !"main", null, !4, !46}
!46 = !{i32 0, i64 16, i32 4, !47}
!47 = !{i32 8, i32 8, i32 4}
!48 = !{!49, !49, i64 0}
!49 = !{!"int", !50, i64 0}
!50 = !{!"omnipotent char", !51, i64 0}
!51 = !{!"Simple C/C++ TBAA"}
!52 = !{!53, !55, !56, !57}
!53 = distinct !{!53, !54, !"\01?ApplyPointAndSpotLightWithLightCullingPrecise_unfolded0000@@YAXAIAH0MV?$vector@M$03@@111AIAMV?$vector@M$02@@AIAV1@I@Z: %_pipe_nPntAttCurvType"}
!54 = distinct !{!54, !"\01?ApplyPointAndSpotLightWithLightCullingPrecise_unfolded0000@@YAXAIAH0MV?$vector@M$03@@111AIAMV?$vector@M$02@@AIAV1@I@Z"}
!55 = distinct !{!55, !54, !"\01?ApplyPointAndSpotLightWithLightCullingPrecise_unfolded0000@@YAXAIAH0MV?$vector@M$03@@111AIAMV?$vector@M$02@@AIAV1@I@Z: %_pipe_nSptAttCurvType"}
!56 = distinct !{!56, !54, !"\01?ApplyPointAndSpotLightWithLightCullingPrecise_unfolded0000@@YAXAIAH0MV?$vector@M$03@@111AIAMV?$vector@M$02@@AIAV1@I@Z: %_pipe_fPixelDepth"}
!57 = distinct !{!57, !54, !"\01?ApplyPointAndSpotLightWithLightCullingPrecise_unfolded0000@@YAXAIAH0MV?$vector@M$03@@111AIAMV?$vector@M$02@@AIAV1@I@Z: %_pipe_fogLighting"}

