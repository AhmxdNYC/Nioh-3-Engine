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
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 4, i1 false)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 5, i1 false)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 6, i1 false)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 3, i1 false)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 16, i1 false)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 2, i1 false)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 0, i1 false)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 1, i1 false)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 3, i1 false)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 2, i1 false)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 0, i1 false)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 1, i1 false)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %17 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %18 = fadd fast float %16, -5.000000e-01
  %19 = fadd fast float %17, -5.000000e-01
  %20 = fptosi float %16 to i32
  %21 = fptosi float %17 to i32
  %22 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)
  %23 = extractvalue %dx.types.ResRet.f32 %22, 0
  %24 = extractvalue %dx.types.ResRet.f32 %22, 1
  %25 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %3, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)
  %26 = extractvalue %dx.types.ResRet.f32 %25, 3
  %27 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %1, i32 0, i32 %20, i32 %21, i32 undef, i32 undef, i32 undef, i32 undef)
  %28 = extractvalue %dx.types.ResRet.f32 %27, 0
  %29 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 143)
  %30 = extractvalue %dx.types.CBufRet.f32 %29, 0
  %31 = extractvalue %dx.types.CBufRet.f32 %29, 1
  %32 = extractvalue %dx.types.CBufRet.f32 %29, 2
  %33 = extractvalue %dx.types.CBufRet.f32 %29, 3
  %34 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 144)
  %35 = extractvalue %dx.types.CBufRet.f32 %34, 0
  %36 = extractvalue %dx.types.CBufRet.f32 %34, 1
  %37 = extractvalue %dx.types.CBufRet.f32 %34, 2
  %38 = extractvalue %dx.types.CBufRet.f32 %34, 3
  %39 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 145)
  %40 = extractvalue %dx.types.CBufRet.f32 %39, 0
  %41 = extractvalue %dx.types.CBufRet.f32 %39, 1
  %42 = extractvalue %dx.types.CBufRet.f32 %39, 2
  %43 = extractvalue %dx.types.CBufRet.f32 %39, 3
  %44 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 146)
  %45 = extractvalue %dx.types.CBufRet.f32 %44, 0
  %46 = extractvalue %dx.types.CBufRet.f32 %44, 1
  %47 = extractvalue %dx.types.CBufRet.f32 %44, 2
  %48 = extractvalue %dx.types.CBufRet.f32 %44, 3
  %49 = fmul fast float %30, %14
  %50 = call float @dx.op.tertiary.f32(i32 46, float %15, float %35, float %49)
  %51 = call float @dx.op.tertiary.f32(i32 46, float %28, float %40, float %50)
  %52 = fadd fast float %51, %45
  %53 = fmul fast float %31, %14
  %54 = call float @dx.op.tertiary.f32(i32 46, float %15, float %36, float %53)
  %55 = call float @dx.op.tertiary.f32(i32 46, float %28, float %41, float %54)
  %56 = fadd fast float %55, %46
  %57 = fmul fast float %32, %14
  %58 = call float @dx.op.tertiary.f32(i32 46, float %15, float %37, float %57)
  %59 = call float @dx.op.tertiary.f32(i32 46, float %28, float %42, float %58)
  %60 = fadd fast float %59, %47
  %61 = fmul fast float %33, %14
  %62 = call float @dx.op.tertiary.f32(i32 46, float %15, float %38, float %61)
  %63 = call float @dx.op.tertiary.f32(i32 46, float %28, float %43, float %62)
  %64 = fadd fast float %63, %48
  %65 = fdiv fast float %52, %64
  %66 = fdiv fast float %56, %64
  %67 = fdiv fast float %60, %64
  %68 = fmul fast float %23, 2.000000e+00
  %69 = fmul fast float %24, 2.000000e+00
  %70 = fadd fast float %68, -1.000000e+00
  %71 = fadd fast float %69, -1.000000e+00
  %72 = call float @dx.op.unary.f32(i32 6, float %70)
  %73 = fsub fast float 1.000000e+00, %72
  %74 = call float @dx.op.unary.f32(i32 6, float %71)
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
  %86 = call float @dx.op.dot3.f32(i32 55, float %84, float %85, float %75, float %84, float %85, float %75)
  %87 = call float @dx.op.unary.f32(i32 25, float %86)
  %88 = fmul fast float %84, %87
  %89 = fmul fast float %85, %87
  %90 = fmul fast float %87, %75
  %91 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 139)
  %92 = extractvalue %dx.types.CBufRet.f32 %91, 0
  %93 = extractvalue %dx.types.CBufRet.f32 %91, 1
  %94 = extractvalue %dx.types.CBufRet.f32 %91, 2
  %95 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 140)
  %96 = extractvalue %dx.types.CBufRet.f32 %95, 0
  %97 = extractvalue %dx.types.CBufRet.f32 %95, 1
  %98 = extractvalue %dx.types.CBufRet.f32 %95, 2
  %99 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 141)
  %100 = extractvalue %dx.types.CBufRet.f32 %99, 0
  %101 = extractvalue %dx.types.CBufRet.f32 %99, 1
  %102 = extractvalue %dx.types.CBufRet.f32 %99, 2
  %103 = fmul fast float %92, %88
  %104 = call float @dx.op.tertiary.f32(i32 46, float %89, float %96, float %103)
  %105 = call float @dx.op.tertiary.f32(i32 46, float %90, float %100, float %104)
  %106 = fmul fast float %93, %88
  %107 = call float @dx.op.tertiary.f32(i32 46, float %89, float %97, float %106)
  %108 = call float @dx.op.tertiary.f32(i32 46, float %90, float %101, float %107)
  %109 = fmul fast float %94, %88
  %110 = call float @dx.op.tertiary.f32(i32 46, float %89, float %98, float %109)
  %111 = call float @dx.op.tertiary.f32(i32 46, float %90, float %102, float %110)
  %112 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 4)
  %113 = extractvalue %dx.types.CBufRet.f32 %112, 0
  %114 = extractvalue %dx.types.CBufRet.f32 %112, 1
  %115 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 5)
  %116 = extractvalue %dx.types.CBufRet.f32 %115, 0
  %117 = extractvalue %dx.types.CBufRet.f32 %115, 1
  %118 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 6)
  %119 = extractvalue %dx.types.CBufRet.f32 %118, 0
  %120 = extractvalue %dx.types.CBufRet.f32 %118, 1
  %121 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 7)
  %122 = extractvalue %dx.types.CBufRet.f32 %121, 0
  %123 = extractvalue %dx.types.CBufRet.f32 %121, 1
  %124 = fmul fast float %113, %65
  %125 = call float @dx.op.tertiary.f32(i32 46, float %66, float %116, float %124)
  %126 = call float @dx.op.tertiary.f32(i32 46, float %67, float %119, float %125)
  %127 = fadd fast float %126, %122
  %128 = fmul fast float %114, %65
  %129 = call float @dx.op.tertiary.f32(i32 46, float %66, float %117, float %128)
  %130 = call float @dx.op.tertiary.f32(i32 46, float %67, float %120, float %129)
  %131 = fadd fast float %130, %123
  %132 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)
  %133 = extractvalue %dx.types.CBufRet.f32 %132, 0
  %134 = extractvalue %dx.types.CBufRet.f32 %132, 1
  %135 = extractvalue %dx.types.CBufRet.f32 %132, 2
  %136 = extractvalue %dx.types.CBufRet.f32 %132, 3
  %137 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 10)
  %138 = extractvalue %dx.types.CBufRet.f32 %137, 0
  %139 = extractvalue %dx.types.CBufRet.f32 %137, 1
  %140 = extractvalue %dx.types.CBufRet.f32 %137, 2
  %141 = extractvalue %dx.types.CBufRet.f32 %137, 3
  %142 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)
  %143 = extractvalue %dx.types.CBufRet.f32 %142, 0
  %144 = extractvalue %dx.types.CBufRet.f32 %142, 1
  %145 = extractvalue %dx.types.CBufRet.f32 %142, 2
  %146 = extractvalue %dx.types.CBufRet.f32 %142, 3
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 0
  %149 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %150 = extractvalue %dx.types.CBufRet.f32 %147, 2
  %151 = extractvalue %dx.types.CBufRet.f32 %147, 3
  %152 = fmul fast float %133, %65
  %153 = call float @dx.op.tertiary.f32(i32 46, float %66, float %138, float %152)
  %154 = call float @dx.op.tertiary.f32(i32 46, float %67, float %143, float %153)
  %155 = fadd fast float %154, %148
  %156 = fmul fast float %134, %65
  %157 = call float @dx.op.tertiary.f32(i32 46, float %66, float %139, float %156)
  %158 = call float @dx.op.tertiary.f32(i32 46, float %67, float %144, float %157)
  %159 = fadd fast float %158, %149
  %160 = fmul fast float %135, %65
  %161 = call float @dx.op.tertiary.f32(i32 46, float %66, float %140, float %160)
  %162 = call float @dx.op.tertiary.f32(i32 46, float %67, float %145, float %161)
  %163 = fadd fast float %162, %150
  %164 = fmul fast float %136, %65
  %165 = call float @dx.op.tertiary.f32(i32 46, float %66, float %141, float %164)
  %166 = call float @dx.op.tertiary.f32(i32 46, float %67, float %146, float %165)
  %167 = fadd fast float %166, %151
  %168 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 13)
  %169 = extractvalue %dx.types.CBufRet.f32 %168, 0
  %170 = extractvalue %dx.types.CBufRet.f32 %168, 1
  %171 = extractvalue %dx.types.CBufRet.f32 %168, 2
  %172 = extractvalue %dx.types.CBufRet.f32 %168, 3
  %173 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)
  %174 = extractvalue %dx.types.CBufRet.f32 %173, 0
  %175 = extractvalue %dx.types.CBufRet.f32 %173, 1
  %176 = extractvalue %dx.types.CBufRet.f32 %173, 2
  %177 = extractvalue %dx.types.CBufRet.f32 %173, 3
  %178 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)
  %179 = extractvalue %dx.types.CBufRet.f32 %178, 0
  %180 = extractvalue %dx.types.CBufRet.f32 %178, 1
  %181 = extractvalue %dx.types.CBufRet.f32 %178, 2
  %182 = extractvalue %dx.types.CBufRet.f32 %178, 3
  %183 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)
  %184 = extractvalue %dx.types.CBufRet.f32 %183, 0
  %185 = extractvalue %dx.types.CBufRet.f32 %183, 1
  %186 = extractvalue %dx.types.CBufRet.f32 %183, 2
  %187 = extractvalue %dx.types.CBufRet.f32 %183, 3
  %188 = fmul fast float %169, %65
  %189 = call float @dx.op.tertiary.f32(i32 46, float %66, float %174, float %188)
  %190 = call float @dx.op.tertiary.f32(i32 46, float %67, float %179, float %189)
  %191 = fadd fast float %190, %184
  %192 = fmul fast float %170, %65
  %193 = call float @dx.op.tertiary.f32(i32 46, float %66, float %175, float %192)
  %194 = call float @dx.op.tertiary.f32(i32 46, float %67, float %180, float %193)
  %195 = fadd fast float %194, %185
  %196 = fmul fast float %171, %65
  %197 = call float @dx.op.tertiary.f32(i32 46, float %66, float %176, float %196)
  %198 = call float @dx.op.tertiary.f32(i32 46, float %67, float %181, float %197)
  %199 = fadd fast float %198, %186
  %200 = fmul fast float %172, %65
  %201 = call float @dx.op.tertiary.f32(i32 46, float %66, float %177, float %200)
  %202 = call float @dx.op.tertiary.f32(i32 46, float %67, float %182, float %201)
  %203 = fadd fast float %202, %187
  %204 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)
  %205 = extractvalue %dx.types.CBufRet.f32 %204, 0
  %206 = extractvalue %dx.types.CBufRet.f32 %204, 1
  %207 = extractvalue %dx.types.CBufRet.f32 %204, 2
  %208 = extractvalue %dx.types.CBufRet.f32 %204, 3
  %209 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 18)
  %210 = extractvalue %dx.types.CBufRet.f32 %209, 0
  %211 = extractvalue %dx.types.CBufRet.f32 %209, 1
  %212 = extractvalue %dx.types.CBufRet.f32 %209, 2
  %213 = extractvalue %dx.types.CBufRet.f32 %209, 3
  %214 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 19)
  %215 = extractvalue %dx.types.CBufRet.f32 %214, 0
  %216 = extractvalue %dx.types.CBufRet.f32 %214, 1
  %217 = extractvalue %dx.types.CBufRet.f32 %214, 2
  %218 = extractvalue %dx.types.CBufRet.f32 %214, 3
  %219 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 20)
  %220 = extractvalue %dx.types.CBufRet.f32 %219, 0
  %221 = extractvalue %dx.types.CBufRet.f32 %219, 1
  %222 = extractvalue %dx.types.CBufRet.f32 %219, 2
  %223 = extractvalue %dx.types.CBufRet.f32 %219, 3
  %224 = fmul fast float %205, %65
  %225 = call float @dx.op.tertiary.f32(i32 46, float %66, float %210, float %224)
  %226 = call float @dx.op.tertiary.f32(i32 46, float %67, float %215, float %225)
  %227 = fadd fast float %226, %220
  %228 = fmul fast float %206, %65
  %229 = call float @dx.op.tertiary.f32(i32 46, float %66, float %211, float %228)
  %230 = call float @dx.op.tertiary.f32(i32 46, float %67, float %216, float %229)
  %231 = fadd fast float %230, %221
  %232 = fmul fast float %207, %65
  %233 = call float @dx.op.tertiary.f32(i32 46, float %66, float %212, float %232)
  %234 = call float @dx.op.tertiary.f32(i32 46, float %67, float %217, float %233)
  %235 = fadd fast float %234, %222
  %236 = fmul fast float %208, %65
  %237 = call float @dx.op.tertiary.f32(i32 46, float %66, float %213, float %236)
  %238 = call float @dx.op.tertiary.f32(i32 46, float %67, float %218, float %237)
  %239 = fadd fast float %238, %223
  %240 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 21)
  %241 = extractvalue %dx.types.CBufRet.f32 %240, 0
  %242 = extractvalue %dx.types.CBufRet.f32 %240, 1
  %243 = extractvalue %dx.types.CBufRet.f32 %240, 2
  %244 = extractvalue %dx.types.CBufRet.f32 %240, 3
  %245 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 22)
  %246 = extractvalue %dx.types.CBufRet.f32 %245, 0
  %247 = extractvalue %dx.types.CBufRet.f32 %245, 1
  %248 = extractvalue %dx.types.CBufRet.f32 %245, 2
  %249 = extractvalue %dx.types.CBufRet.f32 %245, 3
  %250 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 23)
  %251 = extractvalue %dx.types.CBufRet.f32 %250, 0
  %252 = extractvalue %dx.types.CBufRet.f32 %250, 1
  %253 = extractvalue %dx.types.CBufRet.f32 %250, 2
  %254 = extractvalue %dx.types.CBufRet.f32 %250, 3
  %255 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 24)
  %256 = extractvalue %dx.types.CBufRet.f32 %255, 0
  %257 = extractvalue %dx.types.CBufRet.f32 %255, 1
  %258 = extractvalue %dx.types.CBufRet.f32 %255, 2
  %259 = extractvalue %dx.types.CBufRet.f32 %255, 3
  %260 = fmul fast float %241, %65
  %261 = call float @dx.op.tertiary.f32(i32 46, float %66, float %246, float %260)
  %262 = call float @dx.op.tertiary.f32(i32 46, float %67, float %251, float %261)
  %263 = fadd fast float %262, %256
  %264 = fmul fast float %242, %65
  %265 = call float @dx.op.tertiary.f32(i32 46, float %66, float %247, float %264)
  %266 = call float @dx.op.tertiary.f32(i32 46, float %67, float %252, float %265)
  %267 = fadd fast float %266, %257
  %268 = fmul fast float %243, %65
  %269 = call float @dx.op.tertiary.f32(i32 46, float %66, float %248, float %268)
  %270 = call float @dx.op.tertiary.f32(i32 46, float %67, float %253, float %269)
  %271 = fadd fast float %270, %258
  %272 = fmul fast float %244, %65
  %273 = call float @dx.op.tertiary.f32(i32 46, float %66, float %249, float %272)
  %274 = call float @dx.op.tertiary.f32(i32 46, float %67, float %254, float %273)
  %275 = fadd fast float %274, %259
  %276 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 25)
  %277 = extractvalue %dx.types.CBufRet.f32 %276, 0
  %278 = extractvalue %dx.types.CBufRet.f32 %276, 1
  %279 = extractvalue %dx.types.CBufRet.f32 %276, 2
  %280 = extractvalue %dx.types.CBufRet.f32 %276, 3
  %281 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 29)
  %282 = extractvalue %dx.types.CBufRet.f32 %281, 0
  %283 = extractvalue %dx.types.CBufRet.f32 %281, 1
  %284 = extractvalue %dx.types.CBufRet.f32 %281, 2
  %285 = extractvalue %dx.types.CBufRet.f32 %281, 3
  %286 = call float @dx.op.dot4.f32(i32 56, float %155, float %159, float %163, float %167, float %282, float %283, float %284, float %285)
  %287 = fcmp fast ogt float %286, 0.000000e+00
  %288 = select i1 %287, float %191, float %155
  %289 = select i1 %287, float %195, float %159
  %290 = select i1 %287, float %199, float %163
  %291 = select i1 %287, float %203, float %167
  %292 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 26)
  %293 = extractvalue %dx.types.CBufRet.f32 %292, 0
  %294 = extractvalue %dx.types.CBufRet.f32 %292, 1
  %295 = extractvalue %dx.types.CBufRet.f32 %292, 2
  %296 = extractvalue %dx.types.CBufRet.f32 %292, 3
  %297 = select i1 %287, float %293, float %277
  %298 = select i1 %287, float %294, float %278
  %299 = select i1 %287, float %295, float %279
  %300 = select i1 %287, float %296, float %280
  %301 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 30)
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
  %312 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 27)
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
  %325 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 28)
  %326 = extractvalue %dx.types.CBufRet.f32 %325, 0
  %327 = extractvalue %dx.types.CBufRet.f32 %325, 1
  %328 = extractvalue %dx.types.CBufRet.f32 %325, 2
  %329 = extractvalue %dx.types.CBufRet.f32 %325, 3
  %330 = select i1 %307, float %326, float %317
  %331 = select i1 %307, float %327, float %318
  %332 = select i1 %307, float %328, float %319
  %333 = select i1 %307, float %329, float %320
  %334 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)
  %335 = extractvalue %dx.types.CBufRet.i32 %334, 1
  %336 = fmul fast float %18, 5.000000e-01
  %337 = fadd fast float %19, %18
  %338 = fmul fast float %337, 5.000000e-01
  %339 = call float @dx.op.unary.f32(i32 22, float %336)
  %340 = call float @dx.op.unary.f32(i32 22, float %338)
  %341 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 31)
  %342 = extractvalue %dx.types.CBufRet.i32 %341, 0
  %343 = icmp eq i32 %342, 0
  br i1 %343, label %3301, label %344

; <label>:344
  %345 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)
  %346 = extractvalue %dx.types.CBufRet.i32 %345, 0
  %347 = icmp eq i32 %346, 0
  br i1 %347, label %396, label %348

; <label>:348
  %349 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 35)
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

; <label>:369
  %370 = call float @dx.op.unary.f32(i32 7, float %357)
  %371 = call float @dx.op.unary.f32(i32 7, float %359)
  %372 = fmul fast float %370, %330
  %373 = fmul fast float %371, %331
  %374 = fadd fast float %372, %332
  %375 = fadd fast float %373, %333
  %376 = call float @dx.op.unary.f32(i32 7, float %323)
  %377 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %374, float %375, float undef, float undef, i32 0, i32 0, i32 undef, float %376)
  %378 = extractvalue %dx.types.ResRet.f32 %377, 0
  %379 = call float @dx.op.unary.f32(i32 7, float %361)
  %380 = fmul fast float %379, %330
  %381 = fadd fast float %380, %332
  %382 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %381, float %375, float undef, float undef, i32 0, i32 0, i32 undef, float %376)
  %383 = extractvalue %dx.types.ResRet.f32 %382, 0
  %384 = fadd fast float %383, %378
  %385 = call float @dx.op.unary.f32(i32 7, float %366)
  %386 = fmul fast float %385, %331
  %387 = fadd fast float %386, %333
  %388 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %374, float %387, float undef, float undef, i32 0, i32 0, i32 undef, float %376)
  %389 = extractvalue %dx.types.ResRet.f32 %388, 0
  %390 = fadd fast float %384, %389
  %391 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %381, float %387, float undef, float undef, i32 0, i32 0, i32 undef, float %376)
  %392 = extractvalue %dx.types.ResRet.f32 %391, 0
  %393 = fadd fast float %390, %392
  %394 = fmul fast float %393, 2.500000e-01
  %395 = fsub fast float 1.000000e+00, %394
  br label %515

; <label>:396
  %397 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)
  %398 = extractvalue %dx.types.CBufRet.i32 %397, 0
  %399 = icmp eq i32 %398, 0
  br i1 %399, label %501, label %400

; <label>:400
  %401 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 35)
  %402 = extractvalue %dx.types.CBufRet.f32 %401, 0
  %403 = extractvalue %dx.types.CBufRet.f32 %401, 1
  %404 = fadd fast float %402, %321
  %405 = fadd fast float %403, %322
  %406 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 34)
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

; <label>:420
  %421 = call float @dx.op.unary.f32(i32 7, float %404)
  %422 = call float @dx.op.unary.f32(i32 7, float %405)
  %423 = fmul fast float %421, %330
  %424 = fmul fast float %422, %331
  %425 = fadd fast float %423, %332
  %426 = fadd fast float %424, %333
  %427 = call float @dx.op.unary.f32(i32 7, float %323)
  %428 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %429 = extractvalue %dx.types.ResRet.f32 %428, 0
  %430 = fadd fast float %404, %407
  %431 = call float @dx.op.unary.f32(i32 7, float %430)
  %432 = fmul fast float %431, %330
  %433 = fadd fast float %432, %332
  %434 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %435 = extractvalue %dx.types.ResRet.f32 %434, 0
  %436 = fadd fast float %435, %429
  %437 = fadd fast float %430, %407
  %438 = call float @dx.op.unary.f32(i32 7, float %437)
  %439 = fmul fast float %438, %330
  %440 = fadd fast float %439, %332
  %441 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %442 = extractvalue %dx.types.ResRet.f32 %441, 0
  %443 = fadd fast float %436, %442
  %444 = fadd fast float %437, %407
  %445 = call float @dx.op.unary.f32(i32 7, float %444)
  %446 = fmul fast float %445, %330
  %447 = fadd fast float %446, %332
  %448 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %426, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %449 = extractvalue %dx.types.ResRet.f32 %448, 0
  %450 = fadd fast float %443, %449
  %451 = fadd fast float %405, %408
  %452 = call float @dx.op.unary.f32(i32 7, float %451)
  %453 = fmul fast float %452, %331
  %454 = fadd fast float %453, %333
  %455 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %456 = extractvalue %dx.types.ResRet.f32 %455, 0
  %457 = fadd fast float %450, %456
  %458 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %459 = extractvalue %dx.types.ResRet.f32 %458, 0
  %460 = fadd fast float %457, %459
  %461 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %462 = extractvalue %dx.types.ResRet.f32 %461, 0
  %463 = fadd fast float %460, %462
  %464 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %454, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %465 = extractvalue %dx.types.ResRet.f32 %464, 0
  %466 = fadd fast float %463, %465
  %467 = fadd fast float %451, %408
  %468 = call float @dx.op.unary.f32(i32 7, float %467)
  %469 = fmul fast float %468, %331
  %470 = fadd fast float %469, %333
  %471 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %472 = extractvalue %dx.types.ResRet.f32 %471, 0
  %473 = fadd fast float %466, %472
  %474 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %475 = extractvalue %dx.types.ResRet.f32 %474, 0
  %476 = fadd fast float %473, %475
  %477 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %478 = extractvalue %dx.types.ResRet.f32 %477, 0
  %479 = fadd fast float %476, %478
  %480 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %470, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %481 = extractvalue %dx.types.ResRet.f32 %480, 0
  %482 = fadd fast float %479, %481
  %483 = fadd fast float %467, %408
  %484 = call float @dx.op.unary.f32(i32 7, float %483)
  %485 = fmul fast float %484, %331
  %486 = fadd fast float %485, %333
  %487 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %425, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %488 = extractvalue %dx.types.ResRet.f32 %487, 0
  %489 = fadd fast float %482, %488
  %490 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %433, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %491 = extractvalue %dx.types.ResRet.f32 %490, 0
  %492 = fadd fast float %489, %491
  %493 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %440, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %494 = extractvalue %dx.types.ResRet.f32 %493, 0
  %495 = fadd fast float %492, %494
  %496 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %447, float %486, float undef, float undef, i32 0, i32 0, i32 undef, float %427)
  %497 = extractvalue %dx.types.ResRet.f32 %496, 0
  %498 = fadd fast float %495, %497
  %499 = fmul fast float %498, 6.250000e-02
  %500 = fsub fast float 1.000000e+00, %499
  br label %515

; <label>:501
  %502 = call float @dx.op.unary.f32(i32 7, float %321)
  %503 = call float @dx.op.unary.f32(i32 7, float %322)
  %504 = fmul fast float %502, %330
  %505 = fmul fast float %503, %331
  %506 = fadd fast float %504, %332
  %507 = fadd fast float %505, %333
  %508 = fdiv fast float %323, %324
  %509 = call float @dx.op.unary.f32(i32 7, float %508)
  %510 = fdiv fast float %506, %324
  %511 = fdiv fast float %507, %324
  %512 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %7, %dx.types.Handle %11, float %510, float %511, float undef, float undef, i32 0, i32 0, i32 undef, float %509)
  %513 = extractvalue %dx.types.ResRet.f32 %512, 0
  %514 = fsub fast float 1.000000e+00, %513
  br label %515

; <label>:515
  %516 = phi float [ %514, %501 ], [ %395, %369 ], [ 0.000000e+00, %348 ], [ %500, %420 ], [ 0.000000e+00, %400 ]
  %517 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 2)
  %518 = extractvalue %dx.types.CBufRet.f32 %517, 3
  %519 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 36)
  %520 = extractvalue %dx.types.CBufRet.f32 %519, 0
  %521 = extractvalue %dx.types.CBufRet.f32 %519, 1
  %522 = extractvalue %dx.types.CBufRet.f32 %519, 2
  %523 = extractvalue %dx.types.CBufRet.f32 %519, 3
  %524 = call float @dx.op.dot4.f32(i32 56, float %520, float %521, float %522, float %523, float %155, float %159, float %163, float %167)
  %525 = call float @dx.op.unary.f32(i32 7, float %524)
  %526 = fmul fast float %518, %516
  %527 = fmul fast float %526, %525
  %528 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)
  %529 = extractvalue %dx.types.CBufRet.i32 %528, 2
  %530 = icmp sgt i32 %529, 0
  br i1 %530, label %531, label %713

; <label>:531
  br label %532

; <label>:532
  %533 = phi i32 [ %708, %697 ], [ 0, %531 ]
  %534 = phi float [ %707, %697 ], [ %527, %531 ]
  %535 = shl i32 %533, 2
  %536 = add i32 %535, 38
  %537 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %536)
  %538 = extractvalue %dx.types.CBufRet.f32 %537, 0
  %539 = extractvalue %dx.types.CBufRet.f32 %537, 1
  %540 = extractvalue %dx.types.CBufRet.f32 %537, 2
  %541 = add i32 %535, 39
  %542 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %541)
  %543 = extractvalue %dx.types.CBufRet.f32 %542, 0
  %544 = extractvalue %dx.types.CBufRet.f32 %542, 1
  %545 = extractvalue %dx.types.CBufRet.f32 %542, 2
  %546 = add i32 %535, 40
  %547 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %546)
  %548 = extractvalue %dx.types.CBufRet.f32 %547, 0
  %549 = extractvalue %dx.types.CBufRet.f32 %547, 1
  %550 = extractvalue %dx.types.CBufRet.f32 %547, 2
  %551 = add i32 %535, 41
  %552 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %551)
  %553 = extractvalue %dx.types.CBufRet.f32 %552, 0
  %554 = extractvalue %dx.types.CBufRet.f32 %552, 1
  %555 = extractvalue %dx.types.CBufRet.f32 %552, 2
  %556 = fmul fast float %538, %65
  %557 = call float @dx.op.tertiary.f32(i32 46, float %66, float %543, float %556)
  %558 = call float @dx.op.tertiary.f32(i32 46, float %67, float %548, float %557)
  %559 = fadd fast float %558, %553
  %560 = fmul fast float %539, %65
  %561 = call float @dx.op.tertiary.f32(i32 46, float %66, float %544, float %560)
  %562 = call float @dx.op.tertiary.f32(i32 46, float %67, float %549, float %561)
  %563 = fadd fast float %562, %554
  %564 = fmul fast float %540, %65
  %565 = call float @dx.op.tertiary.f32(i32 46, float %66, float %545, float %564)
  %566 = call float @dx.op.tertiary.f32(i32 46, float %67, float %550, float %565)
  %567 = fadd fast float %566, %555
  %568 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)
  %569 = extractvalue %dx.types.CBufRet.i32 %568, 0
  %570 = icmp eq i32 %569, 0
  br i1 %570, label %610, label %571

; <label>:571
  %572 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 54)
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

; <label>:594
  %595 = sitofp i32 %533 to float
  %596 = call float @dx.op.unary.f32(i32 7, float %567)
  %597 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %583, float %584, float %595, float undef, i32 0, i32 0, i32 undef, float %596)
  %598 = extractvalue %dx.types.ResRet.f32 %597, 0
  %599 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %586, float %584, float %595, float undef, i32 0, i32 0, i32 undef, float %596)
  %600 = extractvalue %dx.types.ResRet.f32 %599, 0
  %601 = fadd fast float %600, %598
  %602 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %583, float %591, float %595, float undef, i32 0, i32 0, i32 undef, float %596)
  %603 = extractvalue %dx.types.ResRet.f32 %602, 0
  %604 = fadd fast float %601, %603
  %605 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %586, float %591, float %595, float undef, i32 0, i32 0, i32 undef, float %596)
  %606 = extractvalue %dx.types.ResRet.f32 %605, 0
  %607 = fadd fast float %604, %606
  %608 = fmul fast float %607, 2.500000e-01
  %609 = fsub fast float 1.000000e+00, %608
  br label %697

; <label>:610
  %611 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)
  %612 = extractvalue %dx.types.CBufRet.i32 %611, 0
  %613 = icmp eq i32 %612, 0
  br i1 %613, label %691, label %614

; <label>:614
  %615 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 54)
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

; <label>:633
  %634 = sitofp i32 %533 to float
  %635 = call float @dx.op.unary.f32(i32 7, float %567)
  %636 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %637 = extractvalue %dx.types.ResRet.f32 %636, 0
  %638 = fadd fast float %620, %616
  %639 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %640 = extractvalue %dx.types.ResRet.f32 %639, 0
  %641 = fadd fast float %640, %637
  %642 = fadd fast float %638, %616
  %643 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %644 = extractvalue %dx.types.ResRet.f32 %643, 0
  %645 = fadd fast float %641, %644
  %646 = fadd fast float %642, %616
  %647 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %621, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %648 = extractvalue %dx.types.ResRet.f32 %647, 0
  %649 = fadd fast float %645, %648
  %650 = fadd fast float %621, %617
  %651 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %652 = extractvalue %dx.types.ResRet.f32 %651, 0
  %653 = fadd fast float %649, %652
  %654 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %655 = extractvalue %dx.types.ResRet.f32 %654, 0
  %656 = fadd fast float %653, %655
  %657 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %658 = extractvalue %dx.types.ResRet.f32 %657, 0
  %659 = fadd fast float %656, %658
  %660 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %650, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %661 = extractvalue %dx.types.ResRet.f32 %660, 0
  %662 = fadd fast float %659, %661
  %663 = fadd fast float %650, %617
  %664 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %665 = extractvalue %dx.types.ResRet.f32 %664, 0
  %666 = fadd fast float %662, %665
  %667 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %668 = extractvalue %dx.types.ResRet.f32 %667, 0
  %669 = fadd fast float %666, %668
  %670 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %671 = extractvalue %dx.types.ResRet.f32 %670, 0
  %672 = fadd fast float %669, %671
  %673 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %663, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %674 = extractvalue %dx.types.ResRet.f32 %673, 0
  %675 = fadd fast float %672, %674
  %676 = fadd fast float %663, %617
  %677 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %620, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %678 = extractvalue %dx.types.ResRet.f32 %677, 0
  %679 = fadd fast float %675, %678
  %680 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %638, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %681 = extractvalue %dx.types.ResRet.f32 %680, 0
  %682 = fadd fast float %679, %681
  %683 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %642, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %684 = extractvalue %dx.types.ResRet.f32 %683, 0
  %685 = fadd fast float %682, %684
  %686 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %646, float %676, float %634, float undef, i32 0, i32 0, i32 undef, float %635)
  %687 = extractvalue %dx.types.ResRet.f32 %686, 0
  %688 = fadd fast float %685, %687
  %689 = fmul fast float %688, 6.250000e-02
  %690 = fsub fast float 1.000000e+00, %689
  br label %697

; <label>:691
  %692 = sitofp i32 %533 to float
  %693 = call float @dx.op.unary.f32(i32 7, float %567)
  %694 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %6, %dx.types.Handle %10, float %559, float %563, float %692, float undef, i32 0, i32 0, i32 undef, float %693)
  %695 = extractvalue %dx.types.ResRet.f32 %694, 0
  %696 = fsub fast float 1.000000e+00, %695
  br label %697

; <label>:697
  %698 = phi float [ %696, %691 ], [ %609, %594 ], [ 0.000000e+00, %571 ], [ %690, %633 ], [ 0.000000e+00, %614 ]
  %699 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 36)
  %700 = extractvalue %dx.types.CBufRet.f32 %699, 0
  %701 = extractvalue %dx.types.CBufRet.f32 %699, 1
  %702 = extractvalue %dx.types.CBufRet.f32 %699, 2
  %703 = extractvalue %dx.types.CBufRet.f32 %699, 3
  %704 = call float @dx.op.dot4.f32(i32 56, float %700, float %701, float %702, float %703, float %155, float %159, float %163, float %167)
  %705 = call float @dx.op.unary.f32(i32 7, float %704)
  %706 = fmul fast float %705, %698
  %707 = call float @dx.op.binary.f32(i32 35, float %534, float %706)
  %708 = add nuw nsw i32 %533, 1
  %709 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)
  %710 = extractvalue %dx.types.CBufRet.i32 %709, 2
  %711 = icmp slt i32 %708, %710
  br i1 %711, label %532, label %712

; <label>:712
  br label %713

; <label>:713
  %714 = phi float [ %527, %515 ], [ %707, %712 ]
  %715 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 37)
  %716 = extractvalue %dx.types.CBufRet.i32 %715, 1
  %717 = icmp sgt i32 %716, 0
  br i1 %717, label %718, label %3298

; <label>:718
  %719 = fcmp fast oeq float %28, 0.000000e+00
  %720 = fcmp fast oeq float %28, 1.000000e+00
  %721 = or i1 %719, %720
  br i1 %721, label %3298, label %722

; <label>:722
  %723 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 3)
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
  %735 = call float @dx.op.unary.f32(i32 24, float %734)
  %736 = extractvalue %dx.types.CBufRet.i32 %715, 0
  %737 = call float @dx.op.unary.f32(i32 23, float %735)
  %738 = fadd fast float %737, 1.000000e+00
  %739 = uitofp i32 %736 to float
  %740 = fsub fast float %738, %739
  %741 = and i32 %736, 31
  %742 = shl i32 1, %741
  %743 = uitofp i32 %742 to float
  %744 = fmul fast float %743, 5.000000e-01
  %745 = fdiv fast float %735, %744
  %746 = fmul fast float %745, 2.000000e+00
  %747 = call float @dx.op.unary.f32(i32 6, float %746)
  %748 = call float @dx.op.binary.f32(i32 35, float %747, float 1.000000e+00)
  %749 = call float @dx.op.unary.f32(i32 23, float %748)
  %750 = fptoui float %749 to i32
  %751 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 55)
  %752 = extractvalue %dx.types.CBufRet.i32 %751, 0
  %753 = call i32 @dx.op.binary.i32(i32 37, i32 %750, i32 %752)
  %754 = icmp ult i32 %753, %716
  br i1 %754, label %755, label %903

; <label>:755
  br label %756

; <label>:756
  %757 = phi i32 [ %828, %756 ], [ %716, %755 ]
  %758 = phi i32 [ %829, %756 ], [ %753, %755 ]
  %759 = shl i32 %758, 2
  %760 = add i32 %759, 56
  %761 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %760)
  %762 = extractvalue %dx.types.CBufRet.f32 %761, 0
  %763 = extractvalue %dx.types.CBufRet.f32 %761, 1
  %764 = add i32 %759, 57
  %765 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %764)
  %766 = extractvalue %dx.types.CBufRet.f32 %765, 0
  %767 = extractvalue %dx.types.CBufRet.f32 %765, 1
  %768 = add i32 %759, 58
  %769 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %768)
  %770 = extractvalue %dx.types.CBufRet.f32 %769, 0
  %771 = extractvalue %dx.types.CBufRet.f32 %769, 1
  %772 = add i32 %759, 59
  %773 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %772)
  %774 = extractvalue %dx.types.CBufRet.f32 %773, 0
  %775 = extractvalue %dx.types.CBufRet.f32 %773, 1
  %776 = fmul fast float %762, %65
  %777 = call float @dx.op.tertiary.f32(i32 46, float %66, float %766, float %776)
  %778 = call float @dx.op.tertiary.f32(i32 46, float %67, float %770, float %777)
  %779 = fadd fast float %778, %774
  %780 = fmul fast float %763, %65
  %781 = call float @dx.op.tertiary.f32(i32 46, float %66, float %767, float %780)
  %782 = call float @dx.op.tertiary.f32(i32 46, float %67, float %771, float %781)
  %783 = fadd fast float %782, %775
  %784 = call float @dx.op.binary.f32(i32 35, float %779, float 0.000000e+00)
  %785 = call float @dx.op.binary.f32(i32 35, float %783, float 0.000000e+00)
  %786 = call float @dx.op.binary.f32(i32 36, float %784, float 0x3FEFF7CEE0000000)
  %787 = call float @dx.op.binary.f32(i32 36, float %785, float 0x3FEFF7CEE0000000)
  %788 = add i32 %758, 120
  %789 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %788)
  %790 = extractvalue %dx.types.CBufRet.f32 %789, 0
  %791 = extractvalue %dx.types.CBufRet.f32 %789, 1
  %792 = fadd fast float %790, %786
  %793 = fadd fast float %791, %787
  %794 = call float @dx.op.unary.f32(i32 22, float %792)
  %795 = call float @dx.op.unary.f32(i32 22, float %793)
  %796 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 135)
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
  %811 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)
  %812 = extractvalue %dx.types.CBufRet.f32 %811, 0
  %813 = extractvalue %dx.types.CBufRet.f32 %811, 1
  %814 = fmul fast float %813, %795
  %815 = extractvalue %dx.types.CBufRet.f32 %811, 2
  %816 = fmul fast float %815, %810
  %817 = call float @dx.op.unary.f32(i32 27, float %814)
  %818 = fadd fast float %817, %794
  %819 = fmul fast float %818, %812
  %820 = fadd fast float %819, %816
  %821 = fptoui float %820 to i32
  %822 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %821, i32 undef)
  %823 = extractvalue %dx.types.ResRet.i32 %822, 0
  %824 = and i32 %823, 1
  %825 = icmp ne i32 %824, 0
  %826 = and i1 %809, %825
  %827 = select i1 %826, i32 %758, i32 %716
  %828 = call i32 @dx.op.binary.i32(i32 40, i32 %757, i32 %827)
  %829 = add nuw i32 %758, 1
  %830 = icmp eq i32 %829, %716
  br i1 %830, label %831, label %756

; <label>:831
  %832 = icmp ult i32 %828, %716
  br i1 %832, label %837, label %833

; <label>:833
  %834 = add i32 %753, 1
  %835 = icmp eq i32 %834, %716
  %836 = select i1 %835, i32 -3, i32 -4
  br label %903

; <label>:837
  %838 = shl i32 %828, 2
  %839 = add i32 %838, 56
  %840 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %839)
  %841 = extractvalue %dx.types.CBufRet.f32 %840, 0
  %842 = extractvalue %dx.types.CBufRet.f32 %840, 1
  %843 = extractvalue %dx.types.CBufRet.f32 %840, 2
  %844 = add i32 %838, 57
  %845 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %844)
  %846 = extractvalue %dx.types.CBufRet.f32 %845, 0
  %847 = extractvalue %dx.types.CBufRet.f32 %845, 1
  %848 = extractvalue %dx.types.CBufRet.f32 %845, 2
  %849 = add i32 %838, 58
  %850 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %849)
  %851 = extractvalue %dx.types.CBufRet.f32 %850, 0
  %852 = extractvalue %dx.types.CBufRet.f32 %850, 1
  %853 = extractvalue %dx.types.CBufRet.f32 %850, 2
  %854 = add i32 %838, 59
  %855 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %854)
  %856 = extractvalue %dx.types.CBufRet.f32 %855, 0
  %857 = extractvalue %dx.types.CBufRet.f32 %855, 1
  %858 = extractvalue %dx.types.CBufRet.f32 %855, 2
  %859 = fmul fast float %841, %65
  %860 = call float @dx.op.tertiary.f32(i32 46, float %66, float %846, float %859)
  %861 = call float @dx.op.tertiary.f32(i32 46, float %67, float %851, float %860)
  %862 = fadd fast float %861, %856
  %863 = fmul fast float %842, %65
  %864 = call float @dx.op.tertiary.f32(i32 46, float %66, float %847, float %863)
  %865 = call float @dx.op.tertiary.f32(i32 46, float %67, float %852, float %864)
  %866 = fadd fast float %865, %857
  %867 = fmul fast float %843, %65
  %868 = call float @dx.op.tertiary.f32(i32 46, float %66, float %848, float %867)
  %869 = call float @dx.op.tertiary.f32(i32 46, float %67, float %853, float %868)
  %870 = fadd fast float %869, %858
  %871 = add i32 %838, 60
  %872 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %871)
  %873 = extractvalue %dx.types.CBufRet.f32 %872, 0
  %874 = extractvalue %dx.types.CBufRet.f32 %872, 1
  %875 = extractvalue %dx.types.CBufRet.f32 %872, 2
  %876 = add i32 %838, 61
  %877 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %876)
  %878 = extractvalue %dx.types.CBufRet.f32 %877, 0
  %879 = extractvalue %dx.types.CBufRet.f32 %877, 1
  %880 = extractvalue %dx.types.CBufRet.f32 %877, 2
  %881 = add i32 %838, 62
  %882 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %881)
  %883 = extractvalue %dx.types.CBufRet.f32 %882, 0
  %884 = extractvalue %dx.types.CBufRet.f32 %882, 1
  %885 = extractvalue %dx.types.CBufRet.f32 %882, 2
  %886 = add i32 %838, 63
  %887 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %886)
  %888 = extractvalue %dx.types.CBufRet.f32 %887, 0
  %889 = extractvalue %dx.types.CBufRet.f32 %887, 1
  %890 = extractvalue %dx.types.CBufRet.f32 %887, 2
  %891 = fmul fast float %873, %65
  %892 = call float @dx.op.tertiary.f32(i32 46, float %66, float %878, float %891)
  %893 = call float @dx.op.tertiary.f32(i32 46, float %67, float %883, float %892)
  %894 = fadd fast float %893, %888
  %895 = fmul fast float %874, %65
  %896 = call float @dx.op.tertiary.f32(i32 46, float %66, float %879, float %895)
  %897 = call float @dx.op.tertiary.f32(i32 46, float %67, float %884, float %896)
  %898 = fadd fast float %897, %889
  %899 = fmul fast float %875, %65
  %900 = call float @dx.op.tertiary.f32(i32 46, float %66, float %880, float %899)
  %901 = call float @dx.op.tertiary.f32(i32 46, float %67, float %885, float %900)
  %902 = fadd fast float %901, %890
  br label %903

; <label>:903
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

; <label>:913
  %914 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %915 = extractvalue %dx.types.CBufRet.f32 %914, 0
  %916 = fsub fast float 1.000000e+00, %915
  %917 = fmul fast float %916, %518
  %918 = call float @dx.op.binary.f32(i32 35, float %714, float %917)
  br label %3298

; <label>:919
  %920 = icmp eq i32 %904, -3
  br i1 %920, label %921, label %953

; <label>:921
  %922 = add i32 %716, -1
  %923 = uitofp i32 %922 to float
  %924 = fsub fast float %923, %740
  %925 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
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

; <label>:935
  %936 = fsub fast float %924, %929
  %937 = fsub fast float %932, %928
  %938 = fmul fast float %937, 0x3F847AE140000000
  %939 = fdiv fast float %936, %938
  %940 = call float @dx.op.unary.f32(i32 7, float %939)
  br label %944

; <label>:941
  %942 = fcmp fast oge float %924, %933
  %943 = select i1 %942, float 1.000000e+00, float 0.000000e+00
  br label %944

; <label>:944
  %945 = phi float [ %940, %935 ], [ %943, %941 ]
  %946 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %947 = extractvalue %dx.types.CBufRet.f32 %946, 0
  %948 = fsub fast float 1.000000e+00, %945
  %949 = fmul fast float %947, %948
  %950 = fsub fast float %948, %949
  %951 = fmul fast float %950, %518
  %952 = call float @dx.op.binary.f32(i32 35, float %714, float %951)
  br label %3298

; <label>:953
  %954 = icmp eq i32 %904, 0
  %955 = icmp slt i32 %911, %716
  %956 = and i1 %954, %955
  br i1 %956, label %957, label %3298

; <label>:957
  %958 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 33)
  %959 = extractvalue %dx.types.CBufRet.i32 %958, 0
  %960 = icmp eq i32 %959, 0
  br i1 %960, label %1426, label %961

; <label>:961
  %962 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 138)
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
  br i1 %983, label %3294, label %984

; <label>:984
  %985 = call float @dx.op.binary.f32(i32 35, float %973, float 0.000000e+00)
  %986 = call float @dx.op.binary.f32(i32 35, float %974, float 0.000000e+00)
  %987 = call float @dx.op.binary.f32(i32 36, float %985, float 0x3FEFF7CEE0000000)
  %988 = call float @dx.op.binary.f32(i32 36, float %986, float 0x3FEFF7CEE0000000)
  %989 = add i32 %911, 120
  %990 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %989)
  %991 = extractvalue %dx.types.CBufRet.f32 %990, 0
  %992 = extractvalue %dx.types.CBufRet.f32 %990, 1
  %993 = fadd fast float %991, %987
  %994 = fadd fast float %992, %988
  %995 = call float @dx.op.unary.f32(i32 22, float %993)
  %996 = call float @dx.op.unary.f32(i32 22, float %994)
  %997 = uitofp i32 %911 to float
  %998 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)
  %999 = extractvalue %dx.types.CBufRet.f32 %998, 0
  %1000 = extractvalue %dx.types.CBufRet.f32 %998, 1
  %1001 = fmul fast float %1000, %996
  %1002 = extractvalue %dx.types.CBufRet.f32 %998, 2
  %1003 = fmul fast float %1002, %997
  %1004 = call float @dx.op.unary.f32(i32 27, float %1001)
  %1005 = fadd fast float %1004, %995
  %1006 = fmul fast float %1005, %999
  %1007 = fadd fast float %1006, %1003
  %1008 = fptoui float %1007 to i32
  %1009 = call float @dx.op.unary.f32(i32 7, float %910)
  %1010 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1008, i32 undef)
  %1011 = extractvalue %dx.types.ResRet.i32 %1010, 0
  %1012 = and i32 %1011, 1
  %1013 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %995, float %996, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)
  %1014 = extractvalue %dx.types.ResRet.f32 %1013, 0
  %1015 = icmp ne i32 %1012, 0
  %1016 = select i1 %1015, float %1014, float 1.000000e+00
  %1017 = icmp eq i32 %1012, 0
  br i1 %1017, label %1094, label %1018

; <label>:1018
  %1019 = add i32 %911, 1
  %1020 = icmp eq i32 %1019, %716
  %1021 = fsub fast float %997, %740
  %1022 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
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

; <label>:1032
  br i1 %1031, label %1033, label %1039

; <label>:1033
  %1034 = fsub fast float %1021, %1026
  %1035 = fsub fast float %1029, %1025
  %1036 = fmul fast float %1035, 0x3F847AE140000000
  %1037 = fdiv fast float %1034, %1036
  %1038 = call float @dx.op.unary.f32(i32 7, float %1037)
  br label %1042

; <label>:1039
  %1040 = fcmp fast oge float %1021, %1030
  %1041 = select i1 %1040, float 1.000000e+00, float 0.000000e+00
  br label %1042

; <label>:1042
  %1043 = phi float [ %1038, %1033 ], [ %1041, %1039 ]
  %1044 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1045 = extractvalue %dx.types.CBufRet.f32 %1044, 0
  %1046 = fmul fast float %1043, %1016
  %1047 = fsub fast float 1.000000e+00, %1043
  %1048 = fmul fast float %1045, %1047
  %1049 = fadd fast float %1048, %1046
  br label %1094

; <label>:1050
  br i1 %1031, label %1051, label %1057

; <label>:1051
  %1052 = fsub fast float %1021, %1026
  %1053 = fsub fast float %1029, %1025
  %1054 = fmul fast float %1053, 0x3F847AE140000000
  %1055 = fdiv fast float %1052, %1054
  %1056 = call float @dx.op.unary.f32(i32 7, float %1055)
  br label %1060

; <label>:1057
  %1058 = fcmp fast oge float %1021, %1030
  %1059 = select i1 %1058, float 1.000000e+00, float 0.000000e+00
  br label %1060

; <label>:1060
  %1061 = phi float [ %1056, %1051 ], [ %1059, %1057 ]
  %1062 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1063 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1064 = call float @dx.op.binary.f32(i32 36, float %1062, float 0x3FEFF7CEE0000000)
  %1065 = call float @dx.op.binary.f32(i32 36, float %1063, float 0x3FEFF7CEE0000000)
  %1066 = add i32 %911, 121
  %1067 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1066)
  %1068 = extractvalue %dx.types.CBufRet.f32 %1067, 0
  %1069 = extractvalue %dx.types.CBufRet.f32 %1067, 1
  %1070 = fadd fast float %1068, %1064
  %1071 = fadd fast float %1069, %1065
  %1072 = call float @dx.op.unary.f32(i32 22, float %1070)
  %1073 = call float @dx.op.unary.f32(i32 22, float %1071)
  %1074 = uitofp i32 %1019 to float
  %1075 = fmul fast float %1000, %1073
  %1076 = fmul fast float %1002, %1074
  %1077 = call float @dx.op.unary.f32(i32 27, float %1075)
  %1078 = fadd fast float %1077, %1072
  %1079 = fmul fast float %1078, %999
  %1080 = fadd fast float %1079, %1076
  %1081 = fptoui float %1080 to i32
  %1082 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1081, i32 undef)
  %1083 = extractvalue %dx.types.ResRet.i32 %1082, 0
  %1084 = and i32 %1083, 1
  %1085 = icmp eq i32 %1084, 0
  br i1 %1085, label %1094, label %1086

; <label>:1086
  %1087 = call float @dx.op.unary.f32(i32 7, float %907)
  %1088 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1072, float %1073, float %1074, float undef, i32 0, i32 0, i32 undef, float %1087)
  %1089 = extractvalue %dx.types.ResRet.f32 %1088, 0
  %1090 = fmul fast float %1061, %1016
  %1091 = fsub fast float 1.000000e+00, %1061
  %1092 = fmul fast float %1089, %1091
  %1093 = fadd fast float %1092, %1090
  br label %1094

; <label>:1094
  %1095 = phi float [ %1016, %984 ], [ %1049, %1042 ], [ %1093, %1086 ], [ %1016, %1060 ]
  %1096 = phi i1 [ false, %984 ], [ false, %1042 ], [ true, %1060 ], [ true, %1086 ]
  %1097 = phi i32 [ 0, %984 ], [ 0, %1042 ], [ %1081, %1060 ], [ %1081, %1086 ]
  %1098 = phi i32 [ 0, %984 ], [ 0, %1042 ], [ %1083, %1060 ], [ %1083, %1086 ]
  %1099 = select i1 %1015, float 1.000000e+00, float 0.000000e+00
  %1100 = call float @dx.op.binary.f32(i32 35, float %976, float 0.000000e+00)
  %1101 = call float @dx.op.binary.f32(i32 36, float %1100, float 0x3FEFF7CEE0000000)
  %1102 = fadd fast float %991, %1101
  %1103 = call float @dx.op.unary.f32(i32 22, float %1102)
  %1104 = fadd fast float %1004, %1103
  %1105 = fmul fast float %1104, %999
  %1106 = fadd fast float %1105, %1003
  %1107 = fptoui float %1106 to i32
  %1108 = icmp eq i32 %1107, %1008
  br i1 %1108, label %1112, label %1109

; <label>:1109
  %1110 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1107, i32 undef)
  %1111 = extractvalue %dx.types.ResRet.i32 %1110, 0
  br label %1112

; <label>:1112
  %1113 = phi i32 [ %1111, %1109 ], [ %1011, %1094 ]
  %1114 = and i32 %1113, 1
  %1115 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1103, float %996, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)
  %1116 = extractvalue %dx.types.ResRet.f32 %1115, 0
  %1117 = icmp ne i32 %1114, 0
  %1118 = select i1 %1117, float %1116, float 1.000000e+00
  %1119 = icmp eq i32 %1114, 0
  br i1 %1119, label %1201, label %1120

; <label>:1120
  %1121 = add i32 %911, 1
  %1122 = icmp eq i32 %1121, %716
  %1123 = fsub fast float %997, %740
  %1124 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1125 = extractvalue %dx.types.CBufRet.i32 %1124, 1
  %1126 = and i32 %1125, 255
  %1127 = uitofp i32 %1126 to float
  %1128 = fmul fast float %1127, 0x3F847AE140000000
  %1129 = lshr i32 %1125, 8
  %1130 = and i32 %1129, 255
  %1131 = uitofp i32 %1130 to float
  %1132 = fmul fast float %1131, 0x3F847AE140000000
  %1133 = fcmp fast ogt float %1132, %1128
  br i1 %1122, label %1134, label %1152

; <label>:1134
  br i1 %1133, label %1135, label %1141

; <label>:1135
  %1136 = fsub fast float %1123, %1128
  %1137 = fsub fast float %1131, %1127
  %1138 = fmul fast float %1137, 0x3F847AE140000000
  %1139 = fdiv fast float %1136, %1138
  %1140 = call float @dx.op.unary.f32(i32 7, float %1139)
  br label %1144

; <label>:1141
  %1142 = fcmp fast oge float %1123, %1132
  %1143 = select i1 %1142, float 1.000000e+00, float 0.000000e+00
  br label %1144

; <label>:1144
  %1145 = phi float [ %1140, %1135 ], [ %1143, %1141 ]
  %1146 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1147 = extractvalue %dx.types.CBufRet.f32 %1146, 0
  %1148 = fmul fast float %1145, %1118
  %1149 = fsub fast float 1.000000e+00, %1145
  %1150 = fmul fast float %1147, %1149
  %1151 = fadd fast float %1150, %1148
  br label %1201

; <label>:1152
  br i1 %1133, label %1153, label %1159

; <label>:1153
  %1154 = fsub fast float %1123, %1128
  %1155 = fsub fast float %1131, %1127
  %1156 = fmul fast float %1155, 0x3F847AE140000000
  %1157 = fdiv fast float %1154, %1156
  %1158 = call float @dx.op.unary.f32(i32 7, float %1157)
  br label %1162

; <label>:1159
  %1160 = fcmp fast oge float %1123, %1132
  %1161 = select i1 %1160, float 1.000000e+00, float 0.000000e+00
  br label %1162

; <label>:1162
  %1163 = phi float [ %1158, %1153 ], [ %1161, %1159 ]
  %1164 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1165 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1166 = call float @dx.op.binary.f32(i32 36, float %1164, float 0x3FEFF7CEE0000000)
  %1167 = call float @dx.op.binary.f32(i32 36, float %1165, float 0x3FEFF7CEE0000000)
  %1168 = add i32 %911, 121
  %1169 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1168)
  %1170 = extractvalue %dx.types.CBufRet.f32 %1169, 0
  %1171 = extractvalue %dx.types.CBufRet.f32 %1169, 1
  %1172 = fadd fast float %1170, %1166
  %1173 = fadd fast float %1171, %1167
  %1174 = call float @dx.op.unary.f32(i32 22, float %1172)
  %1175 = call float @dx.op.unary.f32(i32 22, float %1173)
  %1176 = uitofp i32 %1121 to float
  %1177 = fmul fast float %1000, %1175
  %1178 = fmul fast float %1002, %1176
  %1179 = call float @dx.op.unary.f32(i32 27, float %1177)
  %1180 = fadd fast float %1179, %1174
  %1181 = fmul fast float %1180, %999
  %1182 = fadd fast float %1181, %1178
  %1183 = fptoui float %1182 to i32
  %1184 = icmp eq i32 %1183, %1097
  %1185 = and i1 %1096, %1184
  br i1 %1185, label %1189, label %1186

; <label>:1186
  %1187 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1183, i32 undef)
  %1188 = extractvalue %dx.types.ResRet.i32 %1187, 0
  br label %1189

; <label>:1189
  %1190 = phi i32 [ %1188, %1186 ], [ %1098, %1162 ]
  %1191 = and i32 %1190, 1
  %1192 = icmp eq i32 %1191, 0
  br i1 %1192, label %1201, label %1193

; <label>:1193
  %1194 = call float @dx.op.unary.f32(i32 7, float %907)
  %1195 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1174, float %1175, float %1176, float undef, i32 0, i32 0, i32 undef, float %1194)
  %1196 = extractvalue %dx.types.ResRet.f32 %1195, 0
  %1197 = fmul fast float %1163, %1118
  %1198 = fsub fast float 1.000000e+00, %1163
  %1199 = fmul fast float %1196, %1198
  %1200 = fadd fast float %1199, %1197
  br label %1201

; <label>:1201
  %1202 = phi float [ %1118, %1112 ], [ %1151, %1144 ], [ %1200, %1193 ], [ %1118, %1189 ]
  %1203 = phi i1 [ %1096, %1112 ], [ %1096, %1144 ], [ true, %1189 ], [ true, %1193 ]
  %1204 = phi i32 [ %1097, %1112 ], [ %1097, %1144 ], [ %1183, %1189 ], [ %1183, %1193 ]
  %1205 = phi i32 [ %1098, %1112 ], [ %1098, %1144 ], [ %1190, %1189 ], [ %1190, %1193 ]
  %1206 = fadd fast float %1202, %1095
  %1207 = select i1 %1117, float 1.000000e+00, float 0.000000e+00
  %1208 = fadd fast float %1207, %1099
  %1209 = call float @dx.op.binary.f32(i32 35, float %981, float 0.000000e+00)
  %1210 = call float @dx.op.binary.f32(i32 36, float %1209, float 0x3FEFF7CEE0000000)
  %1211 = fadd fast float %992, %1210
  %1212 = call float @dx.op.unary.f32(i32 22, float %1211)
  %1213 = fmul fast float %1000, %1212
  %1214 = call float @dx.op.unary.f32(i32 27, float %1213)
  %1215 = fadd fast float %1214, %995
  %1216 = fmul fast float %1215, %999
  %1217 = fadd fast float %1216, %1003
  %1218 = fptoui float %1217 to i32
  %1219 = icmp eq i32 %1218, %1107
  br i1 %1219, label %1223, label %1220

; <label>:1220
  %1221 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1218, i32 undef)
  %1222 = extractvalue %dx.types.ResRet.i32 %1221, 0
  br label %1223

; <label>:1223
  %1224 = phi i32 [ %1222, %1220 ], [ %1113, %1201 ]
  %1225 = and i32 %1224, 1
  %1226 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %995, float %1212, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)
  %1227 = extractvalue %dx.types.ResRet.f32 %1226, 0
  %1228 = icmp ne i32 %1225, 0
  %1229 = select i1 %1228, float %1227, float 1.000000e+00
  %1230 = icmp eq i32 %1225, 0
  br i1 %1230, label %1312, label %1231

; <label>:1231
  %1232 = add i32 %911, 1
  %1233 = icmp eq i32 %1232, %716
  %1234 = fsub fast float %997, %740
  %1235 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1236 = extractvalue %dx.types.CBufRet.i32 %1235, 1
  %1237 = and i32 %1236, 255
  %1238 = uitofp i32 %1237 to float
  %1239 = fmul fast float %1238, 0x3F847AE140000000
  %1240 = lshr i32 %1236, 8
  %1241 = and i32 %1240, 255
  %1242 = uitofp i32 %1241 to float
  %1243 = fmul fast float %1242, 0x3F847AE140000000
  %1244 = fcmp fast ogt float %1243, %1239
  br i1 %1233, label %1245, label %1263

; <label>:1245
  br i1 %1244, label %1246, label %1252

; <label>:1246
  %1247 = fsub fast float %1234, %1239
  %1248 = fsub fast float %1242, %1238
  %1249 = fmul fast float %1248, 0x3F847AE140000000
  %1250 = fdiv fast float %1247, %1249
  %1251 = call float @dx.op.unary.f32(i32 7, float %1250)
  br label %1255

; <label>:1252
  %1253 = fcmp fast oge float %1234, %1243
  %1254 = select i1 %1253, float 1.000000e+00, float 0.000000e+00
  br label %1255

; <label>:1255
  %1256 = phi float [ %1251, %1246 ], [ %1254, %1252 ]
  %1257 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1258 = extractvalue %dx.types.CBufRet.f32 %1257, 0
  %1259 = fmul fast float %1256, %1229
  %1260 = fsub fast float 1.000000e+00, %1256
  %1261 = fmul fast float %1258, %1260
  %1262 = fadd fast float %1261, %1259
  br label %1312

; <label>:1263
  br i1 %1244, label %1264, label %1270

; <label>:1264
  %1265 = fsub fast float %1234, %1239
  %1266 = fsub fast float %1242, %1238
  %1267 = fmul fast float %1266, 0x3F847AE140000000
  %1268 = fdiv fast float %1265, %1267
  %1269 = call float @dx.op.unary.f32(i32 7, float %1268)
  br label %1273

; <label>:1270
  %1271 = fcmp fast oge float %1234, %1243
  %1272 = select i1 %1271, float 1.000000e+00, float 0.000000e+00
  br label %1273

; <label>:1273
  %1274 = phi float [ %1269, %1264 ], [ %1272, %1270 ]
  %1275 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1276 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1277 = call float @dx.op.binary.f32(i32 36, float %1275, float 0x3FEFF7CEE0000000)
  %1278 = call float @dx.op.binary.f32(i32 36, float %1276, float 0x3FEFF7CEE0000000)
  %1279 = add i32 %911, 121
  %1280 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1279)
  %1281 = extractvalue %dx.types.CBufRet.f32 %1280, 0
  %1282 = extractvalue %dx.types.CBufRet.f32 %1280, 1
  %1283 = fadd fast float %1281, %1277
  %1284 = fadd fast float %1282, %1278
  %1285 = call float @dx.op.unary.f32(i32 22, float %1283)
  %1286 = call float @dx.op.unary.f32(i32 22, float %1284)
  %1287 = uitofp i32 %1232 to float
  %1288 = fmul fast float %1000, %1286
  %1289 = fmul fast float %1002, %1287
  %1290 = call float @dx.op.unary.f32(i32 27, float %1288)
  %1291 = fadd fast float %1290, %1285
  %1292 = fmul fast float %1291, %999
  %1293 = fadd fast float %1292, %1289
  %1294 = fptoui float %1293 to i32
  %1295 = icmp eq i32 %1294, %1204
  %1296 = and i1 %1203, %1295
  br i1 %1296, label %1300, label %1297

; <label>:1297
  %1298 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1294, i32 undef)
  %1299 = extractvalue %dx.types.ResRet.i32 %1298, 0
  br label %1300

; <label>:1300
  %1301 = phi i32 [ %1299, %1297 ], [ %1205, %1273 ]
  %1302 = and i32 %1301, 1
  %1303 = icmp eq i32 %1302, 0
  br i1 %1303, label %1312, label %1304

; <label>:1304
  %1305 = call float @dx.op.unary.f32(i32 7, float %907)
  %1306 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1285, float %1286, float %1287, float undef, i32 0, i32 0, i32 undef, float %1305)
  %1307 = extractvalue %dx.types.ResRet.f32 %1306, 0
  %1308 = fmul fast float %1274, %1229
  %1309 = fsub fast float 1.000000e+00, %1274
  %1310 = fmul fast float %1307, %1309
  %1311 = fadd fast float %1310, %1308
  br label %1312

; <label>:1312
  %1313 = phi float [ %1229, %1223 ], [ %1262, %1255 ], [ %1311, %1304 ], [ %1229, %1300 ]
  %1314 = phi i1 [ %1203, %1223 ], [ %1203, %1255 ], [ true, %1300 ], [ true, %1304 ]
  %1315 = phi i32 [ %1204, %1223 ], [ %1204, %1255 ], [ %1294, %1300 ], [ %1294, %1304 ]
  %1316 = phi i32 [ %1205, %1223 ], [ %1205, %1255 ], [ %1301, %1300 ], [ %1301, %1304 ]
  %1317 = fadd fast float %1206, %1313
  %1318 = select i1 %1228, float 1.000000e+00, float 0.000000e+00
  %1319 = fadd fast float %1208, %1318
  %1320 = fadd fast float %1214, %1103
  %1321 = fmul fast float %1320, %999
  %1322 = fadd fast float %1321, %1003
  %1323 = fptoui float %1322 to i32
  %1324 = icmp eq i32 %1323, %1218
  br i1 %1324, label %1328, label %1325

; <label>:1325
  %1326 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1323, i32 undef)
  %1327 = extractvalue %dx.types.ResRet.i32 %1326, 0
  br label %1328

; <label>:1328
  %1329 = phi i32 [ %1327, %1325 ], [ %1224, %1312 ]
  %1330 = and i32 %1329, 1
  %1331 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1103, float %1212, float %997, float undef, i32 0, i32 0, i32 undef, float %1009)
  %1332 = extractvalue %dx.types.ResRet.f32 %1331, 0
  %1333 = icmp ne i32 %1330, 0
  %1334 = select i1 %1333, float %1332, float 1.000000e+00
  %1335 = icmp eq i32 %1330, 0
  br i1 %1335, label %1417, label %1336

; <label>:1336
  %1337 = add i32 %911, 1
  %1338 = icmp eq i32 %1337, %716
  %1339 = fsub fast float %997, %740
  %1340 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1341 = extractvalue %dx.types.CBufRet.i32 %1340, 1
  %1342 = and i32 %1341, 255
  %1343 = uitofp i32 %1342 to float
  %1344 = fmul fast float %1343, 0x3F847AE140000000
  %1345 = lshr i32 %1341, 8
  %1346 = and i32 %1345, 255
  %1347 = uitofp i32 %1346 to float
  %1348 = fmul fast float %1347, 0x3F847AE140000000
  %1349 = fcmp fast ogt float %1348, %1344
  br i1 %1338, label %1350, label %1368

; <label>:1350
  br i1 %1349, label %1351, label %1357

; <label>:1351
  %1352 = fsub fast float %1339, %1344
  %1353 = fsub fast float %1347, %1343
  %1354 = fmul fast float %1353, 0x3F847AE140000000
  %1355 = fdiv fast float %1352, %1354
  %1356 = call float @dx.op.unary.f32(i32 7, float %1355)
  br label %1360

; <label>:1357
  %1358 = fcmp fast oge float %1339, %1348
  %1359 = select i1 %1358, float 1.000000e+00, float 0.000000e+00
  br label %1360

; <label>:1360
  %1361 = phi float [ %1356, %1351 ], [ %1359, %1357 ]
  %1362 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1363 = extractvalue %dx.types.CBufRet.f32 %1362, 0
  %1364 = fmul fast float %1361, %1334
  %1365 = fsub fast float 1.000000e+00, %1361
  %1366 = fmul fast float %1363, %1365
  %1367 = fadd fast float %1366, %1364
  br label %1417

; <label>:1368
  br i1 %1349, label %1369, label %1375

; <label>:1369
  %1370 = fsub fast float %1339, %1344
  %1371 = fsub fast float %1347, %1343
  %1372 = fmul fast float %1371, 0x3F847AE140000000
  %1373 = fdiv fast float %1370, %1372
  %1374 = call float @dx.op.unary.f32(i32 7, float %1373)
  br label %1378

; <label>:1375
  %1376 = fcmp fast oge float %1339, %1348
  %1377 = select i1 %1376, float 1.000000e+00, float 0.000000e+00
  br label %1378

; <label>:1378
  %1379 = phi float [ %1374, %1369 ], [ %1377, %1375 ]
  %1380 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1381 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1382 = call float @dx.op.binary.f32(i32 36, float %1380, float 0x3FEFF7CEE0000000)
  %1383 = call float @dx.op.binary.f32(i32 36, float %1381, float 0x3FEFF7CEE0000000)
  %1384 = add i32 %911, 121
  %1385 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1384)
  %1386 = extractvalue %dx.types.CBufRet.f32 %1385, 0
  %1387 = extractvalue %dx.types.CBufRet.f32 %1385, 1
  %1388 = fadd fast float %1386, %1382
  %1389 = fadd fast float %1387, %1383
  %1390 = call float @dx.op.unary.f32(i32 22, float %1388)
  %1391 = call float @dx.op.unary.f32(i32 22, float %1389)
  %1392 = uitofp i32 %1337 to float
  %1393 = fmul fast float %1000, %1391
  %1394 = fmul fast float %1002, %1392
  %1395 = call float @dx.op.unary.f32(i32 27, float %1393)
  %1396 = fadd fast float %1395, %1390
  %1397 = fmul fast float %1396, %999
  %1398 = fadd fast float %1397, %1394
  %1399 = fptoui float %1398 to i32
  %1400 = icmp eq i32 %1399, %1315
  %1401 = and i1 %1314, %1400
  br i1 %1401, label %1405, label %1402

; <label>:1402
  %1403 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1399, i32 undef)
  %1404 = extractvalue %dx.types.ResRet.i32 %1403, 0
  br label %1405

; <label>:1405
  %1406 = phi i32 [ %1404, %1402 ], [ %1316, %1378 ]
  %1407 = and i32 %1406, 1
  %1408 = icmp eq i32 %1407, 0
  br i1 %1408, label %1417, label %1409

; <label>:1409
  %1410 = call float @dx.op.unary.f32(i32 7, float %907)
  %1411 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1390, float %1391, float %1392, float undef, i32 0, i32 0, i32 undef, float %1410)
  %1412 = extractvalue %dx.types.ResRet.f32 %1411, 0
  %1413 = fmul fast float %1379, %1334
  %1414 = fsub fast float 1.000000e+00, %1379
  %1415 = fmul fast float %1412, %1414
  %1416 = fadd fast float %1415, %1413
  br label %1417

; <label>:1417
  %1418 = phi float [ %1334, %1328 ], [ %1367, %1360 ], [ %1416, %1409 ], [ %1334, %1405 ]
  %1419 = fadd fast float %1317, %1418
  %1420 = select i1 %1333, float 1.000000e+00, float 0.000000e+00
  %1421 = fadd fast float %1319, %1420
  %1422 = fcmp fast une float %1421, 0.000000e+00
  %1423 = fdiv fast float %1419, %1421
  %1424 = fsub fast float 1.000000e+00, %1423
  %1425 = select i1 %1422, float %1424, float 0.000000e+00
  br label %3294

; <label>:1426
  %1427 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 32)
  %1428 = extractvalue %dx.types.CBufRet.i32 %1427, 0
  %1429 = icmp eq i32 %1428, 0
  br i1 %1429, label %3181, label %1430

; <label>:1430
  %1431 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 138)
  %1432 = extractvalue %dx.types.CBufRet.f32 %1431, 0
  %1433 = extractvalue %dx.types.CBufRet.f32 %1431, 1
  %1434 = fmul fast float %1432, 1.500000e+00
  %1435 = fmul fast float %1433, 1.500000e+00
  %1436 = fsub fast float %908, %1434
  %1437 = fsub fast float %909, %1435
  %1438 = fcmp fast oge float %1436, 1.000000e+00
  %1439 = fmul fast float %1432, 3.000000e+00
  %1440 = fadd fast float %1436, %1439
  %1441 = fcmp fast ole float %1440, 0.000000e+00
  %1442 = or i1 %1438, %1441
  %1443 = fcmp fast oge float %1437, 1.000000e+00
  %1444 = or i1 %1443, %1442
  %1445 = fmul fast float %1433, 3.000000e+00
  %1446 = fadd fast float %1437, %1445
  %1447 = fcmp fast ole float %1446, 0.000000e+00
  %1448 = or i1 %1447, %1444
  br i1 %1448, label %3294, label %1449

; <label>:1449
  %1450 = call float @dx.op.binary.f32(i32 35, float %1436, float 0.000000e+00)
  %1451 = call float @dx.op.binary.f32(i32 35, float %1437, float 0.000000e+00)
  %1452 = call float @dx.op.binary.f32(i32 36, float %1450, float 0x3FEFF7CEE0000000)
  %1453 = call float @dx.op.binary.f32(i32 36, float %1451, float 0x3FEFF7CEE0000000)
  %1454 = add i32 %911, 120
  %1455 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1454)
  %1456 = extractvalue %dx.types.CBufRet.f32 %1455, 0
  %1457 = extractvalue %dx.types.CBufRet.f32 %1455, 1
  %1458 = fadd fast float %1456, %1452
  %1459 = fadd fast float %1457, %1453
  %1460 = call float @dx.op.unary.f32(i32 22, float %1458)
  %1461 = call float @dx.op.unary.f32(i32 22, float %1459)
  %1462 = uitofp i32 %911 to float
  %1463 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)
  %1464 = extractvalue %dx.types.CBufRet.f32 %1463, 0
  %1465 = extractvalue %dx.types.CBufRet.f32 %1463, 1
  %1466 = fmul fast float %1465, %1461
  %1467 = extractvalue %dx.types.CBufRet.f32 %1463, 2
  %1468 = fmul fast float %1467, %1462
  %1469 = call float @dx.op.unary.f32(i32 27, float %1466)
  %1470 = fadd fast float %1469, %1460
  %1471 = fmul fast float %1470, %1464
  %1472 = fadd fast float %1471, %1468
  %1473 = fptoui float %1472 to i32
  %1474 = call float @dx.op.unary.f32(i32 7, float %910)
  %1475 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1473, i32 undef)
  %1476 = extractvalue %dx.types.ResRet.i32 %1475, 0
  %1477 = and i32 %1476, 1
  %1478 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1460, float %1461, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %1479 = extractvalue %dx.types.ResRet.f32 %1478, 0
  %1480 = icmp ne i32 %1477, 0
  %1481 = select i1 %1480, float %1479, float 1.000000e+00
  %1482 = icmp eq i32 %1477, 0
  br i1 %1482, label %1559, label %1483

; <label>:1483
  %1484 = add i32 %911, 1
  %1485 = icmp eq i32 %1484, %716
  %1486 = fsub fast float %1462, %740
  %1487 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1488 = extractvalue %dx.types.CBufRet.i32 %1487, 1
  %1489 = and i32 %1488, 255
  %1490 = uitofp i32 %1489 to float
  %1491 = fmul fast float %1490, 0x3F847AE140000000
  %1492 = lshr i32 %1488, 8
  %1493 = and i32 %1492, 255
  %1494 = uitofp i32 %1493 to float
  %1495 = fmul fast float %1494, 0x3F847AE140000000
  %1496 = fcmp fast ogt float %1495, %1491
  br i1 %1485, label %1497, label %1515

; <label>:1497
  br i1 %1496, label %1498, label %1504

; <label>:1498
  %1499 = fsub fast float %1486, %1491
  %1500 = fsub fast float %1494, %1490
  %1501 = fmul fast float %1500, 0x3F847AE140000000
  %1502 = fdiv fast float %1499, %1501
  %1503 = call float @dx.op.unary.f32(i32 7, float %1502)
  br label %1507

; <label>:1504
  %1505 = fcmp fast oge float %1486, %1495
  %1506 = select i1 %1505, float 1.000000e+00, float 0.000000e+00
  br label %1507

; <label>:1507
  %1508 = phi float [ %1503, %1498 ], [ %1506, %1504 ]
  %1509 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1510 = extractvalue %dx.types.CBufRet.f32 %1509, 0
  %1511 = fmul fast float %1508, %1481
  %1512 = fsub fast float 1.000000e+00, %1508
  %1513 = fmul fast float %1510, %1512
  %1514 = fadd fast float %1513, %1511
  br label %1559

; <label>:1515
  br i1 %1496, label %1516, label %1522

; <label>:1516
  %1517 = fsub fast float %1486, %1491
  %1518 = fsub fast float %1494, %1490
  %1519 = fmul fast float %1518, 0x3F847AE140000000
  %1520 = fdiv fast float %1517, %1519
  %1521 = call float @dx.op.unary.f32(i32 7, float %1520)
  br label %1525

; <label>:1522
  %1523 = fcmp fast oge float %1486, %1495
  %1524 = select i1 %1523, float 1.000000e+00, float 0.000000e+00
  br label %1525

; <label>:1525
  %1526 = phi float [ %1521, %1516 ], [ %1524, %1522 ]
  %1527 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1528 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1529 = call float @dx.op.binary.f32(i32 36, float %1527, float 0x3FEFF7CEE0000000)
  %1530 = call float @dx.op.binary.f32(i32 36, float %1528, float 0x3FEFF7CEE0000000)
  %1531 = add i32 %911, 121
  %1532 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1531)
  %1533 = extractvalue %dx.types.CBufRet.f32 %1532, 0
  %1534 = extractvalue %dx.types.CBufRet.f32 %1532, 1
  %1535 = fadd fast float %1533, %1529
  %1536 = fadd fast float %1534, %1530
  %1537 = call float @dx.op.unary.f32(i32 22, float %1535)
  %1538 = call float @dx.op.unary.f32(i32 22, float %1536)
  %1539 = uitofp i32 %1484 to float
  %1540 = fmul fast float %1465, %1538
  %1541 = fmul fast float %1467, %1539
  %1542 = call float @dx.op.unary.f32(i32 27, float %1540)
  %1543 = fadd fast float %1542, %1537
  %1544 = fmul fast float %1543, %1464
  %1545 = fadd fast float %1544, %1541
  %1546 = fptoui float %1545 to i32
  %1547 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1546, i32 undef)
  %1548 = extractvalue %dx.types.ResRet.i32 %1547, 0
  %1549 = and i32 %1548, 1
  %1550 = icmp eq i32 %1549, 0
  br i1 %1550, label %1559, label %1551

; <label>:1551
  %1552 = call float @dx.op.unary.f32(i32 7, float %907)
  %1553 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1537, float %1538, float %1539, float undef, i32 0, i32 0, i32 undef, float %1552)
  %1554 = extractvalue %dx.types.ResRet.f32 %1553, 0
  %1555 = fmul fast float %1526, %1481
  %1556 = fsub fast float 1.000000e+00, %1526
  %1557 = fmul fast float %1554, %1556
  %1558 = fadd fast float %1557, %1555
  br label %1559

; <label>:1559
  %1560 = phi float [ %1481, %1449 ], [ %1514, %1507 ], [ %1558, %1551 ], [ %1481, %1525 ]
  %1561 = phi i1 [ false, %1449 ], [ false, %1507 ], [ true, %1525 ], [ true, %1551 ]
  %1562 = phi i32 [ 0, %1449 ], [ 0, %1507 ], [ %1546, %1525 ], [ %1546, %1551 ]
  %1563 = phi i32 [ 0, %1449 ], [ 0, %1507 ], [ %1548, %1525 ], [ %1548, %1551 ]
  %1564 = select i1 %1480, float 1.000000e+00, float 0.000000e+00
  %1565 = fadd fast float %1436, %1432
  %1566 = call float @dx.op.binary.f32(i32 35, float %1565, float 0.000000e+00)
  %1567 = call float @dx.op.binary.f32(i32 36, float %1566, float 0x3FEFF7CEE0000000)
  %1568 = fadd fast float %1456, %1567
  %1569 = call float @dx.op.unary.f32(i32 22, float %1568)
  %1570 = fadd fast float %1469, %1569
  %1571 = fmul fast float %1570, %1464
  %1572 = fadd fast float %1571, %1468
  %1573 = fptoui float %1572 to i32
  %1574 = icmp eq i32 %1573, %1473
  br i1 %1574, label %1578, label %1575

; <label>:1575
  %1576 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1573, i32 undef)
  %1577 = extractvalue %dx.types.ResRet.i32 %1576, 0
  br label %1578

; <label>:1578
  %1579 = phi i32 [ %1577, %1575 ], [ %1476, %1559 ]
  %1580 = and i32 %1579, 1
  %1581 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1569, float %1461, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %1582 = extractvalue %dx.types.ResRet.f32 %1581, 0
  %1583 = icmp ne i32 %1580, 0
  %1584 = select i1 %1583, float %1582, float 1.000000e+00
  %1585 = icmp eq i32 %1580, 0
  br i1 %1585, label %1667, label %1586

; <label>:1586
  %1587 = add i32 %911, 1
  %1588 = icmp eq i32 %1587, %716
  %1589 = fsub fast float %1462, %740
  %1590 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1591 = extractvalue %dx.types.CBufRet.i32 %1590, 1
  %1592 = and i32 %1591, 255
  %1593 = uitofp i32 %1592 to float
  %1594 = fmul fast float %1593, 0x3F847AE140000000
  %1595 = lshr i32 %1591, 8
  %1596 = and i32 %1595, 255
  %1597 = uitofp i32 %1596 to float
  %1598 = fmul fast float %1597, 0x3F847AE140000000
  %1599 = fcmp fast ogt float %1598, %1594
  br i1 %1588, label %1600, label %1618

; <label>:1600
  br i1 %1599, label %1601, label %1607

; <label>:1601
  %1602 = fsub fast float %1589, %1594
  %1603 = fsub fast float %1597, %1593
  %1604 = fmul fast float %1603, 0x3F847AE140000000
  %1605 = fdiv fast float %1602, %1604
  %1606 = call float @dx.op.unary.f32(i32 7, float %1605)
  br label %1610

; <label>:1607
  %1608 = fcmp fast oge float %1589, %1598
  %1609 = select i1 %1608, float 1.000000e+00, float 0.000000e+00
  br label %1610

; <label>:1610
  %1611 = phi float [ %1606, %1601 ], [ %1609, %1607 ]
  %1612 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1613 = extractvalue %dx.types.CBufRet.f32 %1612, 0
  %1614 = fmul fast float %1611, %1584
  %1615 = fsub fast float 1.000000e+00, %1611
  %1616 = fmul fast float %1613, %1615
  %1617 = fadd fast float %1616, %1614
  br label %1667

; <label>:1618
  br i1 %1599, label %1619, label %1625

; <label>:1619
  %1620 = fsub fast float %1589, %1594
  %1621 = fsub fast float %1597, %1593
  %1622 = fmul fast float %1621, 0x3F847AE140000000
  %1623 = fdiv fast float %1620, %1622
  %1624 = call float @dx.op.unary.f32(i32 7, float %1623)
  br label %1628

; <label>:1625
  %1626 = fcmp fast oge float %1589, %1598
  %1627 = select i1 %1626, float 1.000000e+00, float 0.000000e+00
  br label %1628

; <label>:1628
  %1629 = phi float [ %1624, %1619 ], [ %1627, %1625 ]
  %1630 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1631 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1632 = call float @dx.op.binary.f32(i32 36, float %1630, float 0x3FEFF7CEE0000000)
  %1633 = call float @dx.op.binary.f32(i32 36, float %1631, float 0x3FEFF7CEE0000000)
  %1634 = add i32 %911, 121
  %1635 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1634)
  %1636 = extractvalue %dx.types.CBufRet.f32 %1635, 0
  %1637 = extractvalue %dx.types.CBufRet.f32 %1635, 1
  %1638 = fadd fast float %1636, %1632
  %1639 = fadd fast float %1637, %1633
  %1640 = call float @dx.op.unary.f32(i32 22, float %1638)
  %1641 = call float @dx.op.unary.f32(i32 22, float %1639)
  %1642 = uitofp i32 %1587 to float
  %1643 = fmul fast float %1465, %1641
  %1644 = fmul fast float %1467, %1642
  %1645 = call float @dx.op.unary.f32(i32 27, float %1643)
  %1646 = fadd fast float %1645, %1640
  %1647 = fmul fast float %1646, %1464
  %1648 = fadd fast float %1647, %1644
  %1649 = fptoui float %1648 to i32
  %1650 = icmp eq i32 %1649, %1562
  %1651 = and i1 %1561, %1650
  br i1 %1651, label %1655, label %1652

; <label>:1652
  %1653 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1649, i32 undef)
  %1654 = extractvalue %dx.types.ResRet.i32 %1653, 0
  br label %1655

; <label>:1655
  %1656 = phi i32 [ %1654, %1652 ], [ %1563, %1628 ]
  %1657 = and i32 %1656, 1
  %1658 = icmp eq i32 %1657, 0
  br i1 %1658, label %1667, label %1659

; <label>:1659
  %1660 = call float @dx.op.unary.f32(i32 7, float %907)
  %1661 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1640, float %1641, float %1642, float undef, i32 0, i32 0, i32 undef, float %1660)
  %1662 = extractvalue %dx.types.ResRet.f32 %1661, 0
  %1663 = fmul fast float %1629, %1584
  %1664 = fsub fast float 1.000000e+00, %1629
  %1665 = fmul fast float %1662, %1664
  %1666 = fadd fast float %1665, %1663
  br label %1667

; <label>:1667
  %1668 = phi float [ %1584, %1578 ], [ %1617, %1610 ], [ %1666, %1659 ], [ %1584, %1655 ]
  %1669 = phi i1 [ %1561, %1578 ], [ %1561, %1610 ], [ true, %1655 ], [ true, %1659 ]
  %1670 = phi i32 [ %1562, %1578 ], [ %1562, %1610 ], [ %1649, %1655 ], [ %1649, %1659 ]
  %1671 = phi i32 [ %1563, %1578 ], [ %1563, %1610 ], [ %1656, %1655 ], [ %1656, %1659 ]
  %1672 = select i1 %1583, float 1.000000e+00, float 0.000000e+00
  %1673 = fadd fast float %1672, %1564
  %1674 = fadd fast float %1565, %1432
  %1675 = call float @dx.op.binary.f32(i32 35, float %1674, float 0.000000e+00)
  %1676 = call float @dx.op.binary.f32(i32 36, float %1675, float 0x3FEFF7CEE0000000)
  %1677 = fadd fast float %1456, %1676
  %1678 = call float @dx.op.unary.f32(i32 22, float %1677)
  %1679 = fadd fast float %1469, %1678
  %1680 = fmul fast float %1679, %1464
  %1681 = fadd fast float %1680, %1468
  %1682 = fptoui float %1681 to i32
  %1683 = icmp eq i32 %1682, %1573
  br i1 %1683, label %1687, label %1684

; <label>:1684
  %1685 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1682, i32 undef)
  %1686 = extractvalue %dx.types.ResRet.i32 %1685, 0
  br label %1687

; <label>:1687
  %1688 = phi i32 [ %1686, %1684 ], [ %1579, %1667 ]
  %1689 = and i32 %1688, 1
  %1690 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1678, float %1461, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %1691 = extractvalue %dx.types.ResRet.f32 %1690, 0
  %1692 = icmp ne i32 %1689, 0
  %1693 = select i1 %1692, float %1691, float 1.000000e+00
  %1694 = icmp eq i32 %1689, 0
  br i1 %1694, label %1776, label %1695

; <label>:1695
  %1696 = add i32 %911, 1
  %1697 = icmp eq i32 %1696, %716
  %1698 = fsub fast float %1462, %740
  %1699 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1700 = extractvalue %dx.types.CBufRet.i32 %1699, 1
  %1701 = and i32 %1700, 255
  %1702 = uitofp i32 %1701 to float
  %1703 = fmul fast float %1702, 0x3F847AE140000000
  %1704 = lshr i32 %1700, 8
  %1705 = and i32 %1704, 255
  %1706 = uitofp i32 %1705 to float
  %1707 = fmul fast float %1706, 0x3F847AE140000000
  %1708 = fcmp fast ogt float %1707, %1703
  br i1 %1697, label %1709, label %1727

; <label>:1709
  br i1 %1708, label %1710, label %1716

; <label>:1710
  %1711 = fsub fast float %1698, %1703
  %1712 = fsub fast float %1706, %1702
  %1713 = fmul fast float %1712, 0x3F847AE140000000
  %1714 = fdiv fast float %1711, %1713
  %1715 = call float @dx.op.unary.f32(i32 7, float %1714)
  br label %1719

; <label>:1716
  %1717 = fcmp fast oge float %1698, %1707
  %1718 = select i1 %1717, float 1.000000e+00, float 0.000000e+00
  br label %1719

; <label>:1719
  %1720 = phi float [ %1715, %1710 ], [ %1718, %1716 ]
  %1721 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1722 = extractvalue %dx.types.CBufRet.f32 %1721, 0
  %1723 = fmul fast float %1720, %1693
  %1724 = fsub fast float 1.000000e+00, %1720
  %1725 = fmul fast float %1722, %1724
  %1726 = fadd fast float %1725, %1723
  br label %1776

; <label>:1727
  br i1 %1708, label %1728, label %1734

; <label>:1728
  %1729 = fsub fast float %1698, %1703
  %1730 = fsub fast float %1706, %1702
  %1731 = fmul fast float %1730, 0x3F847AE140000000
  %1732 = fdiv fast float %1729, %1731
  %1733 = call float @dx.op.unary.f32(i32 7, float %1732)
  br label %1737

; <label>:1734
  %1735 = fcmp fast oge float %1698, %1707
  %1736 = select i1 %1735, float 1.000000e+00, float 0.000000e+00
  br label %1737

; <label>:1737
  %1738 = phi float [ %1733, %1728 ], [ %1736, %1734 ]
  %1739 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1740 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1741 = call float @dx.op.binary.f32(i32 36, float %1739, float 0x3FEFF7CEE0000000)
  %1742 = call float @dx.op.binary.f32(i32 36, float %1740, float 0x3FEFF7CEE0000000)
  %1743 = add i32 %911, 121
  %1744 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1743)
  %1745 = extractvalue %dx.types.CBufRet.f32 %1744, 0
  %1746 = extractvalue %dx.types.CBufRet.f32 %1744, 1
  %1747 = fadd fast float %1745, %1741
  %1748 = fadd fast float %1746, %1742
  %1749 = call float @dx.op.unary.f32(i32 22, float %1747)
  %1750 = call float @dx.op.unary.f32(i32 22, float %1748)
  %1751 = uitofp i32 %1696 to float
  %1752 = fmul fast float %1465, %1750
  %1753 = fmul fast float %1467, %1751
  %1754 = call float @dx.op.unary.f32(i32 27, float %1752)
  %1755 = fadd fast float %1754, %1749
  %1756 = fmul fast float %1755, %1464
  %1757 = fadd fast float %1756, %1753
  %1758 = fptoui float %1757 to i32
  %1759 = icmp eq i32 %1758, %1670
  %1760 = and i1 %1669, %1759
  br i1 %1760, label %1764, label %1761

; <label>:1761
  %1762 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1758, i32 undef)
  %1763 = extractvalue %dx.types.ResRet.i32 %1762, 0
  br label %1764

; <label>:1764
  %1765 = phi i32 [ %1763, %1761 ], [ %1671, %1737 ]
  %1766 = and i32 %1765, 1
  %1767 = icmp eq i32 %1766, 0
  br i1 %1767, label %1776, label %1768

; <label>:1768
  %1769 = call float @dx.op.unary.f32(i32 7, float %907)
  %1770 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1749, float %1750, float %1751, float undef, i32 0, i32 0, i32 undef, float %1769)
  %1771 = extractvalue %dx.types.ResRet.f32 %1770, 0
  %1772 = fmul fast float %1738, %1693
  %1773 = fsub fast float 1.000000e+00, %1738
  %1774 = fmul fast float %1771, %1773
  %1775 = fadd fast float %1774, %1772
  br label %1776

; <label>:1776
  %1777 = phi float [ %1693, %1687 ], [ %1726, %1719 ], [ %1775, %1768 ], [ %1693, %1764 ]
  %1778 = phi i1 [ %1669, %1687 ], [ %1669, %1719 ], [ true, %1764 ], [ true, %1768 ]
  %1779 = phi i32 [ %1670, %1687 ], [ %1670, %1719 ], [ %1758, %1764 ], [ %1758, %1768 ]
  %1780 = phi i32 [ %1671, %1687 ], [ %1671, %1719 ], [ %1765, %1764 ], [ %1765, %1768 ]
  %1781 = select i1 %1692, float 1.000000e+00, float 0.000000e+00
  %1782 = fadd fast float %1673, %1781
  %1783 = fadd fast float %1674, %1432
  %1784 = call float @dx.op.binary.f32(i32 35, float %1783, float 0.000000e+00)
  %1785 = call float @dx.op.binary.f32(i32 36, float %1784, float 0x3FEFF7CEE0000000)
  %1786 = fadd fast float %1456, %1785
  %1787 = call float @dx.op.unary.f32(i32 22, float %1786)
  %1788 = fadd fast float %1469, %1787
  %1789 = fmul fast float %1788, %1464
  %1790 = fadd fast float %1789, %1468
  %1791 = fptoui float %1790 to i32
  %1792 = icmp eq i32 %1791, %1682
  br i1 %1792, label %1796, label %1793

; <label>:1793
  %1794 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1791, i32 undef)
  %1795 = extractvalue %dx.types.ResRet.i32 %1794, 0
  br label %1796

; <label>:1796
  %1797 = phi i32 [ %1795, %1793 ], [ %1688, %1776 ]
  %1798 = and i32 %1797, 1
  %1799 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1787, float %1461, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %1800 = extractvalue %dx.types.ResRet.f32 %1799, 0
  %1801 = icmp ne i32 %1798, 0
  %1802 = select i1 %1801, float %1800, float 1.000000e+00
  %1803 = icmp eq i32 %1798, 0
  br i1 %1803, label %1885, label %1804

; <label>:1804
  %1805 = add i32 %911, 1
  %1806 = icmp eq i32 %1805, %716
  %1807 = fsub fast float %1462, %740
  %1808 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1809 = extractvalue %dx.types.CBufRet.i32 %1808, 1
  %1810 = and i32 %1809, 255
  %1811 = uitofp i32 %1810 to float
  %1812 = fmul fast float %1811, 0x3F847AE140000000
  %1813 = lshr i32 %1809, 8
  %1814 = and i32 %1813, 255
  %1815 = uitofp i32 %1814 to float
  %1816 = fmul fast float %1815, 0x3F847AE140000000
  %1817 = fcmp fast ogt float %1816, %1812
  br i1 %1806, label %1818, label %1836

; <label>:1818
  br i1 %1817, label %1819, label %1825

; <label>:1819
  %1820 = fsub fast float %1807, %1812
  %1821 = fsub fast float %1815, %1811
  %1822 = fmul fast float %1821, 0x3F847AE140000000
  %1823 = fdiv fast float %1820, %1822
  %1824 = call float @dx.op.unary.f32(i32 7, float %1823)
  br label %1828

; <label>:1825
  %1826 = fcmp fast oge float %1807, %1816
  %1827 = select i1 %1826, float 1.000000e+00, float 0.000000e+00
  br label %1828

; <label>:1828
  %1829 = phi float [ %1824, %1819 ], [ %1827, %1825 ]
  %1830 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1831 = extractvalue %dx.types.CBufRet.f32 %1830, 0
  %1832 = fmul fast float %1829, %1802
  %1833 = fsub fast float 1.000000e+00, %1829
  %1834 = fmul fast float %1831, %1833
  %1835 = fadd fast float %1834, %1832
  br label %1885

; <label>:1836
  br i1 %1817, label %1837, label %1843

; <label>:1837
  %1838 = fsub fast float %1807, %1812
  %1839 = fsub fast float %1815, %1811
  %1840 = fmul fast float %1839, 0x3F847AE140000000
  %1841 = fdiv fast float %1838, %1840
  %1842 = call float @dx.op.unary.f32(i32 7, float %1841)
  br label %1846

; <label>:1843
  %1844 = fcmp fast oge float %1807, %1816
  %1845 = select i1 %1844, float 1.000000e+00, float 0.000000e+00
  br label %1846

; <label>:1846
  %1847 = phi float [ %1842, %1837 ], [ %1845, %1843 ]
  %1848 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1849 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1850 = call float @dx.op.binary.f32(i32 36, float %1848, float 0x3FEFF7CEE0000000)
  %1851 = call float @dx.op.binary.f32(i32 36, float %1849, float 0x3FEFF7CEE0000000)
  %1852 = add i32 %911, 121
  %1853 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1852)
  %1854 = extractvalue %dx.types.CBufRet.f32 %1853, 0
  %1855 = extractvalue %dx.types.CBufRet.f32 %1853, 1
  %1856 = fadd fast float %1854, %1850
  %1857 = fadd fast float %1855, %1851
  %1858 = call float @dx.op.unary.f32(i32 22, float %1856)
  %1859 = call float @dx.op.unary.f32(i32 22, float %1857)
  %1860 = uitofp i32 %1805 to float
  %1861 = fmul fast float %1465, %1859
  %1862 = fmul fast float %1467, %1860
  %1863 = call float @dx.op.unary.f32(i32 27, float %1861)
  %1864 = fadd fast float %1863, %1858
  %1865 = fmul fast float %1864, %1464
  %1866 = fadd fast float %1865, %1862
  %1867 = fptoui float %1866 to i32
  %1868 = icmp eq i32 %1867, %1779
  %1869 = and i1 %1778, %1868
  br i1 %1869, label %1873, label %1870

; <label>:1870
  %1871 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1867, i32 undef)
  %1872 = extractvalue %dx.types.ResRet.i32 %1871, 0
  br label %1873

; <label>:1873
  %1874 = phi i32 [ %1872, %1870 ], [ %1780, %1846 ]
  %1875 = and i32 %1874, 1
  %1876 = icmp eq i32 %1875, 0
  br i1 %1876, label %1885, label %1877

; <label>:1877
  %1878 = call float @dx.op.unary.f32(i32 7, float %907)
  %1879 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1858, float %1859, float %1860, float undef, i32 0, i32 0, i32 undef, float %1878)
  %1880 = extractvalue %dx.types.ResRet.f32 %1879, 0
  %1881 = fmul fast float %1847, %1802
  %1882 = fsub fast float 1.000000e+00, %1847
  %1883 = fmul fast float %1880, %1882
  %1884 = fadd fast float %1883, %1881
  br label %1885

; <label>:1885
  %1886 = phi float [ %1802, %1796 ], [ %1835, %1828 ], [ %1884, %1877 ], [ %1802, %1873 ]
  %1887 = phi i1 [ %1778, %1796 ], [ %1778, %1828 ], [ true, %1873 ], [ true, %1877 ]
  %1888 = phi i32 [ %1779, %1796 ], [ %1779, %1828 ], [ %1867, %1873 ], [ %1867, %1877 ]
  %1889 = phi i32 [ %1780, %1796 ], [ %1780, %1828 ], [ %1874, %1873 ], [ %1874, %1877 ]
  %1890 = select i1 %1801, float 1.000000e+00, float 0.000000e+00
  %1891 = fadd fast float %1782, %1890
  %1892 = fadd fast float %1437, %1433
  %1893 = call float @dx.op.binary.f32(i32 35, float %1892, float 0.000000e+00)
  %1894 = call float @dx.op.binary.f32(i32 36, float %1893, float 0x3FEFF7CEE0000000)
  %1895 = fadd fast float %1457, %1894
  %1896 = call float @dx.op.unary.f32(i32 22, float %1895)
  %1897 = fmul fast float %1465, %1896
  %1898 = call float @dx.op.unary.f32(i32 27, float %1897)
  %1899 = fadd fast float %1898, %1460
  %1900 = fmul fast float %1899, %1464
  %1901 = fadd fast float %1900, %1468
  %1902 = fptoui float %1901 to i32
  %1903 = icmp eq i32 %1902, %1791
  br i1 %1903, label %1907, label %1904

; <label>:1904
  %1905 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1902, i32 undef)
  %1906 = extractvalue %dx.types.ResRet.i32 %1905, 0
  br label %1907

; <label>:1907
  %1908 = phi i32 [ %1906, %1904 ], [ %1797, %1885 ]
  %1909 = and i32 %1908, 1
  %1910 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1460, float %1896, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %1911 = extractvalue %dx.types.ResRet.f32 %1910, 0
  %1912 = icmp ne i32 %1909, 0
  %1913 = select i1 %1912, float %1911, float 1.000000e+00
  %1914 = icmp eq i32 %1909, 0
  br i1 %1914, label %1996, label %1915

; <label>:1915
  %1916 = add i32 %911, 1
  %1917 = icmp eq i32 %1916, %716
  %1918 = fsub fast float %1462, %740
  %1919 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %1920 = extractvalue %dx.types.CBufRet.i32 %1919, 1
  %1921 = and i32 %1920, 255
  %1922 = uitofp i32 %1921 to float
  %1923 = fmul fast float %1922, 0x3F847AE140000000
  %1924 = lshr i32 %1920, 8
  %1925 = and i32 %1924, 255
  %1926 = uitofp i32 %1925 to float
  %1927 = fmul fast float %1926, 0x3F847AE140000000
  %1928 = fcmp fast ogt float %1927, %1923
  br i1 %1917, label %1929, label %1947

; <label>:1929
  br i1 %1928, label %1930, label %1936

; <label>:1930
  %1931 = fsub fast float %1918, %1923
  %1932 = fsub fast float %1926, %1922
  %1933 = fmul fast float %1932, 0x3F847AE140000000
  %1934 = fdiv fast float %1931, %1933
  %1935 = call float @dx.op.unary.f32(i32 7, float %1934)
  br label %1939

; <label>:1936
  %1937 = fcmp fast oge float %1918, %1927
  %1938 = select i1 %1937, float 1.000000e+00, float 0.000000e+00
  br label %1939

; <label>:1939
  %1940 = phi float [ %1935, %1930 ], [ %1938, %1936 ]
  %1941 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %1942 = extractvalue %dx.types.CBufRet.f32 %1941, 0
  %1943 = fmul fast float %1940, %1913
  %1944 = fsub fast float 1.000000e+00, %1940
  %1945 = fmul fast float %1942, %1944
  %1946 = fadd fast float %1945, %1943
  br label %1996

; <label>:1947
  br i1 %1928, label %1948, label %1954

; <label>:1948
  %1949 = fsub fast float %1918, %1923
  %1950 = fsub fast float %1926, %1922
  %1951 = fmul fast float %1950, 0x3F847AE140000000
  %1952 = fdiv fast float %1949, %1951
  %1953 = call float @dx.op.unary.f32(i32 7, float %1952)
  br label %1957

; <label>:1954
  %1955 = fcmp fast oge float %1918, %1927
  %1956 = select i1 %1955, float 1.000000e+00, float 0.000000e+00
  br label %1957

; <label>:1957
  %1958 = phi float [ %1953, %1948 ], [ %1956, %1954 ]
  %1959 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %1960 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %1961 = call float @dx.op.binary.f32(i32 36, float %1959, float 0x3FEFF7CEE0000000)
  %1962 = call float @dx.op.binary.f32(i32 36, float %1960, float 0x3FEFF7CEE0000000)
  %1963 = add i32 %911, 121
  %1964 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %1963)
  %1965 = extractvalue %dx.types.CBufRet.f32 %1964, 0
  %1966 = extractvalue %dx.types.CBufRet.f32 %1964, 1
  %1967 = fadd fast float %1965, %1961
  %1968 = fadd fast float %1966, %1962
  %1969 = call float @dx.op.unary.f32(i32 22, float %1967)
  %1970 = call float @dx.op.unary.f32(i32 22, float %1968)
  %1971 = uitofp i32 %1916 to float
  %1972 = fmul fast float %1465, %1970
  %1973 = fmul fast float %1467, %1971
  %1974 = call float @dx.op.unary.f32(i32 27, float %1972)
  %1975 = fadd fast float %1974, %1969
  %1976 = fmul fast float %1975, %1464
  %1977 = fadd fast float %1976, %1973
  %1978 = fptoui float %1977 to i32
  %1979 = icmp eq i32 %1978, %1888
  %1980 = and i1 %1887, %1979
  br i1 %1980, label %1984, label %1981

; <label>:1981
  %1982 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %1978, i32 undef)
  %1983 = extractvalue %dx.types.ResRet.i32 %1982, 0
  br label %1984

; <label>:1984
  %1985 = phi i32 [ %1983, %1981 ], [ %1889, %1957 ]
  %1986 = and i32 %1985, 1
  %1987 = icmp eq i32 %1986, 0
  br i1 %1987, label %1996, label %1988

; <label>:1988
  %1989 = call float @dx.op.unary.f32(i32 7, float %907)
  %1990 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1969, float %1970, float %1971, float undef, i32 0, i32 0, i32 undef, float %1989)
  %1991 = extractvalue %dx.types.ResRet.f32 %1990, 0
  %1992 = fmul fast float %1958, %1913
  %1993 = fsub fast float 1.000000e+00, %1958
  %1994 = fmul fast float %1991, %1993
  %1995 = fadd fast float %1994, %1992
  br label %1996

; <label>:1996
  %1997 = phi float [ %1913, %1907 ], [ %1946, %1939 ], [ %1995, %1988 ], [ %1913, %1984 ]
  %1998 = phi i1 [ %1887, %1907 ], [ %1887, %1939 ], [ true, %1984 ], [ true, %1988 ]
  %1999 = phi i32 [ %1888, %1907 ], [ %1888, %1939 ], [ %1978, %1984 ], [ %1978, %1988 ]
  %2000 = phi i32 [ %1889, %1907 ], [ %1889, %1939 ], [ %1985, %1984 ], [ %1985, %1988 ]
  %2001 = select i1 %1912, float 1.000000e+00, float 0.000000e+00
  %2002 = fadd fast float %1891, %2001
  %2003 = fadd fast float %1898, %1569
  %2004 = fmul fast float %2003, %1464
  %2005 = fadd fast float %2004, %1468
  %2006 = fptoui float %2005 to i32
  %2007 = icmp eq i32 %2006, %1902
  br i1 %2007, label %2011, label %2008

; <label>:2008
  %2009 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2006, i32 undef)
  %2010 = extractvalue %dx.types.ResRet.i32 %2009, 0
  br label %2011

; <label>:2011
  %2012 = phi i32 [ %2010, %2008 ], [ %1908, %1996 ]
  %2013 = and i32 %2012, 1
  %2014 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1569, float %1896, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2015 = extractvalue %dx.types.ResRet.f32 %2014, 0
  %2016 = icmp ne i32 %2013, 0
  %2017 = select i1 %2016, float %2015, float 1.000000e+00
  %2018 = icmp eq i32 %2013, 0
  br i1 %2018, label %2100, label %2019

; <label>:2019
  %2020 = add i32 %911, 1
  %2021 = icmp eq i32 %2020, %716
  %2022 = fsub fast float %1462, %740
  %2023 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2024 = extractvalue %dx.types.CBufRet.i32 %2023, 1
  %2025 = and i32 %2024, 255
  %2026 = uitofp i32 %2025 to float
  %2027 = fmul fast float %2026, 0x3F847AE140000000
  %2028 = lshr i32 %2024, 8
  %2029 = and i32 %2028, 255
  %2030 = uitofp i32 %2029 to float
  %2031 = fmul fast float %2030, 0x3F847AE140000000
  %2032 = fcmp fast ogt float %2031, %2027
  br i1 %2021, label %2033, label %2051

; <label>:2033
  br i1 %2032, label %2034, label %2040

; <label>:2034
  %2035 = fsub fast float %2022, %2027
  %2036 = fsub fast float %2030, %2026
  %2037 = fmul fast float %2036, 0x3F847AE140000000
  %2038 = fdiv fast float %2035, %2037
  %2039 = call float @dx.op.unary.f32(i32 7, float %2038)
  br label %2043

; <label>:2040
  %2041 = fcmp fast oge float %2022, %2031
  %2042 = select i1 %2041, float 1.000000e+00, float 0.000000e+00
  br label %2043

; <label>:2043
  %2044 = phi float [ %2039, %2034 ], [ %2042, %2040 ]
  %2045 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2046 = extractvalue %dx.types.CBufRet.f32 %2045, 0
  %2047 = fmul fast float %2044, %2017
  %2048 = fsub fast float 1.000000e+00, %2044
  %2049 = fmul fast float %2046, %2048
  %2050 = fadd fast float %2049, %2047
  br label %2100

; <label>:2051
  br i1 %2032, label %2052, label %2058

; <label>:2052
  %2053 = fsub fast float %2022, %2027
  %2054 = fsub fast float %2030, %2026
  %2055 = fmul fast float %2054, 0x3F847AE140000000
  %2056 = fdiv fast float %2053, %2055
  %2057 = call float @dx.op.unary.f32(i32 7, float %2056)
  br label %2061

; <label>:2058
  %2059 = fcmp fast oge float %2022, %2031
  %2060 = select i1 %2059, float 1.000000e+00, float 0.000000e+00
  br label %2061

; <label>:2061
  %2062 = phi float [ %2057, %2052 ], [ %2060, %2058 ]
  %2063 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2064 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2065 = call float @dx.op.binary.f32(i32 36, float %2063, float 0x3FEFF7CEE0000000)
  %2066 = call float @dx.op.binary.f32(i32 36, float %2064, float 0x3FEFF7CEE0000000)
  %2067 = add i32 %911, 121
  %2068 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2067)
  %2069 = extractvalue %dx.types.CBufRet.f32 %2068, 0
  %2070 = extractvalue %dx.types.CBufRet.f32 %2068, 1
  %2071 = fadd fast float %2069, %2065
  %2072 = fadd fast float %2070, %2066
  %2073 = call float @dx.op.unary.f32(i32 22, float %2071)
  %2074 = call float @dx.op.unary.f32(i32 22, float %2072)
  %2075 = uitofp i32 %2020 to float
  %2076 = fmul fast float %1465, %2074
  %2077 = fmul fast float %1467, %2075
  %2078 = call float @dx.op.unary.f32(i32 27, float %2076)
  %2079 = fadd fast float %2078, %2073
  %2080 = fmul fast float %2079, %1464
  %2081 = fadd fast float %2080, %2077
  %2082 = fptoui float %2081 to i32
  %2083 = icmp eq i32 %2082, %1999
  %2084 = and i1 %1998, %2083
  br i1 %2084, label %2088, label %2085

; <label>:2085
  %2086 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2082, i32 undef)
  %2087 = extractvalue %dx.types.ResRet.i32 %2086, 0
  br label %2088

; <label>:2088
  %2089 = phi i32 [ %2087, %2085 ], [ %2000, %2061 ]
  %2090 = and i32 %2089, 1
  %2091 = icmp eq i32 %2090, 0
  br i1 %2091, label %2100, label %2092

; <label>:2092
  %2093 = call float @dx.op.unary.f32(i32 7, float %907)
  %2094 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2073, float %2074, float %2075, float undef, i32 0, i32 0, i32 undef, float %2093)
  %2095 = extractvalue %dx.types.ResRet.f32 %2094, 0
  %2096 = fmul fast float %2062, %2017
  %2097 = fsub fast float 1.000000e+00, %2062
  %2098 = fmul fast float %2095, %2097
  %2099 = fadd fast float %2098, %2096
  br label %2100

; <label>:2100
  %2101 = phi float [ %2017, %2011 ], [ %2050, %2043 ], [ %2099, %2092 ], [ %2017, %2088 ]
  %2102 = phi i1 [ %1998, %2011 ], [ %1998, %2043 ], [ true, %2088 ], [ true, %2092 ]
  %2103 = phi i32 [ %1999, %2011 ], [ %1999, %2043 ], [ %2082, %2088 ], [ %2082, %2092 ]
  %2104 = phi i32 [ %2000, %2011 ], [ %2000, %2043 ], [ %2089, %2088 ], [ %2089, %2092 ]
  %2105 = select i1 %2016, float 1.000000e+00, float 0.000000e+00
  %2106 = fadd fast float %2002, %2105
  %2107 = fadd fast float %1898, %1678
  %2108 = fmul fast float %2107, %1464
  %2109 = fadd fast float %2108, %1468
  %2110 = fptoui float %2109 to i32
  %2111 = icmp eq i32 %2110, %2006
  br i1 %2111, label %2115, label %2112

; <label>:2112
  %2113 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2110, i32 undef)
  %2114 = extractvalue %dx.types.ResRet.i32 %2113, 0
  br label %2115

; <label>:2115
  %2116 = phi i32 [ %2114, %2112 ], [ %2012, %2100 ]
  %2117 = and i32 %2116, 1
  %2118 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1678, float %1896, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2119 = extractvalue %dx.types.ResRet.f32 %2118, 0
  %2120 = icmp ne i32 %2117, 0
  %2121 = select i1 %2120, float %2119, float 1.000000e+00
  %2122 = icmp eq i32 %2117, 0
  br i1 %2122, label %2204, label %2123

; <label>:2123
  %2124 = add i32 %911, 1
  %2125 = icmp eq i32 %2124, %716
  %2126 = fsub fast float %1462, %740
  %2127 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2128 = extractvalue %dx.types.CBufRet.i32 %2127, 1
  %2129 = and i32 %2128, 255
  %2130 = uitofp i32 %2129 to float
  %2131 = fmul fast float %2130, 0x3F847AE140000000
  %2132 = lshr i32 %2128, 8
  %2133 = and i32 %2132, 255
  %2134 = uitofp i32 %2133 to float
  %2135 = fmul fast float %2134, 0x3F847AE140000000
  %2136 = fcmp fast ogt float %2135, %2131
  br i1 %2125, label %2137, label %2155

; <label>:2137
  br i1 %2136, label %2138, label %2144

; <label>:2138
  %2139 = fsub fast float %2126, %2131
  %2140 = fsub fast float %2134, %2130
  %2141 = fmul fast float %2140, 0x3F847AE140000000
  %2142 = fdiv fast float %2139, %2141
  %2143 = call float @dx.op.unary.f32(i32 7, float %2142)
  br label %2147

; <label>:2144
  %2145 = fcmp fast oge float %2126, %2135
  %2146 = select i1 %2145, float 1.000000e+00, float 0.000000e+00
  br label %2147

; <label>:2147
  %2148 = phi float [ %2143, %2138 ], [ %2146, %2144 ]
  %2149 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2150 = extractvalue %dx.types.CBufRet.f32 %2149, 0
  %2151 = fmul fast float %2148, %2121
  %2152 = fsub fast float 1.000000e+00, %2148
  %2153 = fmul fast float %2150, %2152
  %2154 = fadd fast float %2153, %2151
  br label %2204

; <label>:2155
  br i1 %2136, label %2156, label %2162

; <label>:2156
  %2157 = fsub fast float %2126, %2131
  %2158 = fsub fast float %2134, %2130
  %2159 = fmul fast float %2158, 0x3F847AE140000000
  %2160 = fdiv fast float %2157, %2159
  %2161 = call float @dx.op.unary.f32(i32 7, float %2160)
  br label %2165

; <label>:2162
  %2163 = fcmp fast oge float %2126, %2135
  %2164 = select i1 %2163, float 1.000000e+00, float 0.000000e+00
  br label %2165

; <label>:2165
  %2166 = phi float [ %2161, %2156 ], [ %2164, %2162 ]
  %2167 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2168 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2169 = call float @dx.op.binary.f32(i32 36, float %2167, float 0x3FEFF7CEE0000000)
  %2170 = call float @dx.op.binary.f32(i32 36, float %2168, float 0x3FEFF7CEE0000000)
  %2171 = add i32 %911, 121
  %2172 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2171)
  %2173 = extractvalue %dx.types.CBufRet.f32 %2172, 0
  %2174 = extractvalue %dx.types.CBufRet.f32 %2172, 1
  %2175 = fadd fast float %2173, %2169
  %2176 = fadd fast float %2174, %2170
  %2177 = call float @dx.op.unary.f32(i32 22, float %2175)
  %2178 = call float @dx.op.unary.f32(i32 22, float %2176)
  %2179 = uitofp i32 %2124 to float
  %2180 = fmul fast float %1465, %2178
  %2181 = fmul fast float %1467, %2179
  %2182 = call float @dx.op.unary.f32(i32 27, float %2180)
  %2183 = fadd fast float %2182, %2177
  %2184 = fmul fast float %2183, %1464
  %2185 = fadd fast float %2184, %2181
  %2186 = fptoui float %2185 to i32
  %2187 = icmp eq i32 %2186, %2103
  %2188 = and i1 %2102, %2187
  br i1 %2188, label %2192, label %2189

; <label>:2189
  %2190 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2186, i32 undef)
  %2191 = extractvalue %dx.types.ResRet.i32 %2190, 0
  br label %2192

; <label>:2192
  %2193 = phi i32 [ %2191, %2189 ], [ %2104, %2165 ]
  %2194 = and i32 %2193, 1
  %2195 = icmp eq i32 %2194, 0
  br i1 %2195, label %2204, label %2196

; <label>:2196
  %2197 = call float @dx.op.unary.f32(i32 7, float %907)
  %2198 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2177, float %2178, float %2179, float undef, i32 0, i32 0, i32 undef, float %2197)
  %2199 = extractvalue %dx.types.ResRet.f32 %2198, 0
  %2200 = fmul fast float %2166, %2121
  %2201 = fsub fast float 1.000000e+00, %2166
  %2202 = fmul fast float %2199, %2201
  %2203 = fadd fast float %2202, %2200
  br label %2204

; <label>:2204
  %2205 = phi float [ %2121, %2115 ], [ %2154, %2147 ], [ %2203, %2196 ], [ %2121, %2192 ]
  %2206 = phi i1 [ %2102, %2115 ], [ %2102, %2147 ], [ true, %2192 ], [ true, %2196 ]
  %2207 = phi i32 [ %2103, %2115 ], [ %2103, %2147 ], [ %2186, %2192 ], [ %2186, %2196 ]
  %2208 = phi i32 [ %2104, %2115 ], [ %2104, %2147 ], [ %2193, %2192 ], [ %2193, %2196 ]
  %2209 = select i1 %2120, float 1.000000e+00, float 0.000000e+00
  %2210 = fadd fast float %2106, %2209
  %2211 = fadd fast float %1898, %1787
  %2212 = fmul fast float %2211, %1464
  %2213 = fadd fast float %2212, %1468
  %2214 = fptoui float %2213 to i32
  %2215 = icmp eq i32 %2214, %2110
  br i1 %2215, label %2219, label %2216

; <label>:2216
  %2217 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2214, i32 undef)
  %2218 = extractvalue %dx.types.ResRet.i32 %2217, 0
  br label %2219

; <label>:2219
  %2220 = phi i32 [ %2218, %2216 ], [ %2116, %2204 ]
  %2221 = and i32 %2220, 1
  %2222 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1787, float %1896, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2223 = extractvalue %dx.types.ResRet.f32 %2222, 0
  %2224 = icmp ne i32 %2221, 0
  %2225 = select i1 %2224, float %2223, float 1.000000e+00
  %2226 = icmp eq i32 %2221, 0
  br i1 %2226, label %2308, label %2227

; <label>:2227
  %2228 = add i32 %911, 1
  %2229 = icmp eq i32 %2228, %716
  %2230 = fsub fast float %1462, %740
  %2231 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2232 = extractvalue %dx.types.CBufRet.i32 %2231, 1
  %2233 = and i32 %2232, 255
  %2234 = uitofp i32 %2233 to float
  %2235 = fmul fast float %2234, 0x3F847AE140000000
  %2236 = lshr i32 %2232, 8
  %2237 = and i32 %2236, 255
  %2238 = uitofp i32 %2237 to float
  %2239 = fmul fast float %2238, 0x3F847AE140000000
  %2240 = fcmp fast ogt float %2239, %2235
  br i1 %2229, label %2241, label %2259

; <label>:2241
  br i1 %2240, label %2242, label %2248

; <label>:2242
  %2243 = fsub fast float %2230, %2235
  %2244 = fsub fast float %2238, %2234
  %2245 = fmul fast float %2244, 0x3F847AE140000000
  %2246 = fdiv fast float %2243, %2245
  %2247 = call float @dx.op.unary.f32(i32 7, float %2246)
  br label %2251

; <label>:2248
  %2249 = fcmp fast oge float %2230, %2239
  %2250 = select i1 %2249, float 1.000000e+00, float 0.000000e+00
  br label %2251

; <label>:2251
  %2252 = phi float [ %2247, %2242 ], [ %2250, %2248 ]
  %2253 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2254 = extractvalue %dx.types.CBufRet.f32 %2253, 0
  %2255 = fmul fast float %2252, %2225
  %2256 = fsub fast float 1.000000e+00, %2252
  %2257 = fmul fast float %2254, %2256
  %2258 = fadd fast float %2257, %2255
  br label %2308

; <label>:2259
  br i1 %2240, label %2260, label %2266

; <label>:2260
  %2261 = fsub fast float %2230, %2235
  %2262 = fsub fast float %2238, %2234
  %2263 = fmul fast float %2262, 0x3F847AE140000000
  %2264 = fdiv fast float %2261, %2263
  %2265 = call float @dx.op.unary.f32(i32 7, float %2264)
  br label %2269

; <label>:2266
  %2267 = fcmp fast oge float %2230, %2239
  %2268 = select i1 %2267, float 1.000000e+00, float 0.000000e+00
  br label %2269

; <label>:2269
  %2270 = phi float [ %2265, %2260 ], [ %2268, %2266 ]
  %2271 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2272 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2273 = call float @dx.op.binary.f32(i32 36, float %2271, float 0x3FEFF7CEE0000000)
  %2274 = call float @dx.op.binary.f32(i32 36, float %2272, float 0x3FEFF7CEE0000000)
  %2275 = add i32 %911, 121
  %2276 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2275)
  %2277 = extractvalue %dx.types.CBufRet.f32 %2276, 0
  %2278 = extractvalue %dx.types.CBufRet.f32 %2276, 1
  %2279 = fadd fast float %2277, %2273
  %2280 = fadd fast float %2278, %2274
  %2281 = call float @dx.op.unary.f32(i32 22, float %2279)
  %2282 = call float @dx.op.unary.f32(i32 22, float %2280)
  %2283 = uitofp i32 %2228 to float
  %2284 = fmul fast float %1465, %2282
  %2285 = fmul fast float %1467, %2283
  %2286 = call float @dx.op.unary.f32(i32 27, float %2284)
  %2287 = fadd fast float %2286, %2281
  %2288 = fmul fast float %2287, %1464
  %2289 = fadd fast float %2288, %2285
  %2290 = fptoui float %2289 to i32
  %2291 = icmp eq i32 %2290, %2207
  %2292 = and i1 %2206, %2291
  br i1 %2292, label %2296, label %2293

; <label>:2293
  %2294 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2290, i32 undef)
  %2295 = extractvalue %dx.types.ResRet.i32 %2294, 0
  br label %2296

; <label>:2296
  %2297 = phi i32 [ %2295, %2293 ], [ %2208, %2269 ]
  %2298 = and i32 %2297, 1
  %2299 = icmp eq i32 %2298, 0
  br i1 %2299, label %2308, label %2300

; <label>:2300
  %2301 = call float @dx.op.unary.f32(i32 7, float %907)
  %2302 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2281, float %2282, float %2283, float undef, i32 0, i32 0, i32 undef, float %2301)
  %2303 = extractvalue %dx.types.ResRet.f32 %2302, 0
  %2304 = fmul fast float %2270, %2225
  %2305 = fsub fast float 1.000000e+00, %2270
  %2306 = fmul fast float %2303, %2305
  %2307 = fadd fast float %2306, %2304
  br label %2308

; <label>:2308
  %2309 = phi float [ %2225, %2219 ], [ %2258, %2251 ], [ %2307, %2300 ], [ %2225, %2296 ]
  %2310 = phi i1 [ %2206, %2219 ], [ %2206, %2251 ], [ true, %2296 ], [ true, %2300 ]
  %2311 = phi i32 [ %2207, %2219 ], [ %2207, %2251 ], [ %2290, %2296 ], [ %2290, %2300 ]
  %2312 = phi i32 [ %2208, %2219 ], [ %2208, %2251 ], [ %2297, %2296 ], [ %2297, %2300 ]
  %2313 = select i1 %2224, float 1.000000e+00, float 0.000000e+00
  %2314 = fadd fast float %2210, %2313
  %2315 = fadd fast float %1892, %1433
  %2316 = call float @dx.op.binary.f32(i32 35, float %2315, float 0.000000e+00)
  %2317 = call float @dx.op.binary.f32(i32 36, float %2316, float 0x3FEFF7CEE0000000)
  %2318 = fadd fast float %1457, %2317
  %2319 = call float @dx.op.unary.f32(i32 22, float %2318)
  %2320 = fmul fast float %1465, %2319
  %2321 = call float @dx.op.unary.f32(i32 27, float %2320)
  %2322 = fadd fast float %2321, %1460
  %2323 = fmul fast float %2322, %1464
  %2324 = fadd fast float %2323, %1468
  %2325 = fptoui float %2324 to i32
  %2326 = icmp eq i32 %2325, %2214
  br i1 %2326, label %2330, label %2327

; <label>:2327
  %2328 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2325, i32 undef)
  %2329 = extractvalue %dx.types.ResRet.i32 %2328, 0
  br label %2330

; <label>:2330
  %2331 = phi i32 [ %2329, %2327 ], [ %2220, %2308 ]
  %2332 = and i32 %2331, 1
  %2333 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1460, float %2319, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2334 = extractvalue %dx.types.ResRet.f32 %2333, 0
  %2335 = icmp ne i32 %2332, 0
  %2336 = select i1 %2335, float %2334, float 1.000000e+00
  %2337 = icmp eq i32 %2332, 0
  br i1 %2337, label %2419, label %2338

; <label>:2338
  %2339 = add i32 %911, 1
  %2340 = icmp eq i32 %2339, %716
  %2341 = fsub fast float %1462, %740
  %2342 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2343 = extractvalue %dx.types.CBufRet.i32 %2342, 1
  %2344 = and i32 %2343, 255
  %2345 = uitofp i32 %2344 to float
  %2346 = fmul fast float %2345, 0x3F847AE140000000
  %2347 = lshr i32 %2343, 8
  %2348 = and i32 %2347, 255
  %2349 = uitofp i32 %2348 to float
  %2350 = fmul fast float %2349, 0x3F847AE140000000
  %2351 = fcmp fast ogt float %2350, %2346
  br i1 %2340, label %2352, label %2370

; <label>:2352
  br i1 %2351, label %2353, label %2359

; <label>:2353
  %2354 = fsub fast float %2341, %2346
  %2355 = fsub fast float %2349, %2345
  %2356 = fmul fast float %2355, 0x3F847AE140000000
  %2357 = fdiv fast float %2354, %2356
  %2358 = call float @dx.op.unary.f32(i32 7, float %2357)
  br label %2362

; <label>:2359
  %2360 = fcmp fast oge float %2341, %2350
  %2361 = select i1 %2360, float 1.000000e+00, float 0.000000e+00
  br label %2362

; <label>:2362
  %2363 = phi float [ %2358, %2353 ], [ %2361, %2359 ]
  %2364 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2365 = extractvalue %dx.types.CBufRet.f32 %2364, 0
  %2366 = fmul fast float %2363, %2336
  %2367 = fsub fast float 1.000000e+00, %2363
  %2368 = fmul fast float %2365, %2367
  %2369 = fadd fast float %2368, %2366
  br label %2419

; <label>:2370
  br i1 %2351, label %2371, label %2377

; <label>:2371
  %2372 = fsub fast float %2341, %2346
  %2373 = fsub fast float %2349, %2345
  %2374 = fmul fast float %2373, 0x3F847AE140000000
  %2375 = fdiv fast float %2372, %2374
  %2376 = call float @dx.op.unary.f32(i32 7, float %2375)
  br label %2380

; <label>:2377
  %2378 = fcmp fast oge float %2341, %2350
  %2379 = select i1 %2378, float 1.000000e+00, float 0.000000e+00
  br label %2380

; <label>:2380
  %2381 = phi float [ %2376, %2371 ], [ %2379, %2377 ]
  %2382 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2383 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2384 = call float @dx.op.binary.f32(i32 36, float %2382, float 0x3FEFF7CEE0000000)
  %2385 = call float @dx.op.binary.f32(i32 36, float %2383, float 0x3FEFF7CEE0000000)
  %2386 = add i32 %911, 121
  %2387 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2386)
  %2388 = extractvalue %dx.types.CBufRet.f32 %2387, 0
  %2389 = extractvalue %dx.types.CBufRet.f32 %2387, 1
  %2390 = fadd fast float %2388, %2384
  %2391 = fadd fast float %2389, %2385
  %2392 = call float @dx.op.unary.f32(i32 22, float %2390)
  %2393 = call float @dx.op.unary.f32(i32 22, float %2391)
  %2394 = uitofp i32 %2339 to float
  %2395 = fmul fast float %1465, %2393
  %2396 = fmul fast float %1467, %2394
  %2397 = call float @dx.op.unary.f32(i32 27, float %2395)
  %2398 = fadd fast float %2397, %2392
  %2399 = fmul fast float %2398, %1464
  %2400 = fadd fast float %2399, %2396
  %2401 = fptoui float %2400 to i32
  %2402 = icmp eq i32 %2401, %2311
  %2403 = and i1 %2310, %2402
  br i1 %2403, label %2407, label %2404

; <label>:2404
  %2405 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2401, i32 undef)
  %2406 = extractvalue %dx.types.ResRet.i32 %2405, 0
  br label %2407

; <label>:2407
  %2408 = phi i32 [ %2406, %2404 ], [ %2312, %2380 ]
  %2409 = and i32 %2408, 1
  %2410 = icmp eq i32 %2409, 0
  br i1 %2410, label %2419, label %2411

; <label>:2411
  %2412 = call float @dx.op.unary.f32(i32 7, float %907)
  %2413 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2392, float %2393, float %2394, float undef, i32 0, i32 0, i32 undef, float %2412)
  %2414 = extractvalue %dx.types.ResRet.f32 %2413, 0
  %2415 = fmul fast float %2381, %2336
  %2416 = fsub fast float 1.000000e+00, %2381
  %2417 = fmul fast float %2414, %2416
  %2418 = fadd fast float %2417, %2415
  br label %2419

; <label>:2419
  %2420 = phi float [ %2336, %2330 ], [ %2369, %2362 ], [ %2418, %2411 ], [ %2336, %2407 ]
  %2421 = phi i1 [ %2310, %2330 ], [ %2310, %2362 ], [ true, %2407 ], [ true, %2411 ]
  %2422 = phi i32 [ %2311, %2330 ], [ %2311, %2362 ], [ %2401, %2407 ], [ %2401, %2411 ]
  %2423 = phi i32 [ %2312, %2330 ], [ %2312, %2362 ], [ %2408, %2407 ], [ %2408, %2411 ]
  %2424 = select i1 %2335, float 1.000000e+00, float 0.000000e+00
  %2425 = fadd fast float %2314, %2424
  %2426 = fadd fast float %2321, %1569
  %2427 = fmul fast float %2426, %1464
  %2428 = fadd fast float %2427, %1468
  %2429 = fptoui float %2428 to i32
  %2430 = icmp eq i32 %2429, %2325
  br i1 %2430, label %2434, label %2431

; <label>:2431
  %2432 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2429, i32 undef)
  %2433 = extractvalue %dx.types.ResRet.i32 %2432, 0
  br label %2434

; <label>:2434
  %2435 = phi i32 [ %2433, %2431 ], [ %2331, %2419 ]
  %2436 = and i32 %2435, 1
  %2437 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1569, float %2319, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2438 = extractvalue %dx.types.ResRet.f32 %2437, 0
  %2439 = icmp ne i32 %2436, 0
  %2440 = select i1 %2439, float %2438, float 1.000000e+00
  %2441 = icmp eq i32 %2436, 0
  br i1 %2441, label %2523, label %2442

; <label>:2442
  %2443 = add i32 %911, 1
  %2444 = icmp eq i32 %2443, %716
  %2445 = fsub fast float %1462, %740
  %2446 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2447 = extractvalue %dx.types.CBufRet.i32 %2446, 1
  %2448 = and i32 %2447, 255
  %2449 = uitofp i32 %2448 to float
  %2450 = fmul fast float %2449, 0x3F847AE140000000
  %2451 = lshr i32 %2447, 8
  %2452 = and i32 %2451, 255
  %2453 = uitofp i32 %2452 to float
  %2454 = fmul fast float %2453, 0x3F847AE140000000
  %2455 = fcmp fast ogt float %2454, %2450
  br i1 %2444, label %2456, label %2474

; <label>:2456
  br i1 %2455, label %2457, label %2463

; <label>:2457
  %2458 = fsub fast float %2445, %2450
  %2459 = fsub fast float %2453, %2449
  %2460 = fmul fast float %2459, 0x3F847AE140000000
  %2461 = fdiv fast float %2458, %2460
  %2462 = call float @dx.op.unary.f32(i32 7, float %2461)
  br label %2466

; <label>:2463
  %2464 = fcmp fast oge float %2445, %2454
  %2465 = select i1 %2464, float 1.000000e+00, float 0.000000e+00
  br label %2466

; <label>:2466
  %2467 = phi float [ %2462, %2457 ], [ %2465, %2463 ]
  %2468 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2469 = extractvalue %dx.types.CBufRet.f32 %2468, 0
  %2470 = fmul fast float %2467, %2440
  %2471 = fsub fast float 1.000000e+00, %2467
  %2472 = fmul fast float %2469, %2471
  %2473 = fadd fast float %2472, %2470
  br label %2523

; <label>:2474
  br i1 %2455, label %2475, label %2481

; <label>:2475
  %2476 = fsub fast float %2445, %2450
  %2477 = fsub fast float %2453, %2449
  %2478 = fmul fast float %2477, 0x3F847AE140000000
  %2479 = fdiv fast float %2476, %2478
  %2480 = call float @dx.op.unary.f32(i32 7, float %2479)
  br label %2484

; <label>:2481
  %2482 = fcmp fast oge float %2445, %2454
  %2483 = select i1 %2482, float 1.000000e+00, float 0.000000e+00
  br label %2484

; <label>:2484
  %2485 = phi float [ %2480, %2475 ], [ %2483, %2481 ]
  %2486 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2487 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2488 = call float @dx.op.binary.f32(i32 36, float %2486, float 0x3FEFF7CEE0000000)
  %2489 = call float @dx.op.binary.f32(i32 36, float %2487, float 0x3FEFF7CEE0000000)
  %2490 = add i32 %911, 121
  %2491 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2490)
  %2492 = extractvalue %dx.types.CBufRet.f32 %2491, 0
  %2493 = extractvalue %dx.types.CBufRet.f32 %2491, 1
  %2494 = fadd fast float %2492, %2488
  %2495 = fadd fast float %2493, %2489
  %2496 = call float @dx.op.unary.f32(i32 22, float %2494)
  %2497 = call float @dx.op.unary.f32(i32 22, float %2495)
  %2498 = uitofp i32 %2443 to float
  %2499 = fmul fast float %1465, %2497
  %2500 = fmul fast float %1467, %2498
  %2501 = call float @dx.op.unary.f32(i32 27, float %2499)
  %2502 = fadd fast float %2501, %2496
  %2503 = fmul fast float %2502, %1464
  %2504 = fadd fast float %2503, %2500
  %2505 = fptoui float %2504 to i32
  %2506 = icmp eq i32 %2505, %2422
  %2507 = and i1 %2421, %2506
  br i1 %2507, label %2511, label %2508

; <label>:2508
  %2509 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2505, i32 undef)
  %2510 = extractvalue %dx.types.ResRet.i32 %2509, 0
  br label %2511

; <label>:2511
  %2512 = phi i32 [ %2510, %2508 ], [ %2423, %2484 ]
  %2513 = and i32 %2512, 1
  %2514 = icmp eq i32 %2513, 0
  br i1 %2514, label %2523, label %2515

; <label>:2515
  %2516 = call float @dx.op.unary.f32(i32 7, float %907)
  %2517 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2496, float %2497, float %2498, float undef, i32 0, i32 0, i32 undef, float %2516)
  %2518 = extractvalue %dx.types.ResRet.f32 %2517, 0
  %2519 = fmul fast float %2485, %2440
  %2520 = fsub fast float 1.000000e+00, %2485
  %2521 = fmul fast float %2518, %2520
  %2522 = fadd fast float %2521, %2519
  br label %2523

; <label>:2523
  %2524 = phi float [ %2440, %2434 ], [ %2473, %2466 ], [ %2522, %2515 ], [ %2440, %2511 ]
  %2525 = phi i1 [ %2421, %2434 ], [ %2421, %2466 ], [ true, %2511 ], [ true, %2515 ]
  %2526 = phi i32 [ %2422, %2434 ], [ %2422, %2466 ], [ %2505, %2511 ], [ %2505, %2515 ]
  %2527 = phi i32 [ %2423, %2434 ], [ %2423, %2466 ], [ %2512, %2511 ], [ %2512, %2515 ]
  %2528 = select i1 %2439, float 1.000000e+00, float 0.000000e+00
  %2529 = fadd fast float %2425, %2528
  %2530 = fadd fast float %2321, %1678
  %2531 = fmul fast float %2530, %1464
  %2532 = fadd fast float %2531, %1468
  %2533 = fptoui float %2532 to i32
  %2534 = icmp eq i32 %2533, %2429
  br i1 %2534, label %2538, label %2535

; <label>:2535
  %2536 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2533, i32 undef)
  %2537 = extractvalue %dx.types.ResRet.i32 %2536, 0
  br label %2538

; <label>:2538
  %2539 = phi i32 [ %2537, %2535 ], [ %2435, %2523 ]
  %2540 = and i32 %2539, 1
  %2541 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1678, float %2319, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2542 = extractvalue %dx.types.ResRet.f32 %2541, 0
  %2543 = icmp ne i32 %2540, 0
  %2544 = select i1 %2543, float %2542, float 1.000000e+00
  %2545 = icmp eq i32 %2540, 0
  br i1 %2545, label %2627, label %2546

; <label>:2546
  %2547 = add i32 %911, 1
  %2548 = icmp eq i32 %2547, %716
  %2549 = fsub fast float %1462, %740
  %2550 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2551 = extractvalue %dx.types.CBufRet.i32 %2550, 1
  %2552 = and i32 %2551, 255
  %2553 = uitofp i32 %2552 to float
  %2554 = fmul fast float %2553, 0x3F847AE140000000
  %2555 = lshr i32 %2551, 8
  %2556 = and i32 %2555, 255
  %2557 = uitofp i32 %2556 to float
  %2558 = fmul fast float %2557, 0x3F847AE140000000
  %2559 = fcmp fast ogt float %2558, %2554
  br i1 %2548, label %2560, label %2578

; <label>:2560
  br i1 %2559, label %2561, label %2567

; <label>:2561
  %2562 = fsub fast float %2549, %2554
  %2563 = fsub fast float %2557, %2553
  %2564 = fmul fast float %2563, 0x3F847AE140000000
  %2565 = fdiv fast float %2562, %2564
  %2566 = call float @dx.op.unary.f32(i32 7, float %2565)
  br label %2570

; <label>:2567
  %2568 = fcmp fast oge float %2549, %2558
  %2569 = select i1 %2568, float 1.000000e+00, float 0.000000e+00
  br label %2570

; <label>:2570
  %2571 = phi float [ %2566, %2561 ], [ %2569, %2567 ]
  %2572 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2573 = extractvalue %dx.types.CBufRet.f32 %2572, 0
  %2574 = fmul fast float %2571, %2544
  %2575 = fsub fast float 1.000000e+00, %2571
  %2576 = fmul fast float %2573, %2575
  %2577 = fadd fast float %2576, %2574
  br label %2627

; <label>:2578
  br i1 %2559, label %2579, label %2585

; <label>:2579
  %2580 = fsub fast float %2549, %2554
  %2581 = fsub fast float %2557, %2553
  %2582 = fmul fast float %2581, 0x3F847AE140000000
  %2583 = fdiv fast float %2580, %2582
  %2584 = call float @dx.op.unary.f32(i32 7, float %2583)
  br label %2588

; <label>:2585
  %2586 = fcmp fast oge float %2549, %2558
  %2587 = select i1 %2586, float 1.000000e+00, float 0.000000e+00
  br label %2588

; <label>:2588
  %2589 = phi float [ %2584, %2579 ], [ %2587, %2585 ]
  %2590 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2591 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2592 = call float @dx.op.binary.f32(i32 36, float %2590, float 0x3FEFF7CEE0000000)
  %2593 = call float @dx.op.binary.f32(i32 36, float %2591, float 0x3FEFF7CEE0000000)
  %2594 = add i32 %911, 121
  %2595 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2594)
  %2596 = extractvalue %dx.types.CBufRet.f32 %2595, 0
  %2597 = extractvalue %dx.types.CBufRet.f32 %2595, 1
  %2598 = fadd fast float %2596, %2592
  %2599 = fadd fast float %2597, %2593
  %2600 = call float @dx.op.unary.f32(i32 22, float %2598)
  %2601 = call float @dx.op.unary.f32(i32 22, float %2599)
  %2602 = uitofp i32 %2547 to float
  %2603 = fmul fast float %1465, %2601
  %2604 = fmul fast float %1467, %2602
  %2605 = call float @dx.op.unary.f32(i32 27, float %2603)
  %2606 = fadd fast float %2605, %2600
  %2607 = fmul fast float %2606, %1464
  %2608 = fadd fast float %2607, %2604
  %2609 = fptoui float %2608 to i32
  %2610 = icmp eq i32 %2609, %2526
  %2611 = and i1 %2525, %2610
  br i1 %2611, label %2615, label %2612

; <label>:2612
  %2613 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2609, i32 undef)
  %2614 = extractvalue %dx.types.ResRet.i32 %2613, 0
  br label %2615

; <label>:2615
  %2616 = phi i32 [ %2614, %2612 ], [ %2527, %2588 ]
  %2617 = and i32 %2616, 1
  %2618 = icmp eq i32 %2617, 0
  br i1 %2618, label %2627, label %2619

; <label>:2619
  %2620 = call float @dx.op.unary.f32(i32 7, float %907)
  %2621 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2600, float %2601, float %2602, float undef, i32 0, i32 0, i32 undef, float %2620)
  %2622 = extractvalue %dx.types.ResRet.f32 %2621, 0
  %2623 = fmul fast float %2589, %2544
  %2624 = fsub fast float 1.000000e+00, %2589
  %2625 = fmul fast float %2622, %2624
  %2626 = fadd fast float %2625, %2623
  br label %2627

; <label>:2627
  %2628 = phi float [ %2544, %2538 ], [ %2577, %2570 ], [ %2626, %2619 ], [ %2544, %2615 ]
  %2629 = phi i1 [ %2525, %2538 ], [ %2525, %2570 ], [ true, %2615 ], [ true, %2619 ]
  %2630 = phi i32 [ %2526, %2538 ], [ %2526, %2570 ], [ %2609, %2615 ], [ %2609, %2619 ]
  %2631 = phi i32 [ %2527, %2538 ], [ %2527, %2570 ], [ %2616, %2615 ], [ %2616, %2619 ]
  %2632 = select i1 %2543, float 1.000000e+00, float 0.000000e+00
  %2633 = fadd fast float %2529, %2632
  %2634 = fadd fast float %2321, %1787
  %2635 = fmul fast float %2634, %1464
  %2636 = fadd fast float %2635, %1468
  %2637 = fptoui float %2636 to i32
  %2638 = icmp eq i32 %2637, %2533
  br i1 %2638, label %2642, label %2639

; <label>:2639
  %2640 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2637, i32 undef)
  %2641 = extractvalue %dx.types.ResRet.i32 %2640, 0
  br label %2642

; <label>:2642
  %2643 = phi i32 [ %2641, %2639 ], [ %2539, %2627 ]
  %2644 = and i32 %2643, 1
  %2645 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1787, float %2319, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2646 = extractvalue %dx.types.ResRet.f32 %2645, 0
  %2647 = icmp ne i32 %2644, 0
  %2648 = select i1 %2647, float %2646, float 1.000000e+00
  %2649 = icmp eq i32 %2644, 0
  br i1 %2649, label %2731, label %2650

; <label>:2650
  %2651 = add i32 %911, 1
  %2652 = icmp eq i32 %2651, %716
  %2653 = fsub fast float %1462, %740
  %2654 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2655 = extractvalue %dx.types.CBufRet.i32 %2654, 1
  %2656 = and i32 %2655, 255
  %2657 = uitofp i32 %2656 to float
  %2658 = fmul fast float %2657, 0x3F847AE140000000
  %2659 = lshr i32 %2655, 8
  %2660 = and i32 %2659, 255
  %2661 = uitofp i32 %2660 to float
  %2662 = fmul fast float %2661, 0x3F847AE140000000
  %2663 = fcmp fast ogt float %2662, %2658
  br i1 %2652, label %2664, label %2682

; <label>:2664
  br i1 %2663, label %2665, label %2671

; <label>:2665
  %2666 = fsub fast float %2653, %2658
  %2667 = fsub fast float %2661, %2657
  %2668 = fmul fast float %2667, 0x3F847AE140000000
  %2669 = fdiv fast float %2666, %2668
  %2670 = call float @dx.op.unary.f32(i32 7, float %2669)
  br label %2674

; <label>:2671
  %2672 = fcmp fast oge float %2653, %2662
  %2673 = select i1 %2672, float 1.000000e+00, float 0.000000e+00
  br label %2674

; <label>:2674
  %2675 = phi float [ %2670, %2665 ], [ %2673, %2671 ]
  %2676 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2677 = extractvalue %dx.types.CBufRet.f32 %2676, 0
  %2678 = fmul fast float %2675, %2648
  %2679 = fsub fast float 1.000000e+00, %2675
  %2680 = fmul fast float %2677, %2679
  %2681 = fadd fast float %2680, %2678
  br label %2731

; <label>:2682
  br i1 %2663, label %2683, label %2689

; <label>:2683
  %2684 = fsub fast float %2653, %2658
  %2685 = fsub fast float %2661, %2657
  %2686 = fmul fast float %2685, 0x3F847AE140000000
  %2687 = fdiv fast float %2684, %2686
  %2688 = call float @dx.op.unary.f32(i32 7, float %2687)
  br label %2692

; <label>:2689
  %2690 = fcmp fast oge float %2653, %2662
  %2691 = select i1 %2690, float 1.000000e+00, float 0.000000e+00
  br label %2692

; <label>:2692
  %2693 = phi float [ %2688, %2683 ], [ %2691, %2689 ]
  %2694 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2695 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2696 = call float @dx.op.binary.f32(i32 36, float %2694, float 0x3FEFF7CEE0000000)
  %2697 = call float @dx.op.binary.f32(i32 36, float %2695, float 0x3FEFF7CEE0000000)
  %2698 = add i32 %911, 121
  %2699 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2698)
  %2700 = extractvalue %dx.types.CBufRet.f32 %2699, 0
  %2701 = extractvalue %dx.types.CBufRet.f32 %2699, 1
  %2702 = fadd fast float %2700, %2696
  %2703 = fadd fast float %2701, %2697
  %2704 = call float @dx.op.unary.f32(i32 22, float %2702)
  %2705 = call float @dx.op.unary.f32(i32 22, float %2703)
  %2706 = uitofp i32 %2651 to float
  %2707 = fmul fast float %1465, %2705
  %2708 = fmul fast float %1467, %2706
  %2709 = call float @dx.op.unary.f32(i32 27, float %2707)
  %2710 = fadd fast float %2709, %2704
  %2711 = fmul fast float %2710, %1464
  %2712 = fadd fast float %2711, %2708
  %2713 = fptoui float %2712 to i32
  %2714 = icmp eq i32 %2713, %2630
  %2715 = and i1 %2629, %2714
  br i1 %2715, label %2719, label %2716

; <label>:2716
  %2717 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2713, i32 undef)
  %2718 = extractvalue %dx.types.ResRet.i32 %2717, 0
  br label %2719

; <label>:2719
  %2720 = phi i32 [ %2718, %2716 ], [ %2631, %2692 ]
  %2721 = and i32 %2720, 1
  %2722 = icmp eq i32 %2721, 0
  br i1 %2722, label %2731, label %2723

; <label>:2723
  %2724 = call float @dx.op.unary.f32(i32 7, float %907)
  %2725 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2704, float %2705, float %2706, float undef, i32 0, i32 0, i32 undef, float %2724)
  %2726 = extractvalue %dx.types.ResRet.f32 %2725, 0
  %2727 = fmul fast float %2693, %2648
  %2728 = fsub fast float 1.000000e+00, %2693
  %2729 = fmul fast float %2726, %2728
  %2730 = fadd fast float %2729, %2727
  br label %2731

; <label>:2731
  %2732 = phi float [ %2648, %2642 ], [ %2681, %2674 ], [ %2730, %2723 ], [ %2648, %2719 ]
  %2733 = phi i1 [ %2629, %2642 ], [ %2629, %2674 ], [ true, %2719 ], [ true, %2723 ]
  %2734 = phi i32 [ %2630, %2642 ], [ %2630, %2674 ], [ %2713, %2719 ], [ %2713, %2723 ]
  %2735 = phi i32 [ %2631, %2642 ], [ %2631, %2674 ], [ %2720, %2719 ], [ %2720, %2723 ]
  %2736 = select i1 %2647, float 1.000000e+00, float 0.000000e+00
  %2737 = fadd fast float %2633, %2736
  %2738 = fadd fast float %2315, %1433
  %2739 = call float @dx.op.binary.f32(i32 35, float %2738, float 0.000000e+00)
  %2740 = call float @dx.op.binary.f32(i32 36, float %2739, float 0x3FEFF7CEE0000000)
  %2741 = fadd fast float %1457, %2740
  %2742 = call float @dx.op.unary.f32(i32 22, float %2741)
  %2743 = fmul fast float %1465, %2742
  %2744 = call float @dx.op.unary.f32(i32 27, float %2743)
  %2745 = fadd fast float %2744, %1460
  %2746 = fmul fast float %2745, %1464
  %2747 = fadd fast float %2746, %1468
  %2748 = fptoui float %2747 to i32
  %2749 = icmp eq i32 %2748, %2637
  br i1 %2749, label %2753, label %2750

; <label>:2750
  %2751 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2748, i32 undef)
  %2752 = extractvalue %dx.types.ResRet.i32 %2751, 0
  br label %2753

; <label>:2753
  %2754 = phi i32 [ %2752, %2750 ], [ %2643, %2731 ]
  %2755 = and i32 %2754, 1
  %2756 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1460, float %2742, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2757 = extractvalue %dx.types.ResRet.f32 %2756, 0
  %2758 = icmp ne i32 %2755, 0
  %2759 = select i1 %2758, float %2757, float 1.000000e+00
  %2760 = icmp eq i32 %2755, 0
  br i1 %2760, label %2842, label %2761

; <label>:2761
  %2762 = add i32 %911, 1
  %2763 = icmp eq i32 %2762, %716
  %2764 = fsub fast float %1462, %740
  %2765 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2766 = extractvalue %dx.types.CBufRet.i32 %2765, 1
  %2767 = and i32 %2766, 255
  %2768 = uitofp i32 %2767 to float
  %2769 = fmul fast float %2768, 0x3F847AE140000000
  %2770 = lshr i32 %2766, 8
  %2771 = and i32 %2770, 255
  %2772 = uitofp i32 %2771 to float
  %2773 = fmul fast float %2772, 0x3F847AE140000000
  %2774 = fcmp fast ogt float %2773, %2769
  br i1 %2763, label %2775, label %2793

; <label>:2775
  br i1 %2774, label %2776, label %2782

; <label>:2776
  %2777 = fsub fast float %2764, %2769
  %2778 = fsub fast float %2772, %2768
  %2779 = fmul fast float %2778, 0x3F847AE140000000
  %2780 = fdiv fast float %2777, %2779
  %2781 = call float @dx.op.unary.f32(i32 7, float %2780)
  br label %2785

; <label>:2782
  %2783 = fcmp fast oge float %2764, %2773
  %2784 = select i1 %2783, float 1.000000e+00, float 0.000000e+00
  br label %2785

; <label>:2785
  %2786 = phi float [ %2781, %2776 ], [ %2784, %2782 ]
  %2787 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2788 = extractvalue %dx.types.CBufRet.f32 %2787, 0
  %2789 = fmul fast float %2786, %2759
  %2790 = fsub fast float 1.000000e+00, %2786
  %2791 = fmul fast float %2788, %2790
  %2792 = fadd fast float %2791, %2789
  br label %2842

; <label>:2793
  br i1 %2774, label %2794, label %2800

; <label>:2794
  %2795 = fsub fast float %2764, %2769
  %2796 = fsub fast float %2772, %2768
  %2797 = fmul fast float %2796, 0x3F847AE140000000
  %2798 = fdiv fast float %2795, %2797
  %2799 = call float @dx.op.unary.f32(i32 7, float %2798)
  br label %2803

; <label>:2800
  %2801 = fcmp fast oge float %2764, %2773
  %2802 = select i1 %2801, float 1.000000e+00, float 0.000000e+00
  br label %2803

; <label>:2803
  %2804 = phi float [ %2799, %2794 ], [ %2802, %2800 ]
  %2805 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2806 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2807 = call float @dx.op.binary.f32(i32 36, float %2805, float 0x3FEFF7CEE0000000)
  %2808 = call float @dx.op.binary.f32(i32 36, float %2806, float 0x3FEFF7CEE0000000)
  %2809 = add i32 %911, 121
  %2810 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2809)
  %2811 = extractvalue %dx.types.CBufRet.f32 %2810, 0
  %2812 = extractvalue %dx.types.CBufRet.f32 %2810, 1
  %2813 = fadd fast float %2811, %2807
  %2814 = fadd fast float %2812, %2808
  %2815 = call float @dx.op.unary.f32(i32 22, float %2813)
  %2816 = call float @dx.op.unary.f32(i32 22, float %2814)
  %2817 = uitofp i32 %2762 to float
  %2818 = fmul fast float %1465, %2816
  %2819 = fmul fast float %1467, %2817
  %2820 = call float @dx.op.unary.f32(i32 27, float %2818)
  %2821 = fadd fast float %2820, %2815
  %2822 = fmul fast float %2821, %1464
  %2823 = fadd fast float %2822, %2819
  %2824 = fptoui float %2823 to i32
  %2825 = icmp eq i32 %2824, %2734
  %2826 = and i1 %2733, %2825
  br i1 %2826, label %2830, label %2827

; <label>:2827
  %2828 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2824, i32 undef)
  %2829 = extractvalue %dx.types.ResRet.i32 %2828, 0
  br label %2830

; <label>:2830
  %2831 = phi i32 [ %2829, %2827 ], [ %2735, %2803 ]
  %2832 = and i32 %2831, 1
  %2833 = icmp eq i32 %2832, 0
  br i1 %2833, label %2842, label %2834

; <label>:2834
  %2835 = call float @dx.op.unary.f32(i32 7, float %907)
  %2836 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2815, float %2816, float %2817, float undef, i32 0, i32 0, i32 undef, float %2835)
  %2837 = extractvalue %dx.types.ResRet.f32 %2836, 0
  %2838 = fmul fast float %2804, %2759
  %2839 = fsub fast float 1.000000e+00, %2804
  %2840 = fmul fast float %2837, %2839
  %2841 = fadd fast float %2840, %2838
  br label %2842

; <label>:2842
  %2843 = phi float [ %2759, %2753 ], [ %2792, %2785 ], [ %2841, %2834 ], [ %2759, %2830 ]
  %2844 = phi i1 [ %2733, %2753 ], [ %2733, %2785 ], [ true, %2830 ], [ true, %2834 ]
  %2845 = phi i32 [ %2734, %2753 ], [ %2734, %2785 ], [ %2824, %2830 ], [ %2824, %2834 ]
  %2846 = phi i32 [ %2735, %2753 ], [ %2735, %2785 ], [ %2831, %2830 ], [ %2831, %2834 ]
  %2847 = select i1 %2758, float 1.000000e+00, float 0.000000e+00
  %2848 = fadd fast float %2737, %2847
  %2849 = fadd fast float %2744, %1569
  %2850 = fmul fast float %2849, %1464
  %2851 = fadd fast float %2850, %1468
  %2852 = fptoui float %2851 to i32
  %2853 = icmp eq i32 %2852, %2748
  br i1 %2853, label %2857, label %2854

; <label>:2854
  %2855 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2852, i32 undef)
  %2856 = extractvalue %dx.types.ResRet.i32 %2855, 0
  br label %2857

; <label>:2857
  %2858 = phi i32 [ %2856, %2854 ], [ %2754, %2842 ]
  %2859 = and i32 %2858, 1
  %2860 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1569, float %2742, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2861 = extractvalue %dx.types.ResRet.f32 %2860, 0
  %2862 = icmp ne i32 %2859, 0
  %2863 = select i1 %2862, float %2861, float 1.000000e+00
  %2864 = icmp eq i32 %2859, 0
  br i1 %2864, label %2946, label %2865

; <label>:2865
  %2866 = add i32 %911, 1
  %2867 = icmp eq i32 %2866, %716
  %2868 = fsub fast float %1462, %740
  %2869 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2870 = extractvalue %dx.types.CBufRet.i32 %2869, 1
  %2871 = and i32 %2870, 255
  %2872 = uitofp i32 %2871 to float
  %2873 = fmul fast float %2872, 0x3F847AE140000000
  %2874 = lshr i32 %2870, 8
  %2875 = and i32 %2874, 255
  %2876 = uitofp i32 %2875 to float
  %2877 = fmul fast float %2876, 0x3F847AE140000000
  %2878 = fcmp fast ogt float %2877, %2873
  br i1 %2867, label %2879, label %2897

; <label>:2879
  br i1 %2878, label %2880, label %2886

; <label>:2880
  %2881 = fsub fast float %2868, %2873
  %2882 = fsub fast float %2876, %2872
  %2883 = fmul fast float %2882, 0x3F847AE140000000
  %2884 = fdiv fast float %2881, %2883
  %2885 = call float @dx.op.unary.f32(i32 7, float %2884)
  br label %2889

; <label>:2886
  %2887 = fcmp fast oge float %2868, %2877
  %2888 = select i1 %2887, float 1.000000e+00, float 0.000000e+00
  br label %2889

; <label>:2889
  %2890 = phi float [ %2885, %2880 ], [ %2888, %2886 ]
  %2891 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2892 = extractvalue %dx.types.CBufRet.f32 %2891, 0
  %2893 = fmul fast float %2890, %2863
  %2894 = fsub fast float 1.000000e+00, %2890
  %2895 = fmul fast float %2892, %2894
  %2896 = fadd fast float %2895, %2893
  br label %2946

; <label>:2897
  br i1 %2878, label %2898, label %2904

; <label>:2898
  %2899 = fsub fast float %2868, %2873
  %2900 = fsub fast float %2876, %2872
  %2901 = fmul fast float %2900, 0x3F847AE140000000
  %2902 = fdiv fast float %2899, %2901
  %2903 = call float @dx.op.unary.f32(i32 7, float %2902)
  br label %2907

; <label>:2904
  %2905 = fcmp fast oge float %2868, %2877
  %2906 = select i1 %2905, float 1.000000e+00, float 0.000000e+00
  br label %2907

; <label>:2907
  %2908 = phi float [ %2903, %2898 ], [ %2906, %2904 ]
  %2909 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %2910 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %2911 = call float @dx.op.binary.f32(i32 36, float %2909, float 0x3FEFF7CEE0000000)
  %2912 = call float @dx.op.binary.f32(i32 36, float %2910, float 0x3FEFF7CEE0000000)
  %2913 = add i32 %911, 121
  %2914 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %2913)
  %2915 = extractvalue %dx.types.CBufRet.f32 %2914, 0
  %2916 = extractvalue %dx.types.CBufRet.f32 %2914, 1
  %2917 = fadd fast float %2915, %2911
  %2918 = fadd fast float %2916, %2912
  %2919 = call float @dx.op.unary.f32(i32 22, float %2917)
  %2920 = call float @dx.op.unary.f32(i32 22, float %2918)
  %2921 = uitofp i32 %2866 to float
  %2922 = fmul fast float %1465, %2920
  %2923 = fmul fast float %1467, %2921
  %2924 = call float @dx.op.unary.f32(i32 27, float %2922)
  %2925 = fadd fast float %2924, %2919
  %2926 = fmul fast float %2925, %1464
  %2927 = fadd fast float %2926, %2923
  %2928 = fptoui float %2927 to i32
  %2929 = icmp eq i32 %2928, %2845
  %2930 = and i1 %2844, %2929
  br i1 %2930, label %2934, label %2931

; <label>:2931
  %2932 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2928, i32 undef)
  %2933 = extractvalue %dx.types.ResRet.i32 %2932, 0
  br label %2934

; <label>:2934
  %2935 = phi i32 [ %2933, %2931 ], [ %2846, %2907 ]
  %2936 = and i32 %2935, 1
  %2937 = icmp eq i32 %2936, 0
  br i1 %2937, label %2946, label %2938

; <label>:2938
  %2939 = call float @dx.op.unary.f32(i32 7, float %907)
  %2940 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %2919, float %2920, float %2921, float undef, i32 0, i32 0, i32 undef, float %2939)
  %2941 = extractvalue %dx.types.ResRet.f32 %2940, 0
  %2942 = fmul fast float %2908, %2863
  %2943 = fsub fast float 1.000000e+00, %2908
  %2944 = fmul fast float %2941, %2943
  %2945 = fadd fast float %2944, %2942
  br label %2946

; <label>:2946
  %2947 = phi float [ %2863, %2857 ], [ %2896, %2889 ], [ %2945, %2938 ], [ %2863, %2934 ]
  %2948 = phi i1 [ %2844, %2857 ], [ %2844, %2889 ], [ true, %2934 ], [ true, %2938 ]
  %2949 = phi i32 [ %2845, %2857 ], [ %2845, %2889 ], [ %2928, %2934 ], [ %2928, %2938 ]
  %2950 = phi i32 [ %2846, %2857 ], [ %2846, %2889 ], [ %2935, %2934 ], [ %2935, %2938 ]
  %2951 = select i1 %2862, float 1.000000e+00, float 0.000000e+00
  %2952 = fadd fast float %2848, %2951
  %2953 = fadd fast float %2744, %1678
  %2954 = fmul fast float %2953, %1464
  %2955 = fadd fast float %2954, %1468
  %2956 = fptoui float %2955 to i32
  %2957 = icmp eq i32 %2956, %2852
  br i1 %2957, label %2961, label %2958

; <label>:2958
  %2959 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %2956, i32 undef)
  %2960 = extractvalue %dx.types.ResRet.i32 %2959, 0
  br label %2961

; <label>:2961
  %2962 = phi i32 [ %2960, %2958 ], [ %2858, %2946 ]
  %2963 = and i32 %2962, 1
  %2964 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1678, float %2742, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %2965 = extractvalue %dx.types.ResRet.f32 %2964, 0
  %2966 = icmp ne i32 %2963, 0
  %2967 = select i1 %2966, float %2965, float 1.000000e+00
  %2968 = icmp eq i32 %2963, 0
  br i1 %2968, label %3050, label %2969

; <label>:2969
  %2970 = add i32 %911, 1
  %2971 = icmp eq i32 %2970, %716
  %2972 = fsub fast float %1462, %740
  %2973 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %2974 = extractvalue %dx.types.CBufRet.i32 %2973, 1
  %2975 = and i32 %2974, 255
  %2976 = uitofp i32 %2975 to float
  %2977 = fmul fast float %2976, 0x3F847AE140000000
  %2978 = lshr i32 %2974, 8
  %2979 = and i32 %2978, 255
  %2980 = uitofp i32 %2979 to float
  %2981 = fmul fast float %2980, 0x3F847AE140000000
  %2982 = fcmp fast ogt float %2981, %2977
  br i1 %2971, label %2983, label %3001

; <label>:2983
  br i1 %2982, label %2984, label %2990

; <label>:2984
  %2985 = fsub fast float %2972, %2977
  %2986 = fsub fast float %2980, %2976
  %2987 = fmul fast float %2986, 0x3F847AE140000000
  %2988 = fdiv fast float %2985, %2987
  %2989 = call float @dx.op.unary.f32(i32 7, float %2988)
  br label %2993

; <label>:2990
  %2991 = fcmp fast oge float %2972, %2981
  %2992 = select i1 %2991, float 1.000000e+00, float 0.000000e+00
  br label %2993

; <label>:2993
  %2994 = phi float [ %2989, %2984 ], [ %2992, %2990 ]
  %2995 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %2996 = extractvalue %dx.types.CBufRet.f32 %2995, 0
  %2997 = fmul fast float %2994, %2967
  %2998 = fsub fast float 1.000000e+00, %2994
  %2999 = fmul fast float %2996, %2998
  %3000 = fadd fast float %2999, %2997
  br label %3050

; <label>:3001
  br i1 %2982, label %3002, label %3008

; <label>:3002
  %3003 = fsub fast float %2972, %2977
  %3004 = fsub fast float %2980, %2976
  %3005 = fmul fast float %3004, 0x3F847AE140000000
  %3006 = fdiv fast float %3003, %3005
  %3007 = call float @dx.op.unary.f32(i32 7, float %3006)
  br label %3011

; <label>:3008
  %3009 = fcmp fast oge float %2972, %2981
  %3010 = select i1 %3009, float 1.000000e+00, float 0.000000e+00
  br label %3011

; <label>:3011
  %3012 = phi float [ %3007, %3002 ], [ %3010, %3008 ]
  %3013 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %3014 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %3015 = call float @dx.op.binary.f32(i32 36, float %3013, float 0x3FEFF7CEE0000000)
  %3016 = call float @dx.op.binary.f32(i32 36, float %3014, float 0x3FEFF7CEE0000000)
  %3017 = add i32 %911, 121
  %3018 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %3017)
  %3019 = extractvalue %dx.types.CBufRet.f32 %3018, 0
  %3020 = extractvalue %dx.types.CBufRet.f32 %3018, 1
  %3021 = fadd fast float %3019, %3015
  %3022 = fadd fast float %3020, %3016
  %3023 = call float @dx.op.unary.f32(i32 22, float %3021)
  %3024 = call float @dx.op.unary.f32(i32 22, float %3022)
  %3025 = uitofp i32 %2970 to float
  %3026 = fmul fast float %1465, %3024
  %3027 = fmul fast float %1467, %3025
  %3028 = call float @dx.op.unary.f32(i32 27, float %3026)
  %3029 = fadd fast float %3028, %3023
  %3030 = fmul fast float %3029, %1464
  %3031 = fadd fast float %3030, %3027
  %3032 = fptoui float %3031 to i32
  %3033 = icmp eq i32 %3032, %2949
  %3034 = and i1 %2948, %3033
  br i1 %3034, label %3038, label %3035

; <label>:3035
  %3036 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3032, i32 undef)
  %3037 = extractvalue %dx.types.ResRet.i32 %3036, 0
  br label %3038

; <label>:3038
  %3039 = phi i32 [ %3037, %3035 ], [ %2950, %3011 ]
  %3040 = and i32 %3039, 1
  %3041 = icmp eq i32 %3040, 0
  br i1 %3041, label %3050, label %3042

; <label>:3042
  %3043 = call float @dx.op.unary.f32(i32 7, float %907)
  %3044 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %3023, float %3024, float %3025, float undef, i32 0, i32 0, i32 undef, float %3043)
  %3045 = extractvalue %dx.types.ResRet.f32 %3044, 0
  %3046 = fmul fast float %3012, %2967
  %3047 = fsub fast float 1.000000e+00, %3012
  %3048 = fmul fast float %3045, %3047
  %3049 = fadd fast float %3048, %3046
  br label %3050

; <label>:3050
  %3051 = phi float [ %2967, %2961 ], [ %3000, %2993 ], [ %3049, %3042 ], [ %2967, %3038 ]
  %3052 = phi i1 [ %2948, %2961 ], [ %2948, %2993 ], [ true, %3038 ], [ true, %3042 ]
  %3053 = phi i32 [ %2949, %2961 ], [ %2949, %2993 ], [ %3032, %3038 ], [ %3032, %3042 ]
  %3054 = phi i32 [ %2950, %2961 ], [ %2950, %2993 ], [ %3039, %3038 ], [ %3039, %3042 ]
  %3055 = select i1 %2966, float 1.000000e+00, float 0.000000e+00
  %3056 = fadd fast float %2952, %3055
  %3057 = fadd fast float %2744, %1787
  %3058 = fmul fast float %3057, %1464
  %3059 = fadd fast float %3058, %1468
  %3060 = fptoui float %3059 to i32
  %3061 = icmp eq i32 %3060, %2956
  br i1 %3061, label %3065, label %3062

; <label>:3062
  %3063 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3060, i32 undef)
  %3064 = extractvalue %dx.types.ResRet.i32 %3063, 0
  br label %3065

; <label>:3065
  %3066 = phi i32 [ %3064, %3062 ], [ %2962, %3050 ]
  %3067 = and i32 %3066, 1
  %3068 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %1787, float %2742, float %1462, float undef, i32 0, i32 0, i32 undef, float %1474)
  %3069 = extractvalue %dx.types.ResRet.f32 %3068, 0
  %3070 = icmp ne i32 %3067, 0
  %3071 = select i1 %3070, float %3069, float 1.000000e+00
  %3072 = icmp eq i32 %3067, 0
  br i1 %3072, label %3154, label %3073

; <label>:3073
  %3074 = add i32 %911, 1
  %3075 = icmp eq i32 %3074, %716
  %3076 = fsub fast float %1462, %740
  %3077 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %3078 = extractvalue %dx.types.CBufRet.i32 %3077, 1
  %3079 = and i32 %3078, 255
  %3080 = uitofp i32 %3079 to float
  %3081 = fmul fast float %3080, 0x3F847AE140000000
  %3082 = lshr i32 %3078, 8
  %3083 = and i32 %3082, 255
  %3084 = uitofp i32 %3083 to float
  %3085 = fmul fast float %3084, 0x3F847AE140000000
  %3086 = fcmp fast ogt float %3085, %3081
  br i1 %3075, label %3087, label %3105

; <label>:3087
  br i1 %3086, label %3088, label %3094

; <label>:3088
  %3089 = fsub fast float %3076, %3081
  %3090 = fsub fast float %3084, %3080
  %3091 = fmul fast float %3090, 0x3F847AE140000000
  %3092 = fdiv fast float %3089, %3091
  %3093 = call float @dx.op.unary.f32(i32 7, float %3092)
  br label %3097

; <label>:3094
  %3095 = fcmp fast oge float %3076, %3085
  %3096 = select i1 %3095, float 1.000000e+00, float 0.000000e+00
  br label %3097

; <label>:3097
  %3098 = phi float [ %3093, %3088 ], [ %3096, %3094 ]
  %3099 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %3100 = extractvalue %dx.types.CBufRet.f32 %3099, 0
  %3101 = fmul fast float %3098, %3071
  %3102 = fsub fast float 1.000000e+00, %3098
  %3103 = fmul fast float %3100, %3102
  %3104 = fadd fast float %3103, %3101
  br label %3154

; <label>:3105
  br i1 %3086, label %3106, label %3112

; <label>:3106
  %3107 = fsub fast float %3076, %3081
  %3108 = fsub fast float %3084, %3080
  %3109 = fmul fast float %3108, 0x3F847AE140000000
  %3110 = fdiv fast float %3107, %3109
  %3111 = call float @dx.op.unary.f32(i32 7, float %3110)
  br label %3115

; <label>:3112
  %3113 = fcmp fast oge float %3076, %3085
  %3114 = select i1 %3113, float 1.000000e+00, float 0.000000e+00
  br label %3115

; <label>:3115
  %3116 = phi float [ %3111, %3106 ], [ %3114, %3112 ]
  %3117 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %3118 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %3119 = call float @dx.op.binary.f32(i32 36, float %3117, float 0x3FEFF7CEE0000000)
  %3120 = call float @dx.op.binary.f32(i32 36, float %3118, float 0x3FEFF7CEE0000000)
  %3121 = add i32 %911, 121
  %3122 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %3121)
  %3123 = extractvalue %dx.types.CBufRet.f32 %3122, 0
  %3124 = extractvalue %dx.types.CBufRet.f32 %3122, 1
  %3125 = fadd fast float %3123, %3119
  %3126 = fadd fast float %3124, %3120
  %3127 = call float @dx.op.unary.f32(i32 22, float %3125)
  %3128 = call float @dx.op.unary.f32(i32 22, float %3126)
  %3129 = uitofp i32 %3074 to float
  %3130 = fmul fast float %1465, %3128
  %3131 = fmul fast float %1467, %3129
  %3132 = call float @dx.op.unary.f32(i32 27, float %3130)
  %3133 = fadd fast float %3132, %3127
  %3134 = fmul fast float %3133, %1464
  %3135 = fadd fast float %3134, %3131
  %3136 = fptoui float %3135 to i32
  %3137 = icmp eq i32 %3136, %3053
  %3138 = and i1 %3052, %3137
  br i1 %3138, label %3142, label %3139

; <label>:3139
  %3140 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3136, i32 undef)
  %3141 = extractvalue %dx.types.ResRet.i32 %3140, 0
  br label %3142

; <label>:3142
  %3143 = phi i32 [ %3141, %3139 ], [ %3054, %3115 ]
  %3144 = and i32 %3143, 1
  %3145 = icmp eq i32 %3144, 0
  br i1 %3145, label %3154, label %3146

; <label>:3146
  %3147 = call float @dx.op.unary.f32(i32 7, float %907)
  %3148 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %3127, float %3128, float %3129, float undef, i32 0, i32 0, i32 undef, float %3147)
  %3149 = extractvalue %dx.types.ResRet.f32 %3148, 0
  %3150 = fmul fast float %3116, %3071
  %3151 = fsub fast float 1.000000e+00, %3116
  %3152 = fmul fast float %3149, %3151
  %3153 = fadd fast float %3152, %3150
  br label %3154

; <label>:3154
  %3155 = phi float [ %3071, %3065 ], [ %3104, %3097 ], [ %3153, %3146 ], [ %3071, %3142 ]
  %3156 = select i1 %3070, float 1.000000e+00, float 0.000000e+00
  %3157 = fadd fast float %3056, %3156
  %3158 = fcmp fast une float %3157, 0.000000e+00
  br i1 %3158, label %3159, label %3178

; <label>:3159
  %3160 = fadd fast float %1560, -1.600000e+01
  %3161 = fadd fast float %3160, %1668
  %3162 = fadd fast float %3161, %1777
  %3163 = fadd fast float %3162, %1886
  %3164 = fadd fast float %3163, %1997
  %3165 = fadd fast float %3164, %2101
  %3166 = fadd fast float %3165, %2205
  %3167 = fadd fast float %3166, %2309
  %3168 = fadd fast float %3167, %2420
  %3169 = fadd fast float %3168, %2524
  %3170 = fadd fast float %3169, %2628
  %3171 = fadd fast float %3170, %2732
  %3172 = fadd fast float %3171, %2843
  %3173 = fadd fast float %3172, %2947
  %3174 = fadd fast float %3173, %3051
  %3175 = fadd fast float %3174, %3157
  %3176 = fadd fast float %3175, %3155
  %3177 = fdiv fast float %3176, %3157
  br label %3178

; <label>:3178
  %3179 = phi float [ %3177, %3159 ], [ 1.000000e+00, %3154 ]
  %3180 = fsub fast float 1.000000e+00, %3179
  br label %3294

; <label>:3181
  %3182 = call float @dx.op.binary.f32(i32 35, float %908, float 0.000000e+00)
  %3183 = call float @dx.op.binary.f32(i32 35, float %909, float 0.000000e+00)
  %3184 = call float @dx.op.binary.f32(i32 36, float %3182, float 0x3FEFF7CEE0000000)
  %3185 = call float @dx.op.binary.f32(i32 36, float %3183, float 0x3FEFF7CEE0000000)
  %3186 = add i32 %911, 120
  %3187 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %3186)
  %3188 = extractvalue %dx.types.CBufRet.f32 %3187, 0
  %3189 = extractvalue %dx.types.CBufRet.f32 %3187, 1
  %3190 = fadd fast float %3188, %3184
  %3191 = fadd fast float %3189, %3185
  %3192 = call float @dx.op.unary.f32(i32 22, float %3190)
  %3193 = call float @dx.op.unary.f32(i32 22, float %3191)
  %3194 = uitofp i32 %911 to float
  %3195 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 136)
  %3196 = extractvalue %dx.types.CBufRet.f32 %3195, 0
  %3197 = extractvalue %dx.types.CBufRet.f32 %3195, 1
  %3198 = fmul fast float %3197, %3193
  %3199 = extractvalue %dx.types.CBufRet.f32 %3195, 2
  %3200 = fmul fast float %3199, %3194
  %3201 = call float @dx.op.unary.f32(i32 27, float %3198)
  %3202 = fadd fast float %3201, %3192
  %3203 = fmul fast float %3202, %3196
  %3204 = fadd fast float %3203, %3200
  %3205 = fptoui float %3204 to i32
  %3206 = call float @dx.op.unary.f32(i32 7, float %910)
  %3207 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3205, i32 undef)
  %3208 = extractvalue %dx.types.ResRet.i32 %3207, 0
  %3209 = and i32 %3208, 1
  %3210 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %3192, float %3193, float %3194, float undef, i32 0, i32 0, i32 undef, float %3206)
  %3211 = extractvalue %dx.types.ResRet.f32 %3210, 0
  %3212 = icmp ne i32 %3209, 0
  %3213 = select i1 %3212, float %3211, float 1.000000e+00
  %3214 = icmp eq i32 %3209, 0
  br i1 %3214, label %3291, label %3215

; <label>:3215
  %3216 = add i32 %911, 1
  %3217 = icmp eq i32 %3216, %716
  %3218 = fsub fast float %3194, %740
  %3219 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 137)
  %3220 = extractvalue %dx.types.CBufRet.i32 %3219, 1
  %3221 = and i32 %3220, 255
  %3222 = uitofp i32 %3221 to float
  %3223 = fmul fast float %3222, 0x3F847AE140000000
  %3224 = lshr i32 %3220, 8
  %3225 = and i32 %3224, 255
  %3226 = uitofp i32 %3225 to float
  %3227 = fmul fast float %3226, 0x3F847AE140000000
  %3228 = fcmp fast ogt float %3227, %3223
  br i1 %3217, label %3229, label %3247

; <label>:3229
  br i1 %3228, label %3230, label %3236

; <label>:3230
  %3231 = fsub fast float %3218, %3223
  %3232 = fsub fast float %3226, %3222
  %3233 = fmul fast float %3232, 0x3F847AE140000000
  %3234 = fdiv fast float %3231, %3233
  %3235 = call float @dx.op.unary.f32(i32 7, float %3234)
  br label %3239

; <label>:3236
  %3237 = fcmp fast oge float %3218, %3227
  %3238 = select i1 %3237, float 1.000000e+00, float 0.000000e+00
  br label %3239

; <label>:3239
  %3240 = phi float [ %3235, %3230 ], [ %3238, %3236 ]
  %3241 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 137)
  %3242 = extractvalue %dx.types.CBufRet.f32 %3241, 0
  %3243 = fmul fast float %3240, %3213
  %3244 = fsub fast float 1.000000e+00, %3240
  %3245 = fmul fast float %3242, %3244
  %3246 = fadd fast float %3245, %3243
  br label %3291

; <label>:3247
  br i1 %3228, label %3248, label %3254

; <label>:3248
  %3249 = fsub fast float %3218, %3223
  %3250 = fsub fast float %3226, %3222
  %3251 = fmul fast float %3250, 0x3F847AE140000000
  %3252 = fdiv fast float %3249, %3251
  %3253 = call float @dx.op.unary.f32(i32 7, float %3252)
  br label %3257

; <label>:3254
  %3255 = fcmp fast oge float %3218, %3227
  %3256 = select i1 %3255, float 1.000000e+00, float 0.000000e+00
  br label %3257

; <label>:3257
  %3258 = phi float [ %3253, %3248 ], [ %3256, %3254 ]
  %3259 = call float @dx.op.binary.f32(i32 35, float %905, float 0.000000e+00)
  %3260 = call float @dx.op.binary.f32(i32 35, float %906, float 0.000000e+00)
  %3261 = call float @dx.op.binary.f32(i32 36, float %3259, float 0x3FEFF7CEE0000000)
  %3262 = call float @dx.op.binary.f32(i32 36, float %3260, float 0x3FEFF7CEE0000000)
  %3263 = add i32 %911, 121
  %3264 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 %3263)
  %3265 = extractvalue %dx.types.CBufRet.f32 %3264, 0
  %3266 = extractvalue %dx.types.CBufRet.f32 %3264, 1
  %3267 = fadd fast float %3265, %3261
  %3268 = fadd fast float %3266, %3262
  %3269 = call float @dx.op.unary.f32(i32 22, float %3267)
  %3270 = call float @dx.op.unary.f32(i32 22, float %3268)
  %3271 = uitofp i32 %3216 to float
  %3272 = fmul fast float %3197, %3270
  %3273 = fmul fast float %3199, %3271
  %3274 = call float @dx.op.unary.f32(i32 27, float %3272)
  %3275 = fadd fast float %3274, %3269
  %3276 = fmul fast float %3275, %3196
  %3277 = fadd fast float %3276, %3273
  %3278 = fptoui float %3277 to i32
  %3279 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %3278, i32 undef)
  %3280 = extractvalue %dx.types.ResRet.i32 %3279, 0
  %3281 = and i32 %3280, 1
  %3282 = icmp eq i32 %3281, 0
  br i1 %3282, label %3291, label %3283

; <label>:3283
  %3284 = call float @dx.op.unary.f32(i32 7, float %907)
  %3285 = call %dx.types.ResRet.f32 @dx.op.sampleCmpLevelZero.f32(i32 65, %dx.types.Handle %4, %dx.types.Handle %9, float %3269, float %3270, float %3271, float undef, i32 0, i32 0, i32 undef, float %3284)
  %3286 = extractvalue %dx.types.ResRet.f32 %3285, 0
  %3287 = fmul fast float %3258, %3213
  %3288 = fsub fast float 1.000000e+00, %3258
  %3289 = fmul fast float %3286, %3288
  %3290 = fadd fast float %3289, %3287
  br label %3291

; <label>:3291
  %3292 = phi float [ %3213, %3181 ], [ %3246, %3239 ], [ %3290, %3283 ], [ %3213, %3257 ]
  %3293 = fsub fast float 1.000000e+00, %3292
  br label %3294

; <label>:3294
  %3295 = phi float [ %3293, %3291 ], [ %1425, %1417 ], [ 0.000000e+00, %961 ], [ %3180, %3178 ], [ 0.000000e+00, %1430 ]
  %3296 = fmul fast float %518, %3295
  %3297 = call float @dx.op.binary.f32(i32 35, float %714, float %3296)
  br label %3298

; <label>:3298
  %3299 = phi float [ %714, %718 ], [ %918, %913 ], [ %952, %944 ], [ %714, %953 ], [ %3297, %3294 ], [ %714, %713 ]
  %3300 = fsub fast float 1.000000e+00, %3299
  br label %3301

; <label>:3301
  %3302 = phi float [ %3300, %3298 ], [ 1.000000e+00, %0 ]
  %3303 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, %dx.types.Handle %8, %dx.types.Handle %12, float %127, float %131, float undef, float undef, i32 0, i32 0, i32 undef, float undef)
  %3304 = extractvalue %dx.types.ResRet.f32 %3303, 0
  %3305 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 8)
  %3306 = extractvalue %dx.types.CBufRet.f32 %3305, 0
  %3307 = fadd fast float %3304, -1.000000e+00
  %3308 = fmul fast float %3306, %3307
  %3309 = fadd fast float %3308, 1.000000e+00
  %3310 = call float @dx.op.binary.f32(i32 36, float %3309, float %3302)
  %3311 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 0)
  %3312 = extractvalue %dx.types.CBufRet.f32 %3311, 0
  %3313 = extractvalue %dx.types.CBufRet.f32 %3311, 1
  %3314 = extractvalue %dx.types.CBufRet.f32 %3311, 2
  %3315 = fsub fast float -0.000000e+00, %3312
  %3316 = fsub fast float -0.000000e+00, %3313
  %3317 = fsub fast float -0.000000e+00, %3314
  %3318 = call float @dx.op.dot3.f32(i32 55, float %105, float %108, float %111, float %3315, float %3316, float %3317)
  %3319 = call float @dx.op.unary.f32(i32 23, float %26)
  %3320 = fmul fast float %3319, 2.000000e+00
  %3321 = call float @dx.op.unary.f32(i32 21, float %3320)
  %3322 = fmul fast float %3321, 2.000000e+00
  %3323 = call float @dx.op.unary.f32(i32 6, float %3318)
  %3324 = fadd fast float %3322, -1.000000e+00
  %3325 = fadd fast float %3324, %3323
  %3326 = call float @dx.op.unary.f32(i32 7, float %3325)
  %3327 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 147)
  %3328 = extractvalue %dx.types.CBufRet.f32 %3327, 1
  %3329 = fadd fast float %3326, -1.000000e+00
  %3330 = fmul fast float %3328, %3329
  %3331 = fadd fast float %3330, 1.000000e+00
  %3332 = fmul fast float %3331, %3310
  %3333 = extractvalue %dx.types.CBufRet.f32 %3327, 0
  %3334 = fadd fast float %3332, -1.000000e+00
  %3335 = fmul fast float %3334, %3333
  %3336 = fadd fast float %3335, 1.000000e+00
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %3336)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %3336)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %3336)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %3336)
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

