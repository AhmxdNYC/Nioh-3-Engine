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
; shader debug name: e0e269f8cd2db1bf0aa83fb7a6823cf2.pdb
; shader hash: e0e269f8cd2db1bf0aa83fb7a6823cf2
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
;       uint SIZE;                                    ; Offset:    0
;       float2 RANGE;                                 ; Offset:    4
;       float WIND;                                   ; Offset:   12
;       float TIME;                                   ; Offset:   16
;       float SCALE;                                  ; Offset:   20
;       float CHOPPY;                                 ; Offset:   24
;       uint GRID;                                    ; Offset:   28
;   
;   } $Globals;                                       ; Offset:    0 Size:    32
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; PROFILE                               UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%"$Globals" = type { i32, <2 x float>, float, float, float, float, i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %4 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %2, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %5 = extractvalue %dx.types.CBufRet.i32 %4, 0
  %6 = icmp ult i32 %3, %5
  br i1 %6, label %7, label %388

; <label>:7                                       ; preds = %0
  %8 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %9 = extractvalue %dx.types.CBufRet.f32 %8, 2
  %10 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %11 = extractvalue %dx.types.CBufRet.f32 %10, 1
  %12 = uitofp i32 %3 to float
  %13 = fmul fast float %9, %12
  %14 = fmul fast float %13, %11
  %15 = uitofp i32 %5 to float
  %16 = fdiv fast float %14, %15
  %17 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %2, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %18 = extractvalue %dx.types.CBufRet.i32 %17, 3
  %19 = icmp ne i32 %18, 0
  %20 = and i32 %18, 1
  %21 = add i32 %20, %18
  %22 = select i1 %19, i32 %21, i32 2
  %23 = extractvalue %dx.types.CBufRet.f32 %8, 1
  %24 = call float @dx.op.unary.f32(i32 23, float %23)  ; Log(value)
  %25 = call float @dx.op.unary.f32(i32 23, float %9)  ; Log(value)
  %26 = fsub fast float %25, %24
  %27 = uitofp i32 %22 to float
  %28 = fdiv fast float %26, %27
  %29 = fmul fast float %28, 0x3FD5555560000000
  %30 = fmul fast float %11, %9
  %31 = fdiv fast float %16, %30
  %32 = call float @dx.op.unary.f32(i32 21, float %24)  ; Exp(value)
  %33 = fdiv fast float 0x404ECEF940000000, %32
  %34 = call float @dx.op.unary.f32(i32 24, float %33)  ; Sqrt(value)
  %35 = fmul fast float %34, 0x40B6619440000000
  %36 = call float @dx.op.unary.f32(i32 28, float %35)  ; Round_pi(value)
  %37 = fmul fast float %36, 0x3F26E05A20000000
  %38 = call float @dx.op.unary.f32(i32 23, float %37)  ; Log(value)
  %39 = fmul fast float %38, 2.000000e+00
  %40 = call float @dx.op.unary.f32(i32 21, float %39)  ; Exp(value)
  %41 = fmul fast float %40, 0x3FBA1AD000000000
  %42 = fmul fast float %41, %16
  %43 = fmul fast float %38, 5.000000e+00
  %44 = call float @dx.op.unary.f32(i32 21, float %43)  ; Exp(value)
  %45 = fdiv fast float 0x3FE8ED6760000000, %44
  %46 = extractvalue %dx.types.CBufRet.f32 %8, 3
  %47 = fmul fast float %46, %36
  %48 = fdiv fast float 0x40EB6F7F20000000, %47
  %49 = call float @dx.op.unary.f32(i32 23, float %48)  ; Log(value)
  %50 = fmul fast float %49, 4.000000e+00
  %51 = call float @dx.op.unary.f32(i32 21, float %50)  ; Exp(value)
  %52 = fmul fast float %51, 0xBFF114DDC0000000
  %53 = call float @dx.op.unary.f32(i32 21, float %52)  ; Exp(value)
  %54 = fmul fast float %53, %45
  %55 = call float @dx.op.unary.f32(i32 24, float %54)  ; Sqrt(value)
  %56 = fmul fast float %55, 2.625000e+01
  %57 = extractvalue %dx.types.CBufRet.f32 %10, 0
  %58 = fmul fast float %57, %37
  %59 = fsub fast float %42, %58
  %60 = call float @dx.op.unary.f32(i32 13, float %59)  ; Sin(value)
  %61 = extractvalue %dx.types.CBufRet.f32 %10, 2
  %62 = call float @dx.op.unary.f32(i32 12, float %59)  ; Cos(value)
  %63 = fmul fast float %62, %56
  %64 = fsub fast float 1.000000e+00, %31
  %65 = fsub fast float %16, %30
  %66 = fmul fast float %41, %65
  %67 = fsub fast float %66, %58
  %68 = call float @dx.op.unary.f32(i32 13, float %67)  ; Sin(value)
  %69 = call float @dx.op.unary.f32(i32 12, float %67)  ; Cos(value)
  %70 = fmul fast float %69, %56
  %71 = fcmp fast olt float %31, 0.000000e+00
  %72 = fcmp fast ogt float %31, 1.000000e+00
  %73 = and i1 %71, %72
  br i1 %73, label %84, label %74

; <label>:74                                      ; preds = %7
  %75 = call float @dx.op.unary.f32(i32 23, float %31)  ; Log(value)
  %76 = fmul fast float %75, 3.000000e+00
  %77 = call float @dx.op.unary.f32(i32 21, float %76)  ; Exp(value)
  %78 = fmul fast float %77, 2.000000e+00
  %79 = fmul fast float %75, 2.000000e+00
  %80 = call float @dx.op.unary.f32(i32 21, float %79)  ; Exp(value)
  %81 = fmul fast float %80, 3.000000e+00
  %82 = fadd fast float %78, 1.000000e+00
  %83 = fsub fast float %82, %81
  br label %84

; <label>:84                                      ; preds = %74, %7
  %85 = phi float [ %83, %74 ], [ 0.000000e+00, %7 ]
  %86 = fcmp fast olt float %64, 0.000000e+00
  %87 = fcmp fast ogt float %64, 1.000000e+00
  %88 = and i1 %86, %87
  br i1 %88, label %99, label %89

; <label>:89                                      ; preds = %84
  %90 = call float @dx.op.unary.f32(i32 23, float %64)  ; Log(value)
  %91 = fmul fast float %90, 3.000000e+00
  %92 = call float @dx.op.unary.f32(i32 21, float %91)  ; Exp(value)
  %93 = fmul fast float %92, 2.000000e+00
  %94 = fmul fast float %90, 2.000000e+00
  %95 = call float @dx.op.unary.f32(i32 21, float %94)  ; Exp(value)
  %96 = fmul fast float %95, 3.000000e+00
  %97 = fadd fast float %93, 1.000000e+00
  %98 = fsub fast float %97, %96
  br label %99

; <label>:99                                      ; preds = %89, %84
  %100 = phi float [ %98, %89 ], [ 0.000000e+00, %84 ]
  %101 = fmul fast float %60, %56
  %102 = fmul fast float %101, %61
  %103 = fmul fast float %102, %85
  %104 = fmul fast float %63, %85
  %105 = fmul fast float %56, %41
  %106 = fmul fast float %105, %62
  %107 = fmul fast float %106, %61
  %108 = fmul fast float %107, %85
  %109 = fmul fast float %105, %60
  %110 = fmul fast float %109, %85
  %111 = fmul fast float %55, -2.625000e+01
  %112 = fmul fast float %111, %68
  %113 = fmul fast float %112, %61
  %114 = fmul fast float %113, %100
  %115 = fmul fast float %70, %100
  %116 = fmul fast float %40, 0xBFBA1AD000000000
  %117 = fmul fast float %116, %56
  %118 = fmul fast float %117, %69
  %119 = fmul fast float %118, %61
  %120 = fmul fast float %119, %100
  %121 = fmul fast float %117, %68
  %122 = fmul fast float %121, %100
  %123 = fsub fast float %114, %103
  %124 = fadd fast float %115, %104
  %125 = fsub fast float %120, %108
  %126 = fsub fast float %122, %110
  %127 = fmul fast float %123, %29
  %128 = fmul fast float %124, %29
  %129 = fmul fast float %125, %29
  %130 = fmul fast float %126, %29
  %131 = add i32 %22, 1
  %132 = icmp ugt i32 %131, 1
  br i1 %132, label %133, label %383

; <label>:133                                     ; preds = %99
  br label %134

; <label>:134                                     ; preds = %371, %133
  %135 = phi float [ %379, %371 ], [ %127, %133 ]
  %136 = phi float [ %378, %371 ], [ %128, %133 ]
  %137 = phi float [ %377, %371 ], [ %129, %133 ]
  %138 = phi float [ %376, %371 ], [ %130, %133 ]
  %139 = phi i32 [ %380, %371 ], [ 1, %133 ]
  %140 = uitofp i32 %139 to float
  %141 = fmul fast float %140, %28
  %142 = fadd fast float %141, %24
  %143 = icmp eq i32 %139, %22
  %144 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %145 = extractvalue %dx.types.CBufRet.f32 %144, 2
  %146 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %147 = extractvalue %dx.types.CBufRet.f32 %146, 1
  %148 = fmul fast float %147, %145
  %149 = fdiv fast float %16, %148
  %150 = call float @dx.op.unary.f32(i32 21, float %142)  ; Exp(value)
  %151 = fdiv fast float 0x404ECEF940000000, %150
  %152 = call float @dx.op.unary.f32(i32 24, float %151)  ; Sqrt(value)
  %153 = fmul fast float %152, 0x40B6619440000000
  %154 = call float @dx.op.unary.f32(i32 28, float %153)  ; Round_pi(value)
  %155 = fmul fast float %154, 0x3F26E05A20000000
  %156 = call float @dx.op.unary.f32(i32 23, float %155)  ; Log(value)
  %157 = fmul fast float %156, 2.000000e+00
  %158 = call float @dx.op.unary.f32(i32 21, float %157)  ; Exp(value)
  %159 = fmul fast float %158, 0x3FBA1AD000000000
  %160 = fmul fast float %159, %16
  %161 = fmul fast float %156, 5.000000e+00
  %162 = call float @dx.op.unary.f32(i32 21, float %161)  ; Exp(value)
  %163 = fdiv fast float 0x3FE8ED6760000000, %162
  %164 = extractvalue %dx.types.CBufRet.f32 %144, 3
  %165 = fmul fast float %164, %154
  %166 = fdiv fast float 0x40EB6F7F20000000, %165
  %167 = call float @dx.op.unary.f32(i32 23, float %166)  ; Log(value)
  %168 = fmul fast float %167, 4.000000e+00
  %169 = call float @dx.op.unary.f32(i32 21, float %168)  ; Exp(value)
  %170 = fmul fast float %169, 0xBFF114DDC0000000
  %171 = call float @dx.op.unary.f32(i32 21, float %170)  ; Exp(value)
  %172 = fmul fast float %171, %163
  %173 = call float @dx.op.unary.f32(i32 24, float %172)  ; Sqrt(value)
  %174 = fmul fast float %173, 2.625000e+01
  %175 = extractvalue %dx.types.CBufRet.f32 %146, 0
  %176 = fmul fast float %175, %155
  %177 = fsub fast float %160, %176
  %178 = call float @dx.op.unary.f32(i32 13, float %177)  ; Sin(value)
  %179 = extractvalue %dx.types.CBufRet.f32 %146, 2
  %180 = call float @dx.op.unary.f32(i32 12, float %177)  ; Cos(value)
  %181 = fmul fast float %180, %174
  %182 = fsub fast float 1.000000e+00, %149
  %183 = fsub fast float %16, %148
  %184 = fmul fast float %159, %183
  %185 = fsub fast float %184, %176
  %186 = call float @dx.op.unary.f32(i32 13, float %185)  ; Sin(value)
  %187 = call float @dx.op.unary.f32(i32 12, float %185)  ; Cos(value)
  %188 = fmul fast float %187, %174
  %189 = fcmp fast olt float %149, 0.000000e+00
  %190 = fcmp fast ogt float %149, 1.000000e+00
  %191 = and i1 %189, %190
  br i1 %143, label %192, label %250

; <label>:192                                     ; preds = %134
  br i1 %191, label %203, label %193

; <label>:193                                     ; preds = %192
  %194 = call float @dx.op.unary.f32(i32 23, float %149)  ; Log(value)
  %195 = fmul fast float %194, 3.000000e+00
  %196 = call float @dx.op.unary.f32(i32 21, float %195)  ; Exp(value)
  %197 = fmul fast float %196, 2.000000e+00
  %198 = fmul fast float %194, 2.000000e+00
  %199 = call float @dx.op.unary.f32(i32 21, float %198)  ; Exp(value)
  %200 = fmul fast float %199, 3.000000e+00
  %201 = fadd fast float %197, 1.000000e+00
  %202 = fsub fast float %201, %200
  br label %203

; <label>:203                                     ; preds = %193, %192
  %204 = phi float [ %202, %193 ], [ 0.000000e+00, %192 ]
  %205 = fcmp fast olt float %182, 0.000000e+00
  %206 = fcmp fast ogt float %182, 1.000000e+00
  %207 = and i1 %205, %206
  br i1 %207, label %218, label %208

; <label>:208                                     ; preds = %203
  %209 = call float @dx.op.unary.f32(i32 23, float %182)  ; Log(value)
  %210 = fmul fast float %209, 3.000000e+00
  %211 = call float @dx.op.unary.f32(i32 21, float %210)  ; Exp(value)
  %212 = fmul fast float %211, 2.000000e+00
  %213 = fmul fast float %209, 2.000000e+00
  %214 = call float @dx.op.unary.f32(i32 21, float %213)  ; Exp(value)
  %215 = fmul fast float %214, 3.000000e+00
  %216 = fadd fast float %212, 1.000000e+00
  %217 = fsub fast float %216, %215
  br label %218

; <label>:218                                     ; preds = %208, %203
  %219 = phi float [ %217, %208 ], [ 0.000000e+00, %203 ]
  %220 = fmul fast float %178, %174
  %221 = fmul fast float %220, %179
  %222 = fmul fast float %221, %204
  %223 = fmul fast float %181, %204
  %224 = fmul fast float %174, %159
  %225 = fmul fast float %224, %180
  %226 = fmul fast float %225, %179
  %227 = fmul fast float %226, %204
  %228 = fmul fast float %224, %178
  %229 = fmul fast float %228, %204
  %230 = fmul fast float %173, -2.625000e+01
  %231 = fmul fast float %230, %186
  %232 = fmul fast float %231, %179
  %233 = fmul fast float %232, %219
  %234 = fmul fast float %188, %219
  %235 = fmul fast float %158, 0xBFBA1AD000000000
  %236 = fmul fast float %235, %174
  %237 = fmul fast float %236, %187
  %238 = fmul fast float %237, %179
  %239 = fmul fast float %238, %219
  %240 = fmul fast float %236, %186
  %241 = fmul fast float %240, %219
  %242 = fsub fast float %233, %222
  %243 = fadd fast float %234, %223
  %244 = fsub fast float %239, %227
  %245 = fsub fast float %241, %229
  %246 = fmul fast float %242, %29
  %247 = fmul fast float %243, %29
  %248 = fmul fast float %244, %29
  %249 = fmul fast float %245, %29
  br label %371

; <label>:250                                     ; preds = %134
  %251 = and i32 %139, 1
  %252 = icmp eq i32 %251, 0
  br i1 %252, label %253, label %312

; <label>:253                                     ; preds = %250
  %254 = fmul fast float %28, 0x3FE5555560000000
  br i1 %191, label %265, label %255

; <label>:255                                     ; preds = %253
  %256 = call float @dx.op.unary.f32(i32 23, float %149)  ; Log(value)
  %257 = fmul fast float %256, 3.000000e+00
  %258 = call float @dx.op.unary.f32(i32 21, float %257)  ; Exp(value)
  %259 = fmul fast float %258, 2.000000e+00
  %260 = fmul fast float %256, 2.000000e+00
  %261 = call float @dx.op.unary.f32(i32 21, float %260)  ; Exp(value)
  %262 = fmul fast float %261, 3.000000e+00
  %263 = fadd fast float %259, 1.000000e+00
  %264 = fsub fast float %263, %262
  br label %265

; <label>:265                                     ; preds = %255, %253
  %266 = phi float [ %264, %255 ], [ 0.000000e+00, %253 ]
  %267 = fcmp fast olt float %182, 0.000000e+00
  %268 = fcmp fast ogt float %182, 1.000000e+00
  %269 = and i1 %267, %268
  br i1 %269, label %280, label %270

; <label>:270                                     ; preds = %265
  %271 = call float @dx.op.unary.f32(i32 23, float %182)  ; Log(value)
  %272 = fmul fast float %271, 3.000000e+00
  %273 = call float @dx.op.unary.f32(i32 21, float %272)  ; Exp(value)
  %274 = fmul fast float %273, 2.000000e+00
  %275 = fmul fast float %271, 2.000000e+00
  %276 = call float @dx.op.unary.f32(i32 21, float %275)  ; Exp(value)
  %277 = fmul fast float %276, 3.000000e+00
  %278 = fadd fast float %274, 1.000000e+00
  %279 = fsub fast float %278, %277
  br label %280

; <label>:280                                     ; preds = %270, %265
  %281 = phi float [ %279, %270 ], [ 0.000000e+00, %265 ]
  %282 = fmul fast float %178, %174
  %283 = fmul fast float %282, %179
  %284 = fmul fast float %283, %266
  %285 = fmul fast float %181, %266
  %286 = fmul fast float %174, %159
  %287 = fmul fast float %286, %180
  %288 = fmul fast float %287, %179
  %289 = fmul fast float %288, %266
  %290 = fmul fast float %286, %178
  %291 = fmul fast float %290, %266
  %292 = fmul fast float %173, -2.625000e+01
  %293 = fmul fast float %292, %186
  %294 = fmul fast float %293, %179
  %295 = fmul fast float %294, %281
  %296 = fmul fast float %188, %281
  %297 = fmul fast float %158, 0xBFBA1AD000000000
  %298 = fmul fast float %297, %174
  %299 = fmul fast float %298, %187
  %300 = fmul fast float %299, %179
  %301 = fmul fast float %300, %281
  %302 = fmul fast float %298, %186
  %303 = fmul fast float %302, %281
  %304 = fsub fast float %295, %284
  %305 = fadd fast float %296, %285
  %306 = fsub fast float %301, %289
  %307 = fsub fast float %303, %291
  %308 = fmul fast float %304, %254
  %309 = fmul fast float %305, %254
  %310 = fmul fast float %306, %254
  %311 = fmul fast float %307, %254
  br label %371

; <label>:312                                     ; preds = %250
  %313 = fmul fast float %28, 0x3FF5555560000000
  br i1 %191, label %324, label %314

; <label>:314                                     ; preds = %312
  %315 = call float @dx.op.unary.f32(i32 23, float %149)  ; Log(value)
  %316 = fmul fast float %315, 3.000000e+00
  %317 = call float @dx.op.unary.f32(i32 21, float %316)  ; Exp(value)
  %318 = fmul fast float %317, 2.000000e+00
  %319 = fmul fast float %315, 2.000000e+00
  %320 = call float @dx.op.unary.f32(i32 21, float %319)  ; Exp(value)
  %321 = fmul fast float %320, 3.000000e+00
  %322 = fadd fast float %318, 1.000000e+00
  %323 = fsub fast float %322, %321
  br label %324

; <label>:324                                     ; preds = %314, %312
  %325 = phi float [ %323, %314 ], [ 0.000000e+00, %312 ]
  %326 = fcmp fast olt float %182, 0.000000e+00
  %327 = fcmp fast ogt float %182, 1.000000e+00
  %328 = and i1 %326, %327
  br i1 %328, label %339, label %329

; <label>:329                                     ; preds = %324
  %330 = call float @dx.op.unary.f32(i32 23, float %182)  ; Log(value)
  %331 = fmul fast float %330, 3.000000e+00
  %332 = call float @dx.op.unary.f32(i32 21, float %331)  ; Exp(value)
  %333 = fmul fast float %332, 2.000000e+00
  %334 = fmul fast float %330, 2.000000e+00
  %335 = call float @dx.op.unary.f32(i32 21, float %334)  ; Exp(value)
  %336 = fmul fast float %335, 3.000000e+00
  %337 = fadd fast float %333, 1.000000e+00
  %338 = fsub fast float %337, %336
  br label %339

; <label>:339                                     ; preds = %329, %324
  %340 = phi float [ %338, %329 ], [ 0.000000e+00, %324 ]
  %341 = fmul fast float %178, %174
  %342 = fmul fast float %341, %179
  %343 = fmul fast float %342, %325
  %344 = fmul fast float %181, %325
  %345 = fmul fast float %174, %159
  %346 = fmul fast float %345, %180
  %347 = fmul fast float %346, %179
  %348 = fmul fast float %347, %325
  %349 = fmul fast float %345, %178
  %350 = fmul fast float %349, %325
  %351 = fmul fast float %173, -2.625000e+01
  %352 = fmul fast float %351, %186
  %353 = fmul fast float %352, %179
  %354 = fmul fast float %353, %340
  %355 = fmul fast float %188, %340
  %356 = fmul fast float %158, 0xBFBA1AD000000000
  %357 = fmul fast float %356, %174
  %358 = fmul fast float %357, %187
  %359 = fmul fast float %358, %179
  %360 = fmul fast float %359, %340
  %361 = fmul fast float %357, %186
  %362 = fmul fast float %361, %340
  %363 = fsub fast float %354, %343
  %364 = fadd fast float %355, %344
  %365 = fsub fast float %360, %348
  %366 = fsub fast float %362, %350
  %367 = fmul fast float %363, %313
  %368 = fmul fast float %364, %313
  %369 = fmul fast float %365, %313
  %370 = fmul fast float %366, %313
  br label %371

; <label>:371                                     ; preds = %339, %280, %218
  %372 = phi float [ %246, %218 ], [ %308, %280 ], [ %367, %339 ]
  %373 = phi float [ %247, %218 ], [ %309, %280 ], [ %368, %339 ]
  %374 = phi float [ %248, %218 ], [ %310, %280 ], [ %369, %339 ]
  %375 = phi float [ %249, %218 ], [ %311, %280 ], [ %370, %339 ]
  %376 = fadd float %138, %375
  %377 = fadd float %137, %374
  %378 = fadd float %136, %373
  %379 = fadd float %135, %372
  %380 = add nuw i32 %139, 1
  %381 = icmp eq i32 %380, %131
  br i1 %381, label %382, label %134

; <label>:382                                     ; preds = %371
  br label %383

; <label>:383                                     ; preds = %382, %99
  %384 = phi float [ %127, %99 ], [ %379, %382 ]
  %385 = phi float [ %128, %99 ], [ %378, %382 ]
  %386 = phi float [ %129, %99 ], [ %377, %382 ]
  %387 = phi float [ %130, %99 ], [ %376, %382 ]
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %3, i32 0, i32 undef, float %384, float %385, float %386, float %387, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %388

; <label>:388                                     ; preds = %383, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

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
!dx.entryPoints = !{!10}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{null, !5, !8, null}
!5 = !{!6}
!6 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 32, null}
!10 = !{void ()* @main, !"main", null, !4, !11}
!11 = !{i32 4, !12}
!12 = !{i32 32, i32 1, i32 1}

