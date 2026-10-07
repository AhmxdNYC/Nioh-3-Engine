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
; shader debug name: 89cf7520b494ac7af097b45f5d6449d4.pdb
; shader hash: 89cf7520b494ac7af097b45f5d6449d4
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
;       float fRoughLmt;                              ; Offset:    0
;       row_major float4x4 mV2W;                      ; Offset:   16
;       row_major float4x4 mP2W;                      ; Offset:   80
;       float4 gv_distance_pos;                       ; Offset:  144
;       float4 gv_distance_info;                      ; Offset:  160
;       float4 gv_info_out;                           ; Offset:  176
;       float4 gv_info_in;                            ; Offset:  192
;       float4 gv_probefield_org;                     ; Offset:  208
;       float4 gv_probefield_pitch_size;              ; Offset:  224
;       float4 gv_probefield_pitch_size_rv;           ; Offset:  240
;       float4 gv_tiledoct_pitch_octsize;             ; Offset:  256
;       float4 gv_tiledoct_pitch_octsize_rv;          ; Offset:  272
;       float4 gv_probe_scale_offset;                 ; Offset:  288
;       float4 gv_indirect_weight;                    ; Offset:  304
;       float4 gv_indirect_weight_2;                  ; Offset:  320
;       float4 gv_mip_info;                           ; Offset:  336
;       float4 gv_trace_bias;                         ; Offset:  352
;       float4 gv_trace_bias2;                        ; Offset:  368
;       row_major float4x4 gm_w2f;                    ; Offset:  384
;       float4 gv_viewport;                           ; Offset:  448
;       float4 gv_unified_info;                       ; Offset:  464
;       float4 gv_mask_param;                         ; Offset:  480
;       float4 vEye;                                  ; Offset:  496
;       int4 gv_lod_param;                            ; Offset:  512
;       float4 gv_time_param;                         ; Offset:  528
;   
;   } $Globals;                                       ; Offset:    0 Size:   544
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpsGBuf1                       sampler      NA          NA      S0            s13     1
; __smpsGBuf0                       sampler      NA          NA      S1            s12     1
; __smpsDepth                       sampler      NA          NA      S2            s11     1
; __smpsOctmapSource                sampler      NA          NA      S3             s9     1
; __smpsOctRadianceSource           sampler      NA          NA      S4            s10     1
; sGBuf1                            texture     f32          2d      T0            t13     1
; sGBuf0                            texture     f32          2d      T1            t12     1
; sDepth                            texture     f32          2d      T2            t11     1
; sOctRadianceSource                texture     f32     2darray      T3            t10     1
; sStencil                          texture     u32          2d      T4            t14     1
; sOctDepthSource                   texture     f32     2darray      T5             t0     1
; sOctNormalSource                  texture     f32     2darray      T6             t1     1
; sOctRadianceMask                  texture     f32     2darray      T7             t6     1
; oct_probe_info                    texture     f32         buf      T8            t16     1
; oct_ref_table                     texture     i32         buf      T9            t17     1
; sRLRResult                        texture     f32          2d     T10             t7     1
; uOutTex1                              UAV     f32     2darray      U0             u0     1
; sky_occ                               UAV     f32         buf      U1             u4     1
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
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2D<vector<unsigned int, 4> >" = type { <4 x i32>, %"class.Texture2D<vector<unsigned int, 4> >::mips_type" }
%"class.Texture2D<vector<unsigned int, 4> >::mips_type" = type { i32 }
%"class.Buffer<float>" = type { float }
%"class.Buffer<int>" = type { i32 }
%"class.RWTexture2DArray<vector<float, 4> >" = type { <4 x float> }
%"class.RWBuffer<float>" = type { float }
%"dx.alignment.legacy.$Globals" = type { float, [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, [4 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x i32>, <4 x float> }
%struct.SamplerState = type { i32 }

@sub_offset_list.i.0.hca = internal unnamed_addr constant [3 x i32] [i32 1, i32 0, i32 0]
@sub_offset_list.i.1.hca = internal unnamed_addr constant [3 x i32] [i32 0, i32 1, i32 0]
@sub_offset_list.i.2.hca = internal unnamed_addr constant [3 x i32] [i32 0, i32 0, i32 1]
@"\01?ldsTraceResult@@3PAV?$vector@M$03@@A.v.1dim" = addrspace(3) global [256 x float] undef

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 4, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 10, i32 7, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 9, i32 17, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 8, i32 16, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 6, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 14, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 4, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %17 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %18 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %19 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %20 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %21 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %22 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  %23 = alloca [3 x i32], align 4
  %24 = alloca [3 x i32], align 4
  %25 = alloca [3 x i32], align 4
  %26 = alloca [3 x i32], align 4
  %27 = alloca [3 x i32], align 4
  %28 = alloca [3 x i32], align 4
  %29 = alloca [8 x float], align 4
  %30 = alloca [8 x float], align 4
  %31 = alloca [8 x float], align 4
  %32 = alloca [8 x float], align 4
  %33 = alloca [8 x float], align 4
  %34 = alloca [8 x i32], align 4
  %35 = alloca [2 x i32], align 4
  %36 = alloca [8 x i32], align 4
  %37 = alloca [8 x i32], align 4
  %38 = alloca [8 x i32], align 4
  %39 = alloca [8 x i32], align 4
  %40 = alloca [8 x i32], align 4
  %41 = alloca [8 x i32], align 4
  %42 = alloca [8 x i32], align 4
  %43 = uitofp i32 %20 to float
  %44 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %45 = extractvalue %dx.types.CBufRet.f32 %44, 0
  %46 = fcmp fast oge float %43, %45
  %47 = uitofp i32 %21 to float
  %48 = extractvalue %dx.types.CBufRet.f32 %44, 1
  %49 = fcmp fast oge float %47, %48
  %50 = or i1 %46, %49
  br i1 %50, label %5543, label %51

; <label>:51                                      ; preds = %0
  %52 = mul i32 %22, 4
  %53 = add i32 0, %52
  %54 = getelementptr [256 x float], [256 x float] addrspace(3)* @"\01?ldsTraceResult@@3PAV?$vector@M$03@@A.v.1dim", i32 0, i32 %53
  store float 0.000000e+00, float addrspace(3)* %54, align 16
  %55 = mul i32 %22, 4
  %56 = add i32 1, %55
  %57 = getelementptr [256 x float], [256 x float] addrspace(3)* @"\01?ldsTraceResult@@3PAV?$vector@M$03@@A.v.1dim", i32 0, i32 %56
  store float 0.000000e+00, float addrspace(3)* %57, align 4
  %58 = mul i32 %22, 4
  %59 = add i32 2, %58
  %60 = getelementptr [256 x float], [256 x float] addrspace(3)* @"\01?ldsTraceResult@@3PAV?$vector@M$03@@A.v.1dim", i32 0, i32 %59
  store float 0.000000e+00, float addrspace(3)* %60, align 8
  %61 = mul i32 %22, 4
  %62 = add i32 3, %61
  %63 = getelementptr [256 x float], [256 x float] addrspace(3)* @"\01?ldsTraceResult@@3PAV?$vector@M$03@@A.v.1dim", i32 0, i32 %62
  store float 0.000000e+00, float addrspace(3)* %63, align 4
  %64 = fadd fast float %43, 5.000000e-01
  %65 = fadd fast float %47, 5.000000e-01
  %66 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %67 = extractvalue %dx.types.CBufRet.f32 %66, 2
  %68 = fmul fast float %67, %64
  %69 = fmul fast float %67, %65
  %70 = extractvalue %dx.types.CBufRet.f32 %66, 0
  %71 = extractvalue %dx.types.CBufRet.f32 %66, 1
  %72 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %73 = extractvalue %dx.types.CBufRet.f32 %72, 0
  %74 = extractvalue %dx.types.CBufRet.f32 %72, 1
  %75 = fmul fast float %73, %70
  %76 = fmul fast float %74, %71
  %77 = extractvalue %dx.types.CBufRet.f32 %72, 2
  %78 = extractvalue %dx.types.CBufRet.f32 %72, 3
  %79 = fmul fast float %77, %70
  %80 = fmul fast float %78, %71
  %81 = fsub fast float %68, %75
  %82 = fsub fast float %69, %76
  %83 = fdiv fast float %81, %79
  %84 = fdiv fast float %82, %80
  %85 = call float @dx.op.tertiary.f32(i32 46, float %83, float 2.000000e+00, float -1.000000e+00)  ; FMad(a,b,c)
  %86 = call float @dx.op.tertiary.f32(i32 46, float %84, float 2.000000e+00, float -1.000000e+00)  ; FMad(a,b,c)
  %87 = fsub fast float -0.000000e+00, %86
  %88 = fptosi float %68 to i32
  %89 = fptosi float %69 to i32
  %90 = sitofp i32 %88 to float
  %91 = sitofp i32 %89 to float
  %92 = fadd fast float %90, 5.000000e-01
  %93 = fadd fast float %91, 5.000000e-01
  %94 = fdiv fast float %92, %70
  %95 = fdiv fast float %93, %71
  %96 = fadd fast float %73, %77
  %97 = fcmp fast oge float %94, %73
  %98 = fcmp fast olt float %94, %96
  %99 = and i1 %97, %98
  br i1 %99, label %100, label %5543

; <label>:100                                     ; preds = %51
  %101 = fadd fast float %74, %78
  %102 = fcmp fast oge float %95, %74
  %103 = fcmp fast olt float %95, %101
  %104 = and i1 %102, %103
  br i1 %104, label %105, label %5543

; <label>:105                                     ; preds = %100
  %106 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %9, i32 0)  ; GetDimensions(handle,mipLevel)
  %107 = extractvalue %dx.types.Dimensions %106, 0
  %108 = sitofp i32 %107 to float
  %109 = extractvalue %dx.types.Dimensions %106, 1
  %110 = sitofp i32 %109 to float
  %111 = fmul fast float %108, %94
  %112 = fmul fast float %110, %95
  %113 = fptosi float %111 to i32
  %114 = fptosi float %112 to i32
  %115 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %9, i32 0, i32 %113, i32 %114, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %116 = extractvalue %dx.types.ResRet.i32 %115, 1
  %117 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %118 = extractvalue %dx.types.CBufRet.f32 %117, 0
  %119 = extractvalue %dx.types.CBufRet.f32 %117, 1
  %120 = fptoui float %118 to i32
  %121 = fptoui float %119 to i32
  %122 = and i32 %121, %116
  %123 = icmp eq i32 %120, %122
  br i1 %123, label %124, label %5543

; <label>:124                                     ; preds = %105
  %125 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %12, %dx.types.Handle %17, float %94, float %95, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %126 = extractvalue %dx.types.ResRet.f32 %125, 0
  %127 = extractvalue %dx.types.ResRet.f32 %125, 1
  %128 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %13, %dx.types.Handle %18, float %94, float %95, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %129 = extractvalue %dx.types.ResRet.f32 %128, 3
  %130 = fmul fast float %126, 2.000000e+00
  %131 = fmul fast float %127, 2.000000e+00
  %132 = fadd fast float %130, -1.000000e+00
  %133 = fadd fast float %131, -1.000000e+00
  %134 = call float @dx.op.unary.f32(i32 6, float %132)  ; FAbs(value)
  %135 = fsub fast float 1.000000e+00, %134
  %136 = call float @dx.op.unary.f32(i32 6, float %133)  ; FAbs(value)
  %137 = fsub fast float %135, %136
  %138 = fcmp fast oge float %137, 0.000000e+00
  %139 = fsub fast float 1.000000e+00, %136
  %140 = fcmp fast oge float %132, 0.000000e+00
  %141 = fcmp fast oge float %133, 0.000000e+00
  %142 = select i1 %140, float 1.000000e+00, float -1.000000e+00
  %143 = select i1 %141, float 1.000000e+00, float -1.000000e+00
  %144 = fmul fast float %139, %142
  %145 = fmul fast float %135, %143
  %146 = select i1 %138, float %132, float %144
  %147 = select i1 %138, float %133, float %145
  %148 = call float @dx.op.dot3.f32(i32 55, float %146, float %147, float %137, float %146, float %147, float %137)  ; Dot3(ax,ay,az,bx,by,bz)
  %149 = call float @dx.op.unary.f32(i32 25, float %148)  ; Rsqrt(value)
  %150 = fmul fast float %146, %149
  %151 = fmul fast float %147, %149
  %152 = fmul fast float %149, %137
  %153 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %154 = extractvalue %dx.types.CBufRet.f32 %153, 0
  %155 = extractvalue %dx.types.CBufRet.f32 %153, 1
  %156 = extractvalue %dx.types.CBufRet.f32 %153, 2
  %157 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %158 = extractvalue %dx.types.CBufRet.f32 %157, 0
  %159 = extractvalue %dx.types.CBufRet.f32 %157, 1
  %160 = extractvalue %dx.types.CBufRet.f32 %157, 2
  %161 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %162 = extractvalue %dx.types.CBufRet.f32 %161, 0
  %163 = extractvalue %dx.types.CBufRet.f32 %161, 1
  %164 = extractvalue %dx.types.CBufRet.f32 %161, 2
  %165 = fmul fast float %154, %150
  %166 = call float @dx.op.tertiary.f32(i32 46, float %151, float %158, float %165)  ; FMad(a,b,c)
  %167 = call float @dx.op.tertiary.f32(i32 46, float %152, float %162, float %166)  ; FMad(a,b,c)
  %168 = fmul fast float %155, %150
  %169 = call float @dx.op.tertiary.f32(i32 46, float %151, float %159, float %168)  ; FMad(a,b,c)
  %170 = call float @dx.op.tertiary.f32(i32 46, float %152, float %163, float %169)  ; FMad(a,b,c)
  %171 = fmul fast float %156, %150
  %172 = call float @dx.op.tertiary.f32(i32 46, float %151, float %160, float %171)  ; FMad(a,b,c)
  %173 = call float @dx.op.tertiary.f32(i32 46, float %152, float %164, float %172)  ; FMad(a,b,c)
  %174 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %16, float %94, float %95, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %175 = extractvalue %dx.types.ResRet.f32 %174, 0
  %176 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %177 = extractvalue %dx.types.CBufRet.f32 %176, 0
  %178 = extractvalue %dx.types.CBufRet.f32 %176, 1
  %179 = extractvalue %dx.types.CBufRet.f32 %176, 2
  %180 = extractvalue %dx.types.CBufRet.f32 %176, 3
  %181 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %182 = extractvalue %dx.types.CBufRet.f32 %181, 0
  %183 = extractvalue %dx.types.CBufRet.f32 %181, 1
  %184 = extractvalue %dx.types.CBufRet.f32 %181, 2
  %185 = extractvalue %dx.types.CBufRet.f32 %181, 3
  %186 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %187 = extractvalue %dx.types.CBufRet.f32 %186, 0
  %188 = extractvalue %dx.types.CBufRet.f32 %186, 1
  %189 = extractvalue %dx.types.CBufRet.f32 %186, 2
  %190 = extractvalue %dx.types.CBufRet.f32 %186, 3
  %191 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %192 = extractvalue %dx.types.CBufRet.f32 %191, 0
  %193 = extractvalue %dx.types.CBufRet.f32 %191, 1
  %194 = extractvalue %dx.types.CBufRet.f32 %191, 2
  %195 = extractvalue %dx.types.CBufRet.f32 %191, 3
  %196 = fmul fast float %177, %85
  %197 = call float @dx.op.tertiary.f32(i32 46, float %87, float %182, float %196)  ; FMad(a,b,c)
  %198 = call float @dx.op.tertiary.f32(i32 46, float %175, float %187, float %197)  ; FMad(a,b,c)
  %199 = fadd fast float %198, %192
  %200 = fmul fast float %178, %85
  %201 = call float @dx.op.tertiary.f32(i32 46, float %87, float %183, float %200)  ; FMad(a,b,c)
  %202 = call float @dx.op.tertiary.f32(i32 46, float %175, float %188, float %201)  ; FMad(a,b,c)
  %203 = fadd fast float %202, %193
  %204 = fmul fast float %179, %85
  %205 = call float @dx.op.tertiary.f32(i32 46, float %87, float %184, float %204)  ; FMad(a,b,c)
  %206 = call float @dx.op.tertiary.f32(i32 46, float %175, float %189, float %205)  ; FMad(a,b,c)
  %207 = fadd fast float %206, %194
  %208 = fmul fast float %180, %85
  %209 = call float @dx.op.tertiary.f32(i32 46, float %87, float %185, float %208)  ; FMad(a,b,c)
  %210 = call float @dx.op.tertiary.f32(i32 46, float %175, float %190, float %209)  ; FMad(a,b,c)
  %211 = fadd fast float %210, %195
  %212 = fdiv fast float %199, %211
  %213 = fdiv fast float %203, %211
  %214 = fdiv fast float %207, %211
  %215 = fmul fast float %129, 2.000000e+00
  %216 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %217 = extractvalue %dx.types.CBufRet.f32 %216, 0
  %218 = call float @dx.op.binary.f32(i32 36, float %215, float %217)  ; FMin(a,b)
  %219 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 31)  ; CBufferLoadLegacy(handle,regIndex)
  %220 = extractvalue %dx.types.CBufRet.f32 %219, 0
  %221 = extractvalue %dx.types.CBufRet.f32 %219, 1
  %222 = extractvalue %dx.types.CBufRet.f32 %219, 2
  %223 = fsub fast float %220, %212
  %224 = fsub fast float %221, %213
  %225 = fsub fast float %222, %214
  %226 = call float @dx.op.dot3.f32(i32 55, float %223, float %224, float %225, float %223, float %224, float %225)  ; Dot3(ax,ay,az,bx,by,bz)
  %227 = call float @dx.op.unary.f32(i32 25, float %226)  ; Rsqrt(value)
  %228 = fmul fast float %223, %227
  %229 = fmul fast float %224, %227
  %230 = fmul fast float %225, %227
  %231 = call float @dx.op.dot3.f32(i32 55, float %167, float %170, float %173, float %167, float %170, float %173)  ; Dot3(ax,ay,az,bx,by,bz)
  %232 = call float @dx.op.unary.f32(i32 25, float %231)  ; Rsqrt(value)
  %233 = fmul fast float %232, %167
  %234 = fmul fast float %232, %170
  %235 = fmul fast float %232, %173
  %236 = fmul fast float %218, %218
  %237 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %238 = extractvalue %dx.types.CBufRet.f32 %237, 0
  %239 = extractvalue %dx.types.CBufRet.f32 %237, 1
  %240 = extractvalue %dx.types.CBufRet.f32 %237, 2
  %241 = fsub fast float %212, %238
  %242 = fsub fast float %213, %239
  %243 = fsub fast float %214, %240
  %244 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %245 = extractvalue %dx.types.CBufRet.f32 %244, 0
  %246 = extractvalue %dx.types.CBufRet.f32 %244, 1
  %247 = extractvalue %dx.types.CBufRet.f32 %244, 2
  %248 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %249 = extractvalue %dx.types.CBufRet.f32 %248, 0
  %250 = extractvalue %dx.types.CBufRet.f32 %248, 1
  %251 = extractvalue %dx.types.CBufRet.f32 %248, 2
  %252 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %253 = extractvalue %dx.types.CBufRet.f32 %252, 0
  %254 = extractvalue %dx.types.CBufRet.f32 %252, 1
  %255 = extractvalue %dx.types.CBufRet.f32 %252, 2
  %256 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 27)  ; CBufferLoadLegacy(handle,regIndex)
  %257 = extractvalue %dx.types.CBufRet.f32 %256, 0
  %258 = extractvalue %dx.types.CBufRet.f32 %256, 1
  %259 = extractvalue %dx.types.CBufRet.f32 %256, 2
  %260 = fmul fast float %241, %245
  %261 = call float @dx.op.tertiary.f32(i32 46, float %242, float %249, float %260)  ; FMad(a,b,c)
  %262 = call float @dx.op.tertiary.f32(i32 46, float %243, float %253, float %261)  ; FMad(a,b,c)
  %263 = fadd fast float %262, %257
  %264 = fmul fast float %241, %246
  %265 = call float @dx.op.tertiary.f32(i32 46, float %242, float %250, float %264)  ; FMad(a,b,c)
  %266 = call float @dx.op.tertiary.f32(i32 46, float %243, float %254, float %265)  ; FMad(a,b,c)
  %267 = fadd fast float %266, %258
  %268 = fmul fast float %241, %247
  %269 = call float @dx.op.tertiary.f32(i32 46, float %242, float %251, float %268)  ; FMad(a,b,c)
  %270 = call float @dx.op.tertiary.f32(i32 46, float %243, float %255, float %269)  ; FMad(a,b,c)
  %271 = fadd fast float %270, %259
  %272 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %273 = extractvalue %dx.types.CBufRet.f32 %272, 3
  %274 = fmul fast float %273, 4.000000e+00
  %275 = extractvalue %dx.types.CBufRet.f32 %272, 0
  %276 = extractvalue %dx.types.CBufRet.f32 %272, 1
  %277 = extractvalue %dx.types.CBufRet.f32 %272, 2
  %278 = fmul fast float %275, %274
  %279 = fmul fast float %276, %274
  %280 = fmul fast float %277, %274
  %281 = fsub fast float %278, %274
  %282 = fsub fast float %279, %274
  %283 = fsub fast float %280, %274
  %284 = fmul fast float %281, 5.000000e-01
  %285 = fmul fast float %282, 5.000000e-01
  %286 = fmul fast float %283, 5.000000e-01
  %287 = call float @dx.op.unary.f32(i32 6, float %284)  ; FAbs(value)
  %288 = call float @dx.op.unary.f32(i32 6, float %285)  ; FAbs(value)
  %289 = call float @dx.op.unary.f32(i32 6, float %286)  ; FAbs(value)
  %290 = fadd fast float %284, %274
  %291 = fadd fast float %285, %274
  %292 = fadd fast float %286, %274
  %293 = call float @dx.op.unary.f32(i32 6, float %290)  ; FAbs(value)
  %294 = call float @dx.op.unary.f32(i32 6, float %291)  ; FAbs(value)
  %295 = call float @dx.op.unary.f32(i32 6, float %292)  ; FAbs(value)
  %296 = fsub fast float %263, %284
  %297 = fsub fast float %267, %285
  %298 = fsub fast float %271, %286
  %299 = call float @dx.op.unary.f32(i32 6, float %296)  ; FAbs(value)
  %300 = call float @dx.op.unary.f32(i32 6, float %297)  ; FAbs(value)
  %301 = call float @dx.op.unary.f32(i32 6, float %298)  ; FAbs(value)
  %302 = fcmp fast ogt float %299, %293
  %303 = fcmp fast ogt float %300, %294
  %304 = or i1 %302, %303
  %305 = fcmp fast ogt float %301, %295
  %306 = or i1 %304, %305
  br i1 %306, label %329, label %307

; <label>:307                                     ; preds = %124
  %308 = fcmp fast olt float %299, %287
  %309 = fcmp fast olt float %300, %288
  %310 = and i1 %308, %309
  %311 = fcmp fast olt float %301, %289
  %312 = and i1 %310, %311
  br i1 %312, label %329, label %313

; <label>:313                                     ; preds = %307
  %314 = fsub fast float %299, %287
  %315 = fsub fast float %300, %288
  %316 = fsub fast float %301, %289
  %317 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %314)  ; FMax(a,b)
  %318 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %315)  ; FMax(a,b)
  %319 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %316)  ; FMax(a,b)
  %320 = fmul fast float %317, %317
  %321 = fmul fast float %318, %318
  %322 = fadd fast float %321, %320
  %323 = fmul fast float %319, %319
  %324 = fadd fast float %322, %323
  %325 = call float @dx.op.unary.f32(i32 24, float %324)  ; Sqrt(value)
  %326 = fdiv fast float %325, %274
  %327 = call float @dx.op.unary.f32(i32 7, float %326)  ; Saturate(value)
  %328 = fsub fast float 1.000000e+00, %327
  br label %329

; <label>:329                                     ; preds = %313, %307, %124
  %330 = phi float [ %328, %313 ], [ -1.000000e+00, %124 ], [ 1.000000e+00, %307 ]
  %331 = fcmp fast ogt float %330, 0.000000e+00
  %332 = fcmp fast olt float %175, 1.000000e+00
  %333 = and i1 %332, %331
  %334 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %335 = extractvalue %dx.types.CBufRet.f32 %334, 1
  %336 = fcmp fast ogt float %335, 0.000000e+00
  %337 = and i1 %333, %336
  br i1 %337, label %338, label %5521

; <label>:338                                     ; preds = %329
  %339 = fsub fast float %212, %220
  %340 = fsub fast float %213, %221
  %341 = fsub fast float %214, %222
  %342 = call float @dx.op.dot3.f32(i32 55, float %339, float %340, float %341, float %339, float %340, float %341)  ; Dot3(ax,ay,az,bx,by,bz)
  %343 = call float @dx.op.unary.f32(i32 25, float %342)  ; Rsqrt(value)
  %344 = fmul fast float %339, %343
  %345 = fmul fast float %340, %343
  %346 = fmul fast float %341, %343
  %347 = call float @dx.op.dot3.f32(i32 55, float %344, float %345, float %346, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %348 = fmul fast float %347, 2.000000e+00
  %349 = fmul fast float %348, %233
  %350 = fmul fast float %348, %234
  %351 = fmul fast float %348, %235
  %352 = fsub fast float %344, %349
  %353 = fsub fast float %345, %350
  %354 = fsub fast float %346, %351
  %355 = fcmp fast ogt float %347, 0.000000e+00
  br i1 %355, label %356, label %357

; <label>:356                                     ; preds = %338
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2, i32 %20, i32 %21, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %5543

; <label>:357                                     ; preds = %338
  %358 = call float @dx.op.dot3.f32(i32 55, float %233, float %234, float %235, float %228, float %229, float %230)  ; Dot3(ax,ay,az,bx,by,bz)
  %359 = call float @dx.op.unary.f32(i32 7, float %358)  ; Saturate(value)
  %360 = fsub fast float %228, %233
  %361 = fsub fast float %229, %234
  %362 = fsub fast float %230, %235
  %363 = fmul fast float %359, %360
  %364 = fmul fast float %359, %361
  %365 = fmul fast float %359, %362
  %366 = fadd fast float %363, %233
  %367 = fadd fast float %364, %234
  %368 = fadd fast float %365, %235
  %369 = fmul fast float %366, 1.000000e+01
  %370 = fmul fast float %367, 1.000000e+01
  %371 = fmul fast float %368, 1.000000e+01
  %372 = fadd fast float %369, %241
  %373 = fadd fast float %370, %242
  %374 = fadd fast float %371, %243
  %375 = call float @dx.op.unary.f32(i32 7, float %236)  ; Saturate(value)
  %376 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %3, i32 0)  ; GetDimensions(handle,mipLevel)
  %377 = extractvalue %dx.types.Dimensions %376, 0
  %378 = extractvalue %dx.types.Dimensions %376, 1
  %379 = uitofp i32 %377 to float
  %380 = uitofp i32 %378 to float
  %381 = fmul fast float %379, %94
  %382 = fmul fast float %380, %95
  %383 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %384 = extractvalue %dx.types.CBufRet.f32 %383, 1
  %385 = fmul fast float %381, %384
  %386 = fmul fast float %382, %384
  %387 = fptosi float %385 to i32
  %388 = fptosi float %386 to i32
  %389 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %3, i32 0, i32 %387, i32 %388, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %390 = extractvalue %dx.types.ResRet.f32 %389, 3
  %391 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %392 = extractvalue %dx.types.CBufRet.f32 %391, 2
  %393 = fsub fast float %375, %392
  %394 = fmul fast float %393, 1.000000e+01
  %395 = call float @dx.op.unary.f32(i32 7, float %394)  ; Saturate(value)
  %396 = extractvalue %dx.types.CBufRet.f32 %391, 3
  %397 = fmul fast float %395, %390
  %398 = fmul fast float %397, %396
  %399 = fmul fast float %245, %372
  %400 = call float @dx.op.tertiary.f32(i32 46, float %373, float %249, float %399)  ; FMad(a,b,c)
  %401 = call float @dx.op.tertiary.f32(i32 46, float %374, float %253, float %400)  ; FMad(a,b,c)
  %402 = fadd fast float %401, %257
  %403 = fmul fast float %246, %372
  %404 = call float @dx.op.tertiary.f32(i32 46, float %373, float %250, float %403)  ; FMad(a,b,c)
  %405 = call float @dx.op.tertiary.f32(i32 46, float %374, float %254, float %404)  ; FMad(a,b,c)
  %406 = fadd fast float %405, %258
  %407 = fmul fast float %247, %372
  %408 = call float @dx.op.tertiary.f32(i32 46, float %373, float %251, float %407)  ; FMad(a,b,c)
  %409 = call float @dx.op.tertiary.f32(i32 46, float %374, float %255, float %408)  ; FMad(a,b,c)
  %410 = fadd fast float %409, %259
  %411 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %412 = extractvalue %dx.types.CBufRet.f32 %411, 3
  %413 = fmul fast float %412, %402
  %414 = fmul fast float %412, %406
  %415 = fmul fast float %412, %410
  %416 = call float @dx.op.unary.f32(i32 27, float %413)  ; Round_ni(value)
  %417 = call float @dx.op.unary.f32(i32 27, float %414)  ; Round_ni(value)
  %418 = call float @dx.op.unary.f32(i32 27, float %415)  ; Round_ni(value)
  %419 = getelementptr inbounds [2 x i32], [2 x i32]* %35, i32 0, i32 1
  store i32 0, i32* %419, align 4, !tbaa !34, !noalias !38
  %420 = getelementptr inbounds [2 x i32], [2 x i32]* %35, i32 0, i32 0
  store i32 0, i32* %420, align 4, !tbaa !34, !noalias !38
  br label %421

; <label>:421                                     ; preds = %1610, %357
  %422 = phi i32 [ -1, %357 ], [ %1611, %1610 ]
  %423 = phi i32 [ 0, %357 ], [ %1612, %1610 ]
  %424 = phi i32 [ 0, %357 ], [ %1613, %1610 ]
  %425 = phi i32 [ 0, %357 ], [ %1614, %1610 ]
  %426 = phi i32 [ 0, %357 ], [ %1615, %1610 ]
  %427 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %19, i32 32)  ; CBufferLoadLegacy(handle,regIndex)
  %428 = extractvalue %dx.types.CBufRet.i32 %427, 0
  %429 = shl i32 1, %426
  %430 = and i32 %428, %429
  %431 = icmp eq i32 %430, 0
  br i1 %431, label %1610, label %432

; <label>:432                                     ; preds = %421
  %433 = icmp sgt i32 %426, 1
  br i1 %433, label %434, label %458

; <label>:434                                     ; preds = %432
  %435 = load i32, i32* %420, align 4, !tbaa !34, !noalias !38
  %436 = load i32, i32* %419, align 4, !tbaa !34, !noalias !38
  %437 = add nsw i32 %436, %435
  %438 = icmp sgt i32 %437, 0
  br i1 %438, label %439, label %458

; <label>:439                                     ; preds = %434
  %440 = icmp eq i32 %435, 0
  %441 = zext i1 %440 to i32
  %442 = uitofp i1 %440 to float
  %443 = call float @dx.op.unary.f32(i32 21, float %442)  ; Exp(value)
  %444 = fptosi float %443 to i32
  %445 = sitofp i32 %444 to float
  %446 = fdiv fast float %416, %445
  %447 = fdiv fast float %417, %445
  %448 = fdiv fast float %418, %445
  %449 = call float @dx.op.unary.f32(i32 27, float %446)  ; Round_ni(value)
  %450 = call float @dx.op.unary.f32(i32 27, float %447)  ; Round_ni(value)
  %451 = call float @dx.op.unary.f32(i32 27, float %448)  ; Round_ni(value)
  %452 = fptosi float %449 to i32
  %453 = fptosi float %450 to i32
  %454 = fptosi float %451 to i32
  %455 = mul i32 %452, %444
  %456 = mul i32 %453, %444
  %457 = mul i32 %454, %444
  br label %1642

; <label>:458                                     ; preds = %434, %432
  %459 = sitofp i32 %426 to float
  %460 = call float @dx.op.unary.f32(i32 21, float %459)  ; Exp(value)
  %461 = fptosi float %460 to i32
  %462 = sitofp i32 %461 to float
  %463 = fdiv fast float %416, %462
  %464 = fdiv fast float %417, %462
  %465 = fdiv fast float %418, %462
  %466 = call float @dx.op.unary.f32(i32 27, float %463)  ; Round_ni(value)
  %467 = call float @dx.op.unary.f32(i32 27, float %464)  ; Round_ni(value)
  %468 = call float @dx.op.unary.f32(i32 27, float %465)  ; Round_ni(value)
  %469 = fptosi float %466 to i32
  %470 = fptosi float %467 to i32
  %471 = fptosi float %468 to i32
  %472 = mul i32 %469, %461
  %473 = mul i32 %470, %461
  %474 = mul i32 %471, %461
  br label %475

; <label>:475                                     ; preds = %646, %458
  %476 = phi i32 [ 0, %458 ], [ %647, %646 ]
  %477 = phi i32 [ 0, %458 ], [ %648, %646 ]
  %478 = phi i32 [ 0, %458 ], [ %649, %646 ]
  %479 = and i32 %478, 1
  %480 = lshr i32 %478, 1
  %481 = and i32 %480, 1
  %482 = lshr i32 %478, 2
  %483 = and i32 %482, 1
  %484 = sub nsw i32 0, %479
  %485 = and i32 %461, %484
  %486 = sub nsw i32 0, %481
  %487 = and i32 %461, %486
  %488 = sub nsw i32 0, %483
  %489 = and i32 %461, %488
  %490 = add i32 %485, %472
  %491 = add i32 %487, %473
  %492 = add i32 %489, %474
  %493 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %494 = extractvalue %dx.types.CBufRet.f32 %493, 0
  %495 = extractvalue %dx.types.CBufRet.f32 %493, 1
  %496 = extractvalue %dx.types.CBufRet.f32 %493, 2
  %497 = fptosi float %494 to i32
  %498 = fptosi float %495 to i32
  %499 = fptosi float %496 to i32
  %500 = add i32 %497, -1
  %501 = add i32 %498, -1
  %502 = add i32 %499, -1
  %503 = or i32 %491, %490
  %504 = or i32 %503, %492
  %505 = icmp slt i32 %504, 0
  %506 = icmp sgt i32 %490, %500
  %507 = or i1 %506, %505
  %508 = icmp sgt i32 %491, %501
  %509 = or i1 %508, %507
  %510 = icmp sgt i32 %492, %502
  %511 = or i1 %510, %509
  br i1 %511, label %646, label %512

; <label>:512                                     ; preds = %475
  %513 = sitofp i32 %490 to float
  %514 = sitofp i32 %491 to float
  %515 = fmul fast float %494, %514
  %516 = fadd fast float %515, %513
  %517 = sitofp i32 %492 to float
  %518 = fmul fast float %494, %517
  %519 = fmul fast float %518, %495
  %520 = fadd fast float %516, %519
  %521 = fptosi float %520 to i32
  %522 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %521, i32 undef)  ; BufferLoad(srv,index,wot)
  %523 = extractvalue %dx.types.ResRet.i32 %522, 0
  %524 = icmp sgt i32 %523, -1
  br i1 %524, label %525, label %540

; <label>:525                                     ; preds = %512
  %526 = mul i32 %523, 10
  %527 = add i32 %526, 3
  %528 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %527, i32 undef)  ; BufferLoad(srv,index,wot)
  %529 = extractvalue %dx.types.ResRet.f32 %528, 0
  %530 = bitcast float %529 to i32
  %531 = and i32 %530, 65535
  %532 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %526, i32 undef)  ; BufferLoad(srv,index,wot)
  %533 = extractvalue %dx.types.ResRet.f32 %532, 0
  %534 = or i32 %526, 1
  %535 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %534, i32 undef)  ; BufferLoad(srv,index,wot)
  %536 = extractvalue %dx.types.ResRet.f32 %535, 0
  %537 = add i32 %526, 2
  %538 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %537, i32 undef)  ; BufferLoad(srv,index,wot)
  %539 = extractvalue %dx.types.ResRet.f32 %538, 0
  br label %545

; <label>:540                                     ; preds = %512
  %541 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %542 = extractvalue %dx.types.CBufRet.f32 %541, 0
  %543 = extractvalue %dx.types.CBufRet.f32 %541, 1
  %544 = extractvalue %dx.types.CBufRet.f32 %541, 2
  br label %545

; <label>:545                                     ; preds = %540, %525
  %546 = phi i32 [ %531, %525 ], [ 0, %540 ]
  %547 = phi float [ %533, %525 ], [ %542, %540 ]
  %548 = phi float [ %536, %525 ], [ %543, %540 ]
  %549 = phi float [ %539, %525 ], [ %544, %540 ]
  %550 = and i32 %546, %429
  %551 = icmp eq i32 %550, 0
  %552 = icmp slt i32 %523, 0
  %553 = or i1 %552, %551
  br i1 %553, label %646, label %554

; <label>:554                                     ; preds = %545
  %555 = mul i32 %523, 10
  %556 = add i32 %555, 5
  %557 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %556, i32 undef)  ; BufferLoad(srv,index,wot)
  %558 = extractvalue %dx.types.ResRet.f32 %557, 0
  %559 = fcmp fast ogt float %558, 0.000000e+00
  br i1 %559, label %560, label %646

; <label>:560                                     ; preds = %554
  %561 = add nsw i32 %476, 1
  %562 = add i32 %555, 6
  %563 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %562, i32 undef)  ; BufferLoad(srv,index,wot)
  %564 = extractvalue %dx.types.ResRet.f32 %563, 0
  %565 = fptoui float %564 to i32
  %566 = add i32 %555, 7
  %567 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %566, i32 undef)  ; BufferLoad(srv,index,wot)
  %568 = extractvalue %dx.types.ResRet.f32 %567, 0
  %569 = fptoui float %568 to i32
  %570 = add i32 %555, 8
  %571 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %570, i32 undef)  ; BufferLoad(srv,index,wot)
  %572 = extractvalue %dx.types.ResRet.f32 %571, 0
  %573 = fptoui float %572 to i32
  %574 = add i32 %555, 9
  %575 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %574, i32 undef)  ; BufferLoad(srv,index,wot)
  %576 = extractvalue %dx.types.ResRet.f32 %575, 0
  %577 = fptoui float %576 to i32
  %578 = mul i32 %577, %565
  %579 = mul i32 %577, %569
  %580 = fsub fast float %372, %547
  %581 = fsub fast float %373, %548
  %582 = fsub fast float %374, %549
  %583 = call float @dx.op.dot3.f32(i32 55, float %580, float %581, float %582, float %580, float %581, float %582)  ; Dot3(ax,ay,az,bx,by,bz)
  %584 = call float @dx.op.unary.f32(i32 25, float %583)  ; Rsqrt(value)
  %585 = fmul fast float %584, %580
  %586 = fmul fast float %584, %581
  %587 = fmul fast float %584, %582
  %588 = call float @dx.op.dot3.f32(i32 55, float %585, float %586, float %587, float %580, float %581, float %582)  ; Dot3(ax,ay,az,bx,by,bz)
  %589 = call float @dx.op.unary.f32(i32 6, float %585)  ; FAbs(value)
  %590 = call float @dx.op.unary.f32(i32 6, float %586)  ; FAbs(value)
  %591 = fadd fast float %590, %589
  %592 = call float @dx.op.unary.f32(i32 6, float %587)  ; FAbs(value)
  %593 = fadd fast float %591, %592
  %594 = fdiv fast float 1.000000e+00, %593
  %595 = fmul fast float %594, %585
  %596 = fmul fast float %594, %586
  %597 = fcmp fast ole float %587, 0.000000e+00
  %598 = call float @dx.op.unary.f32(i32 6, float %596)  ; FAbs(value)
  %599 = call float @dx.op.unary.f32(i32 6, float %595)  ; FAbs(value)
  %600 = fsub fast float 1.000000e+00, %598
  %601 = fsub fast float 1.000000e+00, %599
  %602 = fcmp fast oge float %595, 0.000000e+00
  %603 = select i1 %602, float 1.000000e+00, float -1.000000e+00
  %604 = fcmp fast oge float %596, 0.000000e+00
  %605 = select i1 %604, float 1.000000e+00, float -1.000000e+00
  %606 = fmul fast float %603, %600
  %607 = fmul fast float %601, %605
  %608 = select i1 %597, float %606, float %595
  %609 = select i1 %597, float %607, float %596
  %610 = fmul fast float %608, 5.000000e-01
  %611 = fmul fast float %609, 5.000000e-01
  %612 = fadd fast float %610, 5.000000e-01
  %613 = fadd fast float %611, 5.000000e-01
  %614 = sitofp i32 %578 to float
  %615 = sitofp i32 %579 to float
  %616 = sitofp i32 %573 to float
  %617 = sitofp i32 %577 to float
  %618 = fadd fast float %617, -2.000000e+00
  %619 = fmul fast float %612, %618
  %620 = fmul fast float %613, %618
  %621 = fadd fast float %614, 1.000000e+00
  %622 = fadd fast float %621, %619
  %623 = fadd fast float %615, 1.000000e+00
  %624 = fadd fast float %623, %620
  %625 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %626 = extractvalue %dx.types.CBufRet.f32 %625, 0
  %627 = extractvalue %dx.types.CBufRet.f32 %625, 1
  %628 = extractvalue %dx.types.CBufRet.f32 %625, 3
  %629 = fmul fast float %628, %626
  %630 = fmul fast float %628, %627
  %631 = fmul fast float %629, %622
  %632 = fmul fast float %630, %624
  %633 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %634 = extractvalue %dx.types.CBufRet.f32 %633, 0
  %635 = fadd fast float %634, %616
  %636 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %631, float %632, float %635, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %637 = extractvalue %dx.types.ResRet.f32 %636, 2
  %638 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %639 = extractvalue %dx.types.CBufRet.f32 %638, 1
  %640 = fsub fast float %637, %639
  %641 = extractvalue %dx.types.CBufRet.f32 %638, 0
  %642 = fmul fast float %640, %641
  %643 = fcmp fast olt float %588, %642
  br i1 %643, label %644, label %646

; <label>:644                                     ; preds = %560
  %645 = add nsw i32 %477, 1
  br label %646

; <label>:646                                     ; preds = %644, %560, %554, %545, %475
  %647 = phi i32 [ %561, %644 ], [ %561, %560 ], [ %476, %545 ], [ %476, %475 ], [ %476, %554 ]
  %648 = phi i32 [ %645, %644 ], [ %477, %560 ], [ %477, %545 ], [ %477, %475 ], [ %477, %554 ]
  %649 = add nuw nsw i32 %478, 1
  %650 = icmp eq i32 %649, 8
  br i1 %650, label %651, label %475

; <label>:651                                     ; preds = %646
  %652 = icmp slt i32 %426, 2
  br i1 %652, label %653, label %655

; <label>:653                                     ; preds = %651
  %654 = getelementptr inbounds [2 x i32], [2 x i32]* %35, i32 0, i32 %426
  store i32 %647, i32* %654, align 4, !tbaa !34, !noalias !38
  br label %655

; <label>:655                                     ; preds = %653, %651
  %656 = icmp sgt i32 %647, 3
  %657 = and i1 %652, %656
  %658 = icmp sgt i32 %648, 0
  %659 = and i1 %657, %658
  br i1 %659, label %1641, label %660

; <label>:660                                     ; preds = %655
  %661 = icmp eq i32 %648, 0
  br i1 %661, label %662, label %1594

; <label>:662                                     ; preds = %660
  br i1 %652, label %663, label %895

; <label>:663                                     ; preds = %662
  %664 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %665 = extractvalue %dx.types.CBufRet.f32 %664, 3
  %666 = fmul fast float %665, %402
  %667 = fmul fast float %665, %406
  %668 = fmul fast float %665, %410
  %669 = sitofp i32 %472 to float
  %670 = sitofp i32 %473 to float
  %671 = sitofp i32 %474 to float
  %672 = fsub fast float %666, %669
  %673 = fsub fast float %667, %670
  %674 = fsub fast float %668, %671
  %675 = fdiv fast float %672, %462
  %676 = fdiv fast float %673, %462
  %677 = fdiv fast float %674, %462
  %678 = fadd fast float %675, -5.000000e-01
  %679 = fadd fast float %676, -5.000000e-01
  %680 = fadd fast float %677, -5.000000e-01
  %681 = call float @dx.op.unary.f32(i32 28, float %678)  ; Round_pi(value)
  %682 = call float @dx.op.unary.f32(i32 28, float %679)  ; Round_pi(value)
  %683 = call float @dx.op.unary.f32(i32 28, float %680)  ; Round_pi(value)
  %684 = fptosi float %681 to i32
  %685 = fptosi float %682 to i32
  %686 = fptosi float %683 to i32
  %687 = fcmp fast oge float %678, 0.000000e+00
  %688 = fcmp fast oge float %679, 0.000000e+00
  %689 = fcmp fast oge float %680, 0.000000e+00
  %690 = select i1 %687, i32 1, i32 -1
  %691 = select i1 %688, i32 1, i32 -1
  %692 = select i1 %689, i32 1, i32 -1
  %693 = getelementptr [3 x i32], [3 x i32]* %26, i32 0, i32 0
  %694 = getelementptr [3 x i32], [3 x i32]* %27, i32 0, i32 0
  %695 = getelementptr [3 x i32], [3 x i32]* %28, i32 0, i32 0
  store i32 %690, i32* %693, align 4
  store i32 0, i32* %694, align 4
  store i32 0, i32* %695, align 4
  %696 = getelementptr [3 x i32], [3 x i32]* %26, i32 0, i32 1
  %697 = getelementptr [3 x i32], [3 x i32]* %27, i32 0, i32 1
  %698 = getelementptr [3 x i32], [3 x i32]* %28, i32 0, i32 1
  store i32 0, i32* %696, align 4
  store i32 %691, i32* %697, align 4
  store i32 0, i32* %698, align 4
  %699 = getelementptr [3 x i32], [3 x i32]* %26, i32 0, i32 2
  %700 = getelementptr [3 x i32], [3 x i32]* %27, i32 0, i32 2
  %701 = getelementptr [3 x i32], [3 x i32]* %28, i32 0, i32 2
  store i32 0, i32* %699, align 4
  store i32 0, i32* %700, align 4
  store i32 %692, i32* %701, align 4
  br label %702

; <label>:702                                     ; preds = %888, %663
  %703 = phi i32 [ 0, %663 ], [ %894, %888 ]
  %704 = phi i32 [ 0, %663 ], [ %892, %888 ]
  %705 = phi i32 [ %690, %663 ], [ %890, %888 ]
  %706 = phi i32 [ %647, %663 ], [ %881, %888 ]
  %707 = phi i32 [ 0, %663 ], [ %882, %888 ]
  %708 = phi i32 [ 0, %663 ], [ %883, %888 ]
  %709 = phi i32 [ 0, %663 ], [ %884, %888 ]
  %710 = phi i32 [ 0, %663 ], [ %885, %888 ]
  %711 = phi i32 [ 0, %663 ], [ %886, %888 ]
  %712 = add i32 %705, %684
  %713 = add i32 %704, %685
  %714 = add i32 %703, %686
  %715 = mul i32 %712, %461
  %716 = mul i32 %713, %461
  %717 = mul i32 %714, %461
  %718 = add i32 %715, %472
  %719 = add i32 %716, %473
  %720 = add i32 %717, %474
  %721 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %722 = extractvalue %dx.types.CBufRet.f32 %721, 0
  %723 = extractvalue %dx.types.CBufRet.f32 %721, 1
  %724 = extractvalue %dx.types.CBufRet.f32 %721, 2
  %725 = fptosi float %722 to i32
  %726 = fptosi float %723 to i32
  %727 = fptosi float %724 to i32
  %728 = add i32 %725, -1
  %729 = add i32 %726, -1
  %730 = add i32 %727, -1
  %731 = or i32 %719, %718
  %732 = or i32 %731, %720
  %733 = icmp slt i32 %732, 0
  %734 = icmp sgt i32 %718, %728
  %735 = or i1 %733, %734
  %736 = icmp sgt i32 %719, %729
  %737 = or i1 %736, %735
  %738 = icmp sgt i32 %720, %730
  %739 = or i1 %738, %737
  br i1 %739, label %880, label %740

; <label>:740                                     ; preds = %702
  %741 = sitofp i32 %718 to float
  %742 = sitofp i32 %719 to float
  %743 = fmul fast float %722, %742
  %744 = fadd fast float %743, %741
  %745 = sitofp i32 %720 to float
  %746 = fmul fast float %722, %745
  %747 = fmul fast float %746, %723
  %748 = fadd fast float %744, %747
  %749 = fptosi float %748 to i32
  %750 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %749, i32 undef)  ; BufferLoad(srv,index,wot)
  %751 = extractvalue %dx.types.ResRet.i32 %750, 0
  %752 = icmp sgt i32 %751, -1
  br i1 %752, label %753, label %768

; <label>:753                                     ; preds = %740
  %754 = mul i32 %751, 10
  %755 = add i32 %754, 3
  %756 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %755, i32 undef)  ; BufferLoad(srv,index,wot)
  %757 = extractvalue %dx.types.ResRet.f32 %756, 0
  %758 = bitcast float %757 to i32
  %759 = and i32 %758, 65535
  %760 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %754, i32 undef)  ; BufferLoad(srv,index,wot)
  %761 = extractvalue %dx.types.ResRet.f32 %760, 0
  %762 = or i32 %754, 1
  %763 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %762, i32 undef)  ; BufferLoad(srv,index,wot)
  %764 = extractvalue %dx.types.ResRet.f32 %763, 0
  %765 = add i32 %754, 2
  %766 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %765, i32 undef)  ; BufferLoad(srv,index,wot)
  %767 = extractvalue %dx.types.ResRet.f32 %766, 0
  br label %773

; <label>:768                                     ; preds = %740
  %769 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %770 = extractvalue %dx.types.CBufRet.f32 %769, 0
  %771 = extractvalue %dx.types.CBufRet.f32 %769, 1
  %772 = extractvalue %dx.types.CBufRet.f32 %769, 2
  br label %773

; <label>:773                                     ; preds = %768, %753
  %774 = phi i32 [ %759, %753 ], [ 0, %768 ]
  %775 = phi float [ %761, %753 ], [ %770, %768 ]
  %776 = phi float [ %764, %753 ], [ %771, %768 ]
  %777 = phi float [ %767, %753 ], [ %772, %768 ]
  %778 = and i32 %774, %429
  %779 = icmp eq i32 %778, 0
  br i1 %779, label %880, label %780

; <label>:780                                     ; preds = %773
  %781 = mul i32 %751, 10
  %782 = add i32 %781, 6
  %783 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %782, i32 undef)  ; BufferLoad(srv,index,wot)
  %784 = extractvalue %dx.types.ResRet.f32 %783, 0
  %785 = fptoui float %784 to i32
  %786 = add i32 %781, 7
  %787 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %786, i32 undef)  ; BufferLoad(srv,index,wot)
  %788 = extractvalue %dx.types.ResRet.f32 %787, 0
  %789 = fptoui float %788 to i32
  %790 = add i32 %781, 8
  %791 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %790, i32 undef)  ; BufferLoad(srv,index,wot)
  %792 = extractvalue %dx.types.ResRet.f32 %791, 0
  %793 = fptoui float %792 to i32
  %794 = add i32 %781, 9
  %795 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %794, i32 undef)  ; BufferLoad(srv,index,wot)
  %796 = extractvalue %dx.types.ResRet.f32 %795, 0
  %797 = fptoui float %796 to i32
  %798 = mul i32 %797, %785
  %799 = mul i32 %797, %789
  %800 = fsub fast float %372, %775
  %801 = fsub fast float %373, %776
  %802 = fsub fast float %374, %777
  %803 = call float @dx.op.dot3.f32(i32 55, float %800, float %801, float %802, float %800, float %801, float %802)  ; Dot3(ax,ay,az,bx,by,bz)
  %804 = call float @dx.op.unary.f32(i32 25, float %803)  ; Rsqrt(value)
  %805 = fmul fast float %804, %800
  %806 = fmul fast float %804, %801
  %807 = fmul fast float %804, %802
  %808 = call float @dx.op.dot3.f32(i32 55, float %805, float %806, float %807, float %800, float %801, float %802)  ; Dot3(ax,ay,az,bx,by,bz)
  %809 = call float @dx.op.unary.f32(i32 6, float %805)  ; FAbs(value)
  %810 = call float @dx.op.unary.f32(i32 6, float %806)  ; FAbs(value)
  %811 = fadd fast float %810, %809
  %812 = call float @dx.op.unary.f32(i32 6, float %807)  ; FAbs(value)
  %813 = fadd fast float %811, %812
  %814 = fdiv fast float 1.000000e+00, %813
  %815 = fmul fast float %814, %805
  %816 = fmul fast float %814, %806
  %817 = fcmp fast ole float %807, 0.000000e+00
  %818 = call float @dx.op.unary.f32(i32 6, float %816)  ; FAbs(value)
  %819 = call float @dx.op.unary.f32(i32 6, float %815)  ; FAbs(value)
  %820 = fsub fast float 1.000000e+00, %818
  %821 = fsub fast float 1.000000e+00, %819
  %822 = fcmp fast oge float %815, 0.000000e+00
  %823 = select i1 %822, float 1.000000e+00, float -1.000000e+00
  %824 = fcmp fast oge float %816, 0.000000e+00
  %825 = select i1 %824, float 1.000000e+00, float -1.000000e+00
  %826 = fmul fast float %823, %820
  %827 = fmul fast float %821, %825
  %828 = select i1 %817, float %826, float %815
  %829 = select i1 %817, float %827, float %816
  %830 = fmul fast float %828, 5.000000e-01
  %831 = fmul fast float %829, 5.000000e-01
  %832 = fadd fast float %830, 5.000000e-01
  %833 = fadd fast float %831, 5.000000e-01
  %834 = sitofp i32 %798 to float
  %835 = sitofp i32 %799 to float
  %836 = sitofp i32 %793 to float
  %837 = sitofp i32 %797 to float
  %838 = fadd fast float %837, -2.000000e+00
  %839 = fmul fast float %832, %838
  %840 = fmul fast float %833, %838
  %841 = fadd fast float %834, 1.000000e+00
  %842 = fadd fast float %841, %839
  %843 = fadd fast float %835, 1.000000e+00
  %844 = fadd fast float %843, %840
  %845 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %846 = extractvalue %dx.types.CBufRet.f32 %845, 0
  %847 = extractvalue %dx.types.CBufRet.f32 %845, 1
  %848 = extractvalue %dx.types.CBufRet.f32 %845, 3
  %849 = fmul fast float %848, %846
  %850 = fmul fast float %848, %847
  %851 = fmul fast float %849, %842
  %852 = fmul fast float %850, %844
  %853 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %854 = extractvalue %dx.types.CBufRet.f32 %853, 0
  %855 = fadd fast float %854, %836
  %856 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %851, float %852, float %855, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %857 = extractvalue %dx.types.ResRet.f32 %856, 2
  %858 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %859 = extractvalue %dx.types.CBufRet.f32 %858, 1
  %860 = fsub fast float %857, %859
  %861 = extractvalue %dx.types.CBufRet.f32 %858, 0
  %862 = fmul fast float %860, %861
  %863 = fcmp fast olt float %808, %862
  %864 = icmp slt i32 %751, 0
  br i1 %864, label %871, label %865

; <label>:865                                     ; preds = %780
  %866 = add i32 %781, 5
  %867 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %866, i32 undef)  ; BufferLoad(srv,index,wot)
  %868 = extractvalue %dx.types.ResRet.f32 %867, 0
  %869 = fcmp fast ogt float %868, 0.000000e+00
  br i1 %869, label %871, label %870

; <label>:870                                     ; preds = %865
  br label %871

; <label>:871                                     ; preds = %870, %865, %780
  %872 = phi i1 [ false, %870 ], [ false, %780 ], [ true, %865 ]
  %873 = and i1 %863, %872
  br i1 %873, label %874, label %880

; <label>:874                                     ; preds = %871
  %875 = add nsw i32 %706, 1
  %876 = add nsw i32 %707, 1
  %877 = add i32 %705, %708
  %878 = add i32 %704, %709
  %879 = add i32 %703, %710
  br label %880

; <label>:880                                     ; preds = %874, %871, %773, %702
  %881 = phi i32 [ %875, %874 ], [ %706, %871 ], [ %706, %773 ], [ %706, %702 ]
  %882 = phi i32 [ %876, %874 ], [ %707, %871 ], [ %707, %773 ], [ %707, %702 ]
  %883 = phi i32 [ %877, %874 ], [ %708, %871 ], [ %708, %773 ], [ %708, %702 ]
  %884 = phi i32 [ %878, %874 ], [ %709, %871 ], [ %709, %773 ], [ %709, %702 ]
  %885 = phi i32 [ %879, %874 ], [ %710, %871 ], [ %710, %773 ], [ %710, %702 ]
  %886 = add nuw nsw i32 %711, 1
  %887 = icmp eq i32 %886, 3
  br i1 %887, label %1588, label %888

; <label>:888                                     ; preds = %880
  %889 = getelementptr [3 x i32], [3 x i32]* %26, i32 0, i32 %886
  %890 = load i32, i32* %889, align 4
  %891 = getelementptr [3 x i32], [3 x i32]* %27, i32 0, i32 %886
  %892 = load i32, i32* %891, align 4
  %893 = getelementptr [3 x i32], [3 x i32]* %28, i32 0, i32 %886
  %894 = load i32, i32* %893, align 4
  br label %702

; <label>:895                                     ; preds = %662
  %896 = fcmp fast oge float %233, 0.000000e+00
  %897 = fcmp fast oge float %234, 0.000000e+00
  %898 = fcmp fast oge float %235, 0.000000e+00
  %899 = select i1 %896, i32 1, i32 -1
  %900 = select i1 %897, i32 1, i32 -1
  %901 = select i1 %898, i32 1, i32 -1
  %902 = getelementptr [3 x i32], [3 x i32]* %23, i32 0, i32 0
  %903 = getelementptr [3 x i32], [3 x i32]* %24, i32 0, i32 0
  %904 = getelementptr [3 x i32], [3 x i32]* %25, i32 0, i32 0
  store i32 %899, i32* %902, align 4
  store i32 0, i32* %903, align 4
  store i32 0, i32* %904, align 4
  %905 = getelementptr [3 x i32], [3 x i32]* %23, i32 0, i32 1
  %906 = getelementptr [3 x i32], [3 x i32]* %24, i32 0, i32 1
  %907 = getelementptr [3 x i32], [3 x i32]* %25, i32 0, i32 1
  store i32 0, i32* %905, align 4
  store i32 %900, i32* %906, align 4
  store i32 0, i32* %907, align 4
  %908 = getelementptr [3 x i32], [3 x i32]* %23, i32 0, i32 2
  %909 = getelementptr [3 x i32], [3 x i32]* %24, i32 0, i32 2
  %910 = getelementptr [3 x i32], [3 x i32]* %25, i32 0, i32 2
  store i32 0, i32* %908, align 4
  store i32 0, i32* %909, align 4
  store i32 %901, i32* %910, align 4
  br label %911

; <label>:911                                     ; preds = %1581, %895
  %912 = phi i32 [ 0, %895 ], [ %1587, %1581 ]
  %913 = phi i32 [ 0, %895 ], [ %1585, %1581 ]
  %914 = phi i32 [ %899, %895 ], [ %1583, %1581 ]
  %915 = phi i32 [ 0, %895 ], [ %1574, %1581 ]
  %916 = phi i32 [ 0, %895 ], [ %929, %1581 ]
  %917 = sitofp i32 %914 to float
  %918 = sitofp i32 %913 to float
  %919 = sitofp i32 %912 to float
  %920 = fmul fast float %917, 1.500000e+00
  %921 = fmul fast float %918, 1.500000e+00
  %922 = fmul fast float %919, 1.500000e+00
  %923 = call float @dx.op.unary.f32(i32 28, float %920)  ; Round_pi(value)
  %924 = call float @dx.op.unary.f32(i32 28, float %921)  ; Round_pi(value)
  %925 = call float @dx.op.unary.f32(i32 28, float %922)  ; Round_pi(value)
  %926 = fptosi float %923 to i32
  %927 = fptosi float %924 to i32
  %928 = fptosi float %925 to i32
  %929 = add nuw nsw i32 %916, 1
  %930 = icmp eq i32 %929, 3
  %931 = select i1 %930, i32 0, i32 %929
  %932 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.0.hca, i32 0, i32 %931
  %933 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.1.hca, i32 0, i32 %931
  %934 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.2.hca, i32 0, i32 %931
  %935 = load i32, i32* %932, align 4
  %936 = load i32, i32* %933, align 4
  %937 = load i32, i32* %934, align 4
  %938 = add nuw nsw i32 %916, 2
  %939 = srem i32 %938, 3
  %940 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.0.hca, i32 0, i32 %939
  %941 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.1.hca, i32 0, i32 %939
  %942 = getelementptr [3 x i32], [3 x i32]* @sub_offset_list.i.2.hca, i32 0, i32 %939
  %943 = load i32, i32* %940, align 4
  %944 = load i32, i32* %941, align 4
  %945 = load i32, i32* %942, align 4
  %946 = mul i32 %926, %461
  %947 = mul i32 %927, %461
  %948 = mul i32 %928, %461
  %949 = add i32 %946, %472
  %950 = add i32 %947, %473
  %951 = add i32 %948, %474
  %952 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %953 = extractvalue %dx.types.CBufRet.f32 %952, 0
  %954 = extractvalue %dx.types.CBufRet.f32 %952, 1
  %955 = extractvalue %dx.types.CBufRet.f32 %952, 2
  %956 = fptosi float %953 to i32
  %957 = fptosi float %954 to i32
  %958 = fptosi float %955 to i32
  %959 = add i32 %956, -1
  %960 = add i32 %957, -1
  %961 = add i32 %958, -1
  %962 = or i32 %950, %949
  %963 = or i32 %962, %951
  %964 = icmp slt i32 %963, 0
  %965 = icmp sgt i32 %949, %959
  %966 = or i1 %964, %965
  %967 = icmp sgt i32 %950, %960
  %968 = or i1 %967, %966
  %969 = icmp sgt i32 %951, %961
  %970 = or i1 %969, %968
  br i1 %970, label %1105, label %971

; <label>:971                                     ; preds = %911
  %972 = sitofp i32 %949 to float
  %973 = sitofp i32 %950 to float
  %974 = fmul fast float %953, %973
  %975 = fadd fast float %974, %972
  %976 = sitofp i32 %951 to float
  %977 = fmul fast float %953, %976
  %978 = fmul fast float %977, %954
  %979 = fadd fast float %975, %978
  %980 = fptosi float %979 to i32
  %981 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %980, i32 undef)  ; BufferLoad(srv,index,wot)
  %982 = extractvalue %dx.types.ResRet.i32 %981, 0
  %983 = icmp sgt i32 %982, -1
  br i1 %983, label %989, label %984

; <label>:984                                     ; preds = %971
  %985 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %986 = extractvalue %dx.types.CBufRet.f32 %985, 0
  %987 = extractvalue %dx.types.CBufRet.f32 %985, 1
  %988 = extractvalue %dx.types.CBufRet.f32 %985, 2
  br label %1004

; <label>:989                                     ; preds = %971
  %990 = mul i32 %982, 10
  %991 = add i32 %990, 3
  %992 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %991, i32 undef)  ; BufferLoad(srv,index,wot)
  %993 = extractvalue %dx.types.ResRet.f32 %992, 0
  %994 = bitcast float %993 to i32
  %995 = and i32 %994, 65535
  %996 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %990, i32 undef)  ; BufferLoad(srv,index,wot)
  %997 = extractvalue %dx.types.ResRet.f32 %996, 0
  %998 = or i32 %990, 1
  %999 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %998, i32 undef)  ; BufferLoad(srv,index,wot)
  %1000 = extractvalue %dx.types.ResRet.f32 %999, 0
  %1001 = add i32 %990, 2
  %1002 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1001, i32 undef)  ; BufferLoad(srv,index,wot)
  %1003 = extractvalue %dx.types.ResRet.f32 %1002, 0
  br label %1004

; <label>:1004                                    ; preds = %989, %984
  %1005 = phi i32 [ %995, %989 ], [ 0, %984 ]
  %1006 = phi float [ %997, %989 ], [ %986, %984 ]
  %1007 = phi float [ %1000, %989 ], [ %987, %984 ]
  %1008 = phi float [ %1003, %989 ], [ %988, %984 ]
  %1009 = and i32 %1005, %429
  %1010 = icmp eq i32 %1009, 0
  br i1 %1010, label %1105, label %1011

; <label>:1011                                    ; preds = %1004
  %1012 = mul i32 %982, 10
  %1013 = add i32 %1012, 6
  %1014 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1013, i32 undef)  ; BufferLoad(srv,index,wot)
  %1015 = extractvalue %dx.types.ResRet.f32 %1014, 0
  %1016 = fptoui float %1015 to i32
  %1017 = add i32 %1012, 7
  %1018 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1017, i32 undef)  ; BufferLoad(srv,index,wot)
  %1019 = extractvalue %dx.types.ResRet.f32 %1018, 0
  %1020 = fptoui float %1019 to i32
  %1021 = add i32 %1012, 8
  %1022 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1021, i32 undef)  ; BufferLoad(srv,index,wot)
  %1023 = extractvalue %dx.types.ResRet.f32 %1022, 0
  %1024 = fptoui float %1023 to i32
  %1025 = add i32 %1012, 9
  %1026 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1025, i32 undef)  ; BufferLoad(srv,index,wot)
  %1027 = extractvalue %dx.types.ResRet.f32 %1026, 0
  %1028 = fptoui float %1027 to i32
  %1029 = mul i32 %1028, %1016
  %1030 = mul i32 %1028, %1020
  %1031 = fsub fast float %372, %1006
  %1032 = fsub fast float %373, %1007
  %1033 = fsub fast float %374, %1008
  %1034 = call float @dx.op.dot3.f32(i32 55, float %1031, float %1032, float %1033, float %1031, float %1032, float %1033)  ; Dot3(ax,ay,az,bx,by,bz)
  %1035 = call float @dx.op.unary.f32(i32 25, float %1034)  ; Rsqrt(value)
  %1036 = fmul fast float %1035, %1031
  %1037 = fmul fast float %1035, %1032
  %1038 = fmul fast float %1035, %1033
  %1039 = call float @dx.op.dot3.f32(i32 55, float %1036, float %1037, float %1038, float %1031, float %1032, float %1033)  ; Dot3(ax,ay,az,bx,by,bz)
  %1040 = call float @dx.op.unary.f32(i32 6, float %1036)  ; FAbs(value)
  %1041 = call float @dx.op.unary.f32(i32 6, float %1037)  ; FAbs(value)
  %1042 = fadd fast float %1041, %1040
  %1043 = call float @dx.op.unary.f32(i32 6, float %1038)  ; FAbs(value)
  %1044 = fadd fast float %1042, %1043
  %1045 = fdiv fast float 1.000000e+00, %1044
  %1046 = fmul fast float %1045, %1036
  %1047 = fmul fast float %1045, %1037
  %1048 = fcmp fast ole float %1038, 0.000000e+00
  %1049 = call float @dx.op.unary.f32(i32 6, float %1047)  ; FAbs(value)
  %1050 = call float @dx.op.unary.f32(i32 6, float %1046)  ; FAbs(value)
  %1051 = fsub fast float 1.000000e+00, %1049
  %1052 = fsub fast float 1.000000e+00, %1050
  %1053 = fcmp fast oge float %1046, 0.000000e+00
  %1054 = select i1 %1053, float 1.000000e+00, float -1.000000e+00
  %1055 = fcmp fast oge float %1047, 0.000000e+00
  %1056 = select i1 %1055, float 1.000000e+00, float -1.000000e+00
  %1057 = fmul fast float %1054, %1051
  %1058 = fmul fast float %1052, %1056
  %1059 = select i1 %1048, float %1057, float %1046
  %1060 = select i1 %1048, float %1058, float %1047
  %1061 = fmul fast float %1059, 5.000000e-01
  %1062 = fmul fast float %1060, 5.000000e-01
  %1063 = fadd fast float %1061, 5.000000e-01
  %1064 = fadd fast float %1062, 5.000000e-01
  %1065 = sitofp i32 %1029 to float
  %1066 = sitofp i32 %1030 to float
  %1067 = sitofp i32 %1024 to float
  %1068 = sitofp i32 %1028 to float
  %1069 = fadd fast float %1068, -2.000000e+00
  %1070 = fmul fast float %1063, %1069
  %1071 = fmul fast float %1064, %1069
  %1072 = fadd fast float %1065, 1.000000e+00
  %1073 = fadd fast float %1072, %1070
  %1074 = fadd fast float %1066, 1.000000e+00
  %1075 = fadd fast float %1074, %1071
  %1076 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1077 = extractvalue %dx.types.CBufRet.f32 %1076, 0
  %1078 = extractvalue %dx.types.CBufRet.f32 %1076, 1
  %1079 = extractvalue %dx.types.CBufRet.f32 %1076, 3
  %1080 = fmul fast float %1079, %1077
  %1081 = fmul fast float %1079, %1078
  %1082 = fmul fast float %1080, %1073
  %1083 = fmul fast float %1081, %1075
  %1084 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1085 = extractvalue %dx.types.CBufRet.f32 %1084, 0
  %1086 = fadd fast float %1085, %1067
  %1087 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1082, float %1083, float %1086, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1088 = extractvalue %dx.types.ResRet.f32 %1087, 2
  %1089 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1090 = extractvalue %dx.types.CBufRet.f32 %1089, 1
  %1091 = fsub fast float %1088, %1090
  %1092 = extractvalue %dx.types.CBufRet.f32 %1089, 0
  %1093 = fmul fast float %1091, %1092
  %1094 = fcmp fast olt float %1039, %1093
  %1095 = icmp slt i32 %982, 0
  br i1 %1095, label %1102, label %1096

; <label>:1096                                    ; preds = %1011
  %1097 = add i32 %1012, 5
  %1098 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1097, i32 undef)  ; BufferLoad(srv,index,wot)
  %1099 = extractvalue %dx.types.ResRet.f32 %1098, 0
  %1100 = fcmp fast ogt float %1099, 0.000000e+00
  br i1 %1100, label %1102, label %1101

; <label>:1101                                    ; preds = %1096
  br label %1102

; <label>:1102                                    ; preds = %1101, %1096, %1011
  %1103 = phi i1 [ false, %1101 ], [ false, %1011 ], [ true, %1096 ]
  %1104 = and i1 %1094, %1103
  br i1 %1104, label %1570, label %1105

; <label>:1105                                    ; preds = %1102, %1004, %911
  %1106 = phi i32 [ 1, %1102 ], [ 0, %1004 ], [ 0, %911 ]
  %1107 = add i32 %935, %926
  %1108 = add i32 %936, %927
  %1109 = add i32 %937, %928
  %1110 = mul i32 %1107, %461
  %1111 = mul i32 %1108, %461
  %1112 = mul i32 %1109, %461
  %1113 = add i32 %1110, %472
  %1114 = add i32 %1111, %473
  %1115 = add i32 %1112, %474
  %1116 = or i32 %1114, %1113
  %1117 = or i32 %1116, %1115
  %1118 = icmp slt i32 %1117, 0
  %1119 = icmp sgt i32 %1113, %959
  %1120 = or i1 %1118, %1119
  %1121 = icmp sgt i32 %1114, %960
  %1122 = or i1 %1121, %1120
  %1123 = icmp sgt i32 %1115, %961
  %1124 = or i1 %1123, %1122
  br i1 %1124, label %1260, label %1125

; <label>:1125                                    ; preds = %1105
  %1126 = sitofp i32 %1113 to float
  %1127 = sitofp i32 %1114 to float
  %1128 = fmul fast float %953, %1127
  %1129 = fadd fast float %1128, %1126
  %1130 = sitofp i32 %1115 to float
  %1131 = fmul fast float %953, %1130
  %1132 = fmul fast float %1131, %954
  %1133 = fadd fast float %1129, %1132
  %1134 = fptosi float %1133 to i32
  %1135 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1134, i32 undef)  ; BufferLoad(srv,index,wot)
  %1136 = extractvalue %dx.types.ResRet.i32 %1135, 0
  %1137 = icmp sgt i32 %1136, -1
  br i1 %1137, label %1143, label %1138

; <label>:1138                                    ; preds = %1125
  %1139 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1140 = extractvalue %dx.types.CBufRet.f32 %1139, 0
  %1141 = extractvalue %dx.types.CBufRet.f32 %1139, 1
  %1142 = extractvalue %dx.types.CBufRet.f32 %1139, 2
  br label %1158

; <label>:1143                                    ; preds = %1125
  %1144 = mul i32 %1136, 10
  %1145 = add i32 %1144, 3
  %1146 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1145, i32 undef)  ; BufferLoad(srv,index,wot)
  %1147 = extractvalue %dx.types.ResRet.f32 %1146, 0
  %1148 = bitcast float %1147 to i32
  %1149 = and i32 %1148, 65535
  %1150 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1144, i32 undef)  ; BufferLoad(srv,index,wot)
  %1151 = extractvalue %dx.types.ResRet.f32 %1150, 0
  %1152 = or i32 %1144, 1
  %1153 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1152, i32 undef)  ; BufferLoad(srv,index,wot)
  %1154 = extractvalue %dx.types.ResRet.f32 %1153, 0
  %1155 = add i32 %1144, 2
  %1156 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1155, i32 undef)  ; BufferLoad(srv,index,wot)
  %1157 = extractvalue %dx.types.ResRet.f32 %1156, 0
  br label %1158

; <label>:1158                                    ; preds = %1143, %1138
  %1159 = phi i32 [ %1149, %1143 ], [ 0, %1138 ]
  %1160 = phi float [ %1151, %1143 ], [ %1140, %1138 ]
  %1161 = phi float [ %1154, %1143 ], [ %1141, %1138 ]
  %1162 = phi float [ %1157, %1143 ], [ %1142, %1138 ]
  %1163 = and i32 %1159, %429
  %1164 = icmp eq i32 %1163, 0
  br i1 %1164, label %1260, label %1165

; <label>:1165                                    ; preds = %1158
  %1166 = add nuw nsw i32 %1106, 1
  %1167 = mul i32 %1136, 10
  %1168 = add i32 %1167, 6
  %1169 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1168, i32 undef)  ; BufferLoad(srv,index,wot)
  %1170 = extractvalue %dx.types.ResRet.f32 %1169, 0
  %1171 = fptoui float %1170 to i32
  %1172 = add i32 %1167, 7
  %1173 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1172, i32 undef)  ; BufferLoad(srv,index,wot)
  %1174 = extractvalue %dx.types.ResRet.f32 %1173, 0
  %1175 = fptoui float %1174 to i32
  %1176 = add i32 %1167, 8
  %1177 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1176, i32 undef)  ; BufferLoad(srv,index,wot)
  %1178 = extractvalue %dx.types.ResRet.f32 %1177, 0
  %1179 = fptoui float %1178 to i32
  %1180 = add i32 %1167, 9
  %1181 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1180, i32 undef)  ; BufferLoad(srv,index,wot)
  %1182 = extractvalue %dx.types.ResRet.f32 %1181, 0
  %1183 = fptoui float %1182 to i32
  %1184 = mul i32 %1183, %1171
  %1185 = mul i32 %1183, %1175
  %1186 = fsub fast float %372, %1160
  %1187 = fsub fast float %373, %1161
  %1188 = fsub fast float %374, %1162
  %1189 = call float @dx.op.dot3.f32(i32 55, float %1186, float %1187, float %1188, float %1186, float %1187, float %1188)  ; Dot3(ax,ay,az,bx,by,bz)
  %1190 = call float @dx.op.unary.f32(i32 25, float %1189)  ; Rsqrt(value)
  %1191 = fmul fast float %1190, %1186
  %1192 = fmul fast float %1190, %1187
  %1193 = fmul fast float %1190, %1188
  %1194 = call float @dx.op.dot3.f32(i32 55, float %1191, float %1192, float %1193, float %1186, float %1187, float %1188)  ; Dot3(ax,ay,az,bx,by,bz)
  %1195 = call float @dx.op.unary.f32(i32 6, float %1191)  ; FAbs(value)
  %1196 = call float @dx.op.unary.f32(i32 6, float %1192)  ; FAbs(value)
  %1197 = fadd fast float %1196, %1195
  %1198 = call float @dx.op.unary.f32(i32 6, float %1193)  ; FAbs(value)
  %1199 = fadd fast float %1197, %1198
  %1200 = fdiv fast float 1.000000e+00, %1199
  %1201 = fmul fast float %1200, %1191
  %1202 = fmul fast float %1200, %1192
  %1203 = fcmp fast ole float %1193, 0.000000e+00
  %1204 = call float @dx.op.unary.f32(i32 6, float %1202)  ; FAbs(value)
  %1205 = call float @dx.op.unary.f32(i32 6, float %1201)  ; FAbs(value)
  %1206 = fsub fast float 1.000000e+00, %1204
  %1207 = fsub fast float 1.000000e+00, %1205
  %1208 = fcmp fast oge float %1201, 0.000000e+00
  %1209 = select i1 %1208, float 1.000000e+00, float -1.000000e+00
  %1210 = fcmp fast oge float %1202, 0.000000e+00
  %1211 = select i1 %1210, float 1.000000e+00, float -1.000000e+00
  %1212 = fmul fast float %1209, %1206
  %1213 = fmul fast float %1207, %1211
  %1214 = select i1 %1203, float %1212, float %1201
  %1215 = select i1 %1203, float %1213, float %1202
  %1216 = fmul fast float %1214, 5.000000e-01
  %1217 = fmul fast float %1215, 5.000000e-01
  %1218 = fadd fast float %1216, 5.000000e-01
  %1219 = fadd fast float %1217, 5.000000e-01
  %1220 = sitofp i32 %1184 to float
  %1221 = sitofp i32 %1185 to float
  %1222 = sitofp i32 %1179 to float
  %1223 = sitofp i32 %1183 to float
  %1224 = fadd fast float %1223, -2.000000e+00
  %1225 = fmul fast float %1218, %1224
  %1226 = fmul fast float %1219, %1224
  %1227 = fadd fast float %1220, 1.000000e+00
  %1228 = fadd fast float %1227, %1225
  %1229 = fadd fast float %1221, 1.000000e+00
  %1230 = fadd fast float %1229, %1226
  %1231 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1232 = extractvalue %dx.types.CBufRet.f32 %1231, 0
  %1233 = extractvalue %dx.types.CBufRet.f32 %1231, 1
  %1234 = extractvalue %dx.types.CBufRet.f32 %1231, 3
  %1235 = fmul fast float %1234, %1232
  %1236 = fmul fast float %1234, %1233
  %1237 = fmul fast float %1235, %1228
  %1238 = fmul fast float %1236, %1230
  %1239 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1240 = extractvalue %dx.types.CBufRet.f32 %1239, 0
  %1241 = fadd fast float %1240, %1222
  %1242 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1237, float %1238, float %1241, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1243 = extractvalue %dx.types.ResRet.f32 %1242, 2
  %1244 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1245 = extractvalue %dx.types.CBufRet.f32 %1244, 1
  %1246 = fsub fast float %1243, %1245
  %1247 = extractvalue %dx.types.CBufRet.f32 %1244, 0
  %1248 = fmul fast float %1246, %1247
  %1249 = fcmp fast olt float %1194, %1248
  %1250 = icmp slt i32 %1136, 0
  br i1 %1250, label %1257, label %1251

; <label>:1251                                    ; preds = %1165
  %1252 = add i32 %1167, 5
  %1253 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1252, i32 undef)  ; BufferLoad(srv,index,wot)
  %1254 = extractvalue %dx.types.ResRet.f32 %1253, 0
  %1255 = fcmp fast ogt float %1254, 0.000000e+00
  br i1 %1255, label %1257, label %1256

; <label>:1256                                    ; preds = %1251
  br label %1257

; <label>:1257                                    ; preds = %1256, %1251, %1165
  %1258 = phi i1 [ false, %1256 ], [ false, %1165 ], [ true, %1251 ]
  %1259 = and i1 %1249, %1258
  br i1 %1259, label %1570, label %1260

; <label>:1260                                    ; preds = %1257, %1158, %1105
  %1261 = phi i32 [ %1166, %1257 ], [ %1106, %1158 ], [ %1106, %1105 ]
  %1262 = add i32 %943, %926
  %1263 = add i32 %944, %927
  %1264 = add i32 %945, %928
  %1265 = mul i32 %1262, %461
  %1266 = mul i32 %1263, %461
  %1267 = mul i32 %1264, %461
  %1268 = add i32 %1265, %472
  %1269 = add i32 %1266, %473
  %1270 = add i32 %1267, %474
  %1271 = or i32 %1269, %1268
  %1272 = or i32 %1271, %1270
  %1273 = icmp slt i32 %1272, 0
  %1274 = icmp sgt i32 %1268, %959
  %1275 = or i1 %1273, %1274
  %1276 = icmp sgt i32 %1269, %960
  %1277 = or i1 %1276, %1275
  %1278 = icmp sgt i32 %1270, %961
  %1279 = or i1 %1278, %1277
  br i1 %1279, label %1415, label %1280

; <label>:1280                                    ; preds = %1260
  %1281 = sitofp i32 %1268 to float
  %1282 = sitofp i32 %1269 to float
  %1283 = fmul fast float %953, %1282
  %1284 = fadd fast float %1283, %1281
  %1285 = sitofp i32 %1270 to float
  %1286 = fmul fast float %953, %1285
  %1287 = fmul fast float %1286, %954
  %1288 = fadd fast float %1284, %1287
  %1289 = fptosi float %1288 to i32
  %1290 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1289, i32 undef)  ; BufferLoad(srv,index,wot)
  %1291 = extractvalue %dx.types.ResRet.i32 %1290, 0
  %1292 = icmp sgt i32 %1291, -1
  br i1 %1292, label %1298, label %1293

; <label>:1293                                    ; preds = %1280
  %1294 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1295 = extractvalue %dx.types.CBufRet.f32 %1294, 0
  %1296 = extractvalue %dx.types.CBufRet.f32 %1294, 1
  %1297 = extractvalue %dx.types.CBufRet.f32 %1294, 2
  br label %1313

; <label>:1298                                    ; preds = %1280
  %1299 = mul i32 %1291, 10
  %1300 = add i32 %1299, 3
  %1301 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1300, i32 undef)  ; BufferLoad(srv,index,wot)
  %1302 = extractvalue %dx.types.ResRet.f32 %1301, 0
  %1303 = bitcast float %1302 to i32
  %1304 = and i32 %1303, 65535
  %1305 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1299, i32 undef)  ; BufferLoad(srv,index,wot)
  %1306 = extractvalue %dx.types.ResRet.f32 %1305, 0
  %1307 = or i32 %1299, 1
  %1308 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1307, i32 undef)  ; BufferLoad(srv,index,wot)
  %1309 = extractvalue %dx.types.ResRet.f32 %1308, 0
  %1310 = add i32 %1299, 2
  %1311 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1310, i32 undef)  ; BufferLoad(srv,index,wot)
  %1312 = extractvalue %dx.types.ResRet.f32 %1311, 0
  br label %1313

; <label>:1313                                    ; preds = %1298, %1293
  %1314 = phi i32 [ %1304, %1298 ], [ 0, %1293 ]
  %1315 = phi float [ %1306, %1298 ], [ %1295, %1293 ]
  %1316 = phi float [ %1309, %1298 ], [ %1296, %1293 ]
  %1317 = phi float [ %1312, %1298 ], [ %1297, %1293 ]
  %1318 = and i32 %1314, %429
  %1319 = icmp eq i32 %1318, 0
  br i1 %1319, label %1415, label %1320

; <label>:1320                                    ; preds = %1313
  %1321 = add nsw i32 %1261, 1
  %1322 = mul i32 %1291, 10
  %1323 = add i32 %1322, 6
  %1324 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1323, i32 undef)  ; BufferLoad(srv,index,wot)
  %1325 = extractvalue %dx.types.ResRet.f32 %1324, 0
  %1326 = fptoui float %1325 to i32
  %1327 = add i32 %1322, 7
  %1328 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1327, i32 undef)  ; BufferLoad(srv,index,wot)
  %1329 = extractvalue %dx.types.ResRet.f32 %1328, 0
  %1330 = fptoui float %1329 to i32
  %1331 = add i32 %1322, 8
  %1332 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1331, i32 undef)  ; BufferLoad(srv,index,wot)
  %1333 = extractvalue %dx.types.ResRet.f32 %1332, 0
  %1334 = fptoui float %1333 to i32
  %1335 = add i32 %1322, 9
  %1336 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1335, i32 undef)  ; BufferLoad(srv,index,wot)
  %1337 = extractvalue %dx.types.ResRet.f32 %1336, 0
  %1338 = fptoui float %1337 to i32
  %1339 = mul i32 %1338, %1326
  %1340 = mul i32 %1338, %1330
  %1341 = fsub fast float %372, %1315
  %1342 = fsub fast float %373, %1316
  %1343 = fsub fast float %374, %1317
  %1344 = call float @dx.op.dot3.f32(i32 55, float %1341, float %1342, float %1343, float %1341, float %1342, float %1343)  ; Dot3(ax,ay,az,bx,by,bz)
  %1345 = call float @dx.op.unary.f32(i32 25, float %1344)  ; Rsqrt(value)
  %1346 = fmul fast float %1345, %1341
  %1347 = fmul fast float %1345, %1342
  %1348 = fmul fast float %1345, %1343
  %1349 = call float @dx.op.dot3.f32(i32 55, float %1346, float %1347, float %1348, float %1341, float %1342, float %1343)  ; Dot3(ax,ay,az,bx,by,bz)
  %1350 = call float @dx.op.unary.f32(i32 6, float %1346)  ; FAbs(value)
  %1351 = call float @dx.op.unary.f32(i32 6, float %1347)  ; FAbs(value)
  %1352 = fadd fast float %1351, %1350
  %1353 = call float @dx.op.unary.f32(i32 6, float %1348)  ; FAbs(value)
  %1354 = fadd fast float %1352, %1353
  %1355 = fdiv fast float 1.000000e+00, %1354
  %1356 = fmul fast float %1355, %1346
  %1357 = fmul fast float %1355, %1347
  %1358 = fcmp fast ole float %1348, 0.000000e+00
  %1359 = call float @dx.op.unary.f32(i32 6, float %1357)  ; FAbs(value)
  %1360 = call float @dx.op.unary.f32(i32 6, float %1356)  ; FAbs(value)
  %1361 = fsub fast float 1.000000e+00, %1359
  %1362 = fsub fast float 1.000000e+00, %1360
  %1363 = fcmp fast oge float %1356, 0.000000e+00
  %1364 = select i1 %1363, float 1.000000e+00, float -1.000000e+00
  %1365 = fcmp fast oge float %1357, 0.000000e+00
  %1366 = select i1 %1365, float 1.000000e+00, float -1.000000e+00
  %1367 = fmul fast float %1364, %1361
  %1368 = fmul fast float %1362, %1366
  %1369 = select i1 %1358, float %1367, float %1356
  %1370 = select i1 %1358, float %1368, float %1357
  %1371 = fmul fast float %1369, 5.000000e-01
  %1372 = fmul fast float %1370, 5.000000e-01
  %1373 = fadd fast float %1371, 5.000000e-01
  %1374 = fadd fast float %1372, 5.000000e-01
  %1375 = sitofp i32 %1339 to float
  %1376 = sitofp i32 %1340 to float
  %1377 = sitofp i32 %1334 to float
  %1378 = sitofp i32 %1338 to float
  %1379 = fadd fast float %1378, -2.000000e+00
  %1380 = fmul fast float %1373, %1379
  %1381 = fmul fast float %1374, %1379
  %1382 = fadd fast float %1375, 1.000000e+00
  %1383 = fadd fast float %1382, %1380
  %1384 = fadd fast float %1376, 1.000000e+00
  %1385 = fadd fast float %1384, %1381
  %1386 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1387 = extractvalue %dx.types.CBufRet.f32 %1386, 0
  %1388 = extractvalue %dx.types.CBufRet.f32 %1386, 1
  %1389 = extractvalue %dx.types.CBufRet.f32 %1386, 3
  %1390 = fmul fast float %1389, %1387
  %1391 = fmul fast float %1389, %1388
  %1392 = fmul fast float %1390, %1383
  %1393 = fmul fast float %1391, %1385
  %1394 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1395 = extractvalue %dx.types.CBufRet.f32 %1394, 0
  %1396 = fadd fast float %1395, %1377
  %1397 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1392, float %1393, float %1396, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1398 = extractvalue %dx.types.ResRet.f32 %1397, 2
  %1399 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1400 = extractvalue %dx.types.CBufRet.f32 %1399, 1
  %1401 = fsub fast float %1398, %1400
  %1402 = extractvalue %dx.types.CBufRet.f32 %1399, 0
  %1403 = fmul fast float %1401, %1402
  %1404 = fcmp fast olt float %1349, %1403
  %1405 = icmp slt i32 %1291, 0
  br i1 %1405, label %1412, label %1406

; <label>:1406                                    ; preds = %1320
  %1407 = add i32 %1322, 5
  %1408 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1407, i32 undef)  ; BufferLoad(srv,index,wot)
  %1409 = extractvalue %dx.types.ResRet.f32 %1408, 0
  %1410 = fcmp fast ogt float %1409, 0.000000e+00
  br i1 %1410, label %1412, label %1411

; <label>:1411                                    ; preds = %1406
  br label %1412

; <label>:1412                                    ; preds = %1411, %1406, %1320
  %1413 = phi i1 [ false, %1411 ], [ false, %1320 ], [ true, %1406 ]
  %1414 = and i1 %1404, %1413
  br i1 %1414, label %1570, label %1415

; <label>:1415                                    ; preds = %1412, %1313, %1260
  %1416 = phi i32 [ %1321, %1412 ], [ %1261, %1313 ], [ %1261, %1260 ]
  %1417 = add i32 %1107, %943
  %1418 = add i32 %1108, %944
  %1419 = add i32 %1109, %945
  %1420 = mul i32 %1417, %461
  %1421 = mul i32 %1418, %461
  %1422 = mul i32 %1419, %461
  %1423 = add i32 %1420, %472
  %1424 = add i32 %1421, %473
  %1425 = add i32 %1422, %474
  %1426 = or i32 %1424, %1423
  %1427 = or i32 %1426, %1425
  %1428 = icmp slt i32 %1427, 0
  %1429 = icmp sgt i32 %1423, %959
  %1430 = or i1 %1428, %1429
  %1431 = icmp sgt i32 %1424, %960
  %1432 = or i1 %1431, %1430
  %1433 = icmp sgt i32 %1425, %961
  %1434 = or i1 %1433, %1432
  br i1 %1434, label %1573, label %1435

; <label>:1435                                    ; preds = %1415
  %1436 = sitofp i32 %1423 to float
  %1437 = sitofp i32 %1424 to float
  %1438 = fmul fast float %953, %1437
  %1439 = fadd fast float %1438, %1436
  %1440 = sitofp i32 %1425 to float
  %1441 = fmul fast float %953, %1440
  %1442 = fmul fast float %1441, %954
  %1443 = fadd fast float %1439, %1442
  %1444 = fptosi float %1443 to i32
  %1445 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1444, i32 undef)  ; BufferLoad(srv,index,wot)
  %1446 = extractvalue %dx.types.ResRet.i32 %1445, 0
  %1447 = icmp sgt i32 %1446, -1
  br i1 %1447, label %1453, label %1448

; <label>:1448                                    ; preds = %1435
  %1449 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1450 = extractvalue %dx.types.CBufRet.f32 %1449, 0
  %1451 = extractvalue %dx.types.CBufRet.f32 %1449, 1
  %1452 = extractvalue %dx.types.CBufRet.f32 %1449, 2
  br label %1468

; <label>:1453                                    ; preds = %1435
  %1454 = mul i32 %1446, 10
  %1455 = add i32 %1454, 3
  %1456 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1455, i32 undef)  ; BufferLoad(srv,index,wot)
  %1457 = extractvalue %dx.types.ResRet.f32 %1456, 0
  %1458 = bitcast float %1457 to i32
  %1459 = and i32 %1458, 65535
  %1460 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1454, i32 undef)  ; BufferLoad(srv,index,wot)
  %1461 = extractvalue %dx.types.ResRet.f32 %1460, 0
  %1462 = or i32 %1454, 1
  %1463 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1462, i32 undef)  ; BufferLoad(srv,index,wot)
  %1464 = extractvalue %dx.types.ResRet.f32 %1463, 0
  %1465 = add i32 %1454, 2
  %1466 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1465, i32 undef)  ; BufferLoad(srv,index,wot)
  %1467 = extractvalue %dx.types.ResRet.f32 %1466, 0
  br label %1468

; <label>:1468                                    ; preds = %1453, %1448
  %1469 = phi i32 [ %1459, %1453 ], [ 0, %1448 ]
  %1470 = phi float [ %1461, %1453 ], [ %1450, %1448 ]
  %1471 = phi float [ %1464, %1453 ], [ %1451, %1448 ]
  %1472 = phi float [ %1467, %1453 ], [ %1452, %1448 ]
  %1473 = and i32 %1469, %429
  %1474 = icmp eq i32 %1473, 0
  br i1 %1474, label %1573, label %1475

; <label>:1475                                    ; preds = %1468
  %1476 = add nsw i32 %1416, 1
  %1477 = mul i32 %1446, 10
  %1478 = add i32 %1477, 6
  %1479 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1478, i32 undef)  ; BufferLoad(srv,index,wot)
  %1480 = extractvalue %dx.types.ResRet.f32 %1479, 0
  %1481 = fptoui float %1480 to i32
  %1482 = add i32 %1477, 7
  %1483 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1482, i32 undef)  ; BufferLoad(srv,index,wot)
  %1484 = extractvalue %dx.types.ResRet.f32 %1483, 0
  %1485 = fptoui float %1484 to i32
  %1486 = add i32 %1477, 8
  %1487 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1486, i32 undef)  ; BufferLoad(srv,index,wot)
  %1488 = extractvalue %dx.types.ResRet.f32 %1487, 0
  %1489 = fptoui float %1488 to i32
  %1490 = add i32 %1477, 9
  %1491 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1490, i32 undef)  ; BufferLoad(srv,index,wot)
  %1492 = extractvalue %dx.types.ResRet.f32 %1491, 0
  %1493 = fptoui float %1492 to i32
  %1494 = mul i32 %1493, %1481
  %1495 = mul i32 %1493, %1485
  %1496 = fsub fast float %372, %1470
  %1497 = fsub fast float %373, %1471
  %1498 = fsub fast float %374, %1472
  %1499 = call float @dx.op.dot3.f32(i32 55, float %1496, float %1497, float %1498, float %1496, float %1497, float %1498)  ; Dot3(ax,ay,az,bx,by,bz)
  %1500 = call float @dx.op.unary.f32(i32 25, float %1499)  ; Rsqrt(value)
  %1501 = fmul fast float %1500, %1496
  %1502 = fmul fast float %1500, %1497
  %1503 = fmul fast float %1500, %1498
  %1504 = call float @dx.op.dot3.f32(i32 55, float %1501, float %1502, float %1503, float %1496, float %1497, float %1498)  ; Dot3(ax,ay,az,bx,by,bz)
  %1505 = call float @dx.op.unary.f32(i32 6, float %1501)  ; FAbs(value)
  %1506 = call float @dx.op.unary.f32(i32 6, float %1502)  ; FAbs(value)
  %1507 = fadd fast float %1506, %1505
  %1508 = call float @dx.op.unary.f32(i32 6, float %1503)  ; FAbs(value)
  %1509 = fadd fast float %1507, %1508
  %1510 = fdiv fast float 1.000000e+00, %1509
  %1511 = fmul fast float %1510, %1501
  %1512 = fmul fast float %1510, %1502
  %1513 = fcmp fast ole float %1503, 0.000000e+00
  %1514 = call float @dx.op.unary.f32(i32 6, float %1512)  ; FAbs(value)
  %1515 = call float @dx.op.unary.f32(i32 6, float %1511)  ; FAbs(value)
  %1516 = fsub fast float 1.000000e+00, %1514
  %1517 = fsub fast float 1.000000e+00, %1515
  %1518 = fcmp fast oge float %1511, 0.000000e+00
  %1519 = select i1 %1518, float 1.000000e+00, float -1.000000e+00
  %1520 = fcmp fast oge float %1512, 0.000000e+00
  %1521 = select i1 %1520, float 1.000000e+00, float -1.000000e+00
  %1522 = fmul fast float %1519, %1516
  %1523 = fmul fast float %1517, %1521
  %1524 = select i1 %1513, float %1522, float %1511
  %1525 = select i1 %1513, float %1523, float %1512
  %1526 = fmul fast float %1524, 5.000000e-01
  %1527 = fmul fast float %1525, 5.000000e-01
  %1528 = fadd fast float %1526, 5.000000e-01
  %1529 = fadd fast float %1527, 5.000000e-01
  %1530 = sitofp i32 %1494 to float
  %1531 = sitofp i32 %1495 to float
  %1532 = sitofp i32 %1489 to float
  %1533 = sitofp i32 %1493 to float
  %1534 = fadd fast float %1533, -2.000000e+00
  %1535 = fmul fast float %1528, %1534
  %1536 = fmul fast float %1529, %1534
  %1537 = fadd fast float %1530, 1.000000e+00
  %1538 = fadd fast float %1537, %1535
  %1539 = fadd fast float %1531, 1.000000e+00
  %1540 = fadd fast float %1539, %1536
  %1541 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1542 = extractvalue %dx.types.CBufRet.f32 %1541, 0
  %1543 = extractvalue %dx.types.CBufRet.f32 %1541, 1
  %1544 = extractvalue %dx.types.CBufRet.f32 %1541, 3
  %1545 = fmul fast float %1544, %1542
  %1546 = fmul fast float %1544, %1543
  %1547 = fmul fast float %1545, %1538
  %1548 = fmul fast float %1546, %1540
  %1549 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1550 = extractvalue %dx.types.CBufRet.f32 %1549, 0
  %1551 = fadd fast float %1550, %1532
  %1552 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1547, float %1548, float %1551, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1553 = extractvalue %dx.types.ResRet.f32 %1552, 2
  %1554 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1555 = extractvalue %dx.types.CBufRet.f32 %1554, 1
  %1556 = fsub fast float %1553, %1555
  %1557 = extractvalue %dx.types.CBufRet.f32 %1554, 0
  %1558 = fmul fast float %1556, %1557
  %1559 = fcmp fast olt float %1504, %1558
  %1560 = icmp slt i32 %1446, 0
  br i1 %1560, label %1567, label %1561

; <label>:1561                                    ; preds = %1475
  %1562 = add i32 %1477, 5
  %1563 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1562, i32 undef)  ; BufferLoad(srv,index,wot)
  %1564 = extractvalue %dx.types.ResRet.f32 %1563, 0
  %1565 = fcmp fast ogt float %1564, 0.000000e+00
  br i1 %1565, label %1567, label %1566

; <label>:1566                                    ; preds = %1561
  br label %1567

; <label>:1567                                    ; preds = %1566, %1561, %1475
  %1568 = phi i1 [ false, %1566 ], [ false, %1475 ], [ true, %1561 ]
  %1569 = and i1 %1559, %1568
  br i1 %1569, label %1570, label %1573

; <label>:1570                                    ; preds = %1567, %1412, %1257, %1102
  %1571 = phi i32 [ 1, %1102 ], [ %1166, %1257 ], [ %1321, %1412 ], [ %1476, %1567 ]
  %1572 = add nsw i32 %915, 1
  br label %1573

; <label>:1573                                    ; preds = %1570, %1567, %1468, %1415
  %1574 = phi i32 [ %1572, %1570 ], [ %915, %1415 ], [ %915, %1468 ], [ %915, %1567 ]
  %1575 = phi i32 [ %1571, %1570 ], [ %1416, %1415 ], [ %1416, %1468 ], [ %1476, %1567 ]
  %1576 = icmp sgt i32 %1574, 0
  %1577 = icmp sgt i32 %1575, %647
  %1578 = or i1 %1576, %1577
  br i1 %1578, label %1589, label %1579

; <label>:1579                                    ; preds = %1573
  %1580 = icmp slt i32 %929, 3
  br i1 %1580, label %1581, label %1589

; <label>:1581                                    ; preds = %1579
  %1582 = getelementptr [3 x i32], [3 x i32]* %23, i32 0, i32 %929
  %1583 = load i32, i32* %1582, align 4
  %1584 = getelementptr [3 x i32], [3 x i32]* %24, i32 0, i32 %929
  %1585 = load i32, i32* %1584, align 4
  %1586 = getelementptr [3 x i32], [3 x i32]* %25, i32 0, i32 %929
  %1587 = load i32, i32* %1586, align 4
  br label %911

; <label>:1588                                    ; preds = %880
  br label %1594

; <label>:1589                                    ; preds = %1579, %1573
  %1590 = phi i32 [ %1575, %1573 ], [ %647, %1579 ]
  %1591 = phi i32 [ %914, %1573 ], [ 0, %1579 ]
  %1592 = phi i32 [ %913, %1573 ], [ 0, %1579 ]
  %1593 = phi i32 [ %912, %1573 ], [ 0, %1579 ]
  br label %1594

; <label>:1594                                    ; preds = %1589, %1588, %660
  %1595 = phi i32 [ %647, %660 ], [ %881, %1588 ], [ %1590, %1589 ]
  %1596 = phi i32 [ %648, %660 ], [ %882, %1588 ], [ %1574, %1589 ]
  %1597 = phi i32 [ 0, %660 ], [ %883, %1588 ], [ %1591, %1589 ]
  %1598 = phi i32 [ 0, %660 ], [ %884, %1588 ], [ %1592, %1589 ]
  %1599 = phi i32 [ 0, %660 ], [ %885, %1588 ], [ %1593, %1589 ]
  %1600 = select i1 %652, i32 2, i32 1
  %1601 = icmp slt i32 %1595, %1600
  br i1 %1601, label %1610, label %1602

; <label>:1602                                    ; preds = %1594
  %1603 = icmp sgt i32 %1596, 0
  br i1 %1603, label %1617, label %1604

; <label>:1604                                    ; preds = %1602
  %1605 = icmp slt i32 %422, 0
  %1606 = select i1 %1605, i32 %426, i32 %422
  %1607 = select i1 %1605, i32 %1597, i32 %423
  %1608 = select i1 %1605, i32 %1598, i32 %424
  %1609 = select i1 %1605, i32 %1599, i32 %425
  br label %1610

; <label>:1610                                    ; preds = %1604, %1594, %421
  %1611 = phi i32 [ %422, %1594 ], [ %422, %421 ], [ %1606, %1604 ]
  %1612 = phi i32 [ %423, %1594 ], [ %423, %421 ], [ %1607, %1604 ]
  %1613 = phi i32 [ %424, %1594 ], [ %424, %421 ], [ %1608, %1604 ]
  %1614 = phi i32 [ %425, %1594 ], [ %425, %421 ], [ %1609, %1604 ]
  %1615 = add nuw nsw i32 %426, 1
  %1616 = icmp slt i32 %1615, 3
  br i1 %1616, label %421, label %1617

; <label>:1617                                    ; preds = %1610, %1602
  %1618 = phi i32 [ %426, %1602 ], [ %1611, %1610 ]
  %1619 = phi i32 [ %1597, %1602 ], [ %1612, %1610 ]
  %1620 = phi i32 [ %1598, %1602 ], [ %1613, %1610 ]
  %1621 = phi i32 [ %1599, %1602 ], [ %1614, %1610 ]
  %1622 = sitofp i32 %1618 to float
  %1623 = call float @dx.op.unary.f32(i32 21, float %1622)  ; Exp(value)
  %1624 = fptosi float %1623 to i32
  %1625 = sitofp i32 %1624 to float
  %1626 = fdiv fast float %416, %1625
  %1627 = fdiv fast float %417, %1625
  %1628 = fdiv fast float %418, %1625
  %1629 = call float @dx.op.unary.f32(i32 27, float %1626)  ; Round_ni(value)
  %1630 = call float @dx.op.unary.f32(i32 27, float %1627)  ; Round_ni(value)
  %1631 = call float @dx.op.unary.f32(i32 27, float %1628)  ; Round_ni(value)
  %1632 = fptosi float %1629 to i32
  %1633 = fptosi float %1630 to i32
  %1634 = fptosi float %1631 to i32
  %1635 = add i32 %1632, %1619
  %1636 = add i32 %1633, %1620
  %1637 = add i32 %1634, %1621
  %1638 = mul i32 %1635, %1624
  %1639 = mul i32 %1636, %1624
  %1640 = mul i32 %1637, %1624
  br label %1642

; <label>:1641                                    ; preds = %655
  br label %1642

; <label>:1642                                    ; preds = %1641, %1617, %439
  %1643 = phi i32 [ %455, %439 ], [ %1638, %1617 ], [ %472, %1641 ]
  %1644 = phi i32 [ %456, %439 ], [ %1639, %1617 ], [ %473, %1641 ]
  %1645 = phi i32 [ %457, %439 ], [ %1640, %1617 ], [ %474, %1641 ]
  %1646 = phi i32 [ %441, %439 ], [ %1618, %1617 ], [ %426, %1641 ]
  %1647 = icmp slt i32 %1646, 0
  br i1 %1647, label %1648, label %1650

; <label>:1648                                    ; preds = %1642
  %1649 = extractvalue %dx.types.CBufRet.f32 %383, 3
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2, i32 %20, i32 %21, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float %1649, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %5543

; <label>:1650                                    ; preds = %1642
  %1651 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 0
  store i32 -1, i32* %1651, align 4
  %1652 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 1
  store i32 -1, i32* %1652, align 4
  %1653 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 2
  store i32 -1, i32* %1653, align 4
  %1654 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 3
  store i32 -1, i32* %1654, align 4
  %1655 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 4
  store i32 -1, i32* %1655, align 4
  %1656 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 5
  store i32 -1, i32* %1656, align 4
  %1657 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 6
  store i32 -1, i32* %1657, align 4
  %1658 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 7
  store i32 -1, i32* %1658, align 4
  %1659 = sitofp i32 %1646 to float
  %1660 = call float @dx.op.unary.f32(i32 21, float %1659)  ; Exp(value)
  %1661 = fptosi float %1660 to i32
  br label %1662

; <label>:1662                                    ; preds = %1728, %1650
  %1663 = phi i32 [ 0, %1650 ], [ %1729, %1728 ]
  %1664 = and i32 %1663, 1
  %1665 = lshr i32 %1663, 1
  %1666 = and i32 %1665, 1
  %1667 = lshr i32 %1663, 2
  %1668 = and i32 %1667, 1
  %1669 = sub nsw i32 0, %1664
  %1670 = and i32 %1661, %1669
  %1671 = sub nsw i32 0, %1666
  %1672 = and i32 %1661, %1671
  %1673 = sub nsw i32 0, %1668
  %1674 = and i32 %1661, %1673
  %1675 = add i32 %1670, %1643
  %1676 = add i32 %1672, %1644
  %1677 = add i32 %1674, %1645
  %1678 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1679 = extractvalue %dx.types.CBufRet.f32 %1678, 0
  %1680 = extractvalue %dx.types.CBufRet.f32 %1678, 1
  %1681 = extractvalue %dx.types.CBufRet.f32 %1678, 2
  %1682 = fptosi float %1679 to i32
  %1683 = fptosi float %1680 to i32
  %1684 = fptosi float %1681 to i32
  %1685 = add i32 %1682, -1
  %1686 = add i32 %1683, -1
  %1687 = add i32 %1684, -1
  %1688 = or i32 %1676, %1675
  %1689 = or i32 %1688, %1677
  %1690 = icmp slt i32 %1689, 0
  %1691 = icmp sgt i32 %1675, %1685
  %1692 = or i1 %1691, %1690
  %1693 = icmp sgt i32 %1676, %1686
  %1694 = or i1 %1693, %1692
  %1695 = icmp sgt i32 %1677, %1687
  %1696 = or i1 %1695, %1694
  br i1 %1696, label %1728, label %1697

; <label>:1697                                    ; preds = %1662
  %1698 = sitofp i32 %1675 to float
  %1699 = sitofp i32 %1676 to float
  %1700 = fmul fast float %1679, %1699
  %1701 = fadd fast float %1700, %1698
  %1702 = sitofp i32 %1677 to float
  %1703 = fmul fast float %1679, %1702
  %1704 = fmul fast float %1703, %1680
  %1705 = fadd fast float %1701, %1704
  %1706 = fptosi float %1705 to i32
  %1707 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %4, i32 %1706, i32 undef)  ; BufferLoad(srv,index,wot)
  %1708 = extractvalue %dx.types.ResRet.i32 %1707, 0
  %1709 = icmp slt i32 %1708, 0
  br i1 %1709, label %1728, label %1710

; <label>:1710                                    ; preds = %1697
  %1711 = mul i32 %1708, 10
  %1712 = add i32 %1711, 3
  %1713 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1712, i32 undef)  ; BufferLoad(srv,index,wot)
  %1714 = extractvalue %dx.types.ResRet.f32 %1713, 0
  %1715 = bitcast float %1714 to i32
  %1716 = and i32 %1646, 31
  %1717 = shl i32 1, %1716
  %1718 = and i32 %1717, 65535
  %1719 = and i32 %1718, %1715
  %1720 = icmp ne i32 %1719, 0
  %1721 = add i32 %1711, 5
  %1722 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1721, i32 undef)  ; BufferLoad(srv,index,wot)
  %1723 = extractvalue %dx.types.ResRet.f32 %1722, 0
  %1724 = fcmp fast ogt float %1723, 0.000000e+00
  %1725 = and i1 %1724, %1720
  br i1 %1725, label %1726, label %1728

; <label>:1726                                    ; preds = %1710
  %1727 = getelementptr inbounds [8 x i32], [8 x i32]* %34, i32 0, i32 %1663
  store i32 %1708, i32* %1727, align 4, !tbaa !34, !alias.scope !42, !noalias !45
  br label %1728

; <label>:1728                                    ; preds = %1726, %1710, %1697, %1662
  %1729 = add nuw nsw i32 %1663, 1
  %1730 = icmp eq i32 %1729, 8
  br i1 %1730, label %1731, label %1662

; <label>:1731                                    ; preds = %1728
  %1732 = load i32, i32* %1651, align 4
  %1733 = load i32, i32* %1652, align 4
  %1734 = load i32, i32* %1653, align 4
  %1735 = load i32, i32* %1654, align 4
  %1736 = load i32, i32* %1655, align 4
  %1737 = load i32, i32* %1656, align 4
  %1738 = load i32, i32* %1657, align 4
  %1739 = load i32, i32* %1658, align 4
  %1740 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 0
  store i32 %1732, i32* %1740, align 4
  %1741 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 1
  store i32 %1733, i32* %1741, align 4
  %1742 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 2
  store i32 %1734, i32* %1742, align 4
  %1743 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 3
  store i32 %1735, i32* %1743, align 4
  %1744 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 4
  store i32 %1736, i32* %1744, align 4
  %1745 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 5
  store i32 %1737, i32* %1745, align 4
  %1746 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 6
  store i32 %1738, i32* %1746, align 4
  %1747 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 7
  store i32 %1739, i32* %1747, align 4
  %1748 = extractvalue %dx.types.CBufRet.f32 %1678, 3
  %1749 = fdiv fast float %402, %1748
  %1750 = fdiv fast float %406, %1748
  %1751 = fdiv fast float %410, %1748
  %1752 = sitofp i32 %1643 to float
  %1753 = sitofp i32 %1644 to float
  %1754 = sitofp i32 %1645 to float
  %1755 = fsub fast float %1749, %1752
  %1756 = fsub fast float %1750, %1753
  %1757 = fsub fast float %1751, %1754
  %1758 = sitofp i32 %1661 to float
  %1759 = fdiv fast float %1755, %1758
  %1760 = fdiv fast float %1756, %1758
  %1761 = fdiv fast float %1757, %1758
  %1762 = fcmp fast olt float %1759, 0.000000e+00
  %1763 = fcmp fast ogt float %1759, 1.000000e+00
  %1764 = or i1 %1762, %1763
  br i1 %1764, label %1765, label %1769

; <label>:1765                                    ; preds = %1731
  %1766 = fmul fast float %1759, 2.000000e+00
  %1767 = fadd fast float %1766, -1.000000e+00
  %1768 = fdiv fast float %1759, %1767
  br label %1769

; <label>:1769                                    ; preds = %1765, %1731
  %1770 = phi float [ %1768, %1765 ], [ %1759, %1731 ]
  %1771 = fcmp fast olt float %1760, 0.000000e+00
  %1772 = fcmp fast ogt float %1760, 1.000000e+00
  %1773 = or i1 %1771, %1772
  br i1 %1773, label %1774, label %1778

; <label>:1774                                    ; preds = %1769
  %1775 = fmul fast float %1760, 2.000000e+00
  %1776 = fadd fast float %1775, -1.000000e+00
  %1777 = fdiv fast float %1760, %1776
  br label %1778

; <label>:1778                                    ; preds = %1774, %1769
  %1779 = phi float [ %1777, %1774 ], [ %1760, %1769 ]
  %1780 = fcmp fast olt float %1761, 0.000000e+00
  %1781 = fcmp fast ogt float %1761, 1.000000e+00
  %1782 = or i1 %1780, %1781
  br i1 %1782, label %1783, label %1787

; <label>:1783                                    ; preds = %1778
  %1784 = fmul fast float %1761, 2.000000e+00
  %1785 = fadd fast float %1784, -1.000000e+00
  %1786 = fdiv fast float %1761, %1785
  br label %1787

; <label>:1787                                    ; preds = %1783, %1778
  %1788 = phi float [ %1786, %1783 ], [ %1761, %1778 ]
  br label %1789

; <label>:1789                                    ; preds = %2101, %1787
  %1790 = phi i32 [ %1732, %1787 ], [ %2103, %2101 ]
  %1791 = phi float [ 0.000000e+00, %1787 ], [ %2097, %2101 ]
  %1792 = phi float [ 0.000000e+00, %1787 ], [ %2098, %2101 ]
  %1793 = phi i32 [ 0, %1787 ], [ %2099, %2101 ]
  %1794 = icmp slt i32 %1790, 0
  br i1 %1794, label %2096, label %1795

; <label>:1795                                    ; preds = %1789
  %1796 = mul i32 %1790, 10
  %1797 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1796, i32 undef)  ; BufferLoad(srv,index,wot)
  %1798 = extractvalue %dx.types.ResRet.f32 %1797, 0
  %1799 = or i32 %1796, 1
  %1800 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1799, i32 undef)  ; BufferLoad(srv,index,wot)
  %1801 = extractvalue %dx.types.ResRet.f32 %1800, 0
  %1802 = add i32 %1796, 2
  %1803 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1802, i32 undef)  ; BufferLoad(srv,index,wot)
  %1804 = extractvalue %dx.types.ResRet.f32 %1803, 0
  %1805 = add i32 %1796, 4
  %1806 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1805, i32 undef)  ; BufferLoad(srv,index,wot)
  %1807 = extractvalue %dx.types.ResRet.f32 %1806, 0
  %1808 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %1809 = extractvalue %dx.types.CBufRet.f32 %1808, 2
  %1810 = fsub fast float %1809, %1807
  %1811 = extractvalue %dx.types.CBufRet.f32 %1808, 3
  %1812 = fdiv fast float %1810, %1811
  %1813 = call float @dx.op.unary.f32(i32 7, float %1812)  ; Saturate(value)
  %1814 = fsub fast float %1798, %372
  %1815 = fsub fast float %1801, %373
  %1816 = fsub fast float %1804, %374
  %1817 = call float @dx.op.dot3.f32(i32 55, float %1814, float %1815, float %1816, float %1814, float %1815, float %1816)  ; Dot3(ax,ay,az,bx,by,bz)
  %1818 = call float @dx.op.unary.f32(i32 25, float %1817)  ; Rsqrt(value)
  %1819 = fmul fast float %1818, %1814
  %1820 = fmul fast float %1818, %1815
  %1821 = fmul fast float %1818, %1816
  %1822 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float %1819, float %1820, float %1821)  ; Dot3(ax,ay,az,bx,by,bz)
  %1823 = and i32 %1793, 1
  %1824 = lshr i32 %1793, 1
  %1825 = and i32 %1824, 1
  %1826 = lshr i32 %1793, 2
  %1827 = and i32 %1826, 1
  %1828 = sitofp i32 %1823 to float
  %1829 = sitofp i32 %1825 to float
  %1830 = sitofp i32 %1827 to float
  %1831 = fsub fast float 1.000000e+00, %1770
  %1832 = fsub fast float 1.000000e+00, %1779
  %1833 = fsub fast float 1.000000e+00, %1788
  %1834 = fsub fast float %1770, %1831
  %1835 = fsub fast float %1779, %1832
  %1836 = fsub fast float %1788, %1833
  %1837 = fmul fast float %1828, %1834
  %1838 = fmul fast float %1829, %1835
  %1839 = fmul fast float %1830, %1836
  %1840 = fadd fast float %1837, %1831
  %1841 = fadd fast float %1838, %1832
  %1842 = fadd fast float %1839, %1833
  %1843 = fmul fast float %1841, %1840
  %1844 = fmul fast float %1843, %1842
  %1845 = call float @dx.op.binary.f32(i32 35, float %1844, float 0x3F847AE140000000)  ; FMax(a,b)
  %1846 = fsub fast float %372, %1798
  %1847 = fsub fast float %373, %1801
  %1848 = fsub fast float %374, %1804
  %1849 = call float @dx.op.dot3.f32(i32 55, float %1846, float %1847, float %1848, float %1846, float %1847, float %1848)  ; Dot3(ax,ay,az,bx,by,bz)
  %1850 = call float @dx.op.unary.f32(i32 25, float %1849)  ; Rsqrt(value)
  %1851 = fmul fast float %1850, %1846
  %1852 = fmul fast float %1850, %1847
  %1853 = fmul fast float %1850, %1848
  %1854 = fmul fast float %1846, %1846
  %1855 = fmul fast float %1847, %1847
  %1856 = fadd fast float %1855, %1854
  %1857 = fmul fast float %1848, %1848
  %1858 = fadd fast float %1856, %1857
  %1859 = call float @dx.op.unary.f32(i32 24, float %1858)  ; Sqrt(value)
  %1860 = call float @dx.op.unary.f32(i32 6, float %1851)  ; FAbs(value)
  %1861 = call float @dx.op.unary.f32(i32 6, float %1852)  ; FAbs(value)
  %1862 = fadd fast float %1861, %1860
  %1863 = call float @dx.op.unary.f32(i32 6, float %1853)  ; FAbs(value)
  %1864 = fadd fast float %1862, %1863
  %1865 = fdiv fast float 1.000000e+00, %1864
  %1866 = fmul fast float %1865, %1851
  %1867 = fmul fast float %1865, %1852
  %1868 = fcmp fast ole float %1853, 0.000000e+00
  %1869 = call float @dx.op.unary.f32(i32 6, float %1867)  ; FAbs(value)
  %1870 = call float @dx.op.unary.f32(i32 6, float %1866)  ; FAbs(value)
  %1871 = fsub fast float 1.000000e+00, %1869
  %1872 = fsub fast float 1.000000e+00, %1870
  %1873 = fcmp fast oge float %1866, 0.000000e+00
  %1874 = select i1 %1873, float 1.000000e+00, float -1.000000e+00
  %1875 = fcmp fast oge float %1867, 0.000000e+00
  %1876 = select i1 %1875, float 1.000000e+00, float -1.000000e+00
  %1877 = fmul fast float %1874, %1871
  %1878 = fmul fast float %1872, %1876
  %1879 = select i1 %1868, float %1877, float %1866
  %1880 = select i1 %1868, float %1878, float %1867
  %1881 = fmul fast float %1879, 5.000000e-01
  %1882 = fmul fast float %1880, 5.000000e-01
  %1883 = fadd fast float %1881, 5.000000e-01
  %1884 = fadd fast float %1882, 5.000000e-01
  %1885 = add i32 %1796, 6
  %1886 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1885, i32 undef)  ; BufferLoad(srv,index,wot)
  %1887 = extractvalue %dx.types.ResRet.f32 %1886, 0
  %1888 = fptoui float %1887 to i32
  %1889 = add i32 %1796, 7
  %1890 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1889, i32 undef)  ; BufferLoad(srv,index,wot)
  %1891 = extractvalue %dx.types.ResRet.f32 %1890, 0
  %1892 = fptoui float %1891 to i32
  %1893 = add i32 %1796, 8
  %1894 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1893, i32 undef)  ; BufferLoad(srv,index,wot)
  %1895 = extractvalue %dx.types.ResRet.f32 %1894, 0
  %1896 = fptoui float %1895 to i32
  %1897 = add i32 %1796, 9
  %1898 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1897, i32 undef)  ; BufferLoad(srv,index,wot)
  %1899 = extractvalue %dx.types.ResRet.f32 %1898, 0
  %1900 = fptoui float %1899 to i32
  %1901 = mul i32 %1900, %1888
  %1902 = mul i32 %1900, %1892
  %1903 = sitofp i32 %1901 to float
  %1904 = sitofp i32 %1902 to float
  %1905 = sitofp i32 %1896 to float
  %1906 = sitofp i32 %1900 to float
  %1907 = fadd fast float %1906, -2.000000e+00
  %1908 = fmul fast float %1907, %1883
  %1909 = fmul fast float %1907, %1884
  %1910 = fadd fast float %1903, 1.000000e+00
  %1911 = fadd fast float %1910, %1908
  %1912 = fadd fast float %1904, 1.000000e+00
  %1913 = fadd fast float %1912, %1909
  %1914 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1915 = extractvalue %dx.types.CBufRet.f32 %1914, 0
  %1916 = extractvalue %dx.types.CBufRet.f32 %1914, 1
  %1917 = extractvalue %dx.types.CBufRet.f32 %1914, 3
  %1918 = fmul fast float %1917, %1915
  %1919 = fmul fast float %1917, %1916
  %1920 = fmul fast float %1918, %1911
  %1921 = fmul fast float %1919, %1913
  %1922 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %1923 = extractvalue %dx.types.CBufRet.f32 %1922, 0
  %1924 = fadd fast float %1923, %1905
  %1925 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1920, float %1921, float %1924, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1926 = extractvalue %dx.types.ResRet.f32 %1925, 0
  %1927 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1928 = extractvalue %dx.types.CBufRet.f32 %1927, 1
  %1929 = fadd fast float %1928, %1859
  %1930 = extractvalue %dx.types.CBufRet.f32 %1927, 0
  %1931 = fdiv fast float %1929, %1930
  %1932 = fcmp fast ogt float %1931, %1926
  br i1 %1932, label %1933, label %1942

; <label>:1933                                    ; preds = %1795
  %1934 = extractvalue %dx.types.ResRet.f32 %1925, 1
  %1935 = fmul fast float %1926, %1926
  %1936 = fsub fast float %1935, %1934
  %1937 = call float @dx.op.unary.f32(i32 6, float %1936)  ; FAbs(value)
  %1938 = fsub fast float %1931, %1926
  %1939 = fmul fast float %1938, %1938
  %1940 = fadd fast float %1937, %1939
  %1941 = fdiv fast float %1937, %1940
  br label %1942

; <label>:1942                                    ; preds = %1933, %1795
  %1943 = phi float [ %1941, %1933 ], [ 1.000000e+00, %1795 ]
  %1944 = fsub fast float 1.000000e+00, %1943
  %1945 = call float @dx.op.unary.f32(i32 7, float %1822)  ; Saturate(value)
  %1946 = fmul fast float %1944, %1945
  %1947 = call float @dx.op.unary.f32(i32 6, float %352)  ; FAbs(value)
  %1948 = call float @dx.op.unary.f32(i32 6, float %353)  ; FAbs(value)
  %1949 = fadd fast float %1948, %1947
  %1950 = call float @dx.op.unary.f32(i32 6, float %354)  ; FAbs(value)
  %1951 = fadd fast float %1949, %1950
  %1952 = fdiv fast float 1.000000e+00, %1951
  %1953 = fmul fast float %1952, %352
  %1954 = fmul fast float %1952, %353
  %1955 = fcmp fast ole float %354, 0.000000e+00
  %1956 = call float @dx.op.unary.f32(i32 6, float %1954)  ; FAbs(value)
  %1957 = call float @dx.op.unary.f32(i32 6, float %1953)  ; FAbs(value)
  %1958 = fsub fast float 1.000000e+00, %1956
  %1959 = fsub fast float 1.000000e+00, %1957
  %1960 = fcmp fast oge float %1953, 0.000000e+00
  %1961 = select i1 %1960, float 1.000000e+00, float -1.000000e+00
  %1962 = fcmp fast oge float %1954, 0.000000e+00
  %1963 = select i1 %1962, float 1.000000e+00, float -1.000000e+00
  %1964 = fmul fast float %1961, %1958
  %1965 = fmul fast float %1959, %1963
  %1966 = select i1 %1955, float %1964, float %1953
  %1967 = select i1 %1955, float %1965, float %1954
  %1968 = fmul fast float %1966, 5.000000e-01
  %1969 = fmul fast float %1967, 5.000000e-01
  %1970 = fadd fast float %1968, 5.000000e-01
  %1971 = fadd fast float %1969, 5.000000e-01
  %1972 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1885, i32 undef)  ; BufferLoad(srv,index,wot)
  %1973 = extractvalue %dx.types.ResRet.f32 %1972, 0
  %1974 = fptoui float %1973 to i32
  %1975 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1889, i32 undef)  ; BufferLoad(srv,index,wot)
  %1976 = extractvalue %dx.types.ResRet.f32 %1975, 0
  %1977 = fptoui float %1976 to i32
  %1978 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %1893, i32 undef)  ; BufferLoad(srv,index,wot)
  %1979 = extractvalue %dx.types.ResRet.f32 %1978, 0
  %1980 = fptoui float %1979 to i32
  %1981 = mul i32 %1900, %1974
  %1982 = mul i32 %1900, %1977
  %1983 = sitofp i32 %1981 to float
  %1984 = sitofp i32 %1982 to float
  %1985 = sitofp i32 %1980 to float
  %1986 = fmul fast float %1907, %1970
  %1987 = fmul fast float %1907, %1971
  %1988 = fadd fast float %1983, 1.000000e+00
  %1989 = fadd fast float %1988, %1986
  %1990 = fadd fast float %1984, 1.000000e+00
  %1991 = fadd fast float %1990, %1987
  %1992 = fmul fast float %1918, %1989
  %1993 = fmul fast float %1919, %1991
  %1994 = fadd fast float %1923, %1985
  %1995 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %1992, float %1993, float %1994, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1996 = extractvalue %dx.types.ResRet.f32 %1995, 2
  %1997 = fsub fast float %1996, %1928
  %1998 = fmul fast float %1997, %1930
  %1999 = fmul fast float %1998, %352
  %2000 = fmul fast float %1998, %353
  %2001 = fmul fast float %1998, %354
  %2002 = fadd fast float %1814, %1999
  %2003 = fadd fast float %1815, %2000
  %2004 = fadd fast float %1816, %2001
  %2005 = call float @dx.op.dot3.f32(i32 55, float %2002, float %2003, float %2004, float %2002, float %2003, float %2004)  ; Dot3(ax,ay,az,bx,by,bz)
  %2006 = call float @dx.op.unary.f32(i32 25, float %2005)  ; Rsqrt(value)
  %2007 = fmul fast float %2002, %2006
  %2008 = fmul fast float %2003, %2006
  %2009 = fmul fast float %2004, %2006
  %2010 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float %2007, float %2008, float %2009)  ; Dot3(ax,ay,az,bx,by,bz)
  %2011 = call float @dx.op.unary.f32(i32 7, float %2010)  ; Saturate(value)
  %2012 = fmul fast float %1930, %352
  %2013 = fmul fast float %1930, %353
  %2014 = fmul fast float %1930, %354
  %2015 = fadd fast float %1846, %2012
  %2016 = fadd fast float %1847, %2013
  %2017 = fadd fast float %1848, %2014
  %2018 = call float @dx.op.dot3.f32(i32 55, float %2015, float %2016, float %2017, float %2015, float %2016, float %2017)  ; Dot3(ax,ay,az,bx,by,bz)
  %2019 = call float @dx.op.unary.f32(i32 25, float %2018)  ; Rsqrt(value)
  %2020 = fmul fast float %2015, %2019
  %2021 = fmul fast float %2016, %2019
  %2022 = fmul fast float %2017, %2019
  %2023 = fadd fast float %2020, %352
  %2024 = fadd fast float %2021, %353
  %2025 = fadd fast float %2022, %354
  %2026 = fmul fast float %2023, 5.000000e-01
  %2027 = fmul fast float %2024, 5.000000e-01
  %2028 = fmul fast float %2025, 5.000000e-01
  %2029 = mul nsw i32 %1790, 9
  %2030 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2029, i32 undef)  ; BufferLoad(srv,index,wot)
  %2031 = extractvalue %dx.types.ResRet.f32 %2030, 0
  %2032 = add i32 %2029, 1
  %2033 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2032, i32 undef)  ; BufferLoad(srv,index,wot)
  %2034 = extractvalue %dx.types.ResRet.f32 %2033, 0
  %2035 = add i32 %2029, 2
  %2036 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2035, i32 undef)  ; BufferLoad(srv,index,wot)
  %2037 = extractvalue %dx.types.ResRet.f32 %2036, 0
  %2038 = add i32 %2029, 3
  %2039 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2038, i32 undef)  ; BufferLoad(srv,index,wot)
  %2040 = extractvalue %dx.types.ResRet.f32 %2039, 0
  %2041 = add i32 %2029, 4
  %2042 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2041, i32 undef)  ; BufferLoad(srv,index,wot)
  %2043 = extractvalue %dx.types.ResRet.f32 %2042, 0
  %2044 = add i32 %2029, 5
  %2045 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2044, i32 undef)  ; BufferLoad(srv,index,wot)
  %2046 = extractvalue %dx.types.ResRet.f32 %2045, 0
  %2047 = add i32 %2029, 6
  %2048 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2047, i32 undef)  ; BufferLoad(srv,index,wot)
  %2049 = extractvalue %dx.types.ResRet.f32 %2048, 0
  %2050 = add i32 %2029, 7
  %2051 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2050, i32 undef)  ; BufferLoad(srv,index,wot)
  %2052 = extractvalue %dx.types.ResRet.f32 %2051, 0
  %2053 = add i32 %2029, 8
  %2054 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %1, i32 %2053, i32 undef)  ; BufferLoad(srv,index,wot)
  %2055 = extractvalue %dx.types.ResRet.f32 %2054, 0
  %2056 = fmul fast float %2031, 0x3FD20DD820000000
  %2057 = fmul fast float %2034, %2024
  %2058 = fmul fast float %2037, %2025
  %2059 = fmul fast float %2040, %2023
  %2060 = fmul fast float %2027, %2023
  %2061 = fmul fast float %2060, %2043
  %2062 = fmul fast float %2028, %2024
  %2063 = fmul fast float %2062, %2046
  %2064 = fmul fast float %2025, 1.500000e+00
  %2065 = fmul fast float %2064, %2028
  %2066 = fadd fast float %2065, -1.000000e+00
  %2067 = fmul fast float %2066, 0x3FB42F61E0000000
  %2068 = fmul fast float %2067, %2049
  %2069 = fmul fast float %2026, %2025
  %2070 = fmul fast float %2069, %2052
  %2071 = fmul fast float %2026, %2026
  %2072 = fmul fast float %2027, %2027
  %2073 = fsub fast float %2071, %2072
  %2074 = fmul fast float %2055, %2073
  %2075 = fsub fast float %2061, %2063
  %2076 = fsub fast float %2075, %2070
  %2077 = fadd fast float %2076, %2074
  %2078 = fmul fast float %2077, 0x3FC17B13A0000000
  %2079 = fsub fast float %2058, %2057
  %2080 = fsub fast float %2079, %2059
  %2081 = fmul fast float %2080, 0x3FC4D8D900000000
  %2082 = fadd fast float %2068, %2056
  %2083 = fadd fast float %2082, %2078
  %2084 = fadd fast float %2083, %2081
  %2085 = call float @dx.op.unary.f32(i32 7, float %2084)  ; Saturate(value)
  %2086 = fsub fast float 1.000000e+00, %2085
  %2087 = fmul fast float %2086, %2011
  %2088 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float 0.000000e+00, float -1.000000e+00, float 0.000000e+00)  ; Dot3(ax,ay,az,bx,by,bz)
  %2089 = call float @dx.op.unary.f32(i32 7, float %2088)  ; Saturate(value)
  %2090 = call float @dx.op.binary.f32(i32 35, float %1946, float %2089)  ; FMax(a,b)
  %2091 = call float @dx.op.binary.f32(i32 35, float %2087, float %2090)  ; FMax(a,b)
  %2092 = fmul fast float %1845, %1813
  %2093 = fmul fast float %2092, %2091
  %2094 = fadd fast float %2093, %1791
  %2095 = fadd fast float %1845, %1792
  br label %2096

; <label>:2096                                    ; preds = %1942, %1789
  %2097 = phi float [ %1791, %1789 ], [ %2094, %1942 ]
  %2098 = phi float [ %1792, %1789 ], [ %2095, %1942 ]
  %2099 = add nuw nsw i32 %1793, 1
  %2100 = icmp eq i32 %2099, 8
  br i1 %2100, label %2104, label %2101

; <label>:2101                                    ; preds = %2096
  %2102 = getelementptr inbounds [8 x i32], [8 x i32]* %42, i32 0, i32 %2099
  %2103 = load i32, i32* %2102, align 4, !tbaa !34
  br label %1789

; <label>:2104                                    ; preds = %2096
  %2105 = fcmp fast ogt float %2098, 0.000000e+00
  %2106 = fdiv fast float %2097, %2098
  %2107 = select i1 %2105, float %2106, float 0.000000e+00
  %2108 = call float @dx.op.unary.f32(i32 7, float %2107)  ; Saturate(value)
  %2109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2110 = extractvalue %dx.types.CBufRet.f32 %2109, 0
  %2111 = fsub fast float %375, %2110
  %2112 = extractvalue %dx.types.CBufRet.f32 %2109, 1
  %2113 = fdiv fast float %2111, %2112
  %2114 = call float @dx.op.unary.f32(i32 7, float %2113)  ; Saturate(value)
  %2115 = fcmp fast olt float %2114, 1.000000e+00
  br i1 %2115, label %2116, label %4981

; <label>:2116                                    ; preds = %2104
  %2117 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 0
  store i32 %1732, i32* %2117, align 4
  %2118 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 1
  store i32 %1733, i32* %2118, align 4
  %2119 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 2
  store i32 %1734, i32* %2119, align 4
  %2120 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 3
  store i32 %1735, i32* %2120, align 4
  %2121 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 4
  store i32 %1736, i32* %2121, align 4
  %2122 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 5
  store i32 %1737, i32* %2122, align 4
  %2123 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 6
  store i32 %1738, i32* %2123, align 4
  %2124 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 7
  store i32 %1739, i32* %2124, align 4
  %2125 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2126 = extractvalue %dx.types.CBufRet.f32 %2125, 2
  br label %2127

; <label>:2127                                    ; preds = %2744, %2116
  %2128 = phi i32 [ %1732, %2116 ], [ %2746, %2744 ]
  %2129 = phi float [ 0.000000e+00, %2116 ], [ %2735, %2744 ]
  %2130 = phi float [ 0.000000e+00, %2116 ], [ %2736, %2744 ]
  %2131 = phi float [ 0.000000e+00, %2116 ], [ %2737, %2744 ]
  %2132 = phi i32 [ -1, %2116 ], [ %2738, %2744 ]
  %2133 = phi float [ 0.000000e+00, %2116 ], [ %2739, %2744 ]
  %2134 = phi float [ 0.000000e+00, %2116 ], [ %2740, %2744 ]
  %2135 = phi i32 [ 0, %2116 ], [ %2741, %2744 ]
  %2136 = phi i32 [ 0, %2116 ], [ %2742, %2744 ]
  %2137 = phi float [ %372, %2116 ], [ %2147, %2744 ]
  %2138 = phi float [ %373, %2116 ], [ %2148, %2744 ]
  %2139 = phi float [ %374, %2116 ], [ %2149, %2744 ]
  %2140 = load float, float addrspace(3)* %54, align 16
  %2141 = fmul fast float %352, 0x3FE99999A0000000
  %2142 = fmul fast float %2141, %2140
  %2143 = fmul fast float %353, 0x3FE99999A0000000
  %2144 = fmul fast float %2143, %2140
  %2145 = fmul fast float %354, 0x3FE99999A0000000
  %2146 = fmul fast float %2145, %2140
  %2147 = fadd fast float %2142, %2137
  %2148 = fadd fast float %2144, %2138
  %2149 = fadd fast float %2146, %2139
  store float 0.000000e+00, float addrspace(3)* %54, align 16, !noalias !48
  %2150 = icmp sgt i32 %2128, -1
  br i1 %2150, label %2151, label %2734

; <label>:2151                                    ; preds = %2127
  %2152 = mul i32 %2128, 10
  %2153 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2152, i32 undef)  ; BufferLoad(srv,index,wot)
  %2154 = extractvalue %dx.types.ResRet.f32 %2153, 0
  %2155 = or i32 %2152, 1
  %2156 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2155, i32 undef)  ; BufferLoad(srv,index,wot)
  %2157 = extractvalue %dx.types.ResRet.f32 %2156, 0
  %2158 = add i32 %2152, 2
  %2159 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2158, i32 undef)  ; BufferLoad(srv,index,wot)
  %2160 = extractvalue %dx.types.ResRet.f32 %2159, 0
  %2161 = add i32 %2152, 9
  %2162 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2161, i32 undef)  ; BufferLoad(srv,index,wot)
  %2163 = extractvalue %dx.types.ResRet.f32 %2162, 0
  %2164 = fptoui float %2163 to i32
  %2165 = fsub fast float %2147, %2154
  %2166 = fsub fast float %2148, %2157
  %2167 = fsub fast float %2149, %2160
  %2168 = call float @dx.op.dot3.f32(i32 55, float %2165, float %2166, float %2167, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %2169 = call float @dx.op.dot3.f32(i32 55, float %2165, float %2166, float %2167, float %2165, float %2166, float %2167)  ; Dot3(ax,ay,az,bx,by,bz)
  %2170 = fmul fast float %2126, %2126
  %2171 = fsub fast float %2169, %2170
  %2172 = fmul fast float %2168, %2168
  %2173 = fmul fast float %2171, 4.000000e+00
  %2174 = fsub fast float %2172, %2173
  %2175 = call float @dx.op.unary.f32(i32 24, float %2174)  ; Sqrt(value)
  %2176 = fsub fast float -0.000000e+00, %2168
  %2177 = fsub fast float %2176, %2175
  %2178 = fsub fast float %2175, %2168
  %2179 = call float @dx.op.binary.f32(i32 35, float %2178, float %2177)  ; FMax(a,b)
  %2180 = fmul fast float %2179, 5.000000e-01
  %2181 = fcmp fast ogt float %2180, 0.000000e+00
  br i1 %2181, label %2182, label %2734

; <label>:2182                                    ; preds = %2151
  %2183 = add i32 %2152, 8
  %2184 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2183, i32 undef)  ; BufferLoad(srv,index,wot)
  %2185 = extractvalue %dx.types.ResRet.f32 %2184, 0
  %2186 = fptoui float %2185 to i32
  %2187 = add i32 %2152, 7
  %2188 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2187, i32 undef)  ; BufferLoad(srv,index,wot)
  %2189 = extractvalue %dx.types.ResRet.f32 %2188, 0
  %2190 = fptoui float %2189 to i32
  %2191 = add i32 %2152, 6
  %2192 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2191, i32 undef)  ; BufferLoad(srv,index,wot)
  %2193 = extractvalue %dx.types.ResRet.f32 %2192, 0
  %2194 = fptoui float %2193 to i32
  %2195 = mul i32 %2164, %2194
  %2196 = mul i32 %2164, %2190
  %2197 = call float @dx.op.unary.f32(i32 25, float %2169)  ; Rsqrt(value)
  %2198 = fmul fast float %2197, %2165
  %2199 = fmul fast float %2197, %2166
  %2200 = fmul fast float %2197, %2167
  %2201 = call float @dx.op.dot3.f32(i32 55, float %2198, float %2199, float %2200, float %2165, float %2166, float %2167)  ; Dot3(ax,ay,az,bx,by,bz)
  %2202 = call float @dx.op.unary.f32(i32 6, float %2198)  ; FAbs(value)
  %2203 = call float @dx.op.unary.f32(i32 6, float %2199)  ; FAbs(value)
  %2204 = fadd fast float %2203, %2202
  %2205 = call float @dx.op.unary.f32(i32 6, float %2200)  ; FAbs(value)
  %2206 = fadd fast float %2204, %2205
  %2207 = fdiv fast float 1.000000e+00, %2206
  %2208 = fmul fast float %2207, %2198
  %2209 = fmul fast float %2207, %2199
  %2210 = fcmp fast ole float %2200, 0.000000e+00
  %2211 = call float @dx.op.unary.f32(i32 6, float %2209)  ; FAbs(value)
  %2212 = call float @dx.op.unary.f32(i32 6, float %2208)  ; FAbs(value)
  %2213 = fsub fast float 1.000000e+00, %2211
  %2214 = fsub fast float 1.000000e+00, %2212
  %2215 = fcmp fast oge float %2208, 0.000000e+00
  %2216 = select i1 %2215, float 1.000000e+00, float -1.000000e+00
  %2217 = fcmp fast oge float %2209, 0.000000e+00
  %2218 = select i1 %2217, float 1.000000e+00, float -1.000000e+00
  %2219 = fmul fast float %2216, %2213
  %2220 = fmul fast float %2214, %2218
  %2221 = select i1 %2210, float %2219, float %2208
  %2222 = select i1 %2210, float %2220, float %2209
  %2223 = fmul fast float %2221, 5.000000e-01
  %2224 = fmul fast float %2222, 5.000000e-01
  %2225 = fadd fast float %2223, 5.000000e-01
  %2226 = fadd fast float %2224, 5.000000e-01
  %2227 = sitofp i32 %2195 to float
  %2228 = sitofp i32 %2196 to float
  %2229 = sitofp i32 %2186 to float
  %2230 = sitofp i32 %2164 to float
  %2231 = fadd fast float %2230, -2.000000e+00
  %2232 = fmul fast float %2225, %2231
  %2233 = fmul fast float %2226, %2231
  %2234 = fadd fast float %2227, 1.000000e+00
  %2235 = fadd fast float %2234, %2232
  %2236 = fadd fast float %2228, 1.000000e+00
  %2237 = fadd fast float %2236, %2233
  %2238 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2239 = extractvalue %dx.types.CBufRet.f32 %2238, 0
  %2240 = extractvalue %dx.types.CBufRet.f32 %2238, 1
  %2241 = extractvalue %dx.types.CBufRet.f32 %2238, 3
  %2242 = fmul fast float %2241, %2239
  %2243 = fmul fast float %2241, %2240
  %2244 = fmul fast float %2242, %2235
  %2245 = fmul fast float %2243, %2237
  %2246 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %2247 = extractvalue %dx.types.CBufRet.f32 %2246, 0
  %2248 = fadd fast float %2247, %2229
  %2249 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %2244, float %2245, float %2248, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2250 = extractvalue %dx.types.ResRet.f32 %2249, 2
  %2251 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %2252 = extractvalue %dx.types.CBufRet.f32 %2251, 1
  %2253 = fsub fast float %2250, %2252
  %2254 = extractvalue %dx.types.CBufRet.f32 %2251, 0
  %2255 = fmul fast float %2253, %2254
  %2256 = fcmp fast olt float %2201, %2255
  br i1 %2256, label %2257, label %2718

; <label>:2257                                    ; preds = %2182
  %2258 = fsub fast float -0.000000e+00, %2165
  %2259 = fsub fast float -0.000000e+00, %2166
  %2260 = fsub fast float -0.000000e+00, %2167
  %2261 = fdiv fast float %2258, %352
  %2262 = fdiv fast float %2259, %353
  %2263 = fdiv fast float %2260, %354
  %2264 = call float @dx.op.binary.f32(i32 35, float %2261, float 0.000000e+00)  ; FMax(a,b)
  %2265 = call float @dx.op.binary.f32(i32 35, float %2262, float 0.000000e+00)  ; FMax(a,b)
  %2266 = call float @dx.op.binary.f32(i32 35, float %2263, float 0.000000e+00)  ; FMax(a,b)
  %2267 = call float @dx.op.binary.f32(i32 36, float %2264, float %2180)  ; FMin(a,b)
  %2268 = call float @dx.op.binary.f32(i32 36, float %2265, float %2180)  ; FMin(a,b)
  %2269 = call float @dx.op.binary.f32(i32 36, float %2266, float %2180)  ; FMin(a,b)
  %2270 = call float @dx.op.binary.f32(i32 36, float %2267, float %2268)  ; FMin(a,b)
  %2271 = call float @dx.op.binary.f32(i32 35, float %2267, float %2268)  ; FMax(a,b)
  %2272 = call float @dx.op.binary.f32(i32 36, float %2271, float %2269)  ; FMin(a,b)
  %2273 = call float @dx.op.binary.f32(i32 35, float %2271, float %2269)  ; FMax(a,b)
  %2274 = call float @dx.op.binary.f32(i32 36, float %2270, float %2272)  ; FMin(a,b)
  %2275 = call float @dx.op.binary.f32(i32 35, float %2270, float %2272)  ; FMax(a,b)
  %2276 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 0
  store float 0.000000e+00, float* %2276, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2277 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 1
  store float %2274, float* %2277, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2278 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 2
  store float %2274, float* %2278, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2279 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 3
  store float %2275, float* %2279, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2280 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 4
  store float %2275, float* %2280, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2281 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 5
  store float %2273, float* %2281, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2282 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 6
  store float %2273, float* %2282, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  %2283 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 7
  store float %2180, float* %2283, align 4, !tbaa !53, !alias.scope !55, !noalias !58
  br label %2284

; <label>:2284                                    ; preds = %2701, %2257
  %2285 = phi float [ %2129, %2257 ], [ %2702, %2701 ]
  %2286 = phi float [ %2130, %2257 ], [ %2703, %2701 ]
  %2287 = phi float [ %2131, %2257 ], [ %2704, %2701 ]
  %2288 = phi i32 [ %2132, %2257 ], [ %2705, %2701 ]
  %2289 = phi float [ %2133, %2257 ], [ %2706, %2701 ]
  %2290 = phi float [ %2134, %2257 ], [ %2707, %2701 ]
  %2291 = phi i32 [ 0, %2257 ], [ %2708, %2701 ]
  %2292 = shl i32 %2291, 1
  %2293 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 %2292
  %2294 = load float, float* %2293, align 4, !tbaa !53, !noalias !58
  %2295 = or i32 %2292, 1
  %2296 = getelementptr inbounds [8 x float], [8 x float]* %33, i32 0, i32 %2295
  %2297 = load float, float* %2296, align 4, !tbaa !53, !noalias !58
  %2298 = fsub fast float %2297, %2294
  %2299 = fcmp fast ult float %2298, 0x3F50624DE0000000
  br i1 %2299, label %2701, label %2300

; <label>:2300                                    ; preds = %2284
  %2301 = fmul fast float %2294, %352
  %2302 = fmul fast float %2294, %353
  %2303 = fmul fast float %2294, %354
  %2304 = fadd fast float %2301, %2165
  %2305 = fadd fast float %2302, %2166
  %2306 = fadd fast float %2303, %2167
  %2307 = fmul fast float %2297, %352
  %2308 = fmul fast float %2297, %353
  %2309 = fmul fast float %2297, %354
  %2310 = fadd fast float %2307, %2165
  %2311 = fadd fast float %2308, %2166
  %2312 = fadd fast float %2309, %2167
  %2313 = call float @dx.op.dot3.f32(i32 55, float %2304, float %2305, float %2306, float %2304, float %2305, float %2306)  ; Dot3(ax,ay,az,bx,by,bz)
  %2314 = call float @dx.op.unary.f32(i32 25, float %2313)  ; Rsqrt(value)
  %2315 = fmul fast float %2314, %2304
  %2316 = fmul fast float %2314, %2305
  %2317 = fmul fast float %2314, %2306
  %2318 = call float @dx.op.dot3.f32(i32 55, float %2310, float %2311, float %2312, float %2310, float %2311, float %2312)  ; Dot3(ax,ay,az,bx,by,bz)
  %2319 = call float @dx.op.unary.f32(i32 25, float %2318)  ; Rsqrt(value)
  %2320 = fmul fast float %2319, %2310
  %2321 = fmul fast float %2319, %2311
  %2322 = fmul fast float %2319, %2312
  %2323 = call float @dx.op.unary.f32(i32 6, float %2315)  ; FAbs(value)
  %2324 = call float @dx.op.unary.f32(i32 6, float %2316)  ; FAbs(value)
  %2325 = fadd fast float %2324, %2323
  %2326 = call float @dx.op.unary.f32(i32 6, float %2317)  ; FAbs(value)
  %2327 = fadd fast float %2325, %2326
  %2328 = fdiv fast float 1.000000e+00, %2327
  %2329 = fmul fast float %2328, %2315
  %2330 = fmul fast float %2328, %2316
  %2331 = fcmp fast ole float %2317, 0.000000e+00
  %2332 = call float @dx.op.unary.f32(i32 6, float %2330)  ; FAbs(value)
  %2333 = call float @dx.op.unary.f32(i32 6, float %2329)  ; FAbs(value)
  %2334 = fsub fast float 1.000000e+00, %2332
  %2335 = fsub fast float 1.000000e+00, %2333
  %2336 = fcmp fast oge float %2329, 0.000000e+00
  %2337 = select i1 %2336, float 1.000000e+00, float -1.000000e+00
  %2338 = fcmp fast oge float %2330, 0.000000e+00
  %2339 = select i1 %2338, float 1.000000e+00, float -1.000000e+00
  %2340 = fmul fast float %2337, %2334
  %2341 = fmul fast float %2335, %2339
  %2342 = select i1 %2331, float %2340, float %2329
  %2343 = select i1 %2331, float %2341, float %2330
  %2344 = call float @dx.op.unary.f32(i32 6, float %2320)  ; FAbs(value)
  %2345 = call float @dx.op.unary.f32(i32 6, float %2321)  ; FAbs(value)
  %2346 = fadd fast float %2345, %2344
  %2347 = call float @dx.op.unary.f32(i32 6, float %2322)  ; FAbs(value)
  %2348 = fadd fast float %2346, %2347
  %2349 = fdiv fast float 1.000000e+00, %2348
  %2350 = fmul fast float %2349, %2320
  %2351 = fmul fast float %2349, %2321
  %2352 = fcmp fast ole float %2322, 0.000000e+00
  %2353 = call float @dx.op.unary.f32(i32 6, float %2351)  ; FAbs(value)
  %2354 = call float @dx.op.unary.f32(i32 6, float %2350)  ; FAbs(value)
  %2355 = fsub fast float 1.000000e+00, %2353
  %2356 = fsub fast float 1.000000e+00, %2354
  %2357 = fcmp fast oge float %2350, 0.000000e+00
  %2358 = select i1 %2357, float 1.000000e+00, float -1.000000e+00
  %2359 = fcmp fast oge float %2351, 0.000000e+00
  %2360 = select i1 %2359, float 1.000000e+00, float -1.000000e+00
  %2361 = fmul fast float %2358, %2355
  %2362 = fmul fast float %2356, %2360
  %2363 = select i1 %2352, float %2361, float %2350
  %2364 = select i1 %2352, float %2362, float %2351
  %2365 = fmul fast float %352, 5.000000e-01
  %2366 = fmul fast float %2365, %2298
  %2367 = fmul fast float %353, 5.000000e-01
  %2368 = fmul fast float %2367, %2298
  %2369 = fmul fast float %354, 5.000000e-01
  %2370 = fmul fast float %2369, %2298
  %2371 = fadd fast float %2366, %2304
  %2372 = fadd fast float %2368, %2305
  %2373 = fadd fast float %2370, %2306
  %2374 = call float @dx.op.dot3.f32(i32 55, float %2371, float %2372, float %2373, float %2371, float %2372, float %2373)  ; Dot3(ax,ay,az,bx,by,bz)
  %2375 = call float @dx.op.unary.f32(i32 25, float %2374)  ; Rsqrt(value)
  %2376 = fmul fast float %2375, %2371
  %2377 = fmul fast float %2375, %2372
  %2378 = fmul fast float %2375, %2373
  %2379 = call float @dx.op.unary.f32(i32 6, float %2376)  ; FAbs(value)
  %2380 = call float @dx.op.unary.f32(i32 6, float %2377)  ; FAbs(value)
  %2381 = fadd fast float %2380, %2379
  %2382 = call float @dx.op.unary.f32(i32 6, float %2378)  ; FAbs(value)
  %2383 = fadd fast float %2381, %2382
  %2384 = fdiv fast float 1.000000e+00, %2383
  %2385 = fmul fast float %2384, %2376
  %2386 = fmul fast float %2384, %2377
  %2387 = fcmp fast ole float %2378, 0.000000e+00
  %2388 = call float @dx.op.unary.f32(i32 6, float %2386)  ; FAbs(value)
  %2389 = call float @dx.op.unary.f32(i32 6, float %2385)  ; FAbs(value)
  %2390 = fsub fast float 1.000000e+00, %2388
  %2391 = fsub fast float 1.000000e+00, %2389
  %2392 = fcmp fast oge float %2385, 0.000000e+00
  %2393 = select i1 %2392, float 1.000000e+00, float -1.000000e+00
  %2394 = fcmp fast oge float %2386, 0.000000e+00
  %2395 = select i1 %2394, float 1.000000e+00, float -1.000000e+00
  %2396 = fmul fast float %2393, %2390
  %2397 = fmul fast float %2391, %2395
  %2398 = select i1 %2387, float %2396, float %2385
  %2399 = select i1 %2387, float %2397, float %2386
  %2400 = fmul fast float %2398, %2342
  %2401 = fmul fast float %2399, %2343
  %2402 = fcmp fast oge float %2400, 0.000000e+00
  %2403 = select i1 %2402, float 1.000000e+00, float -1.000000e+00
  %2404 = fcmp fast oge float %2401, 0.000000e+00
  %2405 = select i1 %2404, float 1.000000e+00, float -1.000000e+00
  %2406 = fmul fast float %2398, %2363
  %2407 = fmul fast float %2399, %2364
  %2408 = fcmp fast oge float %2406, 0.000000e+00
  %2409 = select i1 %2408, float 1.000000e+00, float -1.000000e+00
  %2410 = fcmp fast oge float %2407, 0.000000e+00
  %2411 = select i1 %2410, float 1.000000e+00, float -1.000000e+00
  %2412 = fmul fast float %2342, 5.000000e-01
  %2413 = fmul fast float %2412, %2403
  %2414 = fmul fast float %2343, 5.000000e-01
  %2415 = fmul fast float %2414, %2405
  %2416 = fadd fast float %2413, 5.000000e-01
  %2417 = fadd fast float %2415, 5.000000e-01
  %2418 = fmul fast float %2363, 5.000000e-01
  %2419 = fmul fast float %2418, %2409
  %2420 = fmul fast float %2364, 5.000000e-01
  %2421 = fmul fast float %2420, %2411
  %2422 = fadd fast float %2419, 5.000000e-01
  %2423 = fadd fast float %2421, 5.000000e-01
  %2424 = fsub fast float %2422, %2416
  %2425 = fsub fast float %2423, %2417
  %2426 = fmul fast float %2424, %2424
  %2427 = fmul fast float %2425, %2425
  %2428 = fadd fast float %2426, %2427
  %2429 = call float @dx.op.unary.f32(i32 24, float %2428)  ; Sqrt(value)
  %2430 = fdiv fast float %2424, %2429
  %2431 = fdiv fast float %2425, %2429
  %2432 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %2433 = extractvalue %dx.types.CBufRet.f32 %2432, 1
  %2434 = fsub fast float 1.000000e+00, %2433
  %2435 = extractvalue %dx.types.CBufRet.f32 %2432, 0
  %2436 = fmul fast float %2434, %2435
  %2437 = call float @dx.op.unary.f32(i32 6, float %2424)  ; FAbs(value)
  %2438 = call float @dx.op.unary.f32(i32 6, float %2425)  ; FAbs(value)
  %2439 = call float @dx.op.binary.f32(i32 35, float %2437, float %2438)  ; FMax(a,b)
  %2440 = fmul fast float %2439, %2230
  %2441 = fptosi float %2440 to i32
  %2442 = add nsw i32 %2441, 1
  %2443 = sitofp i32 %2442 to float
  %2444 = fdiv fast float %2429, %2443
  %2445 = fmul fast float %2304, %2304
  %2446 = fmul fast float %2305, %2305
  %2447 = fadd fast float %2445, %2446
  %2448 = fmul fast float %2306, %2306
  %2449 = fadd fast float %2447, %2448
  %2450 = call float @dx.op.unary.f32(i32 24, float %2449)  ; Sqrt(value)
  %2451 = call float @dx.op.binary.f32(i32 36, float %2444, float %2429)  ; FMin(a,b)
  %2452 = fmul fast float %2451, %2430
  %2453 = fmul fast float %2451, %2431
  %2454 = fadd fast float %2452, %2416
  %2455 = fadd fast float %2453, %2417
  %2456 = call float @dx.op.unary.f32(i32 7, float %2454)  ; Saturate(value)
  %2457 = call float @dx.op.unary.f32(i32 7, float %2455)  ; Saturate(value)
  %2458 = fmul fast float %2456, 2.000000e+00
  %2459 = fmul fast float %2457, 2.000000e+00
  %2460 = fadd fast float %2458, -1.000000e+00
  %2461 = fadd fast float %2459, -1.000000e+00
  %2462 = call float @dx.op.unary.f32(i32 6, float %2460)  ; FAbs(value)
  %2463 = fsub fast float 1.000000e+00, %2462
  %2464 = call float @dx.op.unary.f32(i32 6, float %2461)  ; FAbs(value)
  %2465 = fsub fast float %2463, %2464
  %2466 = fcmp fast olt float %2465, 0.000000e+00
  br i1 %2466, label %2467, label %2475

; <label>:2467                                    ; preds = %2300
  %2468 = fsub fast float 1.000000e+00, %2464
  %2469 = fcmp fast oge float %2460, 0.000000e+00
  %2470 = select i1 %2469, float 1.000000e+00, float -1.000000e+00
  %2471 = fcmp fast oge float %2461, 0.000000e+00
  %2472 = select i1 %2471, float 1.000000e+00, float -1.000000e+00
  %2473 = fmul fast float %2468, %2470
  %2474 = fmul fast float %2463, %2472
  br label %2475

; <label>:2475                                    ; preds = %2467, %2300
  %2476 = phi float [ %2473, %2467 ], [ %2460, %2300 ]
  %2477 = phi float [ %2474, %2467 ], [ %2461, %2300 ]
  %2478 = call float @dx.op.dot3.f32(i32 55, float %2476, float %2477, float %2465, float %2476, float %2477, float %2465)  ; Dot3(ax,ay,az,bx,by,bz)
  %2479 = call float @dx.op.unary.f32(i32 25, float %2478)  ; Rsqrt(value)
  %2480 = fmul fast float %2479, %2476
  %2481 = fmul fast float %2479, %2477
  %2482 = fmul fast float %2479, %2465
  %2483 = fmul fast float %2166, %354
  %2484 = fmul fast float %2167, %353
  %2485 = fsub fast float %2483, %2484
  %2486 = fmul fast float %2167, %352
  %2487 = fmul fast float %2165, %354
  %2488 = fsub fast float %2486, %2487
  %2489 = fmul fast float %2165, %353
  %2490 = fmul fast float %2166, %352
  %2491 = fsub fast float %2489, %2490
  %2492 = fmul fast float %2482, %353
  %2493 = fmul fast float %2481, %354
  %2494 = fsub fast float %2492, %2493
  %2495 = fmul fast float %2480, %354
  %2496 = fmul fast float %2482, %352
  %2497 = fsub fast float %2495, %2496
  %2498 = fmul fast float %2481, %352
  %2499 = fmul fast float %2480, %353
  %2500 = fsub fast float %2498, %2499
  %2501 = fmul fast float %2494, %2494
  %2502 = fmul fast float %2497, %2497
  %2503 = fadd fast float %2501, %2502
  %2504 = fmul fast float %2500, %2500
  %2505 = fadd fast float %2503, %2504
  %2506 = call float @dx.op.unary.f32(i32 24, float %2505)  ; Sqrt(value)
  %2507 = fcmp fast olt float %2506, 0x3F1A36E2E0000000
  br i1 %2507, label %2508, label %2513

; <label>:2508                                    ; preds = %2475
  %2509 = call float @dx.op.dot3.f32(i32 55, float %2480, float %2481, float %2482, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %2510 = fcmp fast olt float %2509, 0.000000e+00
  %2511 = select i1 %2510, float 0.000000e+00, float 1.000000e+00
  %2512 = fmul fast float %2511, %2435
  br label %2521

; <label>:2513                                    ; preds = %2475
  %2514 = fmul fast float %2485, %2485
  %2515 = fmul fast float %2488, %2488
  %2516 = fmul fast float %2491, %2491
  %2517 = fadd fast float %2515, %2516
  %2518 = fadd fast float %2517, %2514
  %2519 = call float @dx.op.unary.f32(i32 24, float %2518)  ; Sqrt(value)
  %2520 = fdiv fast float %2519, %2506
  br label %2521

; <label>:2521                                    ; preds = %2513, %2508
  %2522 = phi float [ %2512, %2508 ], [ %2520, %2513 ]
  %2523 = icmp sgt i32 %2441, -1
  br i1 %2523, label %2524, label %2692

; <label>:2524                                    ; preds = %2521
  %2525 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2522)  ; FMax(a,b)
  br label %2526

; <label>:2526                                    ; preds = %2682, %2524
  %2527 = phi float [ %2631, %2682 ], [ %2450, %2524 ]
  %2528 = phi float [ %2688, %2682 ], [ %2294, %2524 ]
  %2529 = phi float [ %2564, %2682 ], [ %2304, %2524 ]
  %2530 = phi float [ %2565, %2682 ], [ %2305, %2524 ]
  %2531 = phi float [ %2566, %2682 ], [ %2306, %2524 ]
  %2532 = phi float [ %2597, %2682 ], [ %2480, %2524 ]
  %2533 = phi float [ %2598, %2682 ], [ %2481, %2524 ]
  %2534 = phi float [ %2599, %2682 ], [ %2482, %2524 ]
  %2535 = phi float [ %2631, %2682 ], [ %2525, %2524 ]
  %2536 = phi i32 [ %2689, %2682 ], [ 1, %2524 ]
  %2537 = sitofp i32 %2536 to float
  %2538 = fmul fast float %2537, %2444
  %2539 = call float @dx.op.binary.f32(i32 36, float %2538, float %2429)  ; FMin(a,b)
  %2540 = fmul fast float %2539, %2430
  %2541 = fmul fast float %2539, %2431
  %2542 = fadd fast float %2540, %2416
  %2543 = fadd fast float %2541, %2417
  %2544 = call float @dx.op.unary.f32(i32 7, float %2542)  ; Saturate(value)
  %2545 = call float @dx.op.unary.f32(i32 7, float %2543)  ; Saturate(value)
  %2546 = fmul fast float %2544, %2231
  %2547 = fmul fast float %2545, %2231
  %2548 = fadd fast float %2234, %2546
  %2549 = fadd fast float %2236, %2547
  %2550 = fptosi float %2548 to i32
  %2551 = fptosi float %2549 to i32
  %2552 = fptosi float %2229 to i32
  %2553 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %2554 = extractvalue %dx.types.CBufRet.f32 %2553, 0
  %2555 = fptosi float %2554 to i32
  %2556 = add nsw i32 %2555, %2552
  %2557 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %8, i32 0, i32 %2550, i32 %2551, i32 %2556, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2558 = extractvalue %dx.types.ResRet.f32 %2557, 2
  %2559 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %2560 = extractvalue %dx.types.CBufRet.f32 %2559, 1
  %2561 = fsub fast float %2558, %2560
  %2562 = extractvalue %dx.types.CBufRet.f32 %2559, 0
  %2563 = fmul fast float %2561, %2562
  %2564 = fmul fast float %2535, %2532
  %2565 = fmul fast float %2535, %2533
  %2566 = fmul fast float %2535, %2534
  %2567 = fadd fast float %2538, %2444
  %2568 = call float @dx.op.binary.f32(i32 36, float %2567, float %2429)  ; FMin(a,b)
  %2569 = fmul fast float %2568, %2430
  %2570 = fmul fast float %2568, %2431
  %2571 = fadd fast float %2569, %2416
  %2572 = fadd fast float %2570, %2417
  %2573 = call float @dx.op.unary.f32(i32 7, float %2571)  ; Saturate(value)
  %2574 = call float @dx.op.unary.f32(i32 7, float %2572)  ; Saturate(value)
  %2575 = fmul fast float %2573, 2.000000e+00
  %2576 = fmul fast float %2574, 2.000000e+00
  %2577 = fadd fast float %2575, -1.000000e+00
  %2578 = fadd fast float %2576, -1.000000e+00
  %2579 = call float @dx.op.unary.f32(i32 6, float %2577)  ; FAbs(value)
  %2580 = fsub fast float 1.000000e+00, %2579
  %2581 = call float @dx.op.unary.f32(i32 6, float %2578)  ; FAbs(value)
  %2582 = fsub fast float %2580, %2581
  %2583 = fcmp fast olt float %2582, 0.000000e+00
  br i1 %2583, label %2584, label %2592

; <label>:2584                                    ; preds = %2526
  %2585 = fsub fast float 1.000000e+00, %2581
  %2586 = fcmp fast oge float %2577, 0.000000e+00
  %2587 = select i1 %2586, float 1.000000e+00, float -1.000000e+00
  %2588 = fcmp fast oge float %2578, 0.000000e+00
  %2589 = select i1 %2588, float 1.000000e+00, float -1.000000e+00
  %2590 = fmul fast float %2585, %2587
  %2591 = fmul fast float %2580, %2589
  br label %2592

; <label>:2592                                    ; preds = %2584, %2526
  %2593 = phi float [ %2590, %2584 ], [ %2577, %2526 ]
  %2594 = phi float [ %2591, %2584 ], [ %2578, %2526 ]
  %2595 = call float @dx.op.dot3.f32(i32 55, float %2593, float %2594, float %2582, float %2593, float %2594, float %2582)  ; Dot3(ax,ay,az,bx,by,bz)
  %2596 = call float @dx.op.unary.f32(i32 25, float %2595)  ; Rsqrt(value)
  %2597 = fmul fast float %2596, %2593
  %2598 = fmul fast float %2596, %2594
  %2599 = fmul fast float %2596, %2582
  %2600 = fmul fast float %2599, %353
  %2601 = fmul fast float %2598, %354
  %2602 = fsub fast float %2600, %2601
  %2603 = fmul fast float %2597, %354
  %2604 = fmul fast float %2599, %352
  %2605 = fsub fast float %2603, %2604
  %2606 = fmul fast float %2598, %352
  %2607 = fmul fast float %2597, %353
  %2608 = fsub fast float %2606, %2607
  %2609 = fmul fast float %2602, %2602
  %2610 = fmul fast float %2605, %2605
  %2611 = fadd fast float %2609, %2610
  %2612 = fmul fast float %2608, %2608
  %2613 = fadd fast float %2611, %2612
  %2614 = call float @dx.op.unary.f32(i32 24, float %2613)  ; Sqrt(value)
  %2615 = fcmp fast olt float %2614, 0x3F1A36E2E0000000
  br i1 %2615, label %2616, label %2621

; <label>:2616                                    ; preds = %2592
  %2617 = call float @dx.op.dot3.f32(i32 55, float %2597, float %2598, float %2599, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %2618 = fcmp fast olt float %2617, 0.000000e+00
  %2619 = select i1 %2618, float 0.000000e+00, float 1.000000e+00
  %2620 = fmul fast float %2619, %2562
  br label %2629

; <label>:2621                                    ; preds = %2592
  %2622 = fmul fast float %2485, %2485
  %2623 = fmul fast float %2488, %2488
  %2624 = fmul fast float %2491, %2491
  %2625 = fadd fast float %2623, %2624
  %2626 = fadd fast float %2625, %2622
  %2627 = call float @dx.op.unary.f32(i32 24, float %2626)  ; Sqrt(value)
  %2628 = fdiv fast float %2627, %2614
  br label %2629

; <label>:2629                                    ; preds = %2621, %2616
  %2630 = phi float [ %2620, %2616 ], [ %2628, %2621 ]
  %2631 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2630)  ; FMax(a,b)
  %2632 = call float @dx.op.binary.f32(i32 35, float %2527, float %2631)  ; FMax(a,b)
  %2633 = fmul fast float %2631, %2597
  %2634 = fmul fast float %2631, %2598
  %2635 = fmul fast float %2631, %2599
  %2636 = fsub fast float %2633, %2165
  %2637 = fsub fast float %2634, %2166
  %2638 = fsub fast float %2635, %2167
  %2639 = fcmp fast ult float %2632, %2563
  br i1 %2639, label %2682, label %2640

; <label>:2640                                    ; preds = %2629
  %2641 = fcmp fast ugt float %2436, %2563
  br i1 %2641, label %2642, label %2692

; <label>:2642                                    ; preds = %2640
  %2643 = call float @dx.op.binary.f32(i32 36, float %2527, float %2631)  ; FMin(a,b)
  %2644 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %7, i32 0, i32 %2550, i32 %2551, i32 %2556, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2645 = extractvalue %dx.types.ResRet.f32 %2644, 0
  %2646 = extractvalue %dx.types.ResRet.f32 %2644, 1
  %2647 = extractvalue %dx.types.ResRet.f32 %2644, 2
  %2648 = fmul fast float %2645, 2.000000e+00
  %2649 = fmul fast float %2646, 2.000000e+00
  %2650 = fmul fast float %2647, 2.000000e+00
  %2651 = fadd fast float %2648, -1.000000e+00
  %2652 = fadd fast float %2649, -1.000000e+00
  %2653 = fadd fast float %2650, -1.000000e+00
  %2654 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %2655 = extractvalue %dx.types.CBufRet.f32 %2654, 1
  %2656 = fmul fast float %2562, %2655
  %2657 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float %2532, float %2533, float %2534)  ; Dot3(ax,ay,az,bx,by,bz)
  %2658 = call float @dx.op.unary.f32(i32 6, float %2657)  ; FAbs(value)
  %2659 = call float @dx.op.dot3.f32(i32 55, float %2651, float %2652, float %2653, float %2532, float %2533, float %2534)  ; Dot3(ax,ay,az,bx,by,bz)
  %2660 = call float @dx.op.unary.f32(i32 6, float %2659)  ; FAbs(value)
  %2661 = fsub fast float 1.000000e+00, %2660
  %2662 = fmul fast float %2656, %2658
  %2663 = fmul fast float %2662, %2661
  %2664 = call float @dx.op.dot3.f32(i32 55, float %2651, float %2652, float %2653, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %2665 = fadd fast float %2663, %2563
  %2666 = fcmp fast olt float %2643, %2665
  %2667 = fcmp olt float %2664, -0.000000e+00
  %2668 = and i1 %2667, %2666
  br i1 %2668, label %2692, label %2669

; <label>:2669                                    ; preds = %2642
  %2670 = call float @dx.op.binary.f32(i32 36, float %2528, float %2297)  ; FMin(a,b)
  store float %2670, float addrspace(3)* %54, align 16, !noalias !61
  %2671 = fsub fast float %2563, %2643
  %2672 = fadd fast float %2671, %2663
  %2673 = call float @dx.op.unary.f32(i32 6, float %2672)  ; FAbs(value)
  %2674 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2675 = extractvalue %dx.types.CBufRet.f32 %2674, 0
  %2676 = fmul fast float %2675, %2673
  %2677 = fsub fast float 1.000000e+00, %2676
  %2678 = select i1 %2667, float %2677, float 0.000000e+00
  %2679 = load float, float addrspace(3)* %57, align 4
  %2680 = fcmp fast ogt float %2678, %2679
  br i1 %2680, label %2681, label %2692

; <label>:2681                                    ; preds = %2669
  store float %2678, float addrspace(3)* %57, align 4, !noalias !61
  br label %2692

; <label>:2682                                    ; preds = %2629
  %2683 = fmul fast float %2636, %2636
  %2684 = fmul fast float %2637, %2637
  %2685 = fadd fast float %2683, %2684
  %2686 = fmul fast float %2638, %2638
  %2687 = fadd fast float %2685, %2686
  %2688 = call float @dx.op.unary.f32(i32 24, float %2687)  ; Sqrt(value)
  %2689 = add nuw nsw i32 %2536, 1
  %2690 = icmp slt i32 %2536, %2442
  br i1 %2690, label %2526, label %2691

; <label>:2691                                    ; preds = %2682
  br label %2692

; <label>:2692                                    ; preds = %2691, %2681, %2669, %2642, %2640, %2521
  %2693 = phi i32 [ 0, %2640 ], [ 1, %2642 ], [ 2, %2681 ], [ 2, %2669 ], [ 0, %2521 ], [ 0, %2691 ]
  %2694 = phi float [ %2304, %2640 ], [ %2529, %2642 ], [ %2529, %2681 ], [ %2285, %2669 ], [ %2304, %2521 ], [ %2564, %2691 ]
  %2695 = phi float [ %2305, %2640 ], [ %2530, %2642 ], [ %2530, %2681 ], [ %2286, %2669 ], [ %2305, %2521 ], [ %2565, %2691 ]
  %2696 = phi float [ %2306, %2640 ], [ %2531, %2642 ], [ %2531, %2681 ], [ %2287, %2669 ], [ %2306, %2521 ], [ %2566, %2691 ]
  %2697 = phi i32 [ %2128, %2640 ], [ %2128, %2642 ], [ %2128, %2681 ], [ %2288, %2669 ], [ %2128, %2521 ], [ %2128, %2691 ]
  %2698 = phi float [ %2544, %2640 ], [ %2544, %2642 ], [ %2544, %2681 ], [ %2289, %2669 ], [ %2416, %2521 ], [ %2544, %2691 ]
  %2699 = phi float [ %2545, %2640 ], [ %2545, %2642 ], [ %2545, %2681 ], [ %2290, %2669 ], [ %2417, %2521 ], [ %2545, %2691 ]
  %2700 = icmp eq i32 %2693, 0
  br i1 %2700, label %2701, label %2710

; <label>:2701                                    ; preds = %2692, %2284
  %2702 = phi float [ %2694, %2692 ], [ %2285, %2284 ]
  %2703 = phi float [ %2695, %2692 ], [ %2286, %2284 ]
  %2704 = phi float [ %2696, %2692 ], [ %2287, %2284 ]
  %2705 = phi i32 [ %2697, %2692 ], [ %2288, %2284 ]
  %2706 = phi float [ %2698, %2692 ], [ %2289, %2284 ]
  %2707 = phi float [ %2699, %2692 ], [ %2290, %2284 ]
  %2708 = add nuw nsw i32 %2291, 1
  %2709 = icmp ult i32 %2708, 4
  br i1 %2709, label %2284, label %2710

; <label>:2710                                    ; preds = %2701, %2692
  %2711 = phi float [ %2694, %2692 ], [ %2702, %2701 ]
  %2712 = phi float [ %2695, %2692 ], [ %2703, %2701 ]
  %2713 = phi float [ %2696, %2692 ], [ %2704, %2701 ]
  %2714 = phi i32 [ %2697, %2692 ], [ %2705, %2701 ]
  %2715 = phi float [ %2698, %2692 ], [ %2706, %2701 ]
  %2716 = phi float [ %2699, %2692 ], [ %2707, %2701 ]
  %2717 = phi i32 [ %2693, %2692 ], [ 0, %2701 ]
  br label %2718

; <label>:2718                                    ; preds = %2710, %2182
  %2719 = phi float [ %2129, %2182 ], [ %2711, %2710 ]
  %2720 = phi float [ %2130, %2182 ], [ %2712, %2710 ]
  %2721 = phi float [ %2131, %2182 ], [ %2713, %2710 ]
  %2722 = phi i32 [ %2132, %2182 ], [ %2714, %2710 ]
  %2723 = phi float [ %2133, %2182 ], [ %2715, %2710 ]
  %2724 = phi float [ %2134, %2182 ], [ %2716, %2710 ]
  %2725 = phi i32 [ 2, %2182 ], [ %2717, %2710 ]
  %2726 = fadd fast float %2719, %2154
  %2727 = fadd fast float %2720, %2157
  %2728 = fadd fast float %2721, %2160
  %2729 = icmp eq i32 %2725, 2
  br i1 %2729, label %2734, label %2730

; <label>:2730                                    ; preds = %2718
  %2731 = icmp ugt i32 %2136, 7
  %2732 = zext i1 %2731 to i32
  %2733 = add nsw i32 %2732, %1646
  br label %2748

; <label>:2734                                    ; preds = %2718, %2151, %2127
  %2735 = phi float [ %2129, %2151 ], [ %2129, %2127 ], [ %2726, %2718 ]
  %2736 = phi float [ %2130, %2151 ], [ %2130, %2127 ], [ %2727, %2718 ]
  %2737 = phi float [ %2131, %2151 ], [ %2131, %2127 ], [ %2728, %2718 ]
  %2738 = phi i32 [ %2132, %2151 ], [ %2132, %2127 ], [ %2722, %2718 ]
  %2739 = phi float [ %2133, %2151 ], [ %2133, %2127 ], [ %2723, %2718 ]
  %2740 = phi float [ %2134, %2151 ], [ %2134, %2127 ], [ %2724, %2718 ]
  %2741 = phi i32 [ %2135, %2151 ], [ %2135, %2127 ], [ 2, %2718 ]
  %2742 = add nuw nsw i32 %2136, 1
  %2743 = icmp ult i32 %2742, 8
  br i1 %2743, label %2744, label %2747

; <label>:2744                                    ; preds = %2734
  %2745 = getelementptr inbounds [8 x i32], [8 x i32]* %41, i32 0, i32 %2742
  %2746 = load i32, i32* %2745, align 4, !tbaa !34, !noalias !48
  br label %2127

; <label>:2747                                    ; preds = %2734
  br label %2748

; <label>:2748                                    ; preds = %2747, %2730
  %2749 = phi i32 [ %2733, %2730 ], [ %1646, %2747 ]
  %2750 = phi float [ %2726, %2730 ], [ %2735, %2747 ]
  %2751 = phi float [ %2727, %2730 ], [ %2736, %2747 ]
  %2752 = phi float [ %2728, %2730 ], [ %2737, %2747 ]
  %2753 = phi i32 [ %2722, %2730 ], [ %2738, %2747 ]
  %2754 = phi float [ %2723, %2730 ], [ %2739, %2747 ]
  %2755 = phi float [ %2724, %2730 ], [ %2740, %2747 ]
  %2756 = phi i32 [ %2725, %2730 ], [ %2741, %2747 ]
  %2757 = icmp eq i32 %2756, 1
  %2758 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %2759 = extractvalue %dx.types.CBufRet.f32 %2758, 0
  %2760 = extractvalue %dx.types.CBufRet.f32 %2758, 1
  %2761 = extractvalue %dx.types.CBufRet.f32 %2758, 2
  %2762 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %2763 = extractvalue %dx.types.CBufRet.f32 %2762, 0
  %2764 = extractvalue %dx.types.CBufRet.f32 %2762, 1
  %2765 = extractvalue %dx.types.CBufRet.f32 %2762, 2
  %2766 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %2767 = extractvalue %dx.types.CBufRet.f32 %2766, 0
  %2768 = extractvalue %dx.types.CBufRet.f32 %2766, 1
  %2769 = extractvalue %dx.types.CBufRet.f32 %2766, 2
  %2770 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 27)  ; CBufferLoadLegacy(handle,regIndex)
  %2771 = extractvalue %dx.types.CBufRet.f32 %2770, 0
  %2772 = extractvalue %dx.types.CBufRet.f32 %2770, 1
  %2773 = extractvalue %dx.types.CBufRet.f32 %2770, 2
  %2774 = fmul fast float %2759, %372
  %2775 = call float @dx.op.tertiary.f32(i32 46, float %373, float %2763, float %2774)  ; FMad(a,b,c)
  %2776 = call float @dx.op.tertiary.f32(i32 46, float %374, float %2767, float %2775)  ; FMad(a,b,c)
  %2777 = fadd fast float %2776, %2771
  %2778 = fmul fast float %2760, %372
  %2779 = call float @dx.op.tertiary.f32(i32 46, float %373, float %2764, float %2778)  ; FMad(a,b,c)
  %2780 = call float @dx.op.tertiary.f32(i32 46, float %374, float %2768, float %2779)  ; FMad(a,b,c)
  %2781 = fadd fast float %2780, %2772
  %2782 = fmul fast float %2761, %372
  %2783 = call float @dx.op.tertiary.f32(i32 46, float %373, float %2765, float %2782)  ; FMad(a,b,c)
  %2784 = call float @dx.op.tertiary.f32(i32 46, float %374, float %2769, float %2783)  ; FMad(a,b,c)
  %2785 = fadd fast float %2784, %2773
  %2786 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2787 = extractvalue %dx.types.CBufRet.f32 %2786, 3
  %2788 = fdiv fast float %2777, %2787
  %2789 = fdiv fast float %2781, %2787
  %2790 = fdiv fast float %2785, %2787
  %2791 = fsub fast float %2788, %1752
  %2792 = fsub fast float %2789, %1753
  %2793 = fsub fast float %2790, %1754
  br i1 %2757, label %2794, label %3460

; <label>:2794                                    ; preds = %2748
  %2795 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 0
  store i32 %1732, i32* %2795, align 4
  %2796 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 1
  store i32 %1733, i32* %2796, align 4
  %2797 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 2
  store i32 %1734, i32* %2797, align 4
  %2798 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 3
  store i32 %1735, i32* %2798, align 4
  %2799 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 4
  store i32 %1736, i32* %2799, align 4
  %2800 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 5
  store i32 %1737, i32* %2800, align 4
  %2801 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 6
  store i32 %1738, i32* %2801, align 4
  %2802 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 7
  store i32 %1739, i32* %2802, align 4
  %2803 = sitofp i32 %2749 to float
  %2804 = call float @dx.op.unary.f32(i32 21, float %2803)  ; Exp(value)
  %2805 = fptosi float %2804 to i32
  %2806 = sitofp i32 %2805 to float
  %2807 = fdiv fast float %2791, %2806
  %2808 = fdiv fast float %2792, %2806
  %2809 = fdiv fast float %2793, %2806
  %2810 = fcmp fast olt float %2807, 0.000000e+00
  %2811 = fcmp fast ogt float %2807, 1.000000e+00
  %2812 = or i1 %2810, %2811
  br i1 %2812, label %2813, label %2817

; <label>:2813                                    ; preds = %2794
  %2814 = fmul fast float %2807, 2.000000e+00
  %2815 = fadd fast float %2814, -1.000000e+00
  %2816 = fdiv fast float %2807, %2815
  br label %2817

; <label>:2817                                    ; preds = %2813, %2794
  %2818 = phi float [ %2816, %2813 ], [ %2807, %2794 ]
  %2819 = fcmp fast olt float %2808, 0.000000e+00
  %2820 = fcmp fast ogt float %2808, 1.000000e+00
  %2821 = or i1 %2819, %2820
  br i1 %2821, label %2822, label %2826

; <label>:2822                                    ; preds = %2817
  %2823 = fmul fast float %2808, 2.000000e+00
  %2824 = fadd fast float %2823, -1.000000e+00
  %2825 = fdiv fast float %2808, %2824
  br label %2826

; <label>:2826                                    ; preds = %2822, %2817
  %2827 = phi float [ %2825, %2822 ], [ %2808, %2817 ]
  %2828 = fcmp fast olt float %2809, 0.000000e+00
  %2829 = fcmp fast ogt float %2809, 1.000000e+00
  %2830 = or i1 %2828, %2829
  br i1 %2830, label %2831, label %2835

; <label>:2831                                    ; preds = %2826
  %2832 = fmul fast float %2809, 2.000000e+00
  %2833 = fadd fast float %2832, -1.000000e+00
  %2834 = fdiv fast float %2809, %2833
  br label %2835

; <label>:2835                                    ; preds = %2831, %2826
  %2836 = phi float [ %2834, %2831 ], [ %2809, %2826 ]
  %2837 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %2838 = extractvalue %dx.types.CBufRet.f32 %2837, 2
  %2839 = mul nsw i32 %2753, 10
  %2840 = add i32 %2839, 9
  %2841 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2840, i32 undef)  ; BufferLoad(srv,index,wot)
  %2842 = extractvalue %dx.types.ResRet.f32 %2841, 0
  %2843 = fptoui float %2842 to i32
  %2844 = icmp eq i32 %2843, 64
  %2845 = select i1 %2844, float 3.000000e+00, float %2838
  %2846 = fmul fast float %2845, %375
  %2847 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 0
  store float 0.000000e+00, float* %2847, align 4, !tbaa !53
  %2848 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 1
  store float 0.000000e+00, float* %2848, align 4, !tbaa !53
  %2849 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 2
  store float 0.000000e+00, float* %2849, align 4, !tbaa !53
  %2850 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 3
  store float 0.000000e+00, float* %2850, align 4, !tbaa !53
  %2851 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 4
  store float 0.000000e+00, float* %2851, align 4, !tbaa !53
  %2852 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 5
  store float 0.000000e+00, float* %2852, align 4, !tbaa !53
  %2853 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 6
  store float 0.000000e+00, float* %2853, align 4, !tbaa !53
  %2854 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 7
  store float 0.000000e+00, float* %2854, align 4, !tbaa !53
  %2855 = sub nsw i32 %2749, %1646
  br label %2856

; <label>:2856                                    ; preds = %3161, %2835
  %2857 = phi i32 [ %2753, %2835 ], [ %3162, %3161 ]
  %2858 = phi float [ %2754, %2835 ], [ %3163, %3161 ]
  %2859 = phi float [ %2755, %2835 ], [ %3164, %3161 ]
  %2860 = phi float [ %2845, %2835 ], [ %3165, %3161 ]
  %2861 = phi float [ %2846, %2835 ], [ %3166, %3161 ]
  %2862 = phi float [ 0.000000e+00, %2835 ], [ %3167, %3161 ]
  %2863 = phi i32 [ 0, %2835 ], [ %3168, %3161 ]
  %2864 = shl i32 %2855, 3
  %2865 = add i32 %2863, %2864
  %2866 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 %2865
  %2867 = load i32, i32* %2866, align 4, !tbaa !34
  %2868 = icmp slt i32 %2867, 0
  br i1 %2868, label %3161, label %2869

; <label>:2869                                    ; preds = %2856
  %2870 = mul i32 %2867, 10
  %2871 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2870, i32 undef)  ; BufferLoad(srv,index,wot)
  %2872 = extractvalue %dx.types.ResRet.f32 %2871, 0
  %2873 = or i32 %2870, 1
  %2874 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2873, i32 undef)  ; BufferLoad(srv,index,wot)
  %2875 = extractvalue %dx.types.ResRet.f32 %2874, 0
  %2876 = add i32 %2870, 2
  %2877 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2876, i32 undef)  ; BufferLoad(srv,index,wot)
  %2878 = extractvalue %dx.types.ResRet.f32 %2877, 0
  %2879 = fsub fast float %2750, %2872
  %2880 = fsub fast float %2751, %2875
  %2881 = fsub fast float %2752, %2878
  %2882 = call float @dx.op.dot3.f32(i32 55, float %2879, float %2880, float %2881, float %2879, float %2880, float %2881)  ; Dot3(ax,ay,az,bx,by,bz)
  %2883 = call float @dx.op.unary.f32(i32 25, float %2882)  ; Rsqrt(value)
  %2884 = fmul fast float %2883, %2879
  %2885 = fmul fast float %2883, %2880
  %2886 = fmul fast float %2881, %2883
  %2887 = call float @dx.op.dot3.f32(i32 55, float %2884, float %2885, float %2886, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %2888 = fmul fast float %2887, 5.000000e-01
  %2889 = fadd fast float %2888, 5.000000e-01
  %2890 = call float @dx.op.binary.f32(i32 35, float %375, float 0x3F50624DE0000000)  ; FMax(a,b)
  %2891 = fdiv fast float %2889, %2890
  %2892 = call float @dx.op.unary.f32(i32 7, float %2891)  ; Saturate(value)
  %2893 = call float @dx.op.unary.f32(i32 7, float %2888)  ; Saturate(value)
  %2894 = fmul fast float %2893, 2.000000e+00
  %2895 = fmul fast float %2879, %2879
  %2896 = fmul fast float %2880, %2880
  %2897 = fadd fast float %2896, %2895
  %2898 = fmul fast float %2881, %2881
  %2899 = fadd fast float %2897, %2898
  %2900 = call float @dx.op.unary.f32(i32 24, float %2899)  ; Sqrt(value)
  %2901 = call float @dx.op.unary.f32(i32 6, float %2884)  ; FAbs(value)
  %2902 = call float @dx.op.unary.f32(i32 6, float %2885)  ; FAbs(value)
  %2903 = fadd fast float %2902, %2901
  %2904 = call float @dx.op.unary.f32(i32 6, float %2886)  ; FAbs(value)
  %2905 = fadd fast float %2903, %2904
  %2906 = fdiv fast float 1.000000e+00, %2905
  %2907 = fmul fast float %2906, %2884
  %2908 = fmul fast float %2906, %2885
  %2909 = fcmp fast ole float %2886, 0.000000e+00
  %2910 = call float @dx.op.unary.f32(i32 6, float %2908)  ; FAbs(value)
  %2911 = call float @dx.op.unary.f32(i32 6, float %2907)  ; FAbs(value)
  %2912 = fsub fast float 1.000000e+00, %2910
  %2913 = fsub fast float 1.000000e+00, %2911
  %2914 = fcmp fast oge float %2907, 0.000000e+00
  %2915 = select i1 %2914, float 1.000000e+00, float -1.000000e+00
  %2916 = fcmp fast oge float %2908, 0.000000e+00
  %2917 = select i1 %2916, float 1.000000e+00, float -1.000000e+00
  %2918 = fmul fast float %2915, %2912
  %2919 = fmul fast float %2913, %2917
  %2920 = select i1 %2909, float %2918, float %2907
  %2921 = select i1 %2909, float %2919, float %2908
  %2922 = fmul fast float %2920, 5.000000e-01
  %2923 = fmul fast float %2921, 5.000000e-01
  %2924 = fadd fast float %2922, 5.000000e-01
  %2925 = fadd fast float %2923, 5.000000e-01
  %2926 = add i32 %2870, 6
  %2927 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2926, i32 undef)  ; BufferLoad(srv,index,wot)
  %2928 = extractvalue %dx.types.ResRet.f32 %2927, 0
  %2929 = fptoui float %2928 to i32
  %2930 = add i32 %2870, 7
  %2931 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2930, i32 undef)  ; BufferLoad(srv,index,wot)
  %2932 = extractvalue %dx.types.ResRet.f32 %2931, 0
  %2933 = fptoui float %2932 to i32
  %2934 = add i32 %2870, 8
  %2935 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2934, i32 undef)  ; BufferLoad(srv,index,wot)
  %2936 = extractvalue %dx.types.ResRet.f32 %2935, 0
  %2937 = fptoui float %2936 to i32
  %2938 = add i32 %2870, 9
  %2939 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2938, i32 undef)  ; BufferLoad(srv,index,wot)
  %2940 = extractvalue %dx.types.ResRet.f32 %2939, 0
  %2941 = fptoui float %2940 to i32
  %2942 = mul i32 %2941, %2929
  %2943 = mul i32 %2941, %2933
  %2944 = sitofp i32 %2942 to float
  %2945 = sitofp i32 %2943 to float
  %2946 = sitofp i32 %2937 to float
  %2947 = sitofp i32 %2941 to float
  %2948 = fadd fast float %2947, -2.000000e+00
  %2949 = fmul fast float %2948, %2924
  %2950 = fmul fast float %2948, %2925
  %2951 = fadd fast float %2944, 1.000000e+00
  %2952 = fadd fast float %2951, %2949
  %2953 = fadd fast float %2945, 1.000000e+00
  %2954 = fadd fast float %2953, %2950
  %2955 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %2956 = extractvalue %dx.types.CBufRet.f32 %2955, 0
  %2957 = extractvalue %dx.types.CBufRet.f32 %2955, 1
  %2958 = extractvalue %dx.types.CBufRet.f32 %2955, 3
  %2959 = fmul fast float %2958, %2956
  %2960 = fmul fast float %2958, %2957
  %2961 = fmul fast float %2959, %2952
  %2962 = fmul fast float %2960, %2954
  %2963 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %2964 = extractvalue %dx.types.CBufRet.f32 %2963, 0
  %2965 = fadd fast float %2964, %2946
  %2966 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %2961, float %2962, float %2965, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2967 = extractvalue %dx.types.ResRet.f32 %2966, 0
  %2968 = extractvalue %dx.types.ResRet.f32 %2966, 2
  %2969 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %2970 = extractvalue %dx.types.CBufRet.f32 %2969, 1
  %2971 = fadd fast float %2970, %2900
  %2972 = extractvalue %dx.types.CBufRet.f32 %2969, 0
  %2973 = fdiv fast float %2971, %2972
  %2974 = fcmp fast ogt float %2973, %2967
  br i1 %2974, label %2975, label %2984

; <label>:2975                                    ; preds = %2869
  %2976 = extractvalue %dx.types.ResRet.f32 %2966, 1
  %2977 = fmul fast float %2967, %2967
  %2978 = fsub fast float %2977, %2976
  %2979 = call float @dx.op.unary.f32(i32 6, float %2978)  ; FAbs(value)
  %2980 = fsub fast float %2973, %2967
  %2981 = fmul fast float %2980, %2980
  %2982 = fadd fast float %2979, %2981
  %2983 = fdiv fast float %2979, %2982
  br label %2984

; <label>:2984                                    ; preds = %2975, %2869
  %2985 = phi float [ %2983, %2975 ], [ 1.000000e+00, %2869 ]
  %2986 = fsub fast float %2968, %2973
  %2987 = fcmp fast ogt float %2986, 0.000000e+00
  %2988 = select i1 %2987, float 1.000000e+00, float 0.000000e+00
  %2989 = fmul fast float %2894, %2988
  %2990 = call float @dx.op.binary.f32(i32 35, float %2989, float %2985)  ; FMax(a,b)
  %2991 = fmul fast float %2990, %2892
  %2992 = fsub fast float %2991, %2989
  %2993 = fmul fast float %2992, %375
  %2994 = fadd fast float %2993, %2989
  %2995 = fsub fast float %372, %2872
  %2996 = fsub fast float %373, %2875
  %2997 = fsub fast float %374, %2878
  %2998 = call float @dx.op.dot3.f32(i32 55, float %2995, float %2996, float %2997, float %2995, float %2996, float %2997)  ; Dot3(ax,ay,az,bx,by,bz)
  %2999 = call float @dx.op.unary.f32(i32 25, float %2998)  ; Rsqrt(value)
  %3000 = fmul fast float %2999, %2995
  %3001 = fmul fast float %2999, %2996
  %3002 = fmul fast float %2999, %2997
  %3003 = fmul fast float %2995, %2995
  %3004 = fmul fast float %2996, %2996
  %3005 = fadd fast float %3004, %3003
  %3006 = fmul fast float %2997, %2997
  %3007 = fadd fast float %3005, %3006
  %3008 = call float @dx.op.unary.f32(i32 24, float %3007)  ; Sqrt(value)
  %3009 = call float @dx.op.unary.f32(i32 6, float %3000)  ; FAbs(value)
  %3010 = call float @dx.op.unary.f32(i32 6, float %3001)  ; FAbs(value)
  %3011 = fadd fast float %3010, %3009
  %3012 = call float @dx.op.unary.f32(i32 6, float %3002)  ; FAbs(value)
  %3013 = fadd fast float %3011, %3012
  %3014 = fdiv fast float 1.000000e+00, %3013
  %3015 = fmul fast float %3014, %3000
  %3016 = fmul fast float %3014, %3001
  %3017 = fcmp fast ole float %3002, 0.000000e+00
  %3018 = call float @dx.op.unary.f32(i32 6, float %3016)  ; FAbs(value)
  %3019 = call float @dx.op.unary.f32(i32 6, float %3015)  ; FAbs(value)
  %3020 = fsub fast float 1.000000e+00, %3018
  %3021 = fsub fast float 1.000000e+00, %3019
  %3022 = fcmp fast oge float %3015, 0.000000e+00
  %3023 = select i1 %3022, float 1.000000e+00, float -1.000000e+00
  %3024 = fcmp fast oge float %3016, 0.000000e+00
  %3025 = select i1 %3024, float 1.000000e+00, float -1.000000e+00
  %3026 = fmul fast float %3023, %3020
  %3027 = fmul fast float %3021, %3025
  %3028 = select i1 %3017, float %3026, float %3015
  %3029 = select i1 %3017, float %3027, float %3016
  %3030 = fmul fast float %3028, 5.000000e-01
  %3031 = fmul fast float %3029, 5.000000e-01
  %3032 = fadd fast float %3030, 5.000000e-01
  %3033 = fadd fast float %3031, 5.000000e-01
  %3034 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2926, i32 undef)  ; BufferLoad(srv,index,wot)
  %3035 = extractvalue %dx.types.ResRet.f32 %3034, 0
  %3036 = fptoui float %3035 to i32
  %3037 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2930, i32 undef)  ; BufferLoad(srv,index,wot)
  %3038 = extractvalue %dx.types.ResRet.f32 %3037, 0
  %3039 = fptoui float %3038 to i32
  %3040 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2934, i32 undef)  ; BufferLoad(srv,index,wot)
  %3041 = extractvalue %dx.types.ResRet.f32 %3040, 0
  %3042 = fptoui float %3041 to i32
  %3043 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %2938, i32 undef)  ; BufferLoad(srv,index,wot)
  %3044 = extractvalue %dx.types.ResRet.f32 %3043, 0
  %3045 = fptoui float %3044 to i32
  %3046 = mul i32 %3045, %3036
  %3047 = mul i32 %3045, %3039
  %3048 = sitofp i32 %3046 to float
  %3049 = sitofp i32 %3047 to float
  %3050 = sitofp i32 %3042 to float
  %3051 = sitofp i32 %3045 to float
  %3052 = fadd fast float %3051, -2.000000e+00
  %3053 = fmul fast float %3052, %3032
  %3054 = fmul fast float %3052, %3033
  %3055 = fadd fast float %3048, 1.000000e+00
  %3056 = fadd fast float %3055, %3053
  %3057 = fadd fast float %3049, 1.000000e+00
  %3058 = fadd fast float %3057, %3054
  %3059 = fmul fast float %2959, %3056
  %3060 = fmul fast float %2960, %3058
  %3061 = fadd fast float %2964, %3050
  %3062 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %3059, float %3060, float %3061, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3063 = extractvalue %dx.types.ResRet.f32 %3062, 0
  %3064 = extractvalue %dx.types.ResRet.f32 %3062, 2
  %3065 = fadd fast float %2970, %3008
  %3066 = fdiv fast float %3065, %2972
  %3067 = fcmp fast ogt float %3066, %3063
  br i1 %3067, label %3068, label %3077

; <label>:3068                                    ; preds = %2984
  %3069 = extractvalue %dx.types.ResRet.f32 %3062, 1
  %3070 = fmul fast float %3063, %3063
  %3071 = fsub fast float %3070, %3069
  %3072 = call float @dx.op.unary.f32(i32 6, float %3071)  ; FAbs(value)
  %3073 = fsub fast float %3066, %3063
  %3074 = fmul fast float %3073, %3073
  %3075 = fadd fast float %3072, %3074
  %3076 = fdiv fast float %3072, %3075
  br label %3077

; <label>:3077                                    ; preds = %3068, %2984
  %3078 = phi float [ %3076, %3068 ], [ 1.000000e+00, %2984 ]
  %3079 = fsub fast float %3064, %3066
  %3080 = fcmp fast ogt float %3079, 0.000000e+00
  %3081 = select i1 %3080, float 1.000000e+00, float 0.000000e+00
  %3082 = call float @dx.op.binary.f32(i32 35, float %3081, float %3078)  ; FMax(a,b)
  %3083 = fadd fast float %3081, %2989
  %3084 = fcmp fast olt float %3083, 0x3F50624DE0000000
  br i1 %3084, label %3161, label %3085

; <label>:3085                                    ; preds = %3077
  %3086 = fsub fast float %3081, %3082
  %3087 = fmul fast float %3086, %375
  %3088 = fadd fast float %3087, %3082
  %3089 = and i32 %2863, 1
  %3090 = lshr i32 %2863, 1
  %3091 = and i32 %3090, 1
  %3092 = lshr i32 %2863, 2
  %3093 = and i32 %3092, 1
  %3094 = sitofp i32 %3089 to float
  %3095 = sitofp i32 %3091 to float
  %3096 = sitofp i32 %3093 to float
  %3097 = fsub fast float 1.000000e+00, %2818
  %3098 = fsub fast float 1.000000e+00, %2827
  %3099 = fsub fast float 1.000000e+00, %2836
  %3100 = fsub fast float %2818, %3097
  %3101 = fsub fast float %2827, %3098
  %3102 = fsub fast float %2836, %3099
  %3103 = fmul fast float %3094, %3100
  %3104 = fmul fast float %3095, %3101
  %3105 = fmul fast float %3096, %3102
  %3106 = fadd fast float %3103, %3097
  %3107 = fadd fast float %3104, %3098
  %3108 = fadd fast float %3105, %3099
  %3109 = fmul fast float %3107, %3106
  %3110 = fmul fast float %3109, %3108
  %3111 = call float @dx.op.binary.f32(i32 35, float %3110, float 0x3F847AE140000000)  ; FMax(a,b)
  %3112 = fmul fast float %3088, %2994
  %3113 = fmul fast float %3112, %3111
  %3114 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 %2863
  store float %3113, float* %3114, align 4, !tbaa !53
  %3115 = fmul fast float %3081, %2989
  %3116 = fmul fast float %3115, %3111
  %3117 = fcmp fast ogt float %3116, %2862
  br i1 %3117, label %3118, label %3161

; <label>:3118                                    ; preds = %3085
  %3119 = fsub fast float %352, %2884
  %3120 = fsub fast float %353, %2885
  %3121 = fsub fast float %354, %2886
  %3122 = fmul fast float %3119, %375
  %3123 = fmul fast float %3120, %375
  %3124 = fmul fast float %3121, %375
  %3125 = fadd fast float %3122, %2884
  %3126 = fadd fast float %3123, %2885
  %3127 = fadd fast float %3124, %2886
  %3128 = call float @dx.op.dot3.f32(i32 55, float %3125, float %3126, float %3127, float %3125, float %3126, float %3127)  ; Dot3(ax,ay,az,bx,by,bz)
  %3129 = call float @dx.op.unary.f32(i32 25, float %3128)  ; Rsqrt(value)
  %3130 = fmul fast float %3129, %3125
  %3131 = fmul fast float %3129, %3126
  %3132 = fmul fast float %3129, %3127
  %3133 = call float @dx.op.unary.f32(i32 6, float %3130)  ; FAbs(value)
  %3134 = call float @dx.op.unary.f32(i32 6, float %3131)  ; FAbs(value)
  %3135 = fadd fast float %3134, %3133
  %3136 = call float @dx.op.unary.f32(i32 6, float %3132)  ; FAbs(value)
  %3137 = fadd fast float %3135, %3136
  %3138 = fdiv fast float 1.000000e+00, %3137
  %3139 = fmul fast float %3138, %3130
  %3140 = fmul fast float %3138, %3131
  %3141 = fcmp fast ole float %3132, 0.000000e+00
  %3142 = call float @dx.op.unary.f32(i32 6, float %3140)  ; FAbs(value)
  %3143 = call float @dx.op.unary.f32(i32 6, float %3139)  ; FAbs(value)
  %3144 = fsub fast float 1.000000e+00, %3142
  %3145 = fsub fast float 1.000000e+00, %3143
  %3146 = fcmp fast oge float %3139, 0.000000e+00
  %3147 = select i1 %3146, float 1.000000e+00, float -1.000000e+00
  %3148 = fcmp fast oge float %3140, 0.000000e+00
  %3149 = select i1 %3148, float 1.000000e+00, float -1.000000e+00
  %3150 = fmul fast float %3147, %3144
  %3151 = fmul fast float %3145, %3149
  %3152 = select i1 %3141, float %3150, float %3139
  %3153 = select i1 %3141, float %3151, float %3140
  %3154 = fmul fast float %3152, 5.000000e-01
  %3155 = fmul fast float %3153, 5.000000e-01
  %3156 = fadd fast float %3154, 5.000000e-01
  %3157 = fadd fast float %3155, 5.000000e-01
  %3158 = icmp eq i32 %3045, 64
  %3159 = select i1 %3158, float 3.000000e+00, float %2838
  %3160 = fmul fast float %3159, %375
  br label %3161

; <label>:3161                                    ; preds = %3118, %3085, %3077, %2856
  %3162 = phi i32 [ %2857, %2856 ], [ %2857, %3077 ], [ %2867, %3118 ], [ %2857, %3085 ]
  %3163 = phi float [ %2858, %2856 ], [ %2858, %3077 ], [ %3156, %3118 ], [ %2858, %3085 ]
  %3164 = phi float [ %2859, %2856 ], [ %2859, %3077 ], [ %3157, %3118 ], [ %2859, %3085 ]
  %3165 = phi float [ %2860, %2856 ], [ %2860, %3077 ], [ %3159, %3118 ], [ %2860, %3085 ]
  %3166 = phi float [ %2861, %2856 ], [ %2861, %3077 ], [ %3160, %3118 ], [ %2861, %3085 ]
  %3167 = phi float [ %2862, %2856 ], [ %2862, %3077 ], [ %3116, %3118 ], [ %2862, %3085 ]
  %3168 = add nuw nsw i32 %2863, 1
  %3169 = icmp eq i32 %3168, 8
  br i1 %3169, label %3170, label %2856

; <label>:3170                                    ; preds = %3161
  %3171 = fptosi float %3165 to i32
  %3172 = mul nsw i32 %3162, 10
  %3173 = add i32 %3172, 6
  %3174 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3173, i32 undef)  ; BufferLoad(srv,index,wot)
  %3175 = extractvalue %dx.types.ResRet.f32 %3174, 0
  %3176 = fptoui float %3175 to i32
  %3177 = add i32 %3172, 7
  %3178 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3177, i32 undef)  ; BufferLoad(srv,index,wot)
  %3179 = extractvalue %dx.types.ResRet.f32 %3178, 0
  %3180 = fptoui float %3179 to i32
  %3181 = add i32 %3172, 8
  %3182 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3181, i32 undef)  ; BufferLoad(srv,index,wot)
  %3183 = extractvalue %dx.types.ResRet.f32 %3182, 0
  %3184 = fptoui float %3183 to i32
  %3185 = add i32 %3172, 9
  %3186 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3185, i32 undef)  ; BufferLoad(srv,index,wot)
  %3187 = extractvalue %dx.types.ResRet.f32 %3186, 0
  %3188 = fptoui float %3187 to i32
  %3189 = mul i32 %3188, %3176
  %3190 = mul i32 %3188, %3180
  %3191 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %3192 = extractvalue %dx.types.CBufRet.f32 %3191, 0
  %3193 = extractvalue %dx.types.CBufRet.f32 %3191, 1
  %3194 = extractvalue %dx.types.CBufRet.f32 %3191, 3
  %3195 = fmul fast float %3194, %3192
  %3196 = fmul fast float %3194, %3193
  %3197 = call float @dx.op.unary.f32(i32 29, float %3166)  ; Round_z(value)
  %3198 = fsub fast float %3166, %3197
  %3199 = fptosi float %3197 to i32
  %3200 = call i32 @dx.op.binary.i32(i32 37, i32 %3199, i32 0)  ; IMax(a,b)
  %3201 = add nsw i32 %3200, 1
  %3202 = call i32 @dx.op.binary.i32(i32 38, i32 %3201, i32 %3171)  ; IMin(a,b)
  %3203 = sitofp i32 %3200 to float
  %3204 = call float @dx.op.unary.f32(i32 21, float %3203)  ; Exp(value)
  %3205 = fdiv fast float 1.000000e+00, %3204
  %3206 = sitofp i32 %3189 to float
  %3207 = sitofp i32 %3190 to float
  %3208 = fmul fast float %3205, %3206
  %3209 = fmul fast float %3205, %3207
  %3210 = fptosi float %3208 to i32
  %3211 = fptosi float %3209 to i32
  %3212 = sitofp i32 %3188 to float
  %3213 = fmul fast float %3205, %3212
  %3214 = fadd fast float %3213, -2.000000e+00
  %3215 = sitofp i32 %3184 to float
  %3216 = sitofp i32 %3210 to float
  %3217 = sitofp i32 %3211 to float
  %3218 = fmul fast float %3214, %3163
  %3219 = fmul fast float %3214, %3164
  %3220 = fadd fast float %3218, 1.000000e+00
  %3221 = fadd fast float %3219, 1.000000e+00
  %3222 = fadd fast float %3220, %3216
  %3223 = fadd fast float %3221, %3217
  %3224 = fmul fast float %3205, %3195
  %3225 = fmul fast float %3205, %3196
  %3226 = fdiv fast float %3222, %3224
  %3227 = fdiv fast float %3223, %3225
  %3228 = sitofp i32 %3202 to float
  %3229 = call float @dx.op.unary.f32(i32 21, float %3228)  ; Exp(value)
  %3230 = fdiv fast float 1.000000e+00, %3229
  %3231 = fmul fast float %3230, %3206
  %3232 = fmul fast float %3230, %3207
  %3233 = fptosi float %3231 to i32
  %3234 = fptosi float %3232 to i32
  %3235 = fmul fast float %3230, %3212
  %3236 = fadd fast float %3235, -2.000000e+00
  %3237 = sitofp i32 %3233 to float
  %3238 = sitofp i32 %3234 to float
  %3239 = fmul fast float %3236, %3163
  %3240 = fmul fast float %3236, %3164
  %3241 = fadd fast float %3239, 1.000000e+00
  %3242 = fadd fast float %3240, 1.000000e+00
  %3243 = fadd fast float %3241, %3237
  %3244 = fadd fast float %3242, %3238
  %3245 = fmul fast float %3230, %3195
  %3246 = fmul fast float %3230, %3196
  %3247 = fdiv fast float %3243, %3245
  %3248 = fdiv fast float %3244, %3246
  %3249 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3226, float %3227, float %3215, float undef, i32 0, i32 0, i32 undef, float %3203)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3250 = extractvalue %dx.types.ResRet.f32 %3249, 0
  %3251 = extractvalue %dx.types.ResRet.f32 %3249, 1
  %3252 = extractvalue %dx.types.ResRet.f32 %3249, 2
  %3253 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3247, float %3248, float %3215, float undef, i32 0, i32 0, i32 undef, float %3228)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3254 = extractvalue %dx.types.ResRet.f32 %3253, 0
  %3255 = extractvalue %dx.types.ResRet.f32 %3253, 1
  %3256 = extractvalue %dx.types.ResRet.f32 %3253, 2
  %3257 = fsub fast float %3254, %3250
  %3258 = fsub fast float %3255, %3251
  %3259 = fsub fast float %3256, %3252
  %3260 = fmul fast float %3257, %3198
  %3261 = fmul fast float %3258, %3198
  %3262 = fmul fast float %3259, %3198
  %3263 = fadd fast float %3260, %3250
  %3264 = fadd fast float %3261, %3251
  %3265 = fadd fast float %3262, %3252
  br label %3266

; <label>:3266                                    ; preds = %3447, %3170
  %3267 = phi float [ 0.000000e+00, %3170 ], [ %3448, %3447 ]
  %3268 = phi float [ 0.000000e+00, %3170 ], [ %3449, %3447 ]
  %3269 = phi float [ 0.000000e+00, %3170 ], [ %3450, %3447 ]
  %3270 = phi float [ 0.000000e+00, %3170 ], [ %3451, %3447 ]
  %3271 = phi i32 [ 0, %3170 ], [ %3452, %3447 ]
  %3272 = getelementptr inbounds [8 x float], [8 x float]* %32, i32 0, i32 %3271
  %3273 = load float, float* %3272, align 4, !tbaa !53
  %3274 = fcmp fast oeq float %3273, 0.000000e+00
  br i1 %3274, label %3447, label %3275

; <label>:3275                                    ; preds = %3266
  %3276 = add i32 %3271, %2864
  %3277 = getelementptr inbounds [8 x i32], [8 x i32]* %40, i32 0, i32 %3276
  %3278 = load i32, i32* %3277, align 4, !tbaa !34
  %3279 = icmp slt i32 %3278, 0
  br i1 %3279, label %3447, label %3280

; <label>:3280                                    ; preds = %3275
  %3281 = mul i32 %3278, 10
  %3282 = icmp eq i32 %3162, %3278
  br i1 %3282, label %3436, label %3283

; <label>:3283                                    ; preds = %3280
  %3284 = add i32 %3281, 2
  %3285 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3284, i32 undef)  ; BufferLoad(srv,index,wot)
  %3286 = extractvalue %dx.types.ResRet.f32 %3285, 0
  %3287 = or i32 %3281, 1
  %3288 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3287, i32 undef)  ; BufferLoad(srv,index,wot)
  %3289 = extractvalue %dx.types.ResRet.f32 %3288, 0
  %3290 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3281, i32 undef)  ; BufferLoad(srv,index,wot)
  %3291 = extractvalue %dx.types.ResRet.f32 %3290, 0
  %3292 = fsub fast float %2750, %3291
  %3293 = fsub fast float %2751, %3289
  %3294 = fsub fast float %2752, %3286
  %3295 = call float @dx.op.dot3.f32(i32 55, float %3292, float %3293, float %3294, float %3292, float %3293, float %3294)  ; Dot3(ax,ay,az,bx,by,bz)
  %3296 = call float @dx.op.unary.f32(i32 25, float %3295)  ; Rsqrt(value)
  %3297 = fmul fast float %3292, %3296
  %3298 = fmul fast float %3296, %3293
  %3299 = fmul fast float %3296, %3294
  %3300 = fsub fast float %352, %3297
  %3301 = fsub fast float %353, %3298
  %3302 = fsub fast float %354, %3299
  %3303 = fmul fast float %3300, %375
  %3304 = fmul fast float %3301, %375
  %3305 = fmul fast float %3302, %375
  %3306 = fadd fast float %3303, %3297
  %3307 = fadd fast float %3304, %3298
  %3308 = fadd fast float %3305, %3299
  %3309 = call float @dx.op.dot3.f32(i32 55, float %3306, float %3307, float %3308, float %3306, float %3307, float %3308)  ; Dot3(ax,ay,az,bx,by,bz)
  %3310 = call float @dx.op.unary.f32(i32 25, float %3309)  ; Rsqrt(value)
  %3311 = fmul fast float %3306, %3310
  %3312 = fmul fast float %3307, %3310
  %3313 = fmul fast float %3308, %3310
  %3314 = add i32 %3281, 9
  %3315 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3314, i32 undef)  ; BufferLoad(srv,index,wot)
  %3316 = extractvalue %dx.types.ResRet.f32 %3315, 0
  %3317 = fptoui float %3316 to i32
  %3318 = icmp eq i32 %3317, 64
  %3319 = select i1 %3318, float 3.000000e+00, float %2838
  %3320 = fmul fast float %3319, %375
  %3321 = call float @dx.op.unary.f32(i32 6, float %3311)  ; FAbs(value)
  %3322 = call float @dx.op.unary.f32(i32 6, float %3312)  ; FAbs(value)
  %3323 = fadd fast float %3322, %3321
  %3324 = call float @dx.op.unary.f32(i32 6, float %3313)  ; FAbs(value)
  %3325 = fadd fast float %3323, %3324
  %3326 = fdiv fast float 1.000000e+00, %3325
  %3327 = fmul fast float %3326, %3311
  %3328 = fmul fast float %3326, %3312
  %3329 = fcmp fast ole float %3313, 0.000000e+00
  %3330 = call float @dx.op.unary.f32(i32 6, float %3328)  ; FAbs(value)
  %3331 = call float @dx.op.unary.f32(i32 6, float %3327)  ; FAbs(value)
  %3332 = fsub fast float 1.000000e+00, %3330
  %3333 = fsub fast float 1.000000e+00, %3331
  %3334 = fcmp fast oge float %3327, 0.000000e+00
  %3335 = select i1 %3334, float 1.000000e+00, float -1.000000e+00
  %3336 = fcmp fast oge float %3328, 0.000000e+00
  %3337 = select i1 %3336, float 1.000000e+00, float -1.000000e+00
  %3338 = fmul fast float %3335, %3332
  %3339 = fmul fast float %3333, %3337
  %3340 = select i1 %3329, float %3338, float %3327
  %3341 = select i1 %3329, float %3339, float %3328
  %3342 = fmul fast float %3340, 5.000000e-01
  %3343 = fmul fast float %3341, 5.000000e-01
  %3344 = fadd fast float %3342, 5.000000e-01
  %3345 = fadd fast float %3343, 5.000000e-01
  %3346 = fptosi float %3319 to i32
  %3347 = add i32 %3281, 6
  %3348 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3347, i32 undef)  ; BufferLoad(srv,index,wot)
  %3349 = extractvalue %dx.types.ResRet.f32 %3348, 0
  %3350 = fptoui float %3349 to i32
  %3351 = add i32 %3281, 7
  %3352 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3351, i32 undef)  ; BufferLoad(srv,index,wot)
  %3353 = extractvalue %dx.types.ResRet.f32 %3352, 0
  %3354 = fptoui float %3353 to i32
  %3355 = add i32 %3281, 8
  %3356 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3355, i32 undef)  ; BufferLoad(srv,index,wot)
  %3357 = extractvalue %dx.types.ResRet.f32 %3356, 0
  %3358 = fptoui float %3357 to i32
  %3359 = mul i32 %3317, %3350
  %3360 = mul i32 %3317, %3354
  %3361 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %3362 = extractvalue %dx.types.CBufRet.f32 %3361, 0
  %3363 = extractvalue %dx.types.CBufRet.f32 %3361, 1
  %3364 = extractvalue %dx.types.CBufRet.f32 %3361, 3
  %3365 = fmul fast float %3364, %3362
  %3366 = fmul fast float %3364, %3363
  %3367 = call float @dx.op.unary.f32(i32 29, float %3320)  ; Round_z(value)
  %3368 = fsub fast float %3320, %3367
  %3369 = fptosi float %3367 to i32
  %3370 = call i32 @dx.op.binary.i32(i32 37, i32 %3369, i32 0)  ; IMax(a,b)
  %3371 = add nsw i32 %3370, 1
  %3372 = call i32 @dx.op.binary.i32(i32 38, i32 %3371, i32 %3346)  ; IMin(a,b)
  %3373 = sitofp i32 %3370 to float
  %3374 = call float @dx.op.unary.f32(i32 21, float %3373)  ; Exp(value)
  %3375 = fdiv fast float 1.000000e+00, %3374
  %3376 = sitofp i32 %3359 to float
  %3377 = sitofp i32 %3360 to float
  %3378 = fmul fast float %3375, %3376
  %3379 = fmul fast float %3375, %3377
  %3380 = fptosi float %3378 to i32
  %3381 = fptosi float %3379 to i32
  %3382 = sitofp i32 %3317 to float
  %3383 = fmul fast float %3375, %3382
  %3384 = fadd fast float %3383, -2.000000e+00
  %3385 = sitofp i32 %3358 to float
  %3386 = sitofp i32 %3380 to float
  %3387 = sitofp i32 %3381 to float
  %3388 = fmul fast float %3384, %3344
  %3389 = fmul fast float %3384, %3345
  %3390 = fadd fast float %3388, 1.000000e+00
  %3391 = fadd fast float %3389, 1.000000e+00
  %3392 = fadd fast float %3390, %3386
  %3393 = fadd fast float %3391, %3387
  %3394 = fmul fast float %3375, %3365
  %3395 = fmul fast float %3375, %3366
  %3396 = fdiv fast float %3392, %3394
  %3397 = fdiv fast float %3393, %3395
  %3398 = sitofp i32 %3372 to float
  %3399 = call float @dx.op.unary.f32(i32 21, float %3398)  ; Exp(value)
  %3400 = fdiv fast float 1.000000e+00, %3399
  %3401 = fmul fast float %3400, %3376
  %3402 = fmul fast float %3400, %3377
  %3403 = fptosi float %3401 to i32
  %3404 = fptosi float %3402 to i32
  %3405 = fmul fast float %3400, %3382
  %3406 = fadd fast float %3405, -2.000000e+00
  %3407 = sitofp i32 %3403 to float
  %3408 = sitofp i32 %3404 to float
  %3409 = fmul fast float %3406, %3344
  %3410 = fmul fast float %3406, %3345
  %3411 = fadd fast float %3409, 1.000000e+00
  %3412 = fadd fast float %3410, 1.000000e+00
  %3413 = fadd fast float %3411, %3407
  %3414 = fadd fast float %3412, %3408
  %3415 = fmul fast float %3400, %3365
  %3416 = fmul fast float %3400, %3366
  %3417 = fdiv fast float %3413, %3415
  %3418 = fdiv fast float %3414, %3416
  %3419 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3396, float %3397, float %3385, float undef, i32 0, i32 0, i32 undef, float %3373)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3420 = extractvalue %dx.types.ResRet.f32 %3419, 0
  %3421 = extractvalue %dx.types.ResRet.f32 %3419, 1
  %3422 = extractvalue %dx.types.ResRet.f32 %3419, 2
  %3423 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3417, float %3418, float %3385, float undef, i32 0, i32 0, i32 undef, float %3398)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3424 = extractvalue %dx.types.ResRet.f32 %3423, 0
  %3425 = extractvalue %dx.types.ResRet.f32 %3423, 1
  %3426 = extractvalue %dx.types.ResRet.f32 %3423, 2
  %3427 = fsub fast float %3424, %3420
  %3428 = fsub fast float %3425, %3421
  %3429 = fsub fast float %3426, %3422
  %3430 = fmul fast float %3427, %3368
  %3431 = fmul fast float %3428, %3368
  %3432 = fmul fast float %3429, %3368
  %3433 = fadd fast float %3430, %3420
  %3434 = fadd fast float %3431, %3421
  %3435 = fadd fast float %3432, %3422
  br label %3436

; <label>:3436                                    ; preds = %3283, %3280
  %3437 = phi float [ %3433, %3283 ], [ %3263, %3280 ]
  %3438 = phi float [ %3434, %3283 ], [ %3264, %3280 ]
  %3439 = phi float [ %3435, %3283 ], [ %3265, %3280 ]
  %3440 = fmul fast float %3437, %3273
  %3441 = fmul fast float %3438, %3273
  %3442 = fmul fast float %3439, %3273
  %3443 = fadd fast float %3440, %3267
  %3444 = fadd fast float %3441, %3268
  %3445 = fadd fast float %3442, %3269
  %3446 = fadd fast float %3273, %3270
  br label %3447

; <label>:3447                                    ; preds = %3436, %3275, %3266
  %3448 = phi float [ %3267, %3266 ], [ %3267, %3275 ], [ %3443, %3436 ]
  %3449 = phi float [ %3268, %3266 ], [ %3268, %3275 ], [ %3444, %3436 ]
  %3450 = phi float [ %3269, %3266 ], [ %3269, %3275 ], [ %3445, %3436 ]
  %3451 = phi float [ %3270, %3266 ], [ %3270, %3275 ], [ %3446, %3436 ]
  %3452 = add nuw nsw i32 %3271, 1
  %3453 = icmp eq i32 %3452, 8
  br i1 %3453, label %3454, label %3266

; <label>:3454                                    ; preds = %3447
  %3455 = fcmp fast ogt float %3451, 0.000000e+00
  br i1 %3455, label %3456, label %4962

; <label>:3456                                    ; preds = %3454
  %3457 = fdiv fast float %3448, %3451
  %3458 = fdiv fast float %3449, %3451
  %3459 = fdiv fast float %3450, %3451
  br label %4962

; <label>:3460                                    ; preds = %2748
  %3461 = icmp eq i32 %2756, 0
  br i1 %3461, label %3462, label %3816

; <label>:3462                                    ; preds = %3460
  %3463 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 0
  store i32 %1732, i32* %3463, align 4
  %3464 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 1
  store i32 %1733, i32* %3464, align 4
  %3465 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 2
  store i32 %1734, i32* %3465, align 4
  %3466 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 3
  store i32 %1735, i32* %3466, align 4
  %3467 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 4
  store i32 %1736, i32* %3467, align 4
  %3468 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 5
  store i32 %1737, i32* %3468, align 4
  %3469 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 6
  store i32 %1738, i32* %3469, align 4
  %3470 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 7
  store i32 %1739, i32* %3470, align 4
  %3471 = fdiv fast float %2791, %1758
  %3472 = fdiv fast float %2792, %1758
  %3473 = fdiv fast float %2793, %1758
  %3474 = fcmp fast olt float %3471, 0.000000e+00
  %3475 = fcmp fast ogt float %3471, 1.000000e+00
  %3476 = or i1 %3474, %3475
  br i1 %3476, label %3477, label %3481

; <label>:3477                                    ; preds = %3462
  %3478 = fmul fast float %3471, 2.000000e+00
  %3479 = fadd fast float %3478, -1.000000e+00
  %3480 = fdiv fast float %3471, %3479
  br label %3481

; <label>:3481                                    ; preds = %3477, %3462
  %3482 = phi float [ %3480, %3477 ], [ %3471, %3462 ]
  %3483 = fcmp fast olt float %3472, 0.000000e+00
  %3484 = fcmp fast ogt float %3472, 1.000000e+00
  %3485 = or i1 %3483, %3484
  br i1 %3485, label %3486, label %3490

; <label>:3486                                    ; preds = %3481
  %3487 = fmul fast float %3472, 2.000000e+00
  %3488 = fadd fast float %3487, -1.000000e+00
  %3489 = fdiv fast float %3472, %3488
  br label %3490

; <label>:3490                                    ; preds = %3486, %3481
  %3491 = phi float [ %3489, %3486 ], [ %3472, %3481 ]
  %3492 = fcmp fast olt float %3473, 0.000000e+00
  %3493 = fcmp fast ogt float %3473, 1.000000e+00
  %3494 = or i1 %3492, %3493
  br i1 %3494, label %3495, label %3499

; <label>:3495                                    ; preds = %3490
  %3496 = fmul fast float %3473, 2.000000e+00
  %3497 = fadd fast float %3496, -1.000000e+00
  %3498 = fdiv fast float %3473, %3497
  br label %3499

; <label>:3499                                    ; preds = %3495, %3490
  %3500 = phi float [ %3498, %3495 ], [ %3473, %3490 ]
  br label %3501

; <label>:3501                                    ; preds = %3799, %3499
  %3502 = phi i32 [ %1732, %3499 ], [ %3801, %3799 ]
  %3503 = phi float [ 0.000000e+00, %3499 ], [ %3792, %3799 ]
  %3504 = phi i32 [ 0, %3499 ], [ %3797, %3799 ]
  %3505 = phi float [ 0.000000e+00, %3499 ], [ %3793, %3799 ]
  %3506 = phi float [ 0.000000e+00, %3499 ], [ %3794, %3799 ]
  %3507 = phi float [ 0.000000e+00, %3499 ], [ %3795, %3799 ]
  %3508 = phi float [ 0.000000e+00, %3499 ], [ %3796, %3799 ]
  %3509 = icmp slt i32 %3502, 0
  br i1 %3509, label %3791, label %3510

; <label>:3510                                    ; preds = %3501
  %3511 = call float @dx.op.unary.f32(i32 6, float %352)  ; FAbs(value)
  %3512 = call float @dx.op.unary.f32(i32 6, float %353)  ; FAbs(value)
  %3513 = fadd fast float %3512, %3511
  %3514 = call float @dx.op.unary.f32(i32 6, float %354)  ; FAbs(value)
  %3515 = fadd fast float %3513, %3514
  %3516 = fdiv fast float 1.000000e+00, %3515
  %3517 = fmul fast float %3516, %352
  %3518 = fmul fast float %3516, %353
  %3519 = fcmp fast ole float %354, 0.000000e+00
  %3520 = call float @dx.op.unary.f32(i32 6, float %3518)  ; FAbs(value)
  %3521 = call float @dx.op.unary.f32(i32 6, float %3517)  ; FAbs(value)
  %3522 = fsub fast float 1.000000e+00, %3520
  %3523 = fsub fast float 1.000000e+00, %3521
  %3524 = fcmp fast oge float %3517, 0.000000e+00
  %3525 = select i1 %3524, float 1.000000e+00, float -1.000000e+00
  %3526 = fcmp fast oge float %3518, 0.000000e+00
  %3527 = select i1 %3526, float 1.000000e+00, float -1.000000e+00
  %3528 = fmul fast float %3525, %3522
  %3529 = fmul fast float %3523, %3527
  %3530 = select i1 %3519, float %3528, float %3517
  %3531 = select i1 %3519, float %3529, float %3518
  %3532 = fmul fast float %3530, 5.000000e-01
  %3533 = fmul fast float %3531, 5.000000e-01
  %3534 = fadd fast float %3532, 5.000000e-01
  %3535 = fadd fast float %3533, 5.000000e-01
  %3536 = mul i32 %3502, 10
  %3537 = add i32 %3536, 6
  %3538 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3537, i32 undef)  ; BufferLoad(srv,index,wot)
  %3539 = extractvalue %dx.types.ResRet.f32 %3538, 0
  %3540 = fptoui float %3539 to i32
  %3541 = add i32 %3536, 7
  %3542 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3541, i32 undef)  ; BufferLoad(srv,index,wot)
  %3543 = extractvalue %dx.types.ResRet.f32 %3542, 0
  %3544 = fptoui float %3543 to i32
  %3545 = add i32 %3536, 8
  %3546 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3545, i32 undef)  ; BufferLoad(srv,index,wot)
  %3547 = extractvalue %dx.types.ResRet.f32 %3546, 0
  %3548 = fptoui float %3547 to i32
  %3549 = add i32 %3536, 9
  %3550 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3549, i32 undef)  ; BufferLoad(srv,index,wot)
  %3551 = extractvalue %dx.types.ResRet.f32 %3550, 0
  %3552 = fptoui float %3551 to i32
  %3553 = mul i32 %3552, %3540
  %3554 = mul i32 %3552, %3544
  %3555 = sitofp i32 %3553 to float
  %3556 = sitofp i32 %3554 to float
  %3557 = sitofp i32 %3548 to float
  %3558 = sitofp i32 %3552 to float
  %3559 = fadd fast float %3558, -2.000000e+00
  %3560 = fmul fast float %3559, %3534
  %3561 = fmul fast float %3559, %3535
  %3562 = fadd fast float %3555, 1.000000e+00
  %3563 = fadd fast float %3562, %3560
  %3564 = fadd fast float %3556, 1.000000e+00
  %3565 = fadd fast float %3564, %3561
  %3566 = fptosi float %3563 to i32
  %3567 = fptosi float %3565 to i32
  %3568 = fptosi float %3557 to i32
  %3569 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %8, i32 0, i32 %3566, i32 %3567, i32 %3568, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %3570 = extractvalue %dx.types.ResRet.f32 %3569, 2
  %3571 = fcmp fast olt float %3570, 0x3FEFF7CEE0000000
  br i1 %3571, label %3791, label %3572

; <label>:3572                                    ; preds = %3510
  %3573 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %3574 = extractvalue %dx.types.CBufRet.f32 %3573, 2
  %3575 = icmp eq i32 %3552, 64
  %3576 = select i1 %3575, float 3.000000e+00, float %3574
  %3577 = fmul fast float %3576, %375
  %3578 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %3579 = extractvalue %dx.types.CBufRet.f32 %3578, 0
  %3580 = extractvalue %dx.types.CBufRet.f32 %3578, 1
  %3581 = extractvalue %dx.types.CBufRet.f32 %3578, 3
  %3582 = fmul fast float %3581, %3579
  %3583 = fmul fast float %3581, %3580
  %3584 = fptosi float %3576 to i32
  %3585 = call float @dx.op.unary.f32(i32 29, float %3577)  ; Round_z(value)
  %3586 = fsub fast float %3577, %3585
  %3587 = fptosi float %3585 to i32
  %3588 = call i32 @dx.op.binary.i32(i32 37, i32 %3587, i32 0)  ; IMax(a,b)
  %3589 = add nsw i32 %3588, 1
  %3590 = call i32 @dx.op.binary.i32(i32 38, i32 %3589, i32 %3584)  ; IMin(a,b)
  %3591 = sitofp i32 %3588 to float
  %3592 = call float @dx.op.unary.f32(i32 21, float %3591)  ; Exp(value)
  %3593 = fdiv fast float 1.000000e+00, %3592
  %3594 = fmul fast float %3593, %3555
  %3595 = fmul fast float %3593, %3556
  %3596 = fptosi float %3594 to i32
  %3597 = fptosi float %3595 to i32
  %3598 = fmul fast float %3593, %3558
  %3599 = fadd fast float %3598, -2.000000e+00
  %3600 = sitofp i32 %3596 to float
  %3601 = sitofp i32 %3597 to float
  %3602 = fmul fast float %3599, %3534
  %3603 = fmul fast float %3599, %3535
  %3604 = fadd fast float %3602, 1.000000e+00
  %3605 = fadd fast float %3603, 1.000000e+00
  %3606 = fadd fast float %3604, %3600
  %3607 = fadd fast float %3605, %3601
  %3608 = fmul fast float %3593, %3582
  %3609 = fmul fast float %3593, %3583
  %3610 = fdiv fast float %3606, %3608
  %3611 = fdiv fast float %3607, %3609
  %3612 = sitofp i32 %3590 to float
  %3613 = call float @dx.op.unary.f32(i32 21, float %3612)  ; Exp(value)
  %3614 = fdiv fast float 1.000000e+00, %3613
  %3615 = fmul fast float %3614, %3555
  %3616 = fmul fast float %3614, %3556
  %3617 = fptosi float %3615 to i32
  %3618 = fptosi float %3616 to i32
  %3619 = fmul fast float %3614, %3558
  %3620 = fadd fast float %3619, -2.000000e+00
  %3621 = sitofp i32 %3617 to float
  %3622 = sitofp i32 %3618 to float
  %3623 = fmul fast float %3620, %3534
  %3624 = fmul fast float %3620, %3535
  %3625 = fadd fast float %3623, 1.000000e+00
  %3626 = fadd fast float %3624, 1.000000e+00
  %3627 = fadd fast float %3625, %3621
  %3628 = fadd fast float %3626, %3622
  %3629 = fmul fast float %3614, %3582
  %3630 = fmul fast float %3614, %3583
  %3631 = fdiv fast float %3627, %3629
  %3632 = fdiv fast float %3628, %3630
  %3633 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3610, float %3611, float %3557, float undef, i32 0, i32 0, i32 undef, float %3591)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3634 = extractvalue %dx.types.ResRet.f32 %3633, 0
  %3635 = extractvalue %dx.types.ResRet.f32 %3633, 1
  %3636 = extractvalue %dx.types.ResRet.f32 %3633, 2
  %3637 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %3631, float %3632, float %3557, float undef, i32 0, i32 0, i32 undef, float %3612)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3638 = extractvalue %dx.types.ResRet.f32 %3637, 0
  %3639 = extractvalue %dx.types.ResRet.f32 %3637, 1
  %3640 = extractvalue %dx.types.ResRet.f32 %3637, 2
  %3641 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %3610, float %3611, float %3557, float undef, i32 0, i32 0, i32 undef, float %3591)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3642 = extractvalue %dx.types.ResRet.f32 %3641, 0
  %3643 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %3631, float %3632, float %3557, float undef, i32 0, i32 0, i32 undef, float %3612)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3644 = extractvalue %dx.types.ResRet.f32 %3643, 0
  %3645 = fsub fast float %3638, %3634
  %3646 = fsub fast float %3639, %3635
  %3647 = fsub fast float %3640, %3636
  %3648 = fsub fast float %3644, %3642
  %3649 = fmul fast float %3645, %3586
  %3650 = fmul fast float %3646, %3586
  %3651 = fmul fast float %3647, %3586
  %3652 = fmul fast float %3648, %3586
  %3653 = fadd fast float %3649, %3634
  %3654 = fadd fast float %3650, %3635
  %3655 = fadd fast float %3651, %3636
  %3656 = fadd fast float %3652, %3642
  %3657 = and i32 %3504, 1
  %3658 = lshr i32 %3504, 1
  %3659 = and i32 %3658, 1
  %3660 = lshr i32 %3504, 2
  %3661 = and i32 %3660, 1
  %3662 = sitofp i32 %3657 to float
  %3663 = sitofp i32 %3659 to float
  %3664 = sitofp i32 %3661 to float
  %3665 = fsub fast float 1.000000e+00, %3482
  %3666 = fsub fast float 1.000000e+00, %3491
  %3667 = fsub fast float 1.000000e+00, %3500
  %3668 = fsub fast float %3482, %3665
  %3669 = fsub fast float %3491, %3666
  %3670 = fsub fast float %3500, %3667
  %3671 = fmul fast float %3662, %3668
  %3672 = fmul fast float %3663, %3669
  %3673 = fmul fast float %3664, %3670
  %3674 = fadd fast float %3671, %3665
  %3675 = fadd fast float %3672, %3666
  %3676 = fadd fast float %3673, %3667
  %3677 = fmul fast float %3675, %3674
  %3678 = fmul fast float %3677, %3676
  %3679 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3536, i32 undef)  ; BufferLoad(srv,index,wot)
  %3680 = extractvalue %dx.types.ResRet.f32 %3679, 0
  %3681 = or i32 %3536, 1
  %3682 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3681, i32 undef)  ; BufferLoad(srv,index,wot)
  %3683 = extractvalue %dx.types.ResRet.f32 %3682, 0
  %3684 = add i32 %3536, 2
  %3685 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3684, i32 undef)  ; BufferLoad(srv,index,wot)
  %3686 = extractvalue %dx.types.ResRet.f32 %3685, 0
  %3687 = fsub fast float %372, %3680
  %3688 = fsub fast float %373, %3683
  %3689 = fsub fast float %374, %3686
  %3690 = call float @dx.op.dot3.f32(i32 55, float %3687, float %3688, float %3689, float %3687, float %3688, float %3689)  ; Dot3(ax,ay,az,bx,by,bz)
  %3691 = call float @dx.op.unary.f32(i32 25, float %3690)  ; Rsqrt(value)
  %3692 = fmul fast float %3691, %3687
  %3693 = fmul fast float %3691, %3688
  %3694 = fmul fast float %3689, %3691
  %3695 = fmul fast float %3687, %3687
  %3696 = fmul fast float %3688, %3688
  %3697 = fadd fast float %3696, %3695
  %3698 = fmul fast float %3689, %3689
  %3699 = fadd fast float %3697, %3698
  %3700 = call float @dx.op.unary.f32(i32 24, float %3699)  ; Sqrt(value)
  %3701 = call float @dx.op.unary.f32(i32 6, float %3692)  ; FAbs(value)
  %3702 = call float @dx.op.unary.f32(i32 6, float %3693)  ; FAbs(value)
  %3703 = fadd fast float %3702, %3701
  %3704 = call float @dx.op.unary.f32(i32 6, float %3694)  ; FAbs(value)
  %3705 = fadd fast float %3703, %3704
  %3706 = fdiv fast float 1.000000e+00, %3705
  %3707 = fmul fast float %3706, %3692
  %3708 = fmul fast float %3706, %3693
  %3709 = fcmp fast ole float %3694, 0.000000e+00
  %3710 = call float @dx.op.unary.f32(i32 6, float %3708)  ; FAbs(value)
  %3711 = call float @dx.op.unary.f32(i32 6, float %3707)  ; FAbs(value)
  %3712 = fsub fast float 1.000000e+00, %3710
  %3713 = fsub fast float 1.000000e+00, %3711
  %3714 = fcmp fast oge float %3707, 0.000000e+00
  %3715 = select i1 %3714, float 1.000000e+00, float -1.000000e+00
  %3716 = fcmp fast oge float %3708, 0.000000e+00
  %3717 = select i1 %3716, float 1.000000e+00, float -1.000000e+00
  %3718 = fmul fast float %3715, %3712
  %3719 = fmul fast float %3713, %3717
  %3720 = select i1 %3709, float %3718, float %3707
  %3721 = select i1 %3709, float %3719, float %3708
  %3722 = fmul fast float %3720, 5.000000e-01
  %3723 = fmul fast float %3721, 5.000000e-01
  %3724 = fadd fast float %3722, 5.000000e-01
  %3725 = fadd fast float %3723, 5.000000e-01
  %3726 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3537, i32 undef)  ; BufferLoad(srv,index,wot)
  %3727 = extractvalue %dx.types.ResRet.f32 %3726, 0
  %3728 = fptoui float %3727 to i32
  %3729 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3541, i32 undef)  ; BufferLoad(srv,index,wot)
  %3730 = extractvalue %dx.types.ResRet.f32 %3729, 0
  %3731 = fptoui float %3730 to i32
  %3732 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3545, i32 undef)  ; BufferLoad(srv,index,wot)
  %3733 = extractvalue %dx.types.ResRet.f32 %3732, 0
  %3734 = fptoui float %3733 to i32
  %3735 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3549, i32 undef)  ; BufferLoad(srv,index,wot)
  %3736 = extractvalue %dx.types.ResRet.f32 %3735, 0
  %3737 = fptoui float %3736 to i32
  %3738 = mul i32 %3737, %3728
  %3739 = mul i32 %3737, %3731
  %3740 = sitofp i32 %3738 to float
  %3741 = sitofp i32 %3739 to float
  %3742 = sitofp i32 %3734 to float
  %3743 = sitofp i32 %3737 to float
  %3744 = fadd fast float %3743, -2.000000e+00
  %3745 = fmul fast float %3744, %3724
  %3746 = fmul fast float %3744, %3725
  %3747 = fadd fast float %3740, 1.000000e+00
  %3748 = fadd fast float %3747, %3745
  %3749 = fadd fast float %3741, 1.000000e+00
  %3750 = fadd fast float %3749, %3746
  %3751 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %3752 = extractvalue %dx.types.CBufRet.f32 %3751, 0
  %3753 = extractvalue %dx.types.CBufRet.f32 %3751, 1
  %3754 = extractvalue %dx.types.CBufRet.f32 %3751, 3
  %3755 = fmul fast float %3754, %3752
  %3756 = fmul fast float %3754, %3753
  %3757 = fmul fast float %3755, %3748
  %3758 = fmul fast float %3756, %3750
  %3759 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %3760 = extractvalue %dx.types.CBufRet.f32 %3759, 0
  %3761 = fadd fast float %3760, %3742
  %3762 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %3757, float %3758, float %3761, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3763 = extractvalue %dx.types.ResRet.f32 %3762, 0
  %3764 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %3765 = extractvalue %dx.types.CBufRet.f32 %3764, 1
  %3766 = fadd fast float %3765, %3700
  %3767 = extractvalue %dx.types.CBufRet.f32 %3764, 0
  %3768 = fdiv fast float %3766, %3767
  %3769 = fcmp fast ogt float %3768, %3763
  br i1 %3769, label %3770, label %3779

; <label>:3770                                    ; preds = %3572
  %3771 = extractvalue %dx.types.ResRet.f32 %3762, 1
  %3772 = fmul fast float %3763, %3763
  %3773 = fsub fast float %3772, %3771
  %3774 = call float @dx.op.unary.f32(i32 6, float %3773)  ; FAbs(value)
  %3775 = fsub fast float %3768, %3763
  %3776 = fmul fast float %3775, %3775
  %3777 = fadd fast float %3774, %3776
  %3778 = fdiv fast float %3774, %3777
  br label %3779

; <label>:3779                                    ; preds = %3770, %3572
  %3780 = phi float [ %3778, %3770 ], [ 1.000000e+00, %3572 ]
  %3781 = fmul fast float %3678, %3780
  %3782 = fmul fast float %3781, %3653
  %3783 = fmul fast float %3781, %3654
  %3784 = fmul fast float %3781, %3655
  %3785 = fmul fast float %3781, %3656
  %3786 = fadd fast float %3782, %3505
  %3787 = fadd fast float %3783, %3506
  %3788 = fadd fast float %3784, %3507
  %3789 = fadd fast float %3785, %3508
  %3790 = fadd fast float %3781, %3503
  br label %3791

; <label>:3791                                    ; preds = %3779, %3510, %3501
  %3792 = phi float [ %3503, %3501 ], [ %3503, %3510 ], [ %3790, %3779 ]
  %3793 = phi float [ %3505, %3501 ], [ %3505, %3510 ], [ %3786, %3779 ]
  %3794 = phi float [ %3506, %3501 ], [ %3506, %3510 ], [ %3787, %3779 ]
  %3795 = phi float [ %3507, %3501 ], [ %3507, %3510 ], [ %3788, %3779 ]
  %3796 = phi float [ %3508, %3501 ], [ %3508, %3510 ], [ %3789, %3779 ]
  %3797 = add nuw nsw i32 %3504, 1
  %3798 = icmp eq i32 %3797, 8
  br i1 %3798, label %3802, label %3799

; <label>:3799                                    ; preds = %3791
  %3800 = getelementptr inbounds [8 x i32], [8 x i32]* %39, i32 0, i32 %3797
  %3801 = load i32, i32* %3800, align 4, !tbaa !34, !noalias !66
  br label %3501

; <label>:3802                                    ; preds = %3791
  %3803 = fcmp fast oeq float %3792, 0.000000e+00
  br i1 %3803, label %3810, label %3804

; <label>:3804                                    ; preds = %3802
  %3805 = call float @dx.op.binary.f32(i32 35, float %3792, float 0x380FFFFF40000000)  ; FMax(a,b)
  %3806 = fdiv fast float %3793, %3805
  %3807 = fdiv fast float %3794, %3805
  %3808 = fdiv fast float %3795, %3805
  %3809 = fdiv fast float %3796, %3805
  br label %3810

; <label>:3810                                    ; preds = %3804, %3802
  %3811 = phi float [ %3793, %3802 ], [ %3806, %3804 ]
  %3812 = phi float [ %3794, %3802 ], [ %3807, %3804 ]
  %3813 = phi float [ %3795, %3802 ], [ %3808, %3804 ]
  %3814 = phi float [ %3796, %3802 ], [ %3809, %3804 ]
  %3815 = call float @dx.op.binary.f32(i32 35, float %3814, float 0.000000e+00)  ; FMax(a,b)
  br label %4962

; <label>:3816                                    ; preds = %3460
  %3817 = load float, float addrspace(3)* %57, align 4
  %3818 = fcmp fast ogt float %3817, 0.000000e+00
  br i1 %3818, label %3819, label %4485

; <label>:3819                                    ; preds = %3816
  %3820 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 0
  store i32 %1732, i32* %3820, align 4
  %3821 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 1
  store i32 %1733, i32* %3821, align 4
  %3822 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 2
  store i32 %1734, i32* %3822, align 4
  %3823 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 3
  store i32 %1735, i32* %3823, align 4
  %3824 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 4
  store i32 %1736, i32* %3824, align 4
  %3825 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 5
  store i32 %1737, i32* %3825, align 4
  %3826 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 6
  store i32 %1738, i32* %3826, align 4
  %3827 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 7
  store i32 %1739, i32* %3827, align 4
  %3828 = sitofp i32 %2749 to float
  %3829 = call float @dx.op.unary.f32(i32 21, float %3828)  ; Exp(value)
  %3830 = fptosi float %3829 to i32
  %3831 = sitofp i32 %3830 to float
  %3832 = fdiv fast float %2791, %3831
  %3833 = fdiv fast float %2792, %3831
  %3834 = fdiv fast float %2793, %3831
  %3835 = fcmp fast olt float %3832, 0.000000e+00
  %3836 = fcmp fast ogt float %3832, 1.000000e+00
  %3837 = or i1 %3835, %3836
  br i1 %3837, label %3838, label %3842

; <label>:3838                                    ; preds = %3819
  %3839 = fmul fast float %3832, 2.000000e+00
  %3840 = fadd fast float %3839, -1.000000e+00
  %3841 = fdiv fast float %3832, %3840
  br label %3842

; <label>:3842                                    ; preds = %3838, %3819
  %3843 = phi float [ %3841, %3838 ], [ %3832, %3819 ]
  %3844 = fcmp fast olt float %3833, 0.000000e+00
  %3845 = fcmp fast ogt float %3833, 1.000000e+00
  %3846 = or i1 %3844, %3845
  br i1 %3846, label %3847, label %3851

; <label>:3847                                    ; preds = %3842
  %3848 = fmul fast float %3833, 2.000000e+00
  %3849 = fadd fast float %3848, -1.000000e+00
  %3850 = fdiv fast float %3833, %3849
  br label %3851

; <label>:3851                                    ; preds = %3847, %3842
  %3852 = phi float [ %3850, %3847 ], [ %3833, %3842 ]
  %3853 = fcmp fast olt float %3834, 0.000000e+00
  %3854 = fcmp fast ogt float %3834, 1.000000e+00
  %3855 = or i1 %3853, %3854
  br i1 %3855, label %3856, label %3860

; <label>:3856                                    ; preds = %3851
  %3857 = fmul fast float %3834, 2.000000e+00
  %3858 = fadd fast float %3857, -1.000000e+00
  %3859 = fdiv fast float %3834, %3858
  br label %3860

; <label>:3860                                    ; preds = %3856, %3851
  %3861 = phi float [ %3859, %3856 ], [ %3834, %3851 ]
  %3862 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %3863 = extractvalue %dx.types.CBufRet.f32 %3862, 2
  %3864 = mul nsw i32 %2753, 10
  %3865 = add i32 %3864, 9
  %3866 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3865, i32 undef)  ; BufferLoad(srv,index,wot)
  %3867 = extractvalue %dx.types.ResRet.f32 %3866, 0
  %3868 = fptoui float %3867 to i32
  %3869 = icmp eq i32 %3868, 64
  %3870 = select i1 %3869, float 3.000000e+00, float %3863
  %3871 = fmul fast float %3870, %375
  %3872 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 0
  store float 0.000000e+00, float* %3872, align 4, !tbaa !53
  %3873 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 1
  store float 0.000000e+00, float* %3873, align 4, !tbaa !53
  %3874 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 2
  store float 0.000000e+00, float* %3874, align 4, !tbaa !53
  %3875 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 3
  store float 0.000000e+00, float* %3875, align 4, !tbaa !53
  %3876 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 4
  store float 0.000000e+00, float* %3876, align 4, !tbaa !53
  %3877 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 5
  store float 0.000000e+00, float* %3877, align 4, !tbaa !53
  %3878 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 6
  store float 0.000000e+00, float* %3878, align 4, !tbaa !53
  %3879 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 7
  store float 0.000000e+00, float* %3879, align 4, !tbaa !53
  %3880 = sub nsw i32 %2749, %1646
  br label %3881

; <label>:3881                                    ; preds = %4186, %3860
  %3882 = phi i32 [ %2753, %3860 ], [ %4187, %4186 ]
  %3883 = phi float [ %2754, %3860 ], [ %4188, %4186 ]
  %3884 = phi float [ %2755, %3860 ], [ %4189, %4186 ]
  %3885 = phi float [ %3870, %3860 ], [ %4190, %4186 ]
  %3886 = phi float [ %3871, %3860 ], [ %4191, %4186 ]
  %3887 = phi float [ 0.000000e+00, %3860 ], [ %4192, %4186 ]
  %3888 = phi i32 [ 0, %3860 ], [ %4193, %4186 ]
  %3889 = shl i32 %3880, 3
  %3890 = add i32 %3888, %3889
  %3891 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 %3890
  %3892 = load i32, i32* %3891, align 4, !tbaa !34
  %3893 = icmp slt i32 %3892, 0
  br i1 %3893, label %4186, label %3894

; <label>:3894                                    ; preds = %3881
  %3895 = mul i32 %3892, 10
  %3896 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3895, i32 undef)  ; BufferLoad(srv,index,wot)
  %3897 = extractvalue %dx.types.ResRet.f32 %3896, 0
  %3898 = or i32 %3895, 1
  %3899 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3898, i32 undef)  ; BufferLoad(srv,index,wot)
  %3900 = extractvalue %dx.types.ResRet.f32 %3899, 0
  %3901 = add i32 %3895, 2
  %3902 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3901, i32 undef)  ; BufferLoad(srv,index,wot)
  %3903 = extractvalue %dx.types.ResRet.f32 %3902, 0
  %3904 = fsub fast float %2750, %3897
  %3905 = fsub fast float %2751, %3900
  %3906 = fsub fast float %2752, %3903
  %3907 = call float @dx.op.dot3.f32(i32 55, float %3904, float %3905, float %3906, float %3904, float %3905, float %3906)  ; Dot3(ax,ay,az,bx,by,bz)
  %3908 = call float @dx.op.unary.f32(i32 25, float %3907)  ; Rsqrt(value)
  %3909 = fmul fast float %3908, %3904
  %3910 = fmul fast float %3908, %3905
  %3911 = fmul fast float %3906, %3908
  %3912 = call float @dx.op.dot3.f32(i32 55, float %3909, float %3910, float %3911, float %352, float %353, float %354)  ; Dot3(ax,ay,az,bx,by,bz)
  %3913 = fmul fast float %3912, 5.000000e-01
  %3914 = fadd fast float %3913, 5.000000e-01
  %3915 = call float @dx.op.binary.f32(i32 35, float %375, float 0x3F50624DE0000000)  ; FMax(a,b)
  %3916 = fdiv fast float %3914, %3915
  %3917 = call float @dx.op.unary.f32(i32 7, float %3916)  ; Saturate(value)
  %3918 = call float @dx.op.unary.f32(i32 7, float %3913)  ; Saturate(value)
  %3919 = fmul fast float %3918, 2.000000e+00
  %3920 = fmul fast float %3904, %3904
  %3921 = fmul fast float %3905, %3905
  %3922 = fadd fast float %3921, %3920
  %3923 = fmul fast float %3906, %3906
  %3924 = fadd fast float %3922, %3923
  %3925 = call float @dx.op.unary.f32(i32 24, float %3924)  ; Sqrt(value)
  %3926 = call float @dx.op.unary.f32(i32 6, float %3909)  ; FAbs(value)
  %3927 = call float @dx.op.unary.f32(i32 6, float %3910)  ; FAbs(value)
  %3928 = fadd fast float %3927, %3926
  %3929 = call float @dx.op.unary.f32(i32 6, float %3911)  ; FAbs(value)
  %3930 = fadd fast float %3928, %3929
  %3931 = fdiv fast float 1.000000e+00, %3930
  %3932 = fmul fast float %3931, %3909
  %3933 = fmul fast float %3931, %3910
  %3934 = fcmp fast ole float %3911, 0.000000e+00
  %3935 = call float @dx.op.unary.f32(i32 6, float %3933)  ; FAbs(value)
  %3936 = call float @dx.op.unary.f32(i32 6, float %3932)  ; FAbs(value)
  %3937 = fsub fast float 1.000000e+00, %3935
  %3938 = fsub fast float 1.000000e+00, %3936
  %3939 = fcmp fast oge float %3932, 0.000000e+00
  %3940 = select i1 %3939, float 1.000000e+00, float -1.000000e+00
  %3941 = fcmp fast oge float %3933, 0.000000e+00
  %3942 = select i1 %3941, float 1.000000e+00, float -1.000000e+00
  %3943 = fmul fast float %3940, %3937
  %3944 = fmul fast float %3938, %3942
  %3945 = select i1 %3934, float %3943, float %3932
  %3946 = select i1 %3934, float %3944, float %3933
  %3947 = fmul fast float %3945, 5.000000e-01
  %3948 = fmul fast float %3946, 5.000000e-01
  %3949 = fadd fast float %3947, 5.000000e-01
  %3950 = fadd fast float %3948, 5.000000e-01
  %3951 = add i32 %3895, 6
  %3952 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3951, i32 undef)  ; BufferLoad(srv,index,wot)
  %3953 = extractvalue %dx.types.ResRet.f32 %3952, 0
  %3954 = fptoui float %3953 to i32
  %3955 = add i32 %3895, 7
  %3956 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3955, i32 undef)  ; BufferLoad(srv,index,wot)
  %3957 = extractvalue %dx.types.ResRet.f32 %3956, 0
  %3958 = fptoui float %3957 to i32
  %3959 = add i32 %3895, 8
  %3960 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3959, i32 undef)  ; BufferLoad(srv,index,wot)
  %3961 = extractvalue %dx.types.ResRet.f32 %3960, 0
  %3962 = fptoui float %3961 to i32
  %3963 = add i32 %3895, 9
  %3964 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3963, i32 undef)  ; BufferLoad(srv,index,wot)
  %3965 = extractvalue %dx.types.ResRet.f32 %3964, 0
  %3966 = fptoui float %3965 to i32
  %3967 = mul i32 %3966, %3954
  %3968 = mul i32 %3966, %3958
  %3969 = sitofp i32 %3967 to float
  %3970 = sitofp i32 %3968 to float
  %3971 = sitofp i32 %3962 to float
  %3972 = sitofp i32 %3966 to float
  %3973 = fadd fast float %3972, -2.000000e+00
  %3974 = fmul fast float %3973, %3949
  %3975 = fmul fast float %3973, %3950
  %3976 = fadd fast float %3969, 1.000000e+00
  %3977 = fadd fast float %3976, %3974
  %3978 = fadd fast float %3970, 1.000000e+00
  %3979 = fadd fast float %3978, %3975
  %3980 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %3981 = extractvalue %dx.types.CBufRet.f32 %3980, 0
  %3982 = extractvalue %dx.types.CBufRet.f32 %3980, 1
  %3983 = extractvalue %dx.types.CBufRet.f32 %3980, 3
  %3984 = fmul fast float %3983, %3981
  %3985 = fmul fast float %3983, %3982
  %3986 = fmul fast float %3984, %3977
  %3987 = fmul fast float %3985, %3979
  %3988 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %3989 = extractvalue %dx.types.CBufRet.f32 %3988, 0
  %3990 = fadd fast float %3989, %3971
  %3991 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %3986, float %3987, float %3990, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %3992 = extractvalue %dx.types.ResRet.f32 %3991, 0
  %3993 = extractvalue %dx.types.ResRet.f32 %3991, 2
  %3994 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %3995 = extractvalue %dx.types.CBufRet.f32 %3994, 1
  %3996 = fadd fast float %3995, %3925
  %3997 = extractvalue %dx.types.CBufRet.f32 %3994, 0
  %3998 = fdiv fast float %3996, %3997
  %3999 = fcmp fast ogt float %3998, %3992
  br i1 %3999, label %4000, label %4009

; <label>:4000                                    ; preds = %3894
  %4001 = extractvalue %dx.types.ResRet.f32 %3991, 1
  %4002 = fmul fast float %3992, %3992
  %4003 = fsub fast float %4002, %4001
  %4004 = call float @dx.op.unary.f32(i32 6, float %4003)  ; FAbs(value)
  %4005 = fsub fast float %3998, %3992
  %4006 = fmul fast float %4005, %4005
  %4007 = fadd fast float %4004, %4006
  %4008 = fdiv fast float %4004, %4007
  br label %4009

; <label>:4009                                    ; preds = %4000, %3894
  %4010 = phi float [ %4008, %4000 ], [ 1.000000e+00, %3894 ]
  %4011 = fsub fast float %3993, %3998
  %4012 = fcmp fast ogt float %4011, 0.000000e+00
  %4013 = select i1 %4012, float 1.000000e+00, float 0.000000e+00
  %4014 = fmul fast float %3919, %4013
  %4015 = call float @dx.op.binary.f32(i32 35, float %4014, float %4010)  ; FMax(a,b)
  %4016 = fmul fast float %4015, %3917
  %4017 = fsub fast float %4016, %4014
  %4018 = fmul fast float %4017, %375
  %4019 = fadd fast float %4018, %4014
  %4020 = fsub fast float %372, %3897
  %4021 = fsub fast float %373, %3900
  %4022 = fsub fast float %374, %3903
  %4023 = call float @dx.op.dot3.f32(i32 55, float %4020, float %4021, float %4022, float %4020, float %4021, float %4022)  ; Dot3(ax,ay,az,bx,by,bz)
  %4024 = call float @dx.op.unary.f32(i32 25, float %4023)  ; Rsqrt(value)
  %4025 = fmul fast float %4024, %4020
  %4026 = fmul fast float %4024, %4021
  %4027 = fmul fast float %4024, %4022
  %4028 = fmul fast float %4020, %4020
  %4029 = fmul fast float %4021, %4021
  %4030 = fadd fast float %4029, %4028
  %4031 = fmul fast float %4022, %4022
  %4032 = fadd fast float %4030, %4031
  %4033 = call float @dx.op.unary.f32(i32 24, float %4032)  ; Sqrt(value)
  %4034 = call float @dx.op.unary.f32(i32 6, float %4025)  ; FAbs(value)
  %4035 = call float @dx.op.unary.f32(i32 6, float %4026)  ; FAbs(value)
  %4036 = fadd fast float %4035, %4034
  %4037 = call float @dx.op.unary.f32(i32 6, float %4027)  ; FAbs(value)
  %4038 = fadd fast float %4036, %4037
  %4039 = fdiv fast float 1.000000e+00, %4038
  %4040 = fmul fast float %4039, %4025
  %4041 = fmul fast float %4039, %4026
  %4042 = fcmp fast ole float %4027, 0.000000e+00
  %4043 = call float @dx.op.unary.f32(i32 6, float %4041)  ; FAbs(value)
  %4044 = call float @dx.op.unary.f32(i32 6, float %4040)  ; FAbs(value)
  %4045 = fsub fast float 1.000000e+00, %4043
  %4046 = fsub fast float 1.000000e+00, %4044
  %4047 = fcmp fast oge float %4040, 0.000000e+00
  %4048 = select i1 %4047, float 1.000000e+00, float -1.000000e+00
  %4049 = fcmp fast oge float %4041, 0.000000e+00
  %4050 = select i1 %4049, float 1.000000e+00, float -1.000000e+00
  %4051 = fmul fast float %4048, %4045
  %4052 = fmul fast float %4046, %4050
  %4053 = select i1 %4042, float %4051, float %4040
  %4054 = select i1 %4042, float %4052, float %4041
  %4055 = fmul fast float %4053, 5.000000e-01
  %4056 = fmul fast float %4054, 5.000000e-01
  %4057 = fadd fast float %4055, 5.000000e-01
  %4058 = fadd fast float %4056, 5.000000e-01
  %4059 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3951, i32 undef)  ; BufferLoad(srv,index,wot)
  %4060 = extractvalue %dx.types.ResRet.f32 %4059, 0
  %4061 = fptoui float %4060 to i32
  %4062 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3955, i32 undef)  ; BufferLoad(srv,index,wot)
  %4063 = extractvalue %dx.types.ResRet.f32 %4062, 0
  %4064 = fptoui float %4063 to i32
  %4065 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3959, i32 undef)  ; BufferLoad(srv,index,wot)
  %4066 = extractvalue %dx.types.ResRet.f32 %4065, 0
  %4067 = fptoui float %4066 to i32
  %4068 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %3963, i32 undef)  ; BufferLoad(srv,index,wot)
  %4069 = extractvalue %dx.types.ResRet.f32 %4068, 0
  %4070 = fptoui float %4069 to i32
  %4071 = mul i32 %4070, %4061
  %4072 = mul i32 %4070, %4064
  %4073 = sitofp i32 %4071 to float
  %4074 = sitofp i32 %4072 to float
  %4075 = sitofp i32 %4067 to float
  %4076 = sitofp i32 %4070 to float
  %4077 = fadd fast float %4076, -2.000000e+00
  %4078 = fmul fast float %4077, %4057
  %4079 = fmul fast float %4077, %4058
  %4080 = fadd fast float %4073, 1.000000e+00
  %4081 = fadd fast float %4080, %4078
  %4082 = fadd fast float %4074, 1.000000e+00
  %4083 = fadd fast float %4082, %4079
  %4084 = fmul fast float %3984, %4081
  %4085 = fmul fast float %3985, %4083
  %4086 = fadd fast float %3989, %4075
  %4087 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %4084, float %4085, float %4086, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4088 = extractvalue %dx.types.ResRet.f32 %4087, 0
  %4089 = extractvalue %dx.types.ResRet.f32 %4087, 2
  %4090 = fadd fast float %3995, %4033
  %4091 = fdiv fast float %4090, %3997
  %4092 = fcmp fast ogt float %4091, %4088
  br i1 %4092, label %4093, label %4102

; <label>:4093                                    ; preds = %4009
  %4094 = extractvalue %dx.types.ResRet.f32 %4087, 1
  %4095 = fmul fast float %4088, %4088
  %4096 = fsub fast float %4095, %4094
  %4097 = call float @dx.op.unary.f32(i32 6, float %4096)  ; FAbs(value)
  %4098 = fsub fast float %4091, %4088
  %4099 = fmul fast float %4098, %4098
  %4100 = fadd fast float %4097, %4099
  %4101 = fdiv fast float %4097, %4100
  br label %4102

; <label>:4102                                    ; preds = %4093, %4009
  %4103 = phi float [ %4101, %4093 ], [ 1.000000e+00, %4009 ]
  %4104 = fsub fast float %4089, %4091
  %4105 = fcmp fast ogt float %4104, 0.000000e+00
  %4106 = select i1 %4105, float 1.000000e+00, float 0.000000e+00
  %4107 = call float @dx.op.binary.f32(i32 35, float %4106, float %4103)  ; FMax(a,b)
  %4108 = fadd fast float %4106, %4014
  %4109 = fcmp fast olt float %4108, 0x3F50624DE0000000
  br i1 %4109, label %4186, label %4110

; <label>:4110                                    ; preds = %4102
  %4111 = fsub fast float %4106, %4107
  %4112 = fmul fast float %4111, %375
  %4113 = fadd fast float %4112, %4107
  %4114 = and i32 %3888, 1
  %4115 = lshr i32 %3888, 1
  %4116 = and i32 %4115, 1
  %4117 = lshr i32 %3888, 2
  %4118 = and i32 %4117, 1
  %4119 = sitofp i32 %4114 to float
  %4120 = sitofp i32 %4116 to float
  %4121 = sitofp i32 %4118 to float
  %4122 = fsub fast float 1.000000e+00, %3843
  %4123 = fsub fast float 1.000000e+00, %3852
  %4124 = fsub fast float 1.000000e+00, %3861
  %4125 = fsub fast float %3843, %4122
  %4126 = fsub fast float %3852, %4123
  %4127 = fsub fast float %3861, %4124
  %4128 = fmul fast float %4119, %4125
  %4129 = fmul fast float %4120, %4126
  %4130 = fmul fast float %4121, %4127
  %4131 = fadd fast float %4128, %4122
  %4132 = fadd fast float %4129, %4123
  %4133 = fadd fast float %4130, %4124
  %4134 = fmul fast float %4132, %4131
  %4135 = fmul fast float %4134, %4133
  %4136 = call float @dx.op.binary.f32(i32 35, float %4135, float 0x3F847AE140000000)  ; FMax(a,b)
  %4137 = fmul fast float %4113, %4019
  %4138 = fmul fast float %4137, %4136
  %4139 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 %3888
  store float %4138, float* %4139, align 4, !tbaa !53
  %4140 = fmul fast float %4106, %4014
  %4141 = fmul fast float %4140, %4136
  %4142 = fcmp fast ogt float %4141, %3887
  br i1 %4142, label %4143, label %4186

; <label>:4143                                    ; preds = %4110
  %4144 = fsub fast float %352, %3909
  %4145 = fsub fast float %353, %3910
  %4146 = fsub fast float %354, %3911
  %4147 = fmul fast float %4144, %375
  %4148 = fmul fast float %4145, %375
  %4149 = fmul fast float %4146, %375
  %4150 = fadd fast float %4147, %3909
  %4151 = fadd fast float %4148, %3910
  %4152 = fadd fast float %4149, %3911
  %4153 = call float @dx.op.dot3.f32(i32 55, float %4150, float %4151, float %4152, float %4150, float %4151, float %4152)  ; Dot3(ax,ay,az,bx,by,bz)
  %4154 = call float @dx.op.unary.f32(i32 25, float %4153)  ; Rsqrt(value)
  %4155 = fmul fast float %4154, %4150
  %4156 = fmul fast float %4154, %4151
  %4157 = fmul fast float %4154, %4152
  %4158 = call float @dx.op.unary.f32(i32 6, float %4155)  ; FAbs(value)
  %4159 = call float @dx.op.unary.f32(i32 6, float %4156)  ; FAbs(value)
  %4160 = fadd fast float %4159, %4158
  %4161 = call float @dx.op.unary.f32(i32 6, float %4157)  ; FAbs(value)
  %4162 = fadd fast float %4160, %4161
  %4163 = fdiv fast float 1.000000e+00, %4162
  %4164 = fmul fast float %4163, %4155
  %4165 = fmul fast float %4163, %4156
  %4166 = fcmp fast ole float %4157, 0.000000e+00
  %4167 = call float @dx.op.unary.f32(i32 6, float %4165)  ; FAbs(value)
  %4168 = call float @dx.op.unary.f32(i32 6, float %4164)  ; FAbs(value)
  %4169 = fsub fast float 1.000000e+00, %4167
  %4170 = fsub fast float 1.000000e+00, %4168
  %4171 = fcmp fast oge float %4164, 0.000000e+00
  %4172 = select i1 %4171, float 1.000000e+00, float -1.000000e+00
  %4173 = fcmp fast oge float %4165, 0.000000e+00
  %4174 = select i1 %4173, float 1.000000e+00, float -1.000000e+00
  %4175 = fmul fast float %4172, %4169
  %4176 = fmul fast float %4170, %4174
  %4177 = select i1 %4166, float %4175, float %4164
  %4178 = select i1 %4166, float %4176, float %4165
  %4179 = fmul fast float %4177, 5.000000e-01
  %4180 = fmul fast float %4178, 5.000000e-01
  %4181 = fadd fast float %4179, 5.000000e-01
  %4182 = fadd fast float %4180, 5.000000e-01
  %4183 = icmp eq i32 %4070, 64
  %4184 = select i1 %4183, float 3.000000e+00, float %3863
  %4185 = fmul fast float %4184, %375
  br label %4186

; <label>:4186                                    ; preds = %4143, %4110, %4102, %3881
  %4187 = phi i32 [ %3882, %3881 ], [ %3882, %4102 ], [ %3892, %4143 ], [ %3882, %4110 ]
  %4188 = phi float [ %3883, %3881 ], [ %3883, %4102 ], [ %4181, %4143 ], [ %3883, %4110 ]
  %4189 = phi float [ %3884, %3881 ], [ %3884, %4102 ], [ %4182, %4143 ], [ %3884, %4110 ]
  %4190 = phi float [ %3885, %3881 ], [ %3885, %4102 ], [ %4184, %4143 ], [ %3885, %4110 ]
  %4191 = phi float [ %3886, %3881 ], [ %3886, %4102 ], [ %4185, %4143 ], [ %3886, %4110 ]
  %4192 = phi float [ %3887, %3881 ], [ %3887, %4102 ], [ %4141, %4143 ], [ %3887, %4110 ]
  %4193 = add nuw nsw i32 %3888, 1
  %4194 = icmp eq i32 %4193, 8
  br i1 %4194, label %4195, label %3881

; <label>:4195                                    ; preds = %4186
  %4196 = fptosi float %4190 to i32
  %4197 = mul nsw i32 %4187, 10
  %4198 = add i32 %4197, 6
  %4199 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4198, i32 undef)  ; BufferLoad(srv,index,wot)
  %4200 = extractvalue %dx.types.ResRet.f32 %4199, 0
  %4201 = fptoui float %4200 to i32
  %4202 = add i32 %4197, 7
  %4203 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4202, i32 undef)  ; BufferLoad(srv,index,wot)
  %4204 = extractvalue %dx.types.ResRet.f32 %4203, 0
  %4205 = fptoui float %4204 to i32
  %4206 = add i32 %4197, 8
  %4207 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4206, i32 undef)  ; BufferLoad(srv,index,wot)
  %4208 = extractvalue %dx.types.ResRet.f32 %4207, 0
  %4209 = fptoui float %4208 to i32
  %4210 = add i32 %4197, 9
  %4211 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4210, i32 undef)  ; BufferLoad(srv,index,wot)
  %4212 = extractvalue %dx.types.ResRet.f32 %4211, 0
  %4213 = fptoui float %4212 to i32
  %4214 = mul i32 %4213, %4201
  %4215 = mul i32 %4213, %4205
  %4216 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %4217 = extractvalue %dx.types.CBufRet.f32 %4216, 0
  %4218 = extractvalue %dx.types.CBufRet.f32 %4216, 1
  %4219 = extractvalue %dx.types.CBufRet.f32 %4216, 3
  %4220 = fmul fast float %4219, %4217
  %4221 = fmul fast float %4219, %4218
  %4222 = call float @dx.op.unary.f32(i32 29, float %4191)  ; Round_z(value)
  %4223 = fsub fast float %4191, %4222
  %4224 = fptosi float %4222 to i32
  %4225 = call i32 @dx.op.binary.i32(i32 37, i32 %4224, i32 0)  ; IMax(a,b)
  %4226 = add nsw i32 %4225, 1
  %4227 = call i32 @dx.op.binary.i32(i32 38, i32 %4226, i32 %4196)  ; IMin(a,b)
  %4228 = sitofp i32 %4225 to float
  %4229 = call float @dx.op.unary.f32(i32 21, float %4228)  ; Exp(value)
  %4230 = fdiv fast float 1.000000e+00, %4229
  %4231 = sitofp i32 %4214 to float
  %4232 = sitofp i32 %4215 to float
  %4233 = fmul fast float %4230, %4231
  %4234 = fmul fast float %4230, %4232
  %4235 = fptosi float %4233 to i32
  %4236 = fptosi float %4234 to i32
  %4237 = sitofp i32 %4213 to float
  %4238 = fmul fast float %4230, %4237
  %4239 = fadd fast float %4238, -2.000000e+00
  %4240 = sitofp i32 %4209 to float
  %4241 = sitofp i32 %4235 to float
  %4242 = sitofp i32 %4236 to float
  %4243 = fmul fast float %4239, %4188
  %4244 = fmul fast float %4239, %4189
  %4245 = fadd fast float %4243, 1.000000e+00
  %4246 = fadd fast float %4244, 1.000000e+00
  %4247 = fadd fast float %4245, %4241
  %4248 = fadd fast float %4246, %4242
  %4249 = fmul fast float %4230, %4220
  %4250 = fmul fast float %4230, %4221
  %4251 = fdiv fast float %4247, %4249
  %4252 = fdiv fast float %4248, %4250
  %4253 = sitofp i32 %4227 to float
  %4254 = call float @dx.op.unary.f32(i32 21, float %4253)  ; Exp(value)
  %4255 = fdiv fast float 1.000000e+00, %4254
  %4256 = fmul fast float %4255, %4231
  %4257 = fmul fast float %4255, %4232
  %4258 = fptosi float %4256 to i32
  %4259 = fptosi float %4257 to i32
  %4260 = fmul fast float %4255, %4237
  %4261 = fadd fast float %4260, -2.000000e+00
  %4262 = sitofp i32 %4258 to float
  %4263 = sitofp i32 %4259 to float
  %4264 = fmul fast float %4261, %4188
  %4265 = fmul fast float %4261, %4189
  %4266 = fadd fast float %4264, 1.000000e+00
  %4267 = fadd fast float %4265, 1.000000e+00
  %4268 = fadd fast float %4266, %4262
  %4269 = fadd fast float %4267, %4263
  %4270 = fmul fast float %4255, %4220
  %4271 = fmul fast float %4255, %4221
  %4272 = fdiv fast float %4268, %4270
  %4273 = fdiv fast float %4269, %4271
  %4274 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4251, float %4252, float %4240, float undef, i32 0, i32 0, i32 undef, float %4228)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4275 = extractvalue %dx.types.ResRet.f32 %4274, 0
  %4276 = extractvalue %dx.types.ResRet.f32 %4274, 1
  %4277 = extractvalue %dx.types.ResRet.f32 %4274, 2
  %4278 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4272, float %4273, float %4240, float undef, i32 0, i32 0, i32 undef, float %4253)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4279 = extractvalue %dx.types.ResRet.f32 %4278, 0
  %4280 = extractvalue %dx.types.ResRet.f32 %4278, 1
  %4281 = extractvalue %dx.types.ResRet.f32 %4278, 2
  %4282 = fsub fast float %4279, %4275
  %4283 = fsub fast float %4280, %4276
  %4284 = fsub fast float %4281, %4277
  %4285 = fmul fast float %4282, %4223
  %4286 = fmul fast float %4283, %4223
  %4287 = fmul fast float %4284, %4223
  %4288 = fadd fast float %4285, %4275
  %4289 = fadd fast float %4286, %4276
  %4290 = fadd fast float %4287, %4277
  br label %4291

; <label>:4291                                    ; preds = %4472, %4195
  %4292 = phi float [ 0.000000e+00, %4195 ], [ %4473, %4472 ]
  %4293 = phi float [ 0.000000e+00, %4195 ], [ %4474, %4472 ]
  %4294 = phi float [ 0.000000e+00, %4195 ], [ %4475, %4472 ]
  %4295 = phi float [ 0.000000e+00, %4195 ], [ %4476, %4472 ]
  %4296 = phi i32 [ 0, %4195 ], [ %4477, %4472 ]
  %4297 = getelementptr inbounds [8 x float], [8 x float]* %31, i32 0, i32 %4296
  %4298 = load float, float* %4297, align 4, !tbaa !53
  %4299 = fcmp fast oeq float %4298, 0.000000e+00
  br i1 %4299, label %4472, label %4300

; <label>:4300                                    ; preds = %4291
  %4301 = add i32 %4296, %3889
  %4302 = getelementptr inbounds [8 x i32], [8 x i32]* %38, i32 0, i32 %4301
  %4303 = load i32, i32* %4302, align 4, !tbaa !34
  %4304 = icmp slt i32 %4303, 0
  br i1 %4304, label %4472, label %4305

; <label>:4305                                    ; preds = %4300
  %4306 = mul i32 %4303, 10
  %4307 = icmp eq i32 %4187, %4303
  br i1 %4307, label %4461, label %4308

; <label>:4308                                    ; preds = %4305
  %4309 = add i32 %4306, 2
  %4310 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4309, i32 undef)  ; BufferLoad(srv,index,wot)
  %4311 = extractvalue %dx.types.ResRet.f32 %4310, 0
  %4312 = or i32 %4306, 1
  %4313 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4312, i32 undef)  ; BufferLoad(srv,index,wot)
  %4314 = extractvalue %dx.types.ResRet.f32 %4313, 0
  %4315 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4306, i32 undef)  ; BufferLoad(srv,index,wot)
  %4316 = extractvalue %dx.types.ResRet.f32 %4315, 0
  %4317 = fsub fast float %2750, %4316
  %4318 = fsub fast float %2751, %4314
  %4319 = fsub fast float %2752, %4311
  %4320 = call float @dx.op.dot3.f32(i32 55, float %4317, float %4318, float %4319, float %4317, float %4318, float %4319)  ; Dot3(ax,ay,az,bx,by,bz)
  %4321 = call float @dx.op.unary.f32(i32 25, float %4320)  ; Rsqrt(value)
  %4322 = fmul fast float %4317, %4321
  %4323 = fmul fast float %4321, %4318
  %4324 = fmul fast float %4321, %4319
  %4325 = fsub fast float %352, %4322
  %4326 = fsub fast float %353, %4323
  %4327 = fsub fast float %354, %4324
  %4328 = fmul fast float %4325, %375
  %4329 = fmul fast float %4326, %375
  %4330 = fmul fast float %4327, %375
  %4331 = fadd fast float %4328, %4322
  %4332 = fadd fast float %4329, %4323
  %4333 = fadd fast float %4330, %4324
  %4334 = call float @dx.op.dot3.f32(i32 55, float %4331, float %4332, float %4333, float %4331, float %4332, float %4333)  ; Dot3(ax,ay,az,bx,by,bz)
  %4335 = call float @dx.op.unary.f32(i32 25, float %4334)  ; Rsqrt(value)
  %4336 = fmul fast float %4331, %4335
  %4337 = fmul fast float %4332, %4335
  %4338 = fmul fast float %4333, %4335
  %4339 = add i32 %4306, 9
  %4340 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4339, i32 undef)  ; BufferLoad(srv,index,wot)
  %4341 = extractvalue %dx.types.ResRet.f32 %4340, 0
  %4342 = fptoui float %4341 to i32
  %4343 = icmp eq i32 %4342, 64
  %4344 = select i1 %4343, float 3.000000e+00, float %3863
  %4345 = fmul fast float %4344, %375
  %4346 = call float @dx.op.unary.f32(i32 6, float %4336)  ; FAbs(value)
  %4347 = call float @dx.op.unary.f32(i32 6, float %4337)  ; FAbs(value)
  %4348 = fadd fast float %4347, %4346
  %4349 = call float @dx.op.unary.f32(i32 6, float %4338)  ; FAbs(value)
  %4350 = fadd fast float %4348, %4349
  %4351 = fdiv fast float 1.000000e+00, %4350
  %4352 = fmul fast float %4351, %4336
  %4353 = fmul fast float %4351, %4337
  %4354 = fcmp fast ole float %4338, 0.000000e+00
  %4355 = call float @dx.op.unary.f32(i32 6, float %4353)  ; FAbs(value)
  %4356 = call float @dx.op.unary.f32(i32 6, float %4352)  ; FAbs(value)
  %4357 = fsub fast float 1.000000e+00, %4355
  %4358 = fsub fast float 1.000000e+00, %4356
  %4359 = fcmp fast oge float %4352, 0.000000e+00
  %4360 = select i1 %4359, float 1.000000e+00, float -1.000000e+00
  %4361 = fcmp fast oge float %4353, 0.000000e+00
  %4362 = select i1 %4361, float 1.000000e+00, float -1.000000e+00
  %4363 = fmul fast float %4360, %4357
  %4364 = fmul fast float %4358, %4362
  %4365 = select i1 %4354, float %4363, float %4352
  %4366 = select i1 %4354, float %4364, float %4353
  %4367 = fmul fast float %4365, 5.000000e-01
  %4368 = fmul fast float %4366, 5.000000e-01
  %4369 = fadd fast float %4367, 5.000000e-01
  %4370 = fadd fast float %4368, 5.000000e-01
  %4371 = fptosi float %4344 to i32
  %4372 = add i32 %4306, 6
  %4373 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4372, i32 undef)  ; BufferLoad(srv,index,wot)
  %4374 = extractvalue %dx.types.ResRet.f32 %4373, 0
  %4375 = fptoui float %4374 to i32
  %4376 = add i32 %4306, 7
  %4377 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4376, i32 undef)  ; BufferLoad(srv,index,wot)
  %4378 = extractvalue %dx.types.ResRet.f32 %4377, 0
  %4379 = fptoui float %4378 to i32
  %4380 = add i32 %4306, 8
  %4381 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4380, i32 undef)  ; BufferLoad(srv,index,wot)
  %4382 = extractvalue %dx.types.ResRet.f32 %4381, 0
  %4383 = fptoui float %4382 to i32
  %4384 = mul i32 %4342, %4375
  %4385 = mul i32 %4342, %4379
  %4386 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %4387 = extractvalue %dx.types.CBufRet.f32 %4386, 0
  %4388 = extractvalue %dx.types.CBufRet.f32 %4386, 1
  %4389 = extractvalue %dx.types.CBufRet.f32 %4386, 3
  %4390 = fmul fast float %4389, %4387
  %4391 = fmul fast float %4389, %4388
  %4392 = call float @dx.op.unary.f32(i32 29, float %4345)  ; Round_z(value)
  %4393 = fsub fast float %4345, %4392
  %4394 = fptosi float %4392 to i32
  %4395 = call i32 @dx.op.binary.i32(i32 37, i32 %4394, i32 0)  ; IMax(a,b)
  %4396 = add nsw i32 %4395, 1
  %4397 = call i32 @dx.op.binary.i32(i32 38, i32 %4396, i32 %4371)  ; IMin(a,b)
  %4398 = sitofp i32 %4395 to float
  %4399 = call float @dx.op.unary.f32(i32 21, float %4398)  ; Exp(value)
  %4400 = fdiv fast float 1.000000e+00, %4399
  %4401 = sitofp i32 %4384 to float
  %4402 = sitofp i32 %4385 to float
  %4403 = fmul fast float %4400, %4401
  %4404 = fmul fast float %4400, %4402
  %4405 = fptosi float %4403 to i32
  %4406 = fptosi float %4404 to i32
  %4407 = sitofp i32 %4342 to float
  %4408 = fmul fast float %4400, %4407
  %4409 = fadd fast float %4408, -2.000000e+00
  %4410 = sitofp i32 %4383 to float
  %4411 = sitofp i32 %4405 to float
  %4412 = sitofp i32 %4406 to float
  %4413 = fmul fast float %4409, %4369
  %4414 = fmul fast float %4409, %4370
  %4415 = fadd fast float %4413, 1.000000e+00
  %4416 = fadd fast float %4414, 1.000000e+00
  %4417 = fadd fast float %4415, %4411
  %4418 = fadd fast float %4416, %4412
  %4419 = fmul fast float %4400, %4390
  %4420 = fmul fast float %4400, %4391
  %4421 = fdiv fast float %4417, %4419
  %4422 = fdiv fast float %4418, %4420
  %4423 = sitofp i32 %4397 to float
  %4424 = call float @dx.op.unary.f32(i32 21, float %4423)  ; Exp(value)
  %4425 = fdiv fast float 1.000000e+00, %4424
  %4426 = fmul fast float %4425, %4401
  %4427 = fmul fast float %4425, %4402
  %4428 = fptosi float %4426 to i32
  %4429 = fptosi float %4427 to i32
  %4430 = fmul fast float %4425, %4407
  %4431 = fadd fast float %4430, -2.000000e+00
  %4432 = sitofp i32 %4428 to float
  %4433 = sitofp i32 %4429 to float
  %4434 = fmul fast float %4431, %4369
  %4435 = fmul fast float %4431, %4370
  %4436 = fadd fast float %4434, 1.000000e+00
  %4437 = fadd fast float %4435, 1.000000e+00
  %4438 = fadd fast float %4436, %4432
  %4439 = fadd fast float %4437, %4433
  %4440 = fmul fast float %4425, %4390
  %4441 = fmul fast float %4425, %4391
  %4442 = fdiv fast float %4438, %4440
  %4443 = fdiv fast float %4439, %4441
  %4444 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4421, float %4422, float %4410, float undef, i32 0, i32 0, i32 undef, float %4398)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4445 = extractvalue %dx.types.ResRet.f32 %4444, 0
  %4446 = extractvalue %dx.types.ResRet.f32 %4444, 1
  %4447 = extractvalue %dx.types.ResRet.f32 %4444, 2
  %4448 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4442, float %4443, float %4410, float undef, i32 0, i32 0, i32 undef, float %4423)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4449 = extractvalue %dx.types.ResRet.f32 %4448, 0
  %4450 = extractvalue %dx.types.ResRet.f32 %4448, 1
  %4451 = extractvalue %dx.types.ResRet.f32 %4448, 2
  %4452 = fsub fast float %4449, %4445
  %4453 = fsub fast float %4450, %4446
  %4454 = fsub fast float %4451, %4447
  %4455 = fmul fast float %4452, %4393
  %4456 = fmul fast float %4453, %4393
  %4457 = fmul fast float %4454, %4393
  %4458 = fadd fast float %4455, %4445
  %4459 = fadd fast float %4456, %4446
  %4460 = fadd fast float %4457, %4447
  br label %4461

; <label>:4461                                    ; preds = %4308, %4305
  %4462 = phi float [ %4458, %4308 ], [ %4288, %4305 ]
  %4463 = phi float [ %4459, %4308 ], [ %4289, %4305 ]
  %4464 = phi float [ %4460, %4308 ], [ %4290, %4305 ]
  %4465 = fmul fast float %4462, %4298
  %4466 = fmul fast float %4463, %4298
  %4467 = fmul fast float %4464, %4298
  %4468 = fadd fast float %4465, %4292
  %4469 = fadd fast float %4466, %4293
  %4470 = fadd fast float %4467, %4294
  %4471 = fadd fast float %4298, %4295
  br label %4472

; <label>:4472                                    ; preds = %4461, %4300, %4291
  %4473 = phi float [ %4292, %4291 ], [ %4292, %4300 ], [ %4468, %4461 ]
  %4474 = phi float [ %4293, %4291 ], [ %4293, %4300 ], [ %4469, %4461 ]
  %4475 = phi float [ %4294, %4291 ], [ %4294, %4300 ], [ %4470, %4461 ]
  %4476 = phi float [ %4295, %4291 ], [ %4295, %4300 ], [ %4471, %4461 ]
  %4477 = add nuw nsw i32 %4296, 1
  %4478 = icmp eq i32 %4477, 8
  br i1 %4478, label %4479, label %4291

; <label>:4479                                    ; preds = %4472
  %4480 = fcmp fast ogt float %4476, 0.000000e+00
  br i1 %4480, label %4481, label %4962

; <label>:4481                                    ; preds = %4479
  %4482 = fdiv fast float %4473, %4476
  %4483 = fdiv fast float %4474, %4476
  %4484 = fdiv fast float %4475, %4476
  br label %4962

; <label>:4485                                    ; preds = %3816
  %4486 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 0
  store i32 %1732, i32* %4486, align 4
  %4487 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 1
  store i32 %1733, i32* %4487, align 4
  %4488 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 2
  store i32 %1734, i32* %4488, align 4
  %4489 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 3
  store i32 %1735, i32* %4489, align 4
  %4490 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 4
  store i32 %1736, i32* %4490, align 4
  %4491 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 5
  store i32 %1737, i32* %4491, align 4
  %4492 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 6
  store i32 %1738, i32* %4492, align 4
  %4493 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 7
  store i32 %1739, i32* %4493, align 4
  %4494 = fdiv fast float %2791, %1758
  %4495 = fdiv fast float %2792, %1758
  %4496 = fdiv fast float %2793, %1758
  %4497 = fcmp fast olt float %4494, 0.000000e+00
  %4498 = fcmp fast ogt float %4494, 1.000000e+00
  %4499 = or i1 %4497, %4498
  br i1 %4499, label %4500, label %4504

; <label>:4500                                    ; preds = %4485
  %4501 = fmul fast float %4494, 2.000000e+00
  %4502 = fadd fast float %4501, -1.000000e+00
  %4503 = fdiv fast float %4494, %4502
  br label %4504

; <label>:4504                                    ; preds = %4500, %4485
  %4505 = phi float [ %4503, %4500 ], [ %4494, %4485 ]
  %4506 = fcmp fast olt float %4495, 0.000000e+00
  %4507 = fcmp fast ogt float %4495, 1.000000e+00
  %4508 = or i1 %4506, %4507
  br i1 %4508, label %4509, label %4513

; <label>:4509                                    ; preds = %4504
  %4510 = fmul fast float %4495, 2.000000e+00
  %4511 = fadd fast float %4510, -1.000000e+00
  %4512 = fdiv fast float %4495, %4511
  br label %4513

; <label>:4513                                    ; preds = %4509, %4504
  %4514 = phi float [ %4512, %4509 ], [ %4495, %4504 ]
  %4515 = fcmp fast olt float %4496, 0.000000e+00
  %4516 = fcmp fast ogt float %4496, 1.000000e+00
  %4517 = or i1 %4515, %4516
  br i1 %4517, label %4518, label %4522

; <label>:4518                                    ; preds = %4513
  %4519 = fmul fast float %4496, 2.000000e+00
  %4520 = fadd fast float %4519, -1.000000e+00
  %4521 = fdiv fast float %4496, %4520
  br label %4522

; <label>:4522                                    ; preds = %4518, %4513
  %4523 = phi float [ %4521, %4518 ], [ %4496, %4513 ]
  br label %4524

; <label>:4524                                    ; preds = %4740, %4522
  %4525 = phi i32 [ %1732, %4522 ], [ %4742, %4740 ]
  %4526 = phi float [ 0.000000e+00, %4522 ], [ %4734, %4740 ]
  %4527 = phi float [ 0.000000e+00, %4522 ], [ %4735, %4740 ]
  %4528 = phi float [ 0.000000e+00, %4522 ], [ %4736, %4740 ]
  %4529 = phi float [ 0.000000e+00, %4522 ], [ %4737, %4740 ]
  %4530 = phi i32 [ 0, %4522 ], [ %4738, %4740 ]
  %4531 = getelementptr inbounds [8 x float], [8 x float]* %30, i32 0, i32 %4530
  store float 0.000000e+00, float* %4531, align 4, !tbaa !53
  %4532 = icmp slt i32 %4525, 0
  br i1 %4532, label %4733, label %4533

; <label>:4533                                    ; preds = %4524
  %4534 = mul i32 %4525, 10
  %4535 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4534, i32 undef)  ; BufferLoad(srv,index,wot)
  %4536 = extractvalue %dx.types.ResRet.f32 %4535, 0
  %4537 = or i32 %4534, 1
  %4538 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4537, i32 undef)  ; BufferLoad(srv,index,wot)
  %4539 = extractvalue %dx.types.ResRet.f32 %4538, 0
  %4540 = add i32 %4534, 2
  %4541 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4540, i32 undef)  ; BufferLoad(srv,index,wot)
  %4542 = extractvalue %dx.types.ResRet.f32 %4541, 0
  %4543 = fsub fast float %372, %4536
  %4544 = fsub fast float %373, %4539
  %4545 = fsub fast float %374, %4542
  %4546 = call float @dx.op.dot3.f32(i32 55, float %4543, float %4544, float %4545, float %4543, float %4544, float %4545)  ; Dot3(ax,ay,az,bx,by,bz)
  %4547 = call float @dx.op.unary.f32(i32 25, float %4546)  ; Rsqrt(value)
  %4548 = fmul fast float %4547, %4543
  %4549 = fmul fast float %4547, %4544
  %4550 = fmul fast float %4545, %4547
  %4551 = fmul fast float %4543, %4543
  %4552 = fmul fast float %4544, %4544
  %4553 = fadd fast float %4552, %4551
  %4554 = fmul fast float %4545, %4545
  %4555 = fadd fast float %4553, %4554
  %4556 = call float @dx.op.unary.f32(i32 24, float %4555)  ; Sqrt(value)
  %4557 = call float @dx.op.unary.f32(i32 6, float %4548)  ; FAbs(value)
  %4558 = call float @dx.op.unary.f32(i32 6, float %4549)  ; FAbs(value)
  %4559 = fadd fast float %4558, %4557
  %4560 = call float @dx.op.unary.f32(i32 6, float %4550)  ; FAbs(value)
  %4561 = fadd fast float %4559, %4560
  %4562 = fdiv fast float 1.000000e+00, %4561
  %4563 = fmul fast float %4562, %4548
  %4564 = fmul fast float %4562, %4549
  %4565 = fcmp fast ole float %4550, 0.000000e+00
  %4566 = call float @dx.op.unary.f32(i32 6, float %4564)  ; FAbs(value)
  %4567 = call float @dx.op.unary.f32(i32 6, float %4563)  ; FAbs(value)
  %4568 = fsub fast float 1.000000e+00, %4566
  %4569 = fsub fast float 1.000000e+00, %4567
  %4570 = fcmp fast oge float %4563, 0.000000e+00
  %4571 = select i1 %4570, float 1.000000e+00, float -1.000000e+00
  %4572 = fcmp fast oge float %4564, 0.000000e+00
  %4573 = select i1 %4572, float 1.000000e+00, float -1.000000e+00
  %4574 = fmul fast float %4571, %4568
  %4575 = fmul fast float %4569, %4573
  %4576 = select i1 %4565, float %4574, float %4563
  %4577 = select i1 %4565, float %4575, float %4564
  %4578 = fmul fast float %4576, 5.000000e-01
  %4579 = fmul fast float %4577, 5.000000e-01
  %4580 = fadd fast float %4578, 5.000000e-01
  %4581 = fadd fast float %4579, 5.000000e-01
  %4582 = add i32 %4534, 6
  %4583 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4582, i32 undef)  ; BufferLoad(srv,index,wot)
  %4584 = extractvalue %dx.types.ResRet.f32 %4583, 0
  %4585 = fptoui float %4584 to i32
  %4586 = add i32 %4534, 7
  %4587 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4586, i32 undef)  ; BufferLoad(srv,index,wot)
  %4588 = extractvalue %dx.types.ResRet.f32 %4587, 0
  %4589 = fptoui float %4588 to i32
  %4590 = add i32 %4534, 8
  %4591 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4590, i32 undef)  ; BufferLoad(srv,index,wot)
  %4592 = extractvalue %dx.types.ResRet.f32 %4591, 0
  %4593 = fptoui float %4592 to i32
  %4594 = add i32 %4534, 9
  %4595 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4594, i32 undef)  ; BufferLoad(srv,index,wot)
  %4596 = extractvalue %dx.types.ResRet.f32 %4595, 0
  %4597 = fptoui float %4596 to i32
  %4598 = mul i32 %4597, %4585
  %4599 = mul i32 %4597, %4589
  %4600 = sitofp i32 %4598 to float
  %4601 = sitofp i32 %4599 to float
  %4602 = sitofp i32 %4593 to float
  %4603 = sitofp i32 %4597 to float
  %4604 = fadd fast float %4603, -2.000000e+00
  %4605 = fmul fast float %4604, %4580
  %4606 = fmul fast float %4604, %4581
  %4607 = fadd fast float %4600, 1.000000e+00
  %4608 = fadd fast float %4607, %4605
  %4609 = fadd fast float %4601, 1.000000e+00
  %4610 = fadd fast float %4609, %4606
  %4611 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %4612 = extractvalue %dx.types.CBufRet.f32 %4611, 0
  %4613 = extractvalue %dx.types.CBufRet.f32 %4611, 1
  %4614 = extractvalue %dx.types.CBufRet.f32 %4611, 3
  %4615 = fmul fast float %4614, %4612
  %4616 = fmul fast float %4614, %4613
  %4617 = fmul fast float %4615, %4608
  %4618 = fmul fast float %4616, %4610
  %4619 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %4620 = extractvalue %dx.types.CBufRet.f32 %4619, 0
  %4621 = fadd fast float %4620, %4602
  %4622 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %4617, float %4618, float %4621, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4623 = extractvalue %dx.types.ResRet.f32 %4622, 0
  %4624 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %4625 = extractvalue %dx.types.CBufRet.f32 %4624, 1
  %4626 = fadd fast float %4625, %4556
  %4627 = extractvalue %dx.types.CBufRet.f32 %4624, 0
  %4628 = fdiv fast float %4626, %4627
  %4629 = fcmp fast ogt float %4628, %4623
  br i1 %4629, label %4630, label %4639

; <label>:4630                                    ; preds = %4533
  %4631 = extractvalue %dx.types.ResRet.f32 %4622, 1
  %4632 = fmul fast float %4623, %4623
  %4633 = fsub fast float %4632, %4631
  %4634 = call float @dx.op.unary.f32(i32 6, float %4633)  ; FAbs(value)
  %4635 = fsub fast float %4628, %4623
  %4636 = fmul fast float %4635, %4635
  %4637 = fadd fast float %4634, %4636
  %4638 = fdiv fast float %4634, %4637
  br label %4639

; <label>:4639                                    ; preds = %4630, %4533
  %4640 = phi float [ %4638, %4630 ], [ 1.000000e+00, %4533 ]
  %4641 = fcmp fast olt float %4640, 0x3EE9000020000000
  br i1 %4641, label %4733, label %4642

; <label>:4642                                    ; preds = %4639
  %4643 = and i32 %4530, 1
  %4644 = lshr i32 %4530, 1
  %4645 = and i32 %4644, 1
  %4646 = lshr i32 %4530, 2
  %4647 = and i32 %4646, 1
  %4648 = sitofp i32 %4643 to float
  %4649 = sitofp i32 %4645 to float
  %4650 = sitofp i32 %4647 to float
  %4651 = fsub fast float 1.000000e+00, %4505
  %4652 = fsub fast float 1.000000e+00, %4514
  %4653 = fsub fast float 1.000000e+00, %4523
  %4654 = fsub fast float %4505, %4651
  %4655 = fsub fast float %4514, %4652
  %4656 = fsub fast float %4523, %4653
  %4657 = fmul fast float %4648, %4654
  %4658 = fmul fast float %4649, %4655
  %4659 = fmul fast float %4650, %4656
  %4660 = fadd fast float %4657, %4651
  %4661 = fadd fast float %4658, %4652
  %4662 = fadd fast float %4659, %4653
  %4663 = fmul fast float %4661, %4660
  %4664 = fmul fast float %4663, %4662
  %4665 = call float @dx.op.binary.f32(i32 35, float %4664, float 0x3F847AE140000000)  ; FMax(a,b)
  %4666 = call float @dx.op.unary.f32(i32 6, float %352)  ; FAbs(value)
  %4667 = call float @dx.op.unary.f32(i32 6, float %353)  ; FAbs(value)
  %4668 = fadd fast float %4667, %4666
  %4669 = call float @dx.op.unary.f32(i32 6, float %354)  ; FAbs(value)
  %4670 = fadd fast float %4668, %4669
  %4671 = fdiv fast float 1.000000e+00, %4670
  %4672 = fmul fast float %4671, %352
  %4673 = fmul fast float %4671, %353
  %4674 = fcmp fast ole float %354, 0.000000e+00
  %4675 = call float @dx.op.unary.f32(i32 6, float %4673)  ; FAbs(value)
  %4676 = call float @dx.op.unary.f32(i32 6, float %4672)  ; FAbs(value)
  %4677 = fsub fast float 1.000000e+00, %4675
  %4678 = fsub fast float 1.000000e+00, %4676
  %4679 = fcmp fast oge float %4672, 0.000000e+00
  %4680 = select i1 %4679, float 1.000000e+00, float -1.000000e+00
  %4681 = fcmp fast oge float %4673, 0.000000e+00
  %4682 = select i1 %4681, float 1.000000e+00, float -1.000000e+00
  %4683 = fmul fast float %4680, %4677
  %4684 = fmul fast float %4678, %4682
  %4685 = select i1 %4674, float %4683, float %4672
  %4686 = select i1 %4674, float %4684, float %4673
  %4687 = fmul fast float %4685, 5.000000e-01
  %4688 = fmul fast float %4686, 5.000000e-01
  %4689 = fadd fast float %4687, 5.000000e-01
  %4690 = fadd fast float %4688, 5.000000e-01
  %4691 = fmul fast float %4604, %4689
  %4692 = fmul fast float %4604, %4690
  %4693 = fadd fast float %4607, %4691
  %4694 = fadd fast float %4609, %4692
  %4695 = fmul fast float %4615, %4693
  %4696 = fmul fast float %4616, %4694
  %4697 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %4695, float %4696, float %4602, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4698 = extractvalue %dx.types.ResRet.f32 %4697, 0
  %4699 = fsub fast float %4698, %4625
  %4700 = fmul fast float %4699, %4627
  %4701 = fmul fast float %4700, %352
  %4702 = fmul fast float %4700, %353
  %4703 = fmul fast float %4700, %354
  %4704 = fsub fast float %4536, %372
  %4705 = fadd fast float %4704, %4701
  %4706 = fsub fast float %4539, %373
  %4707 = fadd fast float %4706, %4702
  %4708 = fsub fast float %4542, %374
  %4709 = fadd fast float %4708, %4703
  %4710 = call float @dx.op.dot3.f32(i32 55, float %4705, float %4707, float %4709, float %4705, float %4707, float %4709)  ; Dot3(ax,ay,az,bx,by,bz)
  %4711 = call float @dx.op.unary.f32(i32 25, float %4710)  ; Rsqrt(value)
  %4712 = fmul fast float %4705, %4711
  %4713 = fmul fast float %4707, %4711
  %4714 = fmul fast float %4709, %4711
  %4715 = call float @dx.op.dot3.f32(i32 55, float %4712, float %4713, float %4714, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %4716 = fcmp fast olt float %4715, 0.000000e+00
  br i1 %4716, label %4733, label %4717

; <label>:4717                                    ; preds = %4642
  %4718 = fmul fast float %4665, %4640
  %4719 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float %4712, float %4713, float %4714)  ; Dot3(ax,ay,az,bx,by,bz)
  %4720 = call float @dx.op.unary.f32(i32 7, float %4719)  ; Saturate(value)
  %4721 = fmul fast float %4720, %4718
  %4722 = fadd fast float %4721, %4527
  %4723 = fadd fast float %4665, %4528
  %4724 = fmul fast float %4705, %4705
  %4725 = fmul fast float %4707, %4707
  %4726 = fadd fast float %4724, %4725
  %4727 = fmul fast float %4709, %4709
  %4728 = fadd fast float %4726, %4727
  %4729 = call float @dx.op.unary.f32(i32 24, float %4728)  ; Sqrt(value)
  %4730 = fmul fast float %4721, %4729
  %4731 = fadd fast float %4730, %4529
  %4732 = fadd fast float %4718, %4526
  store float %4718, float* %4531, align 4, !tbaa !53
  br label %4733

; <label>:4733                                    ; preds = %4717, %4642, %4639, %4524
  %4734 = phi float [ %4526, %4524 ], [ %4526, %4639 ], [ %4526, %4642 ], [ %4732, %4717 ]
  %4735 = phi float [ %4527, %4524 ], [ %4527, %4639 ], [ %4527, %4642 ], [ %4722, %4717 ]
  %4736 = phi float [ %4528, %4524 ], [ %4528, %4639 ], [ %4528, %4642 ], [ %4723, %4717 ]
  %4737 = phi float [ %4529, %4524 ], [ %4529, %4639 ], [ %4529, %4642 ], [ %4731, %4717 ]
  %4738 = add nuw nsw i32 %4530, 1
  %4739 = icmp eq i32 %4738, 8
  br i1 %4739, label %4743, label %4740

; <label>:4740                                    ; preds = %4733
  %4741 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 %4738
  %4742 = load i32, i32* %4741, align 4, !tbaa !34
  br label %4524

; <label>:4743                                    ; preds = %4733
  %4744 = fcmp fast olt float %4734, 0x3EE9000020000000
  br i1 %4744, label %4952, label %4745

; <label>:4745                                    ; preds = %4743
  %4746 = fdiv fast float %4737, %4734
  %4747 = fmul fast float %4746, %352
  %4748 = fmul fast float %4746, %353
  %4749 = fmul fast float %4746, %354
  %4750 = fadd fast float %4747, %372
  %4751 = fadd fast float %4748, %373
  %4752 = fadd fast float %4749, %374
  br label %4753

; <label>:4753                                    ; preds = %4930, %4745
  %4754 = phi float [ 0.000000e+00, %4745 ], [ %4931, %4930 ]
  %4755 = phi float [ 0.000000e+00, %4745 ], [ %4932, %4930 ]
  %4756 = phi float [ 0.000000e+00, %4745 ], [ %4933, %4930 ]
  %4757 = phi float [ 0.000000e+00, %4745 ], [ %4934, %4930 ]
  %4758 = phi float [ 0.000000e+00, %4745 ], [ %4935, %4930 ]
  %4759 = phi i32 [ 0, %4745 ], [ %4936, %4930 ]
  %4760 = getelementptr inbounds [8 x float], [8 x float]* %30, i32 0, i32 %4759
  %4761 = load float, float* %4760, align 4, !tbaa !53
  %4762 = fcmp fast oeq float %4761, 0.000000e+00
  br i1 %4762, label %4930, label %4763

; <label>:4763                                    ; preds = %4753
  %4764 = getelementptr inbounds [8 x i32], [8 x i32]* %37, i32 0, i32 %4759
  %4765 = load i32, i32* %4764, align 4, !tbaa !34
  %4766 = mul i32 %4765, 10
  %4767 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4766, i32 undef)  ; BufferLoad(srv,index,wot)
  %4768 = extractvalue %dx.types.ResRet.f32 %4767, 0
  %4769 = or i32 %4766, 1
  %4770 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4769, i32 undef)  ; BufferLoad(srv,index,wot)
  %4771 = extractvalue %dx.types.ResRet.f32 %4770, 0
  %4772 = add i32 %4766, 2
  %4773 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4772, i32 undef)  ; BufferLoad(srv,index,wot)
  %4774 = extractvalue %dx.types.ResRet.f32 %4773, 0
  %4775 = fsub fast float %4750, %4768
  %4776 = fsub fast float %4751, %4771
  %4777 = fsub fast float %4752, %4774
  %4778 = fsub fast float %372, %4768
  %4779 = fsub fast float %373, %4771
  %4780 = fsub fast float %374, %4774
  %4781 = call float @dx.op.dot3.f32(i32 55, float %4775, float %4776, float %4777, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %4782 = call float @dx.op.dot3.f32(i32 55, float %4778, float %4779, float %4780, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %4783 = fcmp fast olt float %4781, %4782
  br i1 %4783, label %4930, label %4784

; <label>:4784                                    ; preds = %4763
  %4785 = call float @dx.op.dot3.f32(i32 55, float %4775, float %4776, float %4777, float %4775, float %4776, float %4777)  ; Dot3(ax,ay,az,bx,by,bz)
  %4786 = call float @dx.op.unary.f32(i32 25, float %4785)  ; Rsqrt(value)
  %4787 = fmul fast float %4786, %4775
  %4788 = fmul fast float %4786, %4776
  %4789 = fmul fast float %4786, %4777
  %4790 = call float @dx.op.unary.f32(i32 6, float %4787)  ; FAbs(value)
  %4791 = call float @dx.op.unary.f32(i32 6, float %4788)  ; FAbs(value)
  %4792 = fadd fast float %4791, %4790
  %4793 = call float @dx.op.unary.f32(i32 6, float %4789)  ; FAbs(value)
  %4794 = fadd fast float %4792, %4793
  %4795 = fdiv fast float 1.000000e+00, %4794
  %4796 = fmul fast float %4795, %4787
  %4797 = fmul fast float %4795, %4788
  %4798 = fcmp fast ole float %4789, 0.000000e+00
  %4799 = call float @dx.op.unary.f32(i32 6, float %4797)  ; FAbs(value)
  %4800 = call float @dx.op.unary.f32(i32 6, float %4796)  ; FAbs(value)
  %4801 = fsub fast float 1.000000e+00, %4799
  %4802 = fsub fast float 1.000000e+00, %4800
  %4803 = fcmp fast oge float %4796, 0.000000e+00
  %4804 = select i1 %4803, float 1.000000e+00, float -1.000000e+00
  %4805 = fcmp fast oge float %4797, 0.000000e+00
  %4806 = select i1 %4805, float 1.000000e+00, float -1.000000e+00
  %4807 = fmul fast float %4804, %4801
  %4808 = fmul fast float %4802, %4806
  %4809 = select i1 %4798, float %4807, float %4796
  %4810 = select i1 %4798, float %4808, float %4797
  %4811 = fmul fast float %4809, 5.000000e-01
  %4812 = fmul fast float %4810, 5.000000e-01
  %4813 = fadd fast float %4811, 5.000000e-01
  %4814 = fadd fast float %4812, 5.000000e-01
  %4815 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %4816 = extractvalue %dx.types.CBufRet.f32 %4815, 2
  %4817 = add i32 %4766, 6
  %4818 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4817, i32 undef)  ; BufferLoad(srv,index,wot)
  %4819 = extractvalue %dx.types.ResRet.f32 %4818, 0
  %4820 = fptoui float %4819 to i32
  %4821 = add i32 %4766, 7
  %4822 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4821, i32 undef)  ; BufferLoad(srv,index,wot)
  %4823 = extractvalue %dx.types.ResRet.f32 %4822, 0
  %4824 = fptoui float %4823 to i32
  %4825 = add i32 %4766, 8
  %4826 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4825, i32 undef)  ; BufferLoad(srv,index,wot)
  %4827 = extractvalue %dx.types.ResRet.f32 %4826, 0
  %4828 = fptoui float %4827 to i32
  %4829 = add i32 %4766, 9
  %4830 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4829, i32 undef)  ; BufferLoad(srv,index,wot)
  %4831 = extractvalue %dx.types.ResRet.f32 %4830, 0
  %4832 = fptoui float %4831 to i32
  %4833 = icmp eq i32 %4832, 64
  %4834 = select i1 %4833, float 3.000000e+00, float %4816
  %4835 = fmul fast float %4834, %375
  %4836 = mul i32 %4832, %4820
  %4837 = mul i32 %4832, %4824
  %4838 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %4839 = extractvalue %dx.types.CBufRet.f32 %4838, 0
  %4840 = extractvalue %dx.types.CBufRet.f32 %4838, 1
  %4841 = extractvalue %dx.types.CBufRet.f32 %4838, 3
  %4842 = fmul fast float %4841, %4839
  %4843 = fmul fast float %4841, %4840
  %4844 = fptosi float %4834 to i32
  %4845 = call float @dx.op.unary.f32(i32 29, float %4835)  ; Round_z(value)
  %4846 = fsub fast float %4835, %4845
  %4847 = fptosi float %4845 to i32
  %4848 = call i32 @dx.op.binary.i32(i32 37, i32 %4847, i32 0)  ; IMax(a,b)
  %4849 = add nsw i32 %4848, 1
  %4850 = call i32 @dx.op.binary.i32(i32 38, i32 %4849, i32 %4844)  ; IMin(a,b)
  %4851 = sitofp i32 %4848 to float
  %4852 = call float @dx.op.unary.f32(i32 21, float %4851)  ; Exp(value)
  %4853 = fdiv fast float 1.000000e+00, %4852
  %4854 = sitofp i32 %4836 to float
  %4855 = sitofp i32 %4837 to float
  %4856 = fmul fast float %4853, %4854
  %4857 = fmul fast float %4853, %4855
  %4858 = fptosi float %4856 to i32
  %4859 = fptosi float %4857 to i32
  %4860 = sitofp i32 %4832 to float
  %4861 = fmul fast float %4853, %4860
  %4862 = fadd fast float %4861, -2.000000e+00
  %4863 = sitofp i32 %4828 to float
  %4864 = sitofp i32 %4858 to float
  %4865 = sitofp i32 %4859 to float
  %4866 = fmul fast float %4862, %4813
  %4867 = fmul fast float %4862, %4814
  %4868 = fadd fast float %4866, 1.000000e+00
  %4869 = fadd fast float %4867, 1.000000e+00
  %4870 = fadd fast float %4868, %4864
  %4871 = fadd fast float %4869, %4865
  %4872 = fmul fast float %4853, %4842
  %4873 = fmul fast float %4853, %4843
  %4874 = fdiv fast float %4870, %4872
  %4875 = fdiv fast float %4871, %4873
  %4876 = sitofp i32 %4850 to float
  %4877 = call float @dx.op.unary.f32(i32 21, float %4876)  ; Exp(value)
  %4878 = fdiv fast float 1.000000e+00, %4877
  %4879 = fmul fast float %4878, %4854
  %4880 = fmul fast float %4878, %4855
  %4881 = fptosi float %4879 to i32
  %4882 = fptosi float %4880 to i32
  %4883 = fmul fast float %4878, %4860
  %4884 = fadd fast float %4883, -2.000000e+00
  %4885 = sitofp i32 %4881 to float
  %4886 = sitofp i32 %4882 to float
  %4887 = fmul fast float %4884, %4813
  %4888 = fmul fast float %4884, %4814
  %4889 = fadd fast float %4887, 1.000000e+00
  %4890 = fadd fast float %4888, 1.000000e+00
  %4891 = fadd fast float %4889, %4885
  %4892 = fadd fast float %4890, %4886
  %4893 = fmul fast float %4878, %4842
  %4894 = fmul fast float %4878, %4843
  %4895 = fdiv fast float %4891, %4893
  %4896 = fdiv fast float %4892, %4894
  %4897 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4874, float %4875, float %4863, float undef, i32 0, i32 0, i32 undef, float %4851)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4898 = extractvalue %dx.types.ResRet.f32 %4897, 0
  %4899 = extractvalue %dx.types.ResRet.f32 %4897, 1
  %4900 = extractvalue %dx.types.ResRet.f32 %4897, 2
  %4901 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %4895, float %4896, float %4863, float undef, i32 0, i32 0, i32 undef, float %4876)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4902 = extractvalue %dx.types.ResRet.f32 %4901, 0
  %4903 = extractvalue %dx.types.ResRet.f32 %4901, 1
  %4904 = extractvalue %dx.types.ResRet.f32 %4901, 2
  %4905 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %4874, float %4875, float %4863, float undef, i32 0, i32 0, i32 undef, float %4851)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4906 = extractvalue %dx.types.ResRet.f32 %4905, 0
  %4907 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %4895, float %4896, float %4863, float undef, i32 0, i32 0, i32 undef, float %4876)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %4908 = extractvalue %dx.types.ResRet.f32 %4907, 0
  %4909 = fsub fast float %4902, %4898
  %4910 = fsub fast float %4903, %4899
  %4911 = fsub fast float %4904, %4900
  %4912 = fsub fast float %4908, %4906
  %4913 = fmul fast float %4909, %4846
  %4914 = fmul fast float %4910, %4846
  %4915 = fmul fast float %4911, %4846
  %4916 = fmul fast float %4912, %4846
  %4917 = fadd fast float %4913, %4898
  %4918 = fadd fast float %4914, %4899
  %4919 = fadd fast float %4915, %4900
  %4920 = fadd fast float %4916, %4906
  %4921 = fmul fast float %4917, %4761
  %4922 = fmul fast float %4918, %4761
  %4923 = fmul fast float %4919, %4761
  %4924 = fmul fast float %4920, %4761
  %4925 = fadd fast float %4921, %4754
  %4926 = fadd fast float %4922, %4755
  %4927 = fadd fast float %4923, %4756
  %4928 = fadd fast float %4924, %4757
  %4929 = fadd fast float %4761, %4758
  br label %4930

; <label>:4930                                    ; preds = %4784, %4763, %4753
  %4931 = phi float [ %4754, %4753 ], [ %4754, %4763 ], [ %4925, %4784 ]
  %4932 = phi float [ %4755, %4753 ], [ %4755, %4763 ], [ %4926, %4784 ]
  %4933 = phi float [ %4756, %4753 ], [ %4756, %4763 ], [ %4927, %4784 ]
  %4934 = phi float [ %4757, %4753 ], [ %4757, %4763 ], [ %4928, %4784 ]
  %4935 = phi float [ %4758, %4753 ], [ %4758, %4763 ], [ %4929, %4784 ]
  %4936 = add nuw nsw i32 %4759, 1
  %4937 = icmp eq i32 %4936, 8
  br i1 %4937, label %4938, label %4753

; <label>:4938                                    ; preds = %4930
  %4939 = call float @dx.op.binary.f32(i32 35, float %4935, float 0x3EE9000020000000)  ; FMax(a,b)
  %4940 = fdiv fast float %4931, %4939
  %4941 = fdiv fast float %4932, %4939
  %4942 = fdiv fast float %4933, %4939
  %4943 = fdiv fast float %4934, %4939
  %4944 = fdiv fast float %4735, %4736
  %4945 = call float @dx.op.binary.f32(i32 35, float 0x3EE9000020000000, float %4944)  ; FMax(a,b)
  %4946 = fmul fast float %4945, %4940
  %4947 = fmul fast float %4945, %4941
  %4948 = fmul fast float %4945, %4942
  %4949 = fadd fast float %4943, -1.000000e+00
  %4950 = fmul fast float %4945, %4949
  %4951 = fadd fast float %4950, 1.000000e+00
  br label %4952

; <label>:4952                                    ; preds = %4938, %4743
  %4953 = phi float [ %4946, %4938 ], [ 0.000000e+00, %4743 ]
  %4954 = phi float [ %4947, %4938 ], [ 0.000000e+00, %4743 ]
  %4955 = phi float [ %4948, %4938 ], [ 0.000000e+00, %4743 ]
  %4956 = phi float [ %4951, %4938 ], [ 0.000000e+00, %4743 ]
  %4957 = call float @dx.op.binary.f32(i32 35, float %4956, float 0.000000e+00)  ; FMax(a,b)
  %4958 = call float @dx.op.binary.f32(i32 35, float %2108, float %4957)  ; FMax(a,b)
  %4959 = fmul fast float %4958, %4953
  %4960 = fmul fast float %4958, %4954
  %4961 = fmul fast float %4958, %4955
  br label %4962

; <label>:4962                                    ; preds = %4952, %4481, %4479, %3810, %3456, %3454
  %4963 = phi float [ %3811, %3810 ], [ %4959, %4952 ], [ %3457, %3456 ], [ %3263, %3454 ], [ %4482, %4481 ], [ %4288, %4479 ]
  %4964 = phi float [ %3812, %3810 ], [ %4960, %4952 ], [ %3458, %3456 ], [ %3264, %3454 ], [ %4483, %4481 ], [ %4289, %4479 ]
  %4965 = phi float [ %3813, %3810 ], [ %4961, %4952 ], [ %3459, %3456 ], [ %3265, %3454 ], [ %4484, %4481 ], [ %4290, %4479 ]
  %4966 = phi float [ %3815, %3810 ], [ %4958, %4952 ], [ 1.000000e+00, %3456 ], [ 1.000000e+00, %3454 ], [ 1.000000e+00, %4481 ], [ 1.000000e+00, %4479 ]
  %4967 = mul i32 %2753, 10
  %4968 = add i32 %4967, 4
  %4969 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %4968, i32 undef)  ; BufferLoad(srv,index,wot)
  %4970 = extractvalue %dx.types.ResRet.f32 %4969, 0
  %4971 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %4972 = extractvalue %dx.types.CBufRet.f32 %4971, 2
  %4973 = fsub fast float %4972, %4970
  %4974 = extractvalue %dx.types.CBufRet.f32 %4971, 3
  %4975 = fdiv fast float %4973, %4974
  %4976 = call float @dx.op.unary.f32(i32 7, float %4975)  ; Saturate(value)
  %4977 = fmul fast float %4976, %4963
  %4978 = fmul fast float %4976, %4964
  %4979 = fmul fast float %4976, %4965
  %4980 = fmul fast float %4976, %4966
  br label %4981

; <label>:4981                                    ; preds = %4962, %2104
  %4982 = phi float [ %4977, %4962 ], [ 0.000000e+00, %2104 ]
  %4983 = phi float [ %4978, %4962 ], [ 0.000000e+00, %2104 ]
  %4984 = phi float [ %4979, %4962 ], [ 0.000000e+00, %2104 ]
  %4985 = phi float [ %4980, %4962 ], [ 0.000000e+00, %2104 ]
  %4986 = fcmp fast ogt float %2114, 0.000000e+00
  br i1 %4986, label %4987, label %5497

; <label>:4987                                    ; preds = %4981
  %4988 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 0
  store i32 %1732, i32* %4988, align 4
  %4989 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 1
  store i32 %1733, i32* %4989, align 4
  %4990 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 2
  store i32 %1734, i32* %4990, align 4
  %4991 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 3
  store i32 %1735, i32* %4991, align 4
  %4992 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 4
  store i32 %1736, i32* %4992, align 4
  %4993 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 5
  store i32 %1737, i32* %4993, align 4
  %4994 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 6
  store i32 %1738, i32* %4994, align 4
  %4995 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 7
  store i32 %1739, i32* %4995, align 4
  %4996 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %4997 = extractvalue %dx.types.CBufRet.f32 %4996, 0
  %4998 = extractvalue %dx.types.CBufRet.f32 %4996, 1
  %4999 = extractvalue %dx.types.CBufRet.f32 %4996, 2
  %5000 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %5001 = extractvalue %dx.types.CBufRet.f32 %5000, 0
  %5002 = extractvalue %dx.types.CBufRet.f32 %5000, 1
  %5003 = extractvalue %dx.types.CBufRet.f32 %5000, 2
  %5004 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 26)  ; CBufferLoadLegacy(handle,regIndex)
  %5005 = extractvalue %dx.types.CBufRet.f32 %5004, 0
  %5006 = extractvalue %dx.types.CBufRet.f32 %5004, 1
  %5007 = extractvalue %dx.types.CBufRet.f32 %5004, 2
  %5008 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 27)  ; CBufferLoadLegacy(handle,regIndex)
  %5009 = extractvalue %dx.types.CBufRet.f32 %5008, 0
  %5010 = extractvalue %dx.types.CBufRet.f32 %5008, 1
  %5011 = extractvalue %dx.types.CBufRet.f32 %5008, 2
  %5012 = fmul fast float %4997, %372
  %5013 = call float @dx.op.tertiary.f32(i32 46, float %373, float %5001, float %5012)  ; FMad(a,b,c)
  %5014 = call float @dx.op.tertiary.f32(i32 46, float %374, float %5005, float %5013)  ; FMad(a,b,c)
  %5015 = fadd fast float %5014, %5009
  %5016 = fmul fast float %4998, %372
  %5017 = call float @dx.op.tertiary.f32(i32 46, float %373, float %5002, float %5016)  ; FMad(a,b,c)
  %5018 = call float @dx.op.tertiary.f32(i32 46, float %374, float %5006, float %5017)  ; FMad(a,b,c)
  %5019 = fadd fast float %5018, %5010
  %5020 = fmul fast float %4999, %372
  %5021 = call float @dx.op.tertiary.f32(i32 46, float %373, float %5003, float %5020)  ; FMad(a,b,c)
  %5022 = call float @dx.op.tertiary.f32(i32 46, float %374, float %5007, float %5021)  ; FMad(a,b,c)
  %5023 = fadd fast float %5022, %5011
  %5024 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %5025 = extractvalue %dx.types.CBufRet.f32 %5024, 3
  %5026 = fdiv fast float %5015, %5025
  %5027 = fdiv fast float %5019, %5025
  %5028 = fdiv fast float %5023, %5025
  %5029 = fsub fast float %5026, %1752
  %5030 = fsub fast float %5027, %1753
  %5031 = fsub fast float %5028, %1754
  %5032 = fdiv fast float %5029, %1758
  %5033 = fdiv fast float %5030, %1758
  %5034 = fdiv fast float %5031, %1758
  %5035 = fcmp fast olt float %5032, 0.000000e+00
  %5036 = fcmp fast ogt float %5032, 1.000000e+00
  %5037 = or i1 %5035, %5036
  br i1 %5037, label %5038, label %5042

; <label>:5038                                    ; preds = %4987
  %5039 = fmul fast float %5032, 2.000000e+00
  %5040 = fadd fast float %5039, -1.000000e+00
  %5041 = fdiv fast float %5032, %5040
  br label %5042

; <label>:5042                                    ; preds = %5038, %4987
  %5043 = phi float [ %5041, %5038 ], [ %5032, %4987 ]
  %5044 = fcmp fast olt float %5033, 0.000000e+00
  %5045 = fcmp fast ogt float %5033, 1.000000e+00
  %5046 = or i1 %5044, %5045
  br i1 %5046, label %5047, label %5051

; <label>:5047                                    ; preds = %5042
  %5048 = fmul fast float %5033, 2.000000e+00
  %5049 = fadd fast float %5048, -1.000000e+00
  %5050 = fdiv fast float %5033, %5049
  br label %5051

; <label>:5051                                    ; preds = %5047, %5042
  %5052 = phi float [ %5050, %5047 ], [ %5033, %5042 ]
  %5053 = fcmp fast olt float %5034, 0.000000e+00
  %5054 = fcmp fast ogt float %5034, 1.000000e+00
  %5055 = or i1 %5053, %5054
  br i1 %5055, label %5056, label %5060

; <label>:5056                                    ; preds = %5051
  %5057 = fmul fast float %5034, 2.000000e+00
  %5058 = fadd fast float %5057, -1.000000e+00
  %5059 = fdiv fast float %5034, %5058
  br label %5060

; <label>:5060                                    ; preds = %5056, %5051
  %5061 = phi float [ %5059, %5056 ], [ %5034, %5051 ]
  br label %5062

; <label>:5062                                    ; preds = %5278, %5060
  %5063 = phi i32 [ %1732, %5060 ], [ %5280, %5278 ]
  %5064 = phi float [ 0.000000e+00, %5060 ], [ %5272, %5278 ]
  %5065 = phi float [ 0.000000e+00, %5060 ], [ %5273, %5278 ]
  %5066 = phi float [ 0.000000e+00, %5060 ], [ %5274, %5278 ]
  %5067 = phi float [ 0.000000e+00, %5060 ], [ %5275, %5278 ]
  %5068 = phi i32 [ 0, %5060 ], [ %5276, %5278 ]
  %5069 = getelementptr inbounds [8 x float], [8 x float]* %29, i32 0, i32 %5068
  store float 0.000000e+00, float* %5069, align 4, !tbaa !53
  %5070 = icmp slt i32 %5063, 0
  br i1 %5070, label %5271, label %5071

; <label>:5071                                    ; preds = %5062
  %5072 = mul i32 %5063, 10
  %5073 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5072, i32 undef)  ; BufferLoad(srv,index,wot)
  %5074 = extractvalue %dx.types.ResRet.f32 %5073, 0
  %5075 = or i32 %5072, 1
  %5076 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5075, i32 undef)  ; BufferLoad(srv,index,wot)
  %5077 = extractvalue %dx.types.ResRet.f32 %5076, 0
  %5078 = add i32 %5072, 2
  %5079 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5078, i32 undef)  ; BufferLoad(srv,index,wot)
  %5080 = extractvalue %dx.types.ResRet.f32 %5079, 0
  %5081 = fsub fast float %372, %5074
  %5082 = fsub fast float %373, %5077
  %5083 = fsub fast float %374, %5080
  %5084 = call float @dx.op.dot3.f32(i32 55, float %5081, float %5082, float %5083, float %5081, float %5082, float %5083)  ; Dot3(ax,ay,az,bx,by,bz)
  %5085 = call float @dx.op.unary.f32(i32 25, float %5084)  ; Rsqrt(value)
  %5086 = fmul fast float %5085, %5081
  %5087 = fmul fast float %5085, %5082
  %5088 = fmul fast float %5083, %5085
  %5089 = fmul fast float %5081, %5081
  %5090 = fmul fast float %5082, %5082
  %5091 = fadd fast float %5090, %5089
  %5092 = fmul fast float %5083, %5083
  %5093 = fadd fast float %5091, %5092
  %5094 = call float @dx.op.unary.f32(i32 24, float %5093)  ; Sqrt(value)
  %5095 = call float @dx.op.unary.f32(i32 6, float %5086)  ; FAbs(value)
  %5096 = call float @dx.op.unary.f32(i32 6, float %5087)  ; FAbs(value)
  %5097 = fadd fast float %5096, %5095
  %5098 = call float @dx.op.unary.f32(i32 6, float %5088)  ; FAbs(value)
  %5099 = fadd fast float %5097, %5098
  %5100 = fdiv fast float 1.000000e+00, %5099
  %5101 = fmul fast float %5100, %5086
  %5102 = fmul fast float %5100, %5087
  %5103 = fcmp fast ole float %5088, 0.000000e+00
  %5104 = call float @dx.op.unary.f32(i32 6, float %5102)  ; FAbs(value)
  %5105 = call float @dx.op.unary.f32(i32 6, float %5101)  ; FAbs(value)
  %5106 = fsub fast float 1.000000e+00, %5104
  %5107 = fsub fast float 1.000000e+00, %5105
  %5108 = fcmp fast oge float %5101, 0.000000e+00
  %5109 = select i1 %5108, float 1.000000e+00, float -1.000000e+00
  %5110 = fcmp fast oge float %5102, 0.000000e+00
  %5111 = select i1 %5110, float 1.000000e+00, float -1.000000e+00
  %5112 = fmul fast float %5109, %5106
  %5113 = fmul fast float %5107, %5111
  %5114 = select i1 %5103, float %5112, float %5101
  %5115 = select i1 %5103, float %5113, float %5102
  %5116 = fmul fast float %5114, 5.000000e-01
  %5117 = fmul fast float %5115, 5.000000e-01
  %5118 = fadd fast float %5116, 5.000000e-01
  %5119 = fadd fast float %5117, 5.000000e-01
  %5120 = add i32 %5072, 6
  %5121 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5120, i32 undef)  ; BufferLoad(srv,index,wot)
  %5122 = extractvalue %dx.types.ResRet.f32 %5121, 0
  %5123 = fptoui float %5122 to i32
  %5124 = add i32 %5072, 7
  %5125 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5124, i32 undef)  ; BufferLoad(srv,index,wot)
  %5126 = extractvalue %dx.types.ResRet.f32 %5125, 0
  %5127 = fptoui float %5126 to i32
  %5128 = add i32 %5072, 8
  %5129 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5128, i32 undef)  ; BufferLoad(srv,index,wot)
  %5130 = extractvalue %dx.types.ResRet.f32 %5129, 0
  %5131 = fptoui float %5130 to i32
  %5132 = add i32 %5072, 9
  %5133 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5132, i32 undef)  ; BufferLoad(srv,index,wot)
  %5134 = extractvalue %dx.types.ResRet.f32 %5133, 0
  %5135 = fptoui float %5134 to i32
  %5136 = mul i32 %5135, %5123
  %5137 = mul i32 %5135, %5127
  %5138 = sitofp i32 %5136 to float
  %5139 = sitofp i32 %5137 to float
  %5140 = sitofp i32 %5131 to float
  %5141 = sitofp i32 %5135 to float
  %5142 = fadd fast float %5141, -2.000000e+00
  %5143 = fmul fast float %5142, %5118
  %5144 = fmul fast float %5142, %5119
  %5145 = fadd fast float %5138, 1.000000e+00
  %5146 = fadd fast float %5145, %5143
  %5147 = fadd fast float %5139, 1.000000e+00
  %5148 = fadd fast float %5147, %5144
  %5149 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %5150 = extractvalue %dx.types.CBufRet.f32 %5149, 0
  %5151 = extractvalue %dx.types.CBufRet.f32 %5149, 1
  %5152 = extractvalue %dx.types.CBufRet.f32 %5149, 3
  %5153 = fmul fast float %5152, %5150
  %5154 = fmul fast float %5152, %5151
  %5155 = fmul fast float %5153, %5146
  %5156 = fmul fast float %5154, %5148
  %5157 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %5158 = extractvalue %dx.types.CBufRet.f32 %5157, 0
  %5159 = fadd fast float %5158, %5140
  %5160 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %5155, float %5156, float %5159, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5161 = extractvalue %dx.types.ResRet.f32 %5160, 0
  %5162 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %5163 = extractvalue %dx.types.CBufRet.f32 %5162, 1
  %5164 = fadd fast float %5163, %5094
  %5165 = extractvalue %dx.types.CBufRet.f32 %5162, 0
  %5166 = fdiv fast float %5164, %5165
  %5167 = fcmp fast ogt float %5166, %5161
  br i1 %5167, label %5168, label %5177

; <label>:5168                                    ; preds = %5071
  %5169 = extractvalue %dx.types.ResRet.f32 %5160, 1
  %5170 = fmul fast float %5161, %5161
  %5171 = fsub fast float %5170, %5169
  %5172 = call float @dx.op.unary.f32(i32 6, float %5171)  ; FAbs(value)
  %5173 = fsub fast float %5166, %5161
  %5174 = fmul fast float %5173, %5173
  %5175 = fadd fast float %5172, %5174
  %5176 = fdiv fast float %5172, %5175
  br label %5177

; <label>:5177                                    ; preds = %5168, %5071
  %5178 = phi float [ %5176, %5168 ], [ 1.000000e+00, %5071 ]
  %5179 = fcmp fast olt float %5178, 0x3EE9000020000000
  br i1 %5179, label %5271, label %5180

; <label>:5180                                    ; preds = %5177
  %5181 = and i32 %5068, 1
  %5182 = lshr i32 %5068, 1
  %5183 = and i32 %5182, 1
  %5184 = lshr i32 %5068, 2
  %5185 = and i32 %5184, 1
  %5186 = sitofp i32 %5181 to float
  %5187 = sitofp i32 %5183 to float
  %5188 = sitofp i32 %5185 to float
  %5189 = fsub fast float 1.000000e+00, %5043
  %5190 = fsub fast float 1.000000e+00, %5052
  %5191 = fsub fast float 1.000000e+00, %5061
  %5192 = fsub fast float %5043, %5189
  %5193 = fsub fast float %5052, %5190
  %5194 = fsub fast float %5061, %5191
  %5195 = fmul fast float %5186, %5192
  %5196 = fmul fast float %5187, %5193
  %5197 = fmul fast float %5188, %5194
  %5198 = fadd fast float %5195, %5189
  %5199 = fadd fast float %5196, %5190
  %5200 = fadd fast float %5197, %5191
  %5201 = fmul fast float %5199, %5198
  %5202 = fmul fast float %5201, %5200
  %5203 = call float @dx.op.binary.f32(i32 35, float %5202, float 0x3F847AE140000000)  ; FMax(a,b)
  %5204 = call float @dx.op.unary.f32(i32 6, float %352)  ; FAbs(value)
  %5205 = call float @dx.op.unary.f32(i32 6, float %353)  ; FAbs(value)
  %5206 = fadd fast float %5205, %5204
  %5207 = call float @dx.op.unary.f32(i32 6, float %354)  ; FAbs(value)
  %5208 = fadd fast float %5206, %5207
  %5209 = fdiv fast float 1.000000e+00, %5208
  %5210 = fmul fast float %5209, %352
  %5211 = fmul fast float %5209, %353
  %5212 = fcmp fast ole float %354, 0.000000e+00
  %5213 = call float @dx.op.unary.f32(i32 6, float %5211)  ; FAbs(value)
  %5214 = call float @dx.op.unary.f32(i32 6, float %5210)  ; FAbs(value)
  %5215 = fsub fast float 1.000000e+00, %5213
  %5216 = fsub fast float 1.000000e+00, %5214
  %5217 = fcmp fast oge float %5210, 0.000000e+00
  %5218 = select i1 %5217, float 1.000000e+00, float -1.000000e+00
  %5219 = fcmp fast oge float %5211, 0.000000e+00
  %5220 = select i1 %5219, float 1.000000e+00, float -1.000000e+00
  %5221 = fmul fast float %5218, %5215
  %5222 = fmul fast float %5216, %5220
  %5223 = select i1 %5212, float %5221, float %5210
  %5224 = select i1 %5212, float %5222, float %5211
  %5225 = fmul fast float %5223, 5.000000e-01
  %5226 = fmul fast float %5224, 5.000000e-01
  %5227 = fadd fast float %5225, 5.000000e-01
  %5228 = fadd fast float %5226, 5.000000e-01
  %5229 = fmul fast float %5142, %5227
  %5230 = fmul fast float %5142, %5228
  %5231 = fadd fast float %5145, %5229
  %5232 = fadd fast float %5147, %5230
  %5233 = fmul fast float %5153, %5231
  %5234 = fmul fast float %5154, %5232
  %5235 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %15, float %5233, float %5234, float %5140, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5236 = extractvalue %dx.types.ResRet.f32 %5235, 0
  %5237 = fsub fast float %5236, %5163
  %5238 = fmul fast float %5237, %5165
  %5239 = fmul fast float %5238, %352
  %5240 = fmul fast float %5238, %353
  %5241 = fmul fast float %5238, %354
  %5242 = fsub fast float %5074, %372
  %5243 = fadd fast float %5242, %5239
  %5244 = fsub fast float %5077, %373
  %5245 = fadd fast float %5244, %5240
  %5246 = fsub fast float %5080, %374
  %5247 = fadd fast float %5246, %5241
  %5248 = call float @dx.op.dot3.f32(i32 55, float %5243, float %5245, float %5247, float %5243, float %5245, float %5247)  ; Dot3(ax,ay,az,bx,by,bz)
  %5249 = call float @dx.op.unary.f32(i32 25, float %5248)  ; Rsqrt(value)
  %5250 = fmul fast float %5243, %5249
  %5251 = fmul fast float %5245, %5249
  %5252 = fmul fast float %5247, %5249
  %5253 = call float @dx.op.dot3.f32(i32 55, float %5250, float %5251, float %5252, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %5254 = fcmp fast olt float %5253, 0.000000e+00
  br i1 %5254, label %5271, label %5255

; <label>:5255                                    ; preds = %5180
  %5256 = fmul fast float %5203, %5178
  %5257 = call float @dx.op.dot3.f32(i32 55, float %352, float %353, float %354, float %5250, float %5251, float %5252)  ; Dot3(ax,ay,az,bx,by,bz)
  %5258 = call float @dx.op.unary.f32(i32 7, float %5257)  ; Saturate(value)
  %5259 = fmul fast float %5258, %5256
  %5260 = fadd fast float %5259, %5065
  %5261 = fadd fast float %5203, %5066
  %5262 = fmul fast float %5243, %5243
  %5263 = fmul fast float %5245, %5245
  %5264 = fadd fast float %5262, %5263
  %5265 = fmul fast float %5247, %5247
  %5266 = fadd fast float %5264, %5265
  %5267 = call float @dx.op.unary.f32(i32 24, float %5266)  ; Sqrt(value)
  %5268 = fmul fast float %5259, %5267
  %5269 = fadd fast float %5268, %5067
  %5270 = fadd fast float %5256, %5064
  store float %5256, float* %5069, align 4, !tbaa !53
  br label %5271

; <label>:5271                                    ; preds = %5255, %5180, %5177, %5062
  %5272 = phi float [ %5064, %5062 ], [ %5064, %5177 ], [ %5064, %5180 ], [ %5270, %5255 ]
  %5273 = phi float [ %5065, %5062 ], [ %5065, %5177 ], [ %5065, %5180 ], [ %5260, %5255 ]
  %5274 = phi float [ %5066, %5062 ], [ %5066, %5177 ], [ %5066, %5180 ], [ %5261, %5255 ]
  %5275 = phi float [ %5067, %5062 ], [ %5067, %5177 ], [ %5067, %5180 ], [ %5269, %5255 ]
  %5276 = add nuw nsw i32 %5068, 1
  %5277 = icmp eq i32 %5276, 8
  br i1 %5277, label %5281, label %5278

; <label>:5278                                    ; preds = %5271
  %5279 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 %5276
  %5280 = load i32, i32* %5279, align 4, !tbaa !34
  br label %5062

; <label>:5281                                    ; preds = %5271
  %5282 = fcmp fast olt float %5272, 0x3EE9000020000000
  br i1 %5282, label %5490, label %5283

; <label>:5283                                    ; preds = %5281
  %5284 = fdiv fast float %5275, %5272
  %5285 = fmul fast float %5284, %352
  %5286 = fmul fast float %5284, %353
  %5287 = fmul fast float %5284, %354
  %5288 = fadd fast float %5285, %372
  %5289 = fadd fast float %5286, %373
  %5290 = fadd fast float %5287, %374
  br label %5291

; <label>:5291                                    ; preds = %5468, %5283
  %5292 = phi float [ 0.000000e+00, %5283 ], [ %5469, %5468 ]
  %5293 = phi float [ 0.000000e+00, %5283 ], [ %5470, %5468 ]
  %5294 = phi float [ 0.000000e+00, %5283 ], [ %5471, %5468 ]
  %5295 = phi float [ 0.000000e+00, %5283 ], [ %5472, %5468 ]
  %5296 = phi float [ 0.000000e+00, %5283 ], [ %5473, %5468 ]
  %5297 = phi i32 [ 0, %5283 ], [ %5474, %5468 ]
  %5298 = getelementptr inbounds [8 x float], [8 x float]* %29, i32 0, i32 %5297
  %5299 = load float, float* %5298, align 4, !tbaa !53
  %5300 = fcmp fast oeq float %5299, 0.000000e+00
  br i1 %5300, label %5468, label %5301

; <label>:5301                                    ; preds = %5291
  %5302 = getelementptr inbounds [8 x i32], [8 x i32]* %36, i32 0, i32 %5297
  %5303 = load i32, i32* %5302, align 4, !tbaa !34
  %5304 = mul i32 %5303, 10
  %5305 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5304, i32 undef)  ; BufferLoad(srv,index,wot)
  %5306 = extractvalue %dx.types.ResRet.f32 %5305, 0
  %5307 = or i32 %5304, 1
  %5308 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5307, i32 undef)  ; BufferLoad(srv,index,wot)
  %5309 = extractvalue %dx.types.ResRet.f32 %5308, 0
  %5310 = add i32 %5304, 2
  %5311 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5310, i32 undef)  ; BufferLoad(srv,index,wot)
  %5312 = extractvalue %dx.types.ResRet.f32 %5311, 0
  %5313 = fsub fast float %5288, %5306
  %5314 = fsub fast float %5289, %5309
  %5315 = fsub fast float %5290, %5312
  %5316 = fsub fast float %372, %5306
  %5317 = fsub fast float %373, %5309
  %5318 = fsub fast float %374, %5312
  %5319 = call float @dx.op.dot3.f32(i32 55, float %5313, float %5314, float %5315, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %5320 = call float @dx.op.dot3.f32(i32 55, float %5316, float %5317, float %5318, float %233, float %234, float %235)  ; Dot3(ax,ay,az,bx,by,bz)
  %5321 = fcmp fast olt float %5319, %5320
  br i1 %5321, label %5468, label %5322

; <label>:5322                                    ; preds = %5301
  %5323 = call float @dx.op.dot3.f32(i32 55, float %5313, float %5314, float %5315, float %5313, float %5314, float %5315)  ; Dot3(ax,ay,az,bx,by,bz)
  %5324 = call float @dx.op.unary.f32(i32 25, float %5323)  ; Rsqrt(value)
  %5325 = fmul fast float %5324, %5313
  %5326 = fmul fast float %5324, %5314
  %5327 = fmul fast float %5324, %5315
  %5328 = call float @dx.op.unary.f32(i32 6, float %5325)  ; FAbs(value)
  %5329 = call float @dx.op.unary.f32(i32 6, float %5326)  ; FAbs(value)
  %5330 = fadd fast float %5329, %5328
  %5331 = call float @dx.op.unary.f32(i32 6, float %5327)  ; FAbs(value)
  %5332 = fadd fast float %5330, %5331
  %5333 = fdiv fast float 1.000000e+00, %5332
  %5334 = fmul fast float %5333, %5325
  %5335 = fmul fast float %5333, %5326
  %5336 = fcmp fast ole float %5327, 0.000000e+00
  %5337 = call float @dx.op.unary.f32(i32 6, float %5335)  ; FAbs(value)
  %5338 = call float @dx.op.unary.f32(i32 6, float %5334)  ; FAbs(value)
  %5339 = fsub fast float 1.000000e+00, %5337
  %5340 = fsub fast float 1.000000e+00, %5338
  %5341 = fcmp fast oge float %5334, 0.000000e+00
  %5342 = select i1 %5341, float 1.000000e+00, float -1.000000e+00
  %5343 = fcmp fast oge float %5335, 0.000000e+00
  %5344 = select i1 %5343, float 1.000000e+00, float -1.000000e+00
  %5345 = fmul fast float %5342, %5339
  %5346 = fmul fast float %5340, %5344
  %5347 = select i1 %5336, float %5345, float %5334
  %5348 = select i1 %5336, float %5346, float %5335
  %5349 = fmul fast float %5347, 5.000000e-01
  %5350 = fmul fast float %5348, 5.000000e-01
  %5351 = fadd fast float %5349, 5.000000e-01
  %5352 = fadd fast float %5350, 5.000000e-01
  %5353 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %5354 = extractvalue %dx.types.CBufRet.f32 %5353, 2
  %5355 = add i32 %5304, 6
  %5356 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5355, i32 undef)  ; BufferLoad(srv,index,wot)
  %5357 = extractvalue %dx.types.ResRet.f32 %5356, 0
  %5358 = fptoui float %5357 to i32
  %5359 = add i32 %5304, 7
  %5360 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5359, i32 undef)  ; BufferLoad(srv,index,wot)
  %5361 = extractvalue %dx.types.ResRet.f32 %5360, 0
  %5362 = fptoui float %5361 to i32
  %5363 = add i32 %5304, 8
  %5364 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5363, i32 undef)  ; BufferLoad(srv,index,wot)
  %5365 = extractvalue %dx.types.ResRet.f32 %5364, 0
  %5366 = fptoui float %5365 to i32
  %5367 = add i32 %5304, 9
  %5368 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %5, i32 %5367, i32 undef)  ; BufferLoad(srv,index,wot)
  %5369 = extractvalue %dx.types.ResRet.f32 %5368, 0
  %5370 = fptoui float %5369 to i32
  %5371 = icmp eq i32 %5370, 64
  %5372 = select i1 %5371, float 3.000000e+00, float %5354
  %5373 = fmul fast float %5372, %375
  %5374 = mul i32 %5370, %5358
  %5375 = mul i32 %5370, %5362
  %5376 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %5377 = extractvalue %dx.types.CBufRet.f32 %5376, 0
  %5378 = extractvalue %dx.types.CBufRet.f32 %5376, 1
  %5379 = extractvalue %dx.types.CBufRet.f32 %5376, 3
  %5380 = fmul fast float %5379, %5377
  %5381 = fmul fast float %5379, %5378
  %5382 = fptosi float %5372 to i32
  %5383 = call float @dx.op.unary.f32(i32 29, float %5373)  ; Round_z(value)
  %5384 = fsub fast float %5373, %5383
  %5385 = fptosi float %5383 to i32
  %5386 = call i32 @dx.op.binary.i32(i32 37, i32 %5385, i32 0)  ; IMax(a,b)
  %5387 = add nsw i32 %5386, 1
  %5388 = call i32 @dx.op.binary.i32(i32 38, i32 %5387, i32 %5382)  ; IMin(a,b)
  %5389 = sitofp i32 %5386 to float
  %5390 = call float @dx.op.unary.f32(i32 21, float %5389)  ; Exp(value)
  %5391 = fdiv fast float 1.000000e+00, %5390
  %5392 = sitofp i32 %5374 to float
  %5393 = sitofp i32 %5375 to float
  %5394 = fmul fast float %5391, %5392
  %5395 = fmul fast float %5391, %5393
  %5396 = fptosi float %5394 to i32
  %5397 = fptosi float %5395 to i32
  %5398 = sitofp i32 %5370 to float
  %5399 = fmul fast float %5391, %5398
  %5400 = fadd fast float %5399, -2.000000e+00
  %5401 = sitofp i32 %5366 to float
  %5402 = sitofp i32 %5396 to float
  %5403 = sitofp i32 %5397 to float
  %5404 = fmul fast float %5400, %5351
  %5405 = fmul fast float %5400, %5352
  %5406 = fadd fast float %5404, 1.000000e+00
  %5407 = fadd fast float %5405, 1.000000e+00
  %5408 = fadd fast float %5406, %5402
  %5409 = fadd fast float %5407, %5403
  %5410 = fmul fast float %5391, %5380
  %5411 = fmul fast float %5391, %5381
  %5412 = fdiv fast float %5408, %5410
  %5413 = fdiv fast float %5409, %5411
  %5414 = sitofp i32 %5388 to float
  %5415 = call float @dx.op.unary.f32(i32 21, float %5414)  ; Exp(value)
  %5416 = fdiv fast float 1.000000e+00, %5415
  %5417 = fmul fast float %5416, %5392
  %5418 = fmul fast float %5416, %5393
  %5419 = fptosi float %5417 to i32
  %5420 = fptosi float %5418 to i32
  %5421 = fmul fast float %5416, %5398
  %5422 = fadd fast float %5421, -2.000000e+00
  %5423 = sitofp i32 %5419 to float
  %5424 = sitofp i32 %5420 to float
  %5425 = fmul fast float %5422, %5351
  %5426 = fmul fast float %5422, %5352
  %5427 = fadd fast float %5425, 1.000000e+00
  %5428 = fadd fast float %5426, 1.000000e+00
  %5429 = fadd fast float %5427, %5423
  %5430 = fadd fast float %5428, %5424
  %5431 = fmul fast float %5416, %5380
  %5432 = fmul fast float %5416, %5381
  %5433 = fdiv fast float %5429, %5431
  %5434 = fdiv fast float %5430, %5432
  %5435 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %5412, float %5413, float %5401, float undef, i32 0, i32 0, i32 undef, float %5389)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5436 = extractvalue %dx.types.ResRet.f32 %5435, 0
  %5437 = extractvalue %dx.types.ResRet.f32 %5435, 1
  %5438 = extractvalue %dx.types.ResRet.f32 %5435, 2
  %5439 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %14, float %5433, float %5434, float %5401, float undef, i32 0, i32 0, i32 undef, float %5414)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5440 = extractvalue %dx.types.ResRet.f32 %5439, 0
  %5441 = extractvalue %dx.types.ResRet.f32 %5439, 1
  %5442 = extractvalue %dx.types.ResRet.f32 %5439, 2
  %5443 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %5412, float %5413, float %5401, float undef, i32 0, i32 0, i32 undef, float %5389)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5444 = extractvalue %dx.types.ResRet.f32 %5443, 0
  %5445 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %6, %dx.types.Handle %14, float %5433, float %5434, float %5401, float undef, i32 0, i32 0, i32 undef, float %5414)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %5446 = extractvalue %dx.types.ResRet.f32 %5445, 0
  %5447 = fsub fast float %5440, %5436
  %5448 = fsub fast float %5441, %5437
  %5449 = fsub fast float %5442, %5438
  %5450 = fsub fast float %5446, %5444
  %5451 = fmul fast float %5447, %5384
  %5452 = fmul fast float %5448, %5384
  %5453 = fmul fast float %5449, %5384
  %5454 = fmul fast float %5450, %5384
  %5455 = fadd fast float %5451, %5436
  %5456 = fadd fast float %5452, %5437
  %5457 = fadd fast float %5453, %5438
  %5458 = fadd fast float %5454, %5444
  %5459 = fmul fast float %5455, %5299
  %5460 = fmul fast float %5456, %5299
  %5461 = fmul fast float %5457, %5299
  %5462 = fmul fast float %5458, %5299
  %5463 = fadd fast float %5459, %5292
  %5464 = fadd fast float %5460, %5293
  %5465 = fadd fast float %5461, %5294
  %5466 = fadd fast float %5462, %5295
  %5467 = fadd fast float %5299, %5296
  br label %5468

; <label>:5468                                    ; preds = %5322, %5301, %5291
  %5469 = phi float [ %5292, %5291 ], [ %5292, %5301 ], [ %5463, %5322 ]
  %5470 = phi float [ %5293, %5291 ], [ %5293, %5301 ], [ %5464, %5322 ]
  %5471 = phi float [ %5294, %5291 ], [ %5294, %5301 ], [ %5465, %5322 ]
  %5472 = phi float [ %5295, %5291 ], [ %5295, %5301 ], [ %5466, %5322 ]
  %5473 = phi float [ %5296, %5291 ], [ %5296, %5301 ], [ %5467, %5322 ]
  %5474 = add nuw nsw i32 %5297, 1
  %5475 = icmp eq i32 %5474, 8
  br i1 %5475, label %5476, label %5291

; <label>:5476                                    ; preds = %5468
  %5477 = call float @dx.op.binary.f32(i32 35, float %5473, float 0x3EE9000020000000)  ; FMax(a,b)
  %5478 = fdiv fast float %5469, %5477
  %5479 = fdiv fast float %5470, %5477
  %5480 = fdiv fast float %5471, %5477
  %5481 = fdiv fast float %5472, %5477
  %5482 = fdiv fast float %5273, %5274
  %5483 = call float @dx.op.binary.f32(i32 35, float 0x3EE9000020000000, float %5482)  ; FMax(a,b)
  %5484 = fmul fast float %5483, %5478
  %5485 = fmul fast float %5483, %5479
  %5486 = fmul fast float %5483, %5480
  %5487 = fadd fast float %5481, -1.000000e+00
  %5488 = fmul fast float %5483, %5487
  %5489 = fadd fast float %5488, 1.000000e+00
  br label %5490

; <label>:5490                                    ; preds = %5476, %5281
  %5491 = phi float [ %5484, %5476 ], [ 0.000000e+00, %5281 ]
  %5492 = phi float [ %5485, %5476 ], [ 0.000000e+00, %5281 ]
  %5493 = phi float [ %5486, %5476 ], [ 0.000000e+00, %5281 ]
  %5494 = phi float [ %5489, %5476 ], [ 0.000000e+00, %5281 ]
  %5495 = call float @dx.op.binary.f32(i32 35, float %5494, float 0.000000e+00)  ; FMax(a,b)
  %5496 = call float @dx.op.binary.f32(i32 35, float %2108, float %5495)  ; FMax(a,b)
  br label %5497

; <label>:5497                                    ; preds = %5490, %4981
  %5498 = phi float [ %5491, %5490 ], [ 0.000000e+00, %4981 ]
  %5499 = phi float [ %5492, %5490 ], [ 0.000000e+00, %4981 ]
  %5500 = phi float [ %5493, %5490 ], [ 0.000000e+00, %4981 ]
  %5501 = phi float [ %5496, %5490 ], [ 0.000000e+00, %4981 ]
  %5502 = fsub fast float %5498, %4982
  %5503 = fsub fast float %5499, %4983
  %5504 = fsub fast float %5500, %4984
  %5505 = fsub fast float %5501, %4985
  %5506 = fmul fast float %5502, %2114
  %5507 = fmul fast float %5503, %2114
  %5508 = fmul fast float %5504, %2114
  %5509 = fmul fast float %5505, %2114
  %5510 = fadd fast float %5506, %4982
  %5511 = fadd fast float %5507, %4983
  %5512 = fadd fast float %5508, %4984
  %5513 = fadd fast float %5509, %4985
  %5514 = call float @dx.op.binary.f32(i32 35, float %5513, float %398)  ; FMax(a,b)
  %5515 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %5516 = extractvalue %dx.types.CBufRet.f32 %5515, 1
  %5517 = fmul fast float %5516, %5510
  %5518 = fmul fast float %5516, %5511
  %5519 = fmul fast float %5516, %5512
  %5520 = fmul fast float %5516, %5514
  br label %5521

; <label>:5521                                    ; preds = %5497, %329
  %5522 = phi float [ %5517, %5497 ], [ 0.000000e+00, %329 ]
  %5523 = phi float [ %5518, %5497 ], [ 0.000000e+00, %329 ]
  %5524 = phi float [ %5519, %5497 ], [ 0.000000e+00, %329 ]
  %5525 = phi float [ %5520, %5497 ], [ 0.000000e+00, %329 ]
  %5526 = xor i1 %331, true
  %5527 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %5528 = extractvalue %dx.types.CBufRet.f32 %5527, 1
  %5529 = fcmp fast ogt float %5528, 0.000000e+00
  %5530 = and i1 %5529, %5526
  br i1 %5530, label %5531, label %5538

; <label>:5531                                    ; preds = %5521
  %5532 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %5533 = extractvalue %dx.types.CBufRet.f32 %5532, 3
  %5534 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %19, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %5535 = extractvalue %dx.types.CBufRet.f32 %5534, 3
  %5536 = fmul fast float %5535, %5533
  %5537 = fmul fast float %5536, %5528
  br label %5538

; <label>:5538                                    ; preds = %5531, %5521
  %5539 = phi float [ %5537, %5531 ], [ %5525, %5521 ]
  %5540 = call float @dx.op.binary.f32(i32 35, float %5522, float 0.000000e+00)  ; FMax(a,b)
  %5541 = call float @dx.op.binary.f32(i32 35, float %5523, float 0.000000e+00)  ; FMax(a,b)
  %5542 = call float @dx.op.binary.f32(i32 35, float %5524, float 0.000000e+00)  ; FMax(a,b)
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2, i32 %20, i32 %21, i32 0, float %5540, float %5541, float %5542, float %5539, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %5543

; <label>:5543                                    ; preds = %5538, %1648, %356, %105, %100, %51, %0
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

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readonly
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

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
!dx.entryPoints = !{!31}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !20, !23, !25}
!5 = !{!6, !8, !9, !10, !11, !13, !14, !15, !16, !17, !19}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 13, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 11, i32 1, i32 2, i32 0, !7}
!10 = !{i32 3, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 10, i32 1, i32 7, i32 0, !7}
!11 = !{i32 4, %"class.Texture2D<vector<unsigned int, 4> >"* undef, !"", i32 0, i32 14, i32 1, i32 2, i32 0, !12}
!12 = !{i32 0, i32 5}
!13 = !{i32 5, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 7, i32 0, !7}
!14 = !{i32 6, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 7, i32 0, !7}
!15 = !{i32 7, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 6, i32 1, i32 7, i32 0, !7}
!16 = !{i32 8, %"class.Buffer<float>"* undef, !"", i32 0, i32 16, i32 1, i32 10, i32 0, !7}
!17 = !{i32 9, %"class.Buffer<int>"* undef, !"", i32 0, i32 17, i32 1, i32 10, i32 0, !18}
!18 = !{i32 0, i32 4}
!19 = !{i32 10, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 7, i32 1, i32 2, i32 0, !7}
!20 = !{!21, !22}
!21 = !{i32 0, %"class.RWTexture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 7, i1 false, i1 false, i1 false, !7}
!22 = !{i32 1, %"class.RWBuffer<float>"* undef, !"", i32 0, i32 4, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!23 = !{!24}
!24 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 544, null}
!25 = !{!26, !27, !28, !29, !30}
!26 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 13, i32 1, i32 0, null}
!27 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 12, i32 1, i32 0, null}
!28 = !{i32 2, %struct.SamplerState* undef, !"", i32 0, i32 11, i32 1, i32 0, null}
!29 = !{i32 3, %struct.SamplerState* undef, !"", i32 0, i32 9, i32 1, i32 0, null}
!30 = !{i32 4, %struct.SamplerState* undef, !"", i32 0, i32 10, i32 1, i32 0, null}
!31 = !{void ()* @main, !"main", null, !4, !32}
!32 = !{i32 4, !33}
!33 = !{i32 8, i32 8, i32 1}
!34 = !{!35, !35, i64 0}
!35 = !{!"int", !36, i64 0}
!36 = !{!"omnipotent char", !37, i64 0}
!37 = !{!"Simple C/C++ TBAA"}
!38 = !{!39, !41}
!39 = distinct !{!39, !40, !"\01?slDetectFieldCellForReflection@@YAXUslRay@@V?$vector@M$02@@AIAHAIAV?$vector@H$02@@@Z: %_lod"}
!40 = distinct !{!40, !"\01?slDetectFieldCellForReflection@@YAXUslRay@@V?$vector@M$02@@AIAHAIAV?$vector@H$02@@@Z"}
!41 = distinct !{!41, !40, !"\01?slDetectFieldCellForReflection@@YAXUslRay@@V?$vector@M$02@@AIAHAIAV?$vector@H$02@@@Z: %_base_cell_id"}
!42 = !{!43}
!43 = distinct !{!43, !44, !"\01?slGetValidProbes@@YAIUslRay@@HV?$vector@H$02@@Y07$$CAHH@Z: %_probes"}
!44 = distinct !{!44, !"\01?slGetValidProbes@@YAIUslRay@@HV?$vector@H$02@@Y07$$CAHH@Z"}
!45 = !{!46}
!46 = distinct !{!46, !47, !"\01?slGetValidProbes@@YAIUslRay@@HV?$vector@H$02@@Y07$$CAH@Z: %_probes"}
!47 = distinct !{!47, !"\01?slGetValidProbes@@YAIUslRay@@HV?$vector@H$02@@Y07$$CAH@Z"}
!48 = !{!49, !51}
!49 = distinct !{!49, !50, !"\01?TraceReflectionProbeFieldLOD_OPT@@YAHIUslRay@@MHUslTraceResult@@Y07H@Z: %_hit_res"}
!50 = distinct !{!50, !"\01?TraceReflectionProbeFieldLOD_OPT@@YAHIUslRay@@MHUslTraceResult@@Y07H@Z"}
!51 = distinct !{!51, !52, !"\01?TraceReflectionProbeField@@YAHIUslRay@@MHUslTraceResult@@Y07H@Z: %_hit_res"}
!52 = distinct !{!52, !"\01?TraceReflectionProbeField@@YAHIUslRay@@MHUslTraceResult@@Y07H@Z"}
!53 = !{!54, !54, i64 0}
!54 = !{!"float", !36, i64 0}
!55 = !{!56}
!56 = distinct !{!56, !57, !"\01?GetRayToOctahedronSegmentTime@@YAXV?$vector@M$02@@0V?$vector@M$01@@Y07$$CAM@Z: %segmentT"}
!57 = distinct !{!57, !"\01?GetRayToOctahedronSegmentTime@@YAXV?$vector@M$02@@0V?$vector@M$01@@Y07$$CAM@Z"}
!58 = !{!59, !49, !51}
!59 = distinct !{!59, !60, !"\01?TraceSingleReflectionProbe@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z: %_hit_res"}
!60 = distinct !{!60, !"\01?TraceSingleReflectionProbe@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z"}
!61 = !{!62, !64, !59, !49, !51}
!62 = distinct !{!62, !63, !"\01?TraceProbeSegment@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z: %_hit_res"}
!63 = distinct !{!63, !"\01?TraceProbeSegment@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z"}
!64 = distinct !{!64, !65, !"\01?TraceProbeOneSegment@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z: %_hit_res"}
!65 = distinct !{!65, !"\01?TraceProbeOneSegment@@YAHIUProbeRefInfo@@UslRay@@V?$vector@M$01@@UslTraceResult@@@Z"}
!66 = !{!67}
!67 = distinct !{!67, !68, !"\01?slGetPositionBaseApproximationRadianceFromProbeLOD_OBORO_unfolded0000@@YA?AULODResult@@UslRay@@V?$vector@M$02@@MV?$vector@H$02@@IY07H@Z: %agg.result"}
!68 = distinct !{!68, !"\01?slGetPositionBaseApproximationRadianceFromProbeLOD_OBORO_unfolded0000@@YA?AULODResult@@UslRay@@V?$vector@M$02@@MV?$vector@H$02@@IY07H@Z"}

