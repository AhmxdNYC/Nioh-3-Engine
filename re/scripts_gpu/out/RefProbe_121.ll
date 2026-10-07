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
; shader debug name: dcbbd6ebfcf76369fa89bbcebfcf2285.pdb
; shader hash: dcbbd6ebfcf76369fa89bbcebfcf2285
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
;       row_major float4x4 mV2W;                      ; Offset:    0
;       row_major float4x4 mP2W;                      ; Offset:   64
;       float4 gv_distance_pos;                       ; Offset:  128
;       float4 gv_info_out;                           ; Offset:  144
;       float4 gv_info_in;                            ; Offset:  160
;       float4 gv_probefield_org;                     ; Offset:  176
;       float4 gv_probefield_pitch_size;              ; Offset:  192
;       float4 gv_probefield_pitch_size_rv;           ; Offset:  208
;       float4 gv_tiledoct_pitch_octsize_rv;          ; Offset:  224
;       float4 gv_probe_scale_offset;                 ; Offset:  240
;       float4 gv_indirect_weight;                    ; Offset:  256
;       float4 gv_indirect_weight_2;                  ; Offset:  272
;       float4 gv_trace_bias;                         ; Offset:  288
;       row_major float4x4 gm_f2w;                    ; Offset:  304
;       row_major float4x4 gm_w2f;                    ; Offset:  368
;       float4 gv_viewport;                           ; Offset:  432
;       float4 gv_unified_info;                       ; Offset:  448
;       float4 gv_mask_param;                         ; Offset:  464
;       int4 gv_lod_param;                            ; Offset:  480
;       float4 gv_time_param;                         ; Offset:  496
;   
;   } $Globals;                                       ; Offset:    0 Size:   512
;
; }
;
; Resource bind info for shCoeff
; {
;
;   struct struct.slSHCoeff
;   {
;
;       float3 c[9];                                  ; Offset:    0
;   
;   } $Element;                                       ; Offset:    0 Size:   108
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpsGBuf0                       sampler      NA          NA      S0            s12     1
; __smpsDepth                       sampler      NA          NA      S1            s11     1
; __smpsOctmapSource                sampler      NA          NA      S2             s9     1
; sGBuf0                            texture     f32          2d      T0            t12     1
; sDepth                            texture     f32          2d      T1            t11     1
; sStencil                          texture     u32          2d      T2            t14     1
; sOctDepthSource                   texture     f32     2darray      T3             t0     1
; oct_probe_info                    texture     f32         buf      T4            t16     1
; oct_ref_table                     texture     i32         buf      T5            t17     1
; uOutTex1                              UAV     f32     2darray      U0             u0     1
; shCoeff                               UAV  struct         r/w      U1             u2     1
; sky_occ                               UAV     f32         buf      U2             u4     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.Dimensions = type { i32, i32, i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2D<vector<unsigned int, 4> >" = type { <4 x i32>, %"class.Texture2D<vector<unsigned int, 4> >::mips_type" }
%"class.Texture2D<vector<unsigned int, 4> >::mips_type" = type { i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"class.Buffer<float>" = type { float }
%"class.Buffer<int>" = type { i32 }
%"class.RWTexture2DArray<vector<float, 4> >" = type { <4 x float> }
%"class.RWStructuredBuffer<slSHCoeff>" = type { %struct.slSHCoeff }
%struct.slSHCoeff = type { [9 x <3 x float>] }
%"class.RWBuffer<float>" = type { float }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, <4 x i32>, <4 x float> }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 2, i32 4, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 17, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 16, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 14, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %15 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %16 = alloca [9 x float], align 4
  %17 = alloca [9 x float], align 4
  %18 = alloca [9 x float], align 4
  %19 = alloca [9 x float], align 4
  %20 = alloca [9 x float], align 4
  %21 = alloca [9 x float], align 4
  %22 = alloca [9 x float], align 4
  %23 = alloca [9 x float], align 4
  %24 = alloca [9 x float], align 4
  %25 = alloca [9 x float], align 4
  %26 = alloca [9 x float], align 4
  %27 = alloca [9 x float], align 4
  %28 = uitofp i32 %14 to float
  %29 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %30 = extractvalue %dx.types.CBufRet.f32 %29, 0
  %31 = fcmp fast oge float %28, %30
  %32 = uitofp i32 %15 to float
  %33 = extractvalue %dx.types.CBufRet.f32 %29, 1
  %34 = fcmp fast oge float %32, %33
  %35 = or i1 %31, %34
  br i1 %35, label %2819, label %36

; <label>:36                                      ; preds = %0
  %37 = fadd fast float %28, 5.000000e-01
  %38 = fadd fast float %32, 5.000000e-01
  %39 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %40 = extractvalue %dx.types.CBufRet.f32 %39, 2
  %41 = fmul fast float %40, %37
  %42 = fmul fast float %40, %38
  %43 = extractvalue %dx.types.CBufRet.f32 %39, 0
  %44 = extractvalue %dx.types.CBufRet.f32 %39, 1
  %45 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 27)  ; CBufferLoadLegacy(handle,regIndex)
  %46 = extractvalue %dx.types.CBufRet.f32 %45, 0
  %47 = extractvalue %dx.types.CBufRet.f32 %45, 1
  %48 = fmul fast float %46, %43
  %49 = fmul fast float %47, %44
  %50 = extractvalue %dx.types.CBufRet.f32 %45, 2
  %51 = extractvalue %dx.types.CBufRet.f32 %45, 3
  %52 = fmul fast float %50, %43
  %53 = fmul fast float %51, %44
  %54 = fsub fast float %41, %48
  %55 = fsub fast float %42, %49
  %56 = fdiv fast float %54, %52
  %57 = fdiv fast float %55, %53
  %58 = call float @dx.op.tertiary.f32(i32 46, float %56, float 2.000000e+00, float -1.000000e+00)  ; FMad(a,b,c)
  %59 = call float @dx.op.tertiary.f32(i32 46, float %57, float 2.000000e+00, float -1.000000e+00)  ; FMad(a,b,c)
  %60 = fsub fast float -0.000000e+00, %59
  %61 = fptosi float %41 to i32
  %62 = fptosi float %42 to i32
  %63 = sitofp i32 %61 to float
  %64 = sitofp i32 %62 to float
  %65 = fadd fast float %63, 5.000000e-01
  %66 = fadd fast float %64, 5.000000e-01
  %67 = fdiv fast float %65, %43
  %68 = fdiv fast float %66, %44
  %69 = fadd fast float %46, %50
  %70 = fcmp fast oge float %67, %46
  %71 = fcmp fast olt float %67, %69
  %72 = and i1 %70, %71
  br i1 %72, label %73, label %2819

; <label>:73                                      ; preds = %36
  %74 = fadd fast float %47, %51
  %75 = fcmp fast oge float %68, %47
  %76 = fcmp fast olt float %68, %74
  %77 = and i1 %75, %76
  br i1 %77, label %78, label %2819

; <label>:78                                      ; preds = %73
  %79 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %7, i32 0)  ; GetDimensions(handle,mipLevel)
  %80 = extractvalue %dx.types.Dimensions %79, 0
  %81 = sitofp i32 %80 to float
  %82 = extractvalue %dx.types.Dimensions %79, 1
  %83 = sitofp i32 %82 to float
  %84 = fmul fast float %81, %67
  %85 = fmul fast float %83, %68
  %86 = fptosi float %84 to i32
  %87 = fptosi float %85 to i32
  %88 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %7, i32 0, i32 %86, i32 %87, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %89 = extractvalue %dx.types.ResRet.i32 %88, 1
  %90 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %91 = extractvalue %dx.types.CBufRet.f32 %90, 0
  %92 = extractvalue %dx.types.CBufRet.f32 %90, 1
  %93 = fptoui float %91 to i32
  %94 = fptoui float %92 to i32
  %95 = and i32 %94, %89
  %96 = icmp eq i32 %93, %95
  br i1 %96, label %97, label %2819

; <label>:97                                      ; preds = %78
  %98 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %9, %dx.types.Handle %12, float %67, float %68, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %99 = extractvalue %dx.types.ResRet.f32 %98, 0
  %100 = extractvalue %dx.types.ResRet.f32 %98, 1
  %101 = fmul fast float %99, 2.000000e+00
  %102 = fmul fast float %100, 2.000000e+00
  %103 = fadd fast float %101, -1.000000e+00
  %104 = fadd fast float %102, -1.000000e+00
  %105 = call float @dx.op.unary.f32(i32 6, float %103)  ; FAbs(value)
  %106 = fsub fast float 1.000000e+00, %105
  %107 = call float @dx.op.unary.f32(i32 6, float %104)  ; FAbs(value)
  %108 = fsub fast float %106, %107
  %109 = fcmp fast oge float %108, 0.000000e+00
  %110 = fsub fast float 1.000000e+00, %107
  %111 = fcmp fast oge float %103, 0.000000e+00
  %112 = fcmp fast oge float %104, 0.000000e+00
  %113 = select i1 %111, float 1.000000e+00, float -1.000000e+00
  %114 = select i1 %112, float 1.000000e+00, float -1.000000e+00
  %115 = fmul fast float %113, %110
  %116 = fmul fast float %106, %114
  %117 = select i1 %109, float %103, float %115
  %118 = select i1 %109, float %104, float %116
  %119 = call float @dx.op.dot3.f32(i32 55, float %117, float %118, float %108, float %117, float %118, float %108)  ; Dot3(ax,ay,az,bx,by,bz)
  %120 = call float @dx.op.unary.f32(i32 25, float %119)  ; Rsqrt(value)
  %121 = fmul fast float %117, %120
  %122 = fmul fast float %118, %120
  %123 = fmul fast float %120, %108
  %124 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %125 = extractvalue %dx.types.CBufRet.f32 %124, 0
  %126 = extractvalue %dx.types.CBufRet.f32 %124, 1
  %127 = extractvalue %dx.types.CBufRet.f32 %124, 2
  %128 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %129 = extractvalue %dx.types.CBufRet.f32 %128, 0
  %130 = extractvalue %dx.types.CBufRet.f32 %128, 1
  %131 = extractvalue %dx.types.CBufRet.f32 %128, 2
  %132 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %133 = extractvalue %dx.types.CBufRet.f32 %132, 0
  %134 = extractvalue %dx.types.CBufRet.f32 %132, 1
  %135 = extractvalue %dx.types.CBufRet.f32 %132, 2
  %136 = fmul fast float %121, %125
  %137 = call float @dx.op.tertiary.f32(i32 46, float %122, float %129, float %136)  ; FMad(a,b,c)
  %138 = call float @dx.op.tertiary.f32(i32 46, float %123, float %133, float %137)  ; FMad(a,b,c)
  %139 = fmul fast float %121, %126
  %140 = call float @dx.op.tertiary.f32(i32 46, float %122, float %130, float %139)  ; FMad(a,b,c)
  %141 = call float @dx.op.tertiary.f32(i32 46, float %123, float %134, float %140)  ; FMad(a,b,c)
  %142 = fmul fast float %121, %127
  %143 = call float @dx.op.tertiary.f32(i32 46, float %122, float %131, float %142)  ; FMad(a,b,c)
  %144 = call float @dx.op.tertiary.f32(i32 46, float %123, float %135, float %143)  ; FMad(a,b,c)
  %145 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %11, float %67, float %68, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %146 = extractvalue %dx.types.ResRet.f32 %145, 0
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 0
  %149 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %150 = extractvalue %dx.types.CBufRet.f32 %147, 2
  %151 = extractvalue %dx.types.CBufRet.f32 %147, 3
  %152 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %153 = extractvalue %dx.types.CBufRet.f32 %152, 0
  %154 = extractvalue %dx.types.CBufRet.f32 %152, 1
  %155 = extractvalue %dx.types.CBufRet.f32 %152, 2
  %156 = extractvalue %dx.types.CBufRet.f32 %152, 3
  %157 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %158 = extractvalue %dx.types.CBufRet.f32 %157, 0
  %159 = extractvalue %dx.types.CBufRet.f32 %157, 1
  %160 = extractvalue %dx.types.CBufRet.f32 %157, 2
  %161 = extractvalue %dx.types.CBufRet.f32 %157, 3
  %162 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %163 = extractvalue %dx.types.CBufRet.f32 %162, 0
  %164 = extractvalue %dx.types.CBufRet.f32 %162, 1
  %165 = extractvalue %dx.types.CBufRet.f32 %162, 2
  %166 = extractvalue %dx.types.CBufRet.f32 %162, 3
  %167 = fmul fast float %148, %58
  %168 = call float @dx.op.tertiary.f32(i32 46, float %60, float %153, float %167)  ; FMad(a,b,c)
  %169 = call float @dx.op.tertiary.f32(i32 46, float %146, float %158, float %168)  ; FMad(a,b,c)
  %170 = fadd fast float %169, %163
  %171 = fmul fast float %149, %58
  %172 = call float @dx.op.tertiary.f32(i32 46, float %60, float %154, float %171)  ; FMad(a,b,c)
  %173 = call float @dx.op.tertiary.f32(i32 46, float %146, float %159, float %172)  ; FMad(a,b,c)
  %174 = fadd fast float %173, %164
  %175 = fmul fast float %150, %58
  %176 = call float @dx.op.tertiary.f32(i32 46, float %60, float %155, float %175)  ; FMad(a,b,c)
  %177 = call float @dx.op.tertiary.f32(i32 46, float %146, float %160, float %176)  ; FMad(a,b,c)
  %178 = fadd fast float %177, %165
  %179 = fmul fast float %151, %58
  %180 = call float @dx.op.tertiary.f32(i32 46, float %60, float %156, float %179)  ; FMad(a,b,c)
  %181 = call float @dx.op.tertiary.f32(i32 46, float %146, float %161, float %180)  ; FMad(a,b,c)
  %182 = fadd fast float %181, %166
  %183 = fdiv fast float %170, %182
  %184 = fdiv fast float %174, %182
  %185 = fdiv fast float %178, %182
  %186 = call float @dx.op.dot3.f32(i32 55, float %138, float %141, float %144, float %138, float %141, float %144)  ; Dot3(ax,ay,az,bx,by,bz)
  %187 = call float @dx.op.unary.f32(i32 25, float %186)  ; Rsqrt(value)
  %188 = fmul fast float %187, %138
  %189 = fmul fast float %187, %141
  %190 = fmul fast float %187, %144
  %191 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %192 = extractvalue %dx.types.CBufRet.f32 %191, 0
  %193 = extractvalue %dx.types.CBufRet.f32 %191, 1
  %194 = extractvalue %dx.types.CBufRet.f32 %191, 2
  %195 = fsub fast float %183, %192
  %196 = fsub fast float %184, %193
  %197 = fsub fast float %185, %194
  %198 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %199 = extractvalue %dx.types.CBufRet.f32 %198, 0
  %200 = extractvalue %dx.types.CBufRet.f32 %198, 1
  %201 = extractvalue %dx.types.CBufRet.f32 %198, 2
  %202 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %203 = extractvalue %dx.types.CBufRet.f32 %202, 0
  %204 = extractvalue %dx.types.CBufRet.f32 %202, 1
  %205 = extractvalue %dx.types.CBufRet.f32 %202, 2
  %206 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %207 = extractvalue %dx.types.CBufRet.f32 %206, 0
  %208 = extractvalue %dx.types.CBufRet.f32 %206, 1
  %209 = extractvalue %dx.types.CBufRet.f32 %206, 2
  %210 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %211 = extractvalue %dx.types.CBufRet.f32 %210, 0
  %212 = extractvalue %dx.types.CBufRet.f32 %210, 1
  %213 = extractvalue %dx.types.CBufRet.f32 %210, 2
  %214 = fmul fast float %195, %199
  %215 = call float @dx.op.tertiary.f32(i32 46, float %196, float %203, float %214)  ; FMad(a,b,c)
  %216 = call float @dx.op.tertiary.f32(i32 46, float %197, float %207, float %215)  ; FMad(a,b,c)
  %217 = fadd fast float %216, %211
  %218 = fmul fast float %195, %200
  %219 = call float @dx.op.tertiary.f32(i32 46, float %196, float %204, float %218)  ; FMad(a,b,c)
  %220 = call float @dx.op.tertiary.f32(i32 46, float %197, float %208, float %219)  ; FMad(a,b,c)
  %221 = fadd fast float %220, %212
  %222 = fmul fast float %195, %201
  %223 = call float @dx.op.tertiary.f32(i32 46, float %196, float %205, float %222)  ; FMad(a,b,c)
  %224 = call float @dx.op.tertiary.f32(i32 46, float %197, float %209, float %223)  ; FMad(a,b,c)
  %225 = fadd fast float %224, %213
  %226 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %227 = extractvalue %dx.types.CBufRet.f32 %226, 3
  %228 = fmul fast float %227, 4.000000e+00
  %229 = extractvalue %dx.types.CBufRet.f32 %226, 0
  %230 = extractvalue %dx.types.CBufRet.f32 %226, 1
  %231 = extractvalue %dx.types.CBufRet.f32 %226, 2
  %232 = fmul fast float %229, %228
  %233 = fmul fast float %230, %228
  %234 = fmul fast float %231, %228
  %235 = fsub fast float %232, %228
  %236 = fsub fast float %233, %228
  %237 = fsub fast float %234, %228
  %238 = fmul fast float %235, 5.000000e-01
  %239 = fmul fast float %236, 5.000000e-01
  %240 = fmul fast float %237, 5.000000e-01
  %241 = call float @dx.op.unary.f32(i32 6, float %238)  ; FAbs(value)
  %242 = call float @dx.op.unary.f32(i32 6, float %239)  ; FAbs(value)
  %243 = call float @dx.op.unary.f32(i32 6, float %240)  ; FAbs(value)
  %244 = fadd fast float %238, %228
  %245 = fadd fast float %239, %228
  %246 = fadd fast float %240, %228
  %247 = call float @dx.op.unary.f32(i32 6, float %244)  ; FAbs(value)
  %248 = call float @dx.op.unary.f32(i32 6, float %245)  ; FAbs(value)
  %249 = call float @dx.op.unary.f32(i32 6, float %246)  ; FAbs(value)
  %250 = fsub fast float %217, %238
  %251 = fsub fast float %221, %239
  %252 = fsub fast float %225, %240
  %253 = call float @dx.op.unary.f32(i32 6, float %250)  ; FAbs(value)
  %254 = call float @dx.op.unary.f32(i32 6, float %251)  ; FAbs(value)
  %255 = call float @dx.op.unary.f32(i32 6, float %252)  ; FAbs(value)
  %256 = fcmp fast ogt float %253, %247
  %257 = fcmp fast ogt float %254, %248
  %258 = or i1 %256, %257
  %259 = fcmp fast ogt float %255, %249
  %260 = or i1 %258, %259
  br i1 %260, label %283, label %261

; <label>:261                                     ; preds = %97
  %262 = fcmp fast olt float %253, %241
  %263 = fcmp fast olt float %254, %242
  %264 = and i1 %262, %263
  %265 = fcmp fast olt float %255, %243
  %266 = and i1 %264, %265
  br i1 %266, label %283, label %267

; <label>:267                                     ; preds = %261
  %268 = fsub fast float %253, %241
  %269 = fsub fast float %254, %242
  %270 = fsub fast float %255, %243
  %271 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %268)  ; FMax(a,b)
  %272 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %269)  ; FMax(a,b)
  %273 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %270)  ; FMax(a,b)
  %274 = fmul fast float %271, %271
  %275 = fmul fast float %272, %272
  %276 = fadd fast float %275, %274
  %277 = fmul fast float %273, %273
  %278 = fadd fast float %276, %277
  %279 = call float @dx.op.unary.f32(i32 24, float %278)  ; Sqrt(value)
  %280 = fdiv fast float %279, %228
  %281 = call float @dx.op.unary.f32(i32 7, float %280)  ; Saturate(value)
  %282 = fsub fast float 1.000000e+00, %281
  br label %283

; <label>:283                                     ; preds = %267, %261, %97
  %284 = phi float [ %282, %267 ], [ -1.000000e+00, %97 ], [ 1.000000e+00, %261 ]
  %285 = fcmp fast ogt float %284, 0.000000e+00
  %286 = fcmp fast olt float %146, 1.000000e+00
  %287 = and i1 %286, %285
  %288 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %289 = extractvalue %dx.types.CBufRet.f32 %288, 1
  %290 = fcmp fast ogt float %289, 0.000000e+00
  %291 = and i1 %287, %290
  br i1 %291, label %292, label %2800

; <label>:292                                     ; preds = %283
  %293 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %294 = extractvalue %dx.types.CBufRet.f32 %293, 3
  %295 = fmul fast float %294, %217
  %296 = fmul fast float %294, %221
  %297 = fmul fast float %294, %225
  %298 = call float @dx.op.unary.f32(i32 27, float %295)  ; Round_ni(value)
  %299 = call float @dx.op.unary.f32(i32 27, float %296)  ; Round_ni(value)
  %300 = call float @dx.op.unary.f32(i32 27, float %297)  ; Round_ni(value)
  br label %301

; <label>:301                                     ; preds = %388, %292
  %302 = phi i32 [ 0, %292 ], [ %389, %388 ]
  %303 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %304 = extractvalue %dx.types.CBufRet.i32 %303, 0
  %305 = shl i32 1, %302
  %306 = and i32 %304, %305
  %307 = icmp eq i32 %306, 0
  br i1 %307, label %388, label %308

; <label>:308                                     ; preds = %301
  %309 = sitofp i32 %302 to float
  %310 = call float @dx.op.unary.f32(i32 21, float %309)  ; Exp(value)
  %311 = fptosi float %310 to i32
  %312 = sitofp i32 %311 to float
  %313 = fdiv fast float %298, %312
  %314 = fdiv fast float %299, %312
  %315 = fdiv fast float %300, %312
  %316 = call float @dx.op.unary.f32(i32 27, float %313)  ; Round_ni(value)
  %317 = call float @dx.op.unary.f32(i32 27, float %314)  ; Round_ni(value)
  %318 = call float @dx.op.unary.f32(i32 27, float %315)  ; Round_ni(value)
  %319 = fptosi float %316 to i32
  %320 = fptosi float %317 to i32
  %321 = fptosi float %318 to i32
  %322 = mul i32 %319, %311
  %323 = mul i32 %320, %311
  %324 = mul i32 %321, %311
  br label %325

; <label>:325                                     ; preds = %384, %308
  %326 = phi i32 [ 0, %308 ], [ %385, %384 ]
  %327 = and i32 %326, 1
  %328 = lshr i32 %326, 1
  %329 = and i32 %328, 1
  %330 = lshr i32 %326, 2
  %331 = and i32 %330, 1
  %332 = sub nsw i32 0, %327
  %333 = and i32 %311, %332
  %334 = sub nsw i32 0, %329
  %335 = and i32 %311, %334
  %336 = sub nsw i32 0, %331
  %337 = and i32 %311, %336
  %338 = add i32 %333, %322
  %339 = add i32 %335, %323
  %340 = add i32 %337, %324
  %341 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %342 = extractvalue %dx.types.CBufRet.f32 %341, 0
  %343 = extractvalue %dx.types.CBufRet.f32 %341, 1
  %344 = extractvalue %dx.types.CBufRet.f32 %341, 2
  %345 = fptosi float %342 to i32
  %346 = fptosi float %343 to i32
  %347 = fptosi float %344 to i32
  %348 = add i32 %345, -1
  %349 = add i32 %346, -1
  %350 = add i32 %347, -1
  %351 = or i32 %339, %338
  %352 = or i32 %351, %340
  %353 = icmp slt i32 %352, 0
  %354 = icmp sgt i32 %338, %348
  %355 = or i1 %354, %353
  %356 = icmp sgt i32 %339, %349
  %357 = or i1 %356, %355
  %358 = icmp sgt i32 %340, %350
  %359 = or i1 %358, %357
  br i1 %359, label %384, label %360

; <label>:360                                     ; preds = %325
  %361 = sitofp i32 %338 to float
  %362 = sitofp i32 %339 to float
  %363 = fmul fast float %342, %362
  %364 = fadd fast float %363, %361
  %365 = sitofp i32 %340 to float
  %366 = fmul fast float %342, %365
  %367 = fmul fast float %366, %343
  %368 = fadd fast float %364, %367
  %369 = fptosi float %368 to i32
  %370 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %369, i32 undef)  ; BufferLoad(srv,index,wot)
  %371 = extractvalue %dx.types.ResRet.i32 %370, 0
  %372 = icmp sgt i32 %371, -1
  br i1 %372, label %373, label %380

; <label>:373                                     ; preds = %360
  %374 = mul i32 %371, 10
  %375 = add i32 %374, 3
  %376 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %375, i32 undef)  ; BufferLoad(srv,index,wot)
  %377 = extractvalue %dx.types.ResRet.f32 %376, 0
  %378 = bitcast float %377 to i32
  %379 = and i32 %378, 65535
  br label %380

; <label>:380                                     ; preds = %373, %360
  %381 = phi i32 [ %379, %373 ], [ 0, %360 ]
  %382 = and i32 %381, %305
  %383 = icmp eq i32 %382, 0
  br i1 %383, label %384, label %391

; <label>:384                                     ; preds = %380, %325
  %385 = add nuw nsw i32 %326, 1
  %386 = icmp slt i32 %385, 8
  br i1 %386, label %325, label %387

; <label>:387                                     ; preds = %384
  br label %388

; <label>:388                                     ; preds = %387, %301
  %389 = add nuw nsw i32 %302, 1
  %390 = icmp ult i32 %389, 3
  br i1 %390, label %301, label %392

; <label>:391                                     ; preds = %380
  br label %393

; <label>:392                                     ; preds = %388
  br label %393

; <label>:393                                     ; preds = %392, %391
  %394 = phi i32 [ %302, %391 ], [ -1, %392 ]
  %395 = icmp ult i32 %394, 2
  br i1 %395, label %639, label %396

; <label>:396                                     ; preds = %393
  %397 = sitofp i32 %394 to float
  %398 = call float @dx.op.unary.f32(i32 21, float %397)  ; Exp(value)
  %399 = fptosi float %398 to i32
  %400 = sitofp i32 %399 to float
  %401 = fdiv fast float %298, %400
  %402 = fdiv fast float %299, %400
  %403 = fdiv fast float %300, %400
  %404 = call float @dx.op.unary.f32(i32 27, float %401)  ; Round_ni(value)
  %405 = call float @dx.op.unary.f32(i32 27, float %402)  ; Round_ni(value)
  %406 = call float @dx.op.unary.f32(i32 27, float %403)  ; Round_ni(value)
  %407 = fptosi float %404 to i32
  %408 = fptosi float %405 to i32
  %409 = fptosi float %406 to i32
  %410 = mul i32 %407, %399
  %411 = mul i32 %408, %399
  %412 = mul i32 %409, %399
  br label %413

; <label>:413                                     ; preds = %633, %396
  %414 = phi i32 [ 0, %396 ], [ %634, %633 ]
  %415 = phi i32 [ 0, %396 ], [ %635, %633 ]
  %416 = and i32 %415, 1
  %417 = lshr i32 %415, 1
  %418 = and i32 %417, 1
  %419 = lshr i32 %415, 2
  %420 = and i32 %419, 1
  %421 = sub nsw i32 0, %416
  %422 = and i32 %399, %421
  %423 = sub nsw i32 0, %418
  %424 = and i32 %399, %423
  %425 = sub nsw i32 0, %420
  %426 = and i32 %399, %425
  %427 = add i32 %422, %410
  %428 = add i32 %424, %411
  %429 = add i32 %426, %412
  %430 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %431 = extractvalue %dx.types.CBufRet.f32 %430, 0
  %432 = extractvalue %dx.types.CBufRet.f32 %430, 1
  %433 = extractvalue %dx.types.CBufRet.f32 %430, 2
  %434 = fptosi float %431 to i32
  %435 = fptosi float %432 to i32
  %436 = fptosi float %433 to i32
  %437 = add i32 %434, -1
  %438 = add i32 %435, -1
  %439 = add i32 %436, -1
  %440 = or i32 %428, %427
  %441 = or i32 %440, %429
  %442 = icmp slt i32 %441, 0
  %443 = icmp sgt i32 %427, %437
  %444 = or i1 %443, %442
  %445 = icmp sgt i32 %428, %438
  %446 = or i1 %445, %444
  %447 = icmp sgt i32 %429, %439
  %448 = or i1 %447, %446
  br i1 %448, label %633, label %449

; <label>:449                                     ; preds = %413
  %450 = sitofp i32 %427 to float
  %451 = sitofp i32 %428 to float
  %452 = fmul fast float %431, %451
  %453 = fadd fast float %452, %450
  %454 = sitofp i32 %429 to float
  %455 = fmul fast float %431, %454
  %456 = fmul fast float %455, %432
  %457 = fadd fast float %453, %456
  %458 = fptosi float %457 to i32
  %459 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %458, i32 undef)  ; BufferLoad(srv,index,wot)
  %460 = extractvalue %dx.types.ResRet.i32 %459, 0
  %461 = icmp sgt i32 %460, -1
  br i1 %461, label %462, label %477

; <label>:462                                     ; preds = %449
  %463 = mul i32 %460, 10
  %464 = add i32 %463, 3
  %465 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %464, i32 undef)  ; BufferLoad(srv,index,wot)
  %466 = extractvalue %dx.types.ResRet.f32 %465, 0
  %467 = bitcast float %466 to i32
  %468 = and i32 %467, 65535
  %469 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %463, i32 undef)  ; BufferLoad(srv,index,wot)
  %470 = extractvalue %dx.types.ResRet.f32 %469, 0
  %471 = or i32 %463, 1
  %472 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %471, i32 undef)  ; BufferLoad(srv,index,wot)
  %473 = extractvalue %dx.types.ResRet.f32 %472, 0
  %474 = add i32 %463, 2
  %475 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %474, i32 undef)  ; BufferLoad(srv,index,wot)
  %476 = extractvalue %dx.types.ResRet.f32 %475, 0
  br label %482

; <label>:477                                     ; preds = %449
  %478 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %479 = extractvalue %dx.types.CBufRet.f32 %478, 0
  %480 = extractvalue %dx.types.CBufRet.f32 %478, 1
  %481 = extractvalue %dx.types.CBufRet.f32 %478, 2
  br label %482

; <label>:482                                     ; preds = %477, %462
  %483 = phi i32 [ %468, %462 ], [ 0, %477 ]
  %484 = phi float [ %470, %462 ], [ %479, %477 ]
  %485 = phi float [ %473, %462 ], [ %480, %477 ]
  %486 = phi float [ %476, %462 ], [ %481, %477 ]
  %487 = and i32 %394, 31
  %488 = shl i32 1, %487
  %489 = and i32 %483, %488
  %490 = icmp eq i32 %489, 0
  %491 = icmp slt i32 %460, 0
  %492 = or i1 %491, %490
  br i1 %492, label %633, label %493

; <label>:493                                     ; preds = %482
  %494 = mul i32 %460, 10
  %495 = add i32 %494, 5
  %496 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %495, i32 undef)  ; BufferLoad(srv,index,wot)
  %497 = extractvalue %dx.types.ResRet.f32 %496, 0
  %498 = fcmp fast ogt float %497, 0.000000e+00
  br i1 %498, label %499, label %633

; <label>:499                                     ; preds = %493
  %500 = fsub fast float %484, %195
  %501 = fsub fast float %485, %196
  %502 = fsub fast float %486, %197
  %503 = call float @dx.op.dot3.f32(i32 55, float %500, float %501, float %502, float %500, float %501, float %502)  ; Dot3(ax,ay,az,bx,by,bz)
  %504 = call float @dx.op.unary.f32(i32 25, float %503)  ; Rsqrt(value)
  %505 = fmul fast float %504, %500
  %506 = fmul fast float %504, %501
  %507 = fmul fast float %504, %502
  %508 = call float @dx.op.dot3.f32(i32 55, float %505, float %506, float %507, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %509 = fsub fast float %195, %484
  %510 = fsub fast float %196, %485
  %511 = fsub fast float %197, %486
  %512 = call float @dx.op.dot3.f32(i32 55, float %509, float %510, float %511, float %509, float %510, float %511)  ; Dot3(ax,ay,az,bx,by,bz)
  %513 = call float @dx.op.unary.f32(i32 25, float %512)  ; Rsqrt(value)
  %514 = fmul fast float %513, %509
  %515 = fmul fast float %513, %510
  %516 = fmul fast float %513, %511
  %517 = fmul fast float %509, %509
  %518 = fmul fast float %510, %510
  %519 = fadd fast float %518, %517
  %520 = fmul fast float %511, %511
  %521 = fadd fast float %519, %520
  %522 = call float @dx.op.unary.f32(i32 24, float %521)  ; Sqrt(value)
  %523 = call float @dx.op.unary.f32(i32 6, float %514)  ; FAbs(value)
  %524 = call float @dx.op.unary.f32(i32 6, float %515)  ; FAbs(value)
  %525 = fadd fast float %524, %523
  %526 = call float @dx.op.unary.f32(i32 6, float %516)  ; FAbs(value)
  %527 = fadd fast float %525, %526
  %528 = fdiv fast float 1.000000e+00, %527
  %529 = fmul fast float %528, %514
  %530 = fmul fast float %528, %515
  %531 = fcmp fast ole float %516, 0.000000e+00
  %532 = call float @dx.op.unary.f32(i32 6, float %530)  ; FAbs(value)
  %533 = call float @dx.op.unary.f32(i32 6, float %529)  ; FAbs(value)
  %534 = fsub fast float 1.000000e+00, %532
  %535 = fsub fast float 1.000000e+00, %533
  %536 = fcmp fast oge float %529, 0.000000e+00
  %537 = select i1 %536, float 1.000000e+00, float -1.000000e+00
  %538 = fcmp fast oge float %530, 0.000000e+00
  %539 = select i1 %538, float 1.000000e+00, float -1.000000e+00
  %540 = fmul fast float %537, %534
  %541 = fmul fast float %535, %539
  %542 = select i1 %531, float %540, float %529
  %543 = select i1 %531, float %541, float %530
  %544 = fmul fast float %542, 5.000000e-01
  %545 = fmul fast float %543, 5.000000e-01
  %546 = fadd fast float %544, 5.000000e-01
  %547 = fadd fast float %545, 5.000000e-01
  %548 = add i32 %494, 6
  %549 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %548, i32 undef)  ; BufferLoad(srv,index,wot)
  %550 = extractvalue %dx.types.ResRet.f32 %549, 0
  %551 = fptoui float %550 to i32
  %552 = add i32 %494, 7
  %553 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %552, i32 undef)  ; BufferLoad(srv,index,wot)
  %554 = extractvalue %dx.types.ResRet.f32 %553, 0
  %555 = fptoui float %554 to i32
  %556 = add i32 %494, 8
  %557 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %556, i32 undef)  ; BufferLoad(srv,index,wot)
  %558 = extractvalue %dx.types.ResRet.f32 %557, 0
  %559 = fptoui float %558 to i32
  %560 = add i32 %494, 9
  %561 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %560, i32 undef)  ; BufferLoad(srv,index,wot)
  %562 = extractvalue %dx.types.ResRet.f32 %561, 0
  %563 = fptoui float %562 to i32
  %564 = mul i32 %563, %551
  %565 = mul i32 %563, %555
  %566 = sitofp i32 %564 to float
  %567 = sitofp i32 %565 to float
  %568 = sitofp i32 %559 to float
  %569 = sitofp i32 %563 to float
  %570 = fadd fast float %569, -2.000000e+00
  %571 = fmul fast float %570, %546
  %572 = fmul fast float %570, %547
  %573 = fadd fast float %566, 1.000000e+00
  %574 = fadd fast float %573, %571
  %575 = fadd fast float %567, 1.000000e+00
  %576 = fadd fast float %575, %572
  %577 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %578 = extractvalue %dx.types.CBufRet.f32 %577, 0
  %579 = extractvalue %dx.types.CBufRet.f32 %577, 1
  %580 = extractvalue %dx.types.CBufRet.f32 %577, 3
  %581 = fmul fast float %580, %578
  %582 = fmul fast float %580, %579
  %583 = fmul fast float %581, %574
  %584 = fmul fast float %582, %576
  %585 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %586 = extractvalue %dx.types.CBufRet.f32 %585, 0
  %587 = fadd fast float %586, %568
  %588 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %10, float %583, float %584, float %587, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %589 = extractvalue %dx.types.ResRet.f32 %588, 0
  %590 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %591 = extractvalue %dx.types.CBufRet.f32 %590, 1
  %592 = fadd fast float %591, %522
  %593 = extractvalue %dx.types.CBufRet.f32 %590, 0
  %594 = fdiv fast float %592, %593
  %595 = fcmp fast ogt float %594, %589
  br i1 %595, label %596, label %621

; <label>:596                                     ; preds = %499
  %597 = extractvalue %dx.types.ResRet.f32 %588, 1
  %598 = fmul fast float %589, %589
  %599 = fsub fast float %598, %597
  %600 = call float @dx.op.unary.f32(i32 6, float %599)  ; FAbs(value)
  %601 = fsub fast float %594, %589
  %602 = fmul fast float %601, %601
  %603 = fadd fast float %600, %602
  %604 = fdiv fast float %600, %603
  %605 = fcmp fast olt float %600, 0x3EE9000020000000
  %606 = fmul fast float %594, 0x3F847AE140000000
  %607 = fcmp fast olt float %601, %606
  %608 = and i1 %605, %607
  %609 = select i1 %608, float 1.000000e+00, float %604
  %610 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %611 = extractvalue %dx.types.CBufRet.f32 %610, 2
  %612 = fsub fast float 1.000000e+00, %611
  %613 = fsub fast float %609, %611
  %614 = fdiv fast float %613, %612
  %615 = call float @dx.op.unary.f32(i32 7, float %614)  ; Saturate(value)
  %616 = fmul fast float %615, 2.000000e+00
  %617 = fsub fast float 3.000000e+00, %616
  %618 = fmul fast float %615, %615
  %619 = fmul fast float %618, %617
  %620 = call float @dx.op.binary.f32(i32 36, float %609, float %619)  ; FMin(a,b)
  br label %621

; <label>:621                                     ; preds = %596, %499
  %622 = phi float [ %620, %596 ], [ 1.000000e+00, %499 ]
  %623 = fcmp fast ole float %508, 0.000000e+00
  %624 = fcmp fast olt float %622, 5.000000e-01
  %625 = and i1 %623, %624
  br i1 %625, label %633, label %626

; <label>:626                                     ; preds = %621
  %627 = fcmp fast ogt float %508, 0.000000e+00
  %628 = fcmp fast oge float %622, 5.000000e-01
  %629 = and i1 %627, %628
  %630 = zext i1 %629 to i32
  %631 = add i32 %414, 1
  %632 = add i32 %631, %630
  br label %633

; <label>:633                                     ; preds = %626, %621, %493, %482, %413
  %634 = phi i32 [ %414, %621 ], [ %632, %626 ], [ %414, %482 ], [ %414, %413 ], [ %414, %493 ]
  %635 = add nuw nsw i32 %415, 1
  %636 = icmp eq i32 %635, 8
  br i1 %636, label %637, label %413

; <label>:637                                     ; preds = %633
  %638 = icmp ugt i32 %634, 1
  br label %639

; <label>:639                                     ; preds = %637, %393
  %640 = phi i1 [ %638, %637 ], [ true, %393 ]
  %641 = icmp sgt i32 %394, -1
  %642 = and i1 %641, %640
  br i1 %642, label %643, label %1356

; <label>:643                                     ; preds = %639
  %644 = icmp ult i32 %394, 3
  br i1 %644, label %645, label %1350

; <label>:645                                     ; preds = %643
  br label %646

; <label>:646                                     ; preds = %1341, %645
  %647 = phi float [ %1342, %1341 ], [ 0.000000e+00, %645 ]
  %648 = phi float [ %1343, %1341 ], [ 0.000000e+00, %645 ]
  %649 = phi float [ %1344, %1341 ], [ 0.000000e+00, %645 ]
  %650 = phi float [ %1345, %1341 ], [ 0.000000e+00, %645 ]
  %651 = phi float [ %1346, %1341 ], [ 0.000000e+00, %645 ]
  %652 = phi i32 [ %1347, %1341 ], [ %394, %645 ]
  %653 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %654 = extractvalue %dx.types.CBufRet.i32 %653, 0
  %655 = and i32 %652, 31
  %656 = shl i32 1, %655
  %657 = and i32 %654, %656
  %658 = icmp eq i32 %657, 0
  br i1 %658, label %1341, label %659

; <label>:659                                     ; preds = %646
  %660 = sitofp i32 %652 to float
  %661 = call float @dx.op.unary.f32(i32 21, float %660)  ; Exp(value)
  %662 = fptosi float %661 to i32
  %663 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %664 = extractvalue %dx.types.CBufRet.f32 %663, 0
  %665 = extractvalue %dx.types.CBufRet.f32 %663, 1
  %666 = extractvalue %dx.types.CBufRet.f32 %663, 2
  %667 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %668 = extractvalue %dx.types.CBufRet.f32 %667, 0
  %669 = extractvalue %dx.types.CBufRet.f32 %667, 1
  %670 = extractvalue %dx.types.CBufRet.f32 %667, 2
  %671 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %672 = extractvalue %dx.types.CBufRet.f32 %671, 0
  %673 = extractvalue %dx.types.CBufRet.f32 %671, 1
  %674 = extractvalue %dx.types.CBufRet.f32 %671, 2
  %675 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %676 = extractvalue %dx.types.CBufRet.f32 %675, 0
  %677 = extractvalue %dx.types.CBufRet.f32 %675, 1
  %678 = extractvalue %dx.types.CBufRet.f32 %675, 2
  %679 = fmul fast float %664, %195
  %680 = call float @dx.op.tertiary.f32(i32 46, float %196, float %668, float %679)  ; FMad(a,b,c)
  %681 = call float @dx.op.tertiary.f32(i32 46, float %197, float %672, float %680)  ; FMad(a,b,c)
  %682 = fadd fast float %681, %676
  %683 = fmul fast float %665, %195
  %684 = call float @dx.op.tertiary.f32(i32 46, float %196, float %669, float %683)  ; FMad(a,b,c)
  %685 = call float @dx.op.tertiary.f32(i32 46, float %197, float %673, float %684)  ; FMad(a,b,c)
  %686 = fadd fast float %685, %677
  %687 = fmul fast float %666, %195
  %688 = call float @dx.op.tertiary.f32(i32 46, float %196, float %670, float %687)  ; FMad(a,b,c)
  %689 = call float @dx.op.tertiary.f32(i32 46, float %197, float %674, float %688)  ; FMad(a,b,c)
  %690 = fadd fast float %689, %678
  %691 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %692 = extractvalue %dx.types.CBufRet.f32 %691, 3
  %693 = sitofp i32 %662 to float
  %694 = fmul fast float %692, %693
  %695 = fdiv fast float %682, %694
  %696 = fdiv fast float %686, %694
  %697 = fdiv fast float %690, %694
  %698 = call float @dx.op.unary.f32(i32 27, float %695)  ; Round_ni(value)
  %699 = call float @dx.op.unary.f32(i32 27, float %696)  ; Round_ni(value)
  %700 = call float @dx.op.unary.f32(i32 27, float %697)  ; Round_ni(value)
  %701 = fptosi float %698 to i32
  %702 = fptosi float %699 to i32
  %703 = fptosi float %700 to i32
  %704 = sitofp i32 %701 to float
  %705 = sitofp i32 %702 to float
  %706 = sitofp i32 %703 to float
  %707 = fsub fast float %695, %704
  %708 = fsub fast float %696, %705
  %709 = fsub fast float %697, %706
  %710 = fcmp fast olt float %707, 0.000000e+00
  %711 = fadd fast float %707, 1.000000e+00
  %712 = select i1 %710, float %711, float %707
  %713 = fcmp fast olt float %708, 0.000000e+00
  %714 = fadd fast float %708, 1.000000e+00
  %715 = select i1 %713, float %714, float %708
  %716 = fcmp fast olt float %709, 0.000000e+00
  %717 = fadd fast float %709, 1.000000e+00
  %718 = select i1 %716, float %717, float %709
  %719 = call float @dx.op.unary.f32(i32 7, float %712)  ; Saturate(value)
  %720 = call float @dx.op.unary.f32(i32 7, float %715)  ; Saturate(value)
  %721 = call float @dx.op.unary.f32(i32 7, float %718)  ; Saturate(value)
  %722 = mul i32 %701, %662
  %723 = mul i32 %702, %662
  %724 = mul i32 %703, %662
  br label %725

; <label>:725                                     ; preds = %1274, %659
  %726 = phi float [ 0.000000e+00, %659 ], [ %1298, %1274 ]
  %727 = phi float [ 0.000000e+00, %659 ], [ %1299, %1274 ]
  %728 = phi float [ 0.000000e+00, %659 ], [ %1275, %1274 ]
  %729 = phi i32 [ 0, %659 ], [ %1302, %1274 ]
  %730 = phi float [ 0.000000e+00, %659 ], [ %1301, %1274 ]
  %731 = phi float [ 0.000000e+00, %659 ], [ %1292, %1274 ]
  %732 = phi float [ 0.000000e+00, %659 ], [ %1293, %1274 ]
  %733 = phi float [ 0.000000e+00, %659 ], [ %1294, %1274 ]
  %734 = phi float [ 0.000000e+00, %659 ], [ %1295, %1274 ]
  %735 = and i32 %729, 1
  %736 = lshr i32 %729, 1
  %737 = and i32 %736, 1
  %738 = lshr i32 %729, 2
  %739 = and i32 %738, 1
  %740 = sub nsw i32 0, %735
  %741 = and i32 %662, %740
  %742 = sub nsw i32 0, %737
  %743 = and i32 %662, %742
  %744 = sub nsw i32 0, %739
  %745 = and i32 %662, %744
  %746 = add i32 %741, %722
  %747 = add i32 %743, %723
  %748 = add i32 %745, %724
  %749 = sitofp i32 %735 to float
  %750 = sitofp i32 %737 to float
  %751 = sitofp i32 %739 to float
  %752 = fsub fast float 1.000000e+00, %719
  %753 = fsub fast float 1.000000e+00, %720
  %754 = fsub fast float 1.000000e+00, %721
  %755 = fsub fast float %719, %752
  %756 = fsub fast float %720, %753
  %757 = fsub fast float %721, %754
  %758 = fmul fast float %749, %755
  %759 = fmul fast float %750, %756
  %760 = fmul fast float %751, %757
  %761 = fadd fast float %758, %752
  %762 = fadd fast float %759, %753
  %763 = fadd fast float %760, %754
  %764 = or i32 %747, %746
  %765 = or i32 %764, %748
  %766 = icmp sgt i32 %765, -1
  %767 = sitofp i32 %746 to float
  %768 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %769 = extractvalue %dx.types.CBufRet.f32 %768, 0
  %770 = fcmp fast olt float %767, %769
  %771 = and i1 %766, %770
  %772 = sitofp i32 %747 to float
  %773 = extractvalue %dx.types.CBufRet.f32 %768, 1
  %774 = fcmp fast olt float %772, %773
  %775 = and i1 %771, %774
  %776 = sitofp i32 %748 to float
  %777 = extractvalue %dx.types.CBufRet.f32 %768, 2
  %778 = fcmp fast olt float %776, %777
  %779 = and i1 %775, %778
  br i1 %779, label %780, label %1274

; <label>:780                                     ; preds = %725
  %781 = fmul fast float %769, %772
  %782 = fadd fast float %781, %767
  %783 = fmul fast float %769, %776
  %784 = fmul fast float %783, %773
  %785 = fadd fast float %782, %784
  %786 = fptosi float %785 to i32
  %787 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %786, i32 undef)  ; BufferLoad(srv,index,wot)
  %788 = extractvalue %dx.types.ResRet.i32 %787, 0
  %789 = icmp sgt i32 %788, -1
  br i1 %789, label %790, label %805

; <label>:790                                     ; preds = %780
  %791 = mul i32 %788, 10
  %792 = add i32 %791, 3
  %793 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %792, i32 undef)  ; BufferLoad(srv,index,wot)
  %794 = extractvalue %dx.types.ResRet.f32 %793, 0
  %795 = bitcast float %794 to i32
  %796 = and i32 %795, 65535
  %797 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %791, i32 undef)  ; BufferLoad(srv,index,wot)
  %798 = extractvalue %dx.types.ResRet.f32 %797, 0
  %799 = or i32 %791, 1
  %800 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %799, i32 undef)  ; BufferLoad(srv,index,wot)
  %801 = extractvalue %dx.types.ResRet.f32 %800, 0
  %802 = add i32 %791, 2
  %803 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %802, i32 undef)  ; BufferLoad(srv,index,wot)
  %804 = extractvalue %dx.types.ResRet.f32 %803, 0
  br label %810

; <label>:805                                     ; preds = %780
  %806 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %807 = extractvalue %dx.types.CBufRet.f32 %806, 0
  %808 = extractvalue %dx.types.CBufRet.f32 %806, 1
  %809 = extractvalue %dx.types.CBufRet.f32 %806, 2
  br label %810

; <label>:810                                     ; preds = %805, %790
  %811 = phi i32 [ %796, %790 ], [ 0, %805 ]
  %812 = phi float [ %798, %790 ], [ %807, %805 ]
  %813 = phi float [ %801, %790 ], [ %808, %805 ]
  %814 = phi float [ %804, %790 ], [ %809, %805 ]
  %815 = and i32 %811, %656
  %816 = icmp eq i32 %815, 0
  %817 = icmp slt i32 %788, 0
  %818 = or i1 %817, %816
  br i1 %818, label %1262, label %819

; <label>:819                                     ; preds = %810
  %820 = mul i32 %788, 10
  %821 = add i32 %820, 5
  %822 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %821, i32 undef)  ; BufferLoad(srv,index,wot)
  %823 = extractvalue %dx.types.ResRet.f32 %822, 0
  %824 = fcmp fast ogt float %823, 0.000000e+00
  br i1 %824, label %825, label %1262

; <label>:825                                     ; preds = %819
  %826 = fsub fast float %195, %812
  %827 = fsub fast float %196, %813
  %828 = fsub fast float %197, %814
  %829 = fmul fast float %826, %826
  %830 = fmul fast float %827, %827
  %831 = fadd fast float %830, %829
  %832 = fmul fast float %828, %828
  %833 = fadd fast float %831, %832
  %834 = call float @dx.op.unary.f32(i32 24, float %833)  ; Sqrt(value)
  %835 = fmul fast float %834, %188
  %836 = fmul fast float %834, %189
  %837 = fmul fast float %834, %190
  %838 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %839 = extractvalue %dx.types.CBufRet.f32 %838, 0
  %840 = fmul fast float %835, %839
  %841 = fmul fast float %836, %839
  %842 = fmul fast float %837, %839
  %843 = fadd fast float %826, %840
  %844 = fadd fast float %827, %841
  %845 = fadd fast float %828, %842
  %846 = call float @dx.op.dot3.f32(i32 55, float %843, float %844, float %845, float %843, float %844, float %845)  ; Dot3(ax,ay,az,bx,by,bz)
  %847 = call float @dx.op.unary.f32(i32 25, float %846)  ; Rsqrt(value)
  %848 = fmul fast float %843, %847
  %849 = fmul fast float %844, %847
  %850 = fmul fast float %845, %847
  %851 = fmul fast float %843, %843
  %852 = fmul fast float %844, %844
  %853 = fadd fast float %851, %852
  %854 = fmul fast float %845, %845
  %855 = fadd fast float %853, %854
  %856 = call float @dx.op.unary.f32(i32 24, float %855)  ; Sqrt(value)
  %857 = call float @dx.op.unary.f32(i32 6, float %848)  ; FAbs(value)
  %858 = call float @dx.op.unary.f32(i32 6, float %849)  ; FAbs(value)
  %859 = fadd fast float %858, %857
  %860 = call float @dx.op.unary.f32(i32 6, float %850)  ; FAbs(value)
  %861 = fadd fast float %859, %860
  %862 = fdiv fast float 1.000000e+00, %861
  %863 = fmul fast float %862, %848
  %864 = fmul fast float %862, %849
  %865 = fcmp fast ole float %850, 0.000000e+00
  %866 = call float @dx.op.unary.f32(i32 6, float %864)  ; FAbs(value)
  %867 = call float @dx.op.unary.f32(i32 6, float %863)  ; FAbs(value)
  %868 = fsub fast float 1.000000e+00, %866
  %869 = fsub fast float 1.000000e+00, %867
  %870 = fcmp fast oge float %863, 0.000000e+00
  %871 = select i1 %870, float 1.000000e+00, float -1.000000e+00
  %872 = fcmp fast oge float %864, 0.000000e+00
  %873 = select i1 %872, float 1.000000e+00, float -1.000000e+00
  %874 = fmul fast float %871, %868
  %875 = fmul fast float %869, %873
  %876 = select i1 %865, float %874, float %863
  %877 = select i1 %865, float %875, float %864
  %878 = fmul fast float %876, 5.000000e-01
  %879 = fmul fast float %877, 5.000000e-01
  %880 = fadd fast float %878, 5.000000e-01
  %881 = fadd fast float %879, 5.000000e-01
  %882 = add i32 %820, 6
  %883 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %882, i32 undef)  ; BufferLoad(srv,index,wot)
  %884 = extractvalue %dx.types.ResRet.f32 %883, 0
  %885 = fptoui float %884 to i32
  %886 = add i32 %820, 7
  %887 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %886, i32 undef)  ; BufferLoad(srv,index,wot)
  %888 = extractvalue %dx.types.ResRet.f32 %887, 0
  %889 = fptoui float %888 to i32
  %890 = add i32 %820, 8
  %891 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %890, i32 undef)  ; BufferLoad(srv,index,wot)
  %892 = extractvalue %dx.types.ResRet.f32 %891, 0
  %893 = fptoui float %892 to i32
  %894 = add i32 %820, 9
  %895 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %894, i32 undef)  ; BufferLoad(srv,index,wot)
  %896 = extractvalue %dx.types.ResRet.f32 %895, 0
  %897 = fptoui float %896 to i32
  %898 = mul i32 %897, %885
  %899 = mul i32 %897, %889
  %900 = sitofp i32 %898 to float
  %901 = sitofp i32 %899 to float
  %902 = sitofp i32 %893 to float
  %903 = sitofp i32 %897 to float
  %904 = fadd fast float %903, -2.000000e+00
  %905 = fmul fast float %904, %880
  %906 = fmul fast float %904, %881
  %907 = fadd fast float %900, 1.000000e+00
  %908 = fadd fast float %907, %905
  %909 = fadd fast float %901, 1.000000e+00
  %910 = fadd fast float %909, %906
  %911 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %912 = extractvalue %dx.types.CBufRet.f32 %911, 0
  %913 = extractvalue %dx.types.CBufRet.f32 %911, 1
  %914 = extractvalue %dx.types.CBufRet.f32 %911, 3
  %915 = fmul fast float %914, %912
  %916 = fmul fast float %914, %913
  %917 = fmul fast float %915, %908
  %918 = fmul fast float %916, %910
  %919 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %920 = extractvalue %dx.types.CBufRet.f32 %919, 0
  %921 = fadd fast float %920, %902
  %922 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %10, float %917, float %918, float %921, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %923 = extractvalue %dx.types.ResRet.f32 %922, 0
  %924 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %925 = extractvalue %dx.types.CBufRet.f32 %924, 1
  %926 = fadd fast float %925, %856
  %927 = extractvalue %dx.types.CBufRet.f32 %924, 0
  %928 = fdiv fast float %926, %927
  %929 = fcmp fast ogt float %928, %923
  br i1 %929, label %930, label %955

; <label>:930                                     ; preds = %825
  %931 = extractvalue %dx.types.ResRet.f32 %922, 1
  %932 = fmul fast float %923, %923
  %933 = fsub fast float %932, %931
  %934 = call float @dx.op.unary.f32(i32 6, float %933)  ; FAbs(value)
  %935 = fsub fast float %928, %923
  %936 = fmul fast float %935, %935
  %937 = fadd fast float %934, %936
  %938 = fdiv fast float %934, %937
  %939 = fcmp fast olt float %934, 0x3EE9000020000000
  %940 = fmul fast float %928, 0x3F847AE140000000
  %941 = fcmp fast olt float %935, %940
  %942 = and i1 %939, %941
  %943 = select i1 %942, float 1.000000e+00, float %938
  %944 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %945 = extractvalue %dx.types.CBufRet.f32 %944, 2
  %946 = fsub fast float 1.000000e+00, %945
  %947 = fsub fast float %943, %945
  %948 = fdiv fast float %947, %946
  %949 = call float @dx.op.unary.f32(i32 7, float %948)  ; Saturate(value)
  %950 = fmul fast float %949, 2.000000e+00
  %951 = fsub fast float 3.000000e+00, %950
  %952 = fmul fast float %949, %949
  %953 = fmul fast float %952, %951
  %954 = call float @dx.op.binary.f32(i32 36, float %943, float %953)  ; FMin(a,b)
  br label %955

; <label>:955                                     ; preds = %930, %825
  %956 = phi float [ %954, %930 ], [ 1.000000e+00, %825 ]
  %957 = fsub fast float %812, %195
  %958 = fsub fast float %813, %196
  %959 = fsub fast float %814, %197
  %960 = call float @dx.op.dot3.f32(i32 55, float %957, float %958, float %959, float %957, float %958, float %959)  ; Dot3(ax,ay,az,bx,by,bz)
  %961 = call float @dx.op.unary.f32(i32 25, float %960)  ; Rsqrt(value)
  %962 = fmul fast float %961, %957
  %963 = fmul fast float %961, %958
  %964 = fmul fast float %961, %959
  %965 = call float @dx.op.dot3.f32(i32 55, float %962, float %963, float %964, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %966 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %967 = extractvalue %dx.types.CBufRet.f32 %966, 3
  %968 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %969 = extractvalue %dx.types.CBufRet.f32 %968, 0
  %970 = extractvalue %dx.types.CBufRet.f32 %968, 1
  %971 = extractvalue %dx.types.CBufRet.f32 %968, 2
  %972 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %973 = extractvalue %dx.types.CBufRet.f32 %972, 0
  %974 = extractvalue %dx.types.CBufRet.f32 %972, 1
  %975 = extractvalue %dx.types.CBufRet.f32 %972, 2
  %976 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %977 = extractvalue %dx.types.CBufRet.f32 %976, 0
  %978 = extractvalue %dx.types.CBufRet.f32 %976, 1
  %979 = extractvalue %dx.types.CBufRet.f32 %976, 2
  %980 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %981 = extractvalue %dx.types.CBufRet.f32 %980, 0
  %982 = extractvalue %dx.types.CBufRet.f32 %980, 1
  %983 = extractvalue %dx.types.CBufRet.f32 %980, 2
  %984 = fmul fast float %967, %767
  %985 = fmul fast float %967, %772
  %986 = fmul fast float %967, %776
  %987 = fmul fast float %984, %969
  %988 = call float @dx.op.tertiary.f32(i32 46, float %985, float %973, float %987)  ; FMad(a,b,c)
  %989 = call float @dx.op.tertiary.f32(i32 46, float %986, float %977, float %988)  ; FMad(a,b,c)
  %990 = fmul fast float %984, %970
  %991 = call float @dx.op.tertiary.f32(i32 46, float %985, float %974, float %990)  ; FMad(a,b,c)
  %992 = call float @dx.op.tertiary.f32(i32 46, float %986, float %978, float %991)  ; FMad(a,b,c)
  %993 = fmul fast float %984, %971
  %994 = call float @dx.op.tertiary.f32(i32 46, float %985, float %975, float %993)  ; FMad(a,b,c)
  %995 = call float @dx.op.tertiary.f32(i32 46, float %986, float %979, float %994)  ; FMad(a,b,c)
  %996 = fsub fast float %981, %195
  %997 = fadd fast float %996, %989
  %998 = fsub fast float %982, %196
  %999 = fadd fast float %998, %992
  %1000 = fsub fast float %983, %197
  %1001 = fadd fast float %1000, %995
  %1002 = call float @dx.op.dot3.f32(i32 55, float %997, float %999, float %1001, float %997, float %999, float %1001)  ; Dot3(ax,ay,az,bx,by,bz)
  %1003 = call float @dx.op.unary.f32(i32 25, float %1002)  ; Rsqrt(value)
  %1004 = fmul fast float %1003, %997
  %1005 = fmul fast float %1003, %999
  %1006 = fmul fast float %1003, %1001
  %1007 = call float @dx.op.dot3.f32(i32 55, float %1004, float %1005, float %1006, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %1008 = fmul fast float %1007, %965
  %1009 = fcmp fast olt float %1008, 0.000000e+00
  %1010 = fcmp fast olt float %965, 0.000000e+00
  %1011 = and i1 %1010, %1009
  br i1 %1011, label %1012, label %1014

; <label>:1012                                    ; preds = %955
  %1013 = call float @dx.op.unary.f32(i32 6, float %965)  ; FAbs(value)
  br label %1014

; <label>:1014                                    ; preds = %1012, %955
  %1015 = phi float [ %1013, %1012 ], [ %965, %955 ]
  %1016 = mul nsw i32 %788, 9
  %1017 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1016, i32 undef)  ; BufferLoad(srv,index,wot)
  %1018 = extractvalue %dx.types.ResRet.f32 %1017, 0
  %1019 = add i32 %1016, 1
  %1020 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1019, i32 undef)  ; BufferLoad(srv,index,wot)
  %1021 = extractvalue %dx.types.ResRet.f32 %1020, 0
  %1022 = add i32 %1016, 2
  %1023 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1022, i32 undef)  ; BufferLoad(srv,index,wot)
  %1024 = extractvalue %dx.types.ResRet.f32 %1023, 0
  %1025 = add i32 %1016, 3
  %1026 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1025, i32 undef)  ; BufferLoad(srv,index,wot)
  %1027 = extractvalue %dx.types.ResRet.f32 %1026, 0
  %1028 = add i32 %1016, 4
  %1029 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1028, i32 undef)  ; BufferLoad(srv,index,wot)
  %1030 = extractvalue %dx.types.ResRet.f32 %1029, 0
  %1031 = add i32 %1016, 5
  %1032 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1031, i32 undef)  ; BufferLoad(srv,index,wot)
  %1033 = extractvalue %dx.types.ResRet.f32 %1032, 0
  %1034 = add i32 %1016, 6
  %1035 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1034, i32 undef)  ; BufferLoad(srv,index,wot)
  %1036 = extractvalue %dx.types.ResRet.f32 %1035, 0
  %1037 = add i32 %1016, 7
  %1038 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1037, i32 undef)  ; BufferLoad(srv,index,wot)
  %1039 = extractvalue %dx.types.ResRet.f32 %1038, 0
  %1040 = add i32 %1016, 8
  %1041 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1040, i32 undef)  ; BufferLoad(srv,index,wot)
  %1042 = extractvalue %dx.types.ResRet.f32 %1041, 0
  %1043 = fmul fast float %1018, 0x3FD20DD820000000
  %1044 = fmul fast float %1021, %189
  %1045 = fmul fast float %1024, %190
  %1046 = fmul fast float %188, 0x3FD4D8D900000000
  %1047 = fmul fast float %1046, %1027
  %1048 = fmul fast float %189, %188
  %1049 = fmul fast float %1048, %1030
  %1050 = fmul fast float %190, %189
  %1051 = fmul fast float %1050, %1033
  %1052 = fmul fast float %190, %190
  %1053 = fmul fast float %1052, 0x3FCE4712C0000000
  %1054 = fadd fast float %1053, 0xBFB42F61E0000000
  %1055 = fmul fast float %1054, %1036
  %1056 = fmul fast float %188, %190
  %1057 = fmul fast float %1056, %1039
  %1058 = fmul fast float %188, %188
  %1059 = fmul fast float %189, %189
  %1060 = fsub fast float %1058, %1059
  %1061 = fmul fast float %1060, 0x3FC17B13A0000000
  %1062 = fmul fast float %1061, %1042
  %1063 = fsub fast float %1049, %1051
  %1064 = fsub fast float %1063, %1057
  %1065 = fmul fast float %1064, 0x3FD17B13A0000000
  %1066 = fsub fast float %1045, %1044
  %1067 = fmul fast float %1066, 0x3FD4D8D900000000
  %1068 = fsub fast float %1043, %1047
  %1069 = fadd fast float %1068, %1055
  %1070 = fadd fast float %1069, %1062
  %1071 = fadd fast float %1070, %1065
  %1072 = fadd fast float %1071, %1067
  %1073 = call float @dx.op.unary.f32(i32 7, float %1072)  ; Saturate(value)
  %1074 = call float @dx.op.unary.f32(i32 7, float %956)  ; Saturate(value)
  %1075 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %1076 = extractvalue %dx.types.CBufRet.f32 %1075, 3
  %1077 = fadd fast float %1074, -1.000000e+00
  %1078 = fmul fast float %1076, %1077
  %1079 = fadd fast float %1078, 1.000000e+00
  %1080 = fmul fast float %1079, %1073
  %1081 = extractvalue %dx.types.CBufRet.f32 %1075, 0
  %1082 = call float @dx.op.unary.f32(i32 7, float %1015)  ; Saturate(value)
  %1083 = fsub fast float 1.000000e+00, %1081
  %1084 = fsub fast float 1.000000e+00, %1082
  %1085 = fmul fast float %1083, %1084
  %1086 = fadd fast float %1085, %1082
  %1087 = fcmp fast ogt float %1015, 0.000000e+00
  %1088 = fmul fast float %1086, %1074
  %1089 = select i1 %1087, float %1086, float %1088
  %1090 = add i32 %820, 4
  %1091 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1090, i32 undef)  ; BufferLoad(srv,index,wot)
  %1092 = extractvalue %dx.types.ResRet.f32 %1091, 0
  %1093 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1094 = extractvalue %dx.types.CBufRet.f32 %1093, 2
  %1095 = fsub fast float %1094, %1092
  %1096 = extractvalue %dx.types.CBufRet.f32 %1093, 3
  %1097 = fdiv fast float %1095, %1096
  %1098 = call float @dx.op.unary.f32(i32 7, float %1097)  ; Saturate(value)
  %1099 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 0)  ; BufferLoad(srv,index,wot)
  %1100 = extractvalue %dx.types.ResRet.f32 %1099, 0
  %1101 = extractvalue %dx.types.ResRet.f32 %1099, 1
  %1102 = extractvalue %dx.types.ResRet.f32 %1099, 2
  %1103 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 12)  ; BufferLoad(srv,index,wot)
  %1104 = extractvalue %dx.types.ResRet.f32 %1103, 0
  %1105 = extractvalue %dx.types.ResRet.f32 %1103, 1
  %1106 = extractvalue %dx.types.ResRet.f32 %1103, 2
  %1107 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 24)  ; BufferLoad(srv,index,wot)
  %1108 = extractvalue %dx.types.ResRet.f32 %1107, 0
  %1109 = extractvalue %dx.types.ResRet.f32 %1107, 1
  %1110 = extractvalue %dx.types.ResRet.f32 %1107, 2
  %1111 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 36)  ; BufferLoad(srv,index,wot)
  %1112 = extractvalue %dx.types.ResRet.f32 %1111, 0
  %1113 = extractvalue %dx.types.ResRet.f32 %1111, 1
  %1114 = extractvalue %dx.types.ResRet.f32 %1111, 2
  %1115 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 48)  ; BufferLoad(srv,index,wot)
  %1116 = extractvalue %dx.types.ResRet.f32 %1115, 0
  %1117 = extractvalue %dx.types.ResRet.f32 %1115, 1
  %1118 = extractvalue %dx.types.ResRet.f32 %1115, 2
  %1119 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 60)  ; BufferLoad(srv,index,wot)
  %1120 = extractvalue %dx.types.ResRet.f32 %1119, 0
  %1121 = extractvalue %dx.types.ResRet.f32 %1119, 1
  %1122 = extractvalue %dx.types.ResRet.f32 %1119, 2
  %1123 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 72)  ; BufferLoad(srv,index,wot)
  %1124 = extractvalue %dx.types.ResRet.f32 %1123, 0
  %1125 = extractvalue %dx.types.ResRet.f32 %1123, 1
  %1126 = extractvalue %dx.types.ResRet.f32 %1123, 2
  %1127 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 84)  ; BufferLoad(srv,index,wot)
  %1128 = extractvalue %dx.types.ResRet.f32 %1127, 0
  %1129 = extractvalue %dx.types.ResRet.f32 %1127, 1
  %1130 = extractvalue %dx.types.ResRet.f32 %1127, 2
  %1131 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %788, i32 96)  ; BufferLoad(srv,index,wot)
  %1132 = extractvalue %dx.types.ResRet.f32 %1131, 0
  %1133 = extractvalue %dx.types.ResRet.f32 %1131, 1
  %1134 = extractvalue %dx.types.ResRet.f32 %1131, 2
  %1135 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1136 = extractvalue %dx.types.CBufRet.f32 %1135, 0
  %1137 = fmul fast float %1136, %188
  %1138 = fmul fast float %1136, %189
  %1139 = fmul fast float %1136, %190
  %1140 = fadd fast float %826, %1137
  %1141 = fadd fast float %827, %1138
  %1142 = fadd fast float %828, %1139
  %1143 = call float @dx.op.dot3.f32(i32 55, float %1140, float %1141, float %1142, float %1140, float %1141, float %1142)  ; Dot3(ax,ay,az,bx,by,bz)
  %1144 = call float @dx.op.unary.f32(i32 25, float %1143)  ; Rsqrt(value)
  %1145 = fmul fast float %1140, %1144
  %1146 = fmul fast float %1141, %1144
  %1147 = fmul fast float %1142, %1144
  %1148 = fadd fast float %1145, %188
  %1149 = fadd fast float %1146, %189
  %1150 = fadd fast float %1147, %190
  %1151 = fmul fast float %1148, 5.000000e-01
  %1152 = fmul fast float %1149, 5.000000e-01
  %1153 = fmul fast float %1150, 5.000000e-01
  %1154 = fmul fast float %1100, 0x3FEC5BF980000000
  %1155 = fmul fast float %1101, 0x3FEC5BF980000000
  %1156 = fmul fast float %1102, 0x3FEC5BF980000000
  %1157 = fmul fast float %1124, 0x3FCFB4E740000000
  %1158 = fmul fast float %1125, 0x3FCFB4E740000000
  %1159 = fmul fast float %1126, 0x3FCFB4E740000000
  %1160 = fmul fast float %1132, 0x3FDB756EC0000000
  %1161 = fmul fast float %1133, 0x3FDB756EC0000000
  %1162 = fmul fast float %1134, 0x3FDB756EC0000000
  %1163 = fmul fast float %1152, %1104
  %1164 = fmul fast float %1152, %1105
  %1165 = fmul fast float %1152, %1106
  %1166 = fmul fast float %1153, %1108
  %1167 = fmul fast float %1153, %1109
  %1168 = fmul fast float %1153, %1110
  %1169 = fmul fast float %1151, %1112
  %1170 = fmul fast float %1151, %1113
  %1171 = fmul fast float %1151, %1114
  %1172 = fmul fast float %1151, %1152
  %1173 = fmul fast float %1172, %1116
  %1174 = fmul fast float %1172, %1117
  %1175 = fmul fast float %1172, %1118
  %1176 = fmul fast float %1152, %1153
  %1177 = fmul fast float %1176, %1120
  %1178 = fsub fast float -0.000000e+00, %1177
  %1179 = fmul fast float %1176, %1121
  %1180 = fsub fast float -0.000000e+00, %1179
  %1181 = fmul fast float %1176, %1122
  %1182 = fsub fast float -0.000000e+00, %1181
  %1183 = fmul fast float %1150, 1.500000e+00
  %1184 = fmul fast float %1183, %1153
  %1185 = fadd fast float %1184, -1.000000e+00
  %1186 = fmul fast float %1157, %1185
  %1187 = fmul fast float %1158, %1185
  %1188 = fmul fast float %1159, %1185
  %1189 = fmul fast float %1153, %1151
  %1190 = fmul fast float %1189, %1128
  %1191 = fmul fast float %1189, %1129
  %1192 = fmul fast float %1189, %1130
  %1193 = fmul fast float %1151, %1151
  %1194 = fmul fast float %1152, %1152
  %1195 = fsub fast float %1193, %1194
  %1196 = fmul fast float %1160, %1195
  %1197 = fmul fast float %1161, %1195
  %1198 = fmul fast float %1162, %1195
  %1199 = fsub fast float %1178, %1190
  %1200 = fadd fast float %1199, %1173
  %1201 = fmul fast float %1200, 0x3FEB756EC0000000
  %1202 = fsub fast float %1166, %1169
  %1203 = fsub fast float %1202, %1163
  %1204 = fmul fast float %1203, 0x3FF05F8CA0000000
  %1205 = fadd fast float %1186, %1154
  %1206 = fadd fast float %1205, %1196
  %1207 = fadd fast float %1206, %1201
  %1208 = fadd fast float %1207, %1204
  %1209 = fsub fast float %1180, %1191
  %1210 = fadd fast float %1209, %1174
  %1211 = fmul fast float %1210, 0x3FEB756EC0000000
  %1212 = fsub fast float %1167, %1170
  %1213 = fsub fast float %1212, %1164
  %1214 = fmul fast float %1213, 0x3FF05F8CA0000000
  %1215 = fadd fast float %1187, %1155
  %1216 = fadd fast float %1215, %1197
  %1217 = fadd fast float %1216, %1211
  %1218 = fadd fast float %1217, %1214
  %1219 = fsub fast float %1182, %1192
  %1220 = fadd fast float %1219, %1175
  %1221 = fmul fast float %1220, 0x3FEB756EC0000000
  %1222 = fsub fast float %1168, %1171
  %1223 = fsub fast float %1222, %1165
  %1224 = fmul fast float %1223, 0x3FF05F8CA0000000
  %1225 = fadd fast float %1188, %1156
  %1226 = fadd fast float %1225, %1198
  %1227 = fadd fast float %1226, %1221
  %1228 = fadd fast float %1227, %1224
  %1229 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1208)  ; FMax(a,b)
  %1230 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1218)  ; FMax(a,b)
  %1231 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1228)  ; FMax(a,b)
  %1232 = fmul fast float %957, %957
  %1233 = fmul fast float %958, %958
  %1234 = fadd fast float %1233, %1232
  %1235 = fmul fast float %959, %959
  %1236 = fadd fast float %1234, %1235
  %1237 = call float @dx.op.unary.f32(i32 24, float %1236)  ; Sqrt(value)
  %1238 = call float @dx.op.dot3.f32(i32 55, float %962, float %963, float %964, float %138, float %141, float %144)  ; Dot3(ax,ay,az,bx,by,bz)
  %1239 = fmul fast float %1238, %1237
  %1240 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1239)  ; FMax(a,b)
  %1241 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1242 = extractvalue %dx.types.CBufRet.f32 %1241, 3
  %1243 = fmul fast float %1242, 2.000000e+00
  %1244 = fmul fast float %1243, %693
  %1245 = fadd fast float %1244, %1240
  %1246 = fmul fast float %1244, %1244
  %1247 = fmul fast float %1245, %1245
  %1248 = fdiv fast float %1246, %1247
  %1249 = call float @dx.op.unary.f32(i32 7, float %1248)  ; Saturate(value)
  %1250 = call float @dx.op.unary.f32(i32 7, float %1249)  ; Saturate(value)
  %1251 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1252 = extractvalue %dx.types.CBufRet.f32 %1251, 0
  %1253 = fadd fast float %1250, -1.000000e+00
  %1254 = fmul fast float %1252, %1253
  %1255 = fadd fast float %1254, 1.000000e+00
  %1256 = fmul fast float %1229, %1074
  %1257 = fmul fast float %1256, %1255
  %1258 = fmul fast float %1230, %1074
  %1259 = fmul fast float %1258, %1255
  %1260 = fmul fast float %1231, %1074
  %1261 = fmul fast float %1260, %1255
  br label %1262

; <label>:1262                                    ; preds = %1014, %819, %810
  %1263 = phi float [ %1257, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1264 = phi float [ %1259, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1265 = phi float [ %1261, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1266 = phi float [ %1080, %1014 ], [ 1.000000e+00, %810 ], [ 1.000000e+00, %819 ]
  %1267 = phi float [ %1086, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1268 = phi float [ %1089, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1269 = phi float [ %1098, %1014 ], [ 0.000000e+00, %810 ], [ 0.000000e+00, %819 ]
  %1270 = fmul fast float %762, %761
  %1271 = fmul fast float %1270, %763
  %1272 = fmul fast float %1268, 1.250000e-01
  %1273 = fadd fast float %1272, %728
  br label %1274

; <label>:1274                                    ; preds = %1262, %725
  %1275 = phi float [ %1273, %1262 ], [ %728, %725 ]
  %1276 = phi float [ %1263, %1262 ], [ 0.000000e+00, %725 ]
  %1277 = phi float [ %1264, %1262 ], [ 0.000000e+00, %725 ]
  %1278 = phi float [ %1265, %1262 ], [ 0.000000e+00, %725 ]
  %1279 = phi float [ %1266, %1262 ], [ 1.000000e+00, %725 ]
  %1280 = phi float [ %1267, %1262 ], [ 1.000000e+00, %725 ]
  %1281 = phi float [ %1268, %1262 ], [ 1.000000e+00, %725 ]
  %1282 = phi float [ %1269, %1262 ], [ 0.000000e+00, %725 ]
  %1283 = phi float [ %1271, %1262 ], [ 0.000000e+00, %725 ]
  %1284 = fmul fast float %1280, %1276
  %1285 = fmul fast float %1284, %1283
  %1286 = fmul fast float %1280, %1277
  %1287 = fmul fast float %1286, %1283
  %1288 = fmul fast float %1280, %1278
  %1289 = fmul fast float %1288, %1283
  %1290 = fmul fast float %1281, %1279
  %1291 = fmul fast float %1290, %1283
  %1292 = fadd fast float %1285, %731
  %1293 = fadd fast float %1287, %732
  %1294 = fadd fast float %1289, %733
  %1295 = fadd fast float %1291, %734
  %1296 = fmul fast float %1283, %1280
  %1297 = fmul fast float %1283, %1281
  %1298 = fadd fast float %1296, %726
  %1299 = fadd fast float %1297, %727
  %1300 = fmul fast float %1283, %1282
  %1301 = fadd fast float %1300, %730
  %1302 = add nuw nsw i32 %729, 1
  %1303 = icmp eq i32 %1302, 8
  br i1 %1303, label %1304, label %725

; <label>:1304                                    ; preds = %1274
  %1305 = fcmp fast olt float %1275, 0x3F50624DE0000000
  %1306 = select i1 %1305, float 0.000000e+00, float %1299
  %1307 = fcmp fast ogt float %1298, 0.000000e+00
  %1308 = fdiv fast float %1292, %1298
  %1309 = fdiv fast float %1293, %1298
  %1310 = fdiv fast float %1294, %1298
  %1311 = select i1 %1307, float %1308, float 0.000000e+00
  %1312 = select i1 %1307, float %1309, float 0.000000e+00
  %1313 = select i1 %1307, float %1310, float 0.000000e+00
  %1314 = fcmp fast ogt float %1306, 0.000000e+00
  %1315 = fdiv fast float %1295, %1306
  %1316 = select i1 %1314, float %1315, float 1.000000e+00
  %1317 = call float @dx.op.unary.f32(i32 7, float %1316)  ; Saturate(value)
  %1318 = fadd fast float %1301, 0x3F1A36E2E0000000
  %1319 = call float @dx.op.binary.f32(i32 36, float 1.000000e+00, float %1318)  ; FMin(a,b)
  %1320 = fsub fast float 1.000000e+00, %1317
  %1321 = icmp eq i32 %652, 2
  %1322 = select i1 %1321, float 1.000000e+00, float %1319
  %1323 = fsub fast float 1.000000e+00, %651
  %1324 = fmul fast float %1311, %1323
  %1325 = fmul fast float %1312, %1323
  %1326 = fmul fast float %1313, %1323
  %1327 = fmul fast float %1320, %1323
  %1328 = fmul fast float %1324, %1322
  %1329 = fmul fast float %1325, %1322
  %1330 = fmul fast float %1326, %1322
  %1331 = fmul fast float %1327, %1322
  %1332 = fadd fast float %1328, %647
  %1333 = fadd fast float %1329, %648
  %1334 = fadd fast float %1330, %649
  %1335 = fadd fast float %1331, %650
  %1336 = fmul fast float %1319, %1323
  %1337 = fadd fast float %1336, %651
  %1338 = fcmp fast ult float %1337, 1.000000e+00
  br i1 %1338, label %1341, label %1339

; <label>:1339                                    ; preds = %1304
  %1340 = fsub fast float 1.000000e+00, %1335
  br label %2785

; <label>:1341                                    ; preds = %1304, %646
  %1342 = phi float [ %1332, %1304 ], [ %647, %646 ]
  %1343 = phi float [ %1333, %1304 ], [ %648, %646 ]
  %1344 = phi float [ %1334, %1304 ], [ %649, %646 ]
  %1345 = phi float [ %1335, %1304 ], [ %650, %646 ]
  %1346 = phi float [ %1337, %1304 ], [ %651, %646 ]
  %1347 = add nuw nsw i32 %652, 1
  %1348 = icmp ult i32 %1347, 3
  br i1 %1348, label %646, label %1349

; <label>:1349                                    ; preds = %1341
  br label %1350

; <label>:1350                                    ; preds = %1349, %643
  %1351 = phi float [ 0.000000e+00, %643 ], [ %1342, %1349 ]
  %1352 = phi float [ 0.000000e+00, %643 ], [ %1343, %1349 ]
  %1353 = phi float [ 0.000000e+00, %643 ], [ %1344, %1349 ]
  %1354 = phi float [ 0.000000e+00, %643 ], [ %1345, %1349 ]
  %1355 = fsub fast float 1.000000e+00, %1354
  br label %2785

; <label>:1356                                    ; preds = %639
  %1357 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1358 = extractvalue %dx.types.CBufRet.f32 %1357, 3
  %1359 = fmul fast float %188, 4.000000e+00
  %1360 = fmul fast float %1359, %1358
  %1361 = fmul fast float %189, 4.000000e+00
  %1362 = fmul fast float %1361, %1358
  %1363 = fmul fast float %190, 4.000000e+00
  %1364 = fmul fast float %1363, %1358
  %1365 = fadd fast float %1360, %195
  %1366 = fadd fast float %1362, %196
  %1367 = fadd fast float %1364, %197
  %1368 = fmul fast float %1365, %199
  %1369 = call float @dx.op.tertiary.f32(i32 46, float %1366, float %203, float %1368)  ; FMad(a,b,c)
  %1370 = call float @dx.op.tertiary.f32(i32 46, float %1367, float %207, float %1369)  ; FMad(a,b,c)
  %1371 = fadd fast float %1370, %211
  %1372 = fmul fast float %1365, %200
  %1373 = call float @dx.op.tertiary.f32(i32 46, float %1366, float %204, float %1372)  ; FMad(a,b,c)
  %1374 = call float @dx.op.tertiary.f32(i32 46, float %1367, float %208, float %1373)  ; FMad(a,b,c)
  %1375 = fadd fast float %1374, %212
  %1376 = fmul fast float %1365, %201
  %1377 = call float @dx.op.tertiary.f32(i32 46, float %1366, float %205, float %1376)  ; FMad(a,b,c)
  %1378 = call float @dx.op.tertiary.f32(i32 46, float %1367, float %209, float %1377)  ; FMad(a,b,c)
  %1379 = fadd fast float %1378, %213
  %1380 = fmul fast float %294, %1371
  %1381 = fmul fast float %294, %1375
  %1382 = fmul fast float %294, %1379
  %1383 = call float @dx.op.unary.f32(i32 27, float %1380)  ; Round_ni(value)
  %1384 = call float @dx.op.unary.f32(i32 27, float %1381)  ; Round_ni(value)
  %1385 = call float @dx.op.unary.f32(i32 27, float %1382)  ; Round_ni(value)
  %1386 = fmul fast float %1383, 2.500000e-01
  %1387 = fmul fast float %1384, 2.500000e-01
  %1388 = fmul fast float %1385, 2.500000e-01
  %1389 = call float @dx.op.unary.f32(i32 27, float %1386)  ; Round_ni(value)
  %1390 = call float @dx.op.unary.f32(i32 27, float %1387)  ; Round_ni(value)
  %1391 = call float @dx.op.unary.f32(i32 27, float %1388)  ; Round_ni(value)
  %1392 = fptosi float %1389 to i32
  %1393 = fptosi float %1390 to i32
  %1394 = fptosi float %1391 to i32
  %1395 = shl i32 %1392, 2
  %1396 = shl i32 %1393, 2
  %1397 = shl i32 %1394, 2
  br label %1398

; <label>:1398                                    ; preds = %1610, %1356
  %1399 = phi i32 [ 0, %1356 ], [ %1611, %1610 ]
  %1400 = phi i32 [ 0, %1356 ], [ %1612, %1610 ]
  %1401 = and i32 %1400, 4
  %1402 = shl i32 %1400, 2
  %1403 = and i32 %1402, 4
  %1404 = shl i32 %1400, 1
  %1405 = and i32 %1404, 4
  %1406 = add i32 %1403, %1395
  %1407 = add i32 %1405, %1396
  %1408 = add i32 %1401, %1397
  %1409 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1410 = extractvalue %dx.types.CBufRet.f32 %1409, 0
  %1411 = extractvalue %dx.types.CBufRet.f32 %1409, 1
  %1412 = extractvalue %dx.types.CBufRet.f32 %1409, 2
  %1413 = fptosi float %1410 to i32
  %1414 = fptosi float %1411 to i32
  %1415 = fptosi float %1412 to i32
  %1416 = add i32 %1413, -1
  %1417 = add i32 %1414, -1
  %1418 = add i32 %1415, -1
  %1419 = or i32 %1407, %1408
  %1420 = or i32 %1419, %1406
  %1421 = icmp slt i32 %1420, 0
  %1422 = icmp sgt i32 %1406, %1416
  %1423 = or i1 %1422, %1421
  %1424 = icmp sgt i32 %1407, %1417
  %1425 = or i1 %1424, %1423
  %1426 = icmp sgt i32 %1408, %1418
  %1427 = or i1 %1426, %1425
  br i1 %1427, label %1610, label %1428

; <label>:1428                                    ; preds = %1398
  %1429 = sitofp i32 %1406 to float
  %1430 = sitofp i32 %1407 to float
  %1431 = fmul fast float %1410, %1430
  %1432 = fadd fast float %1431, %1429
  %1433 = sitofp i32 %1408 to float
  %1434 = fmul fast float %1410, %1433
  %1435 = fmul fast float %1434, %1411
  %1436 = fadd fast float %1432, %1435
  %1437 = fptosi float %1436 to i32
  %1438 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1437, i32 undef)  ; BufferLoad(srv,index,wot)
  %1439 = extractvalue %dx.types.ResRet.i32 %1438, 0
  %1440 = icmp sgt i32 %1439, -1
  br i1 %1440, label %1441, label %1456

; <label>:1441                                    ; preds = %1428
  %1442 = mul i32 %1439, 10
  %1443 = add i32 %1442, 3
  %1444 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1443, i32 undef)  ; BufferLoad(srv,index,wot)
  %1445 = extractvalue %dx.types.ResRet.f32 %1444, 0
  %1446 = bitcast float %1445 to i32
  %1447 = and i32 %1446, 65535
  %1448 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1442, i32 undef)  ; BufferLoad(srv,index,wot)
  %1449 = extractvalue %dx.types.ResRet.f32 %1448, 0
  %1450 = or i32 %1442, 1
  %1451 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1450, i32 undef)  ; BufferLoad(srv,index,wot)
  %1452 = extractvalue %dx.types.ResRet.f32 %1451, 0
  %1453 = add i32 %1442, 2
  %1454 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1453, i32 undef)  ; BufferLoad(srv,index,wot)
  %1455 = extractvalue %dx.types.ResRet.f32 %1454, 0
  br label %1461

; <label>:1456                                    ; preds = %1428
  %1457 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %1458 = extractvalue %dx.types.CBufRet.f32 %1457, 0
  %1459 = extractvalue %dx.types.CBufRet.f32 %1457, 1
  %1460 = extractvalue %dx.types.CBufRet.f32 %1457, 2
  br label %1461

; <label>:1461                                    ; preds = %1456, %1441
  %1462 = phi i32 [ %1447, %1441 ], [ 0, %1456 ]
  %1463 = phi float [ %1449, %1441 ], [ %1458, %1456 ]
  %1464 = phi float [ %1452, %1441 ], [ %1459, %1456 ]
  %1465 = phi float [ %1455, %1441 ], [ %1460, %1456 ]
  %1466 = and i32 %1462, 4
  %1467 = icmp eq i32 %1466, 0
  %1468 = icmp slt i32 %1439, 0
  %1469 = or i1 %1468, %1467
  br i1 %1469, label %1610, label %1470

; <label>:1470                                    ; preds = %1461
  %1471 = mul i32 %1439, 10
  %1472 = add i32 %1471, 5
  %1473 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1472, i32 undef)  ; BufferLoad(srv,index,wot)
  %1474 = extractvalue %dx.types.ResRet.f32 %1473, 0
  %1475 = fcmp fast ogt float %1474, 0.000000e+00
  br i1 %1475, label %1476, label %1610

; <label>:1476                                    ; preds = %1470
  %1477 = fsub fast float %1463, %195
  %1478 = fsub fast float %1464, %196
  %1479 = fsub fast float %1465, %197
  %1480 = call float @dx.op.dot3.f32(i32 55, float %1477, float %1478, float %1479, float %1477, float %1478, float %1479)  ; Dot3(ax,ay,az,bx,by,bz)
  %1481 = call float @dx.op.unary.f32(i32 25, float %1480)  ; Rsqrt(value)
  %1482 = fmul fast float %1481, %1477
  %1483 = fmul fast float %1481, %1478
  %1484 = fmul fast float %1481, %1479
  %1485 = call float @dx.op.dot3.f32(i32 55, float %1482, float %1483, float %1484, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %1486 = fsub fast float %195, %1463
  %1487 = fsub fast float %196, %1464
  %1488 = fsub fast float %197, %1465
  %1489 = call float @dx.op.dot3.f32(i32 55, float %1486, float %1487, float %1488, float %1486, float %1487, float %1488)  ; Dot3(ax,ay,az,bx,by,bz)
  %1490 = call float @dx.op.unary.f32(i32 25, float %1489)  ; Rsqrt(value)
  %1491 = fmul fast float %1490, %1486
  %1492 = fmul fast float %1490, %1487
  %1493 = fmul fast float %1490, %1488
  %1494 = fmul fast float %1486, %1486
  %1495 = fmul fast float %1487, %1487
  %1496 = fadd fast float %1495, %1494
  %1497 = fmul fast float %1488, %1488
  %1498 = fadd fast float %1496, %1497
  %1499 = call float @dx.op.unary.f32(i32 24, float %1498)  ; Sqrt(value)
  %1500 = call float @dx.op.unary.f32(i32 6, float %1491)  ; FAbs(value)
  %1501 = call float @dx.op.unary.f32(i32 6, float %1492)  ; FAbs(value)
  %1502 = fadd fast float %1501, %1500
  %1503 = call float @dx.op.unary.f32(i32 6, float %1493)  ; FAbs(value)
  %1504 = fadd fast float %1502, %1503
  %1505 = fdiv fast float 1.000000e+00, %1504
  %1506 = fmul fast float %1505, %1491
  %1507 = fmul fast float %1505, %1492
  %1508 = fcmp fast ole float %1493, 0.000000e+00
  %1509 = call float @dx.op.unary.f32(i32 6, float %1507)  ; FAbs(value)
  %1510 = call float @dx.op.unary.f32(i32 6, float %1506)  ; FAbs(value)
  %1511 = fsub fast float 1.000000e+00, %1509
  %1512 = fsub fast float 1.000000e+00, %1510
  %1513 = fcmp fast oge float %1506, 0.000000e+00
  %1514 = select i1 %1513, float 1.000000e+00, float -1.000000e+00
  %1515 = fcmp fast oge float %1507, 0.000000e+00
  %1516 = select i1 %1515, float 1.000000e+00, float -1.000000e+00
  %1517 = fmul fast float %1514, %1511
  %1518 = fmul fast float %1512, %1516
  %1519 = select i1 %1508, float %1517, float %1506
  %1520 = select i1 %1508, float %1518, float %1507
  %1521 = fmul fast float %1519, 5.000000e-01
  %1522 = fmul fast float %1520, 5.000000e-01
  %1523 = fadd fast float %1521, 5.000000e-01
  %1524 = fadd fast float %1522, 5.000000e-01
  %1525 = add i32 %1471, 6
  %1526 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1525, i32 undef)  ; BufferLoad(srv,index,wot)
  %1527 = extractvalue %dx.types.ResRet.f32 %1526, 0
  %1528 = fptoui float %1527 to i32
  %1529 = add i32 %1471, 7
  %1530 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1529, i32 undef)  ; BufferLoad(srv,index,wot)
  %1531 = extractvalue %dx.types.ResRet.f32 %1530, 0
  %1532 = fptoui float %1531 to i32
  %1533 = add i32 %1471, 8
  %1534 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1533, i32 undef)  ; BufferLoad(srv,index,wot)
  %1535 = extractvalue %dx.types.ResRet.f32 %1534, 0
  %1536 = fptoui float %1535 to i32
  %1537 = add i32 %1471, 9
  %1538 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1537, i32 undef)  ; BufferLoad(srv,index,wot)
  %1539 = extractvalue %dx.types.ResRet.f32 %1538, 0
  %1540 = fptoui float %1539 to i32
  %1541 = mul i32 %1540, %1528
  %1542 = mul i32 %1540, %1532
  %1543 = sitofp i32 %1541 to float
  %1544 = sitofp i32 %1542 to float
  %1545 = sitofp i32 %1536 to float
  %1546 = sitofp i32 %1540 to float
  %1547 = fadd fast float %1546, -2.000000e+00
  %1548 = fmul fast float %1547, %1523
  %1549 = fmul fast float %1547, %1524
  %1550 = fadd fast float %1543, 1.000000e+00
  %1551 = fadd fast float %1550, %1548
  %1552 = fadd fast float %1544, 1.000000e+00
  %1553 = fadd fast float %1552, %1549
  %1554 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1555 = extractvalue %dx.types.CBufRet.f32 %1554, 0
  %1556 = extractvalue %dx.types.CBufRet.f32 %1554, 1
  %1557 = extractvalue %dx.types.CBufRet.f32 %1554, 3
  %1558 = fmul fast float %1557, %1555
  %1559 = fmul fast float %1557, %1556
  %1560 = fmul fast float %1558, %1551
  %1561 = fmul fast float %1559, %1553
  %1562 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %1563 = extractvalue %dx.types.CBufRet.f32 %1562, 0
  %1564 = fadd fast float %1563, %1545
  %1565 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %10, float %1560, float %1561, float %1564, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1566 = extractvalue %dx.types.ResRet.f32 %1565, 0
  %1567 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1568 = extractvalue %dx.types.CBufRet.f32 %1567, 1
  %1569 = fadd fast float %1568, %1499
  %1570 = extractvalue %dx.types.CBufRet.f32 %1567, 0
  %1571 = fdiv fast float %1569, %1570
  %1572 = fcmp fast ogt float %1571, %1566
  br i1 %1572, label %1573, label %1598

; <label>:1573                                    ; preds = %1476
  %1574 = extractvalue %dx.types.ResRet.f32 %1565, 1
  %1575 = fmul fast float %1566, %1566
  %1576 = fsub fast float %1575, %1574
  %1577 = call float @dx.op.unary.f32(i32 6, float %1576)  ; FAbs(value)
  %1578 = fsub fast float %1571, %1566
  %1579 = fmul fast float %1578, %1578
  %1580 = fadd fast float %1577, %1579
  %1581 = fdiv fast float %1577, %1580
  %1582 = fcmp fast olt float %1577, 0x3EE9000020000000
  %1583 = fmul fast float %1571, 0x3F847AE140000000
  %1584 = fcmp fast olt float %1578, %1583
  %1585 = and i1 %1582, %1584
  %1586 = select i1 %1585, float 1.000000e+00, float %1581
  %1587 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1588 = extractvalue %dx.types.CBufRet.f32 %1587, 2
  %1589 = fsub fast float 1.000000e+00, %1588
  %1590 = fsub fast float %1586, %1588
  %1591 = fdiv fast float %1590, %1589
  %1592 = call float @dx.op.unary.f32(i32 7, float %1591)  ; Saturate(value)
  %1593 = fmul fast float %1592, 2.000000e+00
  %1594 = fsub fast float 3.000000e+00, %1593
  %1595 = fmul fast float %1592, %1592
  %1596 = fmul fast float %1595, %1594
  %1597 = call float @dx.op.binary.f32(i32 36, float %1586, float %1596)  ; FMin(a,b)
  br label %1598

; <label>:1598                                    ; preds = %1573, %1476
  %1599 = phi float [ %1597, %1573 ], [ 1.000000e+00, %1476 ]
  %1600 = fcmp fast ole float %1485, 0.000000e+00
  %1601 = fcmp fast olt float %1599, 5.000000e-01
  %1602 = and i1 %1600, %1601
  br i1 %1602, label %1610, label %1603

; <label>:1603                                    ; preds = %1598
  %1604 = fcmp fast ogt float %1485, 0.000000e+00
  %1605 = fcmp fast oge float %1599, 5.000000e-01
  %1606 = and i1 %1604, %1605
  %1607 = zext i1 %1606 to i32
  %1608 = add i32 %1399, 1
  %1609 = add i32 %1608, %1607
  br label %1610

; <label>:1610                                    ; preds = %1603, %1598, %1470, %1461, %1398
  %1611 = phi i32 [ %1399, %1598 ], [ %1609, %1603 ], [ %1399, %1461 ], [ %1399, %1398 ], [ %1399, %1470 ]
  %1612 = add nuw nsw i32 %1400, 1
  %1613 = icmp eq i32 %1612, 8
  br i1 %1613, label %1614, label %1398

; <label>:1614                                    ; preds = %1610
  %1615 = icmp ugt i32 %1611, 1
  %1616 = extractvalue %dx.types.CBufRet.f32 %1409, 3
  %1617 = fmul fast float %1616, 4.000000e+00
  br i1 %1615, label %1618, label %2149

; <label>:1618                                    ; preds = %1614
  %1619 = fdiv fast float %1371, %1617
  %1620 = fdiv fast float %1375, %1617
  %1621 = fdiv fast float %1379, %1617
  %1622 = call float @dx.op.unary.f32(i32 27, float %1619)  ; Round_ni(value)
  %1623 = call float @dx.op.unary.f32(i32 27, float %1620)  ; Round_ni(value)
  %1624 = call float @dx.op.unary.f32(i32 27, float %1621)  ; Round_ni(value)
  %1625 = fptosi float %1622 to i32
  %1626 = fptosi float %1623 to i32
  %1627 = fptosi float %1624 to i32
  %1628 = shl i32 %1625, 2
  %1629 = shl i32 %1626, 2
  %1630 = shl i32 %1627, 2
  br label %1631

; <label>:1631                                    ; preds = %1943, %1618
  %1632 = phi float [ 0.000000e+00, %1618 ], [ %1944, %1943 ]
  %1633 = phi float [ 0.000000e+00, %1618 ], [ %1945, %1943 ]
  %1634 = phi float [ 0.000000e+00, %1618 ], [ %1946, %1943 ]
  %1635 = phi float [ 0.000000e+00, %1618 ], [ %1947, %1943 ]
  %1636 = phi float [ 0.000000e+00, %1618 ], [ %1948, %1943 ]
  %1637 = phi float [ 0.000000e+00, %1618 ], [ %1949, %1943 ]
  %1638 = phi float [ 0.000000e+00, %1618 ], [ %1950, %1943 ]
  %1639 = phi float [ 0.000000e+00, %1618 ], [ %1951, %1943 ]
  %1640 = phi float [ 0.000000e+00, %1618 ], [ %1952, %1943 ]
  %1641 = phi float [ 0.000000e+00, %1618 ], [ %1953, %1943 ]
  %1642 = phi float [ 0.000000e+00, %1618 ], [ %1954, %1943 ]
  %1643 = phi float [ 0.000000e+00, %1618 ], [ %1955, %1943 ]
  %1644 = phi float [ 0.000000e+00, %1618 ], [ %1956, %1943 ]
  %1645 = phi float [ 0.000000e+00, %1618 ], [ %1957, %1943 ]
  %1646 = phi float [ 0.000000e+00, %1618 ], [ %1958, %1943 ]
  %1647 = phi float [ 0.000000e+00, %1618 ], [ %1959, %1943 ]
  %1648 = phi float [ 0.000000e+00, %1618 ], [ %1960, %1943 ]
  %1649 = phi float [ 0.000000e+00, %1618 ], [ %1961, %1943 ]
  %1650 = phi float [ 0.000000e+00, %1618 ], [ %1962, %1943 ]
  %1651 = phi float [ 0.000000e+00, %1618 ], [ %1963, %1943 ]
  %1652 = phi float [ 0.000000e+00, %1618 ], [ %1964, %1943 ]
  %1653 = phi float [ 0.000000e+00, %1618 ], [ %1965, %1943 ]
  %1654 = phi float [ 0.000000e+00, %1618 ], [ %1966, %1943 ]
  %1655 = phi float [ 0.000000e+00, %1618 ], [ %1967, %1943 ]
  %1656 = phi float [ 0.000000e+00, %1618 ], [ %1968, %1943 ]
  %1657 = phi float [ 0.000000e+00, %1618 ], [ %1969, %1943 ]
  %1658 = phi float [ 0.000000e+00, %1618 ], [ %1970, %1943 ]
  %1659 = phi float [ 0.000000e+00, %1618 ], [ %1971, %1943 ]
  %1660 = phi float [ 0.000000e+00, %1618 ], [ %1972, %1943 ]
  %1661 = phi float [ 0.000000e+00, %1618 ], [ %1973, %1943 ]
  %1662 = phi float [ 0.000000e+00, %1618 ], [ %1974, %1943 ]
  %1663 = phi i32 [ 0, %1618 ], [ %1975, %1943 ]
  %1664 = phi i32 [ 0, %1618 ], [ %1976, %1943 ]
  %1665 = and i32 %1664, 4
  %1666 = shl i32 %1664, 2
  %1667 = and i32 %1666, 4
  %1668 = shl i32 %1664, 1
  %1669 = and i32 %1668, 4
  %1670 = add i32 %1667, %1628
  %1671 = add i32 %1669, %1629
  %1672 = add i32 %1665, %1630
  %1673 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1674 = extractvalue %dx.types.CBufRet.f32 %1673, 0
  %1675 = extractvalue %dx.types.CBufRet.f32 %1673, 1
  %1676 = extractvalue %dx.types.CBufRet.f32 %1673, 2
  %1677 = fptosi float %1674 to i32
  %1678 = fptosi float %1675 to i32
  %1679 = fptosi float %1676 to i32
  %1680 = add i32 %1677, -1
  %1681 = add i32 %1678, -1
  %1682 = add i32 %1679, -1
  %1683 = or i32 %1671, %1672
  %1684 = or i32 %1683, %1670
  %1685 = icmp slt i32 %1684, 0
  %1686 = icmp sgt i32 %1670, %1680
  %1687 = or i1 %1686, %1685
  %1688 = icmp sgt i32 %1671, %1681
  %1689 = or i1 %1688, %1687
  %1690 = icmp sgt i32 %1672, %1682
  %1691 = or i1 %1690, %1689
  br i1 %1691, label %1943, label %1692

; <label>:1692                                    ; preds = %1631
  %1693 = sitofp i32 %1670 to float
  %1694 = sitofp i32 %1671 to float
  %1695 = fmul fast float %1674, %1694
  %1696 = fadd fast float %1695, %1693
  %1697 = sitofp i32 %1672 to float
  %1698 = fmul fast float %1674, %1697
  %1699 = fmul fast float %1698, %1675
  %1700 = fadd fast float %1696, %1699
  %1701 = fptosi float %1700 to i32
  %1702 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1701, i32 undef)  ; BufferLoad(srv,index,wot)
  %1703 = extractvalue %dx.types.ResRet.i32 %1702, 0
  %1704 = icmp sgt i32 %1703, -1
  br i1 %1704, label %1705, label %1720

; <label>:1705                                    ; preds = %1692
  %1706 = mul i32 %1703, 10
  %1707 = add i32 %1706, 3
  %1708 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1707, i32 undef)  ; BufferLoad(srv,index,wot)
  %1709 = extractvalue %dx.types.ResRet.f32 %1708, 0
  %1710 = bitcast float %1709 to i32
  %1711 = and i32 %1710, 65535
  %1712 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1706, i32 undef)  ; BufferLoad(srv,index,wot)
  %1713 = extractvalue %dx.types.ResRet.f32 %1712, 0
  %1714 = or i32 %1706, 1
  %1715 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1714, i32 undef)  ; BufferLoad(srv,index,wot)
  %1716 = extractvalue %dx.types.ResRet.f32 %1715, 0
  %1717 = add i32 %1706, 2
  %1718 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1717, i32 undef)  ; BufferLoad(srv,index,wot)
  %1719 = extractvalue %dx.types.ResRet.f32 %1718, 0
  br label %1725

; <label>:1720                                    ; preds = %1692
  %1721 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %1722 = extractvalue %dx.types.CBufRet.f32 %1721, 0
  %1723 = extractvalue %dx.types.CBufRet.f32 %1721, 1
  %1724 = extractvalue %dx.types.CBufRet.f32 %1721, 2
  br label %1725

; <label>:1725                                    ; preds = %1720, %1705
  %1726 = phi i32 [ %1711, %1705 ], [ 0, %1720 ]
  %1727 = phi float [ %1713, %1705 ], [ %1722, %1720 ]
  %1728 = phi float [ %1716, %1705 ], [ %1723, %1720 ]
  %1729 = phi float [ %1719, %1705 ], [ %1724, %1720 ]
  %1730 = and i32 %1726, 4
  %1731 = icmp eq i32 %1730, 0
  br i1 %1731, label %1943, label %1732

; <label>:1732                                    ; preds = %1725
  %1733 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 0)  ; BufferLoad(srv,index,wot)
  %1734 = extractvalue %dx.types.ResRet.f32 %1733, 0
  %1735 = extractvalue %dx.types.ResRet.f32 %1733, 1
  %1736 = extractvalue %dx.types.ResRet.f32 %1733, 2
  %1737 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 12)  ; BufferLoad(srv,index,wot)
  %1738 = extractvalue %dx.types.ResRet.f32 %1737, 0
  %1739 = extractvalue %dx.types.ResRet.f32 %1737, 1
  %1740 = extractvalue %dx.types.ResRet.f32 %1737, 2
  %1741 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 24)  ; BufferLoad(srv,index,wot)
  %1742 = extractvalue %dx.types.ResRet.f32 %1741, 0
  %1743 = extractvalue %dx.types.ResRet.f32 %1741, 1
  %1744 = extractvalue %dx.types.ResRet.f32 %1741, 2
  %1745 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 36)  ; BufferLoad(srv,index,wot)
  %1746 = extractvalue %dx.types.ResRet.f32 %1745, 0
  %1747 = extractvalue %dx.types.ResRet.f32 %1745, 1
  %1748 = extractvalue %dx.types.ResRet.f32 %1745, 2
  %1749 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 48)  ; BufferLoad(srv,index,wot)
  %1750 = extractvalue %dx.types.ResRet.f32 %1749, 0
  %1751 = extractvalue %dx.types.ResRet.f32 %1749, 1
  %1752 = extractvalue %dx.types.ResRet.f32 %1749, 2
  %1753 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 60)  ; BufferLoad(srv,index,wot)
  %1754 = extractvalue %dx.types.ResRet.f32 %1753, 0
  %1755 = extractvalue %dx.types.ResRet.f32 %1753, 1
  %1756 = extractvalue %dx.types.ResRet.f32 %1753, 2
  %1757 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 72)  ; BufferLoad(srv,index,wot)
  %1758 = extractvalue %dx.types.ResRet.f32 %1757, 0
  %1759 = extractvalue %dx.types.ResRet.f32 %1757, 1
  %1760 = extractvalue %dx.types.ResRet.f32 %1757, 2
  %1761 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 84)  ; BufferLoad(srv,index,wot)
  %1762 = extractvalue %dx.types.ResRet.f32 %1761, 0
  %1763 = extractvalue %dx.types.ResRet.f32 %1761, 1
  %1764 = extractvalue %dx.types.ResRet.f32 %1761, 2
  %1765 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %1703, i32 96)  ; BufferLoad(srv,index,wot)
  %1766 = extractvalue %dx.types.ResRet.f32 %1765, 0
  %1767 = extractvalue %dx.types.ResRet.f32 %1765, 1
  %1768 = extractvalue %dx.types.ResRet.f32 %1765, 2
  %1769 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 0
  %1770 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 0
  %1771 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 0
  store float %1640, float* %1769, align 4
  store float %1649, float* %1770, align 4
  store float %1658, float* %1771, align 4
  %1772 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 1
  %1773 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 1
  %1774 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 1
  store float %1639, float* %1772, align 4
  store float %1648, float* %1773, align 4
  store float %1657, float* %1774, align 4
  %1775 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 2
  %1776 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 2
  %1777 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 2
  store float %1638, float* %1775, align 4
  store float %1647, float* %1776, align 4
  store float %1656, float* %1777, align 4
  %1778 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 3
  %1779 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 3
  %1780 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 3
  store float %1637, float* %1778, align 4
  store float %1646, float* %1779, align 4
  store float %1655, float* %1780, align 4
  %1781 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 4
  %1782 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 4
  %1783 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 4
  store float %1636, float* %1781, align 4
  store float %1645, float* %1782, align 4
  store float %1654, float* %1783, align 4
  %1784 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 5
  %1785 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 5
  %1786 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 5
  store float %1635, float* %1784, align 4
  store float %1644, float* %1785, align 4
  store float %1653, float* %1786, align 4
  %1787 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 6
  %1788 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 6
  %1789 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 6
  store float %1634, float* %1787, align 4
  store float %1643, float* %1788, align 4
  store float %1652, float* %1789, align 4
  %1790 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 7
  %1791 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 7
  %1792 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 7
  store float %1633, float* %1790, align 4
  store float %1642, float* %1791, align 4
  store float %1651, float* %1792, align 4
  %1793 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 8
  %1794 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 8
  %1795 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 8
  store float %1632, float* %1793, align 4
  store float %1641, float* %1794, align 4
  store float %1650, float* %1795, align 4
  %1796 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 0
  %1797 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 0
  %1798 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 0
  store float %1734, float* %1796, align 4
  store float %1735, float* %1797, align 4
  store float %1736, float* %1798, align 4
  %1799 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 1
  %1800 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 1
  %1801 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 1
  store float %1738, float* %1799, align 4
  store float %1739, float* %1800, align 4
  store float %1740, float* %1801, align 4
  %1802 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 2
  %1803 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 2
  %1804 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 2
  store float %1742, float* %1802, align 4
  store float %1743, float* %1803, align 4
  store float %1744, float* %1804, align 4
  %1805 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 3
  %1806 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 3
  %1807 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 3
  store float %1746, float* %1805, align 4
  store float %1747, float* %1806, align 4
  store float %1748, float* %1807, align 4
  %1808 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 4
  %1809 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 4
  %1810 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 4
  store float %1750, float* %1808, align 4
  store float %1751, float* %1809, align 4
  store float %1752, float* %1810, align 4
  %1811 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 5
  %1812 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 5
  %1813 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 5
  store float %1754, float* %1811, align 4
  store float %1755, float* %1812, align 4
  store float %1756, float* %1813, align 4
  %1814 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 6
  %1815 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 6
  %1816 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 6
  store float %1758, float* %1814, align 4
  store float %1759, float* %1815, align 4
  store float %1760, float* %1816, align 4
  %1817 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 7
  %1818 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 7
  %1819 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 7
  store float %1762, float* %1817, align 4
  store float %1763, float* %1818, align 4
  store float %1764, float* %1819, align 4
  %1820 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 8
  %1821 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 8
  %1822 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 8
  store float %1766, float* %1820, align 4
  store float %1767, float* %1821, align 4
  store float %1768, float* %1822, align 4
  %1823 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 0
  %1824 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 0
  %1825 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 0
  %1826 = load float, float* %1823, align 4
  %1827 = load float, float* %1824, align 4
  %1828 = load float, float* %1825, align 4
  %1829 = fadd fast float %1826, %1734
  %1830 = fadd fast float %1827, %1735
  %1831 = fadd fast float %1828, %1736
  store float %1829, float* %1823, align 4
  store float %1830, float* %1824, align 4
  store float %1831, float* %1825, align 4
  br label %1832

; <label>:1832                                    ; preds = %1832, %1732
  %1833 = phi i32 [ 1, %1732 ], [ %1849, %1832 ]
  %1834 = getelementptr [9 x float], [9 x float]* %19, i32 0, i32 %1833
  %1835 = load float, float* %1834, align 4
  %1836 = getelementptr [9 x float], [9 x float]* %20, i32 0, i32 %1833
  %1837 = load float, float* %1836, align 4
  %1838 = getelementptr [9 x float], [9 x float]* %21, i32 0, i32 %1833
  %1839 = load float, float* %1838, align 4
  %1840 = getelementptr [9 x float], [9 x float]* %16, i32 0, i32 %1833
  %1841 = getelementptr [9 x float], [9 x float]* %17, i32 0, i32 %1833
  %1842 = getelementptr [9 x float], [9 x float]* %18, i32 0, i32 %1833
  %1843 = load float, float* %1840, align 4
  %1844 = load float, float* %1841, align 4
  %1845 = load float, float* %1842, align 4
  %1846 = fadd fast float %1843, %1835
  %1847 = fadd fast float %1844, %1837
  %1848 = fadd fast float %1845, %1839
  store float %1846, float* %1840, align 4
  store float %1847, float* %1841, align 4
  store float %1848, float* %1842, align 4
  %1849 = add nuw nsw i32 %1833, 1
  %1850 = icmp eq i32 %1849, 9
  br i1 %1850, label %1851, label %1832

; <label>:1851                                    ; preds = %1832
  %1852 = load float, float* %1769, align 4
  %1853 = load float, float* %1770, align 4
  %1854 = load float, float* %1771, align 4
  %1855 = load float, float* %1772, align 4
  %1856 = load float, float* %1773, align 4
  %1857 = load float, float* %1774, align 4
  %1858 = load float, float* %1775, align 4
  %1859 = load float, float* %1776, align 4
  %1860 = load float, float* %1777, align 4
  %1861 = load float, float* %1778, align 4
  %1862 = load float, float* %1779, align 4
  %1863 = load float, float* %1780, align 4
  %1864 = load float, float* %1781, align 4
  %1865 = load float, float* %1782, align 4
  %1866 = load float, float* %1783, align 4
  %1867 = load float, float* %1784, align 4
  %1868 = load float, float* %1785, align 4
  %1869 = load float, float* %1786, align 4
  %1870 = load float, float* %1787, align 4
  %1871 = load float, float* %1788, align 4
  %1872 = load float, float* %1789, align 4
  %1873 = load float, float* %1790, align 4
  %1874 = load float, float* %1791, align 4
  %1875 = load float, float* %1792, align 4
  %1876 = load float, float* %1793, align 4
  %1877 = load float, float* %1794, align 4
  %1878 = load float, float* %1795, align 4
  %1879 = fadd fast float %1727, %1659
  %1880 = fadd fast float %1728, %1660
  %1881 = fadd fast float %1729, %1661
  %1882 = mul nsw i32 %1703, 9
  %1883 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1882, i32 undef)  ; BufferLoad(srv,index,wot)
  %1884 = extractvalue %dx.types.ResRet.f32 %1883, 0
  %1885 = add i32 %1882, 1
  %1886 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1885, i32 undef)  ; BufferLoad(srv,index,wot)
  %1887 = extractvalue %dx.types.ResRet.f32 %1886, 0
  %1888 = add i32 %1882, 2
  %1889 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1888, i32 undef)  ; BufferLoad(srv,index,wot)
  %1890 = extractvalue %dx.types.ResRet.f32 %1889, 0
  %1891 = add i32 %1882, 3
  %1892 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1891, i32 undef)  ; BufferLoad(srv,index,wot)
  %1893 = extractvalue %dx.types.ResRet.f32 %1892, 0
  %1894 = add i32 %1882, 4
  %1895 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1894, i32 undef)  ; BufferLoad(srv,index,wot)
  %1896 = extractvalue %dx.types.ResRet.f32 %1895, 0
  %1897 = add i32 %1882, 5
  %1898 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1897, i32 undef)  ; BufferLoad(srv,index,wot)
  %1899 = extractvalue %dx.types.ResRet.f32 %1898, 0
  %1900 = add i32 %1882, 6
  %1901 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1900, i32 undef)  ; BufferLoad(srv,index,wot)
  %1902 = extractvalue %dx.types.ResRet.f32 %1901, 0
  %1903 = add i32 %1882, 7
  %1904 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1903, i32 undef)  ; BufferLoad(srv,index,wot)
  %1905 = extractvalue %dx.types.ResRet.f32 %1904, 0
  %1906 = add i32 %1882, 8
  %1907 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %1906, i32 undef)  ; BufferLoad(srv,index,wot)
  %1908 = extractvalue %dx.types.ResRet.f32 %1907, 0
  %1909 = fmul fast float %1884, 0x3FD20DD820000000
  %1910 = fmul fast float %1887, %189
  %1911 = fmul fast float %1890, %190
  %1912 = fmul fast float %188, 0x3FD4D8D900000000
  %1913 = fmul fast float %1912, %1893
  %1914 = fmul fast float %189, %188
  %1915 = fmul fast float %1914, %1896
  %1916 = fmul fast float %190, %189
  %1917 = fmul fast float %1916, %1899
  %1918 = fmul fast float %190, %190
  %1919 = fmul fast float %1918, 0x3FCE4712C0000000
  %1920 = fadd fast float %1919, 0xBFB42F61E0000000
  %1921 = fmul fast float %1920, %1902
  %1922 = fmul fast float %188, %190
  %1923 = fmul fast float %1922, %1905
  %1924 = fmul fast float %188, %188
  %1925 = fmul fast float %189, %189
  %1926 = fsub fast float %1924, %1925
  %1927 = fmul fast float %1926, 0x3FC17B13A0000000
  %1928 = fmul fast float %1927, %1908
  %1929 = fsub fast float %1915, %1917
  %1930 = fsub fast float %1929, %1923
  %1931 = fmul fast float %1930, 0x3FD17B13A0000000
  %1932 = fsub fast float %1911, %1910
  %1933 = fmul fast float %1932, 0x3FD4D8D900000000
  %1934 = fsub fast float %1909, %1913
  %1935 = fadd fast float %1934, %1921
  %1936 = fadd fast float %1935, %1928
  %1937 = fadd fast float %1936, %1931
  %1938 = fadd fast float %1937, %1933
  %1939 = call float @dx.op.unary.f32(i32 7, float %1938)  ; Saturate(value)
  %1940 = fadd fast float %1662, 1.000000e+00
  %1941 = fsub fast float %1940, %1939
  %1942 = add i32 %1663, 1
  br label %1943

; <label>:1943                                    ; preds = %1851, %1725, %1631
  %1944 = phi float [ %1876, %1851 ], [ %1632, %1725 ], [ %1632, %1631 ]
  %1945 = phi float [ %1873, %1851 ], [ %1633, %1725 ], [ %1633, %1631 ]
  %1946 = phi float [ %1870, %1851 ], [ %1634, %1725 ], [ %1634, %1631 ]
  %1947 = phi float [ %1867, %1851 ], [ %1635, %1725 ], [ %1635, %1631 ]
  %1948 = phi float [ %1864, %1851 ], [ %1636, %1725 ], [ %1636, %1631 ]
  %1949 = phi float [ %1861, %1851 ], [ %1637, %1725 ], [ %1637, %1631 ]
  %1950 = phi float [ %1858, %1851 ], [ %1638, %1725 ], [ %1638, %1631 ]
  %1951 = phi float [ %1855, %1851 ], [ %1639, %1725 ], [ %1639, %1631 ]
  %1952 = phi float [ %1852, %1851 ], [ %1640, %1725 ], [ %1640, %1631 ]
  %1953 = phi float [ %1877, %1851 ], [ %1641, %1725 ], [ %1641, %1631 ]
  %1954 = phi float [ %1874, %1851 ], [ %1642, %1725 ], [ %1642, %1631 ]
  %1955 = phi float [ %1871, %1851 ], [ %1643, %1725 ], [ %1643, %1631 ]
  %1956 = phi float [ %1868, %1851 ], [ %1644, %1725 ], [ %1644, %1631 ]
  %1957 = phi float [ %1865, %1851 ], [ %1645, %1725 ], [ %1645, %1631 ]
  %1958 = phi float [ %1862, %1851 ], [ %1646, %1725 ], [ %1646, %1631 ]
  %1959 = phi float [ %1859, %1851 ], [ %1647, %1725 ], [ %1647, %1631 ]
  %1960 = phi float [ %1856, %1851 ], [ %1648, %1725 ], [ %1648, %1631 ]
  %1961 = phi float [ %1853, %1851 ], [ %1649, %1725 ], [ %1649, %1631 ]
  %1962 = phi float [ %1878, %1851 ], [ %1650, %1725 ], [ %1650, %1631 ]
  %1963 = phi float [ %1875, %1851 ], [ %1651, %1725 ], [ %1651, %1631 ]
  %1964 = phi float [ %1872, %1851 ], [ %1652, %1725 ], [ %1652, %1631 ]
  %1965 = phi float [ %1869, %1851 ], [ %1653, %1725 ], [ %1653, %1631 ]
  %1966 = phi float [ %1866, %1851 ], [ %1654, %1725 ], [ %1654, %1631 ]
  %1967 = phi float [ %1863, %1851 ], [ %1655, %1725 ], [ %1655, %1631 ]
  %1968 = phi float [ %1860, %1851 ], [ %1656, %1725 ], [ %1656, %1631 ]
  %1969 = phi float [ %1857, %1851 ], [ %1657, %1725 ], [ %1657, %1631 ]
  %1970 = phi float [ %1854, %1851 ], [ %1658, %1725 ], [ %1658, %1631 ]
  %1971 = phi float [ %1879, %1851 ], [ %1659, %1725 ], [ %1659, %1631 ]
  %1972 = phi float [ %1880, %1851 ], [ %1660, %1725 ], [ %1660, %1631 ]
  %1973 = phi float [ %1881, %1851 ], [ %1661, %1725 ], [ %1661, %1631 ]
  %1974 = phi float [ %1941, %1851 ], [ %1662, %1725 ], [ %1662, %1631 ]
  %1975 = phi i32 [ %1942, %1851 ], [ %1663, %1725 ], [ %1663, %1631 ]
  %1976 = add nuw nsw i32 %1664, 1
  %1977 = icmp eq i32 %1976, 8
  br i1 %1977, label %1978, label %1631

; <label>:1978                                    ; preds = %1943
  %1979 = icmp eq i32 %1975, 0
  br i1 %1979, label %2785, label %1980

; <label>:1980                                    ; preds = %1978
  %1981 = uitofp i32 %1975 to float
  %1982 = fdiv fast float 1.000000e+00, %1981
  %1983 = fmul fast float %1952, %1982
  %1984 = fmul fast float %1961, %1982
  %1985 = fmul fast float %1970, %1982
  %1986 = fmul fast float %1951, %1982
  %1987 = fmul fast float %1960, %1982
  %1988 = fmul fast float %1969, %1982
  %1989 = fmul fast float %1950, %1982
  %1990 = fmul fast float %1959, %1982
  %1991 = fmul fast float %1968, %1982
  %1992 = fmul fast float %1949, %1982
  %1993 = fmul fast float %1958, %1982
  %1994 = fmul fast float %1967, %1982
  %1995 = fmul fast float %1948, %1982
  %1996 = fmul fast float %1957, %1982
  %1997 = fmul fast float %1966, %1982
  %1998 = fmul fast float %1947, %1982
  %1999 = fmul fast float %1956, %1982
  %2000 = fmul fast float %1965, %1982
  %2001 = fmul fast float %1946, %1982
  %2002 = fmul fast float %1955, %1982
  %2003 = fmul fast float %1964, %1982
  %2004 = fmul fast float %1945, %1982
  %2005 = fmul fast float %1954, %1982
  %2006 = fmul fast float %1963, %1982
  %2007 = fmul fast float %1944, %1982
  %2008 = fmul fast float %1953, %1982
  %2009 = fmul fast float %1962, %1982
  %2010 = fdiv fast float %1971, %1981
  %2011 = fdiv fast float %1972, %1981
  %2012 = fdiv fast float %1973, %1981
  %2013 = fdiv fast float %1974, %1981
  %2014 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %2015 = extractvalue %dx.types.CBufRet.f32 %2014, 0
  %2016 = fmul fast float %2015, %188
  %2017 = fmul fast float %2015, %189
  %2018 = fmul fast float %2015, %190
  %2019 = fsub fast float %195, %2010
  %2020 = fadd fast float %2019, %2016
  %2021 = fsub fast float %196, %2011
  %2022 = fadd fast float %2021, %2017
  %2023 = fsub fast float %197, %2012
  %2024 = fadd fast float %2023, %2018
  %2025 = call float @dx.op.dot3.f32(i32 55, float %2020, float %2022, float %2024, float %2020, float %2022, float %2024)  ; Dot3(ax,ay,az,bx,by,bz)
  %2026 = call float @dx.op.unary.f32(i32 25, float %2025)  ; Rsqrt(value)
  %2027 = fmul fast float %2020, %2026
  %2028 = fmul fast float %2022, %2026
  %2029 = fmul fast float %2024, %2026
  %2030 = fadd fast float %2027, %188
  %2031 = fadd fast float %2028, %189
  %2032 = fadd fast float %2029, %190
  %2033 = fmul fast float %2030, 5.000000e-01
  %2034 = fmul fast float %2031, 5.000000e-01
  %2035 = fmul fast float %2032, 5.000000e-01
  %2036 = fmul fast float %1983, 0x3FEC5BF980000000
  %2037 = fmul fast float %1984, 0x3FEC5BF980000000
  %2038 = fmul fast float %1985, 0x3FEC5BF980000000
  %2039 = fmul fast float %2001, 0x3FCFB4E740000000
  %2040 = fmul fast float %2002, 0x3FCFB4E740000000
  %2041 = fmul fast float %2003, 0x3FCFB4E740000000
  %2042 = fmul fast float %2007, 0x3FDB756EC0000000
  %2043 = fmul fast float %2008, 0x3FDB756EC0000000
  %2044 = fmul fast float %2009, 0x3FDB756EC0000000
  %2045 = fmul fast float %2034, %1986
  %2046 = fmul fast float %2034, %1987
  %2047 = fmul fast float %2034, %1988
  %2048 = fmul fast float %2035, %1989
  %2049 = fmul fast float %2035, %1990
  %2050 = fmul fast float %2035, %1991
  %2051 = fmul fast float %2033, %1992
  %2052 = fmul fast float %2033, %1993
  %2053 = fmul fast float %2033, %1994
  %2054 = fmul fast float %2033, %2034
  %2055 = fmul fast float %2054, %1995
  %2056 = fmul fast float %2054, %1996
  %2057 = fmul fast float %2054, %1997
  %2058 = fmul fast float %2034, %2035
  %2059 = fmul fast float %2058, %1998
  %2060 = fsub fast float -0.000000e+00, %2059
  %2061 = fmul fast float %2058, %1999
  %2062 = fsub fast float -0.000000e+00, %2061
  %2063 = fmul fast float %2058, %2000
  %2064 = fsub fast float -0.000000e+00, %2063
  %2065 = fmul fast float %2032, 1.500000e+00
  %2066 = fmul fast float %2065, %2035
  %2067 = fadd fast float %2066, -1.000000e+00
  %2068 = fmul fast float %2039, %2067
  %2069 = fmul fast float %2040, %2067
  %2070 = fmul fast float %2041, %2067
  %2071 = fmul fast float %2035, %2033
  %2072 = fmul fast float %2071, %2004
  %2073 = fmul fast float %2071, %2005
  %2074 = fmul fast float %2071, %2006
  %2075 = fmul fast float %2033, %2033
  %2076 = fmul fast float %2034, %2034
  %2077 = fsub fast float %2075, %2076
  %2078 = fmul fast float %2042, %2077
  %2079 = fmul fast float %2043, %2077
  %2080 = fmul fast float %2044, %2077
  %2081 = fsub fast float %2060, %2072
  %2082 = fadd fast float %2081, %2055
  %2083 = fmul fast float %2082, 0x3FEB756EC0000000
  %2084 = fsub fast float %2048, %2051
  %2085 = fsub fast float %2084, %2045
  %2086 = fmul fast float %2085, 0x3FF05F8CA0000000
  %2087 = fadd fast float %2068, %2036
  %2088 = fadd fast float %2087, %2078
  %2089 = fadd fast float %2088, %2083
  %2090 = fadd fast float %2089, %2086
  %2091 = fsub fast float %2062, %2073
  %2092 = fadd fast float %2091, %2056
  %2093 = fmul fast float %2092, 0x3FEB756EC0000000
  %2094 = fsub fast float %2049, %2052
  %2095 = fsub fast float %2094, %2046
  %2096 = fmul fast float %2095, 0x3FF05F8CA0000000
  %2097 = fadd fast float %2069, %2037
  %2098 = fadd fast float %2097, %2079
  %2099 = fadd fast float %2098, %2093
  %2100 = fadd fast float %2099, %2096
  %2101 = fsub fast float %2064, %2074
  %2102 = fadd fast float %2101, %2057
  %2103 = fmul fast float %2102, 0x3FEB756EC0000000
  %2104 = fsub fast float %2050, %2053
  %2105 = fsub fast float %2104, %2047
  %2106 = fmul fast float %2105, 0x3FF05F8CA0000000
  %2107 = fadd fast float %2070, %2038
  %2108 = fadd fast float %2107, %2080
  %2109 = fadd fast float %2108, %2103
  %2110 = fadd fast float %2109, %2106
  %2111 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2090)  ; FMax(a,b)
  %2112 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2100)  ; FMax(a,b)
  %2113 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2110)  ; FMax(a,b)
  %2114 = fsub fast float %2010, %195
  %2115 = fsub fast float %2011, %196
  %2116 = fsub fast float %2012, %197
  %2117 = fmul fast float %2114, %2114
  %2118 = fmul fast float %2115, %2115
  %2119 = fadd fast float %2118, %2117
  %2120 = fmul fast float %2116, %2116
  %2121 = fadd fast float %2119, %2120
  %2122 = call float @dx.op.unary.f32(i32 24, float %2121)  ; Sqrt(value)
  %2123 = call float @dx.op.dot3.f32(i32 55, float %2114, float %2115, float %2116, float %2114, float %2115, float %2116)  ; Dot3(ax,ay,az,bx,by,bz)
  %2124 = call float @dx.op.unary.f32(i32 25, float %2123)  ; Rsqrt(value)
  %2125 = fmul fast float %2124, %2114
  %2126 = fmul fast float %2124, %2115
  %2127 = fmul fast float %2124, %2116
  %2128 = call float @dx.op.dot3.f32(i32 55, float %2125, float %2126, float %2127, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %2129 = fmul fast float %2128, %2122
  %2130 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2129)  ; FMax(a,b)
  %2131 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2132 = extractvalue %dx.types.CBufRet.f32 %2131, 3
  %2133 = fmul fast float %2132, 8.000000e+00
  %2134 = fadd fast float %2133, %2130
  %2135 = fmul fast float %2133, %2133
  %2136 = fmul fast float %2134, %2134
  %2137 = fdiv fast float %2135, %2136
  %2138 = call float @dx.op.unary.f32(i32 7, float %2137)  ; Saturate(value)
  %2139 = call float @dx.op.unary.f32(i32 7, float %2138)  ; Saturate(value)
  %2140 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2141 = extractvalue %dx.types.CBufRet.f32 %2140, 0
  %2142 = fadd fast float %2139, -1.000000e+00
  %2143 = fmul fast float %2141, %2142
  %2144 = fadd fast float %2143, 1.000000e+00
  %2145 = fmul fast float %2144, %2111
  %2146 = fmul fast float %2144, %2112
  %2147 = fmul fast float %2144, %2113
  %2148 = fsub fast float 1.000000e+00, %2013
  br label %2785

; <label>:2149                                    ; preds = %1614
  %2150 = fadd fast float %1617, %196
  %2151 = call float @dx.op.tertiary.f32(i32 46, float %2150, float %203, float %214)  ; FMad(a,b,c)
  %2152 = call float @dx.op.tertiary.f32(i32 46, float %197, float %207, float %2151)  ; FMad(a,b,c)
  %2153 = fadd fast float %2152, %211
  %2154 = call float @dx.op.tertiary.f32(i32 46, float %2150, float %204, float %218)  ; FMad(a,b,c)
  %2155 = call float @dx.op.tertiary.f32(i32 46, float %197, float %208, float %2154)  ; FMad(a,b,c)
  %2156 = fadd fast float %2155, %212
  %2157 = call float @dx.op.tertiary.f32(i32 46, float %2150, float %205, float %222)  ; FMad(a,b,c)
  %2158 = call float @dx.op.tertiary.f32(i32 46, float %197, float %209, float %2157)  ; FMad(a,b,c)
  %2159 = fadd fast float %2158, %213
  %2160 = fmul fast float %294, %2153
  %2161 = fmul fast float %294, %2156
  %2162 = fmul fast float %294, %2159
  %2163 = call float @dx.op.unary.f32(i32 27, float %2160)  ; Round_ni(value)
  %2164 = call float @dx.op.unary.f32(i32 27, float %2161)  ; Round_ni(value)
  %2165 = call float @dx.op.unary.f32(i32 27, float %2162)  ; Round_ni(value)
  %2166 = fmul fast float %2163, 2.500000e-01
  %2167 = fmul fast float %2164, 2.500000e-01
  %2168 = fmul fast float %2165, 2.500000e-01
  %2169 = call float @dx.op.unary.f32(i32 27, float %2166)  ; Round_ni(value)
  %2170 = call float @dx.op.unary.f32(i32 27, float %2167)  ; Round_ni(value)
  %2171 = call float @dx.op.unary.f32(i32 27, float %2168)  ; Round_ni(value)
  %2172 = fptosi float %2169 to i32
  %2173 = fptosi float %2170 to i32
  %2174 = fptosi float %2171 to i32
  %2175 = shl i32 %2172, 2
  %2176 = shl i32 %2173, 2
  %2177 = shl i32 %2174, 2
  br label %2178

; <label>:2178                                    ; preds = %2242, %2149
  %2179 = phi i32 [ 0, %2149 ], [ %2243, %2242 ]
  %2180 = phi i32 [ 0, %2149 ], [ %2244, %2242 ]
  %2181 = and i32 %2180, 4
  %2182 = shl i32 %2180, 2
  %2183 = and i32 %2182, 4
  %2184 = shl i32 %2180, 1
  %2185 = and i32 %2184, 4
  %2186 = add i32 %2183, %2175
  %2187 = add i32 %2185, %2176
  %2188 = add i32 %2181, %2177
  %2189 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2190 = extractvalue %dx.types.CBufRet.f32 %2189, 0
  %2191 = extractvalue %dx.types.CBufRet.f32 %2189, 1
  %2192 = extractvalue %dx.types.CBufRet.f32 %2189, 2
  %2193 = fptosi float %2190 to i32
  %2194 = fptosi float %2191 to i32
  %2195 = fptosi float %2192 to i32
  %2196 = add i32 %2193, -1
  %2197 = add i32 %2194, -1
  %2198 = add i32 %2195, -1
  %2199 = or i32 %2187, %2188
  %2200 = or i32 %2199, %2186
  %2201 = icmp slt i32 %2200, 0
  %2202 = icmp sgt i32 %2186, %2196
  %2203 = or i1 %2202, %2201
  %2204 = icmp sgt i32 %2187, %2197
  %2205 = or i1 %2204, %2203
  %2206 = icmp sgt i32 %2188, %2198
  %2207 = or i1 %2206, %2205
  br i1 %2207, label %2242, label %2208

; <label>:2208                                    ; preds = %2178
  %2209 = sitofp i32 %2186 to float
  %2210 = sitofp i32 %2187 to float
  %2211 = fmul fast float %2190, %2210
  %2212 = fadd fast float %2211, %2209
  %2213 = sitofp i32 %2188 to float
  %2214 = fmul fast float %2190, %2213
  %2215 = fmul fast float %2214, %2191
  %2216 = fadd fast float %2212, %2215
  %2217 = fptosi float %2216 to i32
  %2218 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %2217, i32 undef)  ; BufferLoad(srv,index,wot)
  %2219 = extractvalue %dx.types.ResRet.i32 %2218, 0
  %2220 = icmp sgt i32 %2219, -1
  br i1 %2220, label %2221, label %2228

; <label>:2221                                    ; preds = %2208
  %2222 = mul i32 %2219, 10
  %2223 = add i32 %2222, 3
  %2224 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2223, i32 undef)  ; BufferLoad(srv,index,wot)
  %2225 = extractvalue %dx.types.ResRet.f32 %2224, 0
  %2226 = bitcast float %2225 to i32
  %2227 = and i32 %2226, 65535
  br label %2228

; <label>:2228                                    ; preds = %2221, %2208
  %2229 = phi i32 [ %2227, %2221 ], [ 0, %2208 ]
  %2230 = and i32 %2229, 4
  %2231 = icmp eq i32 %2230, 0
  %2232 = icmp slt i32 %2219, 0
  %2233 = or i1 %2232, %2231
  br i1 %2233, label %2242, label %2234

; <label>:2234                                    ; preds = %2228
  %2235 = mul i32 %2219, 10
  %2236 = add i32 %2235, 5
  %2237 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2236, i32 undef)  ; BufferLoad(srv,index,wot)
  %2238 = extractvalue %dx.types.ResRet.f32 %2237, 0
  %2239 = fcmp fast ogt float %2238, 0.000000e+00
  br i1 %2239, label %2240, label %2242

; <label>:2240                                    ; preds = %2234
  %2241 = add i32 %2179, 1
  br label %2242

; <label>:2242                                    ; preds = %2240, %2234, %2228, %2178
  %2243 = phi i32 [ %2241, %2240 ], [ %2179, %2228 ], [ %2179, %2178 ], [ %2179, %2234 ]
  %2244 = add nuw nsw i32 %2180, 1
  %2245 = icmp eq i32 %2244, 8
  br i1 %2245, label %2246, label %2178

; <label>:2246                                    ; preds = %2242
  %2247 = icmp ugt i32 %2243, 1
  br i1 %2247, label %2248, label %2781

; <label>:2248                                    ; preds = %2246
  %2249 = extractvalue %dx.types.CBufRet.f32 %2189, 3
  %2250 = fmul fast float %2249, 4.000000e+00
  %2251 = fdiv fast float %2153, %2250
  %2252 = fdiv fast float %2156, %2250
  %2253 = fdiv fast float %2159, %2250
  %2254 = call float @dx.op.unary.f32(i32 27, float %2251)  ; Round_ni(value)
  %2255 = call float @dx.op.unary.f32(i32 27, float %2252)  ; Round_ni(value)
  %2256 = call float @dx.op.unary.f32(i32 27, float %2253)  ; Round_ni(value)
  %2257 = fptosi float %2254 to i32
  %2258 = fptosi float %2255 to i32
  %2259 = fptosi float %2256 to i32
  %2260 = shl i32 %2257, 2
  %2261 = shl i32 %2258, 2
  %2262 = shl i32 %2259, 2
  br label %2263

; <label>:2263                                    ; preds = %2575, %2248
  %2264 = phi float [ 0.000000e+00, %2248 ], [ %2576, %2575 ]
  %2265 = phi float [ 0.000000e+00, %2248 ], [ %2577, %2575 ]
  %2266 = phi float [ 0.000000e+00, %2248 ], [ %2578, %2575 ]
  %2267 = phi float [ 0.000000e+00, %2248 ], [ %2579, %2575 ]
  %2268 = phi float [ 0.000000e+00, %2248 ], [ %2580, %2575 ]
  %2269 = phi float [ 0.000000e+00, %2248 ], [ %2581, %2575 ]
  %2270 = phi float [ 0.000000e+00, %2248 ], [ %2582, %2575 ]
  %2271 = phi float [ 0.000000e+00, %2248 ], [ %2583, %2575 ]
  %2272 = phi float [ 0.000000e+00, %2248 ], [ %2584, %2575 ]
  %2273 = phi float [ 0.000000e+00, %2248 ], [ %2585, %2575 ]
  %2274 = phi float [ 0.000000e+00, %2248 ], [ %2586, %2575 ]
  %2275 = phi float [ 0.000000e+00, %2248 ], [ %2587, %2575 ]
  %2276 = phi float [ 0.000000e+00, %2248 ], [ %2588, %2575 ]
  %2277 = phi float [ 0.000000e+00, %2248 ], [ %2589, %2575 ]
  %2278 = phi float [ 0.000000e+00, %2248 ], [ %2590, %2575 ]
  %2279 = phi float [ 0.000000e+00, %2248 ], [ %2591, %2575 ]
  %2280 = phi float [ 0.000000e+00, %2248 ], [ %2592, %2575 ]
  %2281 = phi float [ 0.000000e+00, %2248 ], [ %2593, %2575 ]
  %2282 = phi float [ 0.000000e+00, %2248 ], [ %2594, %2575 ]
  %2283 = phi float [ 0.000000e+00, %2248 ], [ %2595, %2575 ]
  %2284 = phi float [ 0.000000e+00, %2248 ], [ %2596, %2575 ]
  %2285 = phi float [ 0.000000e+00, %2248 ], [ %2597, %2575 ]
  %2286 = phi float [ 0.000000e+00, %2248 ], [ %2598, %2575 ]
  %2287 = phi float [ 0.000000e+00, %2248 ], [ %2599, %2575 ]
  %2288 = phi float [ 0.000000e+00, %2248 ], [ %2600, %2575 ]
  %2289 = phi float [ 0.000000e+00, %2248 ], [ %2601, %2575 ]
  %2290 = phi float [ 0.000000e+00, %2248 ], [ %2602, %2575 ]
  %2291 = phi float [ 0.000000e+00, %2248 ], [ %2603, %2575 ]
  %2292 = phi float [ 0.000000e+00, %2248 ], [ %2604, %2575 ]
  %2293 = phi float [ 0.000000e+00, %2248 ], [ %2605, %2575 ]
  %2294 = phi float [ 0.000000e+00, %2248 ], [ %2606, %2575 ]
  %2295 = phi i32 [ 0, %2248 ], [ %2607, %2575 ]
  %2296 = phi i32 [ 0, %2248 ], [ %2608, %2575 ]
  %2297 = and i32 %2296, 4
  %2298 = shl i32 %2296, 2
  %2299 = and i32 %2298, 4
  %2300 = shl i32 %2296, 1
  %2301 = and i32 %2300, 4
  %2302 = add i32 %2299, %2260
  %2303 = add i32 %2301, %2261
  %2304 = add i32 %2297, %2262
  %2305 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2306 = extractvalue %dx.types.CBufRet.f32 %2305, 0
  %2307 = extractvalue %dx.types.CBufRet.f32 %2305, 1
  %2308 = extractvalue %dx.types.CBufRet.f32 %2305, 2
  %2309 = fptosi float %2306 to i32
  %2310 = fptosi float %2307 to i32
  %2311 = fptosi float %2308 to i32
  %2312 = add i32 %2309, -1
  %2313 = add i32 %2310, -1
  %2314 = add i32 %2311, -1
  %2315 = or i32 %2303, %2304
  %2316 = or i32 %2315, %2302
  %2317 = icmp slt i32 %2316, 0
  %2318 = icmp sgt i32 %2302, %2312
  %2319 = or i1 %2318, %2317
  %2320 = icmp sgt i32 %2303, %2313
  %2321 = or i1 %2320, %2319
  %2322 = icmp sgt i32 %2304, %2314
  %2323 = or i1 %2322, %2321
  br i1 %2323, label %2575, label %2324

; <label>:2324                                    ; preds = %2263
  %2325 = sitofp i32 %2302 to float
  %2326 = sitofp i32 %2303 to float
  %2327 = fmul fast float %2306, %2326
  %2328 = fadd fast float %2327, %2325
  %2329 = sitofp i32 %2304 to float
  %2330 = fmul fast float %2306, %2329
  %2331 = fmul fast float %2330, %2307
  %2332 = fadd fast float %2328, %2331
  %2333 = fptosi float %2332 to i32
  %2334 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %2333, i32 undef)  ; BufferLoad(srv,index,wot)
  %2335 = extractvalue %dx.types.ResRet.i32 %2334, 0
  %2336 = icmp sgt i32 %2335, -1
  br i1 %2336, label %2337, label %2352

; <label>:2337                                    ; preds = %2324
  %2338 = mul i32 %2335, 10
  %2339 = add i32 %2338, 3
  %2340 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2339, i32 undef)  ; BufferLoad(srv,index,wot)
  %2341 = extractvalue %dx.types.ResRet.f32 %2340, 0
  %2342 = bitcast float %2341 to i32
  %2343 = and i32 %2342, 65535
  %2344 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2338, i32 undef)  ; BufferLoad(srv,index,wot)
  %2345 = extractvalue %dx.types.ResRet.f32 %2344, 0
  %2346 = or i32 %2338, 1
  %2347 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2346, i32 undef)  ; BufferLoad(srv,index,wot)
  %2348 = extractvalue %dx.types.ResRet.f32 %2347, 0
  %2349 = add i32 %2338, 2
  %2350 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2349, i32 undef)  ; BufferLoad(srv,index,wot)
  %2351 = extractvalue %dx.types.ResRet.f32 %2350, 0
  br label %2357

; <label>:2352                                    ; preds = %2324
  %2353 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2354 = extractvalue %dx.types.CBufRet.f32 %2353, 0
  %2355 = extractvalue %dx.types.CBufRet.f32 %2353, 1
  %2356 = extractvalue %dx.types.CBufRet.f32 %2353, 2
  br label %2357

; <label>:2357                                    ; preds = %2352, %2337
  %2358 = phi i32 [ %2343, %2337 ], [ 0, %2352 ]
  %2359 = phi float [ %2345, %2337 ], [ %2354, %2352 ]
  %2360 = phi float [ %2348, %2337 ], [ %2355, %2352 ]
  %2361 = phi float [ %2351, %2337 ], [ %2356, %2352 ]
  %2362 = and i32 %2358, 4
  %2363 = icmp eq i32 %2362, 0
  br i1 %2363, label %2575, label %2364

; <label>:2364                                    ; preds = %2357
  %2365 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 0)  ; BufferLoad(srv,index,wot)
  %2366 = extractvalue %dx.types.ResRet.f32 %2365, 0
  %2367 = extractvalue %dx.types.ResRet.f32 %2365, 1
  %2368 = extractvalue %dx.types.ResRet.f32 %2365, 2
  %2369 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 12)  ; BufferLoad(srv,index,wot)
  %2370 = extractvalue %dx.types.ResRet.f32 %2369, 0
  %2371 = extractvalue %dx.types.ResRet.f32 %2369, 1
  %2372 = extractvalue %dx.types.ResRet.f32 %2369, 2
  %2373 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 24)  ; BufferLoad(srv,index,wot)
  %2374 = extractvalue %dx.types.ResRet.f32 %2373, 0
  %2375 = extractvalue %dx.types.ResRet.f32 %2373, 1
  %2376 = extractvalue %dx.types.ResRet.f32 %2373, 2
  %2377 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 36)  ; BufferLoad(srv,index,wot)
  %2378 = extractvalue %dx.types.ResRet.f32 %2377, 0
  %2379 = extractvalue %dx.types.ResRet.f32 %2377, 1
  %2380 = extractvalue %dx.types.ResRet.f32 %2377, 2
  %2381 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 48)  ; BufferLoad(srv,index,wot)
  %2382 = extractvalue %dx.types.ResRet.f32 %2381, 0
  %2383 = extractvalue %dx.types.ResRet.f32 %2381, 1
  %2384 = extractvalue %dx.types.ResRet.f32 %2381, 2
  %2385 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 60)  ; BufferLoad(srv,index,wot)
  %2386 = extractvalue %dx.types.ResRet.f32 %2385, 0
  %2387 = extractvalue %dx.types.ResRet.f32 %2385, 1
  %2388 = extractvalue %dx.types.ResRet.f32 %2385, 2
  %2389 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 72)  ; BufferLoad(srv,index,wot)
  %2390 = extractvalue %dx.types.ResRet.f32 %2389, 0
  %2391 = extractvalue %dx.types.ResRet.f32 %2389, 1
  %2392 = extractvalue %dx.types.ResRet.f32 %2389, 2
  %2393 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 84)  ; BufferLoad(srv,index,wot)
  %2394 = extractvalue %dx.types.ResRet.f32 %2393, 0
  %2395 = extractvalue %dx.types.ResRet.f32 %2393, 1
  %2396 = extractvalue %dx.types.ResRet.f32 %2393, 2
  %2397 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %2, i32 %2335, i32 96)  ; BufferLoad(srv,index,wot)
  %2398 = extractvalue %dx.types.ResRet.f32 %2397, 0
  %2399 = extractvalue %dx.types.ResRet.f32 %2397, 1
  %2400 = extractvalue %dx.types.ResRet.f32 %2397, 2
  %2401 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 0
  %2402 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 0
  %2403 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 0
  store float %2272, float* %2401, align 4
  store float %2281, float* %2402, align 4
  store float %2290, float* %2403, align 4
  %2404 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 1
  %2405 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 1
  %2406 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 1
  store float %2271, float* %2404, align 4
  store float %2280, float* %2405, align 4
  store float %2289, float* %2406, align 4
  %2407 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 2
  %2408 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 2
  %2409 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 2
  store float %2270, float* %2407, align 4
  store float %2279, float* %2408, align 4
  store float %2288, float* %2409, align 4
  %2410 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 3
  %2411 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 3
  %2412 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 3
  store float %2269, float* %2410, align 4
  store float %2278, float* %2411, align 4
  store float %2287, float* %2412, align 4
  %2413 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 4
  %2414 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 4
  %2415 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 4
  store float %2268, float* %2413, align 4
  store float %2277, float* %2414, align 4
  store float %2286, float* %2415, align 4
  %2416 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 5
  %2417 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 5
  %2418 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 5
  store float %2267, float* %2416, align 4
  store float %2276, float* %2417, align 4
  store float %2285, float* %2418, align 4
  %2419 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 6
  %2420 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 6
  %2421 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 6
  store float %2266, float* %2419, align 4
  store float %2275, float* %2420, align 4
  store float %2284, float* %2421, align 4
  %2422 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 7
  %2423 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 7
  %2424 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 7
  store float %2265, float* %2422, align 4
  store float %2274, float* %2423, align 4
  store float %2283, float* %2424, align 4
  %2425 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 8
  %2426 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 8
  %2427 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 8
  store float %2264, float* %2425, align 4
  store float %2273, float* %2426, align 4
  store float %2282, float* %2427, align 4
  %2428 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 0
  %2429 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 0
  %2430 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 0
  store float %2366, float* %2428, align 4
  store float %2367, float* %2429, align 4
  store float %2368, float* %2430, align 4
  %2431 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 1
  %2432 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 1
  %2433 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 1
  store float %2370, float* %2431, align 4
  store float %2371, float* %2432, align 4
  store float %2372, float* %2433, align 4
  %2434 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 2
  %2435 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 2
  %2436 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 2
  store float %2374, float* %2434, align 4
  store float %2375, float* %2435, align 4
  store float %2376, float* %2436, align 4
  %2437 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 3
  %2438 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 3
  %2439 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 3
  store float %2378, float* %2437, align 4
  store float %2379, float* %2438, align 4
  store float %2380, float* %2439, align 4
  %2440 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 4
  %2441 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 4
  %2442 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 4
  store float %2382, float* %2440, align 4
  store float %2383, float* %2441, align 4
  store float %2384, float* %2442, align 4
  %2443 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 5
  %2444 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 5
  %2445 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 5
  store float %2386, float* %2443, align 4
  store float %2387, float* %2444, align 4
  store float %2388, float* %2445, align 4
  %2446 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 6
  %2447 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 6
  %2448 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 6
  store float %2390, float* %2446, align 4
  store float %2391, float* %2447, align 4
  store float %2392, float* %2448, align 4
  %2449 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 7
  %2450 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 7
  %2451 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 7
  store float %2394, float* %2449, align 4
  store float %2395, float* %2450, align 4
  store float %2396, float* %2451, align 4
  %2452 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 8
  %2453 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 8
  %2454 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 8
  store float %2398, float* %2452, align 4
  store float %2399, float* %2453, align 4
  store float %2400, float* %2454, align 4
  %2455 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 0
  %2456 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 0
  %2457 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 0
  %2458 = load float, float* %2455, align 4
  %2459 = load float, float* %2456, align 4
  %2460 = load float, float* %2457, align 4
  %2461 = fadd fast float %2458, %2366
  %2462 = fadd fast float %2459, %2367
  %2463 = fadd fast float %2460, %2368
  store float %2461, float* %2455, align 4
  store float %2462, float* %2456, align 4
  store float %2463, float* %2457, align 4
  br label %2464

; <label>:2464                                    ; preds = %2464, %2364
  %2465 = phi i32 [ 1, %2364 ], [ %2481, %2464 ]
  %2466 = getelementptr [9 x float], [9 x float]* %25, i32 0, i32 %2465
  %2467 = load float, float* %2466, align 4
  %2468 = getelementptr [9 x float], [9 x float]* %26, i32 0, i32 %2465
  %2469 = load float, float* %2468, align 4
  %2470 = getelementptr [9 x float], [9 x float]* %27, i32 0, i32 %2465
  %2471 = load float, float* %2470, align 4
  %2472 = getelementptr [9 x float], [9 x float]* %22, i32 0, i32 %2465
  %2473 = getelementptr [9 x float], [9 x float]* %23, i32 0, i32 %2465
  %2474 = getelementptr [9 x float], [9 x float]* %24, i32 0, i32 %2465
  %2475 = load float, float* %2472, align 4
  %2476 = load float, float* %2473, align 4
  %2477 = load float, float* %2474, align 4
  %2478 = fadd fast float %2475, %2467
  %2479 = fadd fast float %2476, %2469
  %2480 = fadd fast float %2477, %2471
  store float %2478, float* %2472, align 4
  store float %2479, float* %2473, align 4
  store float %2480, float* %2474, align 4
  %2481 = add nuw nsw i32 %2465, 1
  %2482 = icmp eq i32 %2481, 9
  br i1 %2482, label %2483, label %2464

; <label>:2483                                    ; preds = %2464
  %2484 = load float, float* %2401, align 4
  %2485 = load float, float* %2402, align 4
  %2486 = load float, float* %2403, align 4
  %2487 = load float, float* %2404, align 4
  %2488 = load float, float* %2405, align 4
  %2489 = load float, float* %2406, align 4
  %2490 = load float, float* %2407, align 4
  %2491 = load float, float* %2408, align 4
  %2492 = load float, float* %2409, align 4
  %2493 = load float, float* %2410, align 4
  %2494 = load float, float* %2411, align 4
  %2495 = load float, float* %2412, align 4
  %2496 = load float, float* %2413, align 4
  %2497 = load float, float* %2414, align 4
  %2498 = load float, float* %2415, align 4
  %2499 = load float, float* %2416, align 4
  %2500 = load float, float* %2417, align 4
  %2501 = load float, float* %2418, align 4
  %2502 = load float, float* %2419, align 4
  %2503 = load float, float* %2420, align 4
  %2504 = load float, float* %2421, align 4
  %2505 = load float, float* %2422, align 4
  %2506 = load float, float* %2423, align 4
  %2507 = load float, float* %2424, align 4
  %2508 = load float, float* %2425, align 4
  %2509 = load float, float* %2426, align 4
  %2510 = load float, float* %2427, align 4
  %2511 = fadd fast float %2359, %2291
  %2512 = fadd fast float %2360, %2292
  %2513 = fadd fast float %2361, %2293
  %2514 = mul nsw i32 %2335, 9
  %2515 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2514, i32 undef)  ; BufferLoad(srv,index,wot)
  %2516 = extractvalue %dx.types.ResRet.f32 %2515, 0
  %2517 = add i32 %2514, 1
  %2518 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2517, i32 undef)  ; BufferLoad(srv,index,wot)
  %2519 = extractvalue %dx.types.ResRet.f32 %2518, 0
  %2520 = add i32 %2514, 2
  %2521 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2520, i32 undef)  ; BufferLoad(srv,index,wot)
  %2522 = extractvalue %dx.types.ResRet.f32 %2521, 0
  %2523 = add i32 %2514, 3
  %2524 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2523, i32 undef)  ; BufferLoad(srv,index,wot)
  %2525 = extractvalue %dx.types.ResRet.f32 %2524, 0
  %2526 = add i32 %2514, 4
  %2527 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2526, i32 undef)  ; BufferLoad(srv,index,wot)
  %2528 = extractvalue %dx.types.ResRet.f32 %2527, 0
  %2529 = add i32 %2514, 5
  %2530 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2529, i32 undef)  ; BufferLoad(srv,index,wot)
  %2531 = extractvalue %dx.types.ResRet.f32 %2530, 0
  %2532 = add i32 %2514, 6
  %2533 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2532, i32 undef)  ; BufferLoad(srv,index,wot)
  %2534 = extractvalue %dx.types.ResRet.f32 %2533, 0
  %2535 = add i32 %2514, 7
  %2536 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2535, i32 undef)  ; BufferLoad(srv,index,wot)
  %2537 = extractvalue %dx.types.ResRet.f32 %2536, 0
  %2538 = add i32 %2514, 8
  %2539 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2538, i32 undef)  ; BufferLoad(srv,index,wot)
  %2540 = extractvalue %dx.types.ResRet.f32 %2539, 0
  %2541 = fmul fast float %2516, 0x3FD20DD820000000
  %2542 = fmul fast float %2519, %189
  %2543 = fmul fast float %2522, %190
  %2544 = fmul fast float %188, 0x3FD4D8D900000000
  %2545 = fmul fast float %2544, %2525
  %2546 = fmul fast float %189, %188
  %2547 = fmul fast float %2546, %2528
  %2548 = fmul fast float %190, %189
  %2549 = fmul fast float %2548, %2531
  %2550 = fmul fast float %190, %190
  %2551 = fmul fast float %2550, 0x3FCE4712C0000000
  %2552 = fadd fast float %2551, 0xBFB42F61E0000000
  %2553 = fmul fast float %2552, %2534
  %2554 = fmul fast float %188, %190
  %2555 = fmul fast float %2554, %2537
  %2556 = fmul fast float %188, %188
  %2557 = fmul fast float %189, %189
  %2558 = fsub fast float %2556, %2557
  %2559 = fmul fast float %2558, 0x3FC17B13A0000000
  %2560 = fmul fast float %2559, %2540
  %2561 = fsub fast float %2547, %2549
  %2562 = fsub fast float %2561, %2555
  %2563 = fmul fast float %2562, 0x3FD17B13A0000000
  %2564 = fsub fast float %2543, %2542
  %2565 = fmul fast float %2564, 0x3FD4D8D900000000
  %2566 = fsub fast float %2541, %2545
  %2567 = fadd fast float %2566, %2553
  %2568 = fadd fast float %2567, %2560
  %2569 = fadd fast float %2568, %2563
  %2570 = fadd fast float %2569, %2565
  %2571 = call float @dx.op.unary.f32(i32 7, float %2570)  ; Saturate(value)
  %2572 = fadd fast float %2294, 1.000000e+00
  %2573 = fsub fast float %2572, %2571
  %2574 = add i32 %2295, 1
  br label %2575

; <label>:2575                                    ; preds = %2483, %2357, %2263
  %2576 = phi float [ %2508, %2483 ], [ %2264, %2357 ], [ %2264, %2263 ]
  %2577 = phi float [ %2505, %2483 ], [ %2265, %2357 ], [ %2265, %2263 ]
  %2578 = phi float [ %2502, %2483 ], [ %2266, %2357 ], [ %2266, %2263 ]
  %2579 = phi float [ %2499, %2483 ], [ %2267, %2357 ], [ %2267, %2263 ]
  %2580 = phi float [ %2496, %2483 ], [ %2268, %2357 ], [ %2268, %2263 ]
  %2581 = phi float [ %2493, %2483 ], [ %2269, %2357 ], [ %2269, %2263 ]
  %2582 = phi float [ %2490, %2483 ], [ %2270, %2357 ], [ %2270, %2263 ]
  %2583 = phi float [ %2487, %2483 ], [ %2271, %2357 ], [ %2271, %2263 ]
  %2584 = phi float [ %2484, %2483 ], [ %2272, %2357 ], [ %2272, %2263 ]
  %2585 = phi float [ %2509, %2483 ], [ %2273, %2357 ], [ %2273, %2263 ]
  %2586 = phi float [ %2506, %2483 ], [ %2274, %2357 ], [ %2274, %2263 ]
  %2587 = phi float [ %2503, %2483 ], [ %2275, %2357 ], [ %2275, %2263 ]
  %2588 = phi float [ %2500, %2483 ], [ %2276, %2357 ], [ %2276, %2263 ]
  %2589 = phi float [ %2497, %2483 ], [ %2277, %2357 ], [ %2277, %2263 ]
  %2590 = phi float [ %2494, %2483 ], [ %2278, %2357 ], [ %2278, %2263 ]
  %2591 = phi float [ %2491, %2483 ], [ %2279, %2357 ], [ %2279, %2263 ]
  %2592 = phi float [ %2488, %2483 ], [ %2280, %2357 ], [ %2280, %2263 ]
  %2593 = phi float [ %2485, %2483 ], [ %2281, %2357 ], [ %2281, %2263 ]
  %2594 = phi float [ %2510, %2483 ], [ %2282, %2357 ], [ %2282, %2263 ]
  %2595 = phi float [ %2507, %2483 ], [ %2283, %2357 ], [ %2283, %2263 ]
  %2596 = phi float [ %2504, %2483 ], [ %2284, %2357 ], [ %2284, %2263 ]
  %2597 = phi float [ %2501, %2483 ], [ %2285, %2357 ], [ %2285, %2263 ]
  %2598 = phi float [ %2498, %2483 ], [ %2286, %2357 ], [ %2286, %2263 ]
  %2599 = phi float [ %2495, %2483 ], [ %2287, %2357 ], [ %2287, %2263 ]
  %2600 = phi float [ %2492, %2483 ], [ %2288, %2357 ], [ %2288, %2263 ]
  %2601 = phi float [ %2489, %2483 ], [ %2289, %2357 ], [ %2289, %2263 ]
  %2602 = phi float [ %2486, %2483 ], [ %2290, %2357 ], [ %2290, %2263 ]
  %2603 = phi float [ %2511, %2483 ], [ %2291, %2357 ], [ %2291, %2263 ]
  %2604 = phi float [ %2512, %2483 ], [ %2292, %2357 ], [ %2292, %2263 ]
  %2605 = phi float [ %2513, %2483 ], [ %2293, %2357 ], [ %2293, %2263 ]
  %2606 = phi float [ %2573, %2483 ], [ %2294, %2357 ], [ %2294, %2263 ]
  %2607 = phi i32 [ %2574, %2483 ], [ %2295, %2357 ], [ %2295, %2263 ]
  %2608 = add nuw nsw i32 %2296, 1
  %2609 = icmp eq i32 %2608, 8
  br i1 %2609, label %2610, label %2263

; <label>:2610                                    ; preds = %2575
  %2611 = icmp eq i32 %2607, 0
  br i1 %2611, label %2785, label %2612

; <label>:2612                                    ; preds = %2610
  %2613 = uitofp i32 %2607 to float
  %2614 = fdiv fast float 1.000000e+00, %2613
  %2615 = fmul fast float %2584, %2614
  %2616 = fmul fast float %2593, %2614
  %2617 = fmul fast float %2602, %2614
  %2618 = fmul fast float %2583, %2614
  %2619 = fmul fast float %2592, %2614
  %2620 = fmul fast float %2601, %2614
  %2621 = fmul fast float %2582, %2614
  %2622 = fmul fast float %2591, %2614
  %2623 = fmul fast float %2600, %2614
  %2624 = fmul fast float %2581, %2614
  %2625 = fmul fast float %2590, %2614
  %2626 = fmul fast float %2599, %2614
  %2627 = fmul fast float %2580, %2614
  %2628 = fmul fast float %2589, %2614
  %2629 = fmul fast float %2598, %2614
  %2630 = fmul fast float %2579, %2614
  %2631 = fmul fast float %2588, %2614
  %2632 = fmul fast float %2597, %2614
  %2633 = fmul fast float %2578, %2614
  %2634 = fmul fast float %2587, %2614
  %2635 = fmul fast float %2596, %2614
  %2636 = fmul fast float %2577, %2614
  %2637 = fmul fast float %2586, %2614
  %2638 = fmul fast float %2595, %2614
  %2639 = fmul fast float %2576, %2614
  %2640 = fmul fast float %2585, %2614
  %2641 = fmul fast float %2594, %2614
  %2642 = fdiv fast float %2603, %2613
  %2643 = fdiv fast float %2604, %2613
  %2644 = fdiv fast float %2605, %2613
  %2645 = fdiv fast float %2606, %2613
  %2646 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %2647 = extractvalue %dx.types.CBufRet.f32 %2646, 0
  %2648 = fmul fast float %2647, %188
  %2649 = fmul fast float %2647, %189
  %2650 = fmul fast float %2647, %190
  %2651 = fsub fast float %195, %2642
  %2652 = fadd fast float %2651, %2648
  %2653 = fsub fast float %196, %2643
  %2654 = fadd fast float %2653, %2649
  %2655 = fsub fast float %197, %2644
  %2656 = fadd fast float %2655, %2650
  %2657 = call float @dx.op.dot3.f32(i32 55, float %2652, float %2654, float %2656, float %2652, float %2654, float %2656)  ; Dot3(ax,ay,az,bx,by,bz)
  %2658 = call float @dx.op.unary.f32(i32 25, float %2657)  ; Rsqrt(value)
  %2659 = fmul fast float %2652, %2658
  %2660 = fmul fast float %2654, %2658
  %2661 = fmul fast float %2656, %2658
  %2662 = fadd fast float %2659, %188
  %2663 = fadd fast float %2660, %189
  %2664 = fadd fast float %2661, %190
  %2665 = fmul fast float %2662, 5.000000e-01
  %2666 = fmul fast float %2663, 5.000000e-01
  %2667 = fmul fast float %2664, 5.000000e-01
  %2668 = fmul fast float %2615, 0x3FEC5BF980000000
  %2669 = fmul fast float %2616, 0x3FEC5BF980000000
  %2670 = fmul fast float %2617, 0x3FEC5BF980000000
  %2671 = fmul fast float %2633, 0x3FCFB4E740000000
  %2672 = fmul fast float %2634, 0x3FCFB4E740000000
  %2673 = fmul fast float %2635, 0x3FCFB4E740000000
  %2674 = fmul fast float %2639, 0x3FDB756EC0000000
  %2675 = fmul fast float %2640, 0x3FDB756EC0000000
  %2676 = fmul fast float %2641, 0x3FDB756EC0000000
  %2677 = fmul fast float %2666, %2618
  %2678 = fmul fast float %2666, %2619
  %2679 = fmul fast float %2666, %2620
  %2680 = fmul fast float %2667, %2621
  %2681 = fmul fast float %2667, %2622
  %2682 = fmul fast float %2667, %2623
  %2683 = fmul fast float %2665, %2624
  %2684 = fmul fast float %2665, %2625
  %2685 = fmul fast float %2665, %2626
  %2686 = fmul fast float %2665, %2666
  %2687 = fmul fast float %2686, %2627
  %2688 = fmul fast float %2686, %2628
  %2689 = fmul fast float %2686, %2629
  %2690 = fmul fast float %2666, %2667
  %2691 = fmul fast float %2690, %2630
  %2692 = fsub fast float -0.000000e+00, %2691
  %2693 = fmul fast float %2690, %2631
  %2694 = fsub fast float -0.000000e+00, %2693
  %2695 = fmul fast float %2690, %2632
  %2696 = fsub fast float -0.000000e+00, %2695
  %2697 = fmul fast float %2664, 1.500000e+00
  %2698 = fmul fast float %2697, %2667
  %2699 = fadd fast float %2698, -1.000000e+00
  %2700 = fmul fast float %2671, %2699
  %2701 = fmul fast float %2672, %2699
  %2702 = fmul fast float %2673, %2699
  %2703 = fmul fast float %2667, %2665
  %2704 = fmul fast float %2703, %2636
  %2705 = fmul fast float %2703, %2637
  %2706 = fmul fast float %2703, %2638
  %2707 = fmul fast float %2665, %2665
  %2708 = fmul fast float %2666, %2666
  %2709 = fsub fast float %2707, %2708
  %2710 = fmul fast float %2674, %2709
  %2711 = fmul fast float %2675, %2709
  %2712 = fmul fast float %2676, %2709
  %2713 = fsub fast float %2692, %2704
  %2714 = fadd fast float %2713, %2687
  %2715 = fmul fast float %2714, 0x3FEB756EC0000000
  %2716 = fsub fast float %2680, %2683
  %2717 = fsub fast float %2716, %2677
  %2718 = fmul fast float %2717, 0x3FF05F8CA0000000
  %2719 = fadd fast float %2700, %2668
  %2720 = fadd fast float %2719, %2710
  %2721 = fadd fast float %2720, %2715
  %2722 = fadd fast float %2721, %2718
  %2723 = fsub fast float %2694, %2705
  %2724 = fadd fast float %2723, %2688
  %2725 = fmul fast float %2724, 0x3FEB756EC0000000
  %2726 = fsub fast float %2681, %2684
  %2727 = fsub fast float %2726, %2678
  %2728 = fmul fast float %2727, 0x3FF05F8CA0000000
  %2729 = fadd fast float %2701, %2669
  %2730 = fadd fast float %2729, %2711
  %2731 = fadd fast float %2730, %2725
  %2732 = fadd fast float %2731, %2728
  %2733 = fsub fast float %2696, %2706
  %2734 = fadd fast float %2733, %2689
  %2735 = fmul fast float %2734, 0x3FEB756EC0000000
  %2736 = fsub fast float %2682, %2685
  %2737 = fsub fast float %2736, %2679
  %2738 = fmul fast float %2737, 0x3FF05F8CA0000000
  %2739 = fadd fast float %2702, %2670
  %2740 = fadd fast float %2739, %2712
  %2741 = fadd fast float %2740, %2735
  %2742 = fadd fast float %2741, %2738
  %2743 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2722)  ; FMax(a,b)
  %2744 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2732)  ; FMax(a,b)
  %2745 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2742)  ; FMax(a,b)
  %2746 = fsub fast float %2642, %195
  %2747 = fsub fast float %2643, %196
  %2748 = fsub fast float %2644, %197
  %2749 = fmul fast float %2746, %2746
  %2750 = fmul fast float %2747, %2747
  %2751 = fadd fast float %2750, %2749
  %2752 = fmul fast float %2748, %2748
  %2753 = fadd fast float %2751, %2752
  %2754 = call float @dx.op.unary.f32(i32 24, float %2753)  ; Sqrt(value)
  %2755 = call float @dx.op.dot3.f32(i32 55, float %2746, float %2747, float %2748, float %2746, float %2747, float %2748)  ; Dot3(ax,ay,az,bx,by,bz)
  %2756 = call float @dx.op.unary.f32(i32 25, float %2755)  ; Rsqrt(value)
  %2757 = fmul fast float %2756, %2746
  %2758 = fmul fast float %2756, %2747
  %2759 = fmul fast float %2756, %2748
  %2760 = call float @dx.op.dot3.f32(i32 55, float %2757, float %2758, float %2759, float %188, float %189, float %190)  ; Dot3(ax,ay,az,bx,by,bz)
  %2761 = fmul fast float %2760, %2754
  %2762 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2761)  ; FMax(a,b)
  %2763 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2764 = extractvalue %dx.types.CBufRet.f32 %2763, 3
  %2765 = fmul fast float %2764, 8.000000e+00
  %2766 = fadd fast float %2765, %2762
  %2767 = fmul fast float %2765, %2765
  %2768 = fmul fast float %2766, %2766
  %2769 = fdiv fast float %2767, %2768
  %2770 = call float @dx.op.unary.f32(i32 7, float %2769)  ; Saturate(value)
  %2771 = call float @dx.op.unary.f32(i32 7, float %2770)  ; Saturate(value)
  %2772 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2773 = extractvalue %dx.types.CBufRet.f32 %2772, 0
  %2774 = fadd fast float %2771, -1.000000e+00
  %2775 = fmul fast float %2773, %2774
  %2776 = fadd fast float %2775, 1.000000e+00
  %2777 = fmul fast float %2776, %2743
  %2778 = fmul fast float %2776, %2744
  %2779 = fmul fast float %2776, %2745
  %2780 = fsub fast float 1.000000e+00, %2645
  br label %2785

; <label>:2781                                    ; preds = %2246
  %2782 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2783 = extractvalue %dx.types.CBufRet.f32 %2782, 3
  %2784 = fsub fast float 1.000000e+00, %2783
  br label %2785

; <label>:2785                                    ; preds = %2781, %2612, %2610, %1980, %1978, %1350, %1339
  %2786 = phi float [ %1332, %1339 ], [ %1351, %1350 ], [ 0.000000e+00, %2781 ], [ %2145, %1980 ], [ 0.000000e+00, %1978 ], [ %2777, %2612 ], [ 0.000000e+00, %2610 ]
  %2787 = phi float [ %1333, %1339 ], [ %1352, %1350 ], [ 0.000000e+00, %2781 ], [ %2146, %1980 ], [ 0.000000e+00, %1978 ], [ %2778, %2612 ], [ 0.000000e+00, %2610 ]
  %2788 = phi float [ %1334, %1339 ], [ %1353, %1350 ], [ 0.000000e+00, %2781 ], [ %2147, %1980 ], [ 0.000000e+00, %1978 ], [ %2779, %2612 ], [ 0.000000e+00, %2610 ]
  %2789 = phi float [ %1340, %1339 ], [ %1355, %1350 ], [ %2784, %2781 ], [ %2148, %1980 ], [ 1.000000e+00, %1978 ], [ %2780, %2612 ], [ 1.000000e+00, %2610 ]
  %2790 = fsub fast float 1.000000e+00, %2789
  %2791 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2792 = extractvalue %dx.types.CBufRet.f32 %2791, 3
  %2793 = fmul fast float %2792, %2790
  %2794 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %2795 = extractvalue %dx.types.CBufRet.f32 %2794, 1
  %2796 = fmul fast float %2795, %2786
  %2797 = fmul fast float %2795, %2787
  %2798 = fmul fast float %2795, %2788
  %2799 = fmul fast float %2793, %2795
  br label %2800

; <label>:2800                                    ; preds = %2785, %283
  %2801 = phi float [ %2796, %2785 ], [ 0.000000e+00, %283 ]
  %2802 = phi float [ %2797, %2785 ], [ 0.000000e+00, %283 ]
  %2803 = phi float [ %2798, %2785 ], [ 0.000000e+00, %283 ]
  %2804 = phi float [ %2799, %2785 ], [ 0.000000e+00, %283 ]
  %2805 = xor i1 %285, true
  %2806 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %2807 = extractvalue %dx.types.CBufRet.f32 %2806, 1
  %2808 = fcmp fast ogt float %2807, 0.000000e+00
  %2809 = and i1 %2808, %2805
  br i1 %2809, label %2810, label %2817

; <label>:2810                                    ; preds = %2800
  %2811 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2812 = extractvalue %dx.types.CBufRet.f32 %2811, 3
  %2813 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2814 = extractvalue %dx.types.CBufRet.f32 %2813, 3
  %2815 = fmul fast float %2814, %2812
  %2816 = fmul fast float %2815, %2807
  br label %2817

; <label>:2817                                    ; preds = %2810, %2800
  %2818 = phi float [ %2816, %2810 ], [ %2804, %2800 ]
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %3, i32 %14, i32 %15, i32 0, float %2801, float %2802, float %2803, float %2818, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %2819

; <label>:2819                                    ; preds = %2817, %78, %73, %36, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #1

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
!dx.entryPoints = !{!26}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !15, !20, !22}
!5 = !{!6, !8, !9, !11, !12, !13}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 11, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<unsigned int, 4> >"* undef, !"", i32 0, i32 14, i32 1, i32 2, i32 0, !10}
!10 = !{i32 0, i32 5}
!11 = !{i32 3, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 7, i32 0, !7}
!12 = !{i32 4, %"class.Buffer<float>"* undef, !"", i32 0, i32 16, i32 1, i32 10, i32 0, !7}
!13 = !{i32 5, %"class.Buffer<int>"* undef, !"", i32 0, i32 17, i32 1, i32 10, i32 0, !14}
!14 = !{i32 0, i32 4}
!15 = !{!16, !17, !19}
!16 = !{i32 0, %"class.RWTexture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 7, i1 false, i1 false, i1 false, !7}
!17 = !{i32 1, %"class.RWStructuredBuffer<slSHCoeff>"* undef, !"", i32 0, i32 2, i32 1, i32 12, i1 false, i1 false, i1 false, !18}
!18 = !{i32 1, i32 108}
!19 = !{i32 2, %"class.RWBuffer<float>"* undef, !"", i32 0, i32 4, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!20 = !{!21}
!21 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 512, null}
!22 = !{!23, !24, !25}
!23 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 12, i32 1, i32 0, null}
!24 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 11, i32 1, i32 0, null}
!25 = !{i32 2, %struct.SamplerState* undef, !"", i32 0, i32 9, i32 1, i32 0, null}
!26 = !{void ()* @main, !"main", null, !4, !27}
!27 = !{i32 0, i64 16, i32 4, !28}
!28 = !{i32 8, i32 8, i32 1}

