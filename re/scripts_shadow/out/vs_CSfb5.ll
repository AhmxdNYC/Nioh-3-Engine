;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; BLENDWEIGHT              0   xyzw        1     NONE   float   xyzw
; BLENDWEIGHT              1   xyzw        2     NONE   float   xyz 
; BINORMAL                 0   xyzw        3     NONE   float   xyzw
; COLOR                    1   xyzw        4     NONE   float   xyzw
; BLENDINDICES             0   xyzw        5     NONE     int   xyzw
; PSIZE                    0   xyzw        6     NONE     int   xyzw
; FOG                      0   xyzw        7     NONE     int   xyzw
; TEXCOORD                 5   xyzw        8     NONE     int   xyzw
; BLENDINDICES             1   xyzw        9     NONE     int   xyzw
; NORMAL                   0   xyzw       10     NONE   float      w
; TEXCOORD                 0   xy         11     NONE   float   xy  
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; TEXCOORD                 0   xy          1     NONE   float   xy  
;
; shader hash: 0be04eb22f459ce02a8a8eabcf3de7a9
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 12
; SigOutputElements: 2
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 12
; SigOutputVectors[0]: 2
; SigOutputVectors[1]: 0
; SigOutputVectors[2]: 0
; SigOutputVectors[3]: 0
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; POSITION                 0                              
; BLENDWEIGHT              0                              
; BLENDWEIGHT              1                              
; BINORMAL                 0                              
; COLOR                    1                              
; BLENDINDICES             0                              
; PSIZE                    0                              
; FOG                      0                              
; TEXCOORD                 5                              
; BLENDINDICES             1                              
; NORMAL                   0                              
; TEXCOORD                 0                              
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; TEXCOORD                 0                 linear       
;
; Buffer Definitions:
;
; cbuffer 
; {
;
;   [14448 x i8] (type annotation not present)
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;                                   cbuffer      NA          NA     CB0            cb0     1
;
;
; ViewId state:
;
; Number of inputs: 46, outputs: 6
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 1 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 2 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 3 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 4 depends on inputs: { 44 }
;   output 5 depends on inputs: { 45 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [384 x <4 x float>], [3 x i32], [512 x <4 x float>] }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 11, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 11, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 10, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %19 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %20 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %21 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %22 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %23 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %24 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %25 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %26 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %27 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %28 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %29 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %30 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %31 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %32 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %33 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %34 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %35 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %36 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %37 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %38 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %39 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %40 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %41 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %42 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %43 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %44 = sitofp i32 %17 to float
  %45 = sitofp i32 %18 to float
  %46 = sitofp i32 %19 to float
  %47 = sitofp i32 %20 to float
  %48 = call float @dx.op.dot4.f32(i32 56, float %44, float %45, float %46, float %47, float %44, float %45, float %46, float %47)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %49 = fcmp fast ogt float %48, 0.000000e+00
  %50 = add i32 %21, 391
  %51 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %50)  ; CBufferLoadLegacy(handle,regIndex)
  %52 = extractvalue %dx.types.CBufRet.f32 %51, 0
  %53 = extractvalue %dx.types.CBufRet.f32 %51, 1
  %54 = extractvalue %dx.types.CBufRet.f32 %51, 2
  %55 = fmul fast float %52, %40
  %56 = fmul fast float %53, %40
  %57 = fmul fast float %54, %40
  %58 = add i32 %22, 391
  %59 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %58)  ; CBufferLoadLegacy(handle,regIndex)
  %60 = extractvalue %dx.types.CBufRet.f32 %59, 0
  %61 = extractvalue %dx.types.CBufRet.f32 %59, 1
  %62 = extractvalue %dx.types.CBufRet.f32 %59, 2
  %63 = fmul fast float %60, %41
  %64 = fmul fast float %61, %41
  %65 = fmul fast float %62, %41
  %66 = fadd fast float %63, %55
  %67 = fadd fast float %64, %56
  %68 = fadd fast float %65, %57
  %69 = add i32 %23, 391
  %70 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %69)  ; CBufferLoadLegacy(handle,regIndex)
  %71 = extractvalue %dx.types.CBufRet.f32 %70, 0
  %72 = extractvalue %dx.types.CBufRet.f32 %70, 1
  %73 = extractvalue %dx.types.CBufRet.f32 %70, 2
  %74 = fmul fast float %71, %42
  %75 = fmul fast float %72, %42
  %76 = fmul fast float %73, %42
  %77 = fadd fast float %66, %74
  %78 = fadd fast float %67, %75
  %79 = fadd fast float %68, %76
  %80 = add i32 %24, 391
  %81 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %80)  ; CBufferLoadLegacy(handle,regIndex)
  %82 = extractvalue %dx.types.CBufRet.f32 %81, 0
  %83 = extractvalue %dx.types.CBufRet.f32 %81, 1
  %84 = extractvalue %dx.types.CBufRet.f32 %81, 2
  %85 = fmul fast float %82, %43
  %86 = fmul fast float %83, %43
  %87 = fmul fast float %84, %43
  %88 = fadd fast float %77, %85
  %89 = fadd fast float %78, %86
  %90 = fadd fast float %79, %87
  %91 = add i32 %17, 391
  %92 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %91)  ; CBufferLoadLegacy(handle,regIndex)
  %93 = extractvalue %dx.types.CBufRet.f32 %92, 0
  %94 = extractvalue %dx.types.CBufRet.f32 %92, 1
  %95 = extractvalue %dx.types.CBufRet.f32 %92, 2
  %96 = fmul fast float %93, %40
  %97 = fmul fast float %94, %40
  %98 = fmul fast float %95, %40
  %99 = add i32 %18, 391
  %100 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %99)  ; CBufferLoadLegacy(handle,regIndex)
  %101 = extractvalue %dx.types.CBufRet.f32 %100, 0
  %102 = extractvalue %dx.types.CBufRet.f32 %100, 1
  %103 = extractvalue %dx.types.CBufRet.f32 %100, 2
  %104 = fmul fast float %101, %41
  %105 = fmul fast float %102, %41
  %106 = fmul fast float %103, %41
  %107 = fadd fast float %104, %96
  %108 = fadd fast float %105, %97
  %109 = fadd fast float %106, %98
  %110 = add i32 %19, 391
  %111 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %110)  ; CBufferLoadLegacy(handle,regIndex)
  %112 = extractvalue %dx.types.CBufRet.f32 %111, 0
  %113 = extractvalue %dx.types.CBufRet.f32 %111, 1
  %114 = extractvalue %dx.types.CBufRet.f32 %111, 2
  %115 = fmul fast float %112, %42
  %116 = fmul fast float %113, %42
  %117 = fmul fast float %114, %42
  %118 = fadd fast float %107, %115
  %119 = fadd fast float %108, %116
  %120 = fadd fast float %109, %117
  %121 = add i32 %20, 391
  %122 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %121)  ; CBufferLoadLegacy(handle,regIndex)
  %123 = extractvalue %dx.types.CBufRet.f32 %122, 0
  %124 = extractvalue %dx.types.CBufRet.f32 %122, 1
  %125 = extractvalue %dx.types.CBufRet.f32 %122, 2
  %126 = fmul fast float %123, %43
  %127 = fmul fast float %124, %43
  %128 = fmul fast float %125, %43
  %129 = fadd fast float %118, %126
  %130 = fadd fast float %119, %127
  %131 = fadd fast float %120, %128
  %132 = add i32 %13, 391
  %133 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %132)  ; CBufferLoadLegacy(handle,regIndex)
  %134 = extractvalue %dx.types.CBufRet.f32 %133, 0
  %135 = extractvalue %dx.types.CBufRet.f32 %133, 1
  %136 = extractvalue %dx.types.CBufRet.f32 %133, 2
  %137 = fmul fast float %134, %40
  %138 = fmul fast float %135, %40
  %139 = fmul fast float %136, %40
  %140 = add i32 %14, 391
  %141 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %140)  ; CBufferLoadLegacy(handle,regIndex)
  %142 = extractvalue %dx.types.CBufRet.f32 %141, 0
  %143 = extractvalue %dx.types.CBufRet.f32 %141, 1
  %144 = extractvalue %dx.types.CBufRet.f32 %141, 2
  %145 = fmul fast float %142, %41
  %146 = fmul fast float %143, %41
  %147 = fmul fast float %144, %41
  %148 = fadd fast float %145, %137
  %149 = fadd fast float %146, %138
  %150 = fadd fast float %147, %139
  %151 = add i32 %15, 391
  %152 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %151)  ; CBufferLoadLegacy(handle,regIndex)
  %153 = extractvalue %dx.types.CBufRet.f32 %152, 0
  %154 = extractvalue %dx.types.CBufRet.f32 %152, 1
  %155 = extractvalue %dx.types.CBufRet.f32 %152, 2
  %156 = fmul fast float %153, %42
  %157 = fmul fast float %154, %42
  %158 = fmul fast float %155, %42
  %159 = fadd fast float %148, %156
  %160 = fadd fast float %149, %157
  %161 = fadd fast float %150, %158
  %162 = add i32 %16, 391
  %163 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %162)  ; CBufferLoadLegacy(handle,regIndex)
  %164 = extractvalue %dx.types.CBufRet.f32 %163, 0
  %165 = extractvalue %dx.types.CBufRet.f32 %163, 1
  %166 = extractvalue %dx.types.CBufRet.f32 %163, 2
  %167 = fmul fast float %164, %43
  %168 = fmul fast float %165, %43
  %169 = fmul fast float %166, %43
  %170 = fadd fast float %159, %167
  %171 = fadd fast float %160, %168
  %172 = fadd fast float %161, %169
  %173 = add i32 %9, 391
  %174 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %173)  ; CBufferLoadLegacy(handle,regIndex)
  %175 = extractvalue %dx.types.CBufRet.f32 %174, 0
  %176 = extractvalue %dx.types.CBufRet.f32 %174, 1
  %177 = extractvalue %dx.types.CBufRet.f32 %174, 2
  %178 = fmul fast float %175, %40
  %179 = fmul fast float %176, %40
  %180 = fmul fast float %177, %40
  %181 = add i32 %10, 391
  %182 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %181)  ; CBufferLoadLegacy(handle,regIndex)
  %183 = extractvalue %dx.types.CBufRet.f32 %182, 0
  %184 = extractvalue %dx.types.CBufRet.f32 %182, 1
  %185 = extractvalue %dx.types.CBufRet.f32 %182, 2
  %186 = fmul fast float %183, %41
  %187 = fmul fast float %184, %41
  %188 = fmul fast float %185, %41
  %189 = fadd fast float %186, %178
  %190 = fadd fast float %187, %179
  %191 = fadd fast float %188, %180
  %192 = add i32 %11, 391
  %193 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %192)  ; CBufferLoadLegacy(handle,regIndex)
  %194 = extractvalue %dx.types.CBufRet.f32 %193, 0
  %195 = extractvalue %dx.types.CBufRet.f32 %193, 1
  %196 = extractvalue %dx.types.CBufRet.f32 %193, 2
  %197 = fmul fast float %194, %42
  %198 = fmul fast float %195, %42
  %199 = fmul fast float %196, %42
  %200 = fadd fast float %189, %197
  %201 = fadd fast float %190, %198
  %202 = fadd fast float %191, %199
  %203 = add i32 %12, 391
  %204 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %203)  ; CBufferLoadLegacy(handle,regIndex)
  %205 = extractvalue %dx.types.CBufRet.f32 %204, 0
  %206 = extractvalue %dx.types.CBufRet.f32 %204, 1
  %207 = extractvalue %dx.types.CBufRet.f32 %204, 2
  %208 = fmul fast float %205, %43
  %209 = fmul fast float %206, %43
  %210 = fmul fast float %207, %43
  %211 = fadd fast float %200, %208
  %212 = fadd fast float %201, %209
  %213 = fadd fast float %202, %210
  %214 = fmul fast float %88, %36
  %215 = fmul fast float %89, %36
  %216 = fmul fast float %90, %36
  %217 = fmul fast float %129, %37
  %218 = fmul fast float %130, %37
  %219 = fmul fast float %131, %37
  %220 = fadd fast float %217, %214
  %221 = fadd fast float %218, %215
  %222 = fadd fast float %219, %216
  %223 = fmul fast float %170, %38
  %224 = fmul fast float %171, %38
  %225 = fmul fast float %172, %38
  %226 = fadd fast float %220, %223
  %227 = fadd fast float %221, %224
  %228 = fadd fast float %222, %225
  %229 = fmul fast float %211, %39
  %230 = fmul fast float %212, %39
  %231 = fmul fast float %213, %39
  %232 = fadd fast float %226, %229
  %233 = fadd fast float %227, %230
  %234 = fadd fast float %228, %231
  %235 = fmul fast float %88, %25
  %236 = fmul fast float %89, %25
  %237 = fmul fast float %90, %25
  %238 = fmul fast float %129, %26
  %239 = fmul fast float %130, %26
  %240 = fmul fast float %131, %26
  %241 = fadd fast float %238, %235
  %242 = fadd fast float %239, %236
  %243 = fadd fast float %240, %237
  %244 = fmul fast float %170, %27
  %245 = fmul fast float %171, %27
  %246 = fmul fast float %172, %27
  %247 = fadd fast float %241, %244
  %248 = fadd fast float %242, %245
  %249 = fadd fast float %243, %246
  %250 = fmul fast float %211, %28
  %251 = fmul fast float %212, %28
  %252 = fmul fast float %213, %28
  %253 = fadd fast float %247, %250
  %254 = fadd fast float %248, %251
  %255 = fadd fast float %249, %252
  %256 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %50)  ; CBufferLoadLegacy(handle,regIndex)
  %257 = extractvalue %dx.types.CBufRet.f32 %256, 0
  %258 = extractvalue %dx.types.CBufRet.f32 %256, 1
  %259 = extractvalue %dx.types.CBufRet.f32 %256, 2
  %260 = fmul fast float %257, %29
  %261 = fmul fast float %258, %29
  %262 = fmul fast float %259, %29
  %263 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %58)  ; CBufferLoadLegacy(handle,regIndex)
  %264 = extractvalue %dx.types.CBufRet.f32 %263, 0
  %265 = extractvalue %dx.types.CBufRet.f32 %263, 1
  %266 = extractvalue %dx.types.CBufRet.f32 %263, 2
  %267 = fmul fast float %264, %30
  %268 = fmul fast float %265, %30
  %269 = fmul fast float %266, %30
  %270 = fadd fast float %267, %260
  %271 = fadd fast float %268, %261
  %272 = fadd fast float %269, %262
  %273 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %69)  ; CBufferLoadLegacy(handle,regIndex)
  %274 = extractvalue %dx.types.CBufRet.f32 %273, 0
  %275 = extractvalue %dx.types.CBufRet.f32 %273, 1
  %276 = extractvalue %dx.types.CBufRet.f32 %273, 2
  %277 = fmul fast float %274, %31
  %278 = fmul fast float %275, %31
  %279 = fmul fast float %276, %31
  %280 = fadd fast float %270, %277
  %281 = fadd fast float %271, %278
  %282 = fadd fast float %272, %279
  %283 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %80)  ; CBufferLoadLegacy(handle,regIndex)
  %284 = extractvalue %dx.types.CBufRet.f32 %283, 0
  %285 = extractvalue %dx.types.CBufRet.f32 %283, 1
  %286 = extractvalue %dx.types.CBufRet.f32 %283, 2
  %287 = fmul fast float %284, %32
  %288 = fmul fast float %285, %32
  %289 = fmul fast float %286, %32
  %290 = fadd fast float %280, %287
  %291 = fadd fast float %281, %288
  %292 = fadd fast float %282, %289
  %293 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %91)  ; CBufferLoadLegacy(handle,regIndex)
  %294 = extractvalue %dx.types.CBufRet.f32 %293, 0
  %295 = extractvalue %dx.types.CBufRet.f32 %293, 1
  %296 = extractvalue %dx.types.CBufRet.f32 %293, 2
  %297 = fmul fast float %294, %29
  %298 = fmul fast float %295, %29
  %299 = fmul fast float %296, %29
  %300 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %99)  ; CBufferLoadLegacy(handle,regIndex)
  %301 = extractvalue %dx.types.CBufRet.f32 %300, 0
  %302 = extractvalue %dx.types.CBufRet.f32 %300, 1
  %303 = extractvalue %dx.types.CBufRet.f32 %300, 2
  %304 = fmul fast float %301, %30
  %305 = fmul fast float %302, %30
  %306 = fmul fast float %303, %30
  %307 = fadd fast float %304, %297
  %308 = fadd fast float %305, %298
  %309 = fadd fast float %306, %299
  %310 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %110)  ; CBufferLoadLegacy(handle,regIndex)
  %311 = extractvalue %dx.types.CBufRet.f32 %310, 0
  %312 = extractvalue %dx.types.CBufRet.f32 %310, 1
  %313 = extractvalue %dx.types.CBufRet.f32 %310, 2
  %314 = fmul fast float %311, %31
  %315 = fmul fast float %312, %31
  %316 = fmul fast float %313, %31
  %317 = fadd fast float %307, %314
  %318 = fadd fast float %308, %315
  %319 = fadd fast float %309, %316
  %320 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %121)  ; CBufferLoadLegacy(handle,regIndex)
  %321 = extractvalue %dx.types.CBufRet.f32 %320, 0
  %322 = extractvalue %dx.types.CBufRet.f32 %320, 1
  %323 = extractvalue %dx.types.CBufRet.f32 %320, 2
  %324 = fmul fast float %321, %32
  %325 = fmul fast float %322, %32
  %326 = fmul fast float %323, %32
  %327 = fadd fast float %317, %324
  %328 = fadd fast float %318, %325
  %329 = fadd fast float %319, %326
  %330 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %132)  ; CBufferLoadLegacy(handle,regIndex)
  %331 = extractvalue %dx.types.CBufRet.f32 %330, 0
  %332 = extractvalue %dx.types.CBufRet.f32 %330, 1
  %333 = extractvalue %dx.types.CBufRet.f32 %330, 2
  %334 = fmul fast float %331, %29
  %335 = fmul fast float %332, %29
  %336 = fmul fast float %333, %29
  %337 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %140)  ; CBufferLoadLegacy(handle,regIndex)
  %338 = extractvalue %dx.types.CBufRet.f32 %337, 0
  %339 = extractvalue %dx.types.CBufRet.f32 %337, 1
  %340 = extractvalue %dx.types.CBufRet.f32 %337, 2
  %341 = fmul fast float %338, %30
  %342 = fmul fast float %339, %30
  %343 = fmul fast float %340, %30
  %344 = fadd fast float %341, %334
  %345 = fadd fast float %342, %335
  %346 = fadd fast float %343, %336
  %347 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %151)  ; CBufferLoadLegacy(handle,regIndex)
  %348 = extractvalue %dx.types.CBufRet.f32 %347, 0
  %349 = extractvalue %dx.types.CBufRet.f32 %347, 1
  %350 = extractvalue %dx.types.CBufRet.f32 %347, 2
  %351 = fmul fast float %348, %31
  %352 = fmul fast float %349, %31
  %353 = fmul fast float %350, %31
  %354 = fadd fast float %344, %351
  %355 = fadd fast float %345, %352
  %356 = fadd fast float %346, %353
  %357 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %162)  ; CBufferLoadLegacy(handle,regIndex)
  %358 = extractvalue %dx.types.CBufRet.f32 %357, 0
  %359 = extractvalue %dx.types.CBufRet.f32 %357, 1
  %360 = extractvalue %dx.types.CBufRet.f32 %357, 2
  %361 = fmul fast float %358, %32
  %362 = fmul fast float %359, %32
  %363 = fmul fast float %360, %32
  %364 = fadd fast float %354, %361
  %365 = fadd fast float %355, %362
  %366 = fadd fast float %356, %363
  %367 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %173)  ; CBufferLoadLegacy(handle,regIndex)
  %368 = extractvalue %dx.types.CBufRet.f32 %367, 0
  %369 = extractvalue %dx.types.CBufRet.f32 %367, 1
  %370 = extractvalue %dx.types.CBufRet.f32 %367, 2
  %371 = fmul fast float %368, %29
  %372 = fmul fast float %369, %29
  %373 = fmul fast float %370, %29
  %374 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %181)  ; CBufferLoadLegacy(handle,regIndex)
  %375 = extractvalue %dx.types.CBufRet.f32 %374, 0
  %376 = extractvalue %dx.types.CBufRet.f32 %374, 1
  %377 = extractvalue %dx.types.CBufRet.f32 %374, 2
  %378 = fmul fast float %375, %30
  %379 = fmul fast float %376, %30
  %380 = fmul fast float %377, %30
  %381 = fadd fast float %378, %371
  %382 = fadd fast float %379, %372
  %383 = fadd fast float %380, %373
  %384 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %192)  ; CBufferLoadLegacy(handle,regIndex)
  %385 = extractvalue %dx.types.CBufRet.f32 %384, 0
  %386 = extractvalue %dx.types.CBufRet.f32 %384, 1
  %387 = extractvalue %dx.types.CBufRet.f32 %384, 2
  %388 = fmul fast float %385, %31
  %389 = fmul fast float %386, %31
  %390 = fmul fast float %387, %31
  %391 = fadd fast float %381, %388
  %392 = fadd fast float %382, %389
  %393 = fadd fast float %383, %390
  %394 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %203)  ; CBufferLoadLegacy(handle,regIndex)
  %395 = extractvalue %dx.types.CBufRet.f32 %394, 0
  %396 = extractvalue %dx.types.CBufRet.f32 %394, 1
  %397 = extractvalue %dx.types.CBufRet.f32 %394, 2
  %398 = fmul fast float %395, %32
  %399 = fmul fast float %396, %32
  %400 = fmul fast float %397, %32
  %401 = fadd fast float %391, %398
  %402 = fadd fast float %392, %399
  %403 = fadd fast float %393, %400
  %404 = fmul fast float %290, %36
  %405 = fmul fast float %291, %36
  %406 = fmul fast float %292, %36
  %407 = fmul fast float %327, %37
  %408 = fmul fast float %328, %37
  %409 = fmul fast float %329, %37
  %410 = fadd fast float %407, %404
  %411 = fadd fast float %408, %405
  %412 = fadd fast float %409, %406
  %413 = fmul fast float %364, %38
  %414 = fmul fast float %365, %38
  %415 = fmul fast float %366, %38
  %416 = fadd fast float %410, %413
  %417 = fadd fast float %411, %414
  %418 = fadd fast float %412, %415
  %419 = fmul fast float %401, %39
  %420 = fmul fast float %402, %39
  %421 = fmul fast float %403, %39
  %422 = fadd fast float %416, %419
  %423 = fadd fast float %417, %420
  %424 = fadd fast float %418, %421
  %425 = fmul fast float %424, %254
  %426 = fmul fast float %423, %255
  %427 = fsub fast float %425, %426
  %428 = fmul fast float %422, %255
  %429 = fmul fast float %424, %253
  %430 = fsub fast float %428, %429
  %431 = fmul fast float %423, %253
  %432 = fmul fast float %422, %254
  %433 = fsub fast float %431, %432
  %434 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 391)  ; CBufferLoadLegacy(handle,regIndex)
  %435 = extractvalue %dx.types.CBufRet.f32 %434, 3
  %436 = fcmp fast olt float %435, 0.000000e+00
  br i1 %436, label %437, label %443

; <label>:437                                     ; preds = %0
  %438 = call float @dx.op.dot3.f32(i32 55, float %427, float %430, float %433, float %427, float %430, float %433)  ; Dot3(ax,ay,az,bx,by,bz)
  %439 = call float @dx.op.unary.f32(i32 25, float %438)  ; Rsqrt(value)
  %440 = fmul fast float %439, %427
  %441 = fmul fast float %439, %430
  %442 = fmul fast float %439, %433
  br label %443

; <label>:443                                     ; preds = %437, %0
  %444 = phi float [ %440, %437 ], [ %427, %0 ]
  %445 = phi float [ %441, %437 ], [ %430, %0 ]
  %446 = phi float [ %442, %437 ], [ %433, %0 ]
  br i1 %49, label %447, label %454

; <label>:447                                     ; preds = %443
  %448 = fmul fast float %446, %4
  %449 = fadd fast float %234, %448
  %450 = fmul fast float %445, %4
  %451 = fadd fast float %233, %450
  %452 = fmul fast float %444, %4
  %453 = fadd fast float %232, %452
  br label %454

; <label>:454                                     ; preds = %447, %443
  %455 = phi i32 [ 0, %447 ], [ %5, %443 ]
  %456 = phi i32 [ 0, %447 ], [ %6, %443 ]
  %457 = phi i32 [ 0, %447 ], [ %7, %443 ]
  %458 = phi i32 [ 0, %447 ], [ %8, %443 ]
  %459 = phi i32 [ 0, %447 ], [ %21, %443 ]
  %460 = phi i32 [ 0, %447 ], [ %22, %443 ]
  %461 = phi i32 [ 0, %447 ], [ %23, %443 ]
  %462 = phi i32 [ 0, %447 ], [ %24, %443 ]
  %463 = phi float [ 0.000000e+00, %447 ], [ %33, %443 ]
  %464 = phi float [ 0.000000e+00, %447 ], [ %34, %443 ]
  %465 = phi float [ 0.000000e+00, %447 ], [ %35, %443 ]
  %466 = phi float [ 1.000000e+00, %447 ], [ %36, %443 ]
  %467 = phi float [ 0.000000e+00, %447 ], [ %37, %443 ]
  %468 = phi float [ 0.000000e+00, %447 ], [ %38, %443 ]
  %469 = phi float [ 0.000000e+00, %447 ], [ %39, %443 ]
  %470 = phi float [ %453, %447 ], [ %40, %443 ]
  %471 = phi float [ %451, %447 ], [ %41, %443 ]
  %472 = phi float [ %449, %447 ], [ %42, %443 ]
  %473 = phi float [ 1.000000e+00, %447 ], [ %43, %443 ]
  %474 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 390)  ; CBufferLoadLegacy(handle,regIndex)
  %475 = extractvalue %dx.types.CBufRet.i32 %474, 0
  %476 = icmp eq i32 %475, 0
  %477 = add i32 %459, 4
  %478 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %477)  ; CBufferLoadLegacy(handle,regIndex)
  %479 = extractvalue %dx.types.CBufRet.f32 %478, 0
  %480 = extractvalue %dx.types.CBufRet.f32 %478, 1
  %481 = extractvalue %dx.types.CBufRet.f32 %478, 2
  %482 = extractvalue %dx.types.CBufRet.f32 %478, 3
  %483 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %479, float %480, float %481, float %482)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %484 = add i32 %459, 5
  %485 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %484)  ; CBufferLoadLegacy(handle,regIndex)
  %486 = extractvalue %dx.types.CBufRet.f32 %485, 0
  %487 = extractvalue %dx.types.CBufRet.f32 %485, 1
  %488 = extractvalue %dx.types.CBufRet.f32 %485, 2
  %489 = extractvalue %dx.types.CBufRet.f32 %485, 3
  %490 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %486, float %487, float %488, float %489)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %491 = add i32 %459, 6
  %492 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %491)  ; CBufferLoadLegacy(handle,regIndex)
  %493 = extractvalue %dx.types.CBufRet.f32 %492, 0
  %494 = extractvalue %dx.types.CBufRet.f32 %492, 1
  %495 = extractvalue %dx.types.CBufRet.f32 %492, 2
  %496 = extractvalue %dx.types.CBufRet.f32 %492, 3
  %497 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %493, float %494, float %495, float %496)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %498 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 388)  ; CBufferLoadLegacy(handle,regIndex)
  %499 = extractvalue %dx.types.CBufRet.i32 %498, 0
  %500 = icmp eq i32 %499, 0
  %501 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 389)  ; CBufferLoadLegacy(handle,regIndex)
  %502 = extractvalue %dx.types.CBufRet.i32 %501, 0
  %503 = icmp eq i32 %502, 0
  br i1 %476, label %756, label %504

; <label>:504                                     ; preds = %454
  %505 = fmul fast float %483, %466
  %506 = fmul fast float %490, %466
  %507 = fmul fast float %497, %466
  %508 = add i32 %460, 4
  %509 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %508)  ; CBufferLoadLegacy(handle,regIndex)
  %510 = extractvalue %dx.types.CBufRet.f32 %509, 0
  %511 = extractvalue %dx.types.CBufRet.f32 %509, 1
  %512 = extractvalue %dx.types.CBufRet.f32 %509, 2
  %513 = extractvalue %dx.types.CBufRet.f32 %509, 3
  %514 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %510, float %511, float %512, float %513)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %515 = add i32 %460, 5
  %516 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %515)  ; CBufferLoadLegacy(handle,regIndex)
  %517 = extractvalue %dx.types.CBufRet.f32 %516, 0
  %518 = extractvalue %dx.types.CBufRet.f32 %516, 1
  %519 = extractvalue %dx.types.CBufRet.f32 %516, 2
  %520 = extractvalue %dx.types.CBufRet.f32 %516, 3
  %521 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %517, float %518, float %519, float %520)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %522 = add i32 %460, 6
  %523 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %522)  ; CBufferLoadLegacy(handle,regIndex)
  %524 = extractvalue %dx.types.CBufRet.f32 %523, 0
  %525 = extractvalue %dx.types.CBufRet.f32 %523, 1
  %526 = extractvalue %dx.types.CBufRet.f32 %523, 2
  %527 = extractvalue %dx.types.CBufRet.f32 %523, 3
  %528 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %524, float %525, float %526, float %527)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %529 = fmul fast float %514, %467
  %530 = fmul fast float %521, %467
  %531 = fmul fast float %528, %467
  %532 = fadd fast float %529, %505
  %533 = fadd fast float %530, %506
  %534 = fadd fast float %531, %507
  %535 = add i32 %461, 4
  %536 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %535)  ; CBufferLoadLegacy(handle,regIndex)
  %537 = extractvalue %dx.types.CBufRet.f32 %536, 0
  %538 = extractvalue %dx.types.CBufRet.f32 %536, 1
  %539 = extractvalue %dx.types.CBufRet.f32 %536, 2
  %540 = extractvalue %dx.types.CBufRet.f32 %536, 3
  %541 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %537, float %538, float %539, float %540)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %542 = add i32 %461, 5
  %543 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %542)  ; CBufferLoadLegacy(handle,regIndex)
  %544 = extractvalue %dx.types.CBufRet.f32 %543, 0
  %545 = extractvalue %dx.types.CBufRet.f32 %543, 1
  %546 = extractvalue %dx.types.CBufRet.f32 %543, 2
  %547 = extractvalue %dx.types.CBufRet.f32 %543, 3
  %548 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %544, float %545, float %546, float %547)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %549 = add i32 %461, 6
  %550 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %549)  ; CBufferLoadLegacy(handle,regIndex)
  %551 = extractvalue %dx.types.CBufRet.f32 %550, 0
  %552 = extractvalue %dx.types.CBufRet.f32 %550, 1
  %553 = extractvalue %dx.types.CBufRet.f32 %550, 2
  %554 = extractvalue %dx.types.CBufRet.f32 %550, 3
  %555 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %551, float %552, float %553, float %554)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %556 = fmul fast float %541, %468
  %557 = fmul fast float %548, %468
  %558 = fmul fast float %555, %468
  %559 = fadd fast float %532, %556
  %560 = fadd fast float %533, %557
  %561 = fadd fast float %534, %558
  %562 = add i32 %462, 4
  %563 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %562)  ; CBufferLoadLegacy(handle,regIndex)
  %564 = extractvalue %dx.types.CBufRet.f32 %563, 0
  %565 = extractvalue %dx.types.CBufRet.f32 %563, 1
  %566 = extractvalue %dx.types.CBufRet.f32 %563, 2
  %567 = extractvalue %dx.types.CBufRet.f32 %563, 3
  %568 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %564, float %565, float %566, float %567)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %569 = add i32 %462, 5
  %570 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %569)  ; CBufferLoadLegacy(handle,regIndex)
  %571 = extractvalue %dx.types.CBufRet.f32 %570, 0
  %572 = extractvalue %dx.types.CBufRet.f32 %570, 1
  %573 = extractvalue %dx.types.CBufRet.f32 %570, 2
  %574 = extractvalue %dx.types.CBufRet.f32 %570, 3
  %575 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %571, float %572, float %573, float %574)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %576 = add i32 %462, 6
  %577 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %576)  ; CBufferLoadLegacy(handle,regIndex)
  %578 = extractvalue %dx.types.CBufRet.f32 %577, 0
  %579 = extractvalue %dx.types.CBufRet.f32 %577, 1
  %580 = extractvalue %dx.types.CBufRet.f32 %577, 2
  %581 = extractvalue %dx.types.CBufRet.f32 %577, 3
  %582 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %578, float %579, float %580, float %581)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %583 = fmul fast float %568, %469
  %584 = fmul fast float %575, %469
  %585 = fmul fast float %582, %469
  %586 = fadd fast float %559, %583
  %587 = fadd fast float %560, %584
  %588 = fadd fast float %561, %585
  %589 = call float @dx.op.dot4.f32(i32 56, float %466, float %467, float %468, float %469, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %590 = add i32 %455, 4
  %591 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %590)  ; CBufferLoadLegacy(handle,regIndex)
  %592 = extractvalue %dx.types.CBufRet.f32 %591, 0
  %593 = extractvalue %dx.types.CBufRet.f32 %591, 1
  %594 = extractvalue %dx.types.CBufRet.f32 %591, 2
  %595 = extractvalue %dx.types.CBufRet.f32 %591, 3
  %596 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %592, float %593, float %594, float %595)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %597 = add i32 %455, 5
  %598 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %597)  ; CBufferLoadLegacy(handle,regIndex)
  %599 = extractvalue %dx.types.CBufRet.f32 %598, 0
  %600 = extractvalue %dx.types.CBufRet.f32 %598, 1
  %601 = extractvalue %dx.types.CBufRet.f32 %598, 2
  %602 = extractvalue %dx.types.CBufRet.f32 %598, 3
  %603 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %599, float %600, float %601, float %602)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %604 = add i32 %455, 6
  %605 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %604)  ; CBufferLoadLegacy(handle,regIndex)
  %606 = extractvalue %dx.types.CBufRet.f32 %605, 0
  %607 = extractvalue %dx.types.CBufRet.f32 %605, 1
  %608 = extractvalue %dx.types.CBufRet.f32 %605, 2
  %609 = extractvalue %dx.types.CBufRet.f32 %605, 3
  %610 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %606, float %607, float %608, float %609)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  br i1 %500, label %712, label %611

; <label>:611                                     ; preds = %504
  %612 = fmul fast float %596, %463
  %613 = fmul fast float %603, %463
  %614 = fmul fast float %610, %463
  %615 = fadd fast float %612, %586
  %616 = fadd fast float %613, %587
  %617 = fadd fast float %614, %588
  %618 = add i32 %456, 4
  %619 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %618)  ; CBufferLoadLegacy(handle,regIndex)
  %620 = extractvalue %dx.types.CBufRet.f32 %619, 0
  %621 = extractvalue %dx.types.CBufRet.f32 %619, 1
  %622 = extractvalue %dx.types.CBufRet.f32 %619, 2
  %623 = extractvalue %dx.types.CBufRet.f32 %619, 3
  %624 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %620, float %621, float %622, float %623)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %625 = add i32 %456, 5
  %626 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %625)  ; CBufferLoadLegacy(handle,regIndex)
  %627 = extractvalue %dx.types.CBufRet.f32 %626, 0
  %628 = extractvalue %dx.types.CBufRet.f32 %626, 1
  %629 = extractvalue %dx.types.CBufRet.f32 %626, 2
  %630 = extractvalue %dx.types.CBufRet.f32 %626, 3
  %631 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %627, float %628, float %629, float %630)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %632 = add i32 %456, 6
  %633 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %632)  ; CBufferLoadLegacy(handle,regIndex)
  %634 = extractvalue %dx.types.CBufRet.f32 %633, 0
  %635 = extractvalue %dx.types.CBufRet.f32 %633, 1
  %636 = extractvalue %dx.types.CBufRet.f32 %633, 2
  %637 = extractvalue %dx.types.CBufRet.f32 %633, 3
  %638 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %634, float %635, float %636, float %637)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %639 = fmul fast float %624, %464
  %640 = fmul fast float %631, %464
  %641 = fmul fast float %638, %464
  %642 = fadd fast float %639, %615
  %643 = fadd fast float %640, %616
  %644 = fadd fast float %641, %617
  %645 = add i32 %457, 4
  %646 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %645)  ; CBufferLoadLegacy(handle,regIndex)
  %647 = extractvalue %dx.types.CBufRet.f32 %646, 0
  %648 = extractvalue %dx.types.CBufRet.f32 %646, 1
  %649 = extractvalue %dx.types.CBufRet.f32 %646, 2
  %650 = extractvalue %dx.types.CBufRet.f32 %646, 3
  %651 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %647, float %648, float %649, float %650)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %652 = add i32 %457, 5
  %653 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %652)  ; CBufferLoadLegacy(handle,regIndex)
  %654 = extractvalue %dx.types.CBufRet.f32 %653, 0
  %655 = extractvalue %dx.types.CBufRet.f32 %653, 1
  %656 = extractvalue %dx.types.CBufRet.f32 %653, 2
  %657 = extractvalue %dx.types.CBufRet.f32 %653, 3
  %658 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %654, float %655, float %656, float %657)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %659 = add i32 %457, 6
  %660 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %659)  ; CBufferLoadLegacy(handle,regIndex)
  %661 = extractvalue %dx.types.CBufRet.f32 %660, 0
  %662 = extractvalue %dx.types.CBufRet.f32 %660, 1
  %663 = extractvalue %dx.types.CBufRet.f32 %660, 2
  %664 = extractvalue %dx.types.CBufRet.f32 %660, 3
  %665 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %661, float %662, float %663, float %664)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %666 = fsub fast float 1.000000e+00, %463
  %667 = fsub fast float %666, %464
  br i1 %503, label %704, label %668

; <label>:668                                     ; preds = %611
  %669 = fmul fast float %651, %465
  %670 = fmul fast float %658, %465
  %671 = fmul fast float %665, %465
  %672 = fadd fast float %642, %669
  %673 = fadd fast float %643, %670
  %674 = fadd fast float %644, %671
  %675 = fsub fast float %667, %465
  %676 = fsub fast float %675, %589
  %677 = add i32 %458, 4
  %678 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %677)  ; CBufferLoadLegacy(handle,regIndex)
  %679 = extractvalue %dx.types.CBufRet.f32 %678, 0
  %680 = extractvalue %dx.types.CBufRet.f32 %678, 1
  %681 = extractvalue %dx.types.CBufRet.f32 %678, 2
  %682 = extractvalue %dx.types.CBufRet.f32 %678, 3
  %683 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %679, float %680, float %681, float %682)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %684 = add i32 %458, 5
  %685 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %684)  ; CBufferLoadLegacy(handle,regIndex)
  %686 = extractvalue %dx.types.CBufRet.f32 %685, 0
  %687 = extractvalue %dx.types.CBufRet.f32 %685, 1
  %688 = extractvalue %dx.types.CBufRet.f32 %685, 2
  %689 = extractvalue %dx.types.CBufRet.f32 %685, 3
  %690 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %686, float %687, float %688, float %689)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %691 = add i32 %458, 6
  %692 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %691)  ; CBufferLoadLegacy(handle,regIndex)
  %693 = extractvalue %dx.types.CBufRet.f32 %692, 0
  %694 = extractvalue %dx.types.CBufRet.f32 %692, 1
  %695 = extractvalue %dx.types.CBufRet.f32 %692, 2
  %696 = extractvalue %dx.types.CBufRet.f32 %692, 3
  %697 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %693, float %694, float %695, float %696)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %698 = fmul fast float %683, %676
  %699 = fmul fast float %690, %676
  %700 = fmul fast float %697, %676
  %701 = fadd fast float %672, %698
  %702 = fadd fast float %673, %699
  %703 = fadd fast float %674, %700
  br label %886

; <label>:704                                     ; preds = %611
  %705 = fsub fast float %667, %589
  %706 = fmul fast float %651, %705
  %707 = fmul fast float %658, %705
  %708 = fmul fast float %665, %705
  %709 = fadd fast float %642, %706
  %710 = fadd fast float %643, %707
  %711 = fadd fast float %644, %708
  br label %886

; <label>:712                                     ; preds = %504
  %713 = fsub fast float 1.000000e+00, %589
  br i1 %503, label %749, label %714

; <label>:714                                     ; preds = %712
  %715 = fmul fast float %596, %463
  %716 = fmul fast float %603, %463
  %717 = fmul fast float %610, %463
  %718 = fadd fast float %715, %586
  %719 = fadd fast float %716, %587
  %720 = fadd fast float %717, %588
  %721 = fsub fast float %713, %463
  %722 = add i32 %456, 4
  %723 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %722)  ; CBufferLoadLegacy(handle,regIndex)
  %724 = extractvalue %dx.types.CBufRet.f32 %723, 0
  %725 = extractvalue %dx.types.CBufRet.f32 %723, 1
  %726 = extractvalue %dx.types.CBufRet.f32 %723, 2
  %727 = extractvalue %dx.types.CBufRet.f32 %723, 3
  %728 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %724, float %725, float %726, float %727)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %729 = add i32 %456, 5
  %730 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %729)  ; CBufferLoadLegacy(handle,regIndex)
  %731 = extractvalue %dx.types.CBufRet.f32 %730, 0
  %732 = extractvalue %dx.types.CBufRet.f32 %730, 1
  %733 = extractvalue %dx.types.CBufRet.f32 %730, 2
  %734 = extractvalue %dx.types.CBufRet.f32 %730, 3
  %735 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %731, float %732, float %733, float %734)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %736 = add i32 %456, 6
  %737 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %736)  ; CBufferLoadLegacy(handle,regIndex)
  %738 = extractvalue %dx.types.CBufRet.f32 %737, 0
  %739 = extractvalue %dx.types.CBufRet.f32 %737, 1
  %740 = extractvalue %dx.types.CBufRet.f32 %737, 2
  %741 = extractvalue %dx.types.CBufRet.f32 %737, 3
  %742 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %738, float %739, float %740, float %741)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %743 = fmul fast float %728, %721
  %744 = fmul fast float %735, %721
  %745 = fmul fast float %742, %721
  %746 = fadd fast float %718, %743
  %747 = fadd fast float %719, %744
  %748 = fadd fast float %720, %745
  br label %886

; <label>:749                                     ; preds = %712
  %750 = fmul fast float %596, %713
  %751 = fmul fast float %603, %713
  %752 = fmul fast float %610, %713
  %753 = fadd fast float %750, %586
  %754 = fadd fast float %751, %587
  %755 = fadd fast float %752, %588
  br label %886

; <label>:756                                     ; preds = %454
  br i1 %500, label %853, label %757

; <label>:757                                     ; preds = %756
  %758 = fmul fast float %483, %466
  %759 = fmul fast float %490, %466
  %760 = fmul fast float %497, %466
  %761 = add i32 %460, 4
  %762 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %761)  ; CBufferLoadLegacy(handle,regIndex)
  %763 = extractvalue %dx.types.CBufRet.f32 %762, 0
  %764 = extractvalue %dx.types.CBufRet.f32 %762, 1
  %765 = extractvalue %dx.types.CBufRet.f32 %762, 2
  %766 = extractvalue %dx.types.CBufRet.f32 %762, 3
  %767 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %763, float %764, float %765, float %766)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %768 = add i32 %460, 5
  %769 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %768)  ; CBufferLoadLegacy(handle,regIndex)
  %770 = extractvalue %dx.types.CBufRet.f32 %769, 0
  %771 = extractvalue %dx.types.CBufRet.f32 %769, 1
  %772 = extractvalue %dx.types.CBufRet.f32 %769, 2
  %773 = extractvalue %dx.types.CBufRet.f32 %769, 3
  %774 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %770, float %771, float %772, float %773)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %775 = add i32 %460, 6
  %776 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %775)  ; CBufferLoadLegacy(handle,regIndex)
  %777 = extractvalue %dx.types.CBufRet.f32 %776, 0
  %778 = extractvalue %dx.types.CBufRet.f32 %776, 1
  %779 = extractvalue %dx.types.CBufRet.f32 %776, 2
  %780 = extractvalue %dx.types.CBufRet.f32 %776, 3
  %781 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %777, float %778, float %779, float %780)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %782 = fmul fast float %767, %467
  %783 = fmul fast float %774, %467
  %784 = fmul fast float %781, %467
  %785 = fadd fast float %782, %758
  %786 = fadd fast float %783, %759
  %787 = fadd fast float %784, %760
  %788 = add i32 %461, 4
  %789 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %788)  ; CBufferLoadLegacy(handle,regIndex)
  %790 = extractvalue %dx.types.CBufRet.f32 %789, 0
  %791 = extractvalue %dx.types.CBufRet.f32 %789, 1
  %792 = extractvalue %dx.types.CBufRet.f32 %789, 2
  %793 = extractvalue %dx.types.CBufRet.f32 %789, 3
  %794 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %790, float %791, float %792, float %793)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %795 = add i32 %461, 5
  %796 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %795)  ; CBufferLoadLegacy(handle,regIndex)
  %797 = extractvalue %dx.types.CBufRet.f32 %796, 0
  %798 = extractvalue %dx.types.CBufRet.f32 %796, 1
  %799 = extractvalue %dx.types.CBufRet.f32 %796, 2
  %800 = extractvalue %dx.types.CBufRet.f32 %796, 3
  %801 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %797, float %798, float %799, float %800)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %802 = add i32 %461, 6
  %803 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %802)  ; CBufferLoadLegacy(handle,regIndex)
  %804 = extractvalue %dx.types.CBufRet.f32 %803, 0
  %805 = extractvalue %dx.types.CBufRet.f32 %803, 1
  %806 = extractvalue %dx.types.CBufRet.f32 %803, 2
  %807 = extractvalue %dx.types.CBufRet.f32 %803, 3
  %808 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %804, float %805, float %806, float %807)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %809 = fsub fast float 1.000000e+00, %466
  %810 = fsub fast float %809, %467
  br i1 %503, label %846, label %811

; <label>:811                                     ; preds = %757
  %812 = fmul fast float %794, %468
  %813 = fmul fast float %801, %468
  %814 = fmul fast float %808, %468
  %815 = fadd fast float %785, %812
  %816 = fadd fast float %786, %813
  %817 = fadd fast float %787, %814
  %818 = fsub fast float %810, %468
  %819 = add i32 %462, 4
  %820 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %819)  ; CBufferLoadLegacy(handle,regIndex)
  %821 = extractvalue %dx.types.CBufRet.f32 %820, 0
  %822 = extractvalue %dx.types.CBufRet.f32 %820, 1
  %823 = extractvalue %dx.types.CBufRet.f32 %820, 2
  %824 = extractvalue %dx.types.CBufRet.f32 %820, 3
  %825 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %821, float %822, float %823, float %824)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %826 = add i32 %462, 5
  %827 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %826)  ; CBufferLoadLegacy(handle,regIndex)
  %828 = extractvalue %dx.types.CBufRet.f32 %827, 0
  %829 = extractvalue %dx.types.CBufRet.f32 %827, 1
  %830 = extractvalue %dx.types.CBufRet.f32 %827, 2
  %831 = extractvalue %dx.types.CBufRet.f32 %827, 3
  %832 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %828, float %829, float %830, float %831)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %833 = add i32 %462, 6
  %834 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %833)  ; CBufferLoadLegacy(handle,regIndex)
  %835 = extractvalue %dx.types.CBufRet.f32 %834, 0
  %836 = extractvalue %dx.types.CBufRet.f32 %834, 1
  %837 = extractvalue %dx.types.CBufRet.f32 %834, 2
  %838 = extractvalue %dx.types.CBufRet.f32 %834, 3
  %839 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %835, float %836, float %837, float %838)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %840 = fmul fast float %825, %818
  %841 = fmul fast float %832, %818
  %842 = fmul fast float %839, %818
  %843 = fadd fast float %815, %840
  %844 = fadd fast float %816, %841
  %845 = fadd fast float %817, %842
  br label %886

; <label>:846                                     ; preds = %757
  %847 = fmul fast float %794, %810
  %848 = fmul fast float %801, %810
  %849 = fmul fast float %808, %810
  %850 = fadd fast float %785, %847
  %851 = fadd fast float %786, %848
  %852 = fadd fast float %787, %849
  br label %886

; <label>:853                                     ; preds = %756
  br i1 %503, label %886, label %854

; <label>:854                                     ; preds = %853
  %855 = fmul fast float %483, %466
  %856 = fmul fast float %490, %466
  %857 = fmul fast float %497, %466
  %858 = fsub fast float 1.000000e+00, %466
  %859 = add i32 %460, 4
  %860 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %859)  ; CBufferLoadLegacy(handle,regIndex)
  %861 = extractvalue %dx.types.CBufRet.f32 %860, 0
  %862 = extractvalue %dx.types.CBufRet.f32 %860, 1
  %863 = extractvalue %dx.types.CBufRet.f32 %860, 2
  %864 = extractvalue %dx.types.CBufRet.f32 %860, 3
  %865 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %861, float %862, float %863, float %864)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %866 = add i32 %460, 5
  %867 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %866)  ; CBufferLoadLegacy(handle,regIndex)
  %868 = extractvalue %dx.types.CBufRet.f32 %867, 0
  %869 = extractvalue %dx.types.CBufRet.f32 %867, 1
  %870 = extractvalue %dx.types.CBufRet.f32 %867, 2
  %871 = extractvalue %dx.types.CBufRet.f32 %867, 3
  %872 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %868, float %869, float %870, float %871)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %873 = add i32 %460, 6
  %874 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %873)  ; CBufferLoadLegacy(handle,regIndex)
  %875 = extractvalue %dx.types.CBufRet.f32 %874, 0
  %876 = extractvalue %dx.types.CBufRet.f32 %874, 1
  %877 = extractvalue %dx.types.CBufRet.f32 %874, 2
  %878 = extractvalue %dx.types.CBufRet.f32 %874, 3
  %879 = call float @dx.op.dot4.f32(i32 56, float %470, float %471, float %472, float %473, float %875, float %876, float %877, float %878)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %880 = fmul fast float %865, %858
  %881 = fmul fast float %872, %858
  %882 = fmul fast float %879, %858
  %883 = fadd fast float %880, %855
  %884 = fadd fast float %881, %856
  %885 = fadd fast float %882, %857
  br label %886

; <label>:886                                     ; preds = %854, %853, %846, %811, %749, %714, %704, %668
  %887 = phi float [ %701, %668 ], [ %709, %704 ], [ %746, %714 ], [ %753, %749 ], [ %843, %811 ], [ %850, %846 ], [ %883, %854 ], [ %483, %853 ]
  %888 = phi float [ %702, %668 ], [ %710, %704 ], [ %747, %714 ], [ %754, %749 ], [ %844, %811 ], [ %851, %846 ], [ %884, %854 ], [ %490, %853 ]
  %889 = phi float [ %703, %668 ], [ %711, %704 ], [ %748, %714 ], [ %755, %749 ], [ %845, %811 ], [ %852, %846 ], [ %885, %854 ], [ %497, %853 ]
  %890 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %891 = extractvalue %dx.types.CBufRet.f32 %890, 0
  %892 = extractvalue %dx.types.CBufRet.f32 %890, 1
  %893 = extractvalue %dx.types.CBufRet.f32 %890, 2
  %894 = extractvalue %dx.types.CBufRet.f32 %890, 3
  %895 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %896 = extractvalue %dx.types.CBufRet.f32 %895, 0
  %897 = extractvalue %dx.types.CBufRet.f32 %895, 1
  %898 = extractvalue %dx.types.CBufRet.f32 %895, 2
  %899 = extractvalue %dx.types.CBufRet.f32 %895, 3
  %900 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %901 = extractvalue %dx.types.CBufRet.f32 %900, 0
  %902 = extractvalue %dx.types.CBufRet.f32 %900, 1
  %903 = extractvalue %dx.types.CBufRet.f32 %900, 2
  %904 = extractvalue %dx.types.CBufRet.f32 %900, 3
  %905 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %906 = extractvalue %dx.types.CBufRet.f32 %905, 0
  %907 = extractvalue %dx.types.CBufRet.f32 %905, 1
  %908 = extractvalue %dx.types.CBufRet.f32 %905, 2
  %909 = extractvalue %dx.types.CBufRet.f32 %905, 3
  %910 = fmul fast float %891, %887
  %911 = call float @dx.op.tertiary.f32(i32 46, float %888, float %896, float %910)  ; FMad(a,b,c)
  %912 = call float @dx.op.tertiary.f32(i32 46, float %889, float %901, float %911)  ; FMad(a,b,c)
  %913 = fadd fast float %912, %906
  %914 = fmul fast float %892, %887
  %915 = call float @dx.op.tertiary.f32(i32 46, float %888, float %897, float %914)  ; FMad(a,b,c)
  %916 = call float @dx.op.tertiary.f32(i32 46, float %889, float %902, float %915)  ; FMad(a,b,c)
  %917 = fadd fast float %916, %907
  %918 = fmul fast float %893, %887
  %919 = call float @dx.op.tertiary.f32(i32 46, float %888, float %898, float %918)  ; FMad(a,b,c)
  %920 = call float @dx.op.tertiary.f32(i32 46, float %889, float %903, float %919)  ; FMad(a,b,c)
  %921 = fadd fast float %920, %908
  %922 = fmul fast float %894, %887
  %923 = call float @dx.op.tertiary.f32(i32 46, float %888, float %899, float %922)  ; FMad(a,b,c)
  %924 = call float @dx.op.tertiary.f32(i32 46, float %889, float %904, float %923)  ; FMad(a,b,c)
  %925 = fadd fast float %924, %909
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %913)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %917)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %921)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %925)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %2)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

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
!dx.viewIdState = !{!6}
!dx.entryPoints = !{!7}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 5}
!2 = !{!"vs", i32 6, i32 5}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 14448, null}
!6 = !{[48 x i32] [i32 46, i32 6, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 0, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 0, i32 0, i32 0, i32 15, i32 16, i32 32]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !29, null}
!9 = !{!10, !13, !14, !17, !18, !19, !20, !21, !22, !24, !25, !27}
!10 = !{i32 0, !"POSITION", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 0, i8 0, !12}
!11 = !{i32 0}
!12 = !{i32 3, i32 15}
!13 = !{i32 1, !"BLENDWEIGHT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 1, i8 0, !12}
!14 = !{i32 2, !"BLENDWEIGHT", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 2, i8 0, !16}
!15 = !{i32 1}
!16 = !{i32 3, i32 7}
!17 = !{i32 3, !"BINORMAL", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 3, i8 0, !12}
!18 = !{i32 4, !"COLOR", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 4, i8 0, !12}
!19 = !{i32 5, !"BLENDINDICES", i8 4, i8 0, !11, i8 0, i32 1, i8 4, i32 5, i8 0, !12}
!20 = !{i32 6, !"PSIZE", i8 4, i8 0, !11, i8 0, i32 1, i8 4, i32 6, i8 0, !12}
!21 = !{i32 7, !"FOG", i8 4, i8 0, !11, i8 0, i32 1, i8 4, i32 7, i8 0, !12}
!22 = !{i32 8, !"TEXCOORD", i8 4, i8 0, !23, i8 0, i32 1, i8 4, i32 8, i8 0, !12}
!23 = !{i32 5}
!24 = !{i32 9, !"BLENDINDICES", i8 4, i8 0, !15, i8 0, i32 1, i8 4, i32 9, i8 0, !12}
!25 = !{i32 10, !"NORMAL", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 10, i8 0, !26}
!26 = !{i32 3, i32 8}
!27 = !{i32 11, !"TEXCOORD", i8 9, i8 0, !11, i8 0, i32 1, i8 2, i32 11, i8 0, !28}
!28 = !{i32 3, i32 3}
!29 = !{!30, !31}
!30 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}
!31 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !11, i8 2, i32 1, i8 2, i32 1, i8 0, !28}

