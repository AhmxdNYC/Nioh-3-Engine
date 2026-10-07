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
; NORMAL                   0   xyz         5     NONE   float   xyz 
; TEXCOORD                 0   xy          6     NONE   float   xy  
; TANGENT                  0   xyzw        7     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; TEXCOORD                 0   xyzw        1     NONE   float   xyzw
; TEXCOORD                 1   xyzw        2     NONE   float   xyzw
; TEXCOORD                 2   xyzw        3     NONE   float   xyzw
; TEXCOORD                 3   xyz         4     NONE   float   xyz 
; TEXCOORD                 4   xyz         5     NONE   float   xyz 
; TEXCOORD                 5   xy          6     NONE   float   xy  
;
; shader hash: b8c8b99b1eb5723092f998cc8dbb060a
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 8
; SigOutputElements: 7
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 8
; SigOutputVectors[0]: 7
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
; NORMAL                   0                              
; TEXCOORD                 0                              
; TANGENT                  0                              
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; TEXCOORD                 0                 linear       
; TEXCOORD                 1                 linear       
; TEXCOORD                 2                 linear       
; TEXCOORD                 3                 linear       
; TEXCOORD                 4                 linear       
; TEXCOORD                 5                 linear       
;
; Buffer Definitions:
;
; cbuffer 
; {
;
;   [12480 x i8] (type annotation not present)
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
; Number of inputs: 32, outputs: 26
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 1 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 2 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 3 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 4 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 5 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 6 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 7 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 8 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 9 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 10 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 12 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 13 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 14 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19 }
;   output 15 depends on inputs: { 31 }
;   output 16 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 28, 29, 30 }
;   output 17 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 28, 29, 30 }
;   output 18 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 28, 29, 30 }
;   output 20 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 }
;   output 21 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 }
;   output 22 depends on inputs: { 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 }
;   output 24 depends on inputs: { 24 }
;   output 25 depends on inputs: { 25 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [768 x <4 x float>], [3 x i32], [4 x <4 x float>], <4 x i32> }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 7, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 7, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 7, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 7, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 6, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 6, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 5, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 5, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 5, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call i32 @dx.op.loadInput.i32(i32 4, i32 4, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = call i32 @dx.op.loadInput.i32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %19 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %20 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %21 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %22 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %23 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %24 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %25 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %26 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %27 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %28 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %29 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %30 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 774)  ; CBufferLoadLegacy(handle,regIndex)
  %31 = extractvalue %dx.types.CBufRet.i32 %30, 0
  %32 = icmp eq i32 %31, 0
  %33 = add i32 %15, 4
  %34 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %33)  ; CBufferLoadLegacy(handle,regIndex)
  %35 = extractvalue %dx.types.CBufRet.f32 %34, 0
  %36 = extractvalue %dx.types.CBufRet.f32 %34, 1
  %37 = extractvalue %dx.types.CBufRet.f32 %34, 2
  %38 = extractvalue %dx.types.CBufRet.f32 %34, 3
  %39 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %35, float %36, float %37, float %38)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %40 = add i32 %15, 5
  %41 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %40)  ; CBufferLoadLegacy(handle,regIndex)
  %42 = extractvalue %dx.types.CBufRet.f32 %41, 0
  %43 = extractvalue %dx.types.CBufRet.f32 %41, 1
  %44 = extractvalue %dx.types.CBufRet.f32 %41, 2
  %45 = extractvalue %dx.types.CBufRet.f32 %41, 3
  %46 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %42, float %43, float %44, float %45)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %47 = add i32 %15, 6
  %48 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %47)  ; CBufferLoadLegacy(handle,regIndex)
  %49 = extractvalue %dx.types.CBufRet.f32 %48, 0
  %50 = extractvalue %dx.types.CBufRet.f32 %48, 1
  %51 = extractvalue %dx.types.CBufRet.f32 %48, 2
  %52 = extractvalue %dx.types.CBufRet.f32 %48, 3
  %53 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %49, float %50, float %51, float %52)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %54 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %35, float %36, float %37)  ; Dot3(ax,ay,az,bx,by,bz)
  %55 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %42, float %43, float %44)  ; Dot3(ax,ay,az,bx,by,bz)
  %56 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %49, float %50, float %51)  ; Dot3(ax,ay,az,bx,by,bz)
  %57 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %35, float %36, float %37)  ; Dot3(ax,ay,az,bx,by,bz)
  %58 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %42, float %43, float %44)  ; Dot3(ax,ay,az,bx,by,bz)
  %59 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %49, float %50, float %51)  ; Dot3(ax,ay,az,bx,by,bz)
  %60 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 772)  ; CBufferLoadLegacy(handle,regIndex)
  %61 = extractvalue %dx.types.CBufRet.i32 %60, 0
  %62 = icmp eq i32 %61, 0
  %63 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 773)  ; CBufferLoadLegacy(handle,regIndex)
  %64 = extractvalue %dx.types.CBufRet.i32 %63, 0
  %65 = icmp eq i32 %64, 0
  br i1 %32, label %504, label %66

; <label>:66                                      ; preds = %0
  %67 = fmul fast float %39, %22
  %68 = fmul fast float %46, %22
  %69 = fmul fast float %53, %22
  %70 = fmul fast float %54, %22
  %71 = fmul fast float %55, %22
  %72 = fmul fast float %56, %22
  %73 = fmul fast float %57, %22
  %74 = fmul fast float %58, %22
  %75 = fmul fast float %59, %22
  %76 = add i32 %16, 4
  %77 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %76)  ; CBufferLoadLegacy(handle,regIndex)
  %78 = extractvalue %dx.types.CBufRet.f32 %77, 0
  %79 = extractvalue %dx.types.CBufRet.f32 %77, 1
  %80 = extractvalue %dx.types.CBufRet.f32 %77, 2
  %81 = extractvalue %dx.types.CBufRet.f32 %77, 3
  %82 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %78, float %79, float %80, float %81)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %83 = add i32 %16, 5
  %84 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %83)  ; CBufferLoadLegacy(handle,regIndex)
  %85 = extractvalue %dx.types.CBufRet.f32 %84, 0
  %86 = extractvalue %dx.types.CBufRet.f32 %84, 1
  %87 = extractvalue %dx.types.CBufRet.f32 %84, 2
  %88 = extractvalue %dx.types.CBufRet.f32 %84, 3
  %89 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %85, float %86, float %87, float %88)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %90 = add i32 %16, 6
  %91 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %90)  ; CBufferLoadLegacy(handle,regIndex)
  %92 = extractvalue %dx.types.CBufRet.f32 %91, 0
  %93 = extractvalue %dx.types.CBufRet.f32 %91, 1
  %94 = extractvalue %dx.types.CBufRet.f32 %91, 2
  %95 = extractvalue %dx.types.CBufRet.f32 %91, 3
  %96 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %92, float %93, float %94, float %95)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %97 = fmul fast float %82, %23
  %98 = fmul fast float %89, %23
  %99 = fmul fast float %96, %23
  %100 = fadd fast float %97, %67
  %101 = fadd fast float %98, %68
  %102 = fadd fast float %99, %69
  %103 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %78, float %79, float %80)  ; Dot3(ax,ay,az,bx,by,bz)
  %104 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %85, float %86, float %87)  ; Dot3(ax,ay,az,bx,by,bz)
  %105 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %92, float %93, float %94)  ; Dot3(ax,ay,az,bx,by,bz)
  %106 = fmul fast float %103, %23
  %107 = fmul fast float %104, %23
  %108 = fmul fast float %105, %23
  %109 = fadd fast float %106, %70
  %110 = fadd fast float %107, %71
  %111 = fadd fast float %108, %72
  %112 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %78, float %79, float %80)  ; Dot3(ax,ay,az,bx,by,bz)
  %113 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %85, float %86, float %87)  ; Dot3(ax,ay,az,bx,by,bz)
  %114 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %92, float %93, float %94)  ; Dot3(ax,ay,az,bx,by,bz)
  %115 = fmul fast float %112, %23
  %116 = fmul fast float %113, %23
  %117 = fmul fast float %114, %23
  %118 = fadd fast float %115, %73
  %119 = fadd fast float %116, %74
  %120 = fadd fast float %117, %75
  %121 = add i32 %17, 4
  %122 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %121)  ; CBufferLoadLegacy(handle,regIndex)
  %123 = extractvalue %dx.types.CBufRet.f32 %122, 0
  %124 = extractvalue %dx.types.CBufRet.f32 %122, 1
  %125 = extractvalue %dx.types.CBufRet.f32 %122, 2
  %126 = extractvalue %dx.types.CBufRet.f32 %122, 3
  %127 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %123, float %124, float %125, float %126)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %128 = add i32 %17, 5
  %129 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %128)  ; CBufferLoadLegacy(handle,regIndex)
  %130 = extractvalue %dx.types.CBufRet.f32 %129, 0
  %131 = extractvalue %dx.types.CBufRet.f32 %129, 1
  %132 = extractvalue %dx.types.CBufRet.f32 %129, 2
  %133 = extractvalue %dx.types.CBufRet.f32 %129, 3
  %134 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %130, float %131, float %132, float %133)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %135 = add i32 %17, 6
  %136 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %135)  ; CBufferLoadLegacy(handle,regIndex)
  %137 = extractvalue %dx.types.CBufRet.f32 %136, 0
  %138 = extractvalue %dx.types.CBufRet.f32 %136, 1
  %139 = extractvalue %dx.types.CBufRet.f32 %136, 2
  %140 = extractvalue %dx.types.CBufRet.f32 %136, 3
  %141 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %137, float %138, float %139, float %140)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %142 = fmul fast float %127, %24
  %143 = fmul fast float %134, %24
  %144 = fmul fast float %141, %24
  %145 = fadd fast float %100, %142
  %146 = fadd fast float %101, %143
  %147 = fadd fast float %102, %144
  %148 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %123, float %124, float %125)  ; Dot3(ax,ay,az,bx,by,bz)
  %149 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %130, float %131, float %132)  ; Dot3(ax,ay,az,bx,by,bz)
  %150 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %137, float %138, float %139)  ; Dot3(ax,ay,az,bx,by,bz)
  %151 = fmul fast float %148, %24
  %152 = fmul fast float %149, %24
  %153 = fmul fast float %150, %24
  %154 = fadd fast float %109, %151
  %155 = fadd fast float %110, %152
  %156 = fadd fast float %111, %153
  %157 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %123, float %124, float %125)  ; Dot3(ax,ay,az,bx,by,bz)
  %158 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %130, float %131, float %132)  ; Dot3(ax,ay,az,bx,by,bz)
  %159 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %137, float %138, float %139)  ; Dot3(ax,ay,az,bx,by,bz)
  %160 = fmul fast float %157, %24
  %161 = fmul fast float %158, %24
  %162 = fmul fast float %159, %24
  %163 = fadd fast float %118, %160
  %164 = fadd fast float %119, %161
  %165 = fadd fast float %120, %162
  %166 = add i32 %18, 4
  %167 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %166)  ; CBufferLoadLegacy(handle,regIndex)
  %168 = extractvalue %dx.types.CBufRet.f32 %167, 0
  %169 = extractvalue %dx.types.CBufRet.f32 %167, 1
  %170 = extractvalue %dx.types.CBufRet.f32 %167, 2
  %171 = extractvalue %dx.types.CBufRet.f32 %167, 3
  %172 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %168, float %169, float %170, float %171)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %173 = add i32 %18, 5
  %174 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %173)  ; CBufferLoadLegacy(handle,regIndex)
  %175 = extractvalue %dx.types.CBufRet.f32 %174, 0
  %176 = extractvalue %dx.types.CBufRet.f32 %174, 1
  %177 = extractvalue %dx.types.CBufRet.f32 %174, 2
  %178 = extractvalue %dx.types.CBufRet.f32 %174, 3
  %179 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %175, float %176, float %177, float %178)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %180 = add i32 %18, 6
  %181 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %180)  ; CBufferLoadLegacy(handle,regIndex)
  %182 = extractvalue %dx.types.CBufRet.f32 %181, 0
  %183 = extractvalue %dx.types.CBufRet.f32 %181, 1
  %184 = extractvalue %dx.types.CBufRet.f32 %181, 2
  %185 = extractvalue %dx.types.CBufRet.f32 %181, 3
  %186 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %182, float %183, float %184, float %185)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %187 = fmul fast float %172, %25
  %188 = fmul fast float %179, %25
  %189 = fmul fast float %186, %25
  %190 = fadd fast float %145, %187
  %191 = fadd fast float %146, %188
  %192 = fadd fast float %147, %189
  %193 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %168, float %169, float %170)  ; Dot3(ax,ay,az,bx,by,bz)
  %194 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %175, float %176, float %177)  ; Dot3(ax,ay,az,bx,by,bz)
  %195 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %182, float %183, float %184)  ; Dot3(ax,ay,az,bx,by,bz)
  %196 = fmul fast float %193, %25
  %197 = fmul fast float %194, %25
  %198 = fmul fast float %195, %25
  %199 = fadd fast float %154, %196
  %200 = fadd fast float %155, %197
  %201 = fadd fast float %156, %198
  %202 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %168, float %169, float %170)  ; Dot3(ax,ay,az,bx,by,bz)
  %203 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %175, float %176, float %177)  ; Dot3(ax,ay,az,bx,by,bz)
  %204 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %182, float %183, float %184)  ; Dot3(ax,ay,az,bx,by,bz)
  %205 = fmul fast float %202, %25
  %206 = fmul fast float %203, %25
  %207 = fmul fast float %204, %25
  %208 = fadd fast float %163, %205
  %209 = fadd fast float %164, %206
  %210 = fadd fast float %165, %207
  %211 = call float @dx.op.dot4.f32(i32 56, float %22, float %23, float %24, float %25, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %212 = add i32 %11, 4
  %213 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %212)  ; CBufferLoadLegacy(handle,regIndex)
  %214 = extractvalue %dx.types.CBufRet.f32 %213, 0
  %215 = extractvalue %dx.types.CBufRet.f32 %213, 1
  %216 = extractvalue %dx.types.CBufRet.f32 %213, 2
  %217 = extractvalue %dx.types.CBufRet.f32 %213, 3
  %218 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %214, float %215, float %216, float %217)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %219 = add i32 %11, 5
  %220 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %219)  ; CBufferLoadLegacy(handle,regIndex)
  %221 = extractvalue %dx.types.CBufRet.f32 %220, 0
  %222 = extractvalue %dx.types.CBufRet.f32 %220, 1
  %223 = extractvalue %dx.types.CBufRet.f32 %220, 2
  %224 = extractvalue %dx.types.CBufRet.f32 %220, 3
  %225 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %221, float %222, float %223, float %224)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %226 = add i32 %11, 6
  %227 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %226)  ; CBufferLoadLegacy(handle,regIndex)
  %228 = extractvalue %dx.types.CBufRet.f32 %227, 0
  %229 = extractvalue %dx.types.CBufRet.f32 %227, 1
  %230 = extractvalue %dx.types.CBufRet.f32 %227, 2
  %231 = extractvalue %dx.types.CBufRet.f32 %227, 3
  %232 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %228, float %229, float %230, float %231)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %233 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %214, float %215, float %216)  ; Dot3(ax,ay,az,bx,by,bz)
  %234 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %221, float %222, float %223)  ; Dot3(ax,ay,az,bx,by,bz)
  %235 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %228, float %229, float %230)  ; Dot3(ax,ay,az,bx,by,bz)
  %236 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %214, float %215, float %216)  ; Dot3(ax,ay,az,bx,by,bz)
  %237 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %221, float %222, float %223)  ; Dot3(ax,ay,az,bx,by,bz)
  %238 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %228, float %229, float %230)  ; Dot3(ax,ay,az,bx,by,bz)
  br i1 %62, label %418, label %239

; <label>:239                                     ; preds = %66
  %240 = fmul fast float %218, %19
  %241 = fmul fast float %225, %19
  %242 = fmul fast float %232, %19
  %243 = fadd fast float %240, %190
  %244 = fadd fast float %241, %191
  %245 = fadd fast float %242, %192
  %246 = fmul fast float %233, %19
  %247 = fmul fast float %234, %19
  %248 = fmul fast float %235, %19
  %249 = fadd fast float %246, %199
  %250 = fadd fast float %247, %200
  %251 = fadd fast float %248, %201
  %252 = fmul fast float %236, %19
  %253 = fmul fast float %237, %19
  %254 = fmul fast float %238, %19
  %255 = fadd fast float %252, %208
  %256 = fadd fast float %253, %209
  %257 = fadd fast float %254, %210
  %258 = add i32 %12, 4
  %259 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %258)  ; CBufferLoadLegacy(handle,regIndex)
  %260 = extractvalue %dx.types.CBufRet.f32 %259, 0
  %261 = extractvalue %dx.types.CBufRet.f32 %259, 1
  %262 = extractvalue %dx.types.CBufRet.f32 %259, 2
  %263 = extractvalue %dx.types.CBufRet.f32 %259, 3
  %264 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %260, float %261, float %262, float %263)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %265 = add i32 %12, 5
  %266 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %265)  ; CBufferLoadLegacy(handle,regIndex)
  %267 = extractvalue %dx.types.CBufRet.f32 %266, 0
  %268 = extractvalue %dx.types.CBufRet.f32 %266, 1
  %269 = extractvalue %dx.types.CBufRet.f32 %266, 2
  %270 = extractvalue %dx.types.CBufRet.f32 %266, 3
  %271 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %267, float %268, float %269, float %270)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %272 = add i32 %12, 6
  %273 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %272)  ; CBufferLoadLegacy(handle,regIndex)
  %274 = extractvalue %dx.types.CBufRet.f32 %273, 0
  %275 = extractvalue %dx.types.CBufRet.f32 %273, 1
  %276 = extractvalue %dx.types.CBufRet.f32 %273, 2
  %277 = extractvalue %dx.types.CBufRet.f32 %273, 3
  %278 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %274, float %275, float %276, float %277)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %279 = fmul fast float %264, %20
  %280 = fmul fast float %271, %20
  %281 = fmul fast float %278, %20
  %282 = fadd fast float %279, %243
  %283 = fadd fast float %280, %244
  %284 = fadd fast float %281, %245
  %285 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %260, float %261, float %262)  ; Dot3(ax,ay,az,bx,by,bz)
  %286 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %267, float %268, float %269)  ; Dot3(ax,ay,az,bx,by,bz)
  %287 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %274, float %275, float %276)  ; Dot3(ax,ay,az,bx,by,bz)
  %288 = fmul fast float %285, %20
  %289 = fmul fast float %286, %20
  %290 = fmul fast float %287, %20
  %291 = fadd fast float %288, %249
  %292 = fadd fast float %289, %250
  %293 = fadd fast float %290, %251
  %294 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %260, float %261, float %262)  ; Dot3(ax,ay,az,bx,by,bz)
  %295 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %267, float %268, float %269)  ; Dot3(ax,ay,az,bx,by,bz)
  %296 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %274, float %275, float %276)  ; Dot3(ax,ay,az,bx,by,bz)
  %297 = fmul fast float %294, %20
  %298 = fmul fast float %295, %20
  %299 = fmul fast float %296, %20
  %300 = fadd fast float %297, %255
  %301 = fadd fast float %298, %256
  %302 = fadd fast float %299, %257
  %303 = add i32 %13, 4
  %304 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %303)  ; CBufferLoadLegacy(handle,regIndex)
  %305 = extractvalue %dx.types.CBufRet.f32 %304, 0
  %306 = extractvalue %dx.types.CBufRet.f32 %304, 1
  %307 = extractvalue %dx.types.CBufRet.f32 %304, 2
  %308 = extractvalue %dx.types.CBufRet.f32 %304, 3
  %309 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %305, float %306, float %307, float %308)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %310 = add i32 %13, 5
  %311 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %310)  ; CBufferLoadLegacy(handle,regIndex)
  %312 = extractvalue %dx.types.CBufRet.f32 %311, 0
  %313 = extractvalue %dx.types.CBufRet.f32 %311, 1
  %314 = extractvalue %dx.types.CBufRet.f32 %311, 2
  %315 = extractvalue %dx.types.CBufRet.f32 %311, 3
  %316 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %312, float %313, float %314, float %315)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %317 = add i32 %13, 6
  %318 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %317)  ; CBufferLoadLegacy(handle,regIndex)
  %319 = extractvalue %dx.types.CBufRet.f32 %318, 0
  %320 = extractvalue %dx.types.CBufRet.f32 %318, 1
  %321 = extractvalue %dx.types.CBufRet.f32 %318, 2
  %322 = extractvalue %dx.types.CBufRet.f32 %318, 3
  %323 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %319, float %320, float %321, float %322)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %324 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %305, float %306, float %307)  ; Dot3(ax,ay,az,bx,by,bz)
  %325 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %312, float %313, float %314)  ; Dot3(ax,ay,az,bx,by,bz)
  %326 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %319, float %320, float %321)  ; Dot3(ax,ay,az,bx,by,bz)
  %327 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %305, float %306, float %307)  ; Dot3(ax,ay,az,bx,by,bz)
  %328 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %312, float %313, float %314)  ; Dot3(ax,ay,az,bx,by,bz)
  %329 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %319, float %320, float %321)  ; Dot3(ax,ay,az,bx,by,bz)
  %330 = fsub fast float 1.000000e+00, %19
  %331 = fsub fast float %330, %20
  br i1 %65, label %398, label %332

; <label>:332                                     ; preds = %239
  %333 = fmul fast float %309, %21
  %334 = fmul fast float %316, %21
  %335 = fmul fast float %323, %21
  %336 = fadd fast float %282, %333
  %337 = fadd fast float %283, %334
  %338 = fadd fast float %284, %335
  %339 = fmul fast float %324, %21
  %340 = fmul fast float %325, %21
  %341 = fmul fast float %326, %21
  %342 = fadd fast float %291, %339
  %343 = fadd fast float %292, %340
  %344 = fadd fast float %293, %341
  %345 = fmul fast float %327, %21
  %346 = fmul fast float %328, %21
  %347 = fmul fast float %329, %21
  %348 = fadd fast float %300, %345
  %349 = fadd fast float %301, %346
  %350 = fadd fast float %302, %347
  %351 = fsub fast float %331, %21
  %352 = fsub fast float %351, %211
  %353 = add i32 %14, 4
  %354 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %353)  ; CBufferLoadLegacy(handle,regIndex)
  %355 = extractvalue %dx.types.CBufRet.f32 %354, 0
  %356 = extractvalue %dx.types.CBufRet.f32 %354, 1
  %357 = extractvalue %dx.types.CBufRet.f32 %354, 2
  %358 = extractvalue %dx.types.CBufRet.f32 %354, 3
  %359 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %355, float %356, float %357, float %358)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %360 = add i32 %14, 5
  %361 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %360)  ; CBufferLoadLegacy(handle,regIndex)
  %362 = extractvalue %dx.types.CBufRet.f32 %361, 0
  %363 = extractvalue %dx.types.CBufRet.f32 %361, 1
  %364 = extractvalue %dx.types.CBufRet.f32 %361, 2
  %365 = extractvalue %dx.types.CBufRet.f32 %361, 3
  %366 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %362, float %363, float %364, float %365)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %367 = add i32 %14, 6
  %368 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %367)  ; CBufferLoadLegacy(handle,regIndex)
  %369 = extractvalue %dx.types.CBufRet.f32 %368, 0
  %370 = extractvalue %dx.types.CBufRet.f32 %368, 1
  %371 = extractvalue %dx.types.CBufRet.f32 %368, 2
  %372 = extractvalue %dx.types.CBufRet.f32 %368, 3
  %373 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %369, float %370, float %371, float %372)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %374 = fmul fast float %359, %352
  %375 = fmul fast float %366, %352
  %376 = fmul fast float %373, %352
  %377 = fadd fast float %336, %374
  %378 = fadd fast float %337, %375
  %379 = fadd fast float %338, %376
  %380 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %355, float %356, float %357)  ; Dot3(ax,ay,az,bx,by,bz)
  %381 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %362, float %363, float %364)  ; Dot3(ax,ay,az,bx,by,bz)
  %382 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %369, float %370, float %371)  ; Dot3(ax,ay,az,bx,by,bz)
  %383 = fmul fast float %380, %352
  %384 = fmul fast float %381, %352
  %385 = fmul fast float %382, %352
  %386 = fadd fast float %342, %383
  %387 = fadd fast float %343, %384
  %388 = fadd fast float %344, %385
  %389 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %355, float %356, float %357)  ; Dot3(ax,ay,az,bx,by,bz)
  %390 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %362, float %363, float %364)  ; Dot3(ax,ay,az,bx,by,bz)
  %391 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %369, float %370, float %371)  ; Dot3(ax,ay,az,bx,by,bz)
  %392 = fmul fast float %389, %352
  %393 = fmul fast float %390, %352
  %394 = fmul fast float %391, %352
  %395 = fadd fast float %348, %392
  %396 = fadd fast float %349, %393
  %397 = fadd fast float %350, %394
  br label %730

; <label>:398                                     ; preds = %239
  %399 = fsub fast float %331, %211
  %400 = fmul fast float %309, %399
  %401 = fmul fast float %316, %399
  %402 = fmul fast float %323, %399
  %403 = fadd fast float %282, %400
  %404 = fadd fast float %283, %401
  %405 = fadd fast float %284, %402
  %406 = fmul fast float %324, %399
  %407 = fmul fast float %325, %399
  %408 = fmul fast float %326, %399
  %409 = fadd fast float %291, %406
  %410 = fadd fast float %292, %407
  %411 = fadd fast float %293, %408
  %412 = fmul fast float %327, %399
  %413 = fmul fast float %328, %399
  %414 = fmul fast float %329, %399
  %415 = fadd fast float %300, %412
  %416 = fadd fast float %301, %413
  %417 = fadd fast float %302, %414
  br label %730

; <label>:418                                     ; preds = %66
  %419 = fsub fast float 1.000000e+00, %211
  br i1 %65, label %485, label %420

; <label>:420                                     ; preds = %418
  %421 = fmul fast float %218, %19
  %422 = fmul fast float %225, %19
  %423 = fmul fast float %232, %19
  %424 = fadd fast float %421, %190
  %425 = fadd fast float %422, %191
  %426 = fadd fast float %423, %192
  %427 = fmul fast float %233, %19
  %428 = fmul fast float %234, %19
  %429 = fmul fast float %235, %19
  %430 = fadd fast float %427, %199
  %431 = fadd fast float %428, %200
  %432 = fadd fast float %429, %201
  %433 = fmul fast float %236, %19
  %434 = fmul fast float %237, %19
  %435 = fmul fast float %238, %19
  %436 = fadd fast float %433, %208
  %437 = fadd fast float %434, %209
  %438 = fadd fast float %435, %210
  %439 = fsub fast float %419, %19
  %440 = add i32 %12, 4
  %441 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %440)  ; CBufferLoadLegacy(handle,regIndex)
  %442 = extractvalue %dx.types.CBufRet.f32 %441, 0
  %443 = extractvalue %dx.types.CBufRet.f32 %441, 1
  %444 = extractvalue %dx.types.CBufRet.f32 %441, 2
  %445 = extractvalue %dx.types.CBufRet.f32 %441, 3
  %446 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %442, float %443, float %444, float %445)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %447 = add i32 %12, 5
  %448 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %447)  ; CBufferLoadLegacy(handle,regIndex)
  %449 = extractvalue %dx.types.CBufRet.f32 %448, 0
  %450 = extractvalue %dx.types.CBufRet.f32 %448, 1
  %451 = extractvalue %dx.types.CBufRet.f32 %448, 2
  %452 = extractvalue %dx.types.CBufRet.f32 %448, 3
  %453 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %449, float %450, float %451, float %452)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %454 = add i32 %12, 6
  %455 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %454)  ; CBufferLoadLegacy(handle,regIndex)
  %456 = extractvalue %dx.types.CBufRet.f32 %455, 0
  %457 = extractvalue %dx.types.CBufRet.f32 %455, 1
  %458 = extractvalue %dx.types.CBufRet.f32 %455, 2
  %459 = extractvalue %dx.types.CBufRet.f32 %455, 3
  %460 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %456, float %457, float %458, float %459)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %461 = fmul fast float %446, %439
  %462 = fmul fast float %453, %439
  %463 = fmul fast float %460, %439
  %464 = fadd fast float %424, %461
  %465 = fadd fast float %425, %462
  %466 = fadd fast float %426, %463
  %467 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %442, float %443, float %444)  ; Dot3(ax,ay,az,bx,by,bz)
  %468 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %449, float %450, float %451)  ; Dot3(ax,ay,az,bx,by,bz)
  %469 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %456, float %457, float %458)  ; Dot3(ax,ay,az,bx,by,bz)
  %470 = fmul fast float %467, %439
  %471 = fmul fast float %468, %439
  %472 = fmul fast float %469, %439
  %473 = fadd fast float %430, %470
  %474 = fadd fast float %431, %471
  %475 = fadd fast float %432, %472
  %476 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %442, float %443, float %444)  ; Dot3(ax,ay,az,bx,by,bz)
  %477 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %449, float %450, float %451)  ; Dot3(ax,ay,az,bx,by,bz)
  %478 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %456, float %457, float %458)  ; Dot3(ax,ay,az,bx,by,bz)
  %479 = fmul fast float %476, %439
  %480 = fmul fast float %477, %439
  %481 = fmul fast float %478, %439
  %482 = fadd fast float %436, %479
  %483 = fadd fast float %437, %480
  %484 = fadd fast float %438, %481
  br label %730

; <label>:485                                     ; preds = %418
  %486 = fmul fast float %218, %419
  %487 = fmul fast float %225, %419
  %488 = fmul fast float %232, %419
  %489 = fadd fast float %486, %190
  %490 = fadd fast float %487, %191
  %491 = fadd fast float %488, %192
  %492 = fmul fast float %233, %419
  %493 = fmul fast float %234, %419
  %494 = fmul fast float %235, %419
  %495 = fadd fast float %492, %199
  %496 = fadd fast float %493, %200
  %497 = fadd fast float %494, %201
  %498 = fmul fast float %236, %419
  %499 = fmul fast float %237, %419
  %500 = fmul fast float %238, %419
  %501 = fadd fast float %498, %208
  %502 = fadd fast float %499, %209
  %503 = fadd fast float %500, %210
  br label %730

; <label>:504                                     ; preds = %0
  br i1 %62, label %673, label %505

; <label>:505                                     ; preds = %504
  %506 = fmul fast float %39, %22
  %507 = fmul fast float %46, %22
  %508 = fmul fast float %53, %22
  %509 = fmul fast float %54, %22
  %510 = fmul fast float %55, %22
  %511 = fmul fast float %56, %22
  %512 = fmul fast float %57, %22
  %513 = fmul fast float %58, %22
  %514 = fmul fast float %59, %22
  %515 = add i32 %16, 4
  %516 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %515)  ; CBufferLoadLegacy(handle,regIndex)
  %517 = extractvalue %dx.types.CBufRet.f32 %516, 0
  %518 = extractvalue %dx.types.CBufRet.f32 %516, 1
  %519 = extractvalue %dx.types.CBufRet.f32 %516, 2
  %520 = extractvalue %dx.types.CBufRet.f32 %516, 3
  %521 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %517, float %518, float %519, float %520)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %522 = add i32 %16, 5
  %523 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %522)  ; CBufferLoadLegacy(handle,regIndex)
  %524 = extractvalue %dx.types.CBufRet.f32 %523, 0
  %525 = extractvalue %dx.types.CBufRet.f32 %523, 1
  %526 = extractvalue %dx.types.CBufRet.f32 %523, 2
  %527 = extractvalue %dx.types.CBufRet.f32 %523, 3
  %528 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %524, float %525, float %526, float %527)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %529 = add i32 %16, 6
  %530 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %529)  ; CBufferLoadLegacy(handle,regIndex)
  %531 = extractvalue %dx.types.CBufRet.f32 %530, 0
  %532 = extractvalue %dx.types.CBufRet.f32 %530, 1
  %533 = extractvalue %dx.types.CBufRet.f32 %530, 2
  %534 = extractvalue %dx.types.CBufRet.f32 %530, 3
  %535 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %531, float %532, float %533, float %534)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %536 = fmul fast float %521, %23
  %537 = fmul fast float %528, %23
  %538 = fmul fast float %535, %23
  %539 = fadd fast float %536, %506
  %540 = fadd fast float %537, %507
  %541 = fadd fast float %538, %508
  %542 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %517, float %518, float %519)  ; Dot3(ax,ay,az,bx,by,bz)
  %543 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %524, float %525, float %526)  ; Dot3(ax,ay,az,bx,by,bz)
  %544 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %531, float %532, float %533)  ; Dot3(ax,ay,az,bx,by,bz)
  %545 = fmul fast float %542, %23
  %546 = fmul fast float %543, %23
  %547 = fmul fast float %544, %23
  %548 = fadd fast float %545, %509
  %549 = fadd fast float %546, %510
  %550 = fadd fast float %547, %511
  %551 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %517, float %518, float %519)  ; Dot3(ax,ay,az,bx,by,bz)
  %552 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %524, float %525, float %526)  ; Dot3(ax,ay,az,bx,by,bz)
  %553 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %531, float %532, float %533)  ; Dot3(ax,ay,az,bx,by,bz)
  %554 = fmul fast float %551, %23
  %555 = fmul fast float %552, %23
  %556 = fmul fast float %553, %23
  %557 = fadd fast float %554, %512
  %558 = fadd fast float %555, %513
  %559 = fadd fast float %556, %514
  %560 = add i32 %17, 4
  %561 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %560)  ; CBufferLoadLegacy(handle,regIndex)
  %562 = extractvalue %dx.types.CBufRet.f32 %561, 0
  %563 = extractvalue %dx.types.CBufRet.f32 %561, 1
  %564 = extractvalue %dx.types.CBufRet.f32 %561, 2
  %565 = extractvalue %dx.types.CBufRet.f32 %561, 3
  %566 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %562, float %563, float %564, float %565)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %567 = add i32 %17, 5
  %568 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %567)  ; CBufferLoadLegacy(handle,regIndex)
  %569 = extractvalue %dx.types.CBufRet.f32 %568, 0
  %570 = extractvalue %dx.types.CBufRet.f32 %568, 1
  %571 = extractvalue %dx.types.CBufRet.f32 %568, 2
  %572 = extractvalue %dx.types.CBufRet.f32 %568, 3
  %573 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %569, float %570, float %571, float %572)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %574 = add i32 %17, 6
  %575 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %574)  ; CBufferLoadLegacy(handle,regIndex)
  %576 = extractvalue %dx.types.CBufRet.f32 %575, 0
  %577 = extractvalue %dx.types.CBufRet.f32 %575, 1
  %578 = extractvalue %dx.types.CBufRet.f32 %575, 2
  %579 = extractvalue %dx.types.CBufRet.f32 %575, 3
  %580 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %576, float %577, float %578, float %579)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %581 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %562, float %563, float %564)  ; Dot3(ax,ay,az,bx,by,bz)
  %582 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %569, float %570, float %571)  ; Dot3(ax,ay,az,bx,by,bz)
  %583 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %576, float %577, float %578)  ; Dot3(ax,ay,az,bx,by,bz)
  %584 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %562, float %563, float %564)  ; Dot3(ax,ay,az,bx,by,bz)
  %585 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %569, float %570, float %571)  ; Dot3(ax,ay,az,bx,by,bz)
  %586 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %576, float %577, float %578)  ; Dot3(ax,ay,az,bx,by,bz)
  %587 = fsub fast float 1.000000e+00, %22
  %588 = fsub fast float %587, %23
  br i1 %65, label %654, label %589

; <label>:589                                     ; preds = %505
  %590 = fmul fast float %566, %24
  %591 = fmul fast float %573, %24
  %592 = fmul fast float %580, %24
  %593 = fadd fast float %539, %590
  %594 = fadd fast float %540, %591
  %595 = fadd fast float %541, %592
  %596 = fmul fast float %581, %24
  %597 = fmul fast float %582, %24
  %598 = fmul fast float %583, %24
  %599 = fadd fast float %548, %596
  %600 = fadd fast float %549, %597
  %601 = fadd fast float %550, %598
  %602 = fmul fast float %584, %24
  %603 = fmul fast float %585, %24
  %604 = fmul fast float %586, %24
  %605 = fadd fast float %557, %602
  %606 = fadd fast float %558, %603
  %607 = fadd fast float %559, %604
  %608 = fsub fast float %588, %24
  %609 = add i32 %18, 4
  %610 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %609)  ; CBufferLoadLegacy(handle,regIndex)
  %611 = extractvalue %dx.types.CBufRet.f32 %610, 0
  %612 = extractvalue %dx.types.CBufRet.f32 %610, 1
  %613 = extractvalue %dx.types.CBufRet.f32 %610, 2
  %614 = extractvalue %dx.types.CBufRet.f32 %610, 3
  %615 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %611, float %612, float %613, float %614)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %616 = add i32 %18, 5
  %617 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %616)  ; CBufferLoadLegacy(handle,regIndex)
  %618 = extractvalue %dx.types.CBufRet.f32 %617, 0
  %619 = extractvalue %dx.types.CBufRet.f32 %617, 1
  %620 = extractvalue %dx.types.CBufRet.f32 %617, 2
  %621 = extractvalue %dx.types.CBufRet.f32 %617, 3
  %622 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %618, float %619, float %620, float %621)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %623 = add i32 %18, 6
  %624 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %623)  ; CBufferLoadLegacy(handle,regIndex)
  %625 = extractvalue %dx.types.CBufRet.f32 %624, 0
  %626 = extractvalue %dx.types.CBufRet.f32 %624, 1
  %627 = extractvalue %dx.types.CBufRet.f32 %624, 2
  %628 = extractvalue %dx.types.CBufRet.f32 %624, 3
  %629 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %625, float %626, float %627, float %628)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %630 = fmul fast float %615, %608
  %631 = fmul fast float %622, %608
  %632 = fmul fast float %629, %608
  %633 = fadd fast float %593, %630
  %634 = fadd fast float %594, %631
  %635 = fadd fast float %595, %632
  %636 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %611, float %612, float %613)  ; Dot3(ax,ay,az,bx,by,bz)
  %637 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %618, float %619, float %620)  ; Dot3(ax,ay,az,bx,by,bz)
  %638 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %625, float %626, float %627)  ; Dot3(ax,ay,az,bx,by,bz)
  %639 = fmul fast float %636, %608
  %640 = fmul fast float %637, %608
  %641 = fmul fast float %638, %608
  %642 = fadd fast float %599, %639
  %643 = fadd fast float %600, %640
  %644 = fadd fast float %601, %641
  %645 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %611, float %612, float %613)  ; Dot3(ax,ay,az,bx,by,bz)
  %646 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %618, float %619, float %620)  ; Dot3(ax,ay,az,bx,by,bz)
  %647 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %625, float %626, float %627)  ; Dot3(ax,ay,az,bx,by,bz)
  %648 = fmul fast float %645, %608
  %649 = fmul fast float %646, %608
  %650 = fmul fast float %647, %608
  %651 = fadd fast float %605, %648
  %652 = fadd fast float %606, %649
  %653 = fadd fast float %607, %650
  br label %730

; <label>:654                                     ; preds = %505
  %655 = fmul fast float %566, %588
  %656 = fmul fast float %573, %588
  %657 = fmul fast float %580, %588
  %658 = fadd fast float %539, %655
  %659 = fadd fast float %540, %656
  %660 = fadd fast float %541, %657
  %661 = fmul fast float %581, %588
  %662 = fmul fast float %582, %588
  %663 = fmul fast float %583, %588
  %664 = fadd fast float %548, %661
  %665 = fadd fast float %549, %662
  %666 = fadd fast float %550, %663
  %667 = fmul fast float %584, %588
  %668 = fmul fast float %585, %588
  %669 = fmul fast float %586, %588
  %670 = fadd fast float %557, %667
  %671 = fadd fast float %558, %668
  %672 = fadd fast float %559, %669
  br label %730

; <label>:673                                     ; preds = %504
  br i1 %65, label %730, label %674

; <label>:674                                     ; preds = %673
  %675 = fmul fast float %39, %22
  %676 = fmul fast float %46, %22
  %677 = fmul fast float %53, %22
  %678 = fmul fast float %54, %22
  %679 = fmul fast float %55, %22
  %680 = fmul fast float %56, %22
  %681 = fmul fast float %57, %22
  %682 = fmul fast float %58, %22
  %683 = fmul fast float %59, %22
  %684 = fsub fast float 1.000000e+00, %22
  %685 = add i32 %16, 4
  %686 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %685)  ; CBufferLoadLegacy(handle,regIndex)
  %687 = extractvalue %dx.types.CBufRet.f32 %686, 0
  %688 = extractvalue %dx.types.CBufRet.f32 %686, 1
  %689 = extractvalue %dx.types.CBufRet.f32 %686, 2
  %690 = extractvalue %dx.types.CBufRet.f32 %686, 3
  %691 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %687, float %688, float %689, float %690)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %692 = add i32 %16, 5
  %693 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %692)  ; CBufferLoadLegacy(handle,regIndex)
  %694 = extractvalue %dx.types.CBufRet.f32 %693, 0
  %695 = extractvalue %dx.types.CBufRet.f32 %693, 1
  %696 = extractvalue %dx.types.CBufRet.f32 %693, 2
  %697 = extractvalue %dx.types.CBufRet.f32 %693, 3
  %698 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %694, float %695, float %696, float %697)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %699 = add i32 %16, 6
  %700 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %699)  ; CBufferLoadLegacy(handle,regIndex)
  %701 = extractvalue %dx.types.CBufRet.f32 %700, 0
  %702 = extractvalue %dx.types.CBufRet.f32 %700, 1
  %703 = extractvalue %dx.types.CBufRet.f32 %700, 2
  %704 = extractvalue %dx.types.CBufRet.f32 %700, 3
  %705 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %701, float %702, float %703, float %704)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %706 = fmul fast float %691, %684
  %707 = fmul fast float %698, %684
  %708 = fmul fast float %705, %684
  %709 = fadd fast float %706, %675
  %710 = fadd fast float %707, %676
  %711 = fadd fast float %708, %677
  %712 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %687, float %688, float %689)  ; Dot3(ax,ay,az,bx,by,bz)
  %713 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %694, float %695, float %696)  ; Dot3(ax,ay,az,bx,by,bz)
  %714 = call float @dx.op.dot3.f32(i32 55, float %8, float %9, float %10, float %701, float %702, float %703)  ; Dot3(ax,ay,az,bx,by,bz)
  %715 = fmul fast float %712, %684
  %716 = fmul fast float %713, %684
  %717 = fmul fast float %714, %684
  %718 = fadd fast float %715, %678
  %719 = fadd fast float %716, %679
  %720 = fadd fast float %717, %680
  %721 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %687, float %688, float %689)  ; Dot3(ax,ay,az,bx,by,bz)
  %722 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %694, float %695, float %696)  ; Dot3(ax,ay,az,bx,by,bz)
  %723 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %701, float %702, float %703)  ; Dot3(ax,ay,az,bx,by,bz)
  %724 = fmul fast float %721, %684
  %725 = fmul fast float %722, %684
  %726 = fmul fast float %723, %684
  %727 = fadd fast float %724, %681
  %728 = fadd fast float %725, %682
  %729 = fadd fast float %726, %683
  br label %730

; <label>:730                                     ; preds = %674, %673, %654, %589, %485, %420, %398, %332
  %731 = phi float [ %395, %332 ], [ %415, %398 ], [ %482, %420 ], [ %501, %485 ], [ %651, %589 ], [ %670, %654 ], [ %727, %674 ], [ %57, %673 ]
  %732 = phi float [ %396, %332 ], [ %416, %398 ], [ %483, %420 ], [ %502, %485 ], [ %652, %589 ], [ %671, %654 ], [ %728, %674 ], [ %58, %673 ]
  %733 = phi float [ %397, %332 ], [ %417, %398 ], [ %484, %420 ], [ %503, %485 ], [ %653, %589 ], [ %672, %654 ], [ %729, %674 ], [ %59, %673 ]
  %734 = phi float [ %386, %332 ], [ %409, %398 ], [ %473, %420 ], [ %495, %485 ], [ %642, %589 ], [ %664, %654 ], [ %718, %674 ], [ %54, %673 ]
  %735 = phi float [ %387, %332 ], [ %410, %398 ], [ %474, %420 ], [ %496, %485 ], [ %643, %589 ], [ %665, %654 ], [ %719, %674 ], [ %55, %673 ]
  %736 = phi float [ %388, %332 ], [ %411, %398 ], [ %475, %420 ], [ %497, %485 ], [ %644, %589 ], [ %666, %654 ], [ %720, %674 ], [ %56, %673 ]
  %737 = phi float [ %377, %332 ], [ %403, %398 ], [ %464, %420 ], [ %489, %485 ], [ %633, %589 ], [ %658, %654 ], [ %709, %674 ], [ %39, %673 ]
  %738 = phi float [ %378, %332 ], [ %404, %398 ], [ %465, %420 ], [ %490, %485 ], [ %634, %589 ], [ %659, %654 ], [ %710, %674 ], [ %46, %673 ]
  %739 = phi float [ %379, %332 ], [ %405, %398 ], [ %466, %420 ], [ %491, %485 ], [ %635, %589 ], [ %660, %654 ], [ %711, %674 ], [ %53, %673 ]
  %740 = call float @dx.op.dot3.f32(i32 55, float %734, float %735, float %736, float %734, float %735, float %736)  ; Dot3(ax,ay,az,bx,by,bz)
  %741 = call float @dx.op.unary.f32(i32 25, float %740)  ; Rsqrt(value)
  %742 = fmul fast float %741, %734
  %743 = fmul fast float %741, %735
  %744 = fmul fast float %741, %736
  %745 = call float @dx.op.dot3.f32(i32 55, float %731, float %732, float %733, float %731, float %732, float %733)  ; Dot3(ax,ay,az,bx,by,bz)
  %746 = call float @dx.op.unary.f32(i32 25, float %745)  ; Rsqrt(value)
  %747 = fmul fast float %746, %731
  %748 = fmul fast float %746, %732
  %749 = fmul fast float %746, %733
  %750 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %751 = extractvalue %dx.types.CBufRet.f32 %750, 0
  %752 = extractvalue %dx.types.CBufRet.f32 %750, 1
  %753 = extractvalue %dx.types.CBufRet.f32 %750, 2
  %754 = extractvalue %dx.types.CBufRet.f32 %750, 3
  %755 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %756 = extractvalue %dx.types.CBufRet.f32 %755, 0
  %757 = extractvalue %dx.types.CBufRet.f32 %755, 1
  %758 = extractvalue %dx.types.CBufRet.f32 %755, 2
  %759 = extractvalue %dx.types.CBufRet.f32 %755, 3
  %760 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %761 = extractvalue %dx.types.CBufRet.f32 %760, 0
  %762 = extractvalue %dx.types.CBufRet.f32 %760, 1
  %763 = extractvalue %dx.types.CBufRet.f32 %760, 2
  %764 = extractvalue %dx.types.CBufRet.f32 %760, 3
  %765 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %766 = extractvalue %dx.types.CBufRet.f32 %765, 0
  %767 = extractvalue %dx.types.CBufRet.f32 %765, 1
  %768 = extractvalue %dx.types.CBufRet.f32 %765, 2
  %769 = extractvalue %dx.types.CBufRet.f32 %765, 3
  %770 = fmul fast float %751, %737
  %771 = call float @dx.op.tertiary.f32(i32 46, float %738, float %756, float %770)  ; FMad(a,b,c)
  %772 = call float @dx.op.tertiary.f32(i32 46, float %739, float %761, float %771)  ; FMad(a,b,c)
  %773 = fadd fast float %772, %766
  %774 = fmul fast float %752, %737
  %775 = call float @dx.op.tertiary.f32(i32 46, float %738, float %757, float %774)  ; FMad(a,b,c)
  %776 = call float @dx.op.tertiary.f32(i32 46, float %739, float %762, float %775)  ; FMad(a,b,c)
  %777 = fadd fast float %776, %767
  %778 = fmul fast float %753, %737
  %779 = call float @dx.op.tertiary.f32(i32 46, float %738, float %758, float %778)  ; FMad(a,b,c)
  %780 = call float @dx.op.tertiary.f32(i32 46, float %739, float %763, float %779)  ; FMad(a,b,c)
  %781 = fadd fast float %780, %768
  %782 = fmul fast float %754, %737
  %783 = call float @dx.op.tertiary.f32(i32 46, float %738, float %759, float %782)  ; FMad(a,b,c)
  %784 = call float @dx.op.tertiary.f32(i32 46, float %739, float %764, float %783)  ; FMad(a,b,c)
  %785 = fadd fast float %784, %769
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %737)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %738)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %739)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 0, float %742)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 1, float %743)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 2, float %744)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 0, float %747)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 1, float %748)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 2, float %749)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %773)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %777)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %781)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %785)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 6, i32 0, i8 0, float %6)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 6, i32 0, i8 1, float %7)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  %786 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 774)  ; CBufferLoadLegacy(handle,regIndex)
  %787 = extractvalue %dx.types.CBufRet.i32 %786, 0
  %788 = icmp eq i32 %787, 0
  %789 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 779)  ; CBufferLoadLegacy(handle,regIndex)
  %790 = extractvalue %dx.types.CBufRet.i32 %789, 0
  %791 = mul i32 %790, 3
  %792 = add i32 %791, %15
  %793 = add i32 %791, %16
  %794 = add i32 %791, %17
  %795 = add i32 %791, %18
  %796 = add i32 %792, 4
  %797 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %796)  ; CBufferLoadLegacy(handle,regIndex)
  %798 = extractvalue %dx.types.CBufRet.f32 %797, 0
  %799 = extractvalue %dx.types.CBufRet.f32 %797, 1
  %800 = extractvalue %dx.types.CBufRet.f32 %797, 2
  %801 = extractvalue %dx.types.CBufRet.f32 %797, 3
  %802 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %798, float %799, float %800, float %801)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %803 = add i32 %792, 5
  %804 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %803)  ; CBufferLoadLegacy(handle,regIndex)
  %805 = extractvalue %dx.types.CBufRet.f32 %804, 0
  %806 = extractvalue %dx.types.CBufRet.f32 %804, 1
  %807 = extractvalue %dx.types.CBufRet.f32 %804, 2
  %808 = extractvalue %dx.types.CBufRet.f32 %804, 3
  %809 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %805, float %806, float %807, float %808)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %810 = add i32 %792, 6
  %811 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %810)  ; CBufferLoadLegacy(handle,regIndex)
  %812 = extractvalue %dx.types.CBufRet.f32 %811, 0
  %813 = extractvalue %dx.types.CBufRet.f32 %811, 1
  %814 = extractvalue %dx.types.CBufRet.f32 %811, 2
  %815 = extractvalue %dx.types.CBufRet.f32 %811, 3
  %816 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %812, float %813, float %814, float %815)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %817 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 772)  ; CBufferLoadLegacy(handle,regIndex)
  %818 = extractvalue %dx.types.CBufRet.i32 %817, 0
  %819 = icmp eq i32 %818, 0
  %820 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 773)  ; CBufferLoadLegacy(handle,regIndex)
  %821 = extractvalue %dx.types.CBufRet.i32 %820, 0
  %822 = icmp eq i32 %821, 0
  br i1 %788, label %1079, label %823

; <label>:823                                     ; preds = %730
  %824 = fmul fast float %802, %22
  %825 = fmul fast float %809, %22
  %826 = fmul fast float %816, %22
  %827 = add i32 %793, 4
  %828 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %827)  ; CBufferLoadLegacy(handle,regIndex)
  %829 = extractvalue %dx.types.CBufRet.f32 %828, 0
  %830 = extractvalue %dx.types.CBufRet.f32 %828, 1
  %831 = extractvalue %dx.types.CBufRet.f32 %828, 2
  %832 = extractvalue %dx.types.CBufRet.f32 %828, 3
  %833 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %829, float %830, float %831, float %832)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %834 = add i32 %793, 5
  %835 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %834)  ; CBufferLoadLegacy(handle,regIndex)
  %836 = extractvalue %dx.types.CBufRet.f32 %835, 0
  %837 = extractvalue %dx.types.CBufRet.f32 %835, 1
  %838 = extractvalue %dx.types.CBufRet.f32 %835, 2
  %839 = extractvalue %dx.types.CBufRet.f32 %835, 3
  %840 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %836, float %837, float %838, float %839)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %841 = add i32 %793, 6
  %842 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %841)  ; CBufferLoadLegacy(handle,regIndex)
  %843 = extractvalue %dx.types.CBufRet.f32 %842, 0
  %844 = extractvalue %dx.types.CBufRet.f32 %842, 1
  %845 = extractvalue %dx.types.CBufRet.f32 %842, 2
  %846 = extractvalue %dx.types.CBufRet.f32 %842, 3
  %847 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %843, float %844, float %845, float %846)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %848 = fmul fast float %833, %23
  %849 = fmul fast float %840, %23
  %850 = fmul fast float %847, %23
  %851 = fadd fast float %848, %824
  %852 = fadd fast float %849, %825
  %853 = fadd fast float %850, %826
  %854 = add i32 %794, 4
  %855 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %854)  ; CBufferLoadLegacy(handle,regIndex)
  %856 = extractvalue %dx.types.CBufRet.f32 %855, 0
  %857 = extractvalue %dx.types.CBufRet.f32 %855, 1
  %858 = extractvalue %dx.types.CBufRet.f32 %855, 2
  %859 = extractvalue %dx.types.CBufRet.f32 %855, 3
  %860 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %856, float %857, float %858, float %859)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %861 = add i32 %794, 5
  %862 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %861)  ; CBufferLoadLegacy(handle,regIndex)
  %863 = extractvalue %dx.types.CBufRet.f32 %862, 0
  %864 = extractvalue %dx.types.CBufRet.f32 %862, 1
  %865 = extractvalue %dx.types.CBufRet.f32 %862, 2
  %866 = extractvalue %dx.types.CBufRet.f32 %862, 3
  %867 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %863, float %864, float %865, float %866)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %868 = add i32 %794, 6
  %869 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %868)  ; CBufferLoadLegacy(handle,regIndex)
  %870 = extractvalue %dx.types.CBufRet.f32 %869, 0
  %871 = extractvalue %dx.types.CBufRet.f32 %869, 1
  %872 = extractvalue %dx.types.CBufRet.f32 %869, 2
  %873 = extractvalue %dx.types.CBufRet.f32 %869, 3
  %874 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %870, float %871, float %872, float %873)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %875 = fmul fast float %860, %24
  %876 = fmul fast float %867, %24
  %877 = fmul fast float %874, %24
  %878 = fadd fast float %851, %875
  %879 = fadd fast float %852, %876
  %880 = fadd fast float %853, %877
  %881 = add i32 %795, 4
  %882 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %881)  ; CBufferLoadLegacy(handle,regIndex)
  %883 = extractvalue %dx.types.CBufRet.f32 %882, 0
  %884 = extractvalue %dx.types.CBufRet.f32 %882, 1
  %885 = extractvalue %dx.types.CBufRet.f32 %882, 2
  %886 = extractvalue %dx.types.CBufRet.f32 %882, 3
  %887 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %883, float %884, float %885, float %886)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %888 = add i32 %795, 5
  %889 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %888)  ; CBufferLoadLegacy(handle,regIndex)
  %890 = extractvalue %dx.types.CBufRet.f32 %889, 0
  %891 = extractvalue %dx.types.CBufRet.f32 %889, 1
  %892 = extractvalue %dx.types.CBufRet.f32 %889, 2
  %893 = extractvalue %dx.types.CBufRet.f32 %889, 3
  %894 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %890, float %891, float %892, float %893)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %895 = add i32 %795, 6
  %896 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %895)  ; CBufferLoadLegacy(handle,regIndex)
  %897 = extractvalue %dx.types.CBufRet.f32 %896, 0
  %898 = extractvalue %dx.types.CBufRet.f32 %896, 1
  %899 = extractvalue %dx.types.CBufRet.f32 %896, 2
  %900 = extractvalue %dx.types.CBufRet.f32 %896, 3
  %901 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %897, float %898, float %899, float %900)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %902 = fmul fast float %887, %25
  %903 = fmul fast float %894, %25
  %904 = fmul fast float %901, %25
  %905 = fadd fast float %878, %902
  %906 = fadd fast float %879, %903
  %907 = fadd fast float %880, %904
  %908 = call float @dx.op.dot4.f32(i32 56, float %22, float %23, float %24, float %25, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %909 = add i32 %791, %11
  %910 = add i32 %791, %12
  %911 = add i32 %791, %13
  %912 = add i32 %791, %14
  %913 = add i32 %909, 4
  %914 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %913)  ; CBufferLoadLegacy(handle,regIndex)
  %915 = extractvalue %dx.types.CBufRet.f32 %914, 0
  %916 = extractvalue %dx.types.CBufRet.f32 %914, 1
  %917 = extractvalue %dx.types.CBufRet.f32 %914, 2
  %918 = extractvalue %dx.types.CBufRet.f32 %914, 3
  %919 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %915, float %916, float %917, float %918)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %920 = add i32 %909, 5
  %921 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %920)  ; CBufferLoadLegacy(handle,regIndex)
  %922 = extractvalue %dx.types.CBufRet.f32 %921, 0
  %923 = extractvalue %dx.types.CBufRet.f32 %921, 1
  %924 = extractvalue %dx.types.CBufRet.f32 %921, 2
  %925 = extractvalue %dx.types.CBufRet.f32 %921, 3
  %926 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %922, float %923, float %924, float %925)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %927 = add i32 %909, 6
  %928 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %927)  ; CBufferLoadLegacy(handle,regIndex)
  %929 = extractvalue %dx.types.CBufRet.f32 %928, 0
  %930 = extractvalue %dx.types.CBufRet.f32 %928, 1
  %931 = extractvalue %dx.types.CBufRet.f32 %928, 2
  %932 = extractvalue %dx.types.CBufRet.f32 %928, 3
  %933 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %929, float %930, float %931, float %932)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  br i1 %819, label %1035, label %934

; <label>:934                                     ; preds = %823
  %935 = fmul fast float %919, %19
  %936 = fmul fast float %926, %19
  %937 = fmul fast float %933, %19
  %938 = fadd fast float %935, %905
  %939 = fadd fast float %936, %906
  %940 = fadd fast float %937, %907
  %941 = add i32 %910, 4
  %942 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %941)  ; CBufferLoadLegacy(handle,regIndex)
  %943 = extractvalue %dx.types.CBufRet.f32 %942, 0
  %944 = extractvalue %dx.types.CBufRet.f32 %942, 1
  %945 = extractvalue %dx.types.CBufRet.f32 %942, 2
  %946 = extractvalue %dx.types.CBufRet.f32 %942, 3
  %947 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %943, float %944, float %945, float %946)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %948 = add i32 %910, 5
  %949 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %948)  ; CBufferLoadLegacy(handle,regIndex)
  %950 = extractvalue %dx.types.CBufRet.f32 %949, 0
  %951 = extractvalue %dx.types.CBufRet.f32 %949, 1
  %952 = extractvalue %dx.types.CBufRet.f32 %949, 2
  %953 = extractvalue %dx.types.CBufRet.f32 %949, 3
  %954 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %950, float %951, float %952, float %953)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %955 = add i32 %910, 6
  %956 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %955)  ; CBufferLoadLegacy(handle,regIndex)
  %957 = extractvalue %dx.types.CBufRet.f32 %956, 0
  %958 = extractvalue %dx.types.CBufRet.f32 %956, 1
  %959 = extractvalue %dx.types.CBufRet.f32 %956, 2
  %960 = extractvalue %dx.types.CBufRet.f32 %956, 3
  %961 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %957, float %958, float %959, float %960)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %962 = fmul fast float %947, %20
  %963 = fmul fast float %954, %20
  %964 = fmul fast float %961, %20
  %965 = fadd fast float %962, %938
  %966 = fadd fast float %963, %939
  %967 = fadd fast float %964, %940
  %968 = add i32 %911, 4
  %969 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %968)  ; CBufferLoadLegacy(handle,regIndex)
  %970 = extractvalue %dx.types.CBufRet.f32 %969, 0
  %971 = extractvalue %dx.types.CBufRet.f32 %969, 1
  %972 = extractvalue %dx.types.CBufRet.f32 %969, 2
  %973 = extractvalue %dx.types.CBufRet.f32 %969, 3
  %974 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %970, float %971, float %972, float %973)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %975 = add i32 %911, 5
  %976 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %975)  ; CBufferLoadLegacy(handle,regIndex)
  %977 = extractvalue %dx.types.CBufRet.f32 %976, 0
  %978 = extractvalue %dx.types.CBufRet.f32 %976, 1
  %979 = extractvalue %dx.types.CBufRet.f32 %976, 2
  %980 = extractvalue %dx.types.CBufRet.f32 %976, 3
  %981 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %977, float %978, float %979, float %980)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %982 = add i32 %911, 6
  %983 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %982)  ; CBufferLoadLegacy(handle,regIndex)
  %984 = extractvalue %dx.types.CBufRet.f32 %983, 0
  %985 = extractvalue %dx.types.CBufRet.f32 %983, 1
  %986 = extractvalue %dx.types.CBufRet.f32 %983, 2
  %987 = extractvalue %dx.types.CBufRet.f32 %983, 3
  %988 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %984, float %985, float %986, float %987)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %989 = fsub fast float 1.000000e+00, %19
  %990 = fsub fast float %989, %20
  br i1 %822, label %1027, label %991

; <label>:991                                     ; preds = %934
  %992 = fmul fast float %974, %21
  %993 = fmul fast float %981, %21
  %994 = fmul fast float %988, %21
  %995 = fadd fast float %965, %992
  %996 = fadd fast float %966, %993
  %997 = fadd fast float %967, %994
  %998 = fsub fast float %990, %21
  %999 = fsub fast float %998, %908
  %1000 = add i32 %912, 4
  %1001 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1000)  ; CBufferLoadLegacy(handle,regIndex)
  %1002 = extractvalue %dx.types.CBufRet.f32 %1001, 0
  %1003 = extractvalue %dx.types.CBufRet.f32 %1001, 1
  %1004 = extractvalue %dx.types.CBufRet.f32 %1001, 2
  %1005 = extractvalue %dx.types.CBufRet.f32 %1001, 3
  %1006 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1002, float %1003, float %1004, float %1005)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1007 = add i32 %912, 5
  %1008 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1007)  ; CBufferLoadLegacy(handle,regIndex)
  %1009 = extractvalue %dx.types.CBufRet.f32 %1008, 0
  %1010 = extractvalue %dx.types.CBufRet.f32 %1008, 1
  %1011 = extractvalue %dx.types.CBufRet.f32 %1008, 2
  %1012 = extractvalue %dx.types.CBufRet.f32 %1008, 3
  %1013 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1009, float %1010, float %1011, float %1012)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1014 = add i32 %912, 6
  %1015 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1014)  ; CBufferLoadLegacy(handle,regIndex)
  %1016 = extractvalue %dx.types.CBufRet.f32 %1015, 0
  %1017 = extractvalue %dx.types.CBufRet.f32 %1015, 1
  %1018 = extractvalue %dx.types.CBufRet.f32 %1015, 2
  %1019 = extractvalue %dx.types.CBufRet.f32 %1015, 3
  %1020 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1016, float %1017, float %1018, float %1019)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1021 = fmul fast float %1006, %999
  %1022 = fmul fast float %1013, %999
  %1023 = fmul fast float %1020, %999
  %1024 = fadd fast float %995, %1021
  %1025 = fadd fast float %996, %1022
  %1026 = fadd fast float %997, %1023
  br label %1209

; <label>:1027                                    ; preds = %934
  %1028 = fsub fast float %990, %908
  %1029 = fmul fast float %974, %1028
  %1030 = fmul fast float %981, %1028
  %1031 = fmul fast float %988, %1028
  %1032 = fadd fast float %965, %1029
  %1033 = fadd fast float %966, %1030
  %1034 = fadd fast float %967, %1031
  br label %1209

; <label>:1035                                    ; preds = %823
  %1036 = fsub fast float 1.000000e+00, %908
  br i1 %822, label %1072, label %1037

; <label>:1037                                    ; preds = %1035
  %1038 = fmul fast float %919, %19
  %1039 = fmul fast float %926, %19
  %1040 = fmul fast float %933, %19
  %1041 = fadd fast float %1038, %905
  %1042 = fadd fast float %1039, %906
  %1043 = fadd fast float %1040, %907
  %1044 = fsub fast float %1036, %19
  %1045 = add i32 %910, 4
  %1046 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1045)  ; CBufferLoadLegacy(handle,regIndex)
  %1047 = extractvalue %dx.types.CBufRet.f32 %1046, 0
  %1048 = extractvalue %dx.types.CBufRet.f32 %1046, 1
  %1049 = extractvalue %dx.types.CBufRet.f32 %1046, 2
  %1050 = extractvalue %dx.types.CBufRet.f32 %1046, 3
  %1051 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1047, float %1048, float %1049, float %1050)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1052 = add i32 %910, 5
  %1053 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1052)  ; CBufferLoadLegacy(handle,regIndex)
  %1054 = extractvalue %dx.types.CBufRet.f32 %1053, 0
  %1055 = extractvalue %dx.types.CBufRet.f32 %1053, 1
  %1056 = extractvalue %dx.types.CBufRet.f32 %1053, 2
  %1057 = extractvalue %dx.types.CBufRet.f32 %1053, 3
  %1058 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1054, float %1055, float %1056, float %1057)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1059 = add i32 %910, 6
  %1060 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1059)  ; CBufferLoadLegacy(handle,regIndex)
  %1061 = extractvalue %dx.types.CBufRet.f32 %1060, 0
  %1062 = extractvalue %dx.types.CBufRet.f32 %1060, 1
  %1063 = extractvalue %dx.types.CBufRet.f32 %1060, 2
  %1064 = extractvalue %dx.types.CBufRet.f32 %1060, 3
  %1065 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1061, float %1062, float %1063, float %1064)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1066 = fmul fast float %1051, %1044
  %1067 = fmul fast float %1058, %1044
  %1068 = fmul fast float %1065, %1044
  %1069 = fadd fast float %1041, %1066
  %1070 = fadd fast float %1042, %1067
  %1071 = fadd fast float %1043, %1068
  br label %1209

; <label>:1072                                    ; preds = %1035
  %1073 = fmul fast float %919, %1036
  %1074 = fmul fast float %926, %1036
  %1075 = fmul fast float %933, %1036
  %1076 = fadd fast float %1073, %905
  %1077 = fadd fast float %1074, %906
  %1078 = fadd fast float %1075, %907
  br label %1209

; <label>:1079                                    ; preds = %730
  br i1 %819, label %1176, label %1080

; <label>:1080                                    ; preds = %1079
  %1081 = fmul fast float %802, %22
  %1082 = fmul fast float %809, %22
  %1083 = fmul fast float %816, %22
  %1084 = add i32 %793, 4
  %1085 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1084)  ; CBufferLoadLegacy(handle,regIndex)
  %1086 = extractvalue %dx.types.CBufRet.f32 %1085, 0
  %1087 = extractvalue %dx.types.CBufRet.f32 %1085, 1
  %1088 = extractvalue %dx.types.CBufRet.f32 %1085, 2
  %1089 = extractvalue %dx.types.CBufRet.f32 %1085, 3
  %1090 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1086, float %1087, float %1088, float %1089)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1091 = add i32 %793, 5
  %1092 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1091)  ; CBufferLoadLegacy(handle,regIndex)
  %1093 = extractvalue %dx.types.CBufRet.f32 %1092, 0
  %1094 = extractvalue %dx.types.CBufRet.f32 %1092, 1
  %1095 = extractvalue %dx.types.CBufRet.f32 %1092, 2
  %1096 = extractvalue %dx.types.CBufRet.f32 %1092, 3
  %1097 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1093, float %1094, float %1095, float %1096)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1098 = add i32 %793, 6
  %1099 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1098)  ; CBufferLoadLegacy(handle,regIndex)
  %1100 = extractvalue %dx.types.CBufRet.f32 %1099, 0
  %1101 = extractvalue %dx.types.CBufRet.f32 %1099, 1
  %1102 = extractvalue %dx.types.CBufRet.f32 %1099, 2
  %1103 = extractvalue %dx.types.CBufRet.f32 %1099, 3
  %1104 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1100, float %1101, float %1102, float %1103)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1105 = fmul fast float %1090, %23
  %1106 = fmul fast float %1097, %23
  %1107 = fmul fast float %1104, %23
  %1108 = fadd fast float %1105, %1081
  %1109 = fadd fast float %1106, %1082
  %1110 = fadd fast float %1107, %1083
  %1111 = add i32 %794, 4
  %1112 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1111)  ; CBufferLoadLegacy(handle,regIndex)
  %1113 = extractvalue %dx.types.CBufRet.f32 %1112, 0
  %1114 = extractvalue %dx.types.CBufRet.f32 %1112, 1
  %1115 = extractvalue %dx.types.CBufRet.f32 %1112, 2
  %1116 = extractvalue %dx.types.CBufRet.f32 %1112, 3
  %1117 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1113, float %1114, float %1115, float %1116)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1118 = add i32 %794, 5
  %1119 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1118)  ; CBufferLoadLegacy(handle,regIndex)
  %1120 = extractvalue %dx.types.CBufRet.f32 %1119, 0
  %1121 = extractvalue %dx.types.CBufRet.f32 %1119, 1
  %1122 = extractvalue %dx.types.CBufRet.f32 %1119, 2
  %1123 = extractvalue %dx.types.CBufRet.f32 %1119, 3
  %1124 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1120, float %1121, float %1122, float %1123)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1125 = add i32 %794, 6
  %1126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1125)  ; CBufferLoadLegacy(handle,regIndex)
  %1127 = extractvalue %dx.types.CBufRet.f32 %1126, 0
  %1128 = extractvalue %dx.types.CBufRet.f32 %1126, 1
  %1129 = extractvalue %dx.types.CBufRet.f32 %1126, 2
  %1130 = extractvalue %dx.types.CBufRet.f32 %1126, 3
  %1131 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1127, float %1128, float %1129, float %1130)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1132 = fsub fast float 1.000000e+00, %22
  %1133 = fsub fast float %1132, %23
  br i1 %822, label %1169, label %1134

; <label>:1134                                    ; preds = %1080
  %1135 = fmul fast float %1117, %24
  %1136 = fmul fast float %1124, %24
  %1137 = fmul fast float %1131, %24
  %1138 = fadd fast float %1108, %1135
  %1139 = fadd fast float %1109, %1136
  %1140 = fadd fast float %1110, %1137
  %1141 = fsub fast float %1133, %24
  %1142 = add i32 %795, 4
  %1143 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1142)  ; CBufferLoadLegacy(handle,regIndex)
  %1144 = extractvalue %dx.types.CBufRet.f32 %1143, 0
  %1145 = extractvalue %dx.types.CBufRet.f32 %1143, 1
  %1146 = extractvalue %dx.types.CBufRet.f32 %1143, 2
  %1147 = extractvalue %dx.types.CBufRet.f32 %1143, 3
  %1148 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1144, float %1145, float %1146, float %1147)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1149 = add i32 %795, 5
  %1150 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1149)  ; CBufferLoadLegacy(handle,regIndex)
  %1151 = extractvalue %dx.types.CBufRet.f32 %1150, 0
  %1152 = extractvalue %dx.types.CBufRet.f32 %1150, 1
  %1153 = extractvalue %dx.types.CBufRet.f32 %1150, 2
  %1154 = extractvalue %dx.types.CBufRet.f32 %1150, 3
  %1155 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1151, float %1152, float %1153, float %1154)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1156 = add i32 %795, 6
  %1157 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1156)  ; CBufferLoadLegacy(handle,regIndex)
  %1158 = extractvalue %dx.types.CBufRet.f32 %1157, 0
  %1159 = extractvalue %dx.types.CBufRet.f32 %1157, 1
  %1160 = extractvalue %dx.types.CBufRet.f32 %1157, 2
  %1161 = extractvalue %dx.types.CBufRet.f32 %1157, 3
  %1162 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1158, float %1159, float %1160, float %1161)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1163 = fmul fast float %1148, %1141
  %1164 = fmul fast float %1155, %1141
  %1165 = fmul fast float %1162, %1141
  %1166 = fadd fast float %1138, %1163
  %1167 = fadd fast float %1139, %1164
  %1168 = fadd fast float %1140, %1165
  br label %1209

; <label>:1169                                    ; preds = %1080
  %1170 = fmul fast float %1117, %1133
  %1171 = fmul fast float %1124, %1133
  %1172 = fmul fast float %1131, %1133
  %1173 = fadd fast float %1108, %1170
  %1174 = fadd fast float %1109, %1171
  %1175 = fadd fast float %1110, %1172
  br label %1209

; <label>:1176                                    ; preds = %1079
  br i1 %822, label %1209, label %1177

; <label>:1177                                    ; preds = %1176
  %1178 = fmul fast float %802, %22
  %1179 = fmul fast float %809, %22
  %1180 = fmul fast float %816, %22
  %1181 = fsub fast float 1.000000e+00, %22
  %1182 = add i32 %793, 4
  %1183 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1182)  ; CBufferLoadLegacy(handle,regIndex)
  %1184 = extractvalue %dx.types.CBufRet.f32 %1183, 0
  %1185 = extractvalue %dx.types.CBufRet.f32 %1183, 1
  %1186 = extractvalue %dx.types.CBufRet.f32 %1183, 2
  %1187 = extractvalue %dx.types.CBufRet.f32 %1183, 3
  %1188 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1184, float %1185, float %1186, float %1187)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1189 = add i32 %793, 5
  %1190 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1189)  ; CBufferLoadLegacy(handle,regIndex)
  %1191 = extractvalue %dx.types.CBufRet.f32 %1190, 0
  %1192 = extractvalue %dx.types.CBufRet.f32 %1190, 1
  %1193 = extractvalue %dx.types.CBufRet.f32 %1190, 2
  %1194 = extractvalue %dx.types.CBufRet.f32 %1190, 3
  %1195 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1191, float %1192, float %1193, float %1194)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1196 = add i32 %793, 6
  %1197 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 %1196)  ; CBufferLoadLegacy(handle,regIndex)
  %1198 = extractvalue %dx.types.CBufRet.f32 %1197, 0
  %1199 = extractvalue %dx.types.CBufRet.f32 %1197, 1
  %1200 = extractvalue %dx.types.CBufRet.f32 %1197, 2
  %1201 = extractvalue %dx.types.CBufRet.f32 %1197, 3
  %1202 = call float @dx.op.dot4.f32(i32 56, float %26, float %27, float %28, float %29, float %1198, float %1199, float %1200, float %1201)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1203 = fmul fast float %1188, %1181
  %1204 = fmul fast float %1195, %1181
  %1205 = fmul fast float %1202, %1181
  %1206 = fadd fast float %1203, %1178
  %1207 = fadd fast float %1204, %1179
  %1208 = fadd fast float %1205, %1180
  br label %1209

; <label>:1209                                    ; preds = %1177, %1176, %1169, %1134, %1072, %1037, %1027, %991
  %1210 = phi float [ %1024, %991 ], [ %1032, %1027 ], [ %1069, %1037 ], [ %1076, %1072 ], [ %1166, %1134 ], [ %1173, %1169 ], [ %1206, %1177 ], [ %802, %1176 ]
  %1211 = phi float [ %1025, %991 ], [ %1033, %1027 ], [ %1070, %1037 ], [ %1077, %1072 ], [ %1167, %1134 ], [ %1174, %1169 ], [ %1207, %1177 ], [ %809, %1176 ]
  %1212 = phi float [ %1026, %991 ], [ %1034, %1027 ], [ %1071, %1037 ], [ %1078, %1072 ], [ %1168, %1134 ], [ %1175, %1169 ], [ %1208, %1177 ], [ %816, %1176 ]
  %1213 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 775)  ; CBufferLoadLegacy(handle,regIndex)
  %1214 = extractvalue %dx.types.CBufRet.f32 %1213, 0
  %1215 = extractvalue %dx.types.CBufRet.f32 %1213, 1
  %1216 = extractvalue %dx.types.CBufRet.f32 %1213, 3
  %1217 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 776)  ; CBufferLoadLegacy(handle,regIndex)
  %1218 = extractvalue %dx.types.CBufRet.f32 %1217, 0
  %1219 = extractvalue %dx.types.CBufRet.f32 %1217, 1
  %1220 = extractvalue %dx.types.CBufRet.f32 %1217, 3
  %1221 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 777)  ; CBufferLoadLegacy(handle,regIndex)
  %1222 = extractvalue %dx.types.CBufRet.f32 %1221, 0
  %1223 = extractvalue %dx.types.CBufRet.f32 %1221, 1
  %1224 = extractvalue %dx.types.CBufRet.f32 %1221, 3
  %1225 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 778)  ; CBufferLoadLegacy(handle,regIndex)
  %1226 = extractvalue %dx.types.CBufRet.f32 %1225, 0
  %1227 = extractvalue %dx.types.CBufRet.f32 %1225, 1
  %1228 = extractvalue %dx.types.CBufRet.f32 %1225, 3
  %1229 = fmul fast float %1214, %1210
  %1230 = call float @dx.op.tertiary.f32(i32 46, float %1211, float %1218, float %1229)  ; FMad(a,b,c)
  %1231 = call float @dx.op.tertiary.f32(i32 46, float %1212, float %1222, float %1230)  ; FMad(a,b,c)
  %1232 = fadd fast float %1231, %1226
  %1233 = fmul fast float %1215, %1210
  %1234 = call float @dx.op.tertiary.f32(i32 46, float %1211, float %1219, float %1233)  ; FMad(a,b,c)
  %1235 = call float @dx.op.tertiary.f32(i32 46, float %1212, float %1223, float %1234)  ; FMad(a,b,c)
  %1236 = fadd fast float %1235, %1227
  %1237 = fmul fast float %1216, %1210
  %1238 = call float @dx.op.tertiary.f32(i32 46, float %1211, float %1220, float %1237)  ; FMad(a,b,c)
  %1239 = call float @dx.op.tertiary.f32(i32 46, float %1212, float %1224, float %1238)  ; FMad(a,b,c)
  %1240 = fadd fast float %1239, %1228
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %773)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %777)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %781)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %785)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %1232)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %1236)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %1240)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 3, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
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
!5 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 12480, null}
!6 = !{[34 x i32] [i32 32, i32 26, i32 30719, i32 30719, i32 30719, i32 30719, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 0, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7829503, i32 7340032, i32 7340032, i32 7340032, i32 0, i32 16777216, i32 33554432, i32 0, i32 0, i32 458752, i32 458752, i32 458752, i32 32768]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !23, null}
!9 = !{!10, !13, !14, !17, !18, !19, !20, !22}
!10 = !{i32 0, !"POSITION", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 0, i8 0, !12}
!11 = !{i32 0}
!12 = !{i32 3, i32 15}
!13 = !{i32 1, !"BLENDWEIGHT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 1, i8 0, !12}
!14 = !{i32 2, !"BLENDWEIGHT", i8 9, i8 0, !15, i8 0, i32 1, i8 4, i32 2, i8 0, !16}
!15 = !{i32 1}
!16 = !{i32 3, i32 7}
!17 = !{i32 3, !"BLENDINDICES", i8 4, i8 0, !11, i8 0, i32 1, i8 4, i32 3, i8 0, !12}
!18 = !{i32 4, !"BLENDINDICES", i8 4, i8 0, !15, i8 0, i32 1, i8 4, i32 4, i8 0, !12}
!19 = !{i32 5, !"NORMAL", i8 9, i8 0, !11, i8 0, i32 1, i8 3, i32 5, i8 0, !16}
!20 = !{i32 6, !"TEXCOORD", i8 9, i8 0, !11, i8 0, i32 1, i8 2, i32 6, i8 0, !21}
!21 = !{i32 3, i32 3}
!22 = !{i32 7, !"TANGENT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 7, i8 0, !12}
!23 = !{!24, !25, !26, !27, !29, !31, !33}
!24 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}
!25 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !11, i8 2, i32 1, i8 4, i32 1, i8 0, !12}
!26 = !{i32 2, !"TEXCOORD", i8 9, i8 0, !15, i8 2, i32 1, i8 4, i32 2, i8 0, !12}
!27 = !{i32 3, !"TEXCOORD", i8 9, i8 0, !28, i8 2, i32 1, i8 4, i32 3, i8 0, !12}
!28 = !{i32 2}
!29 = !{i32 4, !"TEXCOORD", i8 9, i8 0, !30, i8 2, i32 1, i8 3, i32 4, i8 0, !16}
!30 = !{i32 3}
!31 = !{i32 5, !"TEXCOORD", i8 9, i8 0, !32, i8 2, i32 1, i8 3, i32 5, i8 0, !16}
!32 = !{i32 4}
!33 = !{i32 6, !"TEXCOORD", i8 9, i8 0, !34, i8 2, i32 1, i8 2, i32 6, i8 0, !21}
!34 = !{i32 5}

