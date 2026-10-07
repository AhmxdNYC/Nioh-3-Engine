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
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
;
; shader hash: 1e27b1ae2c47d3d1691a0bff2db238f1
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 11
; SigOutputElements: 1
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 11
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
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
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
; Number of inputs: 44, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 1 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 2 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;   output 3 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 43 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [384 x <4 x float>], [3 x i32], [512 x <4 x float>] }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 10, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call i32 @dx.op.loadInput.i32(i32 4, i32 9, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call i32 @dx.op.loadInput.i32(i32 4, i32 8, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call i32 @dx.op.loadInput.i32(i32 4, i32 7, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = call i32 @dx.op.loadInput.i32(i32 4, i32 6, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %19 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %20 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %21 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %22 = call i32 @dx.op.loadInput.i32(i32 4, i32 5, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %23 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %24 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %25 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %26 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %27 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %28 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %29 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %30 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %31 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %32 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %33 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %34 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %35 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %36 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %37 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %38 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %39 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %40 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %41 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %42 = sitofp i32 %15 to float
  %43 = sitofp i32 %16 to float
  %44 = sitofp i32 %17 to float
  %45 = sitofp i32 %18 to float
  %46 = call float @dx.op.dot4.f32(i32 56, float %42, float %43, float %44, float %45, float %42, float %43, float %44, float %45)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %47 = fcmp fast ogt float %46, 0.000000e+00
  %48 = add i32 %19, 391
  %49 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %48)  ; CBufferLoadLegacy(handle,regIndex)
  %50 = extractvalue %dx.types.CBufRet.f32 %49, 0
  %51 = extractvalue %dx.types.CBufRet.f32 %49, 1
  %52 = extractvalue %dx.types.CBufRet.f32 %49, 2
  %53 = fmul fast float %50, %38
  %54 = fmul fast float %51, %38
  %55 = fmul fast float %52, %38
  %56 = add i32 %20, 391
  %57 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %56)  ; CBufferLoadLegacy(handle,regIndex)
  %58 = extractvalue %dx.types.CBufRet.f32 %57, 0
  %59 = extractvalue %dx.types.CBufRet.f32 %57, 1
  %60 = extractvalue %dx.types.CBufRet.f32 %57, 2
  %61 = fmul fast float %58, %39
  %62 = fmul fast float %59, %39
  %63 = fmul fast float %60, %39
  %64 = fadd fast float %61, %53
  %65 = fadd fast float %62, %54
  %66 = fadd fast float %63, %55
  %67 = add i32 %21, 391
  %68 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %67)  ; CBufferLoadLegacy(handle,regIndex)
  %69 = extractvalue %dx.types.CBufRet.f32 %68, 0
  %70 = extractvalue %dx.types.CBufRet.f32 %68, 1
  %71 = extractvalue %dx.types.CBufRet.f32 %68, 2
  %72 = fmul fast float %69, %40
  %73 = fmul fast float %70, %40
  %74 = fmul fast float %71, %40
  %75 = fadd fast float %64, %72
  %76 = fadd fast float %65, %73
  %77 = fadd fast float %66, %74
  %78 = add i32 %22, 391
  %79 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %78)  ; CBufferLoadLegacy(handle,regIndex)
  %80 = extractvalue %dx.types.CBufRet.f32 %79, 0
  %81 = extractvalue %dx.types.CBufRet.f32 %79, 1
  %82 = extractvalue %dx.types.CBufRet.f32 %79, 2
  %83 = fmul fast float %80, %41
  %84 = fmul fast float %81, %41
  %85 = fmul fast float %82, %41
  %86 = fadd fast float %75, %83
  %87 = fadd fast float %76, %84
  %88 = fadd fast float %77, %85
  %89 = add i32 %15, 391
  %90 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %89)  ; CBufferLoadLegacy(handle,regIndex)
  %91 = extractvalue %dx.types.CBufRet.f32 %90, 0
  %92 = extractvalue %dx.types.CBufRet.f32 %90, 1
  %93 = extractvalue %dx.types.CBufRet.f32 %90, 2
  %94 = fmul fast float %91, %38
  %95 = fmul fast float %92, %38
  %96 = fmul fast float %93, %38
  %97 = add i32 %16, 391
  %98 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %97)  ; CBufferLoadLegacy(handle,regIndex)
  %99 = extractvalue %dx.types.CBufRet.f32 %98, 0
  %100 = extractvalue %dx.types.CBufRet.f32 %98, 1
  %101 = extractvalue %dx.types.CBufRet.f32 %98, 2
  %102 = fmul fast float %99, %39
  %103 = fmul fast float %100, %39
  %104 = fmul fast float %101, %39
  %105 = fadd fast float %102, %94
  %106 = fadd fast float %103, %95
  %107 = fadd fast float %104, %96
  %108 = add i32 %17, 391
  %109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %108)  ; CBufferLoadLegacy(handle,regIndex)
  %110 = extractvalue %dx.types.CBufRet.f32 %109, 0
  %111 = extractvalue %dx.types.CBufRet.f32 %109, 1
  %112 = extractvalue %dx.types.CBufRet.f32 %109, 2
  %113 = fmul fast float %110, %40
  %114 = fmul fast float %111, %40
  %115 = fmul fast float %112, %40
  %116 = fadd fast float %105, %113
  %117 = fadd fast float %106, %114
  %118 = fadd fast float %107, %115
  %119 = add i32 %18, 391
  %120 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %119)  ; CBufferLoadLegacy(handle,regIndex)
  %121 = extractvalue %dx.types.CBufRet.f32 %120, 0
  %122 = extractvalue %dx.types.CBufRet.f32 %120, 1
  %123 = extractvalue %dx.types.CBufRet.f32 %120, 2
  %124 = fmul fast float %121, %41
  %125 = fmul fast float %122, %41
  %126 = fmul fast float %123, %41
  %127 = fadd fast float %116, %124
  %128 = fadd fast float %117, %125
  %129 = fadd fast float %118, %126
  %130 = add i32 %11, 391
  %131 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %130)  ; CBufferLoadLegacy(handle,regIndex)
  %132 = extractvalue %dx.types.CBufRet.f32 %131, 0
  %133 = extractvalue %dx.types.CBufRet.f32 %131, 1
  %134 = extractvalue %dx.types.CBufRet.f32 %131, 2
  %135 = fmul fast float %132, %38
  %136 = fmul fast float %133, %38
  %137 = fmul fast float %134, %38
  %138 = add i32 %12, 391
  %139 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %138)  ; CBufferLoadLegacy(handle,regIndex)
  %140 = extractvalue %dx.types.CBufRet.f32 %139, 0
  %141 = extractvalue %dx.types.CBufRet.f32 %139, 1
  %142 = extractvalue %dx.types.CBufRet.f32 %139, 2
  %143 = fmul fast float %140, %39
  %144 = fmul fast float %141, %39
  %145 = fmul fast float %142, %39
  %146 = fadd fast float %143, %135
  %147 = fadd fast float %144, %136
  %148 = fadd fast float %145, %137
  %149 = add i32 %13, 391
  %150 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %149)  ; CBufferLoadLegacy(handle,regIndex)
  %151 = extractvalue %dx.types.CBufRet.f32 %150, 0
  %152 = extractvalue %dx.types.CBufRet.f32 %150, 1
  %153 = extractvalue %dx.types.CBufRet.f32 %150, 2
  %154 = fmul fast float %151, %40
  %155 = fmul fast float %152, %40
  %156 = fmul fast float %153, %40
  %157 = fadd fast float %146, %154
  %158 = fadd fast float %147, %155
  %159 = fadd fast float %148, %156
  %160 = add i32 %14, 391
  %161 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %160)  ; CBufferLoadLegacy(handle,regIndex)
  %162 = extractvalue %dx.types.CBufRet.f32 %161, 0
  %163 = extractvalue %dx.types.CBufRet.f32 %161, 1
  %164 = extractvalue %dx.types.CBufRet.f32 %161, 2
  %165 = fmul fast float %162, %41
  %166 = fmul fast float %163, %41
  %167 = fmul fast float %164, %41
  %168 = fadd fast float %157, %165
  %169 = fadd fast float %158, %166
  %170 = fadd fast float %159, %167
  %171 = add i32 %7, 391
  %172 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %171)  ; CBufferLoadLegacy(handle,regIndex)
  %173 = extractvalue %dx.types.CBufRet.f32 %172, 0
  %174 = extractvalue %dx.types.CBufRet.f32 %172, 1
  %175 = extractvalue %dx.types.CBufRet.f32 %172, 2
  %176 = fmul fast float %173, %38
  %177 = fmul fast float %174, %38
  %178 = fmul fast float %175, %38
  %179 = add i32 %8, 391
  %180 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %179)  ; CBufferLoadLegacy(handle,regIndex)
  %181 = extractvalue %dx.types.CBufRet.f32 %180, 0
  %182 = extractvalue %dx.types.CBufRet.f32 %180, 1
  %183 = extractvalue %dx.types.CBufRet.f32 %180, 2
  %184 = fmul fast float %181, %39
  %185 = fmul fast float %182, %39
  %186 = fmul fast float %183, %39
  %187 = fadd fast float %184, %176
  %188 = fadd fast float %185, %177
  %189 = fadd fast float %186, %178
  %190 = add i32 %9, 391
  %191 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %190)  ; CBufferLoadLegacy(handle,regIndex)
  %192 = extractvalue %dx.types.CBufRet.f32 %191, 0
  %193 = extractvalue %dx.types.CBufRet.f32 %191, 1
  %194 = extractvalue %dx.types.CBufRet.f32 %191, 2
  %195 = fmul fast float %192, %40
  %196 = fmul fast float %193, %40
  %197 = fmul fast float %194, %40
  %198 = fadd fast float %187, %195
  %199 = fadd fast float %188, %196
  %200 = fadd fast float %189, %197
  %201 = add i32 %10, 391
  %202 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %201)  ; CBufferLoadLegacy(handle,regIndex)
  %203 = extractvalue %dx.types.CBufRet.f32 %202, 0
  %204 = extractvalue %dx.types.CBufRet.f32 %202, 1
  %205 = extractvalue %dx.types.CBufRet.f32 %202, 2
  %206 = fmul fast float %203, %41
  %207 = fmul fast float %204, %41
  %208 = fmul fast float %205, %41
  %209 = fadd fast float %198, %206
  %210 = fadd fast float %199, %207
  %211 = fadd fast float %200, %208
  %212 = fmul fast float %86, %34
  %213 = fmul fast float %87, %34
  %214 = fmul fast float %88, %34
  %215 = fmul fast float %127, %35
  %216 = fmul fast float %128, %35
  %217 = fmul fast float %129, %35
  %218 = fadd fast float %215, %212
  %219 = fadd fast float %216, %213
  %220 = fadd fast float %217, %214
  %221 = fmul fast float %168, %36
  %222 = fmul fast float %169, %36
  %223 = fmul fast float %170, %36
  %224 = fadd fast float %218, %221
  %225 = fadd fast float %219, %222
  %226 = fadd fast float %220, %223
  %227 = fmul fast float %209, %37
  %228 = fmul fast float %210, %37
  %229 = fmul fast float %211, %37
  %230 = fadd fast float %224, %227
  %231 = fadd fast float %225, %228
  %232 = fadd fast float %226, %229
  %233 = fmul fast float %86, %23
  %234 = fmul fast float %87, %23
  %235 = fmul fast float %88, %23
  %236 = fmul fast float %127, %24
  %237 = fmul fast float %128, %24
  %238 = fmul fast float %129, %24
  %239 = fadd fast float %236, %233
  %240 = fadd fast float %237, %234
  %241 = fadd fast float %238, %235
  %242 = fmul fast float %168, %25
  %243 = fmul fast float %169, %25
  %244 = fmul fast float %170, %25
  %245 = fadd fast float %239, %242
  %246 = fadd fast float %240, %243
  %247 = fadd fast float %241, %244
  %248 = fmul fast float %209, %26
  %249 = fmul fast float %210, %26
  %250 = fmul fast float %211, %26
  %251 = fadd fast float %245, %248
  %252 = fadd fast float %246, %249
  %253 = fadd fast float %247, %250
  %254 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %48)  ; CBufferLoadLegacy(handle,regIndex)
  %255 = extractvalue %dx.types.CBufRet.f32 %254, 0
  %256 = extractvalue %dx.types.CBufRet.f32 %254, 1
  %257 = extractvalue %dx.types.CBufRet.f32 %254, 2
  %258 = fmul fast float %255, %27
  %259 = fmul fast float %256, %27
  %260 = fmul fast float %257, %27
  %261 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %56)  ; CBufferLoadLegacy(handle,regIndex)
  %262 = extractvalue %dx.types.CBufRet.f32 %261, 0
  %263 = extractvalue %dx.types.CBufRet.f32 %261, 1
  %264 = extractvalue %dx.types.CBufRet.f32 %261, 2
  %265 = fmul fast float %262, %28
  %266 = fmul fast float %263, %28
  %267 = fmul fast float %264, %28
  %268 = fadd fast float %265, %258
  %269 = fadd fast float %266, %259
  %270 = fadd fast float %267, %260
  %271 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %67)  ; CBufferLoadLegacy(handle,regIndex)
  %272 = extractvalue %dx.types.CBufRet.f32 %271, 0
  %273 = extractvalue %dx.types.CBufRet.f32 %271, 1
  %274 = extractvalue %dx.types.CBufRet.f32 %271, 2
  %275 = fmul fast float %272, %29
  %276 = fmul fast float %273, %29
  %277 = fmul fast float %274, %29
  %278 = fadd fast float %268, %275
  %279 = fadd fast float %269, %276
  %280 = fadd fast float %270, %277
  %281 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %78)  ; CBufferLoadLegacy(handle,regIndex)
  %282 = extractvalue %dx.types.CBufRet.f32 %281, 0
  %283 = extractvalue %dx.types.CBufRet.f32 %281, 1
  %284 = extractvalue %dx.types.CBufRet.f32 %281, 2
  %285 = fmul fast float %282, %30
  %286 = fmul fast float %283, %30
  %287 = fmul fast float %284, %30
  %288 = fadd fast float %278, %285
  %289 = fadd fast float %279, %286
  %290 = fadd fast float %280, %287
  %291 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %89)  ; CBufferLoadLegacy(handle,regIndex)
  %292 = extractvalue %dx.types.CBufRet.f32 %291, 0
  %293 = extractvalue %dx.types.CBufRet.f32 %291, 1
  %294 = extractvalue %dx.types.CBufRet.f32 %291, 2
  %295 = fmul fast float %292, %27
  %296 = fmul fast float %293, %27
  %297 = fmul fast float %294, %27
  %298 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %97)  ; CBufferLoadLegacy(handle,regIndex)
  %299 = extractvalue %dx.types.CBufRet.f32 %298, 0
  %300 = extractvalue %dx.types.CBufRet.f32 %298, 1
  %301 = extractvalue %dx.types.CBufRet.f32 %298, 2
  %302 = fmul fast float %299, %28
  %303 = fmul fast float %300, %28
  %304 = fmul fast float %301, %28
  %305 = fadd fast float %302, %295
  %306 = fadd fast float %303, %296
  %307 = fadd fast float %304, %297
  %308 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %108)  ; CBufferLoadLegacy(handle,regIndex)
  %309 = extractvalue %dx.types.CBufRet.f32 %308, 0
  %310 = extractvalue %dx.types.CBufRet.f32 %308, 1
  %311 = extractvalue %dx.types.CBufRet.f32 %308, 2
  %312 = fmul fast float %309, %29
  %313 = fmul fast float %310, %29
  %314 = fmul fast float %311, %29
  %315 = fadd fast float %305, %312
  %316 = fadd fast float %306, %313
  %317 = fadd fast float %307, %314
  %318 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %119)  ; CBufferLoadLegacy(handle,regIndex)
  %319 = extractvalue %dx.types.CBufRet.f32 %318, 0
  %320 = extractvalue %dx.types.CBufRet.f32 %318, 1
  %321 = extractvalue %dx.types.CBufRet.f32 %318, 2
  %322 = fmul fast float %319, %30
  %323 = fmul fast float %320, %30
  %324 = fmul fast float %321, %30
  %325 = fadd fast float %315, %322
  %326 = fadd fast float %316, %323
  %327 = fadd fast float %317, %324
  %328 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %130)  ; CBufferLoadLegacy(handle,regIndex)
  %329 = extractvalue %dx.types.CBufRet.f32 %328, 0
  %330 = extractvalue %dx.types.CBufRet.f32 %328, 1
  %331 = extractvalue %dx.types.CBufRet.f32 %328, 2
  %332 = fmul fast float %329, %27
  %333 = fmul fast float %330, %27
  %334 = fmul fast float %331, %27
  %335 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %138)  ; CBufferLoadLegacy(handle,regIndex)
  %336 = extractvalue %dx.types.CBufRet.f32 %335, 0
  %337 = extractvalue %dx.types.CBufRet.f32 %335, 1
  %338 = extractvalue %dx.types.CBufRet.f32 %335, 2
  %339 = fmul fast float %336, %28
  %340 = fmul fast float %337, %28
  %341 = fmul fast float %338, %28
  %342 = fadd fast float %339, %332
  %343 = fadd fast float %340, %333
  %344 = fadd fast float %341, %334
  %345 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %149)  ; CBufferLoadLegacy(handle,regIndex)
  %346 = extractvalue %dx.types.CBufRet.f32 %345, 0
  %347 = extractvalue %dx.types.CBufRet.f32 %345, 1
  %348 = extractvalue %dx.types.CBufRet.f32 %345, 2
  %349 = fmul fast float %346, %29
  %350 = fmul fast float %347, %29
  %351 = fmul fast float %348, %29
  %352 = fadd fast float %342, %349
  %353 = fadd fast float %343, %350
  %354 = fadd fast float %344, %351
  %355 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %160)  ; CBufferLoadLegacy(handle,regIndex)
  %356 = extractvalue %dx.types.CBufRet.f32 %355, 0
  %357 = extractvalue %dx.types.CBufRet.f32 %355, 1
  %358 = extractvalue %dx.types.CBufRet.f32 %355, 2
  %359 = fmul fast float %356, %30
  %360 = fmul fast float %357, %30
  %361 = fmul fast float %358, %30
  %362 = fadd fast float %352, %359
  %363 = fadd fast float %353, %360
  %364 = fadd fast float %354, %361
  %365 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %171)  ; CBufferLoadLegacy(handle,regIndex)
  %366 = extractvalue %dx.types.CBufRet.f32 %365, 0
  %367 = extractvalue %dx.types.CBufRet.f32 %365, 1
  %368 = extractvalue %dx.types.CBufRet.f32 %365, 2
  %369 = fmul fast float %366, %27
  %370 = fmul fast float %367, %27
  %371 = fmul fast float %368, %27
  %372 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %179)  ; CBufferLoadLegacy(handle,regIndex)
  %373 = extractvalue %dx.types.CBufRet.f32 %372, 0
  %374 = extractvalue %dx.types.CBufRet.f32 %372, 1
  %375 = extractvalue %dx.types.CBufRet.f32 %372, 2
  %376 = fmul fast float %373, %28
  %377 = fmul fast float %374, %28
  %378 = fmul fast float %375, %28
  %379 = fadd fast float %376, %369
  %380 = fadd fast float %377, %370
  %381 = fadd fast float %378, %371
  %382 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %190)  ; CBufferLoadLegacy(handle,regIndex)
  %383 = extractvalue %dx.types.CBufRet.f32 %382, 0
  %384 = extractvalue %dx.types.CBufRet.f32 %382, 1
  %385 = extractvalue %dx.types.CBufRet.f32 %382, 2
  %386 = fmul fast float %383, %29
  %387 = fmul fast float %384, %29
  %388 = fmul fast float %385, %29
  %389 = fadd fast float %379, %386
  %390 = fadd fast float %380, %387
  %391 = fadd fast float %381, %388
  %392 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %201)  ; CBufferLoadLegacy(handle,regIndex)
  %393 = extractvalue %dx.types.CBufRet.f32 %392, 0
  %394 = extractvalue %dx.types.CBufRet.f32 %392, 1
  %395 = extractvalue %dx.types.CBufRet.f32 %392, 2
  %396 = fmul fast float %393, %30
  %397 = fmul fast float %394, %30
  %398 = fmul fast float %395, %30
  %399 = fadd fast float %389, %396
  %400 = fadd fast float %390, %397
  %401 = fadd fast float %391, %398
  %402 = fmul fast float %288, %34
  %403 = fmul fast float %289, %34
  %404 = fmul fast float %290, %34
  %405 = fmul fast float %325, %35
  %406 = fmul fast float %326, %35
  %407 = fmul fast float %327, %35
  %408 = fadd fast float %405, %402
  %409 = fadd fast float %406, %403
  %410 = fadd fast float %407, %404
  %411 = fmul fast float %362, %36
  %412 = fmul fast float %363, %36
  %413 = fmul fast float %364, %36
  %414 = fadd fast float %408, %411
  %415 = fadd fast float %409, %412
  %416 = fadd fast float %410, %413
  %417 = fmul fast float %399, %37
  %418 = fmul fast float %400, %37
  %419 = fmul fast float %401, %37
  %420 = fadd fast float %414, %417
  %421 = fadd fast float %415, %418
  %422 = fadd fast float %416, %419
  %423 = fmul fast float %422, %252
  %424 = fmul fast float %421, %253
  %425 = fsub fast float %423, %424
  %426 = fmul fast float %420, %253
  %427 = fmul fast float %422, %251
  %428 = fsub fast float %426, %427
  %429 = fmul fast float %421, %251
  %430 = fmul fast float %420, %252
  %431 = fsub fast float %429, %430
  %432 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 391)  ; CBufferLoadLegacy(handle,regIndex)
  %433 = extractvalue %dx.types.CBufRet.f32 %432, 3
  %434 = fcmp fast olt float %433, 0.000000e+00
  br i1 %434, label %435, label %441

; <label>:435                                     ; preds = %0
  %436 = call float @dx.op.dot3.f32(i32 55, float %425, float %428, float %431, float %425, float %428, float %431)  ; Dot3(ax,ay,az,bx,by,bz)
  %437 = call float @dx.op.unary.f32(i32 25, float %436)  ; Rsqrt(value)
  %438 = fmul fast float %437, %425
  %439 = fmul fast float %437, %428
  %440 = fmul fast float %437, %431
  br label %441

; <label>:441                                     ; preds = %435, %0
  %442 = phi float [ %438, %435 ], [ %425, %0 ]
  %443 = phi float [ %439, %435 ], [ %428, %0 ]
  %444 = phi float [ %440, %435 ], [ %431, %0 ]
  br i1 %47, label %445, label %452

; <label>:445                                     ; preds = %441
  %446 = fmul fast float %444, %2
  %447 = fadd fast float %232, %446
  %448 = fmul fast float %443, %2
  %449 = fadd fast float %231, %448
  %450 = fmul fast float %442, %2
  %451 = fadd fast float %230, %450
  br label %452

; <label>:452                                     ; preds = %445, %441
  %453 = phi i32 [ 0, %445 ], [ %3, %441 ]
  %454 = phi i32 [ 0, %445 ], [ %4, %441 ]
  %455 = phi i32 [ 0, %445 ], [ %5, %441 ]
  %456 = phi i32 [ 0, %445 ], [ %6, %441 ]
  %457 = phi i32 [ 0, %445 ], [ %19, %441 ]
  %458 = phi i32 [ 0, %445 ], [ %20, %441 ]
  %459 = phi i32 [ 0, %445 ], [ %21, %441 ]
  %460 = phi i32 [ 0, %445 ], [ %22, %441 ]
  %461 = phi float [ 0.000000e+00, %445 ], [ %31, %441 ]
  %462 = phi float [ 0.000000e+00, %445 ], [ %32, %441 ]
  %463 = phi float [ 0.000000e+00, %445 ], [ %33, %441 ]
  %464 = phi float [ 1.000000e+00, %445 ], [ %34, %441 ]
  %465 = phi float [ 0.000000e+00, %445 ], [ %35, %441 ]
  %466 = phi float [ 0.000000e+00, %445 ], [ %36, %441 ]
  %467 = phi float [ 0.000000e+00, %445 ], [ %37, %441 ]
  %468 = phi float [ %451, %445 ], [ %38, %441 ]
  %469 = phi float [ %449, %445 ], [ %39, %441 ]
  %470 = phi float [ %447, %445 ], [ %40, %441 ]
  %471 = phi float [ 1.000000e+00, %445 ], [ %41, %441 ]
  %472 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 390)  ; CBufferLoadLegacy(handle,regIndex)
  %473 = extractvalue %dx.types.CBufRet.i32 %472, 0
  %474 = icmp eq i32 %473, 0
  %475 = add i32 %457, 4
  %476 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %475)  ; CBufferLoadLegacy(handle,regIndex)
  %477 = extractvalue %dx.types.CBufRet.f32 %476, 0
  %478 = extractvalue %dx.types.CBufRet.f32 %476, 1
  %479 = extractvalue %dx.types.CBufRet.f32 %476, 2
  %480 = extractvalue %dx.types.CBufRet.f32 %476, 3
  %481 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %477, float %478, float %479, float %480)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %482 = add i32 %457, 5
  %483 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %482)  ; CBufferLoadLegacy(handle,regIndex)
  %484 = extractvalue %dx.types.CBufRet.f32 %483, 0
  %485 = extractvalue %dx.types.CBufRet.f32 %483, 1
  %486 = extractvalue %dx.types.CBufRet.f32 %483, 2
  %487 = extractvalue %dx.types.CBufRet.f32 %483, 3
  %488 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %484, float %485, float %486, float %487)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %489 = add i32 %457, 6
  %490 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %489)  ; CBufferLoadLegacy(handle,regIndex)
  %491 = extractvalue %dx.types.CBufRet.f32 %490, 0
  %492 = extractvalue %dx.types.CBufRet.f32 %490, 1
  %493 = extractvalue %dx.types.CBufRet.f32 %490, 2
  %494 = extractvalue %dx.types.CBufRet.f32 %490, 3
  %495 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %491, float %492, float %493, float %494)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %496 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 388)  ; CBufferLoadLegacy(handle,regIndex)
  %497 = extractvalue %dx.types.CBufRet.i32 %496, 0
  %498 = icmp eq i32 %497, 0
  %499 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 389)  ; CBufferLoadLegacy(handle,regIndex)
  %500 = extractvalue %dx.types.CBufRet.i32 %499, 0
  %501 = icmp eq i32 %500, 0
  br i1 %474, label %754, label %502

; <label>:502                                     ; preds = %452
  %503 = fmul fast float %481, %464
  %504 = fmul fast float %488, %464
  %505 = fmul fast float %495, %464
  %506 = add i32 %458, 4
  %507 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %506)  ; CBufferLoadLegacy(handle,regIndex)
  %508 = extractvalue %dx.types.CBufRet.f32 %507, 0
  %509 = extractvalue %dx.types.CBufRet.f32 %507, 1
  %510 = extractvalue %dx.types.CBufRet.f32 %507, 2
  %511 = extractvalue %dx.types.CBufRet.f32 %507, 3
  %512 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %508, float %509, float %510, float %511)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %513 = add i32 %458, 5
  %514 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %513)  ; CBufferLoadLegacy(handle,regIndex)
  %515 = extractvalue %dx.types.CBufRet.f32 %514, 0
  %516 = extractvalue %dx.types.CBufRet.f32 %514, 1
  %517 = extractvalue %dx.types.CBufRet.f32 %514, 2
  %518 = extractvalue %dx.types.CBufRet.f32 %514, 3
  %519 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %515, float %516, float %517, float %518)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %520 = add i32 %458, 6
  %521 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %520)  ; CBufferLoadLegacy(handle,regIndex)
  %522 = extractvalue %dx.types.CBufRet.f32 %521, 0
  %523 = extractvalue %dx.types.CBufRet.f32 %521, 1
  %524 = extractvalue %dx.types.CBufRet.f32 %521, 2
  %525 = extractvalue %dx.types.CBufRet.f32 %521, 3
  %526 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %522, float %523, float %524, float %525)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %527 = fmul fast float %512, %465
  %528 = fmul fast float %519, %465
  %529 = fmul fast float %526, %465
  %530 = fadd fast float %527, %503
  %531 = fadd fast float %528, %504
  %532 = fadd fast float %529, %505
  %533 = add i32 %459, 4
  %534 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %533)  ; CBufferLoadLegacy(handle,regIndex)
  %535 = extractvalue %dx.types.CBufRet.f32 %534, 0
  %536 = extractvalue %dx.types.CBufRet.f32 %534, 1
  %537 = extractvalue %dx.types.CBufRet.f32 %534, 2
  %538 = extractvalue %dx.types.CBufRet.f32 %534, 3
  %539 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %535, float %536, float %537, float %538)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %540 = add i32 %459, 5
  %541 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %540)  ; CBufferLoadLegacy(handle,regIndex)
  %542 = extractvalue %dx.types.CBufRet.f32 %541, 0
  %543 = extractvalue %dx.types.CBufRet.f32 %541, 1
  %544 = extractvalue %dx.types.CBufRet.f32 %541, 2
  %545 = extractvalue %dx.types.CBufRet.f32 %541, 3
  %546 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %542, float %543, float %544, float %545)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %547 = add i32 %459, 6
  %548 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %547)  ; CBufferLoadLegacy(handle,regIndex)
  %549 = extractvalue %dx.types.CBufRet.f32 %548, 0
  %550 = extractvalue %dx.types.CBufRet.f32 %548, 1
  %551 = extractvalue %dx.types.CBufRet.f32 %548, 2
  %552 = extractvalue %dx.types.CBufRet.f32 %548, 3
  %553 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %549, float %550, float %551, float %552)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %554 = fmul fast float %539, %466
  %555 = fmul fast float %546, %466
  %556 = fmul fast float %553, %466
  %557 = fadd fast float %530, %554
  %558 = fadd fast float %531, %555
  %559 = fadd fast float %532, %556
  %560 = add i32 %460, 4
  %561 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %560)  ; CBufferLoadLegacy(handle,regIndex)
  %562 = extractvalue %dx.types.CBufRet.f32 %561, 0
  %563 = extractvalue %dx.types.CBufRet.f32 %561, 1
  %564 = extractvalue %dx.types.CBufRet.f32 %561, 2
  %565 = extractvalue %dx.types.CBufRet.f32 %561, 3
  %566 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %562, float %563, float %564, float %565)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %567 = add i32 %460, 5
  %568 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %567)  ; CBufferLoadLegacy(handle,regIndex)
  %569 = extractvalue %dx.types.CBufRet.f32 %568, 0
  %570 = extractvalue %dx.types.CBufRet.f32 %568, 1
  %571 = extractvalue %dx.types.CBufRet.f32 %568, 2
  %572 = extractvalue %dx.types.CBufRet.f32 %568, 3
  %573 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %569, float %570, float %571, float %572)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %574 = add i32 %460, 6
  %575 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %574)  ; CBufferLoadLegacy(handle,regIndex)
  %576 = extractvalue %dx.types.CBufRet.f32 %575, 0
  %577 = extractvalue %dx.types.CBufRet.f32 %575, 1
  %578 = extractvalue %dx.types.CBufRet.f32 %575, 2
  %579 = extractvalue %dx.types.CBufRet.f32 %575, 3
  %580 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %576, float %577, float %578, float %579)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %581 = fmul fast float %566, %467
  %582 = fmul fast float %573, %467
  %583 = fmul fast float %580, %467
  %584 = fadd fast float %557, %581
  %585 = fadd fast float %558, %582
  %586 = fadd fast float %559, %583
  %587 = call float @dx.op.dot4.f32(i32 56, float %464, float %465, float %466, float %467, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %588 = add i32 %453, 4
  %589 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %588)  ; CBufferLoadLegacy(handle,regIndex)
  %590 = extractvalue %dx.types.CBufRet.f32 %589, 0
  %591 = extractvalue %dx.types.CBufRet.f32 %589, 1
  %592 = extractvalue %dx.types.CBufRet.f32 %589, 2
  %593 = extractvalue %dx.types.CBufRet.f32 %589, 3
  %594 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %590, float %591, float %592, float %593)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %595 = add i32 %453, 5
  %596 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %595)  ; CBufferLoadLegacy(handle,regIndex)
  %597 = extractvalue %dx.types.CBufRet.f32 %596, 0
  %598 = extractvalue %dx.types.CBufRet.f32 %596, 1
  %599 = extractvalue %dx.types.CBufRet.f32 %596, 2
  %600 = extractvalue %dx.types.CBufRet.f32 %596, 3
  %601 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %597, float %598, float %599, float %600)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %602 = add i32 %453, 6
  %603 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %602)  ; CBufferLoadLegacy(handle,regIndex)
  %604 = extractvalue %dx.types.CBufRet.f32 %603, 0
  %605 = extractvalue %dx.types.CBufRet.f32 %603, 1
  %606 = extractvalue %dx.types.CBufRet.f32 %603, 2
  %607 = extractvalue %dx.types.CBufRet.f32 %603, 3
  %608 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %604, float %605, float %606, float %607)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  br i1 %498, label %710, label %609

; <label>:609                                     ; preds = %502
  %610 = fmul fast float %594, %461
  %611 = fmul fast float %601, %461
  %612 = fmul fast float %608, %461
  %613 = fadd fast float %610, %584
  %614 = fadd fast float %611, %585
  %615 = fadd fast float %612, %586
  %616 = add i32 %454, 4
  %617 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %616)  ; CBufferLoadLegacy(handle,regIndex)
  %618 = extractvalue %dx.types.CBufRet.f32 %617, 0
  %619 = extractvalue %dx.types.CBufRet.f32 %617, 1
  %620 = extractvalue %dx.types.CBufRet.f32 %617, 2
  %621 = extractvalue %dx.types.CBufRet.f32 %617, 3
  %622 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %618, float %619, float %620, float %621)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %623 = add i32 %454, 5
  %624 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %623)  ; CBufferLoadLegacy(handle,regIndex)
  %625 = extractvalue %dx.types.CBufRet.f32 %624, 0
  %626 = extractvalue %dx.types.CBufRet.f32 %624, 1
  %627 = extractvalue %dx.types.CBufRet.f32 %624, 2
  %628 = extractvalue %dx.types.CBufRet.f32 %624, 3
  %629 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %625, float %626, float %627, float %628)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %630 = add i32 %454, 6
  %631 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %630)  ; CBufferLoadLegacy(handle,regIndex)
  %632 = extractvalue %dx.types.CBufRet.f32 %631, 0
  %633 = extractvalue %dx.types.CBufRet.f32 %631, 1
  %634 = extractvalue %dx.types.CBufRet.f32 %631, 2
  %635 = extractvalue %dx.types.CBufRet.f32 %631, 3
  %636 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %632, float %633, float %634, float %635)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %637 = fmul fast float %622, %462
  %638 = fmul fast float %629, %462
  %639 = fmul fast float %636, %462
  %640 = fadd fast float %637, %613
  %641 = fadd fast float %638, %614
  %642 = fadd fast float %639, %615
  %643 = add i32 %455, 4
  %644 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %643)  ; CBufferLoadLegacy(handle,regIndex)
  %645 = extractvalue %dx.types.CBufRet.f32 %644, 0
  %646 = extractvalue %dx.types.CBufRet.f32 %644, 1
  %647 = extractvalue %dx.types.CBufRet.f32 %644, 2
  %648 = extractvalue %dx.types.CBufRet.f32 %644, 3
  %649 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %645, float %646, float %647, float %648)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %650 = add i32 %455, 5
  %651 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %650)  ; CBufferLoadLegacy(handle,regIndex)
  %652 = extractvalue %dx.types.CBufRet.f32 %651, 0
  %653 = extractvalue %dx.types.CBufRet.f32 %651, 1
  %654 = extractvalue %dx.types.CBufRet.f32 %651, 2
  %655 = extractvalue %dx.types.CBufRet.f32 %651, 3
  %656 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %652, float %653, float %654, float %655)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %657 = add i32 %455, 6
  %658 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %657)  ; CBufferLoadLegacy(handle,regIndex)
  %659 = extractvalue %dx.types.CBufRet.f32 %658, 0
  %660 = extractvalue %dx.types.CBufRet.f32 %658, 1
  %661 = extractvalue %dx.types.CBufRet.f32 %658, 2
  %662 = extractvalue %dx.types.CBufRet.f32 %658, 3
  %663 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %659, float %660, float %661, float %662)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %664 = fsub fast float 1.000000e+00, %461
  %665 = fsub fast float %664, %462
  br i1 %501, label %702, label %666

; <label>:666                                     ; preds = %609
  %667 = fmul fast float %649, %463
  %668 = fmul fast float %656, %463
  %669 = fmul fast float %663, %463
  %670 = fadd fast float %640, %667
  %671 = fadd fast float %641, %668
  %672 = fadd fast float %642, %669
  %673 = fsub fast float %665, %463
  %674 = fsub fast float %673, %587
  %675 = add i32 %456, 4
  %676 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %675)  ; CBufferLoadLegacy(handle,regIndex)
  %677 = extractvalue %dx.types.CBufRet.f32 %676, 0
  %678 = extractvalue %dx.types.CBufRet.f32 %676, 1
  %679 = extractvalue %dx.types.CBufRet.f32 %676, 2
  %680 = extractvalue %dx.types.CBufRet.f32 %676, 3
  %681 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %677, float %678, float %679, float %680)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %682 = add i32 %456, 5
  %683 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %682)  ; CBufferLoadLegacy(handle,regIndex)
  %684 = extractvalue %dx.types.CBufRet.f32 %683, 0
  %685 = extractvalue %dx.types.CBufRet.f32 %683, 1
  %686 = extractvalue %dx.types.CBufRet.f32 %683, 2
  %687 = extractvalue %dx.types.CBufRet.f32 %683, 3
  %688 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %684, float %685, float %686, float %687)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %689 = add i32 %456, 6
  %690 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %689)  ; CBufferLoadLegacy(handle,regIndex)
  %691 = extractvalue %dx.types.CBufRet.f32 %690, 0
  %692 = extractvalue %dx.types.CBufRet.f32 %690, 1
  %693 = extractvalue %dx.types.CBufRet.f32 %690, 2
  %694 = extractvalue %dx.types.CBufRet.f32 %690, 3
  %695 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %691, float %692, float %693, float %694)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %696 = fmul fast float %681, %674
  %697 = fmul fast float %688, %674
  %698 = fmul fast float %695, %674
  %699 = fadd fast float %670, %696
  %700 = fadd fast float %671, %697
  %701 = fadd fast float %672, %698
  br label %884

; <label>:702                                     ; preds = %609
  %703 = fsub fast float %665, %587
  %704 = fmul fast float %649, %703
  %705 = fmul fast float %656, %703
  %706 = fmul fast float %663, %703
  %707 = fadd fast float %640, %704
  %708 = fadd fast float %641, %705
  %709 = fadd fast float %642, %706
  br label %884

; <label>:710                                     ; preds = %502
  %711 = fsub fast float 1.000000e+00, %587
  br i1 %501, label %747, label %712

; <label>:712                                     ; preds = %710
  %713 = fmul fast float %594, %461
  %714 = fmul fast float %601, %461
  %715 = fmul fast float %608, %461
  %716 = fadd fast float %713, %584
  %717 = fadd fast float %714, %585
  %718 = fadd fast float %715, %586
  %719 = fsub fast float %711, %461
  %720 = add i32 %454, 4
  %721 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %720)  ; CBufferLoadLegacy(handle,regIndex)
  %722 = extractvalue %dx.types.CBufRet.f32 %721, 0
  %723 = extractvalue %dx.types.CBufRet.f32 %721, 1
  %724 = extractvalue %dx.types.CBufRet.f32 %721, 2
  %725 = extractvalue %dx.types.CBufRet.f32 %721, 3
  %726 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %722, float %723, float %724, float %725)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %727 = add i32 %454, 5
  %728 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %727)  ; CBufferLoadLegacy(handle,regIndex)
  %729 = extractvalue %dx.types.CBufRet.f32 %728, 0
  %730 = extractvalue %dx.types.CBufRet.f32 %728, 1
  %731 = extractvalue %dx.types.CBufRet.f32 %728, 2
  %732 = extractvalue %dx.types.CBufRet.f32 %728, 3
  %733 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %729, float %730, float %731, float %732)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %734 = add i32 %454, 6
  %735 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %734)  ; CBufferLoadLegacy(handle,regIndex)
  %736 = extractvalue %dx.types.CBufRet.f32 %735, 0
  %737 = extractvalue %dx.types.CBufRet.f32 %735, 1
  %738 = extractvalue %dx.types.CBufRet.f32 %735, 2
  %739 = extractvalue %dx.types.CBufRet.f32 %735, 3
  %740 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %736, float %737, float %738, float %739)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %741 = fmul fast float %726, %719
  %742 = fmul fast float %733, %719
  %743 = fmul fast float %740, %719
  %744 = fadd fast float %716, %741
  %745 = fadd fast float %717, %742
  %746 = fadd fast float %718, %743
  br label %884

; <label>:747                                     ; preds = %710
  %748 = fmul fast float %594, %711
  %749 = fmul fast float %601, %711
  %750 = fmul fast float %608, %711
  %751 = fadd fast float %748, %584
  %752 = fadd fast float %749, %585
  %753 = fadd fast float %750, %586
  br label %884

; <label>:754                                     ; preds = %452
  br i1 %498, label %851, label %755

; <label>:755                                     ; preds = %754
  %756 = fmul fast float %481, %464
  %757 = fmul fast float %488, %464
  %758 = fmul fast float %495, %464
  %759 = add i32 %458, 4
  %760 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %759)  ; CBufferLoadLegacy(handle,regIndex)
  %761 = extractvalue %dx.types.CBufRet.f32 %760, 0
  %762 = extractvalue %dx.types.CBufRet.f32 %760, 1
  %763 = extractvalue %dx.types.CBufRet.f32 %760, 2
  %764 = extractvalue %dx.types.CBufRet.f32 %760, 3
  %765 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %761, float %762, float %763, float %764)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %766 = add i32 %458, 5
  %767 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %766)  ; CBufferLoadLegacy(handle,regIndex)
  %768 = extractvalue %dx.types.CBufRet.f32 %767, 0
  %769 = extractvalue %dx.types.CBufRet.f32 %767, 1
  %770 = extractvalue %dx.types.CBufRet.f32 %767, 2
  %771 = extractvalue %dx.types.CBufRet.f32 %767, 3
  %772 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %768, float %769, float %770, float %771)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %773 = add i32 %458, 6
  %774 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %773)  ; CBufferLoadLegacy(handle,regIndex)
  %775 = extractvalue %dx.types.CBufRet.f32 %774, 0
  %776 = extractvalue %dx.types.CBufRet.f32 %774, 1
  %777 = extractvalue %dx.types.CBufRet.f32 %774, 2
  %778 = extractvalue %dx.types.CBufRet.f32 %774, 3
  %779 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %775, float %776, float %777, float %778)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %780 = fmul fast float %765, %465
  %781 = fmul fast float %772, %465
  %782 = fmul fast float %779, %465
  %783 = fadd fast float %780, %756
  %784 = fadd fast float %781, %757
  %785 = fadd fast float %782, %758
  %786 = add i32 %459, 4
  %787 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %786)  ; CBufferLoadLegacy(handle,regIndex)
  %788 = extractvalue %dx.types.CBufRet.f32 %787, 0
  %789 = extractvalue %dx.types.CBufRet.f32 %787, 1
  %790 = extractvalue %dx.types.CBufRet.f32 %787, 2
  %791 = extractvalue %dx.types.CBufRet.f32 %787, 3
  %792 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %788, float %789, float %790, float %791)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %793 = add i32 %459, 5
  %794 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %793)  ; CBufferLoadLegacy(handle,regIndex)
  %795 = extractvalue %dx.types.CBufRet.f32 %794, 0
  %796 = extractvalue %dx.types.CBufRet.f32 %794, 1
  %797 = extractvalue %dx.types.CBufRet.f32 %794, 2
  %798 = extractvalue %dx.types.CBufRet.f32 %794, 3
  %799 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %795, float %796, float %797, float %798)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %800 = add i32 %459, 6
  %801 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %800)  ; CBufferLoadLegacy(handle,regIndex)
  %802 = extractvalue %dx.types.CBufRet.f32 %801, 0
  %803 = extractvalue %dx.types.CBufRet.f32 %801, 1
  %804 = extractvalue %dx.types.CBufRet.f32 %801, 2
  %805 = extractvalue %dx.types.CBufRet.f32 %801, 3
  %806 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %802, float %803, float %804, float %805)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %807 = fsub fast float 1.000000e+00, %464
  %808 = fsub fast float %807, %465
  br i1 %501, label %844, label %809

; <label>:809                                     ; preds = %755
  %810 = fmul fast float %792, %466
  %811 = fmul fast float %799, %466
  %812 = fmul fast float %806, %466
  %813 = fadd fast float %783, %810
  %814 = fadd fast float %784, %811
  %815 = fadd fast float %785, %812
  %816 = fsub fast float %808, %466
  %817 = add i32 %460, 4
  %818 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %817)  ; CBufferLoadLegacy(handle,regIndex)
  %819 = extractvalue %dx.types.CBufRet.f32 %818, 0
  %820 = extractvalue %dx.types.CBufRet.f32 %818, 1
  %821 = extractvalue %dx.types.CBufRet.f32 %818, 2
  %822 = extractvalue %dx.types.CBufRet.f32 %818, 3
  %823 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %819, float %820, float %821, float %822)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %824 = add i32 %460, 5
  %825 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %824)  ; CBufferLoadLegacy(handle,regIndex)
  %826 = extractvalue %dx.types.CBufRet.f32 %825, 0
  %827 = extractvalue %dx.types.CBufRet.f32 %825, 1
  %828 = extractvalue %dx.types.CBufRet.f32 %825, 2
  %829 = extractvalue %dx.types.CBufRet.f32 %825, 3
  %830 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %826, float %827, float %828, float %829)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %831 = add i32 %460, 6
  %832 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %831)  ; CBufferLoadLegacy(handle,regIndex)
  %833 = extractvalue %dx.types.CBufRet.f32 %832, 0
  %834 = extractvalue %dx.types.CBufRet.f32 %832, 1
  %835 = extractvalue %dx.types.CBufRet.f32 %832, 2
  %836 = extractvalue %dx.types.CBufRet.f32 %832, 3
  %837 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %833, float %834, float %835, float %836)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %838 = fmul fast float %823, %816
  %839 = fmul fast float %830, %816
  %840 = fmul fast float %837, %816
  %841 = fadd fast float %813, %838
  %842 = fadd fast float %814, %839
  %843 = fadd fast float %815, %840
  br label %884

; <label>:844                                     ; preds = %755
  %845 = fmul fast float %792, %808
  %846 = fmul fast float %799, %808
  %847 = fmul fast float %806, %808
  %848 = fadd fast float %783, %845
  %849 = fadd fast float %784, %846
  %850 = fadd fast float %785, %847
  br label %884

; <label>:851                                     ; preds = %754
  br i1 %501, label %884, label %852

; <label>:852                                     ; preds = %851
  %853 = fmul fast float %481, %464
  %854 = fmul fast float %488, %464
  %855 = fmul fast float %495, %464
  %856 = fsub fast float 1.000000e+00, %464
  %857 = add i32 %458, 4
  %858 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %857)  ; CBufferLoadLegacy(handle,regIndex)
  %859 = extractvalue %dx.types.CBufRet.f32 %858, 0
  %860 = extractvalue %dx.types.CBufRet.f32 %858, 1
  %861 = extractvalue %dx.types.CBufRet.f32 %858, 2
  %862 = extractvalue %dx.types.CBufRet.f32 %858, 3
  %863 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %859, float %860, float %861, float %862)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %864 = add i32 %458, 5
  %865 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %864)  ; CBufferLoadLegacy(handle,regIndex)
  %866 = extractvalue %dx.types.CBufRet.f32 %865, 0
  %867 = extractvalue %dx.types.CBufRet.f32 %865, 1
  %868 = extractvalue %dx.types.CBufRet.f32 %865, 2
  %869 = extractvalue %dx.types.CBufRet.f32 %865, 3
  %870 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %866, float %867, float %868, float %869)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %871 = add i32 %458, 6
  %872 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %871)  ; CBufferLoadLegacy(handle,regIndex)
  %873 = extractvalue %dx.types.CBufRet.f32 %872, 0
  %874 = extractvalue %dx.types.CBufRet.f32 %872, 1
  %875 = extractvalue %dx.types.CBufRet.f32 %872, 2
  %876 = extractvalue %dx.types.CBufRet.f32 %872, 3
  %877 = call float @dx.op.dot4.f32(i32 56, float %468, float %469, float %470, float %471, float %873, float %874, float %875, float %876)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %878 = fmul fast float %863, %856
  %879 = fmul fast float %870, %856
  %880 = fmul fast float %877, %856
  %881 = fadd fast float %878, %853
  %882 = fadd fast float %879, %854
  %883 = fadd fast float %880, %855
  br label %884

; <label>:884                                     ; preds = %852, %851, %844, %809, %747, %712, %702, %666
  %885 = phi float [ %699, %666 ], [ %707, %702 ], [ %744, %712 ], [ %751, %747 ], [ %841, %809 ], [ %848, %844 ], [ %881, %852 ], [ %481, %851 ]
  %886 = phi float [ %700, %666 ], [ %708, %702 ], [ %745, %712 ], [ %752, %747 ], [ %842, %809 ], [ %849, %844 ], [ %882, %852 ], [ %488, %851 ]
  %887 = phi float [ %701, %666 ], [ %709, %702 ], [ %746, %712 ], [ %753, %747 ], [ %843, %809 ], [ %850, %844 ], [ %883, %852 ], [ %495, %851 ]
  %888 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %889 = extractvalue %dx.types.CBufRet.f32 %888, 0
  %890 = extractvalue %dx.types.CBufRet.f32 %888, 1
  %891 = extractvalue %dx.types.CBufRet.f32 %888, 2
  %892 = extractvalue %dx.types.CBufRet.f32 %888, 3
  %893 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %894 = extractvalue %dx.types.CBufRet.f32 %893, 0
  %895 = extractvalue %dx.types.CBufRet.f32 %893, 1
  %896 = extractvalue %dx.types.CBufRet.f32 %893, 2
  %897 = extractvalue %dx.types.CBufRet.f32 %893, 3
  %898 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %899 = extractvalue %dx.types.CBufRet.f32 %898, 0
  %900 = extractvalue %dx.types.CBufRet.f32 %898, 1
  %901 = extractvalue %dx.types.CBufRet.f32 %898, 2
  %902 = extractvalue %dx.types.CBufRet.f32 %898, 3
  %903 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %904 = extractvalue %dx.types.CBufRet.f32 %903, 0
  %905 = extractvalue %dx.types.CBufRet.f32 %903, 1
  %906 = extractvalue %dx.types.CBufRet.f32 %903, 2
  %907 = extractvalue %dx.types.CBufRet.f32 %903, 3
  %908 = fmul fast float %889, %885
  %909 = call float @dx.op.tertiary.f32(i32 46, float %886, float %894, float %908)  ; FMad(a,b,c)
  %910 = call float @dx.op.tertiary.f32(i32 46, float %887, float %899, float %909)  ; FMad(a,b,c)
  %911 = fadd fast float %910, %904
  %912 = fmul fast float %890, %885
  %913 = call float @dx.op.tertiary.f32(i32 46, float %886, float %895, float %912)  ; FMad(a,b,c)
  %914 = call float @dx.op.tertiary.f32(i32 46, float %887, float %900, float %913)  ; FMad(a,b,c)
  %915 = fadd fast float %914, %905
  %916 = fmul fast float %891, %885
  %917 = call float @dx.op.tertiary.f32(i32 46, float %886, float %896, float %916)  ; FMad(a,b,c)
  %918 = call float @dx.op.tertiary.f32(i32 46, float %887, float %901, float %917)  ; FMad(a,b,c)
  %919 = fadd fast float %918, %906
  %920 = fmul fast float %892, %885
  %921 = call float @dx.op.tertiary.f32(i32 46, float %886, float %897, float %920)  ; FMad(a,b,c)
  %922 = call float @dx.op.tertiary.f32(i32 46, float %887, float %902, float %921)  ; FMad(a,b,c)
  %923 = fadd fast float %922, %907
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %911)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %915)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %919)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %923)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
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
!6 = !{[46 x i32] [i32 44, i32 4, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 0, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 0, i32 0, i32 0, i32 15]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !27, null}
!9 = !{!10, !13, !14, !17, !18, !19, !20, !21, !22, !24, !25}
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
!27 = !{!28}
!28 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}

