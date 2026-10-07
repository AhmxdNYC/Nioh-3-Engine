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
; shader debug name: 6703ebfcffac0acbcf45a0f8859c5338.pdb
; shader hash: 6703ebfcffac0acbcf45a0f8859c5338
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
;   struct $Globals
;   {
;
;       int nResX;                                    ; Offset:    0
;       int nResY;                                    ; Offset:    4
;       float fThick;                                 ; Offset:    8
;   
;   } $Globals;                                       ; Offset:    0 Size:    12
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; sInputTex                         texture     f32          2d      T0             t8     1
; sDepth                            texture     f32          2d      T1             t9     1
; sStencil                          texture     u32          2d      T2            t10     1
; uOutputTex                            UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2D<float>" = type { float, %"class.Texture2D<float>::mips_type" }
%"class.Texture2D<float>::mips_type" = type { i32 }
%"class.Texture2D<vector<unsigned int, 2> >" = type { <2 x i32>, %"class.Texture2D<vector<unsigned int, 2> >::mips_type" }
%"class.Texture2D<vector<unsigned int, 2> >::mips_type" = type { i32 }
%"class.RWTexture2D<float>" = type { float }
%"$Globals" = type { i32, i32, float }

@sOffset.0 = internal unnamed_addr constant [8 x i32] [i32 0, i32 -1, i32 1, i32 0, i32 -1, i32 1, i32 -1, i32 1]
@sOffset.1 = internal unnamed_addr constant [8 x i32] [i32 -1, i32 0, i32 0, i32 1, i32 -1, i32 -1, i32 1, i32 1]

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %7 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %8 = alloca [8 x float], align 4
  %9 = alloca [8 x float], align 4
  %10 = alloca [8 x float], align 4
  %11 = alloca [8 x float], align 4
  %12 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %4, i32 0, i32 %6, i32 %7, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %13 = extractvalue %dx.types.ResRet.f32 %12, 0
  %14 = extractvalue %dx.types.ResRet.f32 %12, 1
  %15 = extractvalue %dx.types.ResRet.f32 %12, 2
  %16 = extractvalue %dx.types.ResRet.f32 %12, 3
  %17 = fcmp fast une float %13, 0.000000e+00
  %18 = fcmp fast une float %14, 0.000000e+00
  %19 = or i1 %17, %18
  %20 = fcmp fast une float %15, 0.000000e+00
  %21 = or i1 %20, %19
  %22 = fcmp fast une float %16, 0.000000e+00
  %23 = or i1 %22, %21
  br i1 %23, label %259, label %24

; <label>:24                                      ; preds = %0
  br label %25

; <label>:25                                      ; preds = %25, %24
  %26 = phi i32 [ %51, %25 ], [ 0, %24 ]
  %27 = getelementptr [8 x i32], [8 x i32]* @sOffset.0, i32 0, i32 %26
  %28 = getelementptr [8 x i32], [8 x i32]* @sOffset.1, i32 0, i32 %26
  %29 = load i32, i32* %27, align 4
  %30 = load i32, i32* %28, align 4
  %31 = add i32 %29, %6
  %32 = add i32 %30, %7
  %33 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %5, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %34 = extractvalue %dx.types.CBufRet.i32 %33, 0
  %35 = add nsw i32 %34, -1
  %36 = extractvalue %dx.types.CBufRet.i32 %33, 1
  %37 = add nsw i32 %36, -1
  %38 = call i32 @dx.op.binary.i32(i32 37, i32 %31, i32 0)  ; IMax(a,b)
  %39 = call i32 @dx.op.binary.i32(i32 37, i32 %32, i32 0)  ; IMax(a,b)
  %40 = call i32 @dx.op.binary.i32(i32 38, i32 %38, i32 %35)  ; IMin(a,b)
  %41 = call i32 @dx.op.binary.i32(i32 38, i32 %39, i32 %37)  ; IMin(a,b)
  %42 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %4, i32 0, i32 %40, i32 %41, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %43 = extractvalue %dx.types.ResRet.f32 %42, 0
  %44 = extractvalue %dx.types.ResRet.f32 %42, 1
  %45 = extractvalue %dx.types.ResRet.f32 %42, 2
  %46 = extractvalue %dx.types.ResRet.f32 %42, 3
  %47 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 %26
  %48 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 %26
  %49 = getelementptr [8 x float], [8 x float]* %10, i32 0, i32 %26
  %50 = getelementptr [8 x float], [8 x float]* %11, i32 0, i32 %26
  store float %43, float* %47, align 4
  store float %44, float* %48, align 4
  store float %45, float* %49, align 4
  store float %46, float* %50, align 4
  %51 = add nuw nsw i32 %26, 1
  %52 = icmp eq i32 %51, 8
  br i1 %52, label %53, label %25

; <label>:53                                      ; preds = %25
  %54 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %2, i32 0, i32 %6, i32 %7, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %55 = extractvalue %dx.types.ResRet.i32 %54, 1
  %56 = and i32 %55, 1
  %57 = icmp eq i32 %56, 0
  br i1 %57, label %58, label %259

; <label>:58                                      ; preds = %53
  %59 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %60 = extractvalue %dx.types.CBufRet.f32 %59, 2
  %61 = fadd fast float %60, 5.000000e-01
  %62 = fmul fast float %61, 2.500000e-01
  %63 = call i32 @dx.op.binary.i32(i32 37, i32 %6, i32 0)  ; IMax(a,b)
  %64 = call i32 @dx.op.binary.i32(i32 37, i32 %7, i32 0)  ; IMax(a,b)
  %65 = call i32 @dx.op.binary.i32(i32 38, i32 %63, i32 %35)  ; IMin(a,b)
  %66 = call i32 @dx.op.binary.i32(i32 38, i32 %64, i32 %37)  ; IMin(a,b)
  %67 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %3, i32 0, i32 %65, i32 %66, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %68 = extractvalue %dx.types.ResRet.f32 %67, 0
  br label %69

; <label>:69                                      ; preds = %108, %58
  %70 = phi float [ %61, %58 ], [ %109, %108 ]
  %71 = phi i32 [ 0, %58 ], [ %110, %108 ]
  %72 = getelementptr [8 x i32], [8 x i32]* @sOffset.0, i32 0, i32 %71
  %73 = getelementptr [8 x i32], [8 x i32]* @sOffset.1, i32 0, i32 %71
  %74 = load i32, i32* %72, align 4
  %75 = load i32, i32* %73, align 4
  %76 = add i32 %74, %6
  %77 = add i32 %75, %7
  %78 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %5, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %79 = extractvalue %dx.types.CBufRet.i32 %78, 0
  %80 = add nsw i32 %79, -1
  %81 = extractvalue %dx.types.CBufRet.i32 %78, 1
  %82 = add nsw i32 %81, -1
  %83 = call i32 @dx.op.binary.i32(i32 37, i32 %76, i32 0)  ; IMax(a,b)
  %84 = call i32 @dx.op.binary.i32(i32 37, i32 %77, i32 0)  ; IMax(a,b)
  %85 = call i32 @dx.op.binary.i32(i32 38, i32 %83, i32 %80)  ; IMin(a,b)
  %86 = call i32 @dx.op.binary.i32(i32 38, i32 %84, i32 %82)  ; IMin(a,b)
  %87 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %3, i32 0, i32 %85, i32 %86, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %88 = extractvalue %dx.types.ResRet.f32 %87, 0
  %89 = fcmp fast ult float %88, %68
  br i1 %89, label %108, label %90

; <label>:90                                      ; preds = %69
  %91 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 %71
  %92 = load float, float* %91, align 4
  %93 = fcmp fast une float %92, 0.000000e+00
  %94 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 %71
  %95 = load float, float* %94, align 4
  %96 = fcmp fast une float %95, 0.000000e+00
  %97 = or i1 %93, %96
  %98 = getelementptr [8 x float], [8 x float]* %10, i32 0, i32 %71
  %99 = load float, float* %98, align 4
  %100 = fcmp fast une float %99, 0.000000e+00
  %101 = or i1 %97, %100
  %102 = getelementptr [8 x float], [8 x float]* %11, i32 0, i32 %71
  %103 = load float, float* %102, align 4
  %104 = fcmp fast une float %103, 0.000000e+00
  %105 = or i1 %101, %104
  %106 = fsub fast float %70, %62
  %107 = select i1 %105, float %106, float %70
  br label %108

; <label>:108                                     ; preds = %90, %69
  %109 = phi float [ %70, %69 ], [ %107, %90 ]
  %110 = add nuw nsw i32 %71, 1
  %111 = icmp eq i32 %110, 8
  br i1 %111, label %112, label %69

; <label>:112                                     ; preds = %108
  %113 = call float @dx.op.binary.f32(i32 35, float %109, float 0.000000e+00)  ; FMax(a,b)
  %114 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 0
  %115 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 0
  %116 = load float, float* %114, align 4
  %117 = load float, float* %115, align 4
  %118 = fmul fast float %116, 2.550000e+02
  %119 = fmul fast float %117, 2.550000e+02
  %120 = fadd fast float %118, 5.000000e-01
  %121 = fadd fast float %119, 5.000000e-01
  %122 = fptosi float %120 to i32
  %123 = fptosi float %121 to i32
  %124 = fmul fast float %13, 2.550000e+02
  %125 = fmul fast float %14, 2.550000e+02
  %126 = fadd fast float %124, 5.000000e-01
  %127 = fadd fast float %125, 5.000000e-01
  %128 = fptosi float %126 to i32
  %129 = fptosi float %127 to i32
  %130 = icmp ne i32 %122, %128
  %131 = icmp ne i32 %123, %129
  %132 = or i1 %130, %131
  %133 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 1
  %134 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 1
  %135 = load float, float* %133, align 4
  %136 = load float, float* %134, align 4
  %137 = fmul fast float %135, 2.550000e+02
  %138 = fmul fast float %136, 2.550000e+02
  %139 = fadd fast float %137, 5.000000e-01
  %140 = fadd fast float %138, 5.000000e-01
  %141 = fptosi float %139 to i32
  %142 = fptosi float %140 to i32
  %143 = icmp ne i32 %141, %128
  %144 = icmp ne i32 %142, %129
  %145 = or i1 %143, %144
  %146 = or i1 %132, %145
  %147 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 2
  %148 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 2
  %149 = load float, float* %147, align 4
  %150 = load float, float* %148, align 4
  %151 = fmul fast float %149, 2.550000e+02
  %152 = fmul fast float %150, 2.550000e+02
  %153 = fadd fast float %151, 5.000000e-01
  %154 = fadd fast float %152, 5.000000e-01
  %155 = fptosi float %153 to i32
  %156 = fptosi float %154 to i32
  %157 = icmp ne i32 %155, %128
  %158 = icmp ne i32 %156, %129
  %159 = or i1 %157, %158
  %160 = or i1 %146, %159
  %161 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 3
  %162 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 3
  %163 = load float, float* %161, align 4
  %164 = load float, float* %162, align 4
  %165 = fmul fast float %163, 2.550000e+02
  %166 = fmul fast float %164, 2.550000e+02
  %167 = fadd fast float %165, 5.000000e-01
  %168 = fadd fast float %166, 5.000000e-01
  %169 = fptosi float %167 to i32
  %170 = fptosi float %168 to i32
  %171 = icmp ne i32 %169, %128
  %172 = icmp ne i32 %170, %129
  %173 = or i1 %171, %172
  %174 = or i1 %160, %173
  br i1 %174, label %175, label %189

; <label>:175                                     ; preds = %112
  %176 = fcmp fast ogt float %116, 0.000000e+00
  %177 = fcmp fast ogt float %135, 0.000000e+00
  %178 = or i1 %176, %177
  %179 = fcmp fast ogt float %149, 0.000000e+00
  %180 = or i1 %178, %179
  %181 = fcmp fast ogt float %163, 0.000000e+00
  %182 = or i1 %180, %181
  %183 = fcmp fast ogt float %13, 0.000000e+00
  %184 = or i1 %183, %182
  br i1 %184, label %185, label %187

; <label>:185                                     ; preds = %175
  %186 = fsub fast float -5.000000e-01, %113
  br label %259

; <label>:187                                     ; preds = %175
  %188 = fadd fast float %113, 5.000000e-01
  br label %259

; <label>:189                                     ; preds = %112
  %190 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 4
  %191 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 4
  %192 = load float, float* %190, align 4
  %193 = load float, float* %191, align 4
  %194 = fmul fast float %192, 2.550000e+02
  %195 = fmul fast float %193, 2.550000e+02
  %196 = fadd fast float %194, 5.000000e-01
  %197 = fadd fast float %195, 5.000000e-01
  %198 = fptosi float %196 to i32
  %199 = fptosi float %197 to i32
  %200 = icmp ne i32 %198, %122
  %201 = icmp ne i32 %199, %123
  %202 = or i1 %200, %201
  %203 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 5
  %204 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 5
  %205 = load float, float* %203, align 4
  %206 = load float, float* %204, align 4
  %207 = fmul fast float %205, 2.550000e+02
  %208 = fmul fast float %206, 2.550000e+02
  %209 = fadd fast float %207, 5.000000e-01
  %210 = fadd fast float %208, 5.000000e-01
  %211 = fptosi float %209 to i32
  %212 = fptosi float %210 to i32
  %213 = icmp ne i32 %211, %122
  %214 = icmp ne i32 %212, %123
  %215 = or i1 %213, %214
  %216 = or i1 %202, %215
  %217 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 6
  %218 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 6
  %219 = load float, float* %217, align 4
  %220 = load float, float* %218, align 4
  %221 = fmul fast float %219, 2.550000e+02
  %222 = fmul fast float %220, 2.550000e+02
  %223 = fadd fast float %221, 5.000000e-01
  %224 = fadd fast float %222, 5.000000e-01
  %225 = fptosi float %223 to i32
  %226 = fptosi float %224 to i32
  %227 = icmp ne i32 %225, %122
  %228 = icmp ne i32 %226, %123
  %229 = or i1 %227, %228
  %230 = or i1 %216, %229
  %231 = getelementptr [8 x float], [8 x float]* %8, i32 0, i32 7
  %232 = getelementptr [8 x float], [8 x float]* %9, i32 0, i32 7
  %233 = load float, float* %231, align 4
  %234 = load float, float* %232, align 4
  %235 = fmul fast float %233, 2.550000e+02
  %236 = fmul fast float %234, 2.550000e+02
  %237 = fadd fast float %235, 5.000000e-01
  %238 = fadd fast float %236, 5.000000e-01
  %239 = fptosi float %237 to i32
  %240 = fptosi float %238 to i32
  %241 = icmp ne i32 %239, %122
  %242 = icmp ne i32 %240, %123
  %243 = or i1 %241, %242
  %244 = or i1 %230, %243
  br i1 %244, label %245, label %259

; <label>:245                                     ; preds = %189
  %246 = fcmp fast ogt float %192, 0.000000e+00
  %247 = fcmp fast ogt float %205, 0.000000e+00
  %248 = or i1 %246, %247
  %249 = fcmp fast ogt float %219, 0.000000e+00
  %250 = or i1 %248, %249
  %251 = fcmp fast ogt float %233, 0.000000e+00
  %252 = or i1 %250, %251
  %253 = fcmp fast ogt float %13, 0.000000e+00
  %254 = or i1 %253, %252
  br i1 %254, label %255, label %257

; <label>:255                                     ; preds = %245
  %256 = fsub fast float 0xBFE6A09EE0000000, %113
  br label %259

; <label>:257                                     ; preds = %245
  %258 = fadd fast float %113, 0x3FE6A09EE0000000
  br label %259

; <label>:259                                     ; preds = %257, %255, %189, %187, %185, %53, %0
  %260 = phi float [ %186, %185 ], [ %188, %187 ], [ %256, %255 ], [ %258, %257 ], [ 0x47EFFFFFA0000000, %0 ], [ 0x47EFFFFFA0000000, %53 ], [ 0x47EFFFFFA0000000, %189 ]
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %6, i32 %7, i32 undef, float %260, float %260, float %260, float %260, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

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
!dx.entryPoints = !{!15}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !11, !13, null}
!5 = !{!6, !8, !9}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<float>"* undef, !"", i32 0, i32 9, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<unsigned int, 2> >"* undef, !"", i32 0, i32 10, i32 1, i32 2, i32 0, !10}
!10 = !{i32 0, i32 5}
!11 = !{!12}
!12 = !{i32 0, %"class.RWTexture2D<float>"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!13 = !{!14}
!14 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 12, null}
!15 = !{void ()* @main, !"main", null, !4, !16}
!16 = !{i32 4, !17}
!17 = !{i32 16, i32 16, i32 1}

