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
; shader debug name: ba535c0aa830deb1c85f5f8870b8a272.pdb
; shader hash: ba535c0aa830deb1c85f5f8870b8a272
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
;       int SIZE[2];                                  ; Offset:   64
;       row_major float4x4 INVPROJ;                   ; Offset:   96
;       row_major float4x4 INVVIEW;                   ; Offset:  160
;       float4 EYE;                                   ; Offset:  224
;       float CUTOFF;                                 ; Offset:  240
;       float4 LIGHTCOLOR;                            ; Offset:  256
;       float CAUSTICSSIZE;                           ; Offset:  272
;       float4 CAUSTICSDIR;                           ; Offset:  288
;       float2 CAUSTICSOFFSET;                        ; Offset:  304
;       float FRAME;                                  ; Offset:  312
;       int TOTALFRAMES;                              ; Offset:  316
;       int SCATTERLEVELS;                            ; Offset:  320
;       float SCATTERRATE;                            ; Offset:  324
;       float SCALE;                                  ; Offset:  328
;       float4 VIEWPORTRECT;                          ; Offset:  336
;       float NEARHEIGHT;                             ; Offset:  352
;       float WATERLEVEL;                             ; Offset:  356
;       float CHARACTERSCALE;                         ; Offset:  360
;       float3 LIGHTDIR;                              ; Offset:  368
;       float3 WAVECAUSTICSSCALE;                     ; Offset:  384
;       float DIRECTOCCLUSIONSCALE;                   ; Offset:  396
;   
;   } $Globals;                                       ; Offset:    0 Size:   400
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpWAVEMAP                      sampler      NA          NA      S0             s8     1
; __smpWAVEHEIGHT                   sampler      NA          NA      S1             s0     1
; __smpPROPERTYMAP                  sampler      NA          NA      S2             s9     1
; __smpCAUSTICSMAP                  sampler      NA          NA      S3            s10     1
; __smpPROPDIST                     sampler      NA          NA      S4             s1     1
; __smpDIRECT_OCC                   sampler      NA          NA      S5             s2     1
; WAVEMAP                           texture     f32          2d      T0             t8     1
; WAVEHEIGHT                        texture     f32          2d      T1             t0     1
; PROPERTYMAP                       texture     f32     2darray      T2             t9     1
; CAUSTICSMAP                       texture     f32     2darray      T3            t10     1
; GALBEDO                           texture     f32          2d      T4            t11     1
; GREFLECTANCE                      texture     f32          2d      T5            t12     1
; GDEPTH                            texture     f32          2d      T6            t13     1
; GSTENCIL                          texture     u32          2d      T7            t14     1
; GEMISSIVE                         texture     f32          2d      T8            t15     1
; PROPDIST                          texture     f32     2darray      T9             t1     1
; DIRECT_OCC                        texture     f32     2darray     T10             t2     1
; CREFLECTANCE                          UAV     f32          2d      U0             u1     1
; CEMISSIVE                             UAV     f32          2d      U1             u2     1
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
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [2 x i32], [4 x <4 x float>], [4 x <4 x float>], <4 x float>, float, <4 x float>, float, <4 x float>, <2 x float>, float, i32, i32, float, float, <4 x float>, float, float, float, <3 x float>, <3 x float>, float }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 10, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 9, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 8, i32 15, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 14, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 5, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 4, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 3, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %17 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 2, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %18 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %19 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %20 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %21 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %22 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %23 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %24 = extractvalue %dx.types.CBufRet.i32 %23, 0
  %25 = icmp uge i32 %21, %24
  %26 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %27 = extractvalue %dx.types.CBufRet.i32 %26, 0
  %28 = icmp uge i32 %22, %27
  %29 = or i1 %25, %28
  br i1 %29, label %644, label %30

; <label>:30                                      ; preds = %0
  %31 = uitofp i32 %21 to float
  %32 = uitofp i32 %22 to float
  %33 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %34 = extractvalue %dx.types.CBufRet.i32 %33, 0
  %35 = sitofp i32 %34 to float
  %36 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %37 = extractvalue %dx.types.CBufRet.i32 %36, 0
  %38 = sitofp i32 %37 to float
  %39 = fdiv fast float %31, %35
  %40 = fdiv fast float %32, %38
  %41 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %42 = extractvalue %dx.types.CBufRet.f32 %41, 1
  %43 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %44 = extractvalue %dx.types.CBufRet.f32 %43, 2
  %45 = extractvalue %dx.types.CBufRet.f32 %43, 3
  %46 = fdiv fast float %39, %44
  %47 = fdiv fast float %40, %45
  %48 = fadd fast float %46, -5.000000e-01
  %49 = fadd fast float %47, -5.000000e-01
  %50 = fmul fast float %48, 2.000000e+00
  %51 = fmul fast float %49, -2.000000e+00
  %52 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %7, i32 0, i32 %21, i32 %22, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %53 = extractvalue %dx.types.ResRet.f32 %52, 0
  %54 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %55 = extractvalue %dx.types.CBufRet.f32 %54, 0
  %56 = extractvalue %dx.types.CBufRet.f32 %54, 1
  %57 = extractvalue %dx.types.CBufRet.f32 %54, 2
  %58 = extractvalue %dx.types.CBufRet.f32 %54, 3
  %59 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %60 = extractvalue %dx.types.CBufRet.f32 %59, 0
  %61 = extractvalue %dx.types.CBufRet.f32 %59, 1
  %62 = extractvalue %dx.types.CBufRet.f32 %59, 2
  %63 = extractvalue %dx.types.CBufRet.f32 %59, 3
  %64 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %65 = extractvalue %dx.types.CBufRet.f32 %64, 0
  %66 = extractvalue %dx.types.CBufRet.f32 %64, 1
  %67 = extractvalue %dx.types.CBufRet.f32 %64, 2
  %68 = extractvalue %dx.types.CBufRet.f32 %64, 3
  %69 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %70 = extractvalue %dx.types.CBufRet.f32 %69, 0
  %71 = extractvalue %dx.types.CBufRet.f32 %69, 1
  %72 = extractvalue %dx.types.CBufRet.f32 %69, 2
  %73 = extractvalue %dx.types.CBufRet.f32 %69, 3
  %74 = fmul fast float %55, %50
  %75 = call float @dx.op.tertiary.f32(i32 46, float %51, float %60, float %74)  ; FMad(a,b,c)
  %76 = call float @dx.op.tertiary.f32(i32 46, float %53, float %65, float %75)  ; FMad(a,b,c)
  %77 = fadd fast float %76, %70
  %78 = fmul fast float %56, %50
  %79 = call float @dx.op.tertiary.f32(i32 46, float %51, float %61, float %78)  ; FMad(a,b,c)
  %80 = call float @dx.op.tertiary.f32(i32 46, float %53, float %66, float %79)  ; FMad(a,b,c)
  %81 = fadd fast float %80, %71
  %82 = fmul fast float %57, %50
  %83 = call float @dx.op.tertiary.f32(i32 46, float %51, float %62, float %82)  ; FMad(a,b,c)
  %84 = call float @dx.op.tertiary.f32(i32 46, float %53, float %67, float %83)  ; FMad(a,b,c)
  %85 = fadd fast float %84, %72
  %86 = fmul fast float %58, %50
  %87 = call float @dx.op.tertiary.f32(i32 46, float %51, float %63, float %86)  ; FMad(a,b,c)
  %88 = call float @dx.op.tertiary.f32(i32 46, float %53, float %68, float %87)  ; FMad(a,b,c)
  %89 = fadd fast float %88, %73
  %90 = fdiv fast float %77, %89
  %91 = fdiv fast float %81, %89
  %92 = fdiv fast float %85, %89
  %93 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %94 = extractvalue %dx.types.CBufRet.f32 %93, 0
  %95 = extractvalue %dx.types.CBufRet.f32 %93, 1
  %96 = extractvalue %dx.types.CBufRet.f32 %93, 2
  %97 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %98 = extractvalue %dx.types.CBufRet.f32 %97, 0
  %99 = extractvalue %dx.types.CBufRet.f32 %97, 1
  %100 = extractvalue %dx.types.CBufRet.f32 %97, 2
  %101 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %102 = extractvalue %dx.types.CBufRet.f32 %101, 0
  %103 = extractvalue %dx.types.CBufRet.f32 %101, 1
  %104 = extractvalue %dx.types.CBufRet.f32 %101, 2
  %105 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %106 = extractvalue %dx.types.CBufRet.f32 %105, 0
  %107 = extractvalue %dx.types.CBufRet.f32 %105, 1
  %108 = extractvalue %dx.types.CBufRet.f32 %105, 2
  %109 = fmul fast float %94, %90
  %110 = call float @dx.op.tertiary.f32(i32 46, float %91, float %98, float %109)  ; FMad(a,b,c)
  %111 = call float @dx.op.tertiary.f32(i32 46, float %92, float %102, float %110)  ; FMad(a,b,c)
  %112 = fadd fast float %111, %106
  %113 = fmul fast float %95, %90
  %114 = call float @dx.op.tertiary.f32(i32 46, float %91, float %99, float %113)  ; FMad(a,b,c)
  %115 = call float @dx.op.tertiary.f32(i32 46, float %92, float %103, float %114)  ; FMad(a,b,c)
  %116 = fadd fast float %115, %107
  %117 = fmul fast float %96, %90
  %118 = call float @dx.op.tertiary.f32(i32 46, float %91, float %100, float %117)  ; FMad(a,b,c)
  %119 = call float @dx.op.tertiary.f32(i32 46, float %92, float %104, float %118)  ; FMad(a,b,c)
  %120 = fadd fast float %119, %108
  %121 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %122 = extractvalue %dx.types.CBufRet.f32 %121, 1
  %123 = extractvalue %dx.types.CBufRet.f32 %41, 0
  %124 = fsub fast float %122, %123
  %125 = fcmp fast olt float %124, %42
  %126 = fcmp fast olt float %116, %42
  %127 = or i1 %126, %125
  %128 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %129 = extractvalue %dx.types.CBufRet.f32 %128, 0
  %130 = extractvalue %dx.types.CBufRet.f32 %128, 1
  %131 = extractvalue %dx.types.CBufRet.f32 %128, 2
  %132 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %133 = extractvalue %dx.types.CBufRet.f32 %132, 0
  %134 = extractvalue %dx.types.CBufRet.f32 %132, 1
  %135 = extractvalue %dx.types.CBufRet.f32 %132, 2
  %136 = fsub fast float %133, %129
  %137 = fsub fast float %134, %130
  %138 = fsub fast float %135, %131
  %139 = fmul fast float %136, %136
  %140 = fmul fast float %137, %137
  %141 = fadd fast float %139, %140
  %142 = fmul fast float %138, %138
  %143 = fadd fast float %141, %142
  %144 = call float @dx.op.unary.f32(i32 24, float %143)  ; Sqrt(value)
  %145 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %146 = extractvalue %dx.types.CBufRet.f32 %145, 0
  %147 = extractvalue %dx.types.CBufRet.f32 %145, 1
  %148 = extractvalue %dx.types.CBufRet.f32 %145, 2
  %149 = fsub fast float %146, %133
  %150 = fsub fast float %147, %134
  %151 = fsub fast float %148, %135
  %152 = call float @dx.op.dot3.f32(i32 55, float %149, float %150, float %151, float %149, float %150, float %151)  ; Dot3(ax,ay,az,bx,by,bz)
  %153 = call float @dx.op.unary.f32(i32 25, float %152)  ; Rsqrt(value)
  %154 = fmul fast float %149, %153
  %155 = fmul fast float %150, %153
  %156 = fmul fast float %151, %153
  %157 = fsub fast float %146, %129
  %158 = fsub fast float %147, %130
  %159 = fsub fast float %148, %131
  %160 = fmul fast float %158, %156
  %161 = fmul fast float %159, %155
  %162 = fsub fast float %160, %161
  %163 = fmul fast float %159, %154
  %164 = fmul fast float %157, %156
  %165 = fsub fast float %163, %164
  %166 = fmul fast float %157, %155
  %167 = fmul fast float %158, %154
  %168 = fsub fast float %166, %167
  %169 = call float @dx.op.dot3.f32(i32 55, float %162, float %165, float %168, float %162, float %165, float %168)  ; Dot3(ax,ay,az,bx,by,bz)
  %170 = fsub fast float %112, %129
  %171 = fsub fast float %42, %130
  %172 = fsub fast float %120, %131
  %173 = fmul fast float %156, %171
  %174 = fmul fast float %155, %172
  %175 = fsub fast float %173, %174
  %176 = fmul fast float %154, %172
  %177 = fmul fast float %156, %170
  %178 = fsub fast float %176, %177
  %179 = fmul fast float %155, %170
  %180 = fmul fast float %154, %171
  %181 = fsub fast float %179, %180
  %182 = call float @dx.op.dot3.f32(i32 55, float %175, float %178, float %181, float %162, float %165, float %168)  ; Dot3(ax,ay,az,bx,by,bz)
  %183 = fdiv fast float %182, %169
  br i1 %127, label %184, label %191

; <label>:184                                     ; preds = %30
  %185 = fcmp fast olt float %183, 0.000000e+00
  %186 = fcmp fast ogt float %183, 1.000000e+00
  %187 = or i1 %185, %186
  br i1 %187, label %188, label %191

; <label>:188                                     ; preds = %184
  %189 = call float @dx.op.binary.f32(i32 35, float %183, float 0.000000e+00)  ; FMax(a,b)
  %190 = call float @dx.op.binary.f32(i32 36, float %189, float 1.000000e+00)  ; FMin(a,b)
  br label %191

; <label>:191                                     ; preds = %188, %184, %30
  %192 = phi float [ %190, %188 ], [ %183, %184 ], [ %183, %30 ]
  %193 = fcmp fast olt float %192, 0.000000e+00
  %194 = fcmp fast ogt float %192, 1.000000e+00
  %195 = or i1 %193, %194
  br i1 %195, label %246, label %196

; <label>:196                                     ; preds = %191
  %197 = extractvalue %dx.types.CBufRet.f32 %128, 3
  %198 = fmul fast float %197, %144
  %199 = fadd fast float %198, 0xBFEFFFE700000000
  %200 = fmul fast float %199, %192
  %201 = fadd fast float %200, 1.000000e+00
  %202 = call float @dx.op.unary.f32(i32 23, float %201)  ; Log(value)
  %203 = fmul fast float %202, 0x3FE62E4300000000
  %204 = fadd fast float %198, 0x3EE9000020000000
  %205 = call float @dx.op.unary.f32(i32 23, float %204)  ; Log(value)
  %206 = fmul fast float %205, 0x3FE62E4300000000
  %207 = fdiv fast float %203, %206
  %208 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %209 = extractvalue %dx.types.CBufRet.f32 %208, 0
  %210 = extractvalue %dx.types.CBufRet.f32 %208, 1
  %211 = extractvalue %dx.types.CBufRet.f32 %208, 2
  %212 = fmul fast float %157, %192
  %213 = fmul fast float %158, %192
  %214 = fmul fast float %159, %192
  %215 = fadd fast float %212, %129
  %216 = fadd fast float %213, %130
  %217 = fadd fast float %214, %131
  %218 = fsub fast float %133, %209
  %219 = fsub fast float %134, %210
  %220 = fsub fast float %135, %211
  %221 = fmul fast float %218, %192
  %222 = fmul fast float %219, %192
  %223 = fmul fast float %220, %192
  %224 = fadd fast float %221, %209
  %225 = fadd fast float %222, %210
  %226 = fadd fast float %223, %211
  %227 = fsub fast float %224, %215
  %228 = fsub fast float %225, %216
  %229 = fsub fast float %226, %217
  %230 = call float @dx.op.dot3.f32(i32 55, float %227, float %228, float %229, float %227, float %228, float %229)  ; Dot3(ax,ay,az,bx,by,bz)
  %231 = call float @dx.op.unary.f32(i32 25, float %230)  ; Rsqrt(value)
  %232 = fmul fast float %227, %231
  %233 = fmul fast float %228, %231
  %234 = fmul fast float %229, %231
  %235 = fsub fast float %112, %215
  %236 = fsub fast float %42, %216
  %237 = fsub fast float %120, %217
  %238 = call float @dx.op.dot3.f32(i32 55, float %235, float %236, float %237, float %232, float %233, float %234)  ; Dot3(ax,ay,az,bx,by,bz)
  %239 = fmul fast float %227, %227
  %240 = fmul fast float %228, %228
  %241 = fadd fast float %239, %240
  %242 = fmul fast float %229, %229
  %243 = fadd fast float %241, %242
  %244 = call float @dx.op.unary.f32(i32 24, float %243)  ; Sqrt(value)
  %245 = fdiv fast float %238, %244
  br label %246

; <label>:246                                     ; preds = %196, %191
  %247 = phi float [ %245, %196 ], [ 0.000000e+00, %191 ]
  %248 = phi float [ %207, %196 ], [ 0.000000e+00, %191 ]
  %249 = phi i32 [ 1, %196 ], [ 0, %191 ]
  %250 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %12, %dx.types.Handle %18, float %247, float %248, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %251 = extractvalue %dx.types.ResRet.f32 %250, 0
  %252 = extractvalue %dx.types.ResRet.f32 %250, 1
  %253 = fmul fast float %252, 1.000000e+02
  %254 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %17, float %247, float %248, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %255 = extractvalue %dx.types.ResRet.f32 %254, 2
  %256 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %4, %dx.types.Handle %15, float %247, float %248, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %257 = extractvalue %dx.types.ResRet.f32 %256, 0
  %258 = fsub fast float %42, %257
  %259 = fmul fast float %258, 0x3F847AE140000000
  %260 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %261 = extractvalue %dx.types.CBufRet.f32 %260, 0
  %262 = extractvalue %dx.types.CBufRet.f32 %121, 0
  %263 = extractvalue %dx.types.CBufRet.f32 %121, 2
  %264 = fsub fast float %112, %262
  %265 = fsub fast float %120, %263
  %266 = fmul fast float %264, 0x3F847AE140000000
  %267 = fmul fast float %265, 0x3F847AE140000000
  %268 = fmul fast float %266, %266
  %269 = fmul fast float %267, %267
  %270 = fadd fast float %268, %269
  %271 = call float @dx.op.unary.f32(i32 24, float %270)  ; Sqrt(value)
  %272 = fdiv fast float %261, %271
  %273 = call float @dx.op.binary.f32(i32 35, float %272, float 0.000000e+00)  ; FMax(a,b)
  %274 = call float @dx.op.binary.f32(i32 36, float %273, float 1.000000e+00)  ; FMin(a,b)
  %275 = fmul fast float %274, %255
  %276 = fmul fast float %275, %251
  %277 = fsub fast float 1.000000e+00, %275
  %278 = fmul fast float %259, %277
  %279 = fsub fast float %116, %257
  %280 = call float @dx.op.binary.f32(i32 35, float %279, float 0.000000e+00)  ; FMax(a,b)
  %281 = fmul fast float %280, 0x3F847AE140000000
  %282 = fsub fast float %276, %281
  %283 = fadd fast float %282, %278
  %284 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %285 = extractvalue %dx.types.CBufRet.f32 %284, 0
  %286 = extractvalue %dx.types.CBufRet.f32 %284, 2
  %287 = call float @dx.op.dot2.f32(i32 54, float %285, float %286, float %285, float %286)  ; Dot2(ax,ay,bx,by)
  %288 = call float @dx.op.unary.f32(i32 25, float %287)  ; Rsqrt(value)
  %289 = fmul fast float %288, %285
  %290 = fmul fast float %288, %286
  %291 = fmul fast float %289, %279
  %292 = fmul fast float %290, %279
  %293 = fadd fast float %291, %112
  %294 = fadd fast float %292, %120
  %295 = call float @dx.op.dot2.f32(i32 54, float %293, float %294, float %289, float %290)  ; Dot2(ax,ay,bx,by)
  %296 = fsub fast float -0.000000e+00, %290
  %297 = call float @dx.op.dot2.f32(i32 54, float %293, float %294, float %296, float %289)  ; Dot2(ax,ay,bx,by)
  %298 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %299 = extractvalue %dx.types.CBufRet.f32 %298, 0
  %300 = extractvalue %dx.types.CBufRet.f32 %298, 1
  %301 = fmul fast float %299, 1.000000e+02
  %302 = fmul fast float %300, 1.000000e+02
  %303 = fsub fast float %295, %301
  %304 = fsub fast float %297, %302
  %305 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %306 = extractvalue %dx.types.CBufRet.f32 %305, 0
  %307 = fmul fast float %306, 1.000000e+02
  %308 = fdiv fast float %303, %307
  %309 = fdiv fast float %304, %307
  %310 = extractvalue %dx.types.CBufRet.f32 %298, 2
  %311 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %312 = extractvalue %dx.types.CBufRet.i32 %311, 3
  %313 = sitofp i32 %312 to float
  %314 = fmul fast float %313, %310
  %315 = fptosi float %314 to i32
  %316 = sitofp i32 %315 to float
  %317 = fsub fast float %314, %316
  %318 = add nsw i32 %315, 1
  %319 = srem i32 %318, %312
  %320 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %321 = extractvalue %dx.types.CBufRet.i32 %320, 0
  %322 = sitofp i32 %321 to float
  %323 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %324 = extractvalue %dx.types.CBufRet.f32 %323, 1
  %325 = fdiv fast float %283, %324
  %326 = fmul fast float %322, %325
  %327 = fptosi float %326 to i32
  %328 = sitofp i32 %327 to float
  %329 = fsub fast float %326, %328
  %330 = add nsw i32 %327, 1
  %331 = call i32 @dx.op.binary.i32(i32 38, i32 %330, i32 %321)  ; IMin(a,b)
  %332 = mul nsw i32 %312, %327
  %333 = add nsw i32 %332, %315
  %334 = add nsw i32 %332, %319
  %335 = mul nsw i32 %312, %331
  %336 = add nsw i32 %335, %315
  %337 = add nsw i32 %335, %319
  %338 = sitofp i32 %333 to float
  %339 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %16, float %308, float %309, float %338, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %340 = extractvalue %dx.types.ResRet.f32 %339, 0
  %341 = sitofp i32 %334 to float
  %342 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %16, float %308, float %309, float %341, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %343 = extractvalue %dx.types.ResRet.f32 %342, 0
  %344 = sitofp i32 %336 to float
  %345 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %16, float %308, float %309, float %344, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %346 = extractvalue %dx.types.ResRet.f32 %345, 0
  %347 = sitofp i32 %337 to float
  %348 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %10, %dx.types.Handle %16, float %308, float %309, float %347, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %349 = extractvalue %dx.types.ResRet.f32 %348, 0
  %350 = fsub fast float %346, %340
  %351 = fmul fast float %350, %329
  %352 = fadd fast float %351, %340
  %353 = fsub fast float %349, %343
  %354 = fmul fast float %353, %329
  %355 = fsub fast float %343, %352
  %356 = fadd fast float %355, %354
  %357 = fmul fast float %356, %317
  %358 = fadd fast float %357, %352
  %359 = extractvalue %dx.types.CBufRet.f32 %323, 2
  %360 = fmul fast float %358, %359
  %361 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %362 = extractvalue %dx.types.CBufRet.f32 %361, 0
  %363 = extractvalue %dx.types.CBufRet.f32 %361, 1
  %364 = extractvalue %dx.types.CBufRet.f32 %361, 2
  %365 = call float @dx.op.dot3.f32(i32 55, float %362, float %363, float %364, float 0.000000e+00, float 1.000000e+00, float 0.000000e+00)  ; Dot3(ax,ay,az,bx,by,bz)
  %366 = fmul fast float %365, %365
  %367 = fsub fast float 1.000000e+00, %366
  %368 = fmul fast float %367, 0x3FE1FF8A20000000
  %369 = fsub fast float 1.000000e+00, %368
  %370 = fcmp fast oge float %369, 0.000000e+00
  %371 = fmul fast float %363, 0x3FE7FFB160000000
  %372 = call float @dx.op.unary.f32(i32 24, float %369)  ; Sqrt(value)
  %373 = fmul fast float %365, 0xBFE7FFB160000000
  %374 = fsub fast float %373, %372
  %375 = fadd fast float %374, %371
  %376 = select i1 %370, float %375, float 0.000000e+00
  %377 = fcmp fast olt float %376, 0.000000e+00
  br i1 %377, label %378, label %522

; <label>:378                                     ; preds = %246
  %379 = call float @dx.op.unary.f32(i32 6, float %376)  ; FAbs(value)
  %380 = fdiv fast float %283, %379
  %381 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %382 = extractvalue %dx.types.CBufRet.f32 %381, 1
  %383 = fmul fast float %362, 0x4052BFC2A0000000
  %384 = select i1 %370, float %383, float 0.000000e+00
  %385 = fmul fast float %384, %380
  %386 = fmul fast float %385, %382
  %387 = fmul fast float %364, 0x4052BFC2A0000000
  %388 = select i1 %370, float %387, float 0.000000e+00
  %389 = fmul fast float %388, %380
  %390 = fmul fast float %389, %382
  %391 = fsub fast float %112, %386
  %392 = fsub fast float %120, %390
  %393 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %394 = extractvalue %dx.types.CBufRet.f32 %393, 0
  %395 = extractvalue %dx.types.CBufRet.f32 %393, 1
  %396 = extractvalue %dx.types.CBufRet.f32 %393, 2
  %397 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %398 = extractvalue %dx.types.CBufRet.f32 %397, 0
  %399 = extractvalue %dx.types.CBufRet.f32 %397, 1
  %400 = extractvalue %dx.types.CBufRet.f32 %397, 2
  %401 = fsub fast float %398, %394
  %402 = fsub fast float %399, %395
  %403 = fsub fast float %400, %396
  %404 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %405 = extractvalue %dx.types.CBufRet.f32 %404, 0
  %406 = extractvalue %dx.types.CBufRet.f32 %404, 1
  %407 = extractvalue %dx.types.CBufRet.f32 %404, 2
  %408 = fsub fast float %405, %398
  %409 = fsub fast float %406, %399
  %410 = fsub fast float %407, %400
  %411 = call float @dx.op.dot3.f32(i32 55, float %408, float %409, float %410, float %408, float %409, float %410)  ; Dot3(ax,ay,az,bx,by,bz)
  %412 = call float @dx.op.unary.f32(i32 25, float %411)  ; Rsqrt(value)
  %413 = fmul fast float %408, %412
  %414 = fmul fast float %409, %412
  %415 = fmul fast float %410, %412
  %416 = fsub fast float %405, %394
  %417 = fsub fast float %406, %395
  %418 = fsub fast float %407, %396
  %419 = fmul fast float %417, %415
  %420 = fmul fast float %418, %414
  %421 = fsub fast float %419, %420
  %422 = fmul fast float %418, %413
  %423 = fmul fast float %416, %415
  %424 = fsub fast float %422, %423
  %425 = fmul fast float %416, %414
  %426 = fmul fast float %417, %413
  %427 = fsub fast float %425, %426
  %428 = call float @dx.op.dot3.f32(i32 55, float %421, float %424, float %427, float %421, float %424, float %427)  ; Dot3(ax,ay,az,bx,by,bz)
  %429 = fsub fast float %391, %394
  %430 = fsub fast float %42, %395
  %431 = fsub fast float %392, %396
  %432 = fmul fast float %415, %430
  %433 = fmul fast float %414, %431
  %434 = fsub fast float %432, %433
  %435 = fmul fast float %413, %431
  %436 = fmul fast float %415, %429
  %437 = fsub fast float %435, %436
  %438 = fmul fast float %414, %429
  %439 = fmul fast float %413, %430
  %440 = fsub fast float %438, %439
  %441 = call float @dx.op.dot3.f32(i32 55, float %434, float %437, float %440, float %421, float %424, float %427)  ; Dot3(ax,ay,az,bx,by,bz)
  %442 = fdiv fast float %441, %428
  %443 = fcmp fast olt float %442, 0.000000e+00
  %444 = fcmp fast ogt float %442, 1.000000e+00
  %445 = or i1 %443, %444
  br i1 %445, label %502, label %446

; <label>:446                                     ; preds = %378
  %447 = fmul fast float %401, %401
  %448 = fmul fast float %402, %402
  %449 = fadd fast float %447, %448
  %450 = fmul fast float %403, %403
  %451 = fadd fast float %449, %450
  %452 = call float @dx.op.unary.f32(i32 24, float %451)  ; Sqrt(value)
  %453 = extractvalue %dx.types.CBufRet.f32 %393, 3
  %454 = fmul fast float %453, %452
  %455 = fadd fast float %454, 0xBFEFFFE700000000
  %456 = fmul fast float %455, %442
  %457 = fadd fast float %456, 1.000000e+00
  %458 = call float @dx.op.unary.f32(i32 23, float %457)  ; Log(value)
  %459 = fmul fast float %458, 0x3FE62E4300000000
  %460 = fadd fast float %454, 0x3EE9000020000000
  %461 = call float @dx.op.unary.f32(i32 23, float %460)  ; Log(value)
  %462 = fmul fast float %461, 0x3FE62E4300000000
  %463 = fdiv fast float %459, %462
  %464 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %465 = extractvalue %dx.types.CBufRet.f32 %464, 0
  %466 = extractvalue %dx.types.CBufRet.f32 %464, 1
  %467 = extractvalue %dx.types.CBufRet.f32 %464, 2
  %468 = fmul fast float %416, %442
  %469 = fmul fast float %417, %442
  %470 = fmul fast float %418, %442
  %471 = fadd fast float %468, %394
  %472 = fadd fast float %469, %395
  %473 = fadd fast float %470, %396
  %474 = fsub fast float %398, %465
  %475 = fsub fast float %399, %466
  %476 = fsub fast float %400, %467
  %477 = fmul fast float %474, %442
  %478 = fmul fast float %475, %442
  %479 = fmul fast float %476, %442
  %480 = fadd fast float %477, %465
  %481 = fadd fast float %478, %466
  %482 = fadd fast float %479, %467
  %483 = fsub fast float %480, %471
  %484 = fsub fast float %481, %472
  %485 = fsub fast float %482, %473
  %486 = call float @dx.op.dot3.f32(i32 55, float %483, float %484, float %485, float %483, float %484, float %485)  ; Dot3(ax,ay,az,bx,by,bz)
  %487 = call float @dx.op.unary.f32(i32 25, float %486)  ; Rsqrt(value)
  %488 = fmul fast float %483, %487
  %489 = fmul fast float %484, %487
  %490 = fmul fast float %485, %487
  %491 = fsub fast float %391, %471
  %492 = fsub fast float %42, %472
  %493 = fsub fast float %392, %473
  %494 = call float @dx.op.dot3.f32(i32 55, float %491, float %492, float %493, float %488, float %489, float %490)  ; Dot3(ax,ay,az,bx,by,bz)
  %495 = fmul fast float %483, %483
  %496 = fmul fast float %484, %484
  %497 = fadd fast float %495, %496
  %498 = fmul fast float %485, %485
  %499 = fadd fast float %497, %498
  %500 = call float @dx.op.unary.f32(i32 24, float %499)  ; Sqrt(value)
  %501 = fdiv fast float %494, %500
  br label %502

; <label>:502                                     ; preds = %446, %378
  %503 = phi float [ %501, %446 ], [ 0.000000e+00, %378 ]
  %504 = phi float [ %463, %446 ], [ 0.000000e+00, %378 ]
  %505 = phi i1 [ true, %446 ], [ false, %378 ]
  br i1 %505, label %506, label %522

; <label>:506                                     ; preds = %502
  %507 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %13, %dx.types.Handle %19, float %503, float %504, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %508 = extractvalue %dx.types.ResRet.f32 %507, 3
  %509 = fcmp fast ogt float %253, 0.000000e+00
  br i1 %509, label %510, label %517

; <label>:510                                     ; preds = %506
  %511 = extractvalue %dx.types.ResRet.f32 %507, 1
  %512 = fmul fast float %511, 1.000000e+02
  %513 = fdiv fast float %512, %253
  %514 = extractvalue %dx.types.CBufRet.f32 %381, 0
  %515 = fmul fast float %514, %513
  %516 = fadd fast float %515, %360
  br label %517

; <label>:517                                     ; preds = %510, %506
  %518 = phi float [ %516, %510 ], [ %360, %506 ]
  %519 = extractvalue %dx.types.CBufRet.f32 %381, 2
  %520 = fmul fast float %519, %508
  %521 = fadd fast float %520, %518
  br label %522

; <label>:522                                     ; preds = %517, %502, %246
  %523 = phi float [ %521, %517 ], [ %360, %502 ], [ %360, %246 ]
  %524 = fcmp fast ogt float %283, 0.000000e+00
  br i1 %524, label %525, label %530

; <label>:525                                     ; preds = %522
  %526 = call float @dx.op.binary.f32(i32 36, float %283, float 5.000000e+00)  ; FMin(a,b)
  %527 = fmul fast float %526, 0x3FC99999A0000000
  %528 = call float @dx.op.binary.f32(i32 35, float %527, float 0.000000e+00)  ; FMax(a,b)
  %529 = call float @dx.op.binary.f32(i32 36, float %528, float 1.000000e+00)  ; FMin(a,b)
  br label %539

; <label>:530                                     ; preds = %522
  %531 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %532 = extractvalue %dx.types.CBufRet.f32 %531, 1
  %533 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %534 = extractvalue %dx.types.CBufRet.f32 %533, 0
  %535 = fsub fast float %532, %534
  %536 = fcmp fast olt float %535, %42
  %537 = and i1 %126, %536
  %538 = select i1 %537, float 1.000000e+00, float 0.000000e+00
  br label %539

; <label>:539                                     ; preds = %530, %525
  %540 = phi float [ %529, %525 ], [ %538, %530 ]
  %541 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %3, %dx.types.Handle %14, float %39, float %40, float 3.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %542 = extractvalue %dx.types.ResRet.f32 %541, 3
  %543 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %544 = extractvalue %dx.types.CBufRet.f32 %543, 3
  %545 = fadd fast float %542, -1.000000e+00
  %546 = fmul fast float %544, %545
  %547 = fadd fast float %546, 1.000000e+00
  %548 = fmul fast float %547, %540
  %549 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %550 = extractvalue %dx.types.CBufRet.i32 %549, 0
  %551 = sitofp i32 %550 to float
  %552 = fmul fast float %551, %39
  %553 = fptoui float %552 to i32
  %554 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %20, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %555 = extractvalue %dx.types.CBufRet.i32 %554, 0
  %556 = sitofp i32 %555 to float
  %557 = fmul fast float %556, %40
  %558 = fptoui float %557 to i32
  %559 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %6, i32 0, i32 %553, i32 %558, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %560 = extractvalue %dx.types.ResRet.i32 %559, 1
  %561 = and i32 %560, 1
  %562 = icmp eq i32 %561, 0
  br i1 %562, label %567, label %563

; <label>:563                                     ; preds = %539
  %564 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %565 = extractvalue %dx.types.CBufRet.f32 %564, 2
  %566 = fmul fast float %565, %548
  br label %567

; <label>:567                                     ; preds = %563, %539
  %568 = phi float [ %566, %563 ], [ %548, %539 ]
  %569 = fcmp fast ogt float %253, 0.000000e+00
  br i1 %569, label %570, label %577

; <label>:570                                     ; preds = %567
  %571 = fsub fast float %116, %42
  %572 = fdiv fast float %571, %253
  %573 = fsub fast float 1.000000e+00, %572
  %574 = call float @dx.op.binary.f32(i32 35, float %573, float 0.000000e+00)  ; FMax(a,b)
  %575 = call float @dx.op.binary.f32(i32 36, float %574, float 1.000000e+00)  ; FMin(a,b)
  %576 = fmul fast float %575, %568
  br label %580

; <label>:577                                     ; preds = %567
  %578 = fcmp fast ogt float %116, %42
  br i1 %578, label %579, label %580

; <label>:579                                     ; preds = %577
  br label %580

; <label>:580                                     ; preds = %579, %577, %570
  %581 = phi float [ %576, %570 ], [ 0.000000e+00, %579 ], [ %568, %577 ]
  %582 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %4, %dx.types.Handle %15, float %247, float %248, float 2.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %583 = extractvalue %dx.types.ResRet.f32 %582, 0
  %584 = icmp ne i32 %249, 0
  %585 = fcmp uge float %283, 0.000000e+00
  %586 = and i1 %584, %585
  br i1 %586, label %587, label %627

; <label>:587                                     ; preds = %580
  %588 = fcmp fast ole float %583, 0.000000e+00
  %589 = call float @dx.op.unary.f32(i32 6, float %583)  ; FAbs(value)
  %590 = fcmp fast oge float %589, %283
  %591 = and i1 %588, %590
  %592 = select i1 %591, float 0.000000e+00, float %581
  %593 = fcmp fast ogt float %283, %324
  %594 = select i1 %593, float %324, float %283
  %595 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %9, i32 0, i32 %21, i32 %22, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %596 = extractvalue %dx.types.ResRet.f32 %595, 0
  %597 = extractvalue %dx.types.ResRet.f32 %595, 1
  %598 = extractvalue %dx.types.ResRet.f32 %595, 2
  %599 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %11, %dx.types.Handle %17, float %247, float %248, float 1.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %600 = extractvalue %dx.types.ResRet.f32 %599, 0
  %601 = extractvalue %dx.types.ResRet.f32 %599, 1
  %602 = extractvalue %dx.types.ResRet.f32 %599, 2
  %603 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %20, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %604 = extractvalue %dx.types.CBufRet.f32 %603, 0
  %605 = extractvalue %dx.types.CBufRet.f32 %603, 1
  %606 = extractvalue %dx.types.CBufRet.f32 %603, 2
  %607 = fsub fast float 1.000000e+00, %600
  %608 = fsub fast float 1.000000e+00, %601
  %609 = fsub fast float 1.000000e+00, %602
  %610 = fmul fast float %594, 0xBFF7154760000000
  %611 = fmul fast float %610, %607
  %612 = fmul fast float %610, %608
  %613 = fmul fast float %610, %609
  %614 = call float @dx.op.unary.f32(i32 21, float %611)  ; Exp(value)
  %615 = call float @dx.op.unary.f32(i32 21, float %612)  ; Exp(value)
  %616 = call float @dx.op.unary.f32(i32 21, float %613)  ; Exp(value)
  %617 = fmul fast float %592, %523
  %618 = fmul fast float %596, %617
  %619 = fmul fast float %618, %614
  %620 = fmul fast float %619, %604
  %621 = fmul fast float %597, %617
  %622 = fmul fast float %621, %605
  %623 = fmul fast float %622, %615
  %624 = fmul fast float %598, %617
  %625 = fmul fast float %624, %606
  %626 = fmul fast float %625, %616
  br label %627

; <label>:627                                     ; preds = %587, %580
  %628 = phi float [ %620, %587 ], [ 0.000000e+00, %580 ]
  %629 = phi float [ %623, %587 ], [ 0.000000e+00, %580 ]
  %630 = phi float [ %626, %587 ], [ 0.000000e+00, %580 ]
  %631 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %5, i32 0, i32 %21, i32 %22, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %632 = extractvalue %dx.types.ResRet.f32 %631, 0
  %633 = extractvalue %dx.types.ResRet.f32 %631, 1
  %634 = extractvalue %dx.types.ResRet.f32 %631, 2
  %635 = extractvalue %dx.types.ResRet.f32 %631, 3
  %636 = fadd fast float %632, %628
  %637 = fadd fast float %633, %629
  %638 = fadd fast float %634, %630
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %21, i32 %22, i32 undef, float %636, float %637, float %638, float %635, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  %639 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %8, i32 0, i32 %21, i32 %22, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %640 = extractvalue %dx.types.ResRet.f32 %639, 0
  %641 = extractvalue %dx.types.ResRet.f32 %639, 1
  %642 = extractvalue %dx.types.ResRet.f32 %639, 2
  %643 = extractvalue %dx.types.ResRet.f32 %639, 3
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2, i32 %21, i32 %22, i32 undef, float %640, float %641, float %642, float %643, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %644

; <label>:644                                     ; preds = %627, %0
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

; Function Attrs: nounwind readnone
declare float @dx.op.dot2.f32(i32, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

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
!dx.entryPoints = !{!31}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !19, !22, !24}
!5 = !{!6, !8, !9, !10, !11, !12, !13, !14, !16, !17, !18}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 9, i32 1, i32 7, i32 0, !7}
!10 = !{i32 3, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 10, i32 1, i32 7, i32 0, !7}
!11 = !{i32 4, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 11, i32 1, i32 2, i32 0, !7}
!12 = !{i32 5, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 2, i32 0, !7}
!13 = !{i32 6, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 13, i32 1, i32 2, i32 0, !7}
!14 = !{i32 7, %"class.Texture2D<vector<unsigned int, 2> >"* undef, !"", i32 0, i32 14, i32 1, i32 2, i32 0, !15}
!15 = !{i32 0, i32 5}
!16 = !{i32 8, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 15, i32 1, i32 2, i32 0, !7}
!17 = !{i32 9, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 7, i32 0, !7}
!18 = !{i32 10, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 7, i32 0, !7}
!19 = !{!20, !21}
!20 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!21 = !{i32 1, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!22 = !{!23}
!23 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 400, null}
!24 = !{!25, !26, !27, !28, !29, !30}
!25 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!26 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 0, i32 1, i32 0, null}
!27 = !{i32 2, %struct.SamplerState* undef, !"", i32 0, i32 9, i32 1, i32 0, null}
!28 = !{i32 3, %struct.SamplerState* undef, !"", i32 0, i32 10, i32 1, i32 0, null}
!29 = !{i32 4, %struct.SamplerState* undef, !"", i32 0, i32 1, i32 1, i32 0, null}
!30 = !{i32 5, %struct.SamplerState* undef, !"", i32 0, i32 2, i32 1, i32 0, null}
!31 = !{void ()* @main, !"main", null, !4, !32}
!32 = !{i32 4, !33}
!33 = !{i32 32, i32 32, i32 1}

