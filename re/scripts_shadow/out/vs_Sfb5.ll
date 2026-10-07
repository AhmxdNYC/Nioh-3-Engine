;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; BLENDWEIGHT              0   xyzw        1     NONE   float   xyzw
; BLENDWEIGHT              1   xyzw        2     NONE   float   xyz 
; BLENDINDICES             0   xyzw        3     NONE     int   xyzw
; BLENDINDICES             1   xyzw        4     NONE     int   xyzw
; TEXCOORD                 0   xy          5     NONE   float   xy  
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; TEXCOORD                 0   xy          1     NONE   float   xy  
;
; shader hash: 77312b5636f218990dc9b443cd87697a
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 6
; SigOutputElements: 2
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 6
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
; BLENDINDICES             0                              
; BLENDINDICES             1                              
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
;   [6244 x i8] (type annotation not present)
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
; Number of inputs: 22, outputs: 6
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 1 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 2 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 3 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 4 depends on inputs: { 20 }
;   output 5 depends on inputs: { 21 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [384 x <4 x float>], [3 x i32] }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 5, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 5, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %19 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %20 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %21 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %22 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %23 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 390)  ; CBufferLoadLegacy(handle,regIndex)
  %24 = extractvalue %dx.types.CBufRet.i32 %23, 0
  %25 = icmp eq i32 %24, 0
  %26 = add i32 %8, 4
  %27 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %26)  ; CBufferLoadLegacy(handle,regIndex)
  %28 = extractvalue %dx.types.CBufRet.f32 %27, 0
  %29 = extractvalue %dx.types.CBufRet.f32 %27, 1
  %30 = extractvalue %dx.types.CBufRet.f32 %27, 2
  %31 = extractvalue %dx.types.CBufRet.f32 %27, 3
  %32 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %28, float %29, float %30, float %31)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %33 = add i32 %8, 5
  %34 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %33)  ; CBufferLoadLegacy(handle,regIndex)
  %35 = extractvalue %dx.types.CBufRet.f32 %34, 0
  %36 = extractvalue %dx.types.CBufRet.f32 %34, 1
  %37 = extractvalue %dx.types.CBufRet.f32 %34, 2
  %38 = extractvalue %dx.types.CBufRet.f32 %34, 3
  %39 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %35, float %36, float %37, float %38)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %40 = add i32 %8, 6
  %41 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %40)  ; CBufferLoadLegacy(handle,regIndex)
  %42 = extractvalue %dx.types.CBufRet.f32 %41, 0
  %43 = extractvalue %dx.types.CBufRet.f32 %41, 1
  %44 = extractvalue %dx.types.CBufRet.f32 %41, 2
  %45 = extractvalue %dx.types.CBufRet.f32 %41, 3
  %46 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %42, float %43, float %44, float %45)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %47 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 388)  ; CBufferLoadLegacy(handle,regIndex)
  %48 = extractvalue %dx.types.CBufRet.i32 %47, 0
  %49 = icmp eq i32 %48, 0
  %50 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 389)  ; CBufferLoadLegacy(handle,regIndex)
  %51 = extractvalue %dx.types.CBufRet.i32 %50, 0
  %52 = icmp eq i32 %51, 0
  br i1 %25, label %305, label %53

; <label>:53                                      ; preds = %0
  %54 = fmul fast float %32, %15
  %55 = fmul fast float %39, %15
  %56 = fmul fast float %46, %15
  %57 = add i32 %9, 4
  %58 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %57)  ; CBufferLoadLegacy(handle,regIndex)
  %59 = extractvalue %dx.types.CBufRet.f32 %58, 0
  %60 = extractvalue %dx.types.CBufRet.f32 %58, 1
  %61 = extractvalue %dx.types.CBufRet.f32 %58, 2
  %62 = extractvalue %dx.types.CBufRet.f32 %58, 3
  %63 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %59, float %60, float %61, float %62)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %64 = add i32 %9, 5
  %65 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %64)  ; CBufferLoadLegacy(handle,regIndex)
  %66 = extractvalue %dx.types.CBufRet.f32 %65, 0
  %67 = extractvalue %dx.types.CBufRet.f32 %65, 1
  %68 = extractvalue %dx.types.CBufRet.f32 %65, 2
  %69 = extractvalue %dx.types.CBufRet.f32 %65, 3
  %70 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %66, float %67, float %68, float %69)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %71 = add i32 %9, 6
  %72 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %71)  ; CBufferLoadLegacy(handle,regIndex)
  %73 = extractvalue %dx.types.CBufRet.f32 %72, 0
  %74 = extractvalue %dx.types.CBufRet.f32 %72, 1
  %75 = extractvalue %dx.types.CBufRet.f32 %72, 2
  %76 = extractvalue %dx.types.CBufRet.f32 %72, 3
  %77 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %73, float %74, float %75, float %76)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %78 = fmul fast float %63, %16
  %79 = fmul fast float %70, %16
  %80 = fmul fast float %77, %16
  %81 = fadd fast float %78, %54
  %82 = fadd fast float %79, %55
  %83 = fadd fast float %80, %56
  %84 = add i32 %10, 4
  %85 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %84)  ; CBufferLoadLegacy(handle,regIndex)
  %86 = extractvalue %dx.types.CBufRet.f32 %85, 0
  %87 = extractvalue %dx.types.CBufRet.f32 %85, 1
  %88 = extractvalue %dx.types.CBufRet.f32 %85, 2
  %89 = extractvalue %dx.types.CBufRet.f32 %85, 3
  %90 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %86, float %87, float %88, float %89)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %91 = add i32 %10, 5
  %92 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %91)  ; CBufferLoadLegacy(handle,regIndex)
  %93 = extractvalue %dx.types.CBufRet.f32 %92, 0
  %94 = extractvalue %dx.types.CBufRet.f32 %92, 1
  %95 = extractvalue %dx.types.CBufRet.f32 %92, 2
  %96 = extractvalue %dx.types.CBufRet.f32 %92, 3
  %97 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %93, float %94, float %95, float %96)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %98 = add i32 %10, 6
  %99 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %98)  ; CBufferLoadLegacy(handle,regIndex)
  %100 = extractvalue %dx.types.CBufRet.f32 %99, 0
  %101 = extractvalue %dx.types.CBufRet.f32 %99, 1
  %102 = extractvalue %dx.types.CBufRet.f32 %99, 2
  %103 = extractvalue %dx.types.CBufRet.f32 %99, 3
  %104 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %100, float %101, float %102, float %103)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %105 = fmul fast float %90, %17
  %106 = fmul fast float %97, %17
  %107 = fmul fast float %104, %17
  %108 = fadd fast float %81, %105
  %109 = fadd fast float %82, %106
  %110 = fadd fast float %83, %107
  %111 = add i32 %11, 4
  %112 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %111)  ; CBufferLoadLegacy(handle,regIndex)
  %113 = extractvalue %dx.types.CBufRet.f32 %112, 0
  %114 = extractvalue %dx.types.CBufRet.f32 %112, 1
  %115 = extractvalue %dx.types.CBufRet.f32 %112, 2
  %116 = extractvalue %dx.types.CBufRet.f32 %112, 3
  %117 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %113, float %114, float %115, float %116)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %118 = add i32 %11, 5
  %119 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %118)  ; CBufferLoadLegacy(handle,regIndex)
  %120 = extractvalue %dx.types.CBufRet.f32 %119, 0
  %121 = extractvalue %dx.types.CBufRet.f32 %119, 1
  %122 = extractvalue %dx.types.CBufRet.f32 %119, 2
  %123 = extractvalue %dx.types.CBufRet.f32 %119, 3
  %124 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %120, float %121, float %122, float %123)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %125 = add i32 %11, 6
  %126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %125)  ; CBufferLoadLegacy(handle,regIndex)
  %127 = extractvalue %dx.types.CBufRet.f32 %126, 0
  %128 = extractvalue %dx.types.CBufRet.f32 %126, 1
  %129 = extractvalue %dx.types.CBufRet.f32 %126, 2
  %130 = extractvalue %dx.types.CBufRet.f32 %126, 3
  %131 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %127, float %128, float %129, float %130)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %132 = fmul fast float %117, %18
  %133 = fmul fast float %124, %18
  %134 = fmul fast float %131, %18
  %135 = fadd fast float %108, %132
  %136 = fadd fast float %109, %133
  %137 = fadd fast float %110, %134
  %138 = call float @dx.op.dot4.f32(i32 56, float %15, float %16, float %17, float %18, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %139 = add i32 %4, 4
  %140 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %139)  ; CBufferLoadLegacy(handle,regIndex)
  %141 = extractvalue %dx.types.CBufRet.f32 %140, 0
  %142 = extractvalue %dx.types.CBufRet.f32 %140, 1
  %143 = extractvalue %dx.types.CBufRet.f32 %140, 2
  %144 = extractvalue %dx.types.CBufRet.f32 %140, 3
  %145 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %141, float %142, float %143, float %144)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %146 = add i32 %4, 5
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %146)  ; CBufferLoadLegacy(handle,regIndex)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 0
  %149 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %150 = extractvalue %dx.types.CBufRet.f32 %147, 2
  %151 = extractvalue %dx.types.CBufRet.f32 %147, 3
  %152 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %148, float %149, float %150, float %151)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %153 = add i32 %4, 6
  %154 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %153)  ; CBufferLoadLegacy(handle,regIndex)
  %155 = extractvalue %dx.types.CBufRet.f32 %154, 0
  %156 = extractvalue %dx.types.CBufRet.f32 %154, 1
  %157 = extractvalue %dx.types.CBufRet.f32 %154, 2
  %158 = extractvalue %dx.types.CBufRet.f32 %154, 3
  %159 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %155, float %156, float %157, float %158)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  br i1 %49, label %261, label %160

; <label>:160                                     ; preds = %53
  %161 = fmul fast float %145, %12
  %162 = fmul fast float %152, %12
  %163 = fmul fast float %159, %12
  %164 = fadd fast float %161, %135
  %165 = fadd fast float %162, %136
  %166 = fadd fast float %163, %137
  %167 = add i32 %5, 4
  %168 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %167)  ; CBufferLoadLegacy(handle,regIndex)
  %169 = extractvalue %dx.types.CBufRet.f32 %168, 0
  %170 = extractvalue %dx.types.CBufRet.f32 %168, 1
  %171 = extractvalue %dx.types.CBufRet.f32 %168, 2
  %172 = extractvalue %dx.types.CBufRet.f32 %168, 3
  %173 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %169, float %170, float %171, float %172)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %174 = add i32 %5, 5
  %175 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %174)  ; CBufferLoadLegacy(handle,regIndex)
  %176 = extractvalue %dx.types.CBufRet.f32 %175, 0
  %177 = extractvalue %dx.types.CBufRet.f32 %175, 1
  %178 = extractvalue %dx.types.CBufRet.f32 %175, 2
  %179 = extractvalue %dx.types.CBufRet.f32 %175, 3
  %180 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %176, float %177, float %178, float %179)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %181 = add i32 %5, 6
  %182 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %181)  ; CBufferLoadLegacy(handle,regIndex)
  %183 = extractvalue %dx.types.CBufRet.f32 %182, 0
  %184 = extractvalue %dx.types.CBufRet.f32 %182, 1
  %185 = extractvalue %dx.types.CBufRet.f32 %182, 2
  %186 = extractvalue %dx.types.CBufRet.f32 %182, 3
  %187 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %183, float %184, float %185, float %186)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %188 = fmul fast float %173, %13
  %189 = fmul fast float %180, %13
  %190 = fmul fast float %187, %13
  %191 = fadd fast float %188, %164
  %192 = fadd fast float %189, %165
  %193 = fadd fast float %190, %166
  %194 = add i32 %6, 4
  %195 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %194)  ; CBufferLoadLegacy(handle,regIndex)
  %196 = extractvalue %dx.types.CBufRet.f32 %195, 0
  %197 = extractvalue %dx.types.CBufRet.f32 %195, 1
  %198 = extractvalue %dx.types.CBufRet.f32 %195, 2
  %199 = extractvalue %dx.types.CBufRet.f32 %195, 3
  %200 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %196, float %197, float %198, float %199)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %201 = add i32 %6, 5
  %202 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %201)  ; CBufferLoadLegacy(handle,regIndex)
  %203 = extractvalue %dx.types.CBufRet.f32 %202, 0
  %204 = extractvalue %dx.types.CBufRet.f32 %202, 1
  %205 = extractvalue %dx.types.CBufRet.f32 %202, 2
  %206 = extractvalue %dx.types.CBufRet.f32 %202, 3
  %207 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %203, float %204, float %205, float %206)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %208 = add i32 %6, 6
  %209 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %208)  ; CBufferLoadLegacy(handle,regIndex)
  %210 = extractvalue %dx.types.CBufRet.f32 %209, 0
  %211 = extractvalue %dx.types.CBufRet.f32 %209, 1
  %212 = extractvalue %dx.types.CBufRet.f32 %209, 2
  %213 = extractvalue %dx.types.CBufRet.f32 %209, 3
  %214 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %210, float %211, float %212, float %213)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %215 = fsub fast float 1.000000e+00, %12
  %216 = fsub fast float %215, %13
  br i1 %52, label %253, label %217

; <label>:217                                     ; preds = %160
  %218 = fmul fast float %200, %14
  %219 = fmul fast float %207, %14
  %220 = fmul fast float %214, %14
  %221 = fadd fast float %191, %218
  %222 = fadd fast float %192, %219
  %223 = fadd fast float %193, %220
  %224 = fsub fast float %216, %14
  %225 = fsub fast float %224, %138
  %226 = add i32 %7, 4
  %227 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %226)  ; CBufferLoadLegacy(handle,regIndex)
  %228 = extractvalue %dx.types.CBufRet.f32 %227, 0
  %229 = extractvalue %dx.types.CBufRet.f32 %227, 1
  %230 = extractvalue %dx.types.CBufRet.f32 %227, 2
  %231 = extractvalue %dx.types.CBufRet.f32 %227, 3
  %232 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %228, float %229, float %230, float %231)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %233 = add i32 %7, 5
  %234 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %233)  ; CBufferLoadLegacy(handle,regIndex)
  %235 = extractvalue %dx.types.CBufRet.f32 %234, 0
  %236 = extractvalue %dx.types.CBufRet.f32 %234, 1
  %237 = extractvalue %dx.types.CBufRet.f32 %234, 2
  %238 = extractvalue %dx.types.CBufRet.f32 %234, 3
  %239 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %235, float %236, float %237, float %238)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %240 = add i32 %7, 6
  %241 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %240)  ; CBufferLoadLegacy(handle,regIndex)
  %242 = extractvalue %dx.types.CBufRet.f32 %241, 0
  %243 = extractvalue %dx.types.CBufRet.f32 %241, 1
  %244 = extractvalue %dx.types.CBufRet.f32 %241, 2
  %245 = extractvalue %dx.types.CBufRet.f32 %241, 3
  %246 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %242, float %243, float %244, float %245)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %247 = fmul fast float %232, %225
  %248 = fmul fast float %239, %225
  %249 = fmul fast float %246, %225
  %250 = fadd fast float %221, %247
  %251 = fadd fast float %222, %248
  %252 = fadd fast float %223, %249
  br label %435

; <label>:253                                     ; preds = %160
  %254 = fsub fast float %216, %138
  %255 = fmul fast float %200, %254
  %256 = fmul fast float %207, %254
  %257 = fmul fast float %214, %254
  %258 = fadd fast float %191, %255
  %259 = fadd fast float %192, %256
  %260 = fadd fast float %193, %257
  br label %435

; <label>:261                                     ; preds = %53
  %262 = fsub fast float 1.000000e+00, %138
  br i1 %52, label %298, label %263

; <label>:263                                     ; preds = %261
  %264 = fmul fast float %145, %12
  %265 = fmul fast float %152, %12
  %266 = fmul fast float %159, %12
  %267 = fadd fast float %264, %135
  %268 = fadd fast float %265, %136
  %269 = fadd fast float %266, %137
  %270 = fsub fast float %262, %12
  %271 = add i32 %5, 4
  %272 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %271)  ; CBufferLoadLegacy(handle,regIndex)
  %273 = extractvalue %dx.types.CBufRet.f32 %272, 0
  %274 = extractvalue %dx.types.CBufRet.f32 %272, 1
  %275 = extractvalue %dx.types.CBufRet.f32 %272, 2
  %276 = extractvalue %dx.types.CBufRet.f32 %272, 3
  %277 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %273, float %274, float %275, float %276)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %278 = add i32 %5, 5
  %279 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %278)  ; CBufferLoadLegacy(handle,regIndex)
  %280 = extractvalue %dx.types.CBufRet.f32 %279, 0
  %281 = extractvalue %dx.types.CBufRet.f32 %279, 1
  %282 = extractvalue %dx.types.CBufRet.f32 %279, 2
  %283 = extractvalue %dx.types.CBufRet.f32 %279, 3
  %284 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %280, float %281, float %282, float %283)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %285 = add i32 %5, 6
  %286 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %285)  ; CBufferLoadLegacy(handle,regIndex)
  %287 = extractvalue %dx.types.CBufRet.f32 %286, 0
  %288 = extractvalue %dx.types.CBufRet.f32 %286, 1
  %289 = extractvalue %dx.types.CBufRet.f32 %286, 2
  %290 = extractvalue %dx.types.CBufRet.f32 %286, 3
  %291 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %287, float %288, float %289, float %290)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %292 = fmul fast float %277, %270
  %293 = fmul fast float %284, %270
  %294 = fmul fast float %291, %270
  %295 = fadd fast float %267, %292
  %296 = fadd fast float %268, %293
  %297 = fadd fast float %269, %294
  br label %435

; <label>:298                                     ; preds = %261
  %299 = fmul fast float %145, %262
  %300 = fmul fast float %152, %262
  %301 = fmul fast float %159, %262
  %302 = fadd fast float %299, %135
  %303 = fadd fast float %300, %136
  %304 = fadd fast float %301, %137
  br label %435

; <label>:305                                     ; preds = %0
  br i1 %49, label %402, label %306

; <label>:306                                     ; preds = %305
  %307 = fmul fast float %32, %15
  %308 = fmul fast float %39, %15
  %309 = fmul fast float %46, %15
  %310 = add i32 %9, 4
  %311 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %310)  ; CBufferLoadLegacy(handle,regIndex)
  %312 = extractvalue %dx.types.CBufRet.f32 %311, 0
  %313 = extractvalue %dx.types.CBufRet.f32 %311, 1
  %314 = extractvalue %dx.types.CBufRet.f32 %311, 2
  %315 = extractvalue %dx.types.CBufRet.f32 %311, 3
  %316 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %312, float %313, float %314, float %315)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %317 = add i32 %9, 5
  %318 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %317)  ; CBufferLoadLegacy(handle,regIndex)
  %319 = extractvalue %dx.types.CBufRet.f32 %318, 0
  %320 = extractvalue %dx.types.CBufRet.f32 %318, 1
  %321 = extractvalue %dx.types.CBufRet.f32 %318, 2
  %322 = extractvalue %dx.types.CBufRet.f32 %318, 3
  %323 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %319, float %320, float %321, float %322)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %324 = add i32 %9, 6
  %325 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %324)  ; CBufferLoadLegacy(handle,regIndex)
  %326 = extractvalue %dx.types.CBufRet.f32 %325, 0
  %327 = extractvalue %dx.types.CBufRet.f32 %325, 1
  %328 = extractvalue %dx.types.CBufRet.f32 %325, 2
  %329 = extractvalue %dx.types.CBufRet.f32 %325, 3
  %330 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %326, float %327, float %328, float %329)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %331 = fmul fast float %316, %16
  %332 = fmul fast float %323, %16
  %333 = fmul fast float %330, %16
  %334 = fadd fast float %331, %307
  %335 = fadd fast float %332, %308
  %336 = fadd fast float %333, %309
  %337 = add i32 %10, 4
  %338 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %337)  ; CBufferLoadLegacy(handle,regIndex)
  %339 = extractvalue %dx.types.CBufRet.f32 %338, 0
  %340 = extractvalue %dx.types.CBufRet.f32 %338, 1
  %341 = extractvalue %dx.types.CBufRet.f32 %338, 2
  %342 = extractvalue %dx.types.CBufRet.f32 %338, 3
  %343 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %339, float %340, float %341, float %342)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %344 = add i32 %10, 5
  %345 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %344)  ; CBufferLoadLegacy(handle,regIndex)
  %346 = extractvalue %dx.types.CBufRet.f32 %345, 0
  %347 = extractvalue %dx.types.CBufRet.f32 %345, 1
  %348 = extractvalue %dx.types.CBufRet.f32 %345, 2
  %349 = extractvalue %dx.types.CBufRet.f32 %345, 3
  %350 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %346, float %347, float %348, float %349)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %351 = add i32 %10, 6
  %352 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %351)  ; CBufferLoadLegacy(handle,regIndex)
  %353 = extractvalue %dx.types.CBufRet.f32 %352, 0
  %354 = extractvalue %dx.types.CBufRet.f32 %352, 1
  %355 = extractvalue %dx.types.CBufRet.f32 %352, 2
  %356 = extractvalue %dx.types.CBufRet.f32 %352, 3
  %357 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %353, float %354, float %355, float %356)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %358 = fsub fast float 1.000000e+00, %15
  %359 = fsub fast float %358, %16
  br i1 %52, label %395, label %360

; <label>:360                                     ; preds = %306
  %361 = fmul fast float %343, %17
  %362 = fmul fast float %350, %17
  %363 = fmul fast float %357, %17
  %364 = fadd fast float %334, %361
  %365 = fadd fast float %335, %362
  %366 = fadd fast float %336, %363
  %367 = fsub fast float %359, %17
  %368 = add i32 %11, 4
  %369 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %368)  ; CBufferLoadLegacy(handle,regIndex)
  %370 = extractvalue %dx.types.CBufRet.f32 %369, 0
  %371 = extractvalue %dx.types.CBufRet.f32 %369, 1
  %372 = extractvalue %dx.types.CBufRet.f32 %369, 2
  %373 = extractvalue %dx.types.CBufRet.f32 %369, 3
  %374 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %370, float %371, float %372, float %373)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %375 = add i32 %11, 5
  %376 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %375)  ; CBufferLoadLegacy(handle,regIndex)
  %377 = extractvalue %dx.types.CBufRet.f32 %376, 0
  %378 = extractvalue %dx.types.CBufRet.f32 %376, 1
  %379 = extractvalue %dx.types.CBufRet.f32 %376, 2
  %380 = extractvalue %dx.types.CBufRet.f32 %376, 3
  %381 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %377, float %378, float %379, float %380)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %382 = add i32 %11, 6
  %383 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %382)  ; CBufferLoadLegacy(handle,regIndex)
  %384 = extractvalue %dx.types.CBufRet.f32 %383, 0
  %385 = extractvalue %dx.types.CBufRet.f32 %383, 1
  %386 = extractvalue %dx.types.CBufRet.f32 %383, 2
  %387 = extractvalue %dx.types.CBufRet.f32 %383, 3
  %388 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %384, float %385, float %386, float %387)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %389 = fmul fast float %374, %367
  %390 = fmul fast float %381, %367
  %391 = fmul fast float %388, %367
  %392 = fadd fast float %364, %389
  %393 = fadd fast float %365, %390
  %394 = fadd fast float %366, %391
  br label %435

; <label>:395                                     ; preds = %306
  %396 = fmul fast float %343, %359
  %397 = fmul fast float %350, %359
  %398 = fmul fast float %357, %359
  %399 = fadd fast float %334, %396
  %400 = fadd fast float %335, %397
  %401 = fadd fast float %336, %398
  br label %435

; <label>:402                                     ; preds = %305
  br i1 %52, label %435, label %403

; <label>:403                                     ; preds = %402
  %404 = fmul fast float %32, %15
  %405 = fmul fast float %39, %15
  %406 = fmul fast float %46, %15
  %407 = fsub fast float 1.000000e+00, %15
  %408 = add i32 %9, 4
  %409 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %408)  ; CBufferLoadLegacy(handle,regIndex)
  %410 = extractvalue %dx.types.CBufRet.f32 %409, 0
  %411 = extractvalue %dx.types.CBufRet.f32 %409, 1
  %412 = extractvalue %dx.types.CBufRet.f32 %409, 2
  %413 = extractvalue %dx.types.CBufRet.f32 %409, 3
  %414 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %410, float %411, float %412, float %413)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %415 = add i32 %9, 5
  %416 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %415)  ; CBufferLoadLegacy(handle,regIndex)
  %417 = extractvalue %dx.types.CBufRet.f32 %416, 0
  %418 = extractvalue %dx.types.CBufRet.f32 %416, 1
  %419 = extractvalue %dx.types.CBufRet.f32 %416, 2
  %420 = extractvalue %dx.types.CBufRet.f32 %416, 3
  %421 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %417, float %418, float %419, float %420)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %422 = add i32 %9, 6
  %423 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %422)  ; CBufferLoadLegacy(handle,regIndex)
  %424 = extractvalue %dx.types.CBufRet.f32 %423, 0
  %425 = extractvalue %dx.types.CBufRet.f32 %423, 1
  %426 = extractvalue %dx.types.CBufRet.f32 %423, 2
  %427 = extractvalue %dx.types.CBufRet.f32 %423, 3
  %428 = call float @dx.op.dot4.f32(i32 56, float %19, float %20, float %21, float %22, float %424, float %425, float %426, float %427)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %429 = fmul fast float %414, %407
  %430 = fmul fast float %421, %407
  %431 = fmul fast float %428, %407
  %432 = fadd fast float %429, %404
  %433 = fadd fast float %430, %405
  %434 = fadd fast float %431, %406
  br label %435

; <label>:435                                     ; preds = %403, %402, %395, %360, %298, %263, %253, %217
  %436 = phi float [ %250, %217 ], [ %258, %253 ], [ %295, %263 ], [ %302, %298 ], [ %392, %360 ], [ %399, %395 ], [ %432, %403 ], [ %32, %402 ]
  %437 = phi float [ %251, %217 ], [ %259, %253 ], [ %296, %263 ], [ %303, %298 ], [ %393, %360 ], [ %400, %395 ], [ %433, %403 ], [ %39, %402 ]
  %438 = phi float [ %252, %217 ], [ %260, %253 ], [ %297, %263 ], [ %304, %298 ], [ %394, %360 ], [ %401, %395 ], [ %434, %403 ], [ %46, %402 ]
  %439 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %440 = extractvalue %dx.types.CBufRet.f32 %439, 0
  %441 = extractvalue %dx.types.CBufRet.f32 %439, 1
  %442 = extractvalue %dx.types.CBufRet.f32 %439, 2
  %443 = extractvalue %dx.types.CBufRet.f32 %439, 3
  %444 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %445 = extractvalue %dx.types.CBufRet.f32 %444, 0
  %446 = extractvalue %dx.types.CBufRet.f32 %444, 1
  %447 = extractvalue %dx.types.CBufRet.f32 %444, 2
  %448 = extractvalue %dx.types.CBufRet.f32 %444, 3
  %449 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %450 = extractvalue %dx.types.CBufRet.f32 %449, 0
  %451 = extractvalue %dx.types.CBufRet.f32 %449, 1
  %452 = extractvalue %dx.types.CBufRet.f32 %449, 2
  %453 = extractvalue %dx.types.CBufRet.f32 %449, 3
  %454 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %455 = extractvalue %dx.types.CBufRet.f32 %454, 0
  %456 = extractvalue %dx.types.CBufRet.f32 %454, 1
  %457 = extractvalue %dx.types.CBufRet.f32 %454, 2
  %458 = extractvalue %dx.types.CBufRet.f32 %454, 3
  %459 = fmul fast float %440, %436
  %460 = call float @dx.op.tertiary.f32(i32 46, float %437, float %445, float %459)  ; FMad(a,b,c)
  %461 = call float @dx.op.tertiary.f32(i32 46, float %438, float %450, float %460)  ; FMad(a,b,c)
  %462 = fadd fast float %461, %455
  %463 = fmul fast float %441, %436
  %464 = call float @dx.op.tertiary.f32(i32 46, float %437, float %446, float %463)  ; FMad(a,b,c)
  %465 = call float @dx.op.tertiary.f32(i32 46, float %438, float %451, float %464)  ; FMad(a,b,c)
  %466 = fadd fast float %465, %456
  %467 = fmul fast float %442, %436
  %468 = call float @dx.op.tertiary.f32(i32 46, float %437, float %447, float %467)  ; FMad(a,b,c)
  %469 = call float @dx.op.tertiary.f32(i32 46, float %438, float %452, float %468)  ; FMad(a,b,c)
  %470 = fadd fast float %469, %457
  %471 = fmul fast float %443, %436
  %472 = call float @dx.op.tertiary.f32(i32 46, float %437, float %448, float %471)  ; FMad(a,b,c)
  %473 = call float @dx.op.tertiary.f32(i32 46, float %438, float %453, float %472)  ; FMad(a,b,c)
  %474 = fadd fast float %473, %458
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %462)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %466)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %470)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %474)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
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
!5 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 6244, null}
!6 = !{[24 x i32] [i32 22, i32 6, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 0, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 16, i32 32]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !21, null}
!9 = !{!10, !13, !14, !17, !18, !19}
!10 = !{i32 0, !"POSITION", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 0, i8 0, !12}
!11 = !{i32 0}
!12 = !{i32 3, i32 15}
!13 = !{i32 1, !"BLENDWEIGHT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 1, i8 0, !12}
!14 = !{i32 2, !"BLENDWEIGHT", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 2, i8 0, !16}
!15 = !{i32 1}
!16 = !{i32 3, i32 7}
!17 = !{i32 3, !"BLENDINDICES", i8 4, i8 0, !11, i8 0, i32 1, i8 4, i32 3, i8 0, !12}
!18 = !{i32 4, !"BLENDINDICES", i8 4, i8 0, !15, i8 0, i32 1, i8 4, i32 4, i8 0, !12}
!19 = !{i32 5, !"TEXCOORD", i8 9, i8 0, !11, i8 0, i32 1, i8 2, i32 5, i8 0, !20}
!20 = !{i32 3, i32 3}
!21 = !{!22, !23}
!22 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}
!23 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !11, i8 2, i32 1, i8 2, i32 1, i8 0, !20}

