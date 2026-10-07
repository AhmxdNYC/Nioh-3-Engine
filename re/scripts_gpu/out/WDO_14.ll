;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xy  
; TEXCOORD                 0   xyzw        1     NONE   float   xy  
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Target                0   xyzw        0   TARGET   float   xyzw
;
; shader debug name: b8f0be946b7a911905af2c470222b1b0.pdb
; shader hash: b8f0be946b7a911905af2c470222b1b0
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Pixel Shader
; DepthOutput=0
; SampleFrequency=0
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 2
; SigOutputElements: 1
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 2
; SigOutputVectors[0]: 1
; SigOutputVectors[1]: 0
; SigOutputVectors[2]: 0
; SigOutputVectors[3]: 0
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; TEXCOORD                 0                 linear       
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Target                0                              
;
; Buffer Definitions:
;
; cbuffer $Globals
; {
;
;   struct dx.alignment.legacy.$Globals
;   {
;
;       float4 litDir[1];                             ; Offset:    0
;       float4 litCol[1];                             ; Offset:   16
;       float4 ambLit;                                ; Offset:   32
;       float4 vEye;                                  ; Offset:   48
;       row_major float4x4 mW2Pt;                     ; Offset:   64
;       float fPtn;                                   ; Offset:  128
;       row_major float4x4 mW2S[4];                   ; Offset:  144
;       float4 vSMOf[4];                              ; Offset:  400
;       float4 vS2VZ;                                 ; Offset:  464
;       float4 vSMDF;                                 ; Offset:  480
;       bool bSM[3];                                  ; Offset:  496
;       float4 scSM;                                  ; Offset:  544
;       float4 scSM2;                                 ; Offset:  560
;       float4 sdplane;                               ; Offset:  576
;       int nClipLevelMin;                            ; Offset:  592
;       int nArraySize_VirtualSM;                     ; Offset:  596
;       int nSubSdw;                                  ; Offset:  600
;       row_major float4x4 mSubW2S[4];                ; Offset:  608
;       float4 scSubSM;                               ; Offset:  864
;       int nClipmapLevelBias;                        ; Offset:  880
;       row_major float4x4 mW2S_VirtualSM[16];        ; Offset:  896
;       float2 vClipOriginOffset[16];                 ; Offset: 1920
;       int nValidClipmapFlag;                        ; Offset: 2168
;       float4 scMinMipMapVirtualSM;                  ; Offset: 2176
;       float fVirtualShadowDefaultValue;             ; Offset: 2192
;       int nCrossFadeParam_VirtualSM;                ; Offset: 2196
;       float4 scVirtualSM;                           ; Offset: 2208
;       row_major float4x4 mV2W;                      ; Offset: 2224
;       row_major float4x4 mP2W;                      ; Offset: 2288
;       float fDOFade;                                ; Offset: 2352
;       float fMicroShadowsFade;                      ; Offset: 2356
;   
;   } $Globals;                                       ; Offset:    0 Size:  2360
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpsLit                         sampler      NA          NA      S0             s1     1
; __smpsShadow                      sampler      NA          NA      S1             s0     1
; __smpsSubShadow                   sampler      NA          NA      S2             s2     1
; __smpsVirtualShadow               sampler      NA          NA      S3             s3     1
; sLit                              texture     f32          2d      T0             t1     1
; sShadow                           texture     f32          2d      T1             t0     1
; sSubShadow                        texture     f32     2darray      T2             t2     1
; tVirtualShadowMapFlags            texture     u32         buf      T3            t16     1
; sVirtualShadow                    texture     f32     2darray      T4             t3     1
; sGBuf1                            texture     f32          2d      T5             t6     1
; sGBuf0                            texture     f32          2d      T6             t5     1
; sDepth                            texture     f32          2d      T7             t4     1
;
;
; ViewId state:
;
; Number of inputs: 8, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 4, 5 }
;   output 1 depends on inputs: { 0, 1, 4, 5 }
;   output 2 depends on inputs: { 0, 1, 4, 5 }
;   output 3 depends on inputs: { 0, 1, 4, 5 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"class.Buffer<unsigned int>" = type { i32 }
%"dx.alignment.legacy.$Globals" = type { [1 x <4 x float>], [1 x <4 x float>], <4 x float>, <4 x float>, [4 x <4 x float>], float, [4 x [4 x <4 x float>]], [4 x <4 x float>], <4 x float>, <4 x float>, [3 x i32], <4 x float>, <4 x float>, <4 x float>, i32, i32, i32, [4 x [4 x <4 x float>]], <4 x float>, i32, [16 x [4 x <4 x float>]], [16 x <2 x float>], i32, <4 x float>, float, i32, <4 x float>, [4 x <4 x float>], [4 x <4 x float>], float, float }
%struct.SamplerState = type { i32 }
%struct.SamplerComparisonState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 4, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 5, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 6, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 16, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = fadd fast float %16, -5.000000e-01
  %19 = fadd fast float %17, -5.000000e-01
  %20 = fptosi float %16 to i32
  %21 = fptosi float %17 to i32
  %22 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %23 = extractvalue %dx.types.ResRet.f32 %22, 0
  %24 = extractvalue %dx.types.ResRet.f32 %22, 1
  %25 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %3, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %26 = extractvalue %dx.types.ResRet.f32 %25, 3
  %27 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %1, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %28 = extractvalue %dx.types.ResRet.f32 %27, 0
  %29 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 143)  ; CBufferLoadLegacy(handle,regIndex)
  %30 = extractvalue %dx.types.CBufRet.f32 %29, 0
  %31 = extractvalue %dx.types.CBufRet.f32 %29, 1
  %32 = extractvalue %dx.types.CBufRet.f32 %29, 2
  %33 = extractvalue %dx.types.CBufRet.f32 %29, 3
  %34 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 144)  ; CBufferLoadLegacy(handle,regIndex)
  %35 = extractvalue %dx.types.CBufRet.f32 %34, 0
  %36 = extractvalue %dx.types.CBufRet.f32 %34, 1
  %37 = extractvalue %dx.types.CBufRet.f32 %34, 2
  %38 = extractvalue %dx.types.CBufRet.f32 %34, 3
  %39 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 145)  ; CBufferLoadLegacy(handle,regIndex)
  %40 = extractvalue %dx.types.CBufRet.f32 %39, 0
  %41 = extractvalue %dx.types.CBufRet.f32 %39, 1
  %42 = extractvalue %dx.types.CBufRet.f32 %39, 2
  %43 = extractvalue %dx.types.CBufRet.f32 %39, 3
  %44 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 146)  ; CBufferLoadLegacy(handle,regIndex)
  %45 = extractvalue %dx.types.CBufRet.f32 %44, 0
  %46 = extractvalue %dx.types.CBufRet.f32 %44, 1
  %47 = extractvalue %dx.types.CBufRet.f32 %44, 2
  %48 = extractvalue %dx.types.CBufRet.f32 %44, 3
  %49 = fmul fast float %30, %14
  %50 = call float @dx.op.tertiary.f32(i32 46, float %15, float %35, float %49)  ; FMad(a,b,c)
  %51 = call float @dx.op.tertiary.f32(i32 46, float %28, float %40, float %50)  ; FMad(a,b,c)
  %52 = fadd fast float %51, %45
  %53 = fmul fast float %31, %14
  %54 = call float @dx.op.tertiary.f32(i32 46, float %15, float %36, float %53)  ; FMad(a,b,c)
  %55 = call float @dx.op.tertiary.f32(i32 46, float %28, float %41, float %54)  ; FMad(a,b,c)
  %56 = fadd fast float %55, %46
  %57 = fmul fast float %32, %14
  %58 = call float @dx.op.tertiary.f32(i32 46, float %15, float %37, float %57)  ; FMad(a,b,c)
  %59 = call float @dx.op.tertiary.f32(i32 46, float %28, float %42, float %58)  ; FMad(a,b,c)
  %60 = fadd fast float %59, %47
  %61 = fmul fast float %33, %14
  %62 = call float @dx.op.tertiary.f32(i32 46, float %15, float %38, float %61)  ; FMad(a,b,c)
  %63 = call float @dx.op.tertiary.f32(i32 46, float %28, float %43, float %62)  ; FMad(a,b,c)
  %64 = fadd fast float %63, %48
  %65 = fdiv fast float %52, %64
  %66 = fdiv fast float %56, %64
  %67 = fdiv fast float %60, %64
  %68 = fmul fast float %23, 2.000000e+00
  %69 = fmul fast float %24, 2.000000e+00
  %70 = fadd fast float %68, -1.000000e+00
  %71 = fadd fast float %69, -1.000000e+00
  %72 = call float @dx.op.unary.f32(i32 6, float %70)  ; FAbs(value)
  %73 = fsub fast float 1.000000e+00, %72
  %74 = call float @dx.op.unary.f32(i32 6, float %71)  ; FAbs(value)
  %75 = fsub fast float %73, %74
  %76 = fcmp fast oge float %75, 0.000000e+00
  %77 = fsub fast float 1.000000e+00, %74
  %78 = fcmp fast oge float %70, 0.000000e+00
  %79 = fcmp fast oge float %71, 0.000000e+00
  %80 = select i1 %78, float 1.000000e+00, float -1.000000e+00
  %81 = select i1 %79, float 1.000000e+00, float -1.000000e+00
  %82 = fmul fast float %77, %80
  %83 = fmul fast float %73, %81
  %84 = select i1 %76, float %70, float %82
  %85 = select i1 %76, float %71, float %83
  %86 = call float @dx.op.dot3.f32(i32 55, float %84, float %85, float %75, float %84, float %85, float %75)  ; Dot3(ax,ay,az,bx,by,bz)
  %87 = call float @dx.op.unary.f32(i32 25, float %86)  ; Rsqrt(value)
  %88 = fmul fast float %84, %87
  %89 = fmul fast float %85, %87
  %90 = fmul fast float %87, %75
  %91 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 139)  ; CBufferLoadLegacy(handle,regIndex)
  %92 = extractvalue %dx.types.CBufRet.f32 %91, 0
  %93 = extractvalue %dx.types.CBufRet.f32 %91, 1
  %94 = extractvalue %dx.types.CBufRet.f32 %91, 2
  %95 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 140)  ; CBufferLoadLegacy(handle,regIndex)
  %96 = extractvalue %dx.types.CBufRet.f32 %95, 0
  %97 = extractvalue %dx.types.CBufRet.f32 %95, 1
  %98 = extractvalue %dx.types.CBufRet.f32 %95, 2
  %99 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 141)  ; CBufferLoadLegacy(handle,regIndex)
  %100 = extractvalue %dx.types.CBufRet.f32 %99, 0
  %101 = extractvalue %dx.types.CBufRet.f32 %99, 1
  %102 = extractvalue %dx.types.CBufRet.f32 %99, 2
  %103 = fmul fast float %92, %88
  %104 = call float @dx.op.tertiary.f32(i32 46, float %89, float %96, float %103)  ; FMad(a,b,c)
  %105 = call float @dx.op.tertiary.f32(i32 46, float %90, float %100, float %104)  ; FMad(a,b,c)
  %106 = fmul fast float %93, %88
  %107 = call float @dx.op.tertiary.f32(i32 46, float %89, float %97, float %106)  ; FMad(a,b,c)
  %108 = call float @dx.op.tertiary.f32(i32 46, float %90, float %101, float %107)  ; FMad(a,b,c)
  %109 = fmul fast float %94, %88
  %110 = call float @dx.op.tertiary.f32(i32 46, float %89, float %98, float %109)  ; FMad(a,b,c)
  %111 = call float @dx.op.tertiary.f32(i32 46, float %90, float %102, float %110)  ; FMad(a,b,c)
  %112 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %113 = extractvalue %dx.types.CBufRet.f32 %112, 0
  %114 = extractvalue %dx.types.CBufRet.f32 %112, 1
  %115 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %116 = extractvalue %dx.types.CBufRet.f32 %115, 0
  %117 = extractvalue %dx.types.CBufRet.f32 %115, 1
  %118 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %119 = extractvalue %dx.types.CBufRet.f32 %118, 0
  %120 = extractvalue %dx.types.CBufRet.f32 %118, 1
  %121 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %122 = extractvalue %dx.types.CBufRet.f32 %121, 0
  %123 = extractvalue %dx.types.CBufRet.f32 %121, 1
  %124 = fmul fast float %113, %65
  %125 = call float @dx.op.tertiary.f32(i32 46, float %66, float %116, float %124)  ; FMad(a,b,c)
  %126 = call float @dx.op.tertiary.f32(i32 46, float %67, float %119, float %125)  ; FMad(a,b,c)
  %127 = fadd fast float %126, %122
  %128 = fmul fast float %114, %65
  %129 = call float @dx.op.tertiary.f32(i32 46, float %66, float %117, float %128)  ; FMad(a,b,c)
  %130 = call float @dx.op.tertiary.f32(i32 46, float %67, float %120, float %129)  ; FMad(a,b,c)
  %131 = fadd fast float %130, %123
  %132 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %133 = extractvalue %dx.types.CBufRet.f32 %132, 0
  %134 = extractvalue %dx.types.CBufRet.f32 %132, 1
  %135 = extractvalue %dx.types.CBufRet.f32 %132, 2
  %136 = extractvalue %dx.types.CBufRet.f32 %132, 3
  %137 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %138 = extractvalue %dx.types.CBufRet.f32 %137, 0
  %139 = extractvalue %dx.types.CBufRet.f32 %137, 1
  %140 = extractvalue %dx.types.CBufRet.f32 %137, 2
  %141 = extractvalue %dx.types.CBufRet.f32 %137, 3
  %142 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %143 = extractvalue %dx.types.CBufRet.f32 %142, 0
  %144 = extractvalue %dx.types.CBufRet.f32 %142, 1
  %145 = extractvalue %dx.types.CBufRet.f32 %142, 2
  %146 = extractvalue %dx.types.CBufRet.f32 %142, 3
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 0
  %149 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %150 = extractvalue %dx.types.CBufRet.f32 %147, 2
  %151 = extractvalue %dx.types.CBufRet.f32 %147, 3
  %152 = fmul fast float %133, %65
  %153 = call float @dx.op.tertiary.f32(i32 46, float %66, float %138, float %152)  ; FMad(a,b,c)
  %154 = call float @dx.op.tertiary.f32(i32 46, float %67, float %143, float %153)  ; FMad(a,b,c)
  %155 = fadd fast float %154, %148
  %156 = fmul fast float %134, %65
  %157 = call float @dx.op.tertiary.f32(i32 46, float %66, float %139, float %156)  ; FMad(a,b,c)
  %158 = call float @dx.op.tertiary.f32(i32 46, float %67, float %144, float %157)  ; FMad(a,b,c)
  %159 = fadd fast float %158, %149
  %160 = fmul fast float %135, %65
  %161 = call float @dx.op.tertiary.f32(i32 46, float %66, float %140, float %160)  ; FMad(a,b,c)
  %162 = call float @dx.op.tertiary.f32(i32 46, float %67, float %145, float %161)  ; FMad(a,b,c)
  %163 = fadd fast float %162, %150
  %164 = fmul fast float %136, %65
  %165 = call float @dx.op.tertiary.f32(i32 46, float %66, float %141, float %164)  ; FMad(a,b,c)
  %166 = call float @dx.op.tertiary.f32(i32 46, float %67, float %146, float %165)  ; FMad(a,b,c)
  %167 = fadd fast float %166, %151
  %168 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %169 = extractvalue %dx.types.CBufRet.f32 %168, 0
  %170 = extractvalue %dx.types.CBufRet.f32 %168, 1
  %171 = extractvalue %dx.types.CBufRet.f32 %168, 2
  %172 = extractvalue %dx.types.CBufRet.f32 %168, 3
  %173 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %174 = extractvalue %dx.types.CBufRet.f32 %173, 0
  %175 = extractvalue %dx.types.CBufRet.f32 %173, 1
  %176 = extractvalue %dx.types.CBufRet.f32 %173, 2
  %177 = extractvalue %dx.types.CBufRet.f32 %173, 3
  %178 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %179 = extractvalue %dx.types.CBufRet.f32 %178, 0
  %180 = extractvalue %dx.types.CBufRet.f32 %178, 1
  %181 = extractvalue %dx.types.CBufRet.f32 %178, 2
  %182 = extractvalue %dx.types.CBufRet.f32 %178, 3
  %183 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %184 = extractvalue %dx.types.CBufRet.f32 %183, 0
  %185 = extractvalue %dx.types.CBufRet.f32 %183, 1
  %186 = extractvalue %dx.types.CBufRet.f32 %183, 2
  %187 = extractvalue %dx.types.CBufRet.f32 %183, 3
  %188 = fmul fast float %169, %65
  %189 = call float @dx.op.tertiary.f32(i32 46, float %66, float %174, float %188)  ; FMad(a,b,c)
  %190 = call float @dx.op.tertiary.f32(i32 46, float %67, float %179, float %189)  ; FMad(a,b,c)
  %191 = fadd fast float %190, %184
  %192 = fmul fast float %170, %65
  %193 = call float @dx.op.tertiary.f32(i32 46, float %66, float %175, float %192)  ; FMad(a,b,c)
  %194 = call float @dx.op.tertiary.f32(i32 46, float %67, float %180, float %193)  ; FMad(a,b,c)
  %195 = fadd fast float %194, %185
  %196 = fmul fast float %171, %65
  %197 = call float @dx.op.tertiary.f32(i32 46, float %66, float %176, float %196)  ; FMad(a,b,c)
  %198 = call float @dx.op.tertiary.f32(i32 46, float %67, float %181, float %197)  ; FMad(a,b,c)
  %199 = fadd fast float %198, %186
  %200 = fmul fast float %172, %65
  %201 = call float @dx.op.tertiary.f32(i32 46, float %66, float %177, float %200)  ; FMad(a,b,c)
  %202 = call float @dx.op.tertiary.f32(i32 46, float %67, float %182, float %201)  ; FMad(a,b,c)
  %203 = fadd fast float %202, %187
  %204 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %205 = extractvalue %dx.types.CBufRet.f32 %204, 0
  %206 = extractvalue %dx.types.CBufRet.f32 %204, 1
  %207 = extractvalue %dx.types.CBufRet.f32 %204, 2
  %208 = extractvalue %dx.types.CBufRet.f32 %204, 3
  %209 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %210 = extractvalue %dx.types.CBufRet.f32 %209, 0
  %211 = extractvalue %dx.types.CBufRet.f32 %209, 1
  %212 = extractvalue %dx.types.CBufRet.f32 %209, 2
  %213 = extractvalue %dx.types.CBufRet.f32 %209, 3
  %214 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %215 = extractvalue %dx.types.CBufRet.f32 %214, 0
  %216 = extractvalue %dx.types.CBufRet.f32 %214, 1
  %217 = extractvalue %dx.types.CBufRet.f32 %214, 2
  %218 = extractvalue %dx.types.CBufRet.f32 %214, 3
  %219 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %220 = extractvalue %dx.types.CBufRet.f32 %219, 0
  %221 = extractvalue %dx.types.CBufRet.f32 %219, 1
  %222 = extractvalue %dx.types.CBufRet.f32 %219, 2
  %223 = extractvalue %dx.types.CBufRet.f32 %219, 3
  %224 = fmul fast float %205, %65
  %225 = call float @dx.op.tertiary.f32(i32 46, float %66, float %210, float %224)  ; FMad(a,b,c)
  %226 = call float @dx.op.tertiary.f32(i32 46, float %67, float %215, float %225)  ; FMad(a,b,c)
  %227 = fadd fast float %226, %220
  %228 = fmul fast float %206, %65
  %229 = call float @dx.op.tertiary.f32(i32 46, float %66, float %211, float %228)  ; FMad(a,b,c)
  %230 = call float @dx.op.tertiary.f32(i32 46, float %67, float %216, float %229)  ; FMad(a,b,c)
  %231 = fadd fast float %230, %221
  %232 = fmul fast float %207, %65
  %233 = call float @dx.op.tertiary.f32(i32 46, float %66, float %212, float %232)  ; FMad(a,b,c)
  %234 = call float @dx.op.tertiary.f32(i32 46, float %67, float %217, float %233)  ; FMad(a,b,c)
  %235 = fadd fast float %234, %222
  %236 = fmul fast float %208, %65
  %237 = call float @dx.op.tertiary.f32(i32 46, float %66, float %213, float %236)  ; FMad(a,b,c)
  %238 = call float @dx.op.tertiary.f32(i32 46, float %67, float %218, float %237)  ; FMad(a,b,c)
  %239 = fadd fast float %238, %223
  %240 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %241 = extractvalue %dx.types.CBufRet.f32 %240, 0
  %242 = extractvalue %dx.types.CBufRet.f32 %240, 1
  %243 = extractvalue %dx.types.CBufRet.f32 %240, 2
  %244 = extractvalue %dx.types.CBufRet.f32 %240, 3
  %245 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %246 = extractvalue %dx.types.CBufRet.f32 %245, 0
  %247 = extractvalue %dx.types.CBufRet.f32 %245, 1
  %248 = extractvalue %dx.types.CBufRet.f32 %245, 2
  %249 = extractvalue %dx.types.CBufRet.f32 %245, 3
  %250 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %251 = extractvalue %dx.types.CBufRet.f32 %250, 0
  %252 = extractvalue %dx.types.CBufRet.f32 %250, 1
  %253 = extractvalue %dx.types.CBufRet.f32 %250, 2
  %254 = extractvalue %dx.types.CBufRet.f32 %250, 3
  %255 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %256 = extractvalue %dx.types.CBufRet.f32 %255, 0
  %257 = extractvalue %dx.types.CBufRet.f32 %255, 1
  %258 = extractvalue %dx.types.CBufRet.f32 %255, 2
  %259 = extractvalue %dx.types.CBufRet.f32 %255, 3
  %260 = fmul fast float %241, %65
  %261 = call float @dx.op.tertiary.f32(i32 46, float %66, float %246, float %260)  ; FMad(a,b,c)
  %262 = call float @dx.op.tertiary.f32(i32 46, float %67, float %251, float %261)  ; FMad(a,b,c)
  %263 = fadd fast float %262, %256
  %264 = fmul fast float %242, %65
  %265 = call float @dx.op.tertiary.f32(i32 46, float %66, float %247, float %264)  ; FMad(a,b,c)
  %266 = call float @dx.op.tertiary.f32(i32 46, float %67, float %252, float %265)  ; FMad(a,b,c)
  %267 = fadd fast float %266, %257
  %268 = fmul fast float %243, %65
  %269 = call float @dx.op.tertiary.f32(i32 46, float %66, float %248, float %268)  ; FMad(a,b,c)
  %270 = call float @dx.op.tertiary.f32(i32 46, float %67, float %253, float %269)  ; FMad(a,b,c)
  %271 = fadd fast float %270, %258
  %272 = fmul fast float %244, %65
  %273 = call float @dx.op.tertiary.f32(i32 46, float %66, float %249, float %272)  ; FMad(a,b,c)
  %274 = call float @dx.op.tertiary.f32(i32 46, float %67, float %254, float %273)  ; FMad(a,b,c)
  %275 = fadd fast float %274, %259
  %276 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %277 = extractvalue %dx.types.CBufRet.f32 %276, 0
  %278 = extractvalue %dx.types.CBufRet.f32 %276, 1
  %279 = extractvalue %dx.types.CBufRet.f32 %276, 2
  %280 = extractvalue %dx.types.CBufRet.f32 %276, 3
  %281 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %282 = extractvalue %dx.types.CBufRet.f32 %281, 0
  %283 = extractvalue %dx.types.CBufRet.f32 %281, 1
  %284 = extractvalue %dx.types.CBufRet.f32 %281, 2
  %285 = extractvalue %dx.types.CBufRet.f32 %281, 3
  %286 = call float @dx.op.dot4.f32(i32 56, float %155, float %159, float %163, float %167, float %282, float %283, float %284, float %285)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %287 = fcmp fast ogt float %286, 0.000000e+00
  %288 = select i1 %287, float %191, float %155
  %289 = select i1 %287, float %195, float %159
  %290 = select i1 %287, float %199, float %163
  %291 = select i1 %287, float %203, float %167
  %292 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %293 = extractvalue %dx.types.CBufRet.f32 %292, 0
  %294 = extractvalue %dx.types.CBufRet.f32 %292, 1
  %295 = extractvalue %dx.types.CBufRet.f32 %292, 2
  %296 = extractvalue %dx.types.CBufRet.f32 %292, 3
  %297 = select i1 %287, float %293, float %277
  %298 = select i1 %287, float %294, float %278
  %299 = select i1 %287, float %295, float %279
  %300 = select i1 %287, float %296, float %280
  %301 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %302 = extractvalue %dx.types.CBufRet.f32 %301, 0
  %303 = extractvalue %dx.types.CBufRet.f32 %301, 1
  %304 = fsub fast float %286, %302
  %305 = fsub fast float %286, %303
  %306 = fcmp fast ogt float %304, 0.000000e+00
  %307 = fcmp fast ogt float %305, 0.000000e+00
  %308 = select i1 %306, float %227, float %288
  %309 = select i1 %306, float %231, float %289
  %310 = select i1 %306, float %235, float %290
  %311 = select i1 %306, float %239, float %291
  %312 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 27)  ; CBufferLoadLegacy(handle,regIndex)
  %313 = extractvalue %dx.types.CBufRet.f32 %312, 0
  %314 = extractvalue %dx.types.CBufRet.f32 %312, 1
  %315 = extractvalue %dx.types.CBufRet.f32 %312, 2
  %316 = extractvalue %dx.types.CBufRet.f32 %312, 3
  %317 = select i1 %306, float %313, float %297
  %318 = select i1 %306, float %314, float %298
  %319 = select i1 %306, float %315, float %299
  %320 = select i1 %306, float %316, float %300
  %321 = select i1 %307, float %263, float %308
  %322 = select i1 %307, float %267, float %309
  %323 = select i1 %307, float %271, float %310
  %324 = select i1 %307, float %275, float %311
  %325 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %326 = extractvalue %dx.types.CBufRet.f32 %325, 0
  %327 = extractvalue %dx.types.CBufRet.f32 %325, 1
  %328 = extractvalue %dx.types.CBufRet.f32 %325, 2
  %329 = extractvalue %dx.types.CBufRet.f32 %325, 3
  %330 = select i1 %307, float %326, float %317
  %331 = select i1 %307, float %327, float %318
  %332 = select i1 %307, float %328, float %319
  %333 = select i1 %307, float %329, float %320
  %334 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)  ; CBufferLoadLegacy(handle,regIndex)
  %335 = extractvalue %dx.types.CBufRet.i32 %334, 1
  %336 = fmul fast float %18, 5.000000e-01
  %337 = fadd fast float %19, %18
  %338 = fmul fast float %337, 5.000000e-01
  %339 = call float @dx.op.unary.f32(i32 22, float %336)  ; Frc(value)
  %340 = call float @dx.op.unary.f32(i32 22, float %338)  ; Frc(value)
  %341 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 31)  ; CBufferLoadLegacy(handle,regIndex)
  %342 = extractvalue %dx.types.CBufRet.i32 %341, 0
  %343 = icmp eq i32 %342, 0
  br i1 %343, label %3085, label %344

; <label>:344                                     ; preds = %0
  %345 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)  ; CBufferLoadLegacy(handle,regIndex)
  %346 = extractvalue %dx.types.CBufRet.i32 %345, 0
  %347 = icmp eq i32 %346, 0
  br i1 %347, label %396, label %348

; <label>:348                                     ; preds = %344
  %349 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 35)  ; CBufferLoadLegacy(handle,regIndex)
  %350 = extractvalue %dx.types.CBufRet.f32 %349, 0
  %351 = extractvalue %dx.types.CBufRet.f32 %349, 1
  %352 = extractvalue %dx.types.CBufRet.f32 %349, 2
  %353 = extractvalue %dx.types.CBufRet.f32 %349, 3
  %354 = fmul fast float %352, %339
  %355 = fmul fast float %353, %340
  %356 = fadd fast float %350, %321
  %357 = fadd fast float %356, %354
  %358 = fadd fast float %351, %322
  %359 = fadd fast float %358, %355
  %360 = fcmp fast oge float %357, 1.000000e+00
  %361 = fadd fast float %357, %352
  %362 = fcmp fast ole float %361, 0.000000e+00
  %363 = or i1 %360, %362
  %364 = fcmp fast oge float %359, 1.000000e+00
  %365 = or i1 %364, %363
  %366 = fadd fast float %359, %353
  %367 = fcmp fast ole float %366, 0.000000e+00
  %368 = or i1 %367, %365
  br i1 %368, label %515, label %369

; <label>:369                                     ; preds = %348
  %370 = call float @dx.op.unary.f32(i32 7, float %357)  ; Saturate(value)
  %371 = call float @dx.op.unary.f32(i32 7, float %359)  ; Saturate(value)
  %372 = fmul fast float %370, %330
  %373 = fmul fast float %371, %331
  %374 = fadd fast float %372, %332
  %375 = fadd fast float %373, %333
  %376 = call float @dx.op.unary.f32(i32 7, float %323)  ; Saturate(value)
  %377 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %374, float %375, float undef, float undef, i32 0, i32 0, i32 undef, float %376)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %378 = extractvalue %dx.types.ResRet.f32 %377, 0
  %379 = call float @dx.op.unary.f32(i32 7, float %361)  ; Saturate(value)
  %380 = fmul fast float %379, %330
  %381 = fadd fast float %380, %332
  %382 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %381, float %375, float undef, float undef, i32 0, i32 0, i32 undef, float %376)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %383 = extractvalue %dx.types.ResRet.f32 %382, 0
  %384 = fadd fast float %383, %378
  %385 = call float @dx.op.unary.f32(i32 7, float %366)  ; Saturate(value)
  %386 = fmul fast float %385, %331
  %387 = fadd fast float %386, %333
  %388 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %374, float %387, float undef, float undef, i32 0, i32 0, i32 undef, float %376)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %389 = extractvalue %dx.types.ResRet.f32 %388, 0
  %390 = fadd fast float %384, %389
  %391 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %381, float %387, float undef, float undef, i32 0, i32 0, i32 undef, float %376)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %392 = extractvalue %dx.types.ResRet.f32 %391, 0
  %393 = fadd fast float %390, %392
  %394 = fmul fast float %393, 2.500000e-01
  %395 = fsub fast float 1.000000e+00, %394
  br label %515

; <label>:396                                     ; preds = %344
  %397 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)  ; CBufferLoadLegacy(handle,regIndex)
  %398 = extractvalue %dx.types.CBufRet.i32 %397, 0
  %399 = icmp eq i32 %398, 0
  br i1 %399, label %501, label %400

; <label>:400                                     ; preds = %396
  %401 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 35)  ; CBufferLoadLegacy(handle,regIndex)
  %402 = extractvalue %dx.types.CBufRet.f32 %401, 0
  %403 = extractvalue %dx.types.CBufRet.f32 %401, 1
  %404 = fadd fast float %402, %321
  %405 = fadd fast float %403, %322
  %406 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 34)  ; CBufferLoadLegacy(handle,regIndex)
  %407 = extractvalue %dx.types.CBufRet.f32 %406, 0
  %408 = extractvalue %dx.types.CBufRet.f32 %406, 1
  %409 = fcmp fast oge float %404, 1.000000e+00
  %410 = fmul fast float %407, 3.000000e+00
  %411 = fadd fast float %410, %404
  %412 = fcmp fast ole float %411, 0.000000e+00
  %413 = or i1 %409, %412
  %414 = fcmp fast oge float %405, 1.000000e+00
  %415 = or i1 %414, %413
  %416 = fmul fast float %408, 3.000000e+00
  %417 = fadd fast float %416, %405
  %418 = fcmp fast ole float %417, 0.000000e+00
  %419 = or i1 %418, %415
  br i1 %419, label %515, label %420

; <label>:420                                     ; preds = %400
  %421 = call float @dx.op.unary.f32(i32 7, float %404)  ; Saturate(value)
  %422 = call float @dx.op.unary.f32(i32 7, float %405)  ; Saturate(value)
  %423 = fmul fast float %421, %330
  %424 = fmul fast float %422, %331
  %425 = fadd fast float %423, %332
  %426 = fadd fast float %424, %333
  %427 = call float @dx.op.unary.f32(i32 7, float %323)  ; Saturate(value)
  %428 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %429 = extractvalue %dx.types.ResRet.f32 %428, 0
  %430 = fadd fast float %404, %407
  %431 = call float @dx.op.unary.f32(i32 7, float %430)  ; Saturate(value)
  %432 = fmul fast float %431, %330
  %433 = fadd fast float %432, %332
  %434 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %435 = extractvalue %dx.types.ResRet.f32 %434, 0
  %436 = fadd fast float %435, %429
  %437 = fadd fast float %430, %407
  %438 = call float @dx.op.unary.f32(i32 7, float %437)  ; Saturate(value)
  %439 = fmul fast float %438, %330
  %440 = fadd fast float %439, %332
  %441 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %442 = extractvalue %dx.types.ResRet.f32 %441, 0
  %443 = fadd fast float %436, %442
  %444 = fadd fast float %437, %407
  %445 = call float @dx.op.unary.f32(i32 7, float %444)  ; Saturate(value)
  %446 = fmul fast float %445, %330
  %447 = fadd fast float %446, %332
  %448 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %449 = extractvalue %dx.types.ResRet.f32 %448, 0
  %450 = fadd fast float %443, %449
  %451 = fadd fast float %405, %408
  %452 = call float @dx.op.unary.f32(i32 7, float %451)  ; Saturate(value)
  %453 = fmul fast float %452, %331
  %454 = fadd fast float %453, %333
  %455 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %456 = extractvalue %dx.types.ResRet.f32 %455, 0
  %457 = fadd fast float %450, %456
  %458 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %459 = extractvalue %dx.types.ResRet.f32 %458, 0
  %460 = fadd fast float %457, %459
  %461 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %462 = extractvalue %dx.types.ResRet.f32 %461, 0
  %463 = fadd fast float %460, %462
  %464 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %465 = extractvalue %dx.types.ResRet.f32 %464, 0
  %466 = fadd fast float %463, %465
  %467 = fadd fast float %451, %408
  %468 = call float @dx.op.unary.f32(i32 7, float %467)  ; Saturate(value)
  %469 = fmul fast float %468, %331
  %470 = fadd fast float %469, %333
  %471 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %472 = extractvalue %dx.types.ResRet.f32 %471, 0
  %473 = fadd fast float %466, %472
  %474 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %475 = extractvalue %dx.types.ResRet.f32 %474, 0
  %476 = fadd fast float %473, %475
  %477 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %478 = extractvalue %dx.types.ResRet.f32 %477, 0
  %479 = fadd fast float %476, %478
  %480 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %481 = extractvalue %dx.types.ResRet.f32 %480, 0
  %482 = fadd fast float %479, %481
  %483 = fadd fast float %467, %408
  %484 = call float @dx.op.unary.f32(i32 7, float %483)  ; Saturate(value)
  %485 = fmul fast float %484, %331
  %486 = fadd fast float %485, %333
  %487 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %488 = extractvalue %dx.types.ResRet.f32 %487, 0
  %489 = fadd fast float %482, %488
  %490 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %491 = extractvalue %dx.types.ResRet.f32 %490, 0
  %492 = fadd fast float %489, %491
  %493 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %494 = extractvalue %dx.types.ResRet.f32 %493, 0
  %495 = fadd fast float %492, %494
  %496 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %497 = extractvalue %dx.types.ResRet.f32 %496, 0
  %498 = fadd fast float %495, %497
  %499 = fmul fast float %498, 6.250000e-02
  %500 = fsub fast float 1.000000e+00, %499
  br label %515

; <label>:501                                     ; preds = %396
  %502 = call float @dx.op.unary.f32(i32 7, float %321)  ; Saturate(value)
  %503 = call float @dx.op.unary.f32(i32 7, float %322)  ; Saturate(value)
  %504 = fmul fast float %502, %330
  %505 = fmul fast float %503, %331
  %506 = fadd fast float %504, %332
  %507 = fadd fast float %505, %333
  %508 = fdiv fast float %323, %324
  %509 = call float @dx.op.unary.f32(i32 7, float %508)  ; Saturate(value)
  %510 = fdiv fast float %506, %324
  %511 = fdiv fast float %507, %324
  %512 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %510, float %511, float undef, float undef, i32 0, i32 0, i32 undef, float %509)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %513 = extractvalue %dx.types.ResRet.f32 %512, 0
  %514 = fsub fast float 1.000000e+00, %513
  br label %515

; <label>:515                                     ; preds = %501, %420, %400, %369, %348
  %516 = phi float [ %514, %501 ], [ %395, %369 ], [ 0.000000e+00, %348 ], [ %500, %420 ], [ 0.000000e+00, %400 ]
  %517 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %518 = extractvalue %dx.types.CBufRet.f32 %517, 3
  %519 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 36)  ; CBufferLoadLegacy(handle,regIndex)
  %520 = extractvalue %dx.types.CBufRet.f32 %519, 0
  %521 = extractvalue %dx.types.CBufRet.f32 %519, 1
  %522 = extractvalue %dx.types.CBufRet.f32 %519, 2
  %523 = extractvalue %dx.types.CBufRet.f32 %519, 3
  %524 = call float @dx.op.dot4.f32(i32 56, float %520, float %521, float %522, float %523, float %155, float %159, float %163, float %167)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %525 = call float @dx.op.unary.f32(i32 7, float %524)  ; Saturate(value)
  %526 = fmul fast float %518, %516
  %527 = fmul fast float %526, %525
  %528 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)  ; CBufferLoadLegacy(handle,regIndex)
  %529 = extractvalue %dx.types.CBufRet.i32 %528, 2
  %530 = icmp sgt i32 %529, 0
  br i1 %530, label %531, label %713

; <label>:531                                     ; preds = %515
  br label %532

; <label>:532                                     ; preds = %697, %531
  %533 = phi i32 [ %708, %697 ], [ 0, %531 ]
  %534 = phi float [ %707, %697 ], [ %527, %531 ]
  %535 = shl i32 %533, 2
  %536 = add i32 %535, 38
  %537 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %536)  ; CBufferLoadLegacy(handle,regIndex)
  %538 = extractvalue %dx.types.CBufRet.f32 %537, 0
  %539 = extractvalue %dx.types.CBufRet.f32 %537, 1
  %540 = extractvalue %dx.types.CBufRet.f32 %537, 2
  %541 = add i32 %535, 39
  %542 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %541)  ; CBufferLoadLegacy(handle,regIndex)
  %543 = extractvalue %dx.types.CBufRet.f32 %542, 0
  %544 = extractvalue %dx.types.CBufRet.f32 %542, 1
  %545 = extractvalue %dx.types.CBufRet.f32 %542, 2
  %546 = add i32 %535, 40
  %547 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %546)  ; CBufferLoadLegacy(handle,regIndex)
  %548 = extractvalue %dx.types.CBufRet.f32 %547, 0
  %549 = extractvalue %dx.types.CBufRet.f32 %547, 1
  %550 = extractvalue %dx.types.CBufRet.f32 %547, 2
  %551 = add i32 %535, 41
  %552 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %551)  ; CBufferLoadLegacy(handle,regIndex)
  %553 = extractvalue %dx.types.CBufRet.f32 %552, 0
  %554 = extractvalue %dx.types.CBufRet.f32 %552, 1
  %555 = extractvalue %dx.types.CBufRet.f32 %552, 2
  %556 = fmul fast float %538, %65
  %557 = call float @dx.op.tertiary.f32(i32 46, float %66, float %543, float %556)  ; FMad(a,b,c)
  %558 = call float @dx.op.tertiary.f32(i32 46, float %67, float %548, float %557)  ; FMad(a,b,c)
  %559 = fadd fast float %558, %553
  %560 = fmul fast float %539, %65
  %561 = call float @dx.op.tertiary.f32(i32 46, float %66, float %544, float %560)  ; FMad(a,b,c)
  %562 = call float @dx.op.tertiary.f32(i32 46, float %67, float %549, float %561)  ; FMad(a,b,c)
  %563 = fadd fast float %562, %554
  %564 = fmul fast float %540, %65
  %565 = call float @dx.op.tertiary.f32(i32 46, float %66, float %545, float %564)  ; FMad(a,b,c)
  %566 = call float @dx.op.tertiary.f32(i32 46, float %67, float %550, float %565)  ; FMad(a,b,c)
  %567 = fadd fast float %566, %555
  %568 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)  ; CBufferLoadLegacy(handle,regIndex)
  %569 = extractvalue %dx.types.CBufRet.i32 %568, 0
  %570 = icmp eq i32 %569, 0
  br i1 %570, label %610, label %571

; <label>:571                                     ; preds = %532
  %572 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 54)  ; CBufferLoadLegacy(handle,regIndex)
  %573 = extractvalue %dx.types.CBufRet.f32 %572, 0
  %574 = extractvalue %dx.types.CBufRet.f32 %572, 1
  %575 = fmul fast float %573, 2.000000e+00
  %576 = fmul fast float %574, 2.000000e+00
  %577 = fmul fast float %575, %339
  %578 = fmul fast float %576, %340
  %579 = fmul fast float %573, 1.500000e+00
  %580 = fmul fast float %574, 1.500000e+00
  %581 = fsub fast float %577, %579
  %582 = fsub fast float %578, %580
  %583 = fadd fast float %581, %559
  %584 = fadd fast float %582, %563
  %585 = fcmp fast oge float %583, 1.000000e+00
  %586 = fadd fast float %583, %575
  %587 = fcmp fast ole float %586, 0.000000e+00
  %588 = or i1 %585, %587
  %589 = fcmp fast oge float %584, 1.000000e+00
  %590 = or i1 %589, %588
  %591 = fadd fast float %584, %576
  %592 = fcmp fast ole float %591, 0.000000e+00
  %593 = or i1 %592, %590
  br i1 %593, label %697, label %594

; <label>:594                                     ; preds = %571
  %595 = sitofp i32 %533 to float
  %596 = call float @dx.op.unary.f32(i32 7, float %567)  ; Saturate(value)
  %597 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %583, float %584, float %595, float undef, i32 0, i32 0, i32 undef, float %596)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %598 = extractvalue %dx.types.ResRet.f32 %597, 0
  %599 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %586, float %584, float %595, float undef, i32 0, i32 0, i32 undef, float %596)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %600 = extractvalue %dx.types.ResRet.f32 %599, 0
  %601 = fadd fast float %600, %598
  %602 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %583, float %591, float %595, float undef, i32 0, i32 0, i32 undef, float %596)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %603 = extractvalue %dx.types.ResRet.f32 %602, 0
  %604 = fadd fast float %601, %603
  %605 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %586, float %591, float %595, float undef, i32 0, i32 0, i32 undef, float %596)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %606 = extractvalue %dx.types.ResRet.f32 %605, 0
  %607 = fadd fast float %604, %606
  %608 = fmul fast float %607, 2.500000e-01
  %609 = fsub fast float 1.000000e+00, %608
  br label %697

; <label>:610                                     ; preds = %532
  %611 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)  ; CBufferLoadLegacy(handle,regIndex)
  %612 = extractvalue %dx.types.CBufRet.i32 %611, 0
  %613 = icmp eq i32 %612, 0
  br i1 %613, label %691, label %614

; <label>:614                                     ; preds = %610
  %615 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 54)  ; CBufferLoadLegacy(handle,regIndex)
  %616 = extractvalue %dx.types.CBufRet.f32 %615, 0
  %617 = extractvalue %dx.types.CBufRet.f32 %615, 1
  %618 = fmul fast float %616, 1.500000e+00
  %619 = fmul fast float %617, 1.500000e+00
  %620 = fsub fast float %559, %618
  %621 = fsub fast float %563, %619
  %622 = fcmp fast oge float %620, 1.000000e+00
  %623 = fmul fast float %616, 3.000000e+00
  %624 = fadd fast float %620, %623
  %625 = fcmp fast ole float %624, 0.000000e+00
  %626 = or i1 %622, %625
  %627 = fcmp fast oge float %621, 1.000000e+00
  %628 = or i1 %627, %626
  %629 = fmul fast float %617, 3.000000e+00
  %630 = fadd fast float %621, %629
  %631 = fcmp fast ole float %630, 0.000000e+00
  %632 = or i1 %631, %628
  br i1 %632, label %697, label %633

; <label>:633                                     ; preds = %614
  %634 = sitofp i32 %533 to float
  %635 = call float @dx.op.unary.f32(i32 7, float %567)  ; Saturate(value)
  %636 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %637 = extractvalue %dx.types.ResRet.f32 %636, 0
  %638 = fadd fast float %620, %616
  %639 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %640 = extractvalue %dx.types.ResRet.f32 %639, 0
  %641 = fadd fast float %640, %637
  %642 = fadd fast float %638, %616
  %643 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %644 = extractvalue %dx.types.ResRet.f32 %643, 0
  %645 = fadd fast float %641, %644
  %646 = fadd fast float %642, %616
  %647 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %648 = extractvalue %dx.types.ResRet.f32 %647, 0
  %649 = fadd fast float %645, %648
  %650 = fadd fast float %621, %617
  %651 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %652 = extractvalue %dx.types.ResRet.f32 %651, 0
  %653 = fadd fast float %649, %652
  %654 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %655 = extractvalue %dx.types.ResRet.f32 %654, 0
  %656 = fadd fast float %653, %655
  %657 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %658 = extractvalue %dx.types.ResRet.f32 %657, 0
  %659 = fadd fast float %656, %658
  %660 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %661 = extractvalue %dx.types.ResRet.f32 %660, 0
  %662 = fadd fast float %659, %661
  %663 = fadd fast float %650, %617
  %664 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %665 = extractvalue %dx.types.ResRet.f32 %664, 0
  %666 = fadd fast float %662, %665
  %667 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %668 = extractvalue %dx.types.ResRet.f32 %667, 0
  %669 = fadd fast float %666, %668
  %670 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %671 = extractvalue %dx.types.ResRet.f32 %670, 0
  %672 = fadd fast float %669, %671
  %673 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %674 = extractvalue %dx.types.ResRet.f32 %673, 0
  %675 = fadd fast float %672, %674
  %676 = fadd fast float %663, %617
  %677 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %678 = extractvalue %dx.types.ResRet.f32 %677, 0
  %679 = fadd fast float %675, %678
  %680 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %681 = extractvalue %dx.types.ResRet.f32 %680, 0
  %682 = fadd fast float %679, %681
  %683 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %684 = extractvalue %dx.types.ResRet.f32 %683, 0
  %685 = fadd fast float %682, %684
  %686 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %687 = extractvalue %dx.types.ResRet.f32 %686, 0
  %688 = fadd fast float %685, %687
  %689 = fmul fast float %688, 6.250000e-02
  %690 = fsub fast float 1.000000e+00, %689
  br label %697

; <label>:691                                     ; preds = %610
  %692 = sitofp i32 %533 to float
  %693 = call float @dx.op.unary.f32(i32 7, float %567)  ; Saturate(value)
  %694 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %559, float %563, float %692, float undef, i32 0, i32 0, i32 undef, float %693)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %695 = extractvalue %dx.types.ResRet.f32 %694, 0
  %696 = fsub fast float 1.000000e+00, %695
  br label %697

; <label>:697                                     ; preds = %691, %633, %614, %594, %571
  %698 = phi float [ %696, %691 ], [ %609, %594 ], [ 0.000000e+00, %571 ], [ %690, %633 ], [ 0.000000e+00, %614 ]
  %699 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 36)  ; CBufferLoadLegacy(handle,regIndex)
  %700 = extractvalue %dx.types.CBufRet.f32 %699, 0
  %701 = extractvalue %dx.types.CBufRet.f32 %699, 1
  %702 = extractvalue %dx.types.CBufRet.f32 %699, 2
  %703 = extractvalue %dx.types.CBufRet.f32 %699, 3
  %704 = call float @dx.op.dot4.f32(i32 56, float %700, float %701, float %702, float %703, float %155, float %159, float %163, float %167)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %705 = call float @dx.op.unary.f32(i32 7, float %704)  ; Saturate(value)
  %706 = fmul fast float %705, %698
  %707 = call float @dx.op.binary.f32(i32 35, float %534, float %706)  ; FMax(a,b)
  %708 = add nuw nsw i32 %533, 1
  %709 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)  ; CBufferLoadLegacy(handle,regIndex)
  %710 = extractvalue %dx.types.CBufRet.i32 %709, 2
  %711 = icmp slt i32 %708, %710
  br i1 %711, label %532, label %712

; <label>:712                                     ; preds = %697
  br label %713

; <label>:713                                     ; preds = %712, %515
  %714 = phi float [ %527, %515 ], [ %707, %712 ]
  %715 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)  ; CBufferLoadLegacy(handle,regIndex)
  %716 = extractvalue %dx.types.CBufRet.i32 %715, 1
  %717 = icmp sgt i32 %716, 0
  br i1 %717, label %718, label %3082

; <label>:718                                     ; preds = %713
  %719 = fcmp fast oeq float %28, 0.000000e+00
  %720 = fcmp fast oeq float %28, 1.000000e+00
  %721 = or i1 %719, %720
  br i1 %721, label %3082, label %722

; <label>:722                                     ; preds = %718
  %723 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %724 = extractvalue %dx.types.CBufRet.f32 %723, 0
  %725 = extractvalue %dx.types.CBufRet.f32 %723, 1
  %726 = extractvalue %dx.types.CBufRet.f32 %723, 2
  %727 = fsub fast float %65, %724
  %728 = fsub fast float %66, %725
  %729 = fsub fast float %67, %726
  %730 = fmul fast float %727, %727
  %731 = fmul fast float %728, %728
  %732 = fadd fast float %730, %731
  %733 = fmul fast float %729, %729
  %734 = fadd fast float %732, %733
  %735 = call float @dx.op.unary.f32(i32 24, float %734)  ; Sqrt(value)
  %736 = extractvalue %dx.types.CBufRet.i32 %715, 0
  %737 = call float @dx.op.unary.f32(i32 23, float %735)  ; Log(value)
  %738 = fadd fast float %737, 1.000000e+00
  %739 = uitofp i32 %736 to float
  %740 = fsub fast float %738, %739
  %741 = and i32 %736, 31
  %742 = shl i32 1, %741
  %743 = uitofp i32 %742 to float
  %744 = fmul fast float %743, 5.000000e-01
  %745 = fdiv fast float %735, %744
  %746 = fmul fast float %745, 2.000000e+00
  %747 = call float @dx.op.unary.f32(i32 6, float %746)  ; FAbs(value)
  %748 = call float @dx.op.binary.f32(i32 35, float %747, float 1.000000e+00)  ; FMax(a,b)
  %749 = call float @dx.op.unary.f32(i32 23, float %748)  ; Log(value)
  %750 = fptoui float %749 to i32
  %751 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 55)  ; CBufferLoadLegacy(handle,regIndex)
  %752 = extractvalue %dx.types.CBufRet.i32 %751, 0
  %753 = call i32 @dx.op.binary.i32(i32 37, i32 %750, i32 %752)  ; IMax(a,b)
  %754 = icmp ult i32 %753, %716
  br i1 %754, label %755, label %903

; <label>:755                                     ; preds = %722
  br label %756

; <label>:756                                     ; preds = %756, %755
  %757 = phi i32 [ %828, %756 ], [ %716, %755 ]
  %758 = phi i32 [ %829, %756 ], [ %753, %755 ]
  %759 = shl i32 %758, 2
  %760 = add i32 %759, 56
  %761 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %760)  ; CBufferLoadLegacy(handle,regIndex)
  %762 = extractvalue %dx.types.CBufRet.f32 %761, 0
  %763 = extractvalue %dx.types.CBufRet.f32 %761, 1
  %764 = add i32 %759, 57
  %765 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %764)  ; CBufferLoadLegacy(handle,regIndex)
  %766 = extractvalue %dx.types.CBufRet.f32 %765, 0
  %767 = extractvalue %dx.types.CBufRet.f32 %765, 1
  %768 = add i32 %759, 58
  %769 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %768)  ; CBufferLoadLegacy(handle,regIndex)
  %770 = extractvalue %dx.types.CBufRet.f32 %769, 0
  %771 = extractvalue %dx.types.CBufRet.f32 %769, 1
  %772 = add i32 %759, 59
  %773 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %772)  ; CBufferLoadLegacy(handle,regIndex)
  %774 = extractvalue %dx.types.CBufRet.f32 %773, 0
  %775 = extractvalue %dx.types.CBufRet.f32 %773, 1
  %776 = fmul fast float %762, %65
  %777 = call float @dx.op.tertiary.f32(i32 46, float %66, float %766, float %776)  ; FMad(a,b,c)
  %778 = call float @dx.op.tertiary.f32(i32 46, float %67, float %770, float %777)  ; FMad(a,b,c)
  %779 = fadd fast float %778, %774
  %780 = fmul fast float %763, %65
  %781 = call float @dx.op.tertiary.f32(i32 46, float %66, float %767, float %780)  ; FMad(a,b,c)
  %782 = call float @dx.op.tertiary.f32(i32 46, float %67, float %771, float %781)  ; FMad(a,b,c)
  %783 = fadd fast float %782, %775
  %784 = call float @dx.op.binary.f32(i32 35, float %779, float 0.000000e+00)  ; FMax(a,b)
  %785 = call float @dx.op.binary.f32(i32 35, float %783, float 0.000000e+00)  ; FMax(a,b)
  %786 = call float @dx.op.binary.f32(i32 36, float %784, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %787 = call float @dx.op.binary.f32(i32 36, float %785, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %788 = add i32 %758, 120
  %789 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %788)  ; CBufferLoadLegacy(handle,regIndex)
  %790 = extractvalue %dx.types.CBufRet.f32 %789, 0
  %791 = extractvalue %dx.types.CBufRet.f32 %789, 1
  %792 = fadd fast float %790, %786
  %793 = fadd fast float %791, %787
  %794 = call float @dx.op.unary.f32(i32 22, float %792)  ; Frc(value)
  %795 = call float @dx.op.unary.f32(i32 22, float %793)  ; Frc(value)
  %796 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 135)  ; CBufferLoadLegacy(handle,regIndex)
  %797 = extractvalue %dx.types.CBufRet.i32 %796, 2
  %798 = and i32 %758, 31
  %799 = shl i32 1, %798
  %800 = and i32 %797, %799
  %801 = icmp ne i32 %800, 0
  %802 = fcmp fast ogt float %779, 0.000000e+00
  %803 = fcmp fast ogt float %783, 0.000000e+00
  %804 = and i1 %802, %803
  %805 = fcmp fast olt float %779, 1.000000e+00
  %806 = fcmp fast olt float %783, 1.000000e+00
  %807 = and i1 %805, %806
  %808 = and i1 %804, %807
  %809 = and i1 %808, %801
  %810 = uitofp i32 %758 to float
  %811 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)  ; CBufferLoadLegacy(handle,regIndex)
  %812 = extractvalue %dx.types.CBufRet.f32 %811, 0
  %813 = extractvalue %dx.types.CBufRet.f32 %811, 1
  %814 = fmul fast float %813, %795
  %815 = extractvalue %dx.types.CBufRet.f32 %811, 2
  %816 = fmul fast float %815, %810
  %817 = call float @dx.op.unary.f32(i32 27, float %814)  ; Round_ni(value)
  %818 = fadd fast float %817, %794
  %819 = fmul fast float %818, %812
  %820 = fadd fast float %819, %816
  %821 = fptoui float %820 to i32
  %822 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %821, i32 undef)  ; BufferLoad(srv,index,wot)
  %823 = extractvalue %dx.types.ResRet.i32 %822, 0
  %824 = and i32 %823, 1
  %825 = icmp ne i32 %824, 0
  %826 = and i1 %809, %825
  %827 = select i1 %826, i32 %758, i32 %716
  %828 = call i32 @dx.op.binary.i32(i32 40, i32 %757, i32 %827)  ; UMin(a,b)
  %829 = add nuw i32 %758, 1
  %830 = icmp eq i32 %829, %716
  br i1 %830, label %831, label %756

; <label>:831                                     ; preds = %756
  %832 = icmp ult i32 %828, %716
  br i1 %832, label %837, label %833

; <label>:833                                     ; preds = %831
  %834 = add i32 %753, 1
  %835 = icmp eq i32 %834, %716
  %836 = select i1 %835, i32 -3, i32 -4
  br label %903

; <label>:837                                     ; preds = %831
  %838 = shl i32 %828, 2
  %839 = add i32 %838, 56
  %840 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %839)  ; CBufferLoadLegacy(handle,regIndex)
  %841 = extractvalue %dx.types.CBufRet.f32 %840, 0
  %842 = extractvalue %dx.types.CBufRet.f32 %840, 1
  %843 = extractvalue %dx.types.CBufRet.f32 %840, 2
  %844 = add i32 %838, 57
  %845 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %844)  ; CBufferLoadLegacy(handle,regIndex)
  %846 = extractvalue %dx.types.CBufRet.f32 %845, 0
  %847 = extractvalue %dx.types.CBufRet.f32 %845, 1
  %848 = extractvalue %dx.types.CBufRet.f32 %845, 2
  %849 = add i32 %838, 58
  %850 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %849)  ; CBufferLoadLegacy(handle,regIndex)
  %851 = extractvalue %dx.types.CBufRet.f32 %850, 0
  %852 = extractvalue %dx.types.CBufRet.f32 %850, 1
  %853 = extractvalue %dx.types.CBufRet.f32 %850, 2
  %854 = add i32 %838, 59
  %855 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %854)  ; CBufferLoadLegacy(handle,regIndex)
  %856 = extractvalue %dx.types.CBufRet.f32 %855, 0
  %857 = extractvalue %dx.types.CBufRet.f32 %855, 1
  %858 = extractvalue %dx.types.CBufRet.f32 %855, 2
  %859 = fmul fast float %841, %65
  %860 = call float @dx.op.tertiary.f32(i32 46, float %66, float %846, float %859)  ; FMad(a,b,c)
  %861 = call float @dx.op.tertiary.f32(i32 46, float %67, float %851, float %860)  ; FMad(a,b,c)
  %862 = fadd fast float %861, %856
  %863 = fmul fast float %842, %65
  %864 = call float @dx.op.tertiary.f32(i32 46, float %66, float %847, float %863)  ; FMad(a,b,c)
  %865 = call float @dx.op.tertiary.f32(i32 46, float %67, float %852, float %864)  ; FMad(a,b,c)
  %866 = fadd fast float %865, %857
  %867 = fmul fast float %843, %65
  %868 = call float @dx.op.tertiary.f32(i32 46, float %66, float %848, float %867)  ; FMad(a,b,c)
  %869 = call float @dx.op.tertiary.f32(i32 46, float %67, float %853, float %868)  ; FMad(a,b,c)
  %870 = fadd fast float %869, %858
  %871 = add i32 %838, 60
  %872 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %871)  ; CBufferLoadLegacy(handle,regIndex)
  %873 = extractvalue %dx.types.CBufRet.f32 %872, 0
  %874 = extractvalue %dx.types.CBufRet.f32 %872, 1
  %875 = extractvalue %dx.types.CBufRet.f32 %872, 2
  %876 = add i32 %838, 61
  %877 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %876)  ; CBufferLoadLegacy(handle,regIndex)
  %878 = extractvalue %dx.types.CBufRet.f32 %877, 0
  %879 = extractvalue %dx.types.CBufRet.f32 %877, 1
  %880 = extractvalue %dx.types.CBufRet.f32 %877, 2
  %881 = add i32 %838, 62
  %882 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %881)  ; CBufferLoadLegacy(handle,regIndex)
  %883 = extractvalue %dx.types.CBufRet.f32 %882, 0
  %884 = extractvalue %dx.types.CBufRet.f32 %882, 1
  %885 = extractvalue %dx.types.CBufRet.f32 %882, 2
  %886 = add i32 %838, 63
  %887 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %886)  ; CBufferLoadLegacy(handle,regIndex)
  %888 = extractvalue %dx.types.CBufRet.f32 %887, 0
  %889 = extractvalue %dx.types.CBufRet.f32 %887, 1
  %890 = extractvalue %dx.types.CBufRet.f32 %887, 2
  %891 = fmul fast float %873, %65
  %892 = call float @dx.op.tertiary.f32(i32 46, float %66, float %878, float %891)  ; FMad(a,b,c)
  %893 = call float @dx.op.tertiary.f32(i32 46, float %67, float %883, float %892)  ; FMad(a,b,c)
  %894 = fadd fast float %893, %888
  %895 = fmul fast float %874, %65
  %896 = call float @dx.op.tertiary.f32(i32 46, float %66, float %879, float %895)  ; FMad(a,b,c)
  %897 = call float @dx.op.tertiary.f32(i32 46, float %67, float %884, float %896)  ; FMad(a,b,c)
  %898 = fadd fast float %897, %889
  %899 = fmul fast float %875, %65
  %900 = call float @dx.op.tertiary.f32(i32 46, float %66, float %880, float %899)  ; FMad(a,b,c)
  %901 = call float @dx.op.tertiary.f32(i32 46, float %67, float %885, float %900)  ; FMad(a,b,c)
  %902 = fadd fast float %901, %890
  br label %903

; <label>:903                                     ; preds = %837, %833, %722
  %904 = phi i32 [ 0, %837 ], [ -2, %722 ], [ %836, %833 ]
  %905 = phi float [ %894, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %906 = phi float [ %898, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %907 = phi float [ %902, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %908 = phi float [ %862, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %909 = phi float [ %866, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %910 = phi float [ %870, %837 ], [ 0.000000e+00, %722 ], [ 0.000000e+00, %833 ]
  %911 = phi i32 [ %828, %837 ], [ %335, %722 ], [ %828, %833 ]
  %912 = icmp eq i32 %904, -2
  br i1 %912, label %913, label %919

; <label>:913                                     ; preds = %903
  %914 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %915 = extractvalue %dx.types.CBufRet.f32 %914, 0
  %916 = fsub fast float 1.000000e+00, %915
  %917 = fmul fast float %916, %518
  %918 = call float @dx.op.binary.f32(i32 35, float %714, float %917)  ; FMax(a,b)
  br label %3082

; <label>:919                                     ; preds = %903
  %920 = icmp eq i32 %904, -3
  br i1 %920, label %921, label %953

; <label>:921                                     ; preds = %919
  %922 = add i32 %716, -1
  %923 = uitofp i32 %922 to float
  %924 = fsub fast float %923, %740
  %925 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %926 = extractvalue %dx.types.CBufRet.i32 %925, 1
  %927 = and i32 %926, 255
  %928 = uitofp i32 %927 to float
  %929 = fmul fast float %928, 0x3F847AE140000000
  %930 = lshr i32 %926, 8
  %931 = and i32 %930, 255
  %932 = uitofp i32 %931 to float
  %933 = fmul fast float %932, 0x3F847AE140000000
  %934 = fcmp fast ogt float %933, %929
  br i1 %934, label %935, label %941

; <label>:935                                     ; preds = %921
  %936 = fsub fast float %924, %929
  %937 = fsub fast float %932, %928
  %938 = fmul fast float %937, 0x3F847AE140000000
  %939 = fdiv fast float %936, %938
  %940 = call float @dx.op.unary.f32(i32 7, float %939)  ; Saturate(value)
  br label %944

; <label>:941                                     ; preds = %921
  %942 = fcmp fast oge float %924, %933
  %943 = select i1 %942, float 1.000000e+00, float 0.000000e+00
  br label %944

; <label>:944                                     ; preds = %941, %935
  %945 = phi float [ %940, %935 ], [ %943, %941 ]
  %946 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %947 = extractvalue %dx.types.CBufRet.f32 %946, 0
  %948 = fsub fast float 1.000000e+00, %945
  %949 = fmul fast float %947, %948
  %950 = fsub fast float %948, %949
  %951 = fmul fast float %950, %518
  %952 = call float @dx.op.binary.f32(i32 35, float %714, float %951)  ; FMax(a,b)
  br label %3082

; <label>:953                                     ; preds = %919
  %954 = icmp eq i32 %904, 0
  %955 = icmp slt i32 %911, %716
  %956 = and i1 %954, %955
  br i1 %956, label %957, label %3082

; <label>:957                                     ; preds = %953
  %958 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)  ; CBufferLoadLegacy(handle,regIndex)
  %959 = extractvalue %dx.types.CBufRet.i32 %958, 0
  %960 = icmp eq i32 %959, 0
  br i1 %960, label %1390, label %961

; <label>:961                                     ; preds = %957
  %962 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 138)  ; CBufferLoadLegacy(handle,regIndex)
  %963 = extractvalue %dx.types.CBufRet.f32 %962, 0
  %964 = extractvalue %dx.types.CBufRet.f32 %962, 1
  %965 = fmul fast float %963, 2.000000e+00
  %966 = fmul fast float %964, 2.000000e+00
  %967 = fmul fast float %965, %339
  %968 = fmul fast float %966, %340
  %969 = fmul fast float %963, 1.500000e+00
  %970 = fmul fast float %964, 1.500000e+00
  %971 = fsub fast float %967, %969
  %972 = fsub fast float %968, %970
  %973 = fadd fast float %971, %908
  %974 = fadd fast float %972, %909
  %975 = fcmp fast oge float %973, 1.000000e+00
  %976 = fadd fast float %973, %965
  %977 = fcmp fast ole float %976, 0.000000e+00
  %978 = or i1 %975, %977
  %979 = fcmp fast oge float %974, 1.000000e+00
  %980 = or i1 %979, %978
  %981 = fadd fast float %974, %966
  %982 = fcmp fast ole float %981, 0.000000e+00
  %983 = or i1 %982, %980
  br i1 %983, label %3078, label %984

; <label>:984                                     ; preds = %961
  %985 = call float @dx.op.binary.f32(i32 35, float %973, float 0.000000e+00)  ; FMax(a,b)
  %986 = call float @dx.op.binary.f32(i32 35, float %974, float 0.000000e+00)  ; FMax(a,b)
  %987 = call float @dx.op.binary.f32(i32 36, float %985, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %988 = call float @dx.op.binary.f32(i32 36, float %986, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %989 = add i32 %911, 120
  %990 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %989)  ; CBufferLoadLegacy(handle,regIndex)
  %991 = extractvalue %dx.types.CBufRet.f32 %990, 0
  %992 = extractvalue %dx.types.CBufRet.f32 %990, 1
  %993 = fadd fast float %991, %987
  %994 = fadd fast float %992, %988
  %995 = call float @dx.op.unary.f32(i32 22, float %993)  ; Frc(value)
  %996 = call float @dx.op.unary.f32(i32 22, float %994)  ; Frc(value)
  %997 = uitofp i32 %911 to float
  %998 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)  ; CBufferLoadLegacy(handle,regIndex)
  %999 = extractvalue %dx.types.CBufRet.f32 %998, 0
  %1000 = extractvalue %dx.types.CBufRet.f32 %998, 1
  %1001 = fmul fast float %1000, %996
  %1002 = extractvalue %dx.types.CBufRet.f32 %998, 2
  %1003 = fmul fast float %1002, %997
  %1004 = call float @dx.op.unary.f32(i32 27, float %1001)  ; Round_ni(value)
  %1005 = fadd fast float %1004, %995
  %1006 = fmul fast float %1005, %999
  %1007 = fadd fast float %1006, %1003
  %1008 = fptoui float %1007 to i32
  %1009 = call float @dx.op.unary.f32(i32 7, float %910)  ; Saturate(value)
  %1010 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1008, i32 undef)  ; BufferLoad(srv,index,wot)
  %1011 = extractvalue %dx.types.ResRet.i32 %1010, 0
  %1012 = and i32 %1011, 1
  %1013 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %995, float %996, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1014 = extractvalue %dx.types.ResRet.f32 %1013, 0
  %1015 = icmp ne i32 %1012, 0
  %1016 = select i1 %1015, float %1014, float 1.000000e+00
  %1017 = icmp eq i32 %1012, 0
  br i1 %1017, label %1094, label %1018

; <label>:1018                                    ; preds = %984
  %1019 = add i32 %911, 1
  %1020 = icmp eq i32 %1019, %716
  %1021 = fsub fast float %997, %740
  %1022 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1023 = extractvalue %dx.types.CBufRet.i32 %1022, 1
  %1024 = and i32 %1023, 255
  %1025 = uitofp i32 %1024 to float
  %1026 = fmul fast float %1025, 0x3F847AE140000000
  %1027 = lshr i32 %1023, 8
  %1028 = and i32 %1027, 255
  %1029 = uitofp i32 %1028 to float
  %1030 = fmul fast float %1029, 0x3F847AE140000000
  %1031 = fcmp fast ogt float %1030, %1026
  br i1 %1020, label %1032, label %1050

; <label>:1032                                    ; preds = %1018
  br i1 %1031, label %1033, label %1039

; <label>:1033                                    ; preds = %1032
  %1034 = fsub fast float %1021, %1026
  %1035 = fsub fast float %1029, %1025
  %1036 = fmul fast float %1035, 0x3F847AE140000000
  %1037 = fdiv fast float %1034, %1036
  %1038 = call float @dx.op.unary.f32(i32 7, float %1037)  ; Saturate(value)
  br label %1042

; <label>:1039                                    ; preds = %1032
  %1040 = fcmp fast oge float %1021, %1030
  %1041 = select i1 %1040, float 1.000000e+00, float 0.000000e+00
  br label %1042

; <label>:1042                                    ; preds = %1039, %1033
  %1043 = phi float [ %1038, %1033 ], [ %1041, %1039 ]
  %1044 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1045 = extractvalue %dx.types.CBufRet.f32 %1044, 0
  %1046 = fmul fast float %1043, %1016
  %1047 = fsub fast float 1.000000e+00, %1043
  %1048 = fmul fast float %1045, %1047
  %1049 = fadd fast float %1048, %1046
  br label %1094

; <label>:1050                                    ; preds = %1018
  br i1 %1031, label %1051, label %1057

; <label>:1051                                    ; preds = %1050
  %1052 = fsub fast float %1021, %1026
  %1053 = fsub fast float %1029, %1025
  %1054 = fmul fast float %1053, 0x3F847AE140000000
  %1055 = fdiv fast float %1052, %1054
  %1056 = call float @dx.op.unary.f32(i32 7, float %1055)  ; Saturate(value)
  br label %1060

; <label>:1057                                    ; preds = %1050
  %1058 = fcmp fast oge float %1021, %1030
  %1059 = select i1 %1058, float 1.000000e+00, float 0.000000e+00
  br label %1060

; <label>:1060                                    ; preds = %1057, %1051
  %1061 = phi float [ %1056, %1051 ], [ %1059, %1057 ]
  %1062 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1063 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1064 = call float @dx.op.binary.f32(i32 36, float %1062, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1065 = call float @dx.op.binary.f32(i32 36, float %1063, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1066 = add i32 %911, 121
  %1067 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1066)  ; CBufferLoadLegacy(handle,regIndex)
  %1068 = extractvalue %dx.types.CBufRet.f32 %1067, 0
  %1069 = extractvalue %dx.types.CBufRet.f32 %1067, 1
  %1070 = fadd fast float %1068, %1064
  %1071 = fadd fast float %1069, %1065
  %1072 = call float @dx.op.unary.f32(i32 22, float %1070)  ; Frc(value)
  %1073 = call float @dx.op.unary.f32(i32 22, float %1071)  ; Frc(value)
  %1074 = uitofp i32 %1019 to float
  %1075 = fmul fast float %1000, %1073
  %1076 = fmul fast float %1002, %1074
  %1077 = call float @dx.op.unary.f32(i32 27, float %1075)  ; Round_ni(value)
  %1078 = fadd fast float %1077, %1072
  %1079 = fmul fast float %1078, %999
  %1080 = fadd fast float %1079, %1076
  %1081 = fptoui float %1080 to i32
  %1082 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1081, i32 undef)  ; BufferLoad(srv,index,wot)
  %1083 = extractvalue %dx.types.ResRet.i32 %1082, 0
  %1084 = and i32 %1083, 1
  %1085 = icmp eq i32 %1084, 0
  br i1 %1085, label %1094, label %1086

; <label>:1086                                    ; preds = %1060
  %1087 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1088 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1072, float %1073, float %1074, float undef, i32 0, i32 0, i32 undef, float %1087)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1089 = extractvalue %dx.types.ResRet.f32 %1088, 0
  %1090 = fmul fast float %1061, %1016
  %1091 = fsub fast float 1.000000e+00, %1061
  %1092 = fmul fast float %1089, %1091
  %1093 = fadd fast float %1092, %1090
  br label %1094

; <label>:1094                                    ; preds = %1086, %1060, %1042, %984
  %1095 = phi float [ %1016, %984 ], [ %1049, %1042 ], [ %1093, %1086 ], [ %1016, %1060 ]
  %1096 = select i1 %1015, float 1.000000e+00, float 0.000000e+00
  %1097 = call float @dx.op.binary.f32(i32 35, float %976, float 0.000000e+00)  ; FMax(a,b)
  %1098 = call float @dx.op.binary.f32(i32 36, float %1097, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1099 = fadd fast float %991, %1098
  %1100 = call float @dx.op.unary.f32(i32 22, float %1099)  ; Frc(value)
  %1101 = fadd fast float %1004, %1100
  %1102 = fmul fast float %1101, %999
  %1103 = fadd fast float %1102, %1003
  %1104 = fptoui float %1103 to i32
  %1105 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1104, i32 undef)  ; BufferLoad(srv,index,wot)
  %1106 = extractvalue %dx.types.ResRet.i32 %1105, 0
  %1107 = and i32 %1106, 1
  %1108 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1100, float %996, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1109 = extractvalue %dx.types.ResRet.f32 %1108, 0
  %1110 = icmp ne i32 %1107, 0
  %1111 = select i1 %1110, float %1109, float 1.000000e+00
  %1112 = icmp eq i32 %1107, 0
  br i1 %1112, label %1189, label %1113

; <label>:1113                                    ; preds = %1094
  %1114 = add i32 %911, 1
  %1115 = icmp eq i32 %1114, %716
  %1116 = fsub fast float %997, %740
  %1117 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1118 = extractvalue %dx.types.CBufRet.i32 %1117, 1
  %1119 = and i32 %1118, 255
  %1120 = uitofp i32 %1119 to float
  %1121 = fmul fast float %1120, 0x3F847AE140000000
  %1122 = lshr i32 %1118, 8
  %1123 = and i32 %1122, 255
  %1124 = uitofp i32 %1123 to float
  %1125 = fmul fast float %1124, 0x3F847AE140000000
  %1126 = fcmp fast ogt float %1125, %1121
  br i1 %1115, label %1127, label %1145

; <label>:1127                                    ; preds = %1113
  br i1 %1126, label %1128, label %1134

; <label>:1128                                    ; preds = %1127
  %1129 = fsub fast float %1116, %1121
  %1130 = fsub fast float %1124, %1120
  %1131 = fmul fast float %1130, 0x3F847AE140000000
  %1132 = fdiv fast float %1129, %1131
  %1133 = call float @dx.op.unary.f32(i32 7, float %1132)  ; Saturate(value)
  br label %1137

; <label>:1134                                    ; preds = %1127
  %1135 = fcmp fast oge float %1116, %1125
  %1136 = select i1 %1135, float 1.000000e+00, float 0.000000e+00
  br label %1137

; <label>:1137                                    ; preds = %1134, %1128
  %1138 = phi float [ %1133, %1128 ], [ %1136, %1134 ]
  %1139 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1140 = extractvalue %dx.types.CBufRet.f32 %1139, 0
  %1141 = fmul fast float %1138, %1111
  %1142 = fsub fast float 1.000000e+00, %1138
  %1143 = fmul fast float %1140, %1142
  %1144 = fadd fast float %1143, %1141
  br label %1189

; <label>:1145                                    ; preds = %1113
  br i1 %1126, label %1146, label %1152

; <label>:1146                                    ; preds = %1145
  %1147 = fsub fast float %1116, %1121
  %1148 = fsub fast float %1124, %1120
  %1149 = fmul fast float %1148, 0x3F847AE140000000
  %1150 = fdiv fast float %1147, %1149
  %1151 = call float @dx.op.unary.f32(i32 7, float %1150)  ; Saturate(value)
  br label %1155

; <label>:1152                                    ; preds = %1145
  %1153 = fcmp fast oge float %1116, %1125
  %1154 = select i1 %1153, float 1.000000e+00, float 0.000000e+00
  br label %1155

; <label>:1155                                    ; preds = %1152, %1146
  %1156 = phi float [ %1151, %1146 ], [ %1154, %1152 ]
  %1157 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1158 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1159 = call float @dx.op.binary.f32(i32 36, float %1157, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1160 = call float @dx.op.binary.f32(i32 36, float %1158, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1161 = add i32 %911, 121
  %1162 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1161)  ; CBufferLoadLegacy(handle,regIndex)
  %1163 = extractvalue %dx.types.CBufRet.f32 %1162, 0
  %1164 = extractvalue %dx.types.CBufRet.f32 %1162, 1
  %1165 = fadd fast float %1163, %1159
  %1166 = fadd fast float %1164, %1160
  %1167 = call float @dx.op.unary.f32(i32 22, float %1165)  ; Frc(value)
  %1168 = call float @dx.op.unary.f32(i32 22, float %1166)  ; Frc(value)
  %1169 = uitofp i32 %1114 to float
  %1170 = fmul fast float %1000, %1168
  %1171 = fmul fast float %1002, %1169
  %1172 = call float @dx.op.unary.f32(i32 27, float %1170)  ; Round_ni(value)
  %1173 = fadd fast float %1172, %1167
  %1174 = fmul fast float %1173, %999
  %1175 = fadd fast float %1174, %1171
  %1176 = fptoui float %1175 to i32
  %1177 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1176, i32 undef)  ; BufferLoad(srv,index,wot)
  %1178 = extractvalue %dx.types.ResRet.i32 %1177, 0
  %1179 = and i32 %1178, 1
  %1180 = icmp eq i32 %1179, 0
  br i1 %1180, label %1189, label %1181

; <label>:1181                                    ; preds = %1155
  %1182 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1183 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1167, float %1168, float %1169, float undef, i32 0, i32 0, i32 undef, float %1182)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1184 = extractvalue %dx.types.ResRet.f32 %1183, 0
  %1185 = fmul fast float %1156, %1111
  %1186 = fsub fast float 1.000000e+00, %1156
  %1187 = fmul fast float %1184, %1186
  %1188 = fadd fast float %1187, %1185
  br label %1189

; <label>:1189                                    ; preds = %1181, %1155, %1137, %1094
  %1190 = phi float [ %1111, %1094 ], [ %1144, %1137 ], [ %1188, %1181 ], [ %1111, %1155 ]
  %1191 = fadd fast float %1190, %1095
  %1192 = select i1 %1110, float 1.000000e+00, float 0.000000e+00
  %1193 = fadd fast float %1192, %1096
  %1194 = call float @dx.op.binary.f32(i32 35, float %981, float 0.000000e+00)  ; FMax(a,b)
  %1195 = call float @dx.op.binary.f32(i32 36, float %1194, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1196 = fadd fast float %992, %1195
  %1197 = call float @dx.op.unary.f32(i32 22, float %1196)  ; Frc(value)
  %1198 = fmul fast float %1000, %1197
  %1199 = call float @dx.op.unary.f32(i32 27, float %1198)  ; Round_ni(value)
  %1200 = fadd fast float %1199, %995
  %1201 = fmul fast float %1200, %999
  %1202 = fadd fast float %1201, %1003
  %1203 = fptoui float %1202 to i32
  %1204 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1203, i32 undef)  ; BufferLoad(srv,index,wot)
  %1205 = extractvalue %dx.types.ResRet.i32 %1204, 0
  %1206 = and i32 %1205, 1
  %1207 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %995, float %1197, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1208 = extractvalue %dx.types.ResRet.f32 %1207, 0
  %1209 = icmp ne i32 %1206, 0
  %1210 = select i1 %1209, float %1208, float 1.000000e+00
  %1211 = icmp eq i32 %1206, 0
  br i1 %1211, label %1288, label %1212

; <label>:1212                                    ; preds = %1189
  %1213 = add i32 %911, 1
  %1214 = icmp eq i32 %1213, %716
  %1215 = fsub fast float %997, %740
  %1216 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1217 = extractvalue %dx.types.CBufRet.i32 %1216, 1
  %1218 = and i32 %1217, 255
  %1219 = uitofp i32 %1218 to float
  %1220 = fmul fast float %1219, 0x3F847AE140000000
  %1221 = lshr i32 %1217, 8
  %1222 = and i32 %1221, 255
  %1223 = uitofp i32 %1222 to float
  %1224 = fmul fast float %1223, 0x3F847AE140000000
  %1225 = fcmp fast ogt float %1224, %1220
  br i1 %1214, label %1226, label %1244

; <label>:1226                                    ; preds = %1212
  br i1 %1225, label %1227, label %1233

; <label>:1227                                    ; preds = %1226
  %1228 = fsub fast float %1215, %1220
  %1229 = fsub fast float %1223, %1219
  %1230 = fmul fast float %1229, 0x3F847AE140000000
  %1231 = fdiv fast float %1228, %1230
  %1232 = call float @dx.op.unary.f32(i32 7, float %1231)  ; Saturate(value)
  br label %1236

; <label>:1233                                    ; preds = %1226
  %1234 = fcmp fast oge float %1215, %1224
  %1235 = select i1 %1234, float 1.000000e+00, float 0.000000e+00
  br label %1236

; <label>:1236                                    ; preds = %1233, %1227
  %1237 = phi float [ %1232, %1227 ], [ %1235, %1233 ]
  %1238 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1239 = extractvalue %dx.types.CBufRet.f32 %1238, 0
  %1240 = fmul fast float %1237, %1210
  %1241 = fsub fast float 1.000000e+00, %1237
  %1242 = fmul fast float %1239, %1241
  %1243 = fadd fast float %1242, %1240
  br label %1288

; <label>:1244                                    ; preds = %1212
  br i1 %1225, label %1245, label %1251

; <label>:1245                                    ; preds = %1244
  %1246 = fsub fast float %1215, %1220
  %1247 = fsub fast float %1223, %1219
  %1248 = fmul fast float %1247, 0x3F847AE140000000
  %1249 = fdiv fast float %1246, %1248
  %1250 = call float @dx.op.unary.f32(i32 7, float %1249)  ; Saturate(value)
  br label %1254

; <label>:1251                                    ; preds = %1244
  %1252 = fcmp fast oge float %1215, %1224
  %1253 = select i1 %1252, float 1.000000e+00, float 0.000000e+00
  br label %1254

; <label>:1254                                    ; preds = %1251, %1245
  %1255 = phi float [ %1250, %1245 ], [ %1253, %1251 ]
  %1256 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1257 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1258 = call float @dx.op.binary.f32(i32 36, float %1256, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1259 = call float @dx.op.binary.f32(i32 36, float %1257, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1260 = add i32 %911, 121
  %1261 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1260)  ; CBufferLoadLegacy(handle,regIndex)
  %1262 = extractvalue %dx.types.CBufRet.f32 %1261, 0
  %1263 = extractvalue %dx.types.CBufRet.f32 %1261, 1
  %1264 = fadd fast float %1262, %1258
  %1265 = fadd fast float %1263, %1259
  %1266 = call float @dx.op.unary.f32(i32 22, float %1264)  ; Frc(value)
  %1267 = call float @dx.op.unary.f32(i32 22, float %1265)  ; Frc(value)
  %1268 = uitofp i32 %1213 to float
  %1269 = fmul fast float %1000, %1267
  %1270 = fmul fast float %1002, %1268
  %1271 = call float @dx.op.unary.f32(i32 27, float %1269)  ; Round_ni(value)
  %1272 = fadd fast float %1271, %1266
  %1273 = fmul fast float %1272, %999
  %1274 = fadd fast float %1273, %1270
  %1275 = fptoui float %1274 to i32
  %1276 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1275, i32 undef)  ; BufferLoad(srv,index,wot)
  %1277 = extractvalue %dx.types.ResRet.i32 %1276, 0
  %1278 = and i32 %1277, 1
  %1279 = icmp eq i32 %1278, 0
  br i1 %1279, label %1288, label %1280

; <label>:1280                                    ; preds = %1254
  %1281 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1282 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1266, float %1267, float %1268, float undef, i32 0, i32 0, i32 undef, float %1281)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1283 = extractvalue %dx.types.ResRet.f32 %1282, 0
  %1284 = fmul fast float %1255, %1210
  %1285 = fsub fast float 1.000000e+00, %1255
  %1286 = fmul fast float %1283, %1285
  %1287 = fadd fast float %1286, %1284
  br label %1288

; <label>:1288                                    ; preds = %1280, %1254, %1236, %1189
  %1289 = phi float [ %1210, %1189 ], [ %1243, %1236 ], [ %1287, %1280 ], [ %1210, %1254 ]
  %1290 = fadd fast float %1191, %1289
  %1291 = select i1 %1209, float 1.000000e+00, float 0.000000e+00
  %1292 = fadd fast float %1193, %1291
  %1293 = fadd fast float %1199, %1100
  %1294 = fmul fast float %1293, %999
  %1295 = fadd fast float %1294, %1003
  %1296 = fptoui float %1295 to i32
  %1297 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1296, i32 undef)  ; BufferLoad(srv,index,wot)
  %1298 = extractvalue %dx.types.ResRet.i32 %1297, 0
  %1299 = and i32 %1298, 1
  %1300 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1100, float %1197, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1301 = extractvalue %dx.types.ResRet.f32 %1300, 0
  %1302 = icmp ne i32 %1299, 0
  %1303 = select i1 %1302, float %1301, float 1.000000e+00
  %1304 = icmp eq i32 %1299, 0
  br i1 %1304, label %1381, label %1305

; <label>:1305                                    ; preds = %1288
  %1306 = add i32 %911, 1
  %1307 = icmp eq i32 %1306, %716
  %1308 = fsub fast float %997, %740
  %1309 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1310 = extractvalue %dx.types.CBufRet.i32 %1309, 1
  %1311 = and i32 %1310, 255
  %1312 = uitofp i32 %1311 to float
  %1313 = fmul fast float %1312, 0x3F847AE140000000
  %1314 = lshr i32 %1310, 8
  %1315 = and i32 %1314, 255
  %1316 = uitofp i32 %1315 to float
  %1317 = fmul fast float %1316, 0x3F847AE140000000
  %1318 = fcmp fast ogt float %1317, %1313
  br i1 %1307, label %1319, label %1337

; <label>:1319                                    ; preds = %1305
  br i1 %1318, label %1320, label %1326

; <label>:1320                                    ; preds = %1319
  %1321 = fsub fast float %1308, %1313
  %1322 = fsub fast float %1316, %1312
  %1323 = fmul fast float %1322, 0x3F847AE140000000
  %1324 = fdiv fast float %1321, %1323
  %1325 = call float @dx.op.unary.f32(i32 7, float %1324)  ; Saturate(value)
  br label %1329

; <label>:1326                                    ; preds = %1319
  %1327 = fcmp fast oge float %1308, %1317
  %1328 = select i1 %1327, float 1.000000e+00, float 0.000000e+00
  br label %1329

; <label>:1329                                    ; preds = %1326, %1320
  %1330 = phi float [ %1325, %1320 ], [ %1328, %1326 ]
  %1331 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1332 = extractvalue %dx.types.CBufRet.f32 %1331, 0
  %1333 = fmul fast float %1330, %1303
  %1334 = fsub fast float 1.000000e+00, %1330
  %1335 = fmul fast float %1332, %1334
  %1336 = fadd fast float %1335, %1333
  br label %1381

; <label>:1337                                    ; preds = %1305
  br i1 %1318, label %1338, label %1344

; <label>:1338                                    ; preds = %1337
  %1339 = fsub fast float %1308, %1313
  %1340 = fsub fast float %1316, %1312
  %1341 = fmul fast float %1340, 0x3F847AE140000000
  %1342 = fdiv fast float %1339, %1341
  %1343 = call float @dx.op.unary.f32(i32 7, float %1342)  ; Saturate(value)
  br label %1347

; <label>:1344                                    ; preds = %1337
  %1345 = fcmp fast oge float %1308, %1317
  %1346 = select i1 %1345, float 1.000000e+00, float 0.000000e+00
  br label %1347

; <label>:1347                                    ; preds = %1344, %1338
  %1348 = phi float [ %1343, %1338 ], [ %1346, %1344 ]
  %1349 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1350 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1351 = call float @dx.op.binary.f32(i32 36, float %1349, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1352 = call float @dx.op.binary.f32(i32 36, float %1350, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1353 = add i32 %911, 121
  %1354 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1353)  ; CBufferLoadLegacy(handle,regIndex)
  %1355 = extractvalue %dx.types.CBufRet.f32 %1354, 0
  %1356 = extractvalue %dx.types.CBufRet.f32 %1354, 1
  %1357 = fadd fast float %1355, %1351
  %1358 = fadd fast float %1356, %1352
  %1359 = call float @dx.op.unary.f32(i32 22, float %1357)  ; Frc(value)
  %1360 = call float @dx.op.unary.f32(i32 22, float %1358)  ; Frc(value)
  %1361 = uitofp i32 %1306 to float
  %1362 = fmul fast float %1000, %1360
  %1363 = fmul fast float %1002, %1361
  %1364 = call float @dx.op.unary.f32(i32 27, float %1362)  ; Round_ni(value)
  %1365 = fadd fast float %1364, %1359
  %1366 = fmul fast float %1365, %999
  %1367 = fadd fast float %1366, %1363
  %1368 = fptoui float %1367 to i32
  %1369 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1368, i32 undef)  ; BufferLoad(srv,index,wot)
  %1370 = extractvalue %dx.types.ResRet.i32 %1369, 0
  %1371 = and i32 %1370, 1
  %1372 = icmp eq i32 %1371, 0
  br i1 %1372, label %1381, label %1373

; <label>:1373                                    ; preds = %1347
  %1374 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1375 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1359, float %1360, float %1361, float undef, i32 0, i32 0, i32 undef, float %1374)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1376 = extractvalue %dx.types.ResRet.f32 %1375, 0
  %1377 = fmul fast float %1348, %1303
  %1378 = fsub fast float 1.000000e+00, %1348
  %1379 = fmul fast float %1376, %1378
  %1380 = fadd fast float %1379, %1377
  br label %1381

; <label>:1381                                    ; preds = %1373, %1347, %1329, %1288
  %1382 = phi float [ %1303, %1288 ], [ %1336, %1329 ], [ %1380, %1373 ], [ %1303, %1347 ]
  %1383 = fadd fast float %1290, %1382
  %1384 = select i1 %1302, float 1.000000e+00, float 0.000000e+00
  %1385 = fadd fast float %1292, %1384
  %1386 = fcmp fast une float %1385, 0.000000e+00
  %1387 = fdiv fast float %1383, %1385
  %1388 = fsub fast float 1.000000e+00, %1387
  %1389 = select i1 %1386, float %1388, float 0.000000e+00
  br label %3078

; <label>:1390                                    ; preds = %957
  %1391 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)  ; CBufferLoadLegacy(handle,regIndex)
  %1392 = extractvalue %dx.types.CBufRet.i32 %1391, 0
  %1393 = icmp eq i32 %1392, 0
  br i1 %1393, label %2965, label %1394

; <label>:1394                                    ; preds = %1390
  %1395 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 138)  ; CBufferLoadLegacy(handle,regIndex)
  %1396 = extractvalue %dx.types.CBufRet.f32 %1395, 0
  %1397 = extractvalue %dx.types.CBufRet.f32 %1395, 1
  %1398 = fmul fast float %1396, 1.500000e+00
  %1399 = fmul fast float %1397, 1.500000e+00
  %1400 = fsub fast float %908, %1398
  %1401 = fsub fast float %909, %1399
  %1402 = fcmp fast oge float %1400, 1.000000e+00
  %1403 = fmul fast float %1396, 3.000000e+00
  %1404 = fadd fast float %1400, %1403
  %1405 = fcmp fast ole float %1404, 0.000000e+00
  %1406 = or i1 %1402, %1405
  %1407 = fcmp fast oge float %1401, 1.000000e+00
  %1408 = or i1 %1407, %1406
  %1409 = fmul fast float %1397, 3.000000e+00
  %1410 = fadd fast float %1401, %1409
  %1411 = fcmp fast ole float %1410, 0.000000e+00
  %1412 = or i1 %1411, %1408
  br i1 %1412, label %3078, label %1413

; <label>:1413                                    ; preds = %1394
  %1414 = call float @dx.op.binary.f32(i32 35, float %1400, float 0.000000e+00)  ; FMax(a,b)
  %1415 = call float @dx.op.binary.f32(i32 35, float %1401, float 0.000000e+00)  ; FMax(a,b)
  %1416 = call float @dx.op.binary.f32(i32 36, float %1414, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1417 = call float @dx.op.binary.f32(i32 36, float %1415, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1418 = add i32 %911, 120
  %1419 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1418)  ; CBufferLoadLegacy(handle,regIndex)
  %1420 = extractvalue %dx.types.CBufRet.f32 %1419, 0
  %1421 = extractvalue %dx.types.CBufRet.f32 %1419, 1
  %1422 = fadd fast float %1420, %1416
  %1423 = fadd fast float %1421, %1417
  %1424 = call float @dx.op.unary.f32(i32 22, float %1422)  ; Frc(value)
  %1425 = call float @dx.op.unary.f32(i32 22, float %1423)  ; Frc(value)
  %1426 = uitofp i32 %911 to float
  %1427 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)  ; CBufferLoadLegacy(handle,regIndex)
  %1428 = extractvalue %dx.types.CBufRet.f32 %1427, 0
  %1429 = extractvalue %dx.types.CBufRet.f32 %1427, 1
  %1430 = fmul fast float %1429, %1425
  %1431 = extractvalue %dx.types.CBufRet.f32 %1427, 2
  %1432 = fmul fast float %1431, %1426
  %1433 = call float @dx.op.unary.f32(i32 27, float %1430)  ; Round_ni(value)
  %1434 = fadd fast float %1433, %1424
  %1435 = fmul fast float %1434, %1428
  %1436 = fadd fast float %1435, %1432
  %1437 = fptoui float %1436 to i32
  %1438 = call float @dx.op.unary.f32(i32 7, float %910)  ; Saturate(value)
  %1439 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1437, i32 undef)  ; BufferLoad(srv,index,wot)
  %1440 = extractvalue %dx.types.ResRet.i32 %1439, 0
  %1441 = and i32 %1440, 1
  %1442 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1424, float %1425, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1443 = extractvalue %dx.types.ResRet.f32 %1442, 0
  %1444 = icmp ne i32 %1441, 0
  %1445 = select i1 %1444, float %1443, float 1.000000e+00
  %1446 = icmp eq i32 %1441, 0
  br i1 %1446, label %1523, label %1447

; <label>:1447                                    ; preds = %1413
  %1448 = add i32 %911, 1
  %1449 = icmp eq i32 %1448, %716
  %1450 = fsub fast float %1426, %740
  %1451 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1452 = extractvalue %dx.types.CBufRet.i32 %1451, 1
  %1453 = and i32 %1452, 255
  %1454 = uitofp i32 %1453 to float
  %1455 = fmul fast float %1454, 0x3F847AE140000000
  %1456 = lshr i32 %1452, 8
  %1457 = and i32 %1456, 255
  %1458 = uitofp i32 %1457 to float
  %1459 = fmul fast float %1458, 0x3F847AE140000000
  %1460 = fcmp fast ogt float %1459, %1455
  br i1 %1449, label %1461, label %1479

; <label>:1461                                    ; preds = %1447
  br i1 %1460, label %1462, label %1468

; <label>:1462                                    ; preds = %1461
  %1463 = fsub fast float %1450, %1455
  %1464 = fsub fast float %1458, %1454
  %1465 = fmul fast float %1464, 0x3F847AE140000000
  %1466 = fdiv fast float %1463, %1465
  %1467 = call float @dx.op.unary.f32(i32 7, float %1466)  ; Saturate(value)
  br label %1471

; <label>:1468                                    ; preds = %1461
  %1469 = fcmp fast oge float %1450, %1459
  %1470 = select i1 %1469, float 1.000000e+00, float 0.000000e+00
  br label %1471

; <label>:1471                                    ; preds = %1468, %1462
  %1472 = phi float [ %1467, %1462 ], [ %1470, %1468 ]
  %1473 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1474 = extractvalue %dx.types.CBufRet.f32 %1473, 0
  %1475 = fmul fast float %1472, %1445
  %1476 = fsub fast float 1.000000e+00, %1472
  %1477 = fmul fast float %1474, %1476
  %1478 = fadd fast float %1477, %1475
  br label %1523

; <label>:1479                                    ; preds = %1447
  br i1 %1460, label %1480, label %1486

; <label>:1480                                    ; preds = %1479
  %1481 = fsub fast float %1450, %1455
  %1482 = fsub fast float %1458, %1454
  %1483 = fmul fast float %1482, 0x3F847AE140000000
  %1484 = fdiv fast float %1481, %1483
  %1485 = call float @dx.op.unary.f32(i32 7, float %1484)  ; Saturate(value)
  br label %1489

; <label>:1486                                    ; preds = %1479
  %1487 = fcmp fast oge float %1450, %1459
  %1488 = select i1 %1487, float 1.000000e+00, float 0.000000e+00
  br label %1489

; <label>:1489                                    ; preds = %1486, %1480
  %1490 = phi float [ %1485, %1480 ], [ %1488, %1486 ]
  %1491 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1492 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1493 = call float @dx.op.binary.f32(i32 36, float %1491, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1494 = call float @dx.op.binary.f32(i32 36, float %1492, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1495 = add i32 %911, 121
  %1496 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1495)  ; CBufferLoadLegacy(handle,regIndex)
  %1497 = extractvalue %dx.types.CBufRet.f32 %1496, 0
  %1498 = extractvalue %dx.types.CBufRet.f32 %1496, 1
  %1499 = fadd fast float %1497, %1493
  %1500 = fadd fast float %1498, %1494
  %1501 = call float @dx.op.unary.f32(i32 22, float %1499)  ; Frc(value)
  %1502 = call float @dx.op.unary.f32(i32 22, float %1500)  ; Frc(value)
  %1503 = uitofp i32 %1448 to float
  %1504 = fmul fast float %1429, %1502
  %1505 = fmul fast float %1431, %1503
  %1506 = call float @dx.op.unary.f32(i32 27, float %1504)  ; Round_ni(value)
  %1507 = fadd fast float %1506, %1501
  %1508 = fmul fast float %1507, %1428
  %1509 = fadd fast float %1508, %1505
  %1510 = fptoui float %1509 to i32
  %1511 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1510, i32 undef)  ; BufferLoad(srv,index,wot)
  %1512 = extractvalue %dx.types.ResRet.i32 %1511, 0
  %1513 = and i32 %1512, 1
  %1514 = icmp eq i32 %1513, 0
  br i1 %1514, label %1523, label %1515

; <label>:1515                                    ; preds = %1489
  %1516 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1517 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1501, float %1502, float %1503, float undef, i32 0, i32 0, i32 undef, float %1516)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1518 = extractvalue %dx.types.ResRet.f32 %1517, 0
  %1519 = fmul fast float %1490, %1445
  %1520 = fsub fast float 1.000000e+00, %1490
  %1521 = fmul fast float %1518, %1520
  %1522 = fadd fast float %1521, %1519
  br label %1523

; <label>:1523                                    ; preds = %1515, %1489, %1471, %1413
  %1524 = phi float [ %1445, %1413 ], [ %1478, %1471 ], [ %1522, %1515 ], [ %1445, %1489 ]
  %1525 = select i1 %1444, float 1.000000e+00, float 0.000000e+00
  %1526 = fadd fast float %1400, %1396
  %1527 = call float @dx.op.binary.f32(i32 35, float %1526, float 0.000000e+00)  ; FMax(a,b)
  %1528 = call float @dx.op.binary.f32(i32 36, float %1527, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1529 = fadd fast float %1420, %1528
  %1530 = call float @dx.op.unary.f32(i32 22, float %1529)  ; Frc(value)
  %1531 = fadd fast float %1433, %1530
  %1532 = fmul fast float %1531, %1428
  %1533 = fadd fast float %1532, %1432
  %1534 = fptoui float %1533 to i32
  %1535 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1534, i32 undef)  ; BufferLoad(srv,index,wot)
  %1536 = extractvalue %dx.types.ResRet.i32 %1535, 0
  %1537 = and i32 %1536, 1
  %1538 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1530, float %1425, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1539 = extractvalue %dx.types.ResRet.f32 %1538, 0
  %1540 = icmp ne i32 %1537, 0
  %1541 = select i1 %1540, float %1539, float 1.000000e+00
  %1542 = icmp eq i32 %1537, 0
  br i1 %1542, label %1619, label %1543

; <label>:1543                                    ; preds = %1523
  %1544 = add i32 %911, 1
  %1545 = icmp eq i32 %1544, %716
  %1546 = fsub fast float %1426, %740
  %1547 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1548 = extractvalue %dx.types.CBufRet.i32 %1547, 1
  %1549 = and i32 %1548, 255
  %1550 = uitofp i32 %1549 to float
  %1551 = fmul fast float %1550, 0x3F847AE140000000
  %1552 = lshr i32 %1548, 8
  %1553 = and i32 %1552, 255
  %1554 = uitofp i32 %1553 to float
  %1555 = fmul fast float %1554, 0x3F847AE140000000
  %1556 = fcmp fast ogt float %1555, %1551
  br i1 %1545, label %1557, label %1575

; <label>:1557                                    ; preds = %1543
  br i1 %1556, label %1558, label %1564

; <label>:1558                                    ; preds = %1557
  %1559 = fsub fast float %1546, %1551
  %1560 = fsub fast float %1554, %1550
  %1561 = fmul fast float %1560, 0x3F847AE140000000
  %1562 = fdiv fast float %1559, %1561
  %1563 = call float @dx.op.unary.f32(i32 7, float %1562)  ; Saturate(value)
  br label %1567

; <label>:1564                                    ; preds = %1557
  %1565 = fcmp fast oge float %1546, %1555
  %1566 = select i1 %1565, float 1.000000e+00, float 0.000000e+00
  br label %1567

; <label>:1567                                    ; preds = %1564, %1558
  %1568 = phi float [ %1563, %1558 ], [ %1566, %1564 ]
  %1569 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1570 = extractvalue %dx.types.CBufRet.f32 %1569, 0
  %1571 = fmul fast float %1568, %1541
  %1572 = fsub fast float 1.000000e+00, %1568
  %1573 = fmul fast float %1570, %1572
  %1574 = fadd fast float %1573, %1571
  br label %1619

; <label>:1575                                    ; preds = %1543
  br i1 %1556, label %1576, label %1582

; <label>:1576                                    ; preds = %1575
  %1577 = fsub fast float %1546, %1551
  %1578 = fsub fast float %1554, %1550
  %1579 = fmul fast float %1578, 0x3F847AE140000000
  %1580 = fdiv fast float %1577, %1579
  %1581 = call float @dx.op.unary.f32(i32 7, float %1580)  ; Saturate(value)
  br label %1585

; <label>:1582                                    ; preds = %1575
  %1583 = fcmp fast oge float %1546, %1555
  %1584 = select i1 %1583, float 1.000000e+00, float 0.000000e+00
  br label %1585

; <label>:1585                                    ; preds = %1582, %1576
  %1586 = phi float [ %1581, %1576 ], [ %1584, %1582 ]
  %1587 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1588 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1589 = call float @dx.op.binary.f32(i32 36, float %1587, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1590 = call float @dx.op.binary.f32(i32 36, float %1588, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1591 = add i32 %911, 121
  %1592 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1591)  ; CBufferLoadLegacy(handle,regIndex)
  %1593 = extractvalue %dx.types.CBufRet.f32 %1592, 0
  %1594 = extractvalue %dx.types.CBufRet.f32 %1592, 1
  %1595 = fadd fast float %1593, %1589
  %1596 = fadd fast float %1594, %1590
  %1597 = call float @dx.op.unary.f32(i32 22, float %1595)  ; Frc(value)
  %1598 = call float @dx.op.unary.f32(i32 22, float %1596)  ; Frc(value)
  %1599 = uitofp i32 %1544 to float
  %1600 = fmul fast float %1429, %1598
  %1601 = fmul fast float %1431, %1599
  %1602 = call float @dx.op.unary.f32(i32 27, float %1600)  ; Round_ni(value)
  %1603 = fadd fast float %1602, %1597
  %1604 = fmul fast float %1603, %1428
  %1605 = fadd fast float %1604, %1601
  %1606 = fptoui float %1605 to i32
  %1607 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1606, i32 undef)  ; BufferLoad(srv,index,wot)
  %1608 = extractvalue %dx.types.ResRet.i32 %1607, 0
  %1609 = and i32 %1608, 1
  %1610 = icmp eq i32 %1609, 0
  br i1 %1610, label %1619, label %1611

; <label>:1611                                    ; preds = %1585
  %1612 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1613 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1597, float %1598, float %1599, float undef, i32 0, i32 0, i32 undef, float %1612)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1614 = extractvalue %dx.types.ResRet.f32 %1613, 0
  %1615 = fmul fast float %1586, %1541
  %1616 = fsub fast float 1.000000e+00, %1586
  %1617 = fmul fast float %1614, %1616
  %1618 = fadd fast float %1617, %1615
  br label %1619

; <label>:1619                                    ; preds = %1611, %1585, %1567, %1523
  %1620 = phi float [ %1541, %1523 ], [ %1574, %1567 ], [ %1618, %1611 ], [ %1541, %1585 ]
  %1621 = select i1 %1540, float 1.000000e+00, float 0.000000e+00
  %1622 = fadd fast float %1621, %1525
  %1623 = fadd fast float %1526, %1396
  %1624 = call float @dx.op.binary.f32(i32 35, float %1623, float 0.000000e+00)  ; FMax(a,b)
  %1625 = call float @dx.op.binary.f32(i32 36, float %1624, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1626 = fadd fast float %1420, %1625
  %1627 = call float @dx.op.unary.f32(i32 22, float %1626)  ; Frc(value)
  %1628 = fadd fast float %1433, %1627
  %1629 = fmul fast float %1628, %1428
  %1630 = fadd fast float %1629, %1432
  %1631 = fptoui float %1630 to i32
  %1632 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1631, i32 undef)  ; BufferLoad(srv,index,wot)
  %1633 = extractvalue %dx.types.ResRet.i32 %1632, 0
  %1634 = and i32 %1633, 1
  %1635 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1627, float %1425, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1636 = extractvalue %dx.types.ResRet.f32 %1635, 0
  %1637 = icmp ne i32 %1634, 0
  %1638 = select i1 %1637, float %1636, float 1.000000e+00
  %1639 = icmp eq i32 %1634, 0
  br i1 %1639, label %1716, label %1640

; <label>:1640                                    ; preds = %1619
  %1641 = add i32 %911, 1
  %1642 = icmp eq i32 %1641, %716
  %1643 = fsub fast float %1426, %740
  %1644 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1645 = extractvalue %dx.types.CBufRet.i32 %1644, 1
  %1646 = and i32 %1645, 255
  %1647 = uitofp i32 %1646 to float
  %1648 = fmul fast float %1647, 0x3F847AE140000000
  %1649 = lshr i32 %1645, 8
  %1650 = and i32 %1649, 255
  %1651 = uitofp i32 %1650 to float
  %1652 = fmul fast float %1651, 0x3F847AE140000000
  %1653 = fcmp fast ogt float %1652, %1648
  br i1 %1642, label %1654, label %1672

; <label>:1654                                    ; preds = %1640
  br i1 %1653, label %1655, label %1661

; <label>:1655                                    ; preds = %1654
  %1656 = fsub fast float %1643, %1648
  %1657 = fsub fast float %1651, %1647
  %1658 = fmul fast float %1657, 0x3F847AE140000000
  %1659 = fdiv fast float %1656, %1658
  %1660 = call float @dx.op.unary.f32(i32 7, float %1659)  ; Saturate(value)
  br label %1664

; <label>:1661                                    ; preds = %1654
  %1662 = fcmp fast oge float %1643, %1652
  %1663 = select i1 %1662, float 1.000000e+00, float 0.000000e+00
  br label %1664

; <label>:1664                                    ; preds = %1661, %1655
  %1665 = phi float [ %1660, %1655 ], [ %1663, %1661 ]
  %1666 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1667 = extractvalue %dx.types.CBufRet.f32 %1666, 0
  %1668 = fmul fast float %1665, %1638
  %1669 = fsub fast float 1.000000e+00, %1665
  %1670 = fmul fast float %1667, %1669
  %1671 = fadd fast float %1670, %1668
  br label %1716

; <label>:1672                                    ; preds = %1640
  br i1 %1653, label %1673, label %1679

; <label>:1673                                    ; preds = %1672
  %1674 = fsub fast float %1643, %1648
  %1675 = fsub fast float %1651, %1647
  %1676 = fmul fast float %1675, 0x3F847AE140000000
  %1677 = fdiv fast float %1674, %1676
  %1678 = call float @dx.op.unary.f32(i32 7, float %1677)  ; Saturate(value)
  br label %1682

; <label>:1679                                    ; preds = %1672
  %1680 = fcmp fast oge float %1643, %1652
  %1681 = select i1 %1680, float 1.000000e+00, float 0.000000e+00
  br label %1682

; <label>:1682                                    ; preds = %1679, %1673
  %1683 = phi float [ %1678, %1673 ], [ %1681, %1679 ]
  %1684 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1685 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1686 = call float @dx.op.binary.f32(i32 36, float %1684, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1687 = call float @dx.op.binary.f32(i32 36, float %1685, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1688 = add i32 %911, 121
  %1689 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1688)  ; CBufferLoadLegacy(handle,regIndex)
  %1690 = extractvalue %dx.types.CBufRet.f32 %1689, 0
  %1691 = extractvalue %dx.types.CBufRet.f32 %1689, 1
  %1692 = fadd fast float %1690, %1686
  %1693 = fadd fast float %1691, %1687
  %1694 = call float @dx.op.unary.f32(i32 22, float %1692)  ; Frc(value)
  %1695 = call float @dx.op.unary.f32(i32 22, float %1693)  ; Frc(value)
  %1696 = uitofp i32 %1641 to float
  %1697 = fmul fast float %1429, %1695
  %1698 = fmul fast float %1431, %1696
  %1699 = call float @dx.op.unary.f32(i32 27, float %1697)  ; Round_ni(value)
  %1700 = fadd fast float %1699, %1694
  %1701 = fmul fast float %1700, %1428
  %1702 = fadd fast float %1701, %1698
  %1703 = fptoui float %1702 to i32
  %1704 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1703, i32 undef)  ; BufferLoad(srv,index,wot)
  %1705 = extractvalue %dx.types.ResRet.i32 %1704, 0
  %1706 = and i32 %1705, 1
  %1707 = icmp eq i32 %1706, 0
  br i1 %1707, label %1716, label %1708

; <label>:1708                                    ; preds = %1682
  %1709 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1710 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1694, float %1695, float %1696, float undef, i32 0, i32 0, i32 undef, float %1709)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1711 = extractvalue %dx.types.ResRet.f32 %1710, 0
  %1712 = fmul fast float %1683, %1638
  %1713 = fsub fast float 1.000000e+00, %1683
  %1714 = fmul fast float %1711, %1713
  %1715 = fadd fast float %1714, %1712
  br label %1716

; <label>:1716                                    ; preds = %1708, %1682, %1664, %1619
  %1717 = phi float [ %1638, %1619 ], [ %1671, %1664 ], [ %1715, %1708 ], [ %1638, %1682 ]
  %1718 = select i1 %1637, float 1.000000e+00, float 0.000000e+00
  %1719 = fadd fast float %1622, %1718
  %1720 = fadd fast float %1623, %1396
  %1721 = call float @dx.op.binary.f32(i32 35, float %1720, float 0.000000e+00)  ; FMax(a,b)
  %1722 = call float @dx.op.binary.f32(i32 36, float %1721, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1723 = fadd fast float %1420, %1722
  %1724 = call float @dx.op.unary.f32(i32 22, float %1723)  ; Frc(value)
  %1725 = fadd fast float %1433, %1724
  %1726 = fmul fast float %1725, %1428
  %1727 = fadd fast float %1726, %1432
  %1728 = fptoui float %1727 to i32
  %1729 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1728, i32 undef)  ; BufferLoad(srv,index,wot)
  %1730 = extractvalue %dx.types.ResRet.i32 %1729, 0
  %1731 = and i32 %1730, 1
  %1732 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1724, float %1425, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1733 = extractvalue %dx.types.ResRet.f32 %1732, 0
  %1734 = icmp ne i32 %1731, 0
  %1735 = select i1 %1734, float %1733, float 1.000000e+00
  %1736 = icmp eq i32 %1731, 0
  br i1 %1736, label %1813, label %1737

; <label>:1737                                    ; preds = %1716
  %1738 = add i32 %911, 1
  %1739 = icmp eq i32 %1738, %716
  %1740 = fsub fast float %1426, %740
  %1741 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1742 = extractvalue %dx.types.CBufRet.i32 %1741, 1
  %1743 = and i32 %1742, 255
  %1744 = uitofp i32 %1743 to float
  %1745 = fmul fast float %1744, 0x3F847AE140000000
  %1746 = lshr i32 %1742, 8
  %1747 = and i32 %1746, 255
  %1748 = uitofp i32 %1747 to float
  %1749 = fmul fast float %1748, 0x3F847AE140000000
  %1750 = fcmp fast ogt float %1749, %1745
  br i1 %1739, label %1751, label %1769

; <label>:1751                                    ; preds = %1737
  br i1 %1750, label %1752, label %1758

; <label>:1752                                    ; preds = %1751
  %1753 = fsub fast float %1740, %1745
  %1754 = fsub fast float %1748, %1744
  %1755 = fmul fast float %1754, 0x3F847AE140000000
  %1756 = fdiv fast float %1753, %1755
  %1757 = call float @dx.op.unary.f32(i32 7, float %1756)  ; Saturate(value)
  br label %1761

; <label>:1758                                    ; preds = %1751
  %1759 = fcmp fast oge float %1740, %1749
  %1760 = select i1 %1759, float 1.000000e+00, float 0.000000e+00
  br label %1761

; <label>:1761                                    ; preds = %1758, %1752
  %1762 = phi float [ %1757, %1752 ], [ %1760, %1758 ]
  %1763 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1764 = extractvalue %dx.types.CBufRet.f32 %1763, 0
  %1765 = fmul fast float %1762, %1735
  %1766 = fsub fast float 1.000000e+00, %1762
  %1767 = fmul fast float %1764, %1766
  %1768 = fadd fast float %1767, %1765
  br label %1813

; <label>:1769                                    ; preds = %1737
  br i1 %1750, label %1770, label %1776

; <label>:1770                                    ; preds = %1769
  %1771 = fsub fast float %1740, %1745
  %1772 = fsub fast float %1748, %1744
  %1773 = fmul fast float %1772, 0x3F847AE140000000
  %1774 = fdiv fast float %1771, %1773
  %1775 = call float @dx.op.unary.f32(i32 7, float %1774)  ; Saturate(value)
  br label %1779

; <label>:1776                                    ; preds = %1769
  %1777 = fcmp fast oge float %1740, %1749
  %1778 = select i1 %1777, float 1.000000e+00, float 0.000000e+00
  br label %1779

; <label>:1779                                    ; preds = %1776, %1770
  %1780 = phi float [ %1775, %1770 ], [ %1778, %1776 ]
  %1781 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1782 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1783 = call float @dx.op.binary.f32(i32 36, float %1781, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1784 = call float @dx.op.binary.f32(i32 36, float %1782, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1785 = add i32 %911, 121
  %1786 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1785)  ; CBufferLoadLegacy(handle,regIndex)
  %1787 = extractvalue %dx.types.CBufRet.f32 %1786, 0
  %1788 = extractvalue %dx.types.CBufRet.f32 %1786, 1
  %1789 = fadd fast float %1787, %1783
  %1790 = fadd fast float %1788, %1784
  %1791 = call float @dx.op.unary.f32(i32 22, float %1789)  ; Frc(value)
  %1792 = call float @dx.op.unary.f32(i32 22, float %1790)  ; Frc(value)
  %1793 = uitofp i32 %1738 to float
  %1794 = fmul fast float %1429, %1792
  %1795 = fmul fast float %1431, %1793
  %1796 = call float @dx.op.unary.f32(i32 27, float %1794)  ; Round_ni(value)
  %1797 = fadd fast float %1796, %1791
  %1798 = fmul fast float %1797, %1428
  %1799 = fadd fast float %1798, %1795
  %1800 = fptoui float %1799 to i32
  %1801 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1800, i32 undef)  ; BufferLoad(srv,index,wot)
  %1802 = extractvalue %dx.types.ResRet.i32 %1801, 0
  %1803 = and i32 %1802, 1
  %1804 = icmp eq i32 %1803, 0
  br i1 %1804, label %1813, label %1805

; <label>:1805                                    ; preds = %1779
  %1806 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1807 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1791, float %1792, float %1793, float undef, i32 0, i32 0, i32 undef, float %1806)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1808 = extractvalue %dx.types.ResRet.f32 %1807, 0
  %1809 = fmul fast float %1780, %1735
  %1810 = fsub fast float 1.000000e+00, %1780
  %1811 = fmul fast float %1808, %1810
  %1812 = fadd fast float %1811, %1809
  br label %1813

; <label>:1813                                    ; preds = %1805, %1779, %1761, %1716
  %1814 = phi float [ %1735, %1716 ], [ %1768, %1761 ], [ %1812, %1805 ], [ %1735, %1779 ]
  %1815 = select i1 %1734, float 1.000000e+00, float 0.000000e+00
  %1816 = fadd fast float %1719, %1815
  %1817 = fadd fast float %1401, %1397
  %1818 = call float @dx.op.binary.f32(i32 35, float %1817, float 0.000000e+00)  ; FMax(a,b)
  %1819 = call float @dx.op.binary.f32(i32 36, float %1818, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1820 = fadd fast float %1421, %1819
  %1821 = call float @dx.op.unary.f32(i32 22, float %1820)  ; Frc(value)
  %1822 = fmul fast float %1429, %1821
  %1823 = call float @dx.op.unary.f32(i32 27, float %1822)  ; Round_ni(value)
  %1824 = fadd fast float %1823, %1424
  %1825 = fmul fast float %1824, %1428
  %1826 = fadd fast float %1825, %1432
  %1827 = fptoui float %1826 to i32
  %1828 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1827, i32 undef)  ; BufferLoad(srv,index,wot)
  %1829 = extractvalue %dx.types.ResRet.i32 %1828, 0
  %1830 = and i32 %1829, 1
  %1831 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1424, float %1821, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1832 = extractvalue %dx.types.ResRet.f32 %1831, 0
  %1833 = icmp ne i32 %1830, 0
  %1834 = select i1 %1833, float %1832, float 1.000000e+00
  %1835 = icmp eq i32 %1830, 0
  br i1 %1835, label %1912, label %1836

; <label>:1836                                    ; preds = %1813
  %1837 = add i32 %911, 1
  %1838 = icmp eq i32 %1837, %716
  %1839 = fsub fast float %1426, %740
  %1840 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1841 = extractvalue %dx.types.CBufRet.i32 %1840, 1
  %1842 = and i32 %1841, 255
  %1843 = uitofp i32 %1842 to float
  %1844 = fmul fast float %1843, 0x3F847AE140000000
  %1845 = lshr i32 %1841, 8
  %1846 = and i32 %1845, 255
  %1847 = uitofp i32 %1846 to float
  %1848 = fmul fast float %1847, 0x3F847AE140000000
  %1849 = fcmp fast ogt float %1848, %1844
  br i1 %1838, label %1850, label %1868

; <label>:1850                                    ; preds = %1836
  br i1 %1849, label %1851, label %1857

; <label>:1851                                    ; preds = %1850
  %1852 = fsub fast float %1839, %1844
  %1853 = fsub fast float %1847, %1843
  %1854 = fmul fast float %1853, 0x3F847AE140000000
  %1855 = fdiv fast float %1852, %1854
  %1856 = call float @dx.op.unary.f32(i32 7, float %1855)  ; Saturate(value)
  br label %1860

; <label>:1857                                    ; preds = %1850
  %1858 = fcmp fast oge float %1839, %1848
  %1859 = select i1 %1858, float 1.000000e+00, float 0.000000e+00
  br label %1860

; <label>:1860                                    ; preds = %1857, %1851
  %1861 = phi float [ %1856, %1851 ], [ %1859, %1857 ]
  %1862 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1863 = extractvalue %dx.types.CBufRet.f32 %1862, 0
  %1864 = fmul fast float %1861, %1834
  %1865 = fsub fast float 1.000000e+00, %1861
  %1866 = fmul fast float %1863, %1865
  %1867 = fadd fast float %1866, %1864
  br label %1912

; <label>:1868                                    ; preds = %1836
  br i1 %1849, label %1869, label %1875

; <label>:1869                                    ; preds = %1868
  %1870 = fsub fast float %1839, %1844
  %1871 = fsub fast float %1847, %1843
  %1872 = fmul fast float %1871, 0x3F847AE140000000
  %1873 = fdiv fast float %1870, %1872
  %1874 = call float @dx.op.unary.f32(i32 7, float %1873)  ; Saturate(value)
  br label %1878

; <label>:1875                                    ; preds = %1868
  %1876 = fcmp fast oge float %1839, %1848
  %1877 = select i1 %1876, float 1.000000e+00, float 0.000000e+00
  br label %1878

; <label>:1878                                    ; preds = %1875, %1869
  %1879 = phi float [ %1874, %1869 ], [ %1877, %1875 ]
  %1880 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1881 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1882 = call float @dx.op.binary.f32(i32 36, float %1880, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1883 = call float @dx.op.binary.f32(i32 36, float %1881, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1884 = add i32 %911, 121
  %1885 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1884)  ; CBufferLoadLegacy(handle,regIndex)
  %1886 = extractvalue %dx.types.CBufRet.f32 %1885, 0
  %1887 = extractvalue %dx.types.CBufRet.f32 %1885, 1
  %1888 = fadd fast float %1886, %1882
  %1889 = fadd fast float %1887, %1883
  %1890 = call float @dx.op.unary.f32(i32 22, float %1888)  ; Frc(value)
  %1891 = call float @dx.op.unary.f32(i32 22, float %1889)  ; Frc(value)
  %1892 = uitofp i32 %1837 to float
  %1893 = fmul fast float %1429, %1891
  %1894 = fmul fast float %1431, %1892
  %1895 = call float @dx.op.unary.f32(i32 27, float %1893)  ; Round_ni(value)
  %1896 = fadd fast float %1895, %1890
  %1897 = fmul fast float %1896, %1428
  %1898 = fadd fast float %1897, %1894
  %1899 = fptoui float %1898 to i32
  %1900 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1899, i32 undef)  ; BufferLoad(srv,index,wot)
  %1901 = extractvalue %dx.types.ResRet.i32 %1900, 0
  %1902 = and i32 %1901, 1
  %1903 = icmp eq i32 %1902, 0
  br i1 %1903, label %1912, label %1904

; <label>:1904                                    ; preds = %1878
  %1905 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1906 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1890, float %1891, float %1892, float undef, i32 0, i32 0, i32 undef, float %1905)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1907 = extractvalue %dx.types.ResRet.f32 %1906, 0
  %1908 = fmul fast float %1879, %1834
  %1909 = fsub fast float 1.000000e+00, %1879
  %1910 = fmul fast float %1907, %1909
  %1911 = fadd fast float %1910, %1908
  br label %1912

; <label>:1912                                    ; preds = %1904, %1878, %1860, %1813
  %1913 = phi float [ %1834, %1813 ], [ %1867, %1860 ], [ %1911, %1904 ], [ %1834, %1878 ]
  %1914 = select i1 %1833, float 1.000000e+00, float 0.000000e+00
  %1915 = fadd fast float %1816, %1914
  %1916 = fadd fast float %1823, %1530
  %1917 = fmul fast float %1916, %1428
  %1918 = fadd fast float %1917, %1432
  %1919 = fptoui float %1918 to i32
  %1920 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1919, i32 undef)  ; BufferLoad(srv,index,wot)
  %1921 = extractvalue %dx.types.ResRet.i32 %1920, 0
  %1922 = and i32 %1921, 1
  %1923 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1530, float %1821, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1924 = extractvalue %dx.types.ResRet.f32 %1923, 0
  %1925 = icmp ne i32 %1922, 0
  %1926 = select i1 %1925, float %1924, float 1.000000e+00
  %1927 = icmp eq i32 %1922, 0
  br i1 %1927, label %2004, label %1928

; <label>:1928                                    ; preds = %1912
  %1929 = add i32 %911, 1
  %1930 = icmp eq i32 %1929, %716
  %1931 = fsub fast float %1426, %740
  %1932 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1933 = extractvalue %dx.types.CBufRet.i32 %1932, 1
  %1934 = and i32 %1933, 255
  %1935 = uitofp i32 %1934 to float
  %1936 = fmul fast float %1935, 0x3F847AE140000000
  %1937 = lshr i32 %1933, 8
  %1938 = and i32 %1937, 255
  %1939 = uitofp i32 %1938 to float
  %1940 = fmul fast float %1939, 0x3F847AE140000000
  %1941 = fcmp fast ogt float %1940, %1936
  br i1 %1930, label %1942, label %1960

; <label>:1942                                    ; preds = %1928
  br i1 %1941, label %1943, label %1949

; <label>:1943                                    ; preds = %1942
  %1944 = fsub fast float %1931, %1936
  %1945 = fsub fast float %1939, %1935
  %1946 = fmul fast float %1945, 0x3F847AE140000000
  %1947 = fdiv fast float %1944, %1946
  %1948 = call float @dx.op.unary.f32(i32 7, float %1947)  ; Saturate(value)
  br label %1952

; <label>:1949                                    ; preds = %1942
  %1950 = fcmp fast oge float %1931, %1940
  %1951 = select i1 %1950, float 1.000000e+00, float 0.000000e+00
  br label %1952

; <label>:1952                                    ; preds = %1949, %1943
  %1953 = phi float [ %1948, %1943 ], [ %1951, %1949 ]
  %1954 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %1955 = extractvalue %dx.types.CBufRet.f32 %1954, 0
  %1956 = fmul fast float %1953, %1926
  %1957 = fsub fast float 1.000000e+00, %1953
  %1958 = fmul fast float %1955, %1957
  %1959 = fadd fast float %1958, %1956
  br label %2004

; <label>:1960                                    ; preds = %1928
  br i1 %1941, label %1961, label %1967

; <label>:1961                                    ; preds = %1960
  %1962 = fsub fast float %1931, %1936
  %1963 = fsub fast float %1939, %1935
  %1964 = fmul fast float %1963, 0x3F847AE140000000
  %1965 = fdiv fast float %1962, %1964
  %1966 = call float @dx.op.unary.f32(i32 7, float %1965)  ; Saturate(value)
  br label %1970

; <label>:1967                                    ; preds = %1960
  %1968 = fcmp fast oge float %1931, %1940
  %1969 = select i1 %1968, float 1.000000e+00, float 0.000000e+00
  br label %1970

; <label>:1970                                    ; preds = %1967, %1961
  %1971 = phi float [ %1966, %1961 ], [ %1969, %1967 ]
  %1972 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %1973 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %1974 = call float @dx.op.binary.f32(i32 36, float %1972, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1975 = call float @dx.op.binary.f32(i32 36, float %1973, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %1976 = add i32 %911, 121
  %1977 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1976)  ; CBufferLoadLegacy(handle,regIndex)
  %1978 = extractvalue %dx.types.CBufRet.f32 %1977, 0
  %1979 = extractvalue %dx.types.CBufRet.f32 %1977, 1
  %1980 = fadd fast float %1978, %1974
  %1981 = fadd fast float %1979, %1975
  %1982 = call float @dx.op.unary.f32(i32 22, float %1980)  ; Frc(value)
  %1983 = call float @dx.op.unary.f32(i32 22, float %1981)  ; Frc(value)
  %1984 = uitofp i32 %1929 to float
  %1985 = fmul fast float %1429, %1983
  %1986 = fmul fast float %1431, %1984
  %1987 = call float @dx.op.unary.f32(i32 27, float %1985)  ; Round_ni(value)
  %1988 = fadd fast float %1987, %1982
  %1989 = fmul fast float %1988, %1428
  %1990 = fadd fast float %1989, %1986
  %1991 = fptoui float %1990 to i32
  %1992 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1991, i32 undef)  ; BufferLoad(srv,index,wot)
  %1993 = extractvalue %dx.types.ResRet.i32 %1992, 0
  %1994 = and i32 %1993, 1
  %1995 = icmp eq i32 %1994, 0
  br i1 %1995, label %2004, label %1996

; <label>:1996                                    ; preds = %1970
  %1997 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %1998 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1982, float %1983, float %1984, float undef, i32 0, i32 0, i32 undef, float %1997)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %1999 = extractvalue %dx.types.ResRet.f32 %1998, 0
  %2000 = fmul fast float %1971, %1926
  %2001 = fsub fast float 1.000000e+00, %1971
  %2002 = fmul fast float %1999, %2001
  %2003 = fadd fast float %2002, %2000
  br label %2004

; <label>:2004                                    ; preds = %1996, %1970, %1952, %1912
  %2005 = phi float [ %1926, %1912 ], [ %1959, %1952 ], [ %2003, %1996 ], [ %1926, %1970 ]
  %2006 = select i1 %1925, float 1.000000e+00, float 0.000000e+00
  %2007 = fadd fast float %1915, %2006
  %2008 = fadd fast float %1823, %1627
  %2009 = fmul fast float %2008, %1428
  %2010 = fadd fast float %2009, %1432
  %2011 = fptoui float %2010 to i32
  %2012 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2011, i32 undef)  ; BufferLoad(srv,index,wot)
  %2013 = extractvalue %dx.types.ResRet.i32 %2012, 0
  %2014 = and i32 %2013, 1
  %2015 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1627, float %1821, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2016 = extractvalue %dx.types.ResRet.f32 %2015, 0
  %2017 = icmp ne i32 %2014, 0
  %2018 = select i1 %2017, float %2016, float 1.000000e+00
  %2019 = icmp eq i32 %2014, 0
  br i1 %2019, label %2096, label %2020

; <label>:2020                                    ; preds = %2004
  %2021 = add i32 %911, 1
  %2022 = icmp eq i32 %2021, %716
  %2023 = fsub fast float %1426, %740
  %2024 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2025 = extractvalue %dx.types.CBufRet.i32 %2024, 1
  %2026 = and i32 %2025, 255
  %2027 = uitofp i32 %2026 to float
  %2028 = fmul fast float %2027, 0x3F847AE140000000
  %2029 = lshr i32 %2025, 8
  %2030 = and i32 %2029, 255
  %2031 = uitofp i32 %2030 to float
  %2032 = fmul fast float %2031, 0x3F847AE140000000
  %2033 = fcmp fast ogt float %2032, %2028
  br i1 %2022, label %2034, label %2052

; <label>:2034                                    ; preds = %2020
  br i1 %2033, label %2035, label %2041

; <label>:2035                                    ; preds = %2034
  %2036 = fsub fast float %2023, %2028
  %2037 = fsub fast float %2031, %2027
  %2038 = fmul fast float %2037, 0x3F847AE140000000
  %2039 = fdiv fast float %2036, %2038
  %2040 = call float @dx.op.unary.f32(i32 7, float %2039)  ; Saturate(value)
  br label %2044

; <label>:2041                                    ; preds = %2034
  %2042 = fcmp fast oge float %2023, %2032
  %2043 = select i1 %2042, float 1.000000e+00, float 0.000000e+00
  br label %2044

; <label>:2044                                    ; preds = %2041, %2035
  %2045 = phi float [ %2040, %2035 ], [ %2043, %2041 ]
  %2046 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2047 = extractvalue %dx.types.CBufRet.f32 %2046, 0
  %2048 = fmul fast float %2045, %2018
  %2049 = fsub fast float 1.000000e+00, %2045
  %2050 = fmul fast float %2047, %2049
  %2051 = fadd fast float %2050, %2048
  br label %2096

; <label>:2052                                    ; preds = %2020
  br i1 %2033, label %2053, label %2059

; <label>:2053                                    ; preds = %2052
  %2054 = fsub fast float %2023, %2028
  %2055 = fsub fast float %2031, %2027
  %2056 = fmul fast float %2055, 0x3F847AE140000000
  %2057 = fdiv fast float %2054, %2056
  %2058 = call float @dx.op.unary.f32(i32 7, float %2057)  ; Saturate(value)
  br label %2062

; <label>:2059                                    ; preds = %2052
  %2060 = fcmp fast oge float %2023, %2032
  %2061 = select i1 %2060, float 1.000000e+00, float 0.000000e+00
  br label %2062

; <label>:2062                                    ; preds = %2059, %2053
  %2063 = phi float [ %2058, %2053 ], [ %2061, %2059 ]
  %2064 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2065 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2066 = call float @dx.op.binary.f32(i32 36, float %2064, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2067 = call float @dx.op.binary.f32(i32 36, float %2065, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2068 = add i32 %911, 121
  %2069 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2068)  ; CBufferLoadLegacy(handle,regIndex)
  %2070 = extractvalue %dx.types.CBufRet.f32 %2069, 0
  %2071 = extractvalue %dx.types.CBufRet.f32 %2069, 1
  %2072 = fadd fast float %2070, %2066
  %2073 = fadd fast float %2071, %2067
  %2074 = call float @dx.op.unary.f32(i32 22, float %2072)  ; Frc(value)
  %2075 = call float @dx.op.unary.f32(i32 22, float %2073)  ; Frc(value)
  %2076 = uitofp i32 %2021 to float
  %2077 = fmul fast float %1429, %2075
  %2078 = fmul fast float %1431, %2076
  %2079 = call float @dx.op.unary.f32(i32 27, float %2077)  ; Round_ni(value)
  %2080 = fadd fast float %2079, %2074
  %2081 = fmul fast float %2080, %1428
  %2082 = fadd fast float %2081, %2078
  %2083 = fptoui float %2082 to i32
  %2084 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2083, i32 undef)  ; BufferLoad(srv,index,wot)
  %2085 = extractvalue %dx.types.ResRet.i32 %2084, 0
  %2086 = and i32 %2085, 1
  %2087 = icmp eq i32 %2086, 0
  br i1 %2087, label %2096, label %2088

; <label>:2088                                    ; preds = %2062
  %2089 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2090 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2074, float %2075, float %2076, float undef, i32 0, i32 0, i32 undef, float %2089)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2091 = extractvalue %dx.types.ResRet.f32 %2090, 0
  %2092 = fmul fast float %2063, %2018
  %2093 = fsub fast float 1.000000e+00, %2063
  %2094 = fmul fast float %2091, %2093
  %2095 = fadd fast float %2094, %2092
  br label %2096

; <label>:2096                                    ; preds = %2088, %2062, %2044, %2004
  %2097 = phi float [ %2018, %2004 ], [ %2051, %2044 ], [ %2095, %2088 ], [ %2018, %2062 ]
  %2098 = select i1 %2017, float 1.000000e+00, float 0.000000e+00
  %2099 = fadd fast float %2007, %2098
  %2100 = fadd fast float %1823, %1724
  %2101 = fmul fast float %2100, %1428
  %2102 = fadd fast float %2101, %1432
  %2103 = fptoui float %2102 to i32
  %2104 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2103, i32 undef)  ; BufferLoad(srv,index,wot)
  %2105 = extractvalue %dx.types.ResRet.i32 %2104, 0
  %2106 = and i32 %2105, 1
  %2107 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1724, float %1821, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2108 = extractvalue %dx.types.ResRet.f32 %2107, 0
  %2109 = icmp ne i32 %2106, 0
  %2110 = select i1 %2109, float %2108, float 1.000000e+00
  %2111 = icmp eq i32 %2106, 0
  br i1 %2111, label %2188, label %2112

; <label>:2112                                    ; preds = %2096
  %2113 = add i32 %911, 1
  %2114 = icmp eq i32 %2113, %716
  %2115 = fsub fast float %1426, %740
  %2116 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2117 = extractvalue %dx.types.CBufRet.i32 %2116, 1
  %2118 = and i32 %2117, 255
  %2119 = uitofp i32 %2118 to float
  %2120 = fmul fast float %2119, 0x3F847AE140000000
  %2121 = lshr i32 %2117, 8
  %2122 = and i32 %2121, 255
  %2123 = uitofp i32 %2122 to float
  %2124 = fmul fast float %2123, 0x3F847AE140000000
  %2125 = fcmp fast ogt float %2124, %2120
  br i1 %2114, label %2126, label %2144

; <label>:2126                                    ; preds = %2112
  br i1 %2125, label %2127, label %2133

; <label>:2127                                    ; preds = %2126
  %2128 = fsub fast float %2115, %2120
  %2129 = fsub fast float %2123, %2119
  %2130 = fmul fast float %2129, 0x3F847AE140000000
  %2131 = fdiv fast float %2128, %2130
  %2132 = call float @dx.op.unary.f32(i32 7, float %2131)  ; Saturate(value)
  br label %2136

; <label>:2133                                    ; preds = %2126
  %2134 = fcmp fast oge float %2115, %2124
  %2135 = select i1 %2134, float 1.000000e+00, float 0.000000e+00
  br label %2136

; <label>:2136                                    ; preds = %2133, %2127
  %2137 = phi float [ %2132, %2127 ], [ %2135, %2133 ]
  %2138 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2139 = extractvalue %dx.types.CBufRet.f32 %2138, 0
  %2140 = fmul fast float %2137, %2110
  %2141 = fsub fast float 1.000000e+00, %2137
  %2142 = fmul fast float %2139, %2141
  %2143 = fadd fast float %2142, %2140
  br label %2188

; <label>:2144                                    ; preds = %2112
  br i1 %2125, label %2145, label %2151

; <label>:2145                                    ; preds = %2144
  %2146 = fsub fast float %2115, %2120
  %2147 = fsub fast float %2123, %2119
  %2148 = fmul fast float %2147, 0x3F847AE140000000
  %2149 = fdiv fast float %2146, %2148
  %2150 = call float @dx.op.unary.f32(i32 7, float %2149)  ; Saturate(value)
  br label %2154

; <label>:2151                                    ; preds = %2144
  %2152 = fcmp fast oge float %2115, %2124
  %2153 = select i1 %2152, float 1.000000e+00, float 0.000000e+00
  br label %2154

; <label>:2154                                    ; preds = %2151, %2145
  %2155 = phi float [ %2150, %2145 ], [ %2153, %2151 ]
  %2156 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2157 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2158 = call float @dx.op.binary.f32(i32 36, float %2156, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2159 = call float @dx.op.binary.f32(i32 36, float %2157, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2160 = add i32 %911, 121
  %2161 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2160)  ; CBufferLoadLegacy(handle,regIndex)
  %2162 = extractvalue %dx.types.CBufRet.f32 %2161, 0
  %2163 = extractvalue %dx.types.CBufRet.f32 %2161, 1
  %2164 = fadd fast float %2162, %2158
  %2165 = fadd fast float %2163, %2159
  %2166 = call float @dx.op.unary.f32(i32 22, float %2164)  ; Frc(value)
  %2167 = call float @dx.op.unary.f32(i32 22, float %2165)  ; Frc(value)
  %2168 = uitofp i32 %2113 to float
  %2169 = fmul fast float %1429, %2167
  %2170 = fmul fast float %1431, %2168
  %2171 = call float @dx.op.unary.f32(i32 27, float %2169)  ; Round_ni(value)
  %2172 = fadd fast float %2171, %2166
  %2173 = fmul fast float %2172, %1428
  %2174 = fadd fast float %2173, %2170
  %2175 = fptoui float %2174 to i32
  %2176 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2175, i32 undef)  ; BufferLoad(srv,index,wot)
  %2177 = extractvalue %dx.types.ResRet.i32 %2176, 0
  %2178 = and i32 %2177, 1
  %2179 = icmp eq i32 %2178, 0
  br i1 %2179, label %2188, label %2180

; <label>:2180                                    ; preds = %2154
  %2181 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2182 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2166, float %2167, float %2168, float undef, i32 0, i32 0, i32 undef, float %2181)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2183 = extractvalue %dx.types.ResRet.f32 %2182, 0
  %2184 = fmul fast float %2155, %2110
  %2185 = fsub fast float 1.000000e+00, %2155
  %2186 = fmul fast float %2183, %2185
  %2187 = fadd fast float %2186, %2184
  br label %2188

; <label>:2188                                    ; preds = %2180, %2154, %2136, %2096
  %2189 = phi float [ %2110, %2096 ], [ %2143, %2136 ], [ %2187, %2180 ], [ %2110, %2154 ]
  %2190 = select i1 %2109, float 1.000000e+00, float 0.000000e+00
  %2191 = fadd fast float %2099, %2190
  %2192 = fadd fast float %1817, %1397
  %2193 = call float @dx.op.binary.f32(i32 35, float %2192, float 0.000000e+00)  ; FMax(a,b)
  %2194 = call float @dx.op.binary.f32(i32 36, float %2193, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2195 = fadd fast float %1421, %2194
  %2196 = call float @dx.op.unary.f32(i32 22, float %2195)  ; Frc(value)
  %2197 = fmul fast float %1429, %2196
  %2198 = call float @dx.op.unary.f32(i32 27, float %2197)  ; Round_ni(value)
  %2199 = fadd fast float %2198, %1424
  %2200 = fmul fast float %2199, %1428
  %2201 = fadd fast float %2200, %1432
  %2202 = fptoui float %2201 to i32
  %2203 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2202, i32 undef)  ; BufferLoad(srv,index,wot)
  %2204 = extractvalue %dx.types.ResRet.i32 %2203, 0
  %2205 = and i32 %2204, 1
  %2206 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1424, float %2196, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2207 = extractvalue %dx.types.ResRet.f32 %2206, 0
  %2208 = icmp ne i32 %2205, 0
  %2209 = select i1 %2208, float %2207, float 1.000000e+00
  %2210 = icmp eq i32 %2205, 0
  br i1 %2210, label %2287, label %2211

; <label>:2211                                    ; preds = %2188
  %2212 = add i32 %911, 1
  %2213 = icmp eq i32 %2212, %716
  %2214 = fsub fast float %1426, %740
  %2215 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2216 = extractvalue %dx.types.CBufRet.i32 %2215, 1
  %2217 = and i32 %2216, 255
  %2218 = uitofp i32 %2217 to float
  %2219 = fmul fast float %2218, 0x3F847AE140000000
  %2220 = lshr i32 %2216, 8
  %2221 = and i32 %2220, 255
  %2222 = uitofp i32 %2221 to float
  %2223 = fmul fast float %2222, 0x3F847AE140000000
  %2224 = fcmp fast ogt float %2223, %2219
  br i1 %2213, label %2225, label %2243

; <label>:2225                                    ; preds = %2211
  br i1 %2224, label %2226, label %2232

; <label>:2226                                    ; preds = %2225
  %2227 = fsub fast float %2214, %2219
  %2228 = fsub fast float %2222, %2218
  %2229 = fmul fast float %2228, 0x3F847AE140000000
  %2230 = fdiv fast float %2227, %2229
  %2231 = call float @dx.op.unary.f32(i32 7, float %2230)  ; Saturate(value)
  br label %2235

; <label>:2232                                    ; preds = %2225
  %2233 = fcmp fast oge float %2214, %2223
  %2234 = select i1 %2233, float 1.000000e+00, float 0.000000e+00
  br label %2235

; <label>:2235                                    ; preds = %2232, %2226
  %2236 = phi float [ %2231, %2226 ], [ %2234, %2232 ]
  %2237 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2238 = extractvalue %dx.types.CBufRet.f32 %2237, 0
  %2239 = fmul fast float %2236, %2209
  %2240 = fsub fast float 1.000000e+00, %2236
  %2241 = fmul fast float %2238, %2240
  %2242 = fadd fast float %2241, %2239
  br label %2287

; <label>:2243                                    ; preds = %2211
  br i1 %2224, label %2244, label %2250

; <label>:2244                                    ; preds = %2243
  %2245 = fsub fast float %2214, %2219
  %2246 = fsub fast float %2222, %2218
  %2247 = fmul fast float %2246, 0x3F847AE140000000
  %2248 = fdiv fast float %2245, %2247
  %2249 = call float @dx.op.unary.f32(i32 7, float %2248)  ; Saturate(value)
  br label %2253

; <label>:2250                                    ; preds = %2243
  %2251 = fcmp fast oge float %2214, %2223
  %2252 = select i1 %2251, float 1.000000e+00, float 0.000000e+00
  br label %2253

; <label>:2253                                    ; preds = %2250, %2244
  %2254 = phi float [ %2249, %2244 ], [ %2252, %2250 ]
  %2255 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2256 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2257 = call float @dx.op.binary.f32(i32 36, float %2255, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2258 = call float @dx.op.binary.f32(i32 36, float %2256, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2259 = add i32 %911, 121
  %2260 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2259)  ; CBufferLoadLegacy(handle,regIndex)
  %2261 = extractvalue %dx.types.CBufRet.f32 %2260, 0
  %2262 = extractvalue %dx.types.CBufRet.f32 %2260, 1
  %2263 = fadd fast float %2261, %2257
  %2264 = fadd fast float %2262, %2258
  %2265 = call float @dx.op.unary.f32(i32 22, float %2263)  ; Frc(value)
  %2266 = call float @dx.op.unary.f32(i32 22, float %2264)  ; Frc(value)
  %2267 = uitofp i32 %2212 to float
  %2268 = fmul fast float %1429, %2266
  %2269 = fmul fast float %1431, %2267
  %2270 = call float @dx.op.unary.f32(i32 27, float %2268)  ; Round_ni(value)
  %2271 = fadd fast float %2270, %2265
  %2272 = fmul fast float %2271, %1428
  %2273 = fadd fast float %2272, %2269
  %2274 = fptoui float %2273 to i32
  %2275 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2274, i32 undef)  ; BufferLoad(srv,index,wot)
  %2276 = extractvalue %dx.types.ResRet.i32 %2275, 0
  %2277 = and i32 %2276, 1
  %2278 = icmp eq i32 %2277, 0
  br i1 %2278, label %2287, label %2279

; <label>:2279                                    ; preds = %2253
  %2280 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2281 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2265, float %2266, float %2267, float undef, i32 0, i32 0, i32 undef, float %2280)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2282 = extractvalue %dx.types.ResRet.f32 %2281, 0
  %2283 = fmul fast float %2254, %2209
  %2284 = fsub fast float 1.000000e+00, %2254
  %2285 = fmul fast float %2282, %2284
  %2286 = fadd fast float %2285, %2283
  br label %2287

; <label>:2287                                    ; preds = %2279, %2253, %2235, %2188
  %2288 = phi float [ %2209, %2188 ], [ %2242, %2235 ], [ %2286, %2279 ], [ %2209, %2253 ]
  %2289 = select i1 %2208, float 1.000000e+00, float 0.000000e+00
  %2290 = fadd fast float %2191, %2289
  %2291 = fadd fast float %2198, %1530
  %2292 = fmul fast float %2291, %1428
  %2293 = fadd fast float %2292, %1432
  %2294 = fptoui float %2293 to i32
  %2295 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2294, i32 undef)  ; BufferLoad(srv,index,wot)
  %2296 = extractvalue %dx.types.ResRet.i32 %2295, 0
  %2297 = and i32 %2296, 1
  %2298 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1530, float %2196, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2299 = extractvalue %dx.types.ResRet.f32 %2298, 0
  %2300 = icmp ne i32 %2297, 0
  %2301 = select i1 %2300, float %2299, float 1.000000e+00
  %2302 = icmp eq i32 %2297, 0
  br i1 %2302, label %2379, label %2303

; <label>:2303                                    ; preds = %2287
  %2304 = add i32 %911, 1
  %2305 = icmp eq i32 %2304, %716
  %2306 = fsub fast float %1426, %740
  %2307 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2308 = extractvalue %dx.types.CBufRet.i32 %2307, 1
  %2309 = and i32 %2308, 255
  %2310 = uitofp i32 %2309 to float
  %2311 = fmul fast float %2310, 0x3F847AE140000000
  %2312 = lshr i32 %2308, 8
  %2313 = and i32 %2312, 255
  %2314 = uitofp i32 %2313 to float
  %2315 = fmul fast float %2314, 0x3F847AE140000000
  %2316 = fcmp fast ogt float %2315, %2311
  br i1 %2305, label %2317, label %2335

; <label>:2317                                    ; preds = %2303
  br i1 %2316, label %2318, label %2324

; <label>:2318                                    ; preds = %2317
  %2319 = fsub fast float %2306, %2311
  %2320 = fsub fast float %2314, %2310
  %2321 = fmul fast float %2320, 0x3F847AE140000000
  %2322 = fdiv fast float %2319, %2321
  %2323 = call float @dx.op.unary.f32(i32 7, float %2322)  ; Saturate(value)
  br label %2327

; <label>:2324                                    ; preds = %2317
  %2325 = fcmp fast oge float %2306, %2315
  %2326 = select i1 %2325, float 1.000000e+00, float 0.000000e+00
  br label %2327

; <label>:2327                                    ; preds = %2324, %2318
  %2328 = phi float [ %2323, %2318 ], [ %2326, %2324 ]
  %2329 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2330 = extractvalue %dx.types.CBufRet.f32 %2329, 0
  %2331 = fmul fast float %2328, %2301
  %2332 = fsub fast float 1.000000e+00, %2328
  %2333 = fmul fast float %2330, %2332
  %2334 = fadd fast float %2333, %2331
  br label %2379

; <label>:2335                                    ; preds = %2303
  br i1 %2316, label %2336, label %2342

; <label>:2336                                    ; preds = %2335
  %2337 = fsub fast float %2306, %2311
  %2338 = fsub fast float %2314, %2310
  %2339 = fmul fast float %2338, 0x3F847AE140000000
  %2340 = fdiv fast float %2337, %2339
  %2341 = call float @dx.op.unary.f32(i32 7, float %2340)  ; Saturate(value)
  br label %2345

; <label>:2342                                    ; preds = %2335
  %2343 = fcmp fast oge float %2306, %2315
  %2344 = select i1 %2343, float 1.000000e+00, float 0.000000e+00
  br label %2345

; <label>:2345                                    ; preds = %2342, %2336
  %2346 = phi float [ %2341, %2336 ], [ %2344, %2342 ]
  %2347 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2348 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2349 = call float @dx.op.binary.f32(i32 36, float %2347, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2350 = call float @dx.op.binary.f32(i32 36, float %2348, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2351 = add i32 %911, 121
  %2352 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2351)  ; CBufferLoadLegacy(handle,regIndex)
  %2353 = extractvalue %dx.types.CBufRet.f32 %2352, 0
  %2354 = extractvalue %dx.types.CBufRet.f32 %2352, 1
  %2355 = fadd fast float %2353, %2349
  %2356 = fadd fast float %2354, %2350
  %2357 = call float @dx.op.unary.f32(i32 22, float %2355)  ; Frc(value)
  %2358 = call float @dx.op.unary.f32(i32 22, float %2356)  ; Frc(value)
  %2359 = uitofp i32 %2304 to float
  %2360 = fmul fast float %1429, %2358
  %2361 = fmul fast float %1431, %2359
  %2362 = call float @dx.op.unary.f32(i32 27, float %2360)  ; Round_ni(value)
  %2363 = fadd fast float %2362, %2357
  %2364 = fmul fast float %2363, %1428
  %2365 = fadd fast float %2364, %2361
  %2366 = fptoui float %2365 to i32
  %2367 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2366, i32 undef)  ; BufferLoad(srv,index,wot)
  %2368 = extractvalue %dx.types.ResRet.i32 %2367, 0
  %2369 = and i32 %2368, 1
  %2370 = icmp eq i32 %2369, 0
  br i1 %2370, label %2379, label %2371

; <label>:2371                                    ; preds = %2345
  %2372 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2373 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2357, float %2358, float %2359, float undef, i32 0, i32 0, i32 undef, float %2372)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2374 = extractvalue %dx.types.ResRet.f32 %2373, 0
  %2375 = fmul fast float %2346, %2301
  %2376 = fsub fast float 1.000000e+00, %2346
  %2377 = fmul fast float %2374, %2376
  %2378 = fadd fast float %2377, %2375
  br label %2379

; <label>:2379                                    ; preds = %2371, %2345, %2327, %2287
  %2380 = phi float [ %2301, %2287 ], [ %2334, %2327 ], [ %2378, %2371 ], [ %2301, %2345 ]
  %2381 = select i1 %2300, float 1.000000e+00, float 0.000000e+00
  %2382 = fadd fast float %2290, %2381
  %2383 = fadd fast float %2198, %1627
  %2384 = fmul fast float %2383, %1428
  %2385 = fadd fast float %2384, %1432
  %2386 = fptoui float %2385 to i32
  %2387 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2386, i32 undef)  ; BufferLoad(srv,index,wot)
  %2388 = extractvalue %dx.types.ResRet.i32 %2387, 0
  %2389 = and i32 %2388, 1
  %2390 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1627, float %2196, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2391 = extractvalue %dx.types.ResRet.f32 %2390, 0
  %2392 = icmp ne i32 %2389, 0
  %2393 = select i1 %2392, float %2391, float 1.000000e+00
  %2394 = icmp eq i32 %2389, 0
  br i1 %2394, label %2471, label %2395

; <label>:2395                                    ; preds = %2379
  %2396 = add i32 %911, 1
  %2397 = icmp eq i32 %2396, %716
  %2398 = fsub fast float %1426, %740
  %2399 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2400 = extractvalue %dx.types.CBufRet.i32 %2399, 1
  %2401 = and i32 %2400, 255
  %2402 = uitofp i32 %2401 to float
  %2403 = fmul fast float %2402, 0x3F847AE140000000
  %2404 = lshr i32 %2400, 8
  %2405 = and i32 %2404, 255
  %2406 = uitofp i32 %2405 to float
  %2407 = fmul fast float %2406, 0x3F847AE140000000
  %2408 = fcmp fast ogt float %2407, %2403
  br i1 %2397, label %2409, label %2427

; <label>:2409                                    ; preds = %2395
  br i1 %2408, label %2410, label %2416

; <label>:2410                                    ; preds = %2409
  %2411 = fsub fast float %2398, %2403
  %2412 = fsub fast float %2406, %2402
  %2413 = fmul fast float %2412, 0x3F847AE140000000
  %2414 = fdiv fast float %2411, %2413
  %2415 = call float @dx.op.unary.f32(i32 7, float %2414)  ; Saturate(value)
  br label %2419

; <label>:2416                                    ; preds = %2409
  %2417 = fcmp fast oge float %2398, %2407
  %2418 = select i1 %2417, float 1.000000e+00, float 0.000000e+00
  br label %2419

; <label>:2419                                    ; preds = %2416, %2410
  %2420 = phi float [ %2415, %2410 ], [ %2418, %2416 ]
  %2421 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2422 = extractvalue %dx.types.CBufRet.f32 %2421, 0
  %2423 = fmul fast float %2420, %2393
  %2424 = fsub fast float 1.000000e+00, %2420
  %2425 = fmul fast float %2422, %2424
  %2426 = fadd fast float %2425, %2423
  br label %2471

; <label>:2427                                    ; preds = %2395
  br i1 %2408, label %2428, label %2434

; <label>:2428                                    ; preds = %2427
  %2429 = fsub fast float %2398, %2403
  %2430 = fsub fast float %2406, %2402
  %2431 = fmul fast float %2430, 0x3F847AE140000000
  %2432 = fdiv fast float %2429, %2431
  %2433 = call float @dx.op.unary.f32(i32 7, float %2432)  ; Saturate(value)
  br label %2437

; <label>:2434                                    ; preds = %2427
  %2435 = fcmp fast oge float %2398, %2407
  %2436 = select i1 %2435, float 1.000000e+00, float 0.000000e+00
  br label %2437

; <label>:2437                                    ; preds = %2434, %2428
  %2438 = phi float [ %2433, %2428 ], [ %2436, %2434 ]
  %2439 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2440 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2441 = call float @dx.op.binary.f32(i32 36, float %2439, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2442 = call float @dx.op.binary.f32(i32 36, float %2440, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2443 = add i32 %911, 121
  %2444 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2443)  ; CBufferLoadLegacy(handle,regIndex)
  %2445 = extractvalue %dx.types.CBufRet.f32 %2444, 0
  %2446 = extractvalue %dx.types.CBufRet.f32 %2444, 1
  %2447 = fadd fast float %2445, %2441
  %2448 = fadd fast float %2446, %2442
  %2449 = call float @dx.op.unary.f32(i32 22, float %2447)  ; Frc(value)
  %2450 = call float @dx.op.unary.f32(i32 22, float %2448)  ; Frc(value)
  %2451 = uitofp i32 %2396 to float
  %2452 = fmul fast float %1429, %2450
  %2453 = fmul fast float %1431, %2451
  %2454 = call float @dx.op.unary.f32(i32 27, float %2452)  ; Round_ni(value)
  %2455 = fadd fast float %2454, %2449
  %2456 = fmul fast float %2455, %1428
  %2457 = fadd fast float %2456, %2453
  %2458 = fptoui float %2457 to i32
  %2459 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2458, i32 undef)  ; BufferLoad(srv,index,wot)
  %2460 = extractvalue %dx.types.ResRet.i32 %2459, 0
  %2461 = and i32 %2460, 1
  %2462 = icmp eq i32 %2461, 0
  br i1 %2462, label %2471, label %2463

; <label>:2463                                    ; preds = %2437
  %2464 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2465 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2449, float %2450, float %2451, float undef, i32 0, i32 0, i32 undef, float %2464)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2466 = extractvalue %dx.types.ResRet.f32 %2465, 0
  %2467 = fmul fast float %2438, %2393
  %2468 = fsub fast float 1.000000e+00, %2438
  %2469 = fmul fast float %2466, %2468
  %2470 = fadd fast float %2469, %2467
  br label %2471

; <label>:2471                                    ; preds = %2463, %2437, %2419, %2379
  %2472 = phi float [ %2393, %2379 ], [ %2426, %2419 ], [ %2470, %2463 ], [ %2393, %2437 ]
  %2473 = select i1 %2392, float 1.000000e+00, float 0.000000e+00
  %2474 = fadd fast float %2382, %2473
  %2475 = fadd fast float %2198, %1724
  %2476 = fmul fast float %2475, %1428
  %2477 = fadd fast float %2476, %1432
  %2478 = fptoui float %2477 to i32
  %2479 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2478, i32 undef)  ; BufferLoad(srv,index,wot)
  %2480 = extractvalue %dx.types.ResRet.i32 %2479, 0
  %2481 = and i32 %2480, 1
  %2482 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1724, float %2196, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2483 = extractvalue %dx.types.ResRet.f32 %2482, 0
  %2484 = icmp ne i32 %2481, 0
  %2485 = select i1 %2484, float %2483, float 1.000000e+00
  %2486 = icmp eq i32 %2481, 0
  br i1 %2486, label %2563, label %2487

; <label>:2487                                    ; preds = %2471
  %2488 = add i32 %911, 1
  %2489 = icmp eq i32 %2488, %716
  %2490 = fsub fast float %1426, %740
  %2491 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2492 = extractvalue %dx.types.CBufRet.i32 %2491, 1
  %2493 = and i32 %2492, 255
  %2494 = uitofp i32 %2493 to float
  %2495 = fmul fast float %2494, 0x3F847AE140000000
  %2496 = lshr i32 %2492, 8
  %2497 = and i32 %2496, 255
  %2498 = uitofp i32 %2497 to float
  %2499 = fmul fast float %2498, 0x3F847AE140000000
  %2500 = fcmp fast ogt float %2499, %2495
  br i1 %2489, label %2501, label %2519

; <label>:2501                                    ; preds = %2487
  br i1 %2500, label %2502, label %2508

; <label>:2502                                    ; preds = %2501
  %2503 = fsub fast float %2490, %2495
  %2504 = fsub fast float %2498, %2494
  %2505 = fmul fast float %2504, 0x3F847AE140000000
  %2506 = fdiv fast float %2503, %2505
  %2507 = call float @dx.op.unary.f32(i32 7, float %2506)  ; Saturate(value)
  br label %2511

; <label>:2508                                    ; preds = %2501
  %2509 = fcmp fast oge float %2490, %2499
  %2510 = select i1 %2509, float 1.000000e+00, float 0.000000e+00
  br label %2511

; <label>:2511                                    ; preds = %2508, %2502
  %2512 = phi float [ %2507, %2502 ], [ %2510, %2508 ]
  %2513 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2514 = extractvalue %dx.types.CBufRet.f32 %2513, 0
  %2515 = fmul fast float %2512, %2485
  %2516 = fsub fast float 1.000000e+00, %2512
  %2517 = fmul fast float %2514, %2516
  %2518 = fadd fast float %2517, %2515
  br label %2563

; <label>:2519                                    ; preds = %2487
  br i1 %2500, label %2520, label %2526

; <label>:2520                                    ; preds = %2519
  %2521 = fsub fast float %2490, %2495
  %2522 = fsub fast float %2498, %2494
  %2523 = fmul fast float %2522, 0x3F847AE140000000
  %2524 = fdiv fast float %2521, %2523
  %2525 = call float @dx.op.unary.f32(i32 7, float %2524)  ; Saturate(value)
  br label %2529

; <label>:2526                                    ; preds = %2519
  %2527 = fcmp fast oge float %2490, %2499
  %2528 = select i1 %2527, float 1.000000e+00, float 0.000000e+00
  br label %2529

; <label>:2529                                    ; preds = %2526, %2520
  %2530 = phi float [ %2525, %2520 ], [ %2528, %2526 ]
  %2531 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2532 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2533 = call float @dx.op.binary.f32(i32 36, float %2531, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2534 = call float @dx.op.binary.f32(i32 36, float %2532, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2535 = add i32 %911, 121
  %2536 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2535)  ; CBufferLoadLegacy(handle,regIndex)
  %2537 = extractvalue %dx.types.CBufRet.f32 %2536, 0
  %2538 = extractvalue %dx.types.CBufRet.f32 %2536, 1
  %2539 = fadd fast float %2537, %2533
  %2540 = fadd fast float %2538, %2534
  %2541 = call float @dx.op.unary.f32(i32 22, float %2539)  ; Frc(value)
  %2542 = call float @dx.op.unary.f32(i32 22, float %2540)  ; Frc(value)
  %2543 = uitofp i32 %2488 to float
  %2544 = fmul fast float %1429, %2542
  %2545 = fmul fast float %1431, %2543
  %2546 = call float @dx.op.unary.f32(i32 27, float %2544)  ; Round_ni(value)
  %2547 = fadd fast float %2546, %2541
  %2548 = fmul fast float %2547, %1428
  %2549 = fadd fast float %2548, %2545
  %2550 = fptoui float %2549 to i32
  %2551 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2550, i32 undef)  ; BufferLoad(srv,index,wot)
  %2552 = extractvalue %dx.types.ResRet.i32 %2551, 0
  %2553 = and i32 %2552, 1
  %2554 = icmp eq i32 %2553, 0
  br i1 %2554, label %2563, label %2555

; <label>:2555                                    ; preds = %2529
  %2556 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2557 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2541, float %2542, float %2543, float undef, i32 0, i32 0, i32 undef, float %2556)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2558 = extractvalue %dx.types.ResRet.f32 %2557, 0
  %2559 = fmul fast float %2530, %2485
  %2560 = fsub fast float 1.000000e+00, %2530
  %2561 = fmul fast float %2558, %2560
  %2562 = fadd fast float %2561, %2559
  br label %2563

; <label>:2563                                    ; preds = %2555, %2529, %2511, %2471
  %2564 = phi float [ %2485, %2471 ], [ %2518, %2511 ], [ %2562, %2555 ], [ %2485, %2529 ]
  %2565 = select i1 %2484, float 1.000000e+00, float 0.000000e+00
  %2566 = fadd fast float %2474, %2565
  %2567 = fadd fast float %2192, %1397
  %2568 = call float @dx.op.binary.f32(i32 35, float %2567, float 0.000000e+00)  ; FMax(a,b)
  %2569 = call float @dx.op.binary.f32(i32 36, float %2568, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2570 = fadd fast float %1421, %2569
  %2571 = call float @dx.op.unary.f32(i32 22, float %2570)  ; Frc(value)
  %2572 = fmul fast float %1429, %2571
  %2573 = call float @dx.op.unary.f32(i32 27, float %2572)  ; Round_ni(value)
  %2574 = fadd fast float %2573, %1424
  %2575 = fmul fast float %2574, %1428
  %2576 = fadd fast float %2575, %1432
  %2577 = fptoui float %2576 to i32
  %2578 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2577, i32 undef)  ; BufferLoad(srv,index,wot)
  %2579 = extractvalue %dx.types.ResRet.i32 %2578, 0
  %2580 = and i32 %2579, 1
  %2581 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1424, float %2571, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2582 = extractvalue %dx.types.ResRet.f32 %2581, 0
  %2583 = icmp ne i32 %2580, 0
  %2584 = select i1 %2583, float %2582, float 1.000000e+00
  %2585 = icmp eq i32 %2580, 0
  br i1 %2585, label %2662, label %2586

; <label>:2586                                    ; preds = %2563
  %2587 = add i32 %911, 1
  %2588 = icmp eq i32 %2587, %716
  %2589 = fsub fast float %1426, %740
  %2590 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2591 = extractvalue %dx.types.CBufRet.i32 %2590, 1
  %2592 = and i32 %2591, 255
  %2593 = uitofp i32 %2592 to float
  %2594 = fmul fast float %2593, 0x3F847AE140000000
  %2595 = lshr i32 %2591, 8
  %2596 = and i32 %2595, 255
  %2597 = uitofp i32 %2596 to float
  %2598 = fmul fast float %2597, 0x3F847AE140000000
  %2599 = fcmp fast ogt float %2598, %2594
  br i1 %2588, label %2600, label %2618

; <label>:2600                                    ; preds = %2586
  br i1 %2599, label %2601, label %2607

; <label>:2601                                    ; preds = %2600
  %2602 = fsub fast float %2589, %2594
  %2603 = fsub fast float %2597, %2593
  %2604 = fmul fast float %2603, 0x3F847AE140000000
  %2605 = fdiv fast float %2602, %2604
  %2606 = call float @dx.op.unary.f32(i32 7, float %2605)  ; Saturate(value)
  br label %2610

; <label>:2607                                    ; preds = %2600
  %2608 = fcmp fast oge float %2589, %2598
  %2609 = select i1 %2608, float 1.000000e+00, float 0.000000e+00
  br label %2610

; <label>:2610                                    ; preds = %2607, %2601
  %2611 = phi float [ %2606, %2601 ], [ %2609, %2607 ]
  %2612 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2613 = extractvalue %dx.types.CBufRet.f32 %2612, 0
  %2614 = fmul fast float %2611, %2584
  %2615 = fsub fast float 1.000000e+00, %2611
  %2616 = fmul fast float %2613, %2615
  %2617 = fadd fast float %2616, %2614
  br label %2662

; <label>:2618                                    ; preds = %2586
  br i1 %2599, label %2619, label %2625

; <label>:2619                                    ; preds = %2618
  %2620 = fsub fast float %2589, %2594
  %2621 = fsub fast float %2597, %2593
  %2622 = fmul fast float %2621, 0x3F847AE140000000
  %2623 = fdiv fast float %2620, %2622
  %2624 = call float @dx.op.unary.f32(i32 7, float %2623)  ; Saturate(value)
  br label %2628

; <label>:2625                                    ; preds = %2618
  %2626 = fcmp fast oge float %2589, %2598
  %2627 = select i1 %2626, float 1.000000e+00, float 0.000000e+00
  br label %2628

; <label>:2628                                    ; preds = %2625, %2619
  %2629 = phi float [ %2624, %2619 ], [ %2627, %2625 ]
  %2630 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2631 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2632 = call float @dx.op.binary.f32(i32 36, float %2630, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2633 = call float @dx.op.binary.f32(i32 36, float %2631, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2634 = add i32 %911, 121
  %2635 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2634)  ; CBufferLoadLegacy(handle,regIndex)
  %2636 = extractvalue %dx.types.CBufRet.f32 %2635, 0
  %2637 = extractvalue %dx.types.CBufRet.f32 %2635, 1
  %2638 = fadd fast float %2636, %2632
  %2639 = fadd fast float %2637, %2633
  %2640 = call float @dx.op.unary.f32(i32 22, float %2638)  ; Frc(value)
  %2641 = call float @dx.op.unary.f32(i32 22, float %2639)  ; Frc(value)
  %2642 = uitofp i32 %2587 to float
  %2643 = fmul fast float %1429, %2641
  %2644 = fmul fast float %1431, %2642
  %2645 = call float @dx.op.unary.f32(i32 27, float %2643)  ; Round_ni(value)
  %2646 = fadd fast float %2645, %2640
  %2647 = fmul fast float %2646, %1428
  %2648 = fadd fast float %2647, %2644
  %2649 = fptoui float %2648 to i32
  %2650 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2649, i32 undef)  ; BufferLoad(srv,index,wot)
  %2651 = extractvalue %dx.types.ResRet.i32 %2650, 0
  %2652 = and i32 %2651, 1
  %2653 = icmp eq i32 %2652, 0
  br i1 %2653, label %2662, label %2654

; <label>:2654                                    ; preds = %2628
  %2655 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2656 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2640, float %2641, float %2642, float undef, i32 0, i32 0, i32 undef, float %2655)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2657 = extractvalue %dx.types.ResRet.f32 %2656, 0
  %2658 = fmul fast float %2629, %2584
  %2659 = fsub fast float 1.000000e+00, %2629
  %2660 = fmul fast float %2657, %2659
  %2661 = fadd fast float %2660, %2658
  br label %2662

; <label>:2662                                    ; preds = %2654, %2628, %2610, %2563
  %2663 = phi float [ %2584, %2563 ], [ %2617, %2610 ], [ %2661, %2654 ], [ %2584, %2628 ]
  %2664 = select i1 %2583, float 1.000000e+00, float 0.000000e+00
  %2665 = fadd fast float %2566, %2664
  %2666 = fadd fast float %2573, %1530
  %2667 = fmul fast float %2666, %1428
  %2668 = fadd fast float %2667, %1432
  %2669 = fptoui float %2668 to i32
  %2670 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2669, i32 undef)  ; BufferLoad(srv,index,wot)
  %2671 = extractvalue %dx.types.ResRet.i32 %2670, 0
  %2672 = and i32 %2671, 1
  %2673 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1530, float %2571, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2674 = extractvalue %dx.types.ResRet.f32 %2673, 0
  %2675 = icmp ne i32 %2672, 0
  %2676 = select i1 %2675, float %2674, float 1.000000e+00
  %2677 = icmp eq i32 %2672, 0
  br i1 %2677, label %2754, label %2678

; <label>:2678                                    ; preds = %2662
  %2679 = add i32 %911, 1
  %2680 = icmp eq i32 %2679, %716
  %2681 = fsub fast float %1426, %740
  %2682 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2683 = extractvalue %dx.types.CBufRet.i32 %2682, 1
  %2684 = and i32 %2683, 255
  %2685 = uitofp i32 %2684 to float
  %2686 = fmul fast float %2685, 0x3F847AE140000000
  %2687 = lshr i32 %2683, 8
  %2688 = and i32 %2687, 255
  %2689 = uitofp i32 %2688 to float
  %2690 = fmul fast float %2689, 0x3F847AE140000000
  %2691 = fcmp fast ogt float %2690, %2686
  br i1 %2680, label %2692, label %2710

; <label>:2692                                    ; preds = %2678
  br i1 %2691, label %2693, label %2699

; <label>:2693                                    ; preds = %2692
  %2694 = fsub fast float %2681, %2686
  %2695 = fsub fast float %2689, %2685
  %2696 = fmul fast float %2695, 0x3F847AE140000000
  %2697 = fdiv fast float %2694, %2696
  %2698 = call float @dx.op.unary.f32(i32 7, float %2697)  ; Saturate(value)
  br label %2702

; <label>:2699                                    ; preds = %2692
  %2700 = fcmp fast oge float %2681, %2690
  %2701 = select i1 %2700, float 1.000000e+00, float 0.000000e+00
  br label %2702

; <label>:2702                                    ; preds = %2699, %2693
  %2703 = phi float [ %2698, %2693 ], [ %2701, %2699 ]
  %2704 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2705 = extractvalue %dx.types.CBufRet.f32 %2704, 0
  %2706 = fmul fast float %2703, %2676
  %2707 = fsub fast float 1.000000e+00, %2703
  %2708 = fmul fast float %2705, %2707
  %2709 = fadd fast float %2708, %2706
  br label %2754

; <label>:2710                                    ; preds = %2678
  br i1 %2691, label %2711, label %2717

; <label>:2711                                    ; preds = %2710
  %2712 = fsub fast float %2681, %2686
  %2713 = fsub fast float %2689, %2685
  %2714 = fmul fast float %2713, 0x3F847AE140000000
  %2715 = fdiv fast float %2712, %2714
  %2716 = call float @dx.op.unary.f32(i32 7, float %2715)  ; Saturate(value)
  br label %2720

; <label>:2717                                    ; preds = %2710
  %2718 = fcmp fast oge float %2681, %2690
  %2719 = select i1 %2718, float 1.000000e+00, float 0.000000e+00
  br label %2720

; <label>:2720                                    ; preds = %2717, %2711
  %2721 = phi float [ %2716, %2711 ], [ %2719, %2717 ]
  %2722 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2723 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2724 = call float @dx.op.binary.f32(i32 36, float %2722, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2725 = call float @dx.op.binary.f32(i32 36, float %2723, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2726 = add i32 %911, 121
  %2727 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2726)  ; CBufferLoadLegacy(handle,regIndex)
  %2728 = extractvalue %dx.types.CBufRet.f32 %2727, 0
  %2729 = extractvalue %dx.types.CBufRet.f32 %2727, 1
  %2730 = fadd fast float %2728, %2724
  %2731 = fadd fast float %2729, %2725
  %2732 = call float @dx.op.unary.f32(i32 22, float %2730)  ; Frc(value)
  %2733 = call float @dx.op.unary.f32(i32 22, float %2731)  ; Frc(value)
  %2734 = uitofp i32 %2679 to float
  %2735 = fmul fast float %1429, %2733
  %2736 = fmul fast float %1431, %2734
  %2737 = call float @dx.op.unary.f32(i32 27, float %2735)  ; Round_ni(value)
  %2738 = fadd fast float %2737, %2732
  %2739 = fmul fast float %2738, %1428
  %2740 = fadd fast float %2739, %2736
  %2741 = fptoui float %2740 to i32
  %2742 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2741, i32 undef)  ; BufferLoad(srv,index,wot)
  %2743 = extractvalue %dx.types.ResRet.i32 %2742, 0
  %2744 = and i32 %2743, 1
  %2745 = icmp eq i32 %2744, 0
  br i1 %2745, label %2754, label %2746

; <label>:2746                                    ; preds = %2720
  %2747 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2748 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2732, float %2733, float %2734, float undef, i32 0, i32 0, i32 undef, float %2747)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2749 = extractvalue %dx.types.ResRet.f32 %2748, 0
  %2750 = fmul fast float %2721, %2676
  %2751 = fsub fast float 1.000000e+00, %2721
  %2752 = fmul fast float %2749, %2751
  %2753 = fadd fast float %2752, %2750
  br label %2754

; <label>:2754                                    ; preds = %2746, %2720, %2702, %2662
  %2755 = phi float [ %2676, %2662 ], [ %2709, %2702 ], [ %2753, %2746 ], [ %2676, %2720 ]
  %2756 = select i1 %2675, float 1.000000e+00, float 0.000000e+00
  %2757 = fadd fast float %2665, %2756
  %2758 = fadd fast float %2573, %1627
  %2759 = fmul fast float %2758, %1428
  %2760 = fadd fast float %2759, %1432
  %2761 = fptoui float %2760 to i32
  %2762 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2761, i32 undef)  ; BufferLoad(srv,index,wot)
  %2763 = extractvalue %dx.types.ResRet.i32 %2762, 0
  %2764 = and i32 %2763, 1
  %2765 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1627, float %2571, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2766 = extractvalue %dx.types.ResRet.f32 %2765, 0
  %2767 = icmp ne i32 %2764, 0
  %2768 = select i1 %2767, float %2766, float 1.000000e+00
  %2769 = icmp eq i32 %2764, 0
  br i1 %2769, label %2846, label %2770

; <label>:2770                                    ; preds = %2754
  %2771 = add i32 %911, 1
  %2772 = icmp eq i32 %2771, %716
  %2773 = fsub fast float %1426, %740
  %2774 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2775 = extractvalue %dx.types.CBufRet.i32 %2774, 1
  %2776 = and i32 %2775, 255
  %2777 = uitofp i32 %2776 to float
  %2778 = fmul fast float %2777, 0x3F847AE140000000
  %2779 = lshr i32 %2775, 8
  %2780 = and i32 %2779, 255
  %2781 = uitofp i32 %2780 to float
  %2782 = fmul fast float %2781, 0x3F847AE140000000
  %2783 = fcmp fast ogt float %2782, %2778
  br i1 %2772, label %2784, label %2802

; <label>:2784                                    ; preds = %2770
  br i1 %2783, label %2785, label %2791

; <label>:2785                                    ; preds = %2784
  %2786 = fsub fast float %2773, %2778
  %2787 = fsub fast float %2781, %2777
  %2788 = fmul fast float %2787, 0x3F847AE140000000
  %2789 = fdiv fast float %2786, %2788
  %2790 = call float @dx.op.unary.f32(i32 7, float %2789)  ; Saturate(value)
  br label %2794

; <label>:2791                                    ; preds = %2784
  %2792 = fcmp fast oge float %2773, %2782
  %2793 = select i1 %2792, float 1.000000e+00, float 0.000000e+00
  br label %2794

; <label>:2794                                    ; preds = %2791, %2785
  %2795 = phi float [ %2790, %2785 ], [ %2793, %2791 ]
  %2796 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2797 = extractvalue %dx.types.CBufRet.f32 %2796, 0
  %2798 = fmul fast float %2795, %2768
  %2799 = fsub fast float 1.000000e+00, %2795
  %2800 = fmul fast float %2797, %2799
  %2801 = fadd fast float %2800, %2798
  br label %2846

; <label>:2802                                    ; preds = %2770
  br i1 %2783, label %2803, label %2809

; <label>:2803                                    ; preds = %2802
  %2804 = fsub fast float %2773, %2778
  %2805 = fsub fast float %2781, %2777
  %2806 = fmul fast float %2805, 0x3F847AE140000000
  %2807 = fdiv fast float %2804, %2806
  %2808 = call float @dx.op.unary.f32(i32 7, float %2807)  ; Saturate(value)
  br label %2812

; <label>:2809                                    ; preds = %2802
  %2810 = fcmp fast oge float %2773, %2782
  %2811 = select i1 %2810, float 1.000000e+00, float 0.000000e+00
  br label %2812

; <label>:2812                                    ; preds = %2809, %2803
  %2813 = phi float [ %2808, %2803 ], [ %2811, %2809 ]
  %2814 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2815 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2816 = call float @dx.op.binary.f32(i32 36, float %2814, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2817 = call float @dx.op.binary.f32(i32 36, float %2815, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2818 = add i32 %911, 121
  %2819 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2818)  ; CBufferLoadLegacy(handle,regIndex)
  %2820 = extractvalue %dx.types.CBufRet.f32 %2819, 0
  %2821 = extractvalue %dx.types.CBufRet.f32 %2819, 1
  %2822 = fadd fast float %2820, %2816
  %2823 = fadd fast float %2821, %2817
  %2824 = call float @dx.op.unary.f32(i32 22, float %2822)  ; Frc(value)
  %2825 = call float @dx.op.unary.f32(i32 22, float %2823)  ; Frc(value)
  %2826 = uitofp i32 %2771 to float
  %2827 = fmul fast float %1429, %2825
  %2828 = fmul fast float %1431, %2826
  %2829 = call float @dx.op.unary.f32(i32 27, float %2827)  ; Round_ni(value)
  %2830 = fadd fast float %2829, %2824
  %2831 = fmul fast float %2830, %1428
  %2832 = fadd fast float %2831, %2828
  %2833 = fptoui float %2832 to i32
  %2834 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2833, i32 undef)  ; BufferLoad(srv,index,wot)
  %2835 = extractvalue %dx.types.ResRet.i32 %2834, 0
  %2836 = and i32 %2835, 1
  %2837 = icmp eq i32 %2836, 0
  br i1 %2837, label %2846, label %2838

; <label>:2838                                    ; preds = %2812
  %2839 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2840 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2824, float %2825, float %2826, float undef, i32 0, i32 0, i32 undef, float %2839)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2841 = extractvalue %dx.types.ResRet.f32 %2840, 0
  %2842 = fmul fast float %2813, %2768
  %2843 = fsub fast float 1.000000e+00, %2813
  %2844 = fmul fast float %2841, %2843
  %2845 = fadd fast float %2844, %2842
  br label %2846

; <label>:2846                                    ; preds = %2838, %2812, %2794, %2754
  %2847 = phi float [ %2768, %2754 ], [ %2801, %2794 ], [ %2845, %2838 ], [ %2768, %2812 ]
  %2848 = select i1 %2767, float 1.000000e+00, float 0.000000e+00
  %2849 = fadd fast float %2757, %2848
  %2850 = fadd fast float %2573, %1724
  %2851 = fmul fast float %2850, %1428
  %2852 = fadd fast float %2851, %1432
  %2853 = fptoui float %2852 to i32
  %2854 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2853, i32 undef)  ; BufferLoad(srv,index,wot)
  %2855 = extractvalue %dx.types.ResRet.i32 %2854, 0
  %2856 = and i32 %2855, 1
  %2857 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1724, float %2571, float %1426, float undef, i32 0, i32 0, i32 undef, float %1438)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2858 = extractvalue %dx.types.ResRet.f32 %2857, 0
  %2859 = icmp ne i32 %2856, 0
  %2860 = select i1 %2859, float %2858, float 1.000000e+00
  %2861 = icmp eq i32 %2856, 0
  br i1 %2861, label %2938, label %2862

; <label>:2862                                    ; preds = %2846
  %2863 = add i32 %911, 1
  %2864 = icmp eq i32 %2863, %716
  %2865 = fsub fast float %1426, %740
  %2866 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2867 = extractvalue %dx.types.CBufRet.i32 %2866, 1
  %2868 = and i32 %2867, 255
  %2869 = uitofp i32 %2868 to float
  %2870 = fmul fast float %2869, 0x3F847AE140000000
  %2871 = lshr i32 %2867, 8
  %2872 = and i32 %2871, 255
  %2873 = uitofp i32 %2872 to float
  %2874 = fmul fast float %2873, 0x3F847AE140000000
  %2875 = fcmp fast ogt float %2874, %2870
  br i1 %2864, label %2876, label %2894

; <label>:2876                                    ; preds = %2862
  br i1 %2875, label %2877, label %2883

; <label>:2877                                    ; preds = %2876
  %2878 = fsub fast float %2865, %2870
  %2879 = fsub fast float %2873, %2869
  %2880 = fmul fast float %2879, 0x3F847AE140000000
  %2881 = fdiv fast float %2878, %2880
  %2882 = call float @dx.op.unary.f32(i32 7, float %2881)  ; Saturate(value)
  br label %2886

; <label>:2883                                    ; preds = %2876
  %2884 = fcmp fast oge float %2865, %2874
  %2885 = select i1 %2884, float 1.000000e+00, float 0.000000e+00
  br label %2886

; <label>:2886                                    ; preds = %2883, %2877
  %2887 = phi float [ %2882, %2877 ], [ %2885, %2883 ]
  %2888 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %2889 = extractvalue %dx.types.CBufRet.f32 %2888, 0
  %2890 = fmul fast float %2887, %2860
  %2891 = fsub fast float 1.000000e+00, %2887
  %2892 = fmul fast float %2889, %2891
  %2893 = fadd fast float %2892, %2890
  br label %2938

; <label>:2894                                    ; preds = %2862
  br i1 %2875, label %2895, label %2901

; <label>:2895                                    ; preds = %2894
  %2896 = fsub fast float %2865, %2870
  %2897 = fsub fast float %2873, %2869
  %2898 = fmul fast float %2897, 0x3F847AE140000000
  %2899 = fdiv fast float %2896, %2898
  %2900 = call float @dx.op.unary.f32(i32 7, float %2899)  ; Saturate(value)
  br label %2904

; <label>:2901                                    ; preds = %2894
  %2902 = fcmp fast oge float %2865, %2874
  %2903 = select i1 %2902, float 1.000000e+00, float 0.000000e+00
  br label %2904

; <label>:2904                                    ; preds = %2901, %2895
  %2905 = phi float [ %2900, %2895 ], [ %2903, %2901 ]
  %2906 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %2907 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %2908 = call float @dx.op.binary.f32(i32 36, float %2906, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2909 = call float @dx.op.binary.f32(i32 36, float %2907, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2910 = add i32 %911, 121
  %2911 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2910)  ; CBufferLoadLegacy(handle,regIndex)
  %2912 = extractvalue %dx.types.CBufRet.f32 %2911, 0
  %2913 = extractvalue %dx.types.CBufRet.f32 %2911, 1
  %2914 = fadd fast float %2912, %2908
  %2915 = fadd fast float %2913, %2909
  %2916 = call float @dx.op.unary.f32(i32 22, float %2914)  ; Frc(value)
  %2917 = call float @dx.op.unary.f32(i32 22, float %2915)  ; Frc(value)
  %2918 = uitofp i32 %2863 to float
  %2919 = fmul fast float %1429, %2917
  %2920 = fmul fast float %1431, %2918
  %2921 = call float @dx.op.unary.f32(i32 27, float %2919)  ; Round_ni(value)
  %2922 = fadd fast float %2921, %2916
  %2923 = fmul fast float %2922, %1428
  %2924 = fadd fast float %2923, %2920
  %2925 = fptoui float %2924 to i32
  %2926 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2925, i32 undef)  ; BufferLoad(srv,index,wot)
  %2927 = extractvalue %dx.types.ResRet.i32 %2926, 0
  %2928 = and i32 %2927, 1
  %2929 = icmp eq i32 %2928, 0
  br i1 %2929, label %2938, label %2930

; <label>:2930                                    ; preds = %2904
  %2931 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %2932 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2916, float %2917, float %2918, float undef, i32 0, i32 0, i32 undef, float %2931)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2933 = extractvalue %dx.types.ResRet.f32 %2932, 0
  %2934 = fmul fast float %2905, %2860
  %2935 = fsub fast float 1.000000e+00, %2905
  %2936 = fmul fast float %2933, %2935
  %2937 = fadd fast float %2936, %2934
  br label %2938

; <label>:2938                                    ; preds = %2930, %2904, %2886, %2846
  %2939 = phi float [ %2860, %2846 ], [ %2893, %2886 ], [ %2937, %2930 ], [ %2860, %2904 ]
  %2940 = select i1 %2859, float 1.000000e+00, float 0.000000e+00
  %2941 = fadd fast float %2849, %2940
  %2942 = fcmp fast une float %2941, 0.000000e+00
  br i1 %2942, label %2943, label %2962

; <label>:2943                                    ; preds = %2938
  %2944 = fadd fast float %1524, -1.600000e+01
  %2945 = fadd fast float %2944, %1620
  %2946 = fadd fast float %2945, %1717
  %2947 = fadd fast float %2946, %1814
  %2948 = fadd fast float %2947, %1913
  %2949 = fadd fast float %2948, %2005
  %2950 = fadd fast float %2949, %2097
  %2951 = fadd fast float %2950, %2189
  %2952 = fadd fast float %2951, %2288
  %2953 = fadd fast float %2952, %2380
  %2954 = fadd fast float %2953, %2472
  %2955 = fadd fast float %2954, %2564
  %2956 = fadd fast float %2955, %2663
  %2957 = fadd fast float %2956, %2755
  %2958 = fadd fast float %2957, %2847
  %2959 = fadd fast float %2958, %2941
  %2960 = fadd fast float %2959, %2939
  %2961 = fdiv fast float %2960, %2941
  br label %2962

; <label>:2962                                    ; preds = %2943, %2938
  %2963 = phi float [ %2961, %2943 ], [ 1.000000e+00, %2938 ]
  %2964 = fsub fast float 1.000000e+00, %2963
  br label %3078

; <label>:2965                                    ; preds = %1390
  %2966 = call float @dx.op.binary.f32(i32 35, float %908, float 0.000000e+00)  ; FMax(a,b)
  %2967 = call float @dx.op.binary.f32(i32 35, float %909, float 0.000000e+00)  ; FMax(a,b)
  %2968 = call float @dx.op.binary.f32(i32 36, float %2966, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2969 = call float @dx.op.binary.f32(i32 36, float %2967, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2970 = add i32 %911, 120
  %2971 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2970)  ; CBufferLoadLegacy(handle,regIndex)
  %2972 = extractvalue %dx.types.CBufRet.f32 %2971, 0
  %2973 = extractvalue %dx.types.CBufRet.f32 %2971, 1
  %2974 = fadd fast float %2972, %2968
  %2975 = fadd fast float %2973, %2969
  %2976 = call float @dx.op.unary.f32(i32 22, float %2974)  ; Frc(value)
  %2977 = call float @dx.op.unary.f32(i32 22, float %2975)  ; Frc(value)
  %2978 = uitofp i32 %911 to float
  %2979 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)  ; CBufferLoadLegacy(handle,regIndex)
  %2980 = extractvalue %dx.types.CBufRet.f32 %2979, 0
  %2981 = extractvalue %dx.types.CBufRet.f32 %2979, 1
  %2982 = fmul fast float %2981, %2977
  %2983 = extractvalue %dx.types.CBufRet.f32 %2979, 2
  %2984 = fmul fast float %2983, %2978
  %2985 = call float @dx.op.unary.f32(i32 27, float %2982)  ; Round_ni(value)
  %2986 = fadd fast float %2985, %2976
  %2987 = fmul fast float %2986, %2980
  %2988 = fadd fast float %2987, %2984
  %2989 = fptoui float %2988 to i32
  %2990 = call float @dx.op.unary.f32(i32 7, float %910)  ; Saturate(value)
  %2991 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2989, i32 undef)  ; BufferLoad(srv,index,wot)
  %2992 = extractvalue %dx.types.ResRet.i32 %2991, 0
  %2993 = and i32 %2992, 1
  %2994 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2976, float %2977, float %2978, float undef, i32 0, i32 0, i32 undef, float %2990)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %2995 = extractvalue %dx.types.ResRet.f32 %2994, 0
  %2996 = icmp ne i32 %2993, 0
  %2997 = select i1 %2996, float %2995, float 1.000000e+00
  %2998 = icmp eq i32 %2993, 0
  br i1 %2998, label %3075, label %2999

; <label>:2999                                    ; preds = %2965
  %3000 = add i32 %911, 1
  %3001 = icmp eq i32 %3000, %716
  %3002 = fsub fast float %2978, %740
  %3003 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %3004 = extractvalue %dx.types.CBufRet.i32 %3003, 1
  %3005 = and i32 %3004, 255
  %3006 = uitofp i32 %3005 to float
  %3007 = fmul fast float %3006, 0x3F847AE140000000
  %3008 = lshr i32 %3004, 8
  %3009 = and i32 %3008, 255
  %3010 = uitofp i32 %3009 to float
  %3011 = fmul fast float %3010, 0x3F847AE140000000
  %3012 = fcmp fast ogt float %3011, %3007
  br i1 %3001, label %3013, label %3031

; <label>:3013                                    ; preds = %2999
  br i1 %3012, label %3014, label %3020

; <label>:3014                                    ; preds = %3013
  %3015 = fsub fast float %3002, %3007
  %3016 = fsub fast float %3010, %3006
  %3017 = fmul fast float %3016, 0x3F847AE140000000
  %3018 = fdiv fast float %3015, %3017
  %3019 = call float @dx.op.unary.f32(i32 7, float %3018)  ; Saturate(value)
  br label %3023

; <label>:3020                                    ; preds = %3013
  %3021 = fcmp fast oge float %3002, %3011
  %3022 = select i1 %3021, float 1.000000e+00, float 0.000000e+00
  br label %3023

; <label>:3023                                    ; preds = %3020, %3014
  %3024 = phi float [ %3019, %3014 ], [ %3022, %3020 ]
  %3025 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)  ; CBufferLoadLegacy(handle,regIndex)
  %3026 = extractvalue %dx.types.CBufRet.f32 %3025, 0
  %3027 = fmul fast float %3024, %2997
  %3028 = fsub fast float 1.000000e+00, %3024
  %3029 = fmul fast float %3026, %3028
  %3030 = fadd fast float %3029, %3027
  br label %3075

; <label>:3031                                    ; preds = %2999
  br i1 %3012, label %3032, label %3038

; <label>:3032                                    ; preds = %3031
  %3033 = fsub fast float %3002, %3007
  %3034 = fsub fast float %3010, %3006
  %3035 = fmul fast float %3034, 0x3F847AE140000000
  %3036 = fdiv fast float %3033, %3035
  %3037 = call float @dx.op.unary.f32(i32 7, float %3036)  ; Saturate(value)
  br label %3041

; <label>:3038                                    ; preds = %3031
  %3039 = fcmp fast oge float %3002, %3011
  %3040 = select i1 %3039, float 1.000000e+00, float 0.000000e+00
  br label %3041

; <label>:3041                                    ; preds = %3038, %3032
  %3042 = phi float [ %3037, %3032 ], [ %3040, %3038 ]
  %3043 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)  ; FMax(a,b)
  %3044 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)  ; FMax(a,b)
  %3045 = call float @dx.op.binary.f32(i32 36, float %3043, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %3046 = call float @dx.op.binary.f32(i32 36, float %3044, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %3047 = add i32 %911, 121
  %3048 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %3047)  ; CBufferLoadLegacy(handle,regIndex)
  %3049 = extractvalue %dx.types.CBufRet.f32 %3048, 0
  %3050 = extractvalue %dx.types.CBufRet.f32 %3048, 1
  %3051 = fadd fast float %3049, %3045
  %3052 = fadd fast float %3050, %3046
  %3053 = call float @dx.op.unary.f32(i32 22, float %3051)  ; Frc(value)
  %3054 = call float @dx.op.unary.f32(i32 22, float %3052)  ; Frc(value)
  %3055 = uitofp i32 %3000 to float
  %3056 = fmul fast float %2981, %3054
  %3057 = fmul fast float %2983, %3055
  %3058 = call float @dx.op.unary.f32(i32 27, float %3056)  ; Round_ni(value)
  %3059 = fadd fast float %3058, %3053
  %3060 = fmul fast float %3059, %2980
  %3061 = fadd fast float %3060, %3057
  %3062 = fptoui float %3061 to i32
  %3063 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3062, i32 undef)  ; BufferLoad(srv,index,wot)
  %3064 = extractvalue %dx.types.ResRet.i32 %3063, 0
  %3065 = and i32 %3064, 1
  %3066 = icmp eq i32 %3065, 0
  br i1 %3066, label %3075, label %3067

; <label>:3067                                    ; preds = %3041
  %3068 = call float @dx.op.unary.f32(i32 7, float %907)  ; Saturate(value)
  %3069 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %3053, float %3054, float %3055, float undef, i32 0, i32 0, i32 undef, float %3068)  ; SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  %3070 = extractvalue %dx.types.ResRet.f32 %3069, 0
  %3071 = fmul fast float %3042, %2997
  %3072 = fsub fast float 1.000000e+00, %3042
  %3073 = fmul fast float %3070, %3072
  %3074 = fadd fast float %3073, %3071
  br label %3075

; <label>:3075                                    ; preds = %3067, %3041, %3023, %2965
  %3076 = phi float [ %2997, %2965 ], [ %3030, %3023 ], [ %3074, %3067 ], [ %2997, %3041 ]
  %3077 = fsub fast float 1.000000e+00, %3076
  br label %3078

; <label>:3078                                    ; preds = %3075, %2962, %1394, %1381, %961
  %3079 = phi float [ %3077, %3075 ], [ %1389, %1381 ], [ 0.000000e+00, %961 ], [ %2964, %2962 ], [ 0.000000e+00, %1394 ]
  %3080 = fmul fast float %518, %3079
  %3081 = call float @dx.op.binary.f32(i32 35, float %714, float %3080)  ; FMax(a,b)
  br label %3082

; <label>:3082                                    ; preds = %3078, %953, %944, %913, %718, %713
  %3083 = phi float [ %714, %718 ], [ %918, %913 ], [ %952, %944 ], [ %714, %953 ], [ %3081, %3078 ], [ %714, %713 ]
  %3084 = fsub fast float 1.000000e+00, %3083
  br label %3085

; <label>:3085                                    ; preds = %3082, %0
  %3086 = phi float [ %3084, %3082 ], [ 1.000000e+00, %0 ]
  %3087 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, %dx.types.Handle %8, %dx.types.Handle %12, float %127, float %131, float undef, float undef, i32 0, i32 0, i32 undef, float undef)  ; Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  %3088 = extractvalue %dx.types.ResRet.f32 %3087, 0
  %3089 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %3090 = extractvalue %dx.types.CBufRet.f32 %3089, 0
  %3091 = fadd fast float %3088, -1.000000e+00
  %3092 = fmul fast float %3090, %3091
  %3093 = fadd fast float %3092, 1.000000e+00
  %3094 = call float @dx.op.binary.f32(i32 36, float %3093, float %3086)  ; FMin(a,b)
  %3095 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %3096 = extractvalue %dx.types.CBufRet.f32 %3095, 0
  %3097 = extractvalue %dx.types.CBufRet.f32 %3095, 1
  %3098 = extractvalue %dx.types.CBufRet.f32 %3095, 2
  %3099 = fsub fast float -0.000000e+00, %3096
  %3100 = fsub fast float -0.000000e+00, %3097
  %3101 = fsub fast float -0.000000e+00, %3098
  %3102 = call float @dx.op.dot3.f32(i32 55, float %105, float %108, float %111, float %3099, float %3100, float %3101)  ; Dot3(ax,ay,az,bx,by,bz)
  %3103 = call float @dx.op.unary.f32(i32 23, float %26)  ; Log(value)
  %3104 = fmul fast float %3103, 2.000000e+00
  %3105 = call float @dx.op.unary.f32(i32 21, float %3104)  ; Exp(value)
  %3106 = fmul fast float %3105, 2.000000e+00
  %3107 = call float @dx.op.unary.f32(i32 6, float %3102)  ; FAbs(value)
  %3108 = fadd fast float %3106, -1.000000e+00
  %3109 = fadd fast float %3108, %3107
  %3110 = call float @dx.op.unary.f32(i32 7, float %3109)  ; Saturate(value)
  %3111 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 147)  ; CBufferLoadLegacy(handle,regIndex)
  %3112 = extractvalue %dx.types.CBufRet.f32 %3111, 1
  %3113 = fadd fast float %3110, -1.000000e+00
  %3114 = fmul fast float %3112, %3113
  %3115 = fadd fast float %3114, 1.000000e+00
  %3116 = fmul fast float %3115, %3094
  %3117 = extractvalue %dx.types.CBufRet.f32 %3111, 0
  %3118 = fadd fast float %3116, -1.000000e+00
  %3119 = fmul fast float %3118, %3117
  %3120 = fadd fast float %3119, 1.000000e+00
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %3120)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %3120)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %3120)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %3120)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sample.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.viewIdState = !{!22}
!dx.entryPoints = !{!23}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 5}
!2 = !{!"ps", i32 6, i32 5}
!3 = !{!4, null, !15, !17}
!4 = !{!5, !7, !8, !9, !11, !12, !13, !14}
!5 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i32 0, !6}
!6 = !{i32 0, i32 9}
!7 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !6}
!8 = !{i32 2, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 7, i32 0, !6}
!9 = !{i32 3, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 16, i32 1, i32 10, i32 0, !10}
!10 = !{i32 0, i32 5}
!11 = !{i32 4, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 3, i32 1, i32 7, i32 0, !6}
!12 = !{i32 5, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 6, i32 1, i32 2, i32 0, !6}
!13 = !{i32 6, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 5, i32 1, i32 2, i32 0, !6}
!14 = !{i32 7, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 4, i32 1, i32 2, i32 0, !6}
!15 = !{!16}
!16 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 2360, null}
!17 = !{!18, !19, !20, !21}
!18 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 1, i32 1, i32 0, null}
!19 = !{i32 1, %struct.SamplerComparisonState* undef, !"", i32 0, i32 0, i32 1, i32 1, null}
!20 = !{i32 2, %struct.SamplerComparisonState* undef, !"", i32 0, i32 2, i32 1, i32 1, null}
!21 = !{i32 3, %struct.SamplerComparisonState* undef, !"", i32 0, i32 3, i32 1, i32 1, null}
!22 = !{[10 x i32] [i32 8, i32 4, i32 15, i32 15, i32 0, i32 0, i32 15, i32 15, i32 0, i32 0]}
!23 = !{void ()* @main, !"main", !24, !3, null}
!24 = !{!25, !30, null}
!25 = !{!26, !29}
!26 = !{i32 0, !"SV_Position", i8 9, i8 3, !27, i8 4, i32 1, i8 4, i32 0, i8 0, !28}
!27 = !{i32 0}
!28 = !{i32 3, i32 3}
!29 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !27, i8 2, i32 1, i8 4, i32 1, i8 0, !28}
!30 = !{!31}
!31 = !{i32 0, !"SV_Target", i8 9, i8 16, !27, i8 0, i32 1, i8 4, i32 0, i8 0, !32}
!32 = !{i32 3, i32 15}

