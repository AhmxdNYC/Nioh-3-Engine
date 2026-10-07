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
; shader debug name: db8d705eb316ec5dc261954d1269264c.pdb
; shader hash: db8d705eb316ec5dc261954d1269264c
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
;       float4 REFERENCE[4];                          ; Offset:    0
;       uint SIZE[2];                                 ; Offset:   64
;       row_major float4x4 INVPROJ;                   ; Offset:   96
;       row_major float4x4 INVVIEW;                   ; Offset:  160
;       float4 EYE;                                   ; Offset:  224
;       float CUTOFF;                                 ; Offset:  240
;       float4 POROUSALBEDO;                          ; Offset:  256
;       float4 POROUSREFLECTANCE;                     ; Offset:  272
;       float4 VIEWPORTRECT;                          ; Offset:  288
;       float NEARHEIGHT;                             ; Offset:  304
;       float WATERLEVEL;                             ; Offset:  308
;       float CHARACTERSCALE;                         ; Offset:  312
;       float2 FADETHRESHOLD;                         ; Offset:  320
;       float2 FADECOEFFICIENT;                       ; Offset:  328
;       bool FADEMODE[3];                             ; Offset:  336
;   
;   } $Globals;                                       ; Offset:    0 Size:   372
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpWAVEHEIGHT                   sampler      NA          NA      S0             s8     1
; __smpPROPERTYMAP                  sampler      NA          NA      S1             s9     1
; __smpPROPDIST                     sampler      NA          NA      S2            s14     1
; WAVEHEIGHT                        texture     f32          2d      T0             t8     1
; PROPERTYMAP                       texture     f32     2darray      T1             t9     1
; GALBEDO                           texture     f32          2d      T2            t10     1
; GREFLECTANCE                      texture     f32          2d      T3            t11     1
; GDEPTH                            texture     f32          2d      T4            t12     1
; GSTENCIL                          texture     u32          2d      T5            t13     1
; PROPDIST                          texture     f32     2darray      T6            t14     1
; CALBEDO                               UAV     f32          2d      U0             u0     1
; CREFLECTANCE                          UAV     f32          2d      U1             u1     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2D<vector<unsigned int, 2> >" = type { <2 x i32>, %"class.Texture2D<vector<unsigned int, 2> >::mips_type" }
%"class.Texture2D<vector<unsigned int, 2> >::mips_type" = type { i32 }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [2 x i32], [4 x <4 x float>], [4 x <4 x float>], <4 x float>, float, <4 x float>, <4 x float>, <4 x float>, float, float, float, <2 x float>, <2 x float>, [3 x i32] }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 14, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 14, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %15 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %16 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %17 = extractvalue %dx.types.CBufRet.i32 %16, 0
  %18 = icmp uge i32 %14, %17
  %19 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %20 = extractvalue %dx.types.CBufRet.i32 %19, 0
  %21 = icmp uge i32 %15, %20
  %22 = or i1 %18, %21
  br i1 %22, label %452, label %23

; <label>:23                                      ; preds = %0
  %24 = uitofp i32 %14 to float
  %25 = uitofp i32 %15 to float
  %26 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %27 = extractvalue %dx.types.CBufRet.i32 %26, 0
  %28 = uitofp i32 %27 to float
  %29 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %30 = extractvalue %dx.types.CBufRet.i32 %29, 0
  %31 = uitofp i32 %30 to float
  %32 = fdiv fast float %24, %28
  %33 = fdiv fast float %25, %31
  %34 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %35 = extractvalue %dx.types.CBufRet.f32 %34, 2
  %36 = extractvalue %dx.types.CBufRet.f32 %34, 3
  %37 = fdiv fast float %32, %35
  %38 = fdiv fast float %33, %36
  %39 = fadd fast float %37, -5.000000e-01
  %40 = fadd fast float %38, -5.000000e-01
  %41 = fmul fast float %39, 2.000000e+00
  %42 = fmul fast float %40, -2.000000e+00
  %43 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %5, i32 0, i32 %14, i32 %15, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %44 = extractvalue %dx.types.ResRet.f32 %43, 0
  %45 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %46 = extractvalue %dx.types.CBufRet.f32 %45, 0
  %47 = extractvalue %dx.types.CBufRet.f32 %45, 1
  %48 = extractvalue %dx.types.CBufRet.f32 %45, 2
  %49 = extractvalue %dx.types.CBufRet.f32 %45, 3
  %50 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %51 = extractvalue %dx.types.CBufRet.f32 %50, 0
  %52 = extractvalue %dx.types.CBufRet.f32 %50, 1
  %53 = extractvalue %dx.types.CBufRet.f32 %50, 2
  %54 = extractvalue %dx.types.CBufRet.f32 %50, 3
  %55 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %56 = extractvalue %dx.types.CBufRet.f32 %55, 0
  %57 = extractvalue %dx.types.CBufRet.f32 %55, 1
  %58 = extractvalue %dx.types.CBufRet.f32 %55, 2
  %59 = extractvalue %dx.types.CBufRet.f32 %55, 3
  %60 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %61 = extractvalue %dx.types.CBufRet.f32 %60, 0
  %62 = extractvalue %dx.types.CBufRet.f32 %60, 1
  %63 = extractvalue %dx.types.CBufRet.f32 %60, 2
  %64 = extractvalue %dx.types.CBufRet.f32 %60, 3
  %65 = fmul fast float %46, %41
  %66 = call float @dx.op.tertiary.f32(i32 46, float %42, float %51, float %65)  ; FMad(a,b,c)
  %67 = call float @dx.op.tertiary.f32(i32 46, float %44, float %56, float %66)  ; FMad(a,b,c)
  %68 = fadd fast float %67, %61
  %69 = fmul fast float %47, %41
  %70 = call float @dx.op.tertiary.f32(i32 46, float %42, float %52, float %69)  ; FMad(a,b,c)
  %71 = call float @dx.op.tertiary.f32(i32 46, float %44, float %57, float %70)  ; FMad(a,b,c)
  %72 = fadd fast float %71, %62
  %73 = fmul fast float %48, %41
  %74 = call float @dx.op.tertiary.f32(i32 46, float %42, float %53, float %73)  ; FMad(a,b,c)
  %75 = call float @dx.op.tertiary.f32(i32 46, float %44, float %58, float %74)  ; FMad(a,b,c)
  %76 = fadd fast float %75, %63
  %77 = fmul fast float %49, %41
  %78 = call float @dx.op.tertiary.f32(i32 46, float %42, float %54, float %77)  ; FMad(a,b,c)
  %79 = call float @dx.op.tertiary.f32(i32 46, float %44, float %59, float %78)  ; FMad(a,b,c)
  %80 = fadd fast float %79, %64
  %81 = fdiv fast float %68, %80
  %82 = fdiv fast float %72, %80
  %83 = fdiv fast float %76, %80
  %84 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %85 = extractvalue %dx.types.CBufRet.f32 %84, 0
  %86 = extractvalue %dx.types.CBufRet.f32 %84, 1
  %87 = extractvalue %dx.types.CBufRet.f32 %84, 2
  %88 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %89 = extractvalue %dx.types.CBufRet.f32 %88, 0
  %90 = extractvalue %dx.types.CBufRet.f32 %88, 1
  %91 = extractvalue %dx.types.CBufRet.f32 %88, 2
  %92 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %93 = extractvalue %dx.types.CBufRet.f32 %92, 0
  %94 = extractvalue %dx.types.CBufRet.f32 %92, 1
  %95 = extractvalue %dx.types.CBufRet.f32 %92, 2
  %96 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %97 = extractvalue %dx.types.CBufRet.f32 %96, 0
  %98 = extractvalue %dx.types.CBufRet.f32 %96, 1
  %99 = extractvalue %dx.types.CBufRet.f32 %96, 2
  %100 = fmul fast float %85, %81
  %101 = call float @dx.op.tertiary.f32(i32 46, float %82, float %89, float %100)  ; FMad(a,b,c)
  %102 = call float @dx.op.tertiary.f32(i32 46, float %83, float %93, float %101)  ; FMad(a,b,c)
  %103 = fadd fast float %102, %97
  %104 = fmul fast float %86, %81
  %105 = call float @dx.op.tertiary.f32(i32 46, float %82, float %90, float %104)  ; FMad(a,b,c)
  %106 = call float @dx.op.tertiary.f32(i32 46, float %83, float %94, float %105)  ; FMad(a,b,c)
  %107 = fadd fast float %106, %98
  %108 = fmul fast float %87, %81
  %109 = call float @dx.op.tertiary.f32(i32 46, float %82, float %91, float %108)  ; FMad(a,b,c)
  %110 = call float @dx.op.tertiary.f32(i32 46, float %83, float %95, float %109)  ; FMad(a,b,c)
  %111 = fadd fast float %110, %99
  %112 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %113 = extractvalue %dx.types.CBufRet.f32 %112, 1
  %114 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %115 = extractvalue %dx.types.CBufRet.f32 %114, 1
  %116 = extractvalue %dx.types.CBufRet.f32 %112, 0
  %117 = fsub fast float %115, %116
  %118 = fcmp fast olt float %117, %113
  %119 = fcmp fast olt float %107, %113
  %120 = and i1 %119, %118
  %121 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %122 = extractvalue %dx.types.CBufRet.f32 %121, 0
  %123 = extractvalue %dx.types.CBufRet.f32 %121, 1
  %124 = extractvalue %dx.types.CBufRet.f32 %121, 2
  %125 = extractvalue %dx.types.CBufRet.f32 %121, 3
  %126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %127 = extractvalue %dx.types.CBufRet.f32 %126, 0
  %128 = extractvalue %dx.types.CBufRet.f32 %126, 1
  %129 = extractvalue %dx.types.CBufRet.f32 %126, 2
  %130 = extractvalue %dx.types.CBufRet.f32 %126, 3
  %131 = fsub fast float %127, %122
  %132 = fsub fast float %128, %123
  %133 = fsub fast float %129, %124
  %134 = fsub fast float %130, %125
  %135 = fmul fast float %131, %131
  %136 = fmul fast float %132, %132
  %137 = fadd fast float %135, %136
  %138 = fmul fast float %133, %133
  %139 = fadd fast float %137, %138
  %140 = fmul fast float %134, %134
  %141 = fadd fast float %139, %140
  %142 = call float @dx.op.unary.f32(i32 24, float %141)  ; Sqrt(value)
  %143 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %144 = extractvalue %dx.types.CBufRet.f32 %143, 0
  %145 = extractvalue %dx.types.CBufRet.f32 %143, 1
  %146 = extractvalue %dx.types.CBufRet.f32 %143, 2
  %147 = fsub fast float %144, %127
  %148 = fsub fast float %145, %128
  %149 = fsub fast float %146, %129
  %150 = call float @dx.op.dot3.f32(i32 55, float %147, float %148, float %149, float %147, float %148, float %149)  ; Dot3(ax,ay,az,bx,by,bz)
  %151 = call float @dx.op.unary.f32(i32 25, float %150)  ; Rsqrt(value)
  %152 = fmul fast float %147, %151
  %153 = fmul fast float %148, %151
  %154 = fmul fast float %149, %151
  %155 = fsub fast float %144, %122
  %156 = fsub fast float %145, %123
  %157 = fsub fast float %146, %124
  %158 = fmul fast float %156, %154
  %159 = fmul fast float %157, %153
  %160 = fsub fast float %158, %159
  %161 = fmul fast float %157, %152
  %162 = fmul fast float %155, %154
  %163 = fsub fast float %161, %162
  %164 = fmul fast float %155, %153
  %165 = fmul fast float %156, %152
  %166 = fsub fast float %164, %165
  %167 = call float @dx.op.dot3.f32(i32 55, float %160, float %163, float %166, float %160, float %163, float %166)  ; Dot3(ax,ay,az,bx,by,bz)
  %168 = fsub fast float %103, %122
  %169 = fsub fast float %113, %123
  %170 = fsub fast float %111, %124
  %171 = fmul fast float %154, %169
  %172 = fmul fast float %153, %170
  %173 = fsub fast float %171, %172
  %174 = fmul fast float %152, %170
  %175 = fmul fast float %154, %168
  %176 = fsub fast float %174, %175
  %177 = fmul fast float %153, %168
  %178 = fmul fast float %152, %169
  %179 = fsub fast float %177, %178
  %180 = call float @dx.op.dot3.f32(i32 55, float %173, float %176, float %179, float %160, float %163, float %166)  ; Dot3(ax,ay,az,bx,by,bz)
  %181 = fdiv fast float %180, %167
  br i1 %120, label %182, label %189

; <label>:182                                     ; preds = %23
  %183 = fcmp fast olt float %181, 0.000000e+00
  %184 = fcmp fast ogt float %181, 1.000000e+00
  %185 = or i1 %183, %184
  br i1 %185, label %186, label %189

; <label>:186                                     ; preds = %182
  %187 = call float @dx.op.binary.f32(i32 35, float %181, float 0.000000e+00)  ; FMax(a,b)
  %188 = call float @dx.op.binary.f32(i32 36, float %187, float 1.000000e+00)  ; FMin(a,b)
  br label %189

; <label>:189                                     ; preds = %186, %182, %23
  %190 = phi float [ %188, %186 ], [ %181, %182 ], [ %181, %23 ]
  %191 = fcmp fast olt float %190, 0.000000e+00
  %192 = fcmp fast ogt float %190, 1.000000e+00
  %193 = or i1 %191, %192
  br i1 %193, label %243, label %194

; <label>:194                                     ; preds = %189
  %195 = fmul fast float %125, %142
  %196 = fadd fast float %195, 0xBFEFFFE700000000
  %197 = fmul fast float %196, %190
  %198 = fadd fast float %197, 1.000000e+00
  %199 = call float @dx.op.unary.f32(i32 23, float %198)  ; Log(value)
  %200 = fmul fast float %199, 0x3FE62E4300000000
  %201 = fadd fast float %195, 0x3EE9000020000000
  %202 = call float @dx.op.unary.f32(i32 23, float %201)  ; Log(value)
  %203 = fmul fast float %202, 0x3FE62E4300000000
  %204 = fdiv fast float %200, %203
  %205 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %206 = extractvalue %dx.types.CBufRet.f32 %205, 0
  %207 = extractvalue %dx.types.CBufRet.f32 %205, 1
  %208 = extractvalue %dx.types.CBufRet.f32 %205, 2
  %209 = fmul fast float %155, %190
  %210 = fmul fast float %156, %190
  %211 = fmul fast float %157, %190
  %212 = fadd fast float %209, %122
  %213 = fadd fast float %210, %123
  %214 = fadd fast float %211, %124
  %215 = fsub fast float %127, %206
  %216 = fsub fast float %128, %207
  %217 = fsub fast float %129, %208
  %218 = fmul fast float %215, %190
  %219 = fmul fast float %216, %190
  %220 = fmul fast float %217, %190
  %221 = fadd fast float %218, %206
  %222 = fadd fast float %219, %207
  %223 = fadd fast float %220, %208
  %224 = fsub fast float %221, %212
  %225 = fsub fast float %222, %213
  %226 = fsub fast float %223, %214
  %227 = call float @dx.op.dot3.f32(i32 55, float %224, float %225, float %226, float %224, float %225, float %226)  ; Dot3(ax,ay,az,bx,by,bz)
  %228 = call float @dx.op.unary.f32(i32 25, float %227)  ; Rsqrt(value)
  %229 = fmul fast float %224, %228
  %230 = fmul fast float %225, %228
  %231 = fmul fast float %226, %228
  %232 = fsub fast float %103, %212
  %233 = fsub fast float %113, %213
  %234 = fsub fast float %111, %214
  %235 = call float @dx.op.dot3.f32(i32 55, float %232, float %233, float %234, float %229, float %230, float %231)  ; Dot3(ax,ay,az,bx,by,bz)
  %236 = fmul fast float %224, %224
  %237 = fmul fast float %225, %225
  %238 = fadd fast float %236, %237
  %239 = fmul fast float %226, %226
  %240 = fadd fast float %238, %239
  %241 = call float @dx.op.unary.f32(i32 24, float %240)  ; Sqrt(value)
  %242 = fdiv fast float %235, %241
  br label %243

; <label>:243                                     ; preds = %194, %189
  %244 = phi float [ %242, %194 ], [ 0.000000e+00, %189 ]
  %245 = phi float [ %204, %194 ], [ 0.000000e+00, %189 ]
  %246 = phi i1 [ true, %194 ], [ false, %189 ]
  %247 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %8, %dx.types.Handle %11, float %244, float %245, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %248 = extractvalue %dx.types.ResRet.f32 %247, 1
  %249 = extractvalue %dx.types.ResRet.f32 %247, 2
  %250 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %251 = extractvalue %dx.types.CBufRet.f32 %250, 0
  %252 = extractvalue %dx.types.CBufRet.f32 %114, 0
  %253 = extractvalue %dx.types.CBufRet.f32 %114, 2
  %254 = fsub fast float %103, %252
  %255 = fsub fast float %111, %253
  %256 = fmul fast float %254, 0x3F847AE140000000
  %257 = fmul fast float %255, 0x3F847AE140000000
  %258 = fmul fast float %256, %256
  %259 = fmul fast float %257, %257
  %260 = fadd fast float %258, %259
  %261 = call float @dx.op.unary.f32(i32 24, float %260)  ; Sqrt(value)
  %262 = fdiv fast float %251, %261
  %263 = call float @dx.op.binary.f32(i32 35, float %262, float 0.000000e+00)  ; FMax(a,b)
  %264 = call float @dx.op.binary.f32(i32 36, float %263, float 1.000000e+00)  ; FMin(a,b)
  %265 = fmul fast float %264, %249
  %266 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %3, %dx.types.Handle %10, float %244, float %245, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %267 = extractvalue %dx.types.ResRet.f32 %266, 0
  %268 = fsub fast float %113, %267
  %269 = fmul fast float %268, 0x3F847AE140000000
  %270 = fcmp fast ogt float %269, 0.000000e+00
  br i1 %270, label %271, label %276

; <label>:271                                     ; preds = %243
  %272 = call float @dx.op.unary.f32(i32 24, float %269)  ; Sqrt(value)
  %273 = call float @dx.op.binary.f32(i32 36, float %272, float 1.000000e+00)  ; FMin(a,b)
  %274 = call float @dx.op.binary.f32(i32 35, float %273, float 0.000000e+00)  ; FMax(a,b)
  %275 = call float @dx.op.binary.f32(i32 36, float %274, float 1.000000e+00)  ; FMin(a,b)
  br label %276

; <label>:276                                     ; preds = %271, %243
  %277 = phi float [ %275, %271 ], [ 0.000000e+00, %243 ]
  %278 = fmul fast float %265, %248
  %279 = fsub fast float 1.000000e+00, %265
  %280 = fmul fast float %277, %279
  %281 = fadd fast float %280, %278
  %282 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %283 = extractvalue %dx.types.CBufRet.i32 %282, 0
  %284 = uitofp i32 %283 to float
  %285 = fmul fast float %284, %32
  %286 = fptoui float %285 to i32
  %287 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %288 = extractvalue %dx.types.CBufRet.i32 %287, 0
  %289 = uitofp i32 %288 to float
  %290 = fmul fast float %289, %33
  %291 = fptoui float %290 to i32
  %292 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %4, i32 0, i32 %286, i32 %291, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %293 = extractvalue %dx.types.ResRet.i32 %292, 1
  %294 = and i32 %293, 1
  %295 = icmp eq i32 %294, 0
  %296 = extractvalue %dx.types.CBufRet.f32 %112, 2
  %297 = fmul fast float %296, %281
  %298 = select i1 %295, float %281, float %297
  %299 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %9, %dx.types.Handle %12, float %244, float %245, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %300 = extractvalue %dx.types.ResRet.f32 %299, 0
  %301 = extractvalue %dx.types.ResRet.f32 %299, 1
  %302 = fmul fast float %301, 1.000000e+02
  %303 = fcmp fast ogt float %302, 0.000000e+00
  br i1 %303, label %304, label %311

; <label>:304                                     ; preds = %276
  %305 = fsub fast float %107, %113
  %306 = fdiv fast float %305, %302
  %307 = fsub fast float 1.000000e+00, %306
  %308 = call float @dx.op.binary.f32(i32 35, float %307, float 0.000000e+00)  ; FMax(a,b)
  %309 = call float @dx.op.binary.f32(i32 36, float %308, float 1.000000e+00)  ; FMin(a,b)
  %310 = fmul fast float %309, %298
  br label %314

; <label>:311                                     ; preds = %276
  %312 = fcmp fast ogt float %107, %113
  br i1 %312, label %313, label %314

; <label>:313                                     ; preds = %311
  br label %314

; <label>:314                                     ; preds = %313, %311, %304
  %315 = phi float [ %310, %304 ], [ 0.000000e+00, %313 ], [ %298, %311 ]
  %316 = fmul fast float %300, %265
  %317 = fmul fast float %279, 0x3F847AE140000000
  %318 = fmul fast float %317, %268
  %319 = fadd fast float %316, %318
  %320 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %3, %dx.types.Handle %10, float %244, float %245, float 2.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %321 = extractvalue %dx.types.ResRet.f32 %320, 0
  %322 = fcmp fast ole float %321, 0.000000e+00
  %323 = call float @dx.op.unary.f32(i32 6, float %321)  ; FAbs(value)
  %324 = fcmp fast oge float %323, %319
  %325 = and i1 %322, %324
  %326 = select i1 %325, float 0.000000e+00, float %315
  %327 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %328 = extractvalue %dx.types.CBufRet.f32 %327, 0
  %329 = fsub fast float %328, %319
  %330 = extractvalue %dx.types.CBufRet.f32 %327, 2
  %331 = fmul fast float %330, 0xBFF7154760000000
  %332 = fmul fast float %331, %329
  %333 = call float @dx.op.unary.f32(i32 21, float %332)  ; Exp(value)
  %334 = call float @dx.op.binary.f32(i32 35, float %333, float 0.000000e+00)  ; FMax(a,b)
  %335 = call float @dx.op.binary.f32(i32 36, float %334, float 1.000000e+00)  ; FMin(a,b)
  %336 = fmul fast float %335, %326
  %337 = extractvalue %dx.types.CBufRet.f32 %327, 1
  %338 = fsub fast float %319, %337
  %339 = extractvalue %dx.types.CBufRet.f32 %327, 3
  %340 = fmul fast float %339, 0xBFF7154760000000
  %341 = fmul fast float %340, %338
  %342 = call float @dx.op.unary.f32(i32 21, float %341)  ; Exp(value)
  %343 = call float @dx.op.binary.f32(i32 35, float %342, float 0.000000e+00)  ; FMax(a,b)
  %344 = call float @dx.op.binary.f32(i32 36, float %343, float 1.000000e+00)  ; FMin(a,b)
  %345 = fmul fast float %344, %336
  %346 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %7, i32 0, i32 %14, i32 %15, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %347 = extractvalue %dx.types.ResRet.f32 %346, 0
  %348 = extractvalue %dx.types.ResRet.f32 %346, 1
  %349 = extractvalue %dx.types.ResRet.f32 %346, 2
  %350 = extractvalue %dx.types.ResRet.f32 %346, 3
  %351 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %352 = extractvalue %dx.types.CBufRet.i32 %351, 0
  %353 = icmp ne i32 %352, 0
  %354 = select i1 %353, float %345, float %336
  br i1 %246, label %355, label %372

; <label>:355                                     ; preds = %314
  %356 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %357 = extractvalue %dx.types.CBufRet.f32 %356, 0
  %358 = call float @dx.op.unary.f32(i32 23, float %357)  ; Log(value)
  %359 = fmul fast float %358, %354
  %360 = call float @dx.op.unary.f32(i32 21, float %359)  ; Exp(value)
  %361 = fmul fast float %360, %347
  %362 = extractvalue %dx.types.CBufRet.f32 %356, 1
  %363 = call float @dx.op.unary.f32(i32 23, float %362)  ; Log(value)
  %364 = fmul fast float %363, %354
  %365 = call float @dx.op.unary.f32(i32 21, float %364)  ; Exp(value)
  %366 = fmul fast float %365, %348
  %367 = extractvalue %dx.types.CBufRet.f32 %356, 2
  %368 = call float @dx.op.unary.f32(i32 23, float %367)  ; Log(value)
  %369 = fmul fast float %368, %354
  %370 = call float @dx.op.unary.f32(i32 21, float %369)  ; Exp(value)
  %371 = fmul fast float %370, %349
  br label %372

; <label>:372                                     ; preds = %355, %314
  %373 = phi float [ %361, %355 ], [ %347, %314 ]
  %374 = phi float [ %366, %355 ], [ %348, %314 ]
  %375 = phi float [ %371, %355 ], [ %349, %314 ]
  %376 = fcmp fast ugt float %373, 0x3F69A62EE0000000
  br i1 %376, label %379, label %377

; <label>:377                                     ; preds = %372
  %378 = fmul fast float %373, 0x4029D70A40000000
  br label %385

; <label>:379                                     ; preds = %372
  %380 = call float @dx.op.unary.f32(i32 23, float %373)  ; Log(value)
  %381 = fmul fast float %380, 0x3FDAAAAAA0000000
  %382 = call float @dx.op.unary.f32(i32 21, float %381)  ; Exp(value)
  %383 = fmul fast float %382, 0x3FF0E147A0000000
  %384 = fadd fast float %383, 0xBFAC28F5C0000000
  br label %385

; <label>:385                                     ; preds = %379, %377
  %386 = phi float [ %378, %377 ], [ %384, %379 ]
  %387 = fcmp fast ugt float %374, 0x3F69A62EE0000000
  br i1 %387, label %390, label %388

; <label>:388                                     ; preds = %385
  %389 = fmul fast float %374, 0x4029D70A40000000
  br label %396

; <label>:390                                     ; preds = %385
  %391 = call float @dx.op.unary.f32(i32 23, float %374)  ; Log(value)
  %392 = fmul fast float %391, 0x3FDAAAAAA0000000
  %393 = call float @dx.op.unary.f32(i32 21, float %392)  ; Exp(value)
  %394 = fmul fast float %393, 0x3FF0E147A0000000
  %395 = fadd fast float %394, 0xBFAC28F5C0000000
  br label %396

; <label>:396                                     ; preds = %390, %388
  %397 = phi float [ %389, %388 ], [ %395, %390 ]
  %398 = fcmp fast ugt float %375, 0x3F69A62EE0000000
  br i1 %398, label %401, label %399

; <label>:399                                     ; preds = %396
  %400 = fmul fast float %375, 0x4029D70A40000000
  br label %407

; <label>:401                                     ; preds = %396
  %402 = call float @dx.op.unary.f32(i32 23, float %375)  ; Log(value)
  %403 = fmul fast float %402, 0x3FDAAAAAA0000000
  %404 = call float @dx.op.unary.f32(i32 21, float %403)  ; Exp(value)
  %405 = fmul fast float %404, 0x3FF0E147A0000000
  %406 = fadd fast float %405, 0xBFAC28F5C0000000
  br label %407

; <label>:407                                     ; preds = %401, %399
  %408 = phi float [ %400, %399 ], [ %406, %401 ]
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2, i32 %14, i32 %15, i32 undef, float %386, float %397, float %408, float %350, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  %409 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %6, i32 0, i32 %14, i32 %15, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %410 = extractvalue %dx.types.ResRet.f32 %409, 0
  %411 = extractvalue %dx.types.ResRet.f32 %409, 1
  %412 = extractvalue %dx.types.ResRet.f32 %409, 2
  %413 = extractvalue %dx.types.ResRet.f32 %409, 3
  %414 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %415 = extractvalue %dx.types.CBufRet.i32 %414, 0
  %416 = icmp ne i32 %415, 0
  %417 = select i1 %416, float %345, float %336
  %418 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %13, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %419 = extractvalue %dx.types.CBufRet.i32 %418, 0
  %420 = icmp ne i32 %419, 0
  %421 = select i1 %420, float %345, float %336
  br i1 %246, label %422, label %447

; <label>:422                                     ; preds = %407
  %423 = fsub fast float 1.000000e+00, %417
  %424 = fmul fast float %423, %410
  %425 = fmul fast float %423, %411
  %426 = fmul fast float %423, %412
  %427 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %428 = extractvalue %dx.types.CBufRet.f32 %427, 0
  %429 = extractvalue %dx.types.CBufRet.f32 %427, 1
  %430 = extractvalue %dx.types.CBufRet.f32 %427, 2
  %431 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %13, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %432 = extractvalue %dx.types.CBufRet.f32 %431, 3
  %433 = fmul fast float %428, %417
  %434 = fmul fast float %433, %432
  %435 = fmul fast float %429, %417
  %436 = fmul fast float %435, %432
  %437 = fmul fast float %430, %417
  %438 = fmul fast float %437, %432
  %439 = fadd fast float %434, %424
  %440 = fadd fast float %436, %425
  %441 = fadd fast float %438, %426
  %442 = fsub fast float 1.000000e+00, %421
  %443 = fmul fast float %442, %413
  %444 = extractvalue %dx.types.CBufRet.f32 %427, 3
  %445 = fmul fast float %444, %421
  %446 = fadd fast float %445, %443
  br label %447

; <label>:447                                     ; preds = %422, %407
  %448 = phi float [ %439, %422 ], [ %410, %407 ]
  %449 = phi float [ %440, %422 ], [ %411, %407 ]
  %450 = phi float [ %441, %422 ], [ %412, %407 ]
  %451 = phi float [ %446, %422 ], [ %413, %407 ]
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %14, i32 %15, i32 undef, float %448, float %449, float %450, float %451, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %452

; <label>:452                                     ; preds = %447, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

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
!dx.entryPoints = !{!24}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !15, !18, !20}
!5 = !{!6, !8, !9, !10, !11, !12, !14}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 9, i32 1, i32 7, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 10, i32 1, i32 2, i32 0, !7}
!10 = !{i32 3, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 11, i32 1, i32 2, i32 0, !7}
!11 = !{i32 4, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 2, i32 0, !7}
!12 = !{i32 5, %"class.Texture2D<vector<unsigned int, 2> >"* undef, !"", i32 0, i32 13, i32 1, i32 2, i32 0, !13}
!13 = !{i32 0, i32 5}
!14 = !{i32 6, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 14, i32 1, i32 7, i32 0, !7}
!15 = !{!16, !17}
!16 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!17 = !{i32 1, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!18 = !{!19}
!19 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 372, null}
!20 = !{!21, !22, !23}
!21 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!22 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 9, i32 1, i32 0, null}
!23 = !{i32 2, %struct.SamplerState* undef, !"", i32 0, i32 14, i32 1, i32 0, null}
!24 = !{void ()* @main, !"main", null, !4, !25}
!25 = !{i32 4, !26}
!26 = !{i32 32, i32 32, i32 1}

