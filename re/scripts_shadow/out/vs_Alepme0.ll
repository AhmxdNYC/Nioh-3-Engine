;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; NORMAL                   0   xyz         1     NONE   float   xyz 
; TEXCOORD                 0   xy          2     NONE   float   xy  
; TANGENT                  0   xyzw        3     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; TEXCOORD                 0   xyzw        1     NONE   float   xyzw
; TEXCOORD                 1   xyzw        2     NONE   float   xyzw
; TEXCOORD                 2   xyz         3     NONE   float   xyz 
; TEXCOORD                 3   xyz         4     NONE   float   xyz 
; TEXCOORD                 4   xy          5     NONE   float   xy  
;
; shader hash: 6fd5522adfe0b2668c397bd633694fd7
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 4
; SigOutputElements: 6
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 4
; SigOutputVectors[0]: 6
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
;
; Buffer Definitions:
;
; cbuffer 
; {
;
;   [272 x i8] (type annotation not present)
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
; Number of inputs: 16, outputs: 22
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3 }
;   output 1 depends on inputs: { 0, 1, 2, 3 }
;   output 2 depends on inputs: { 0, 1, 2, 3 }
;   output 3 depends on inputs: { 0, 1, 2, 3 }
;   output 4 depends on inputs: { 0, 1, 2, 3 }
;   output 5 depends on inputs: { 0, 1, 2, 3 }
;   output 6 depends on inputs: { 0, 1, 2, 3 }
;   output 7 depends on inputs: { 0, 1, 2, 3 }
;   output 8 depends on inputs: { 0, 1, 2, 3 }
;   output 9 depends on inputs: { 0, 1, 2, 3 }
;   output 10 depends on inputs: { 0, 1, 2, 3 }
;   output 11 depends on inputs: { 15 }
;   output 12 depends on inputs: { 12, 13, 14 }
;   output 13 depends on inputs: { 12, 13, 14 }
;   output 14 depends on inputs: { 12, 13, 14 }
;   output 16 depends on inputs: { 4, 5, 6 }
;   output 17 depends on inputs: { 4, 5, 6 }
;   output 18 depends on inputs: { 4, 5, 6 }
;   output 20 depends on inputs: { 8 }
;   output 21 depends on inputs: { 9 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [3 x <4 x float>], [6 x <4 x float>], [4 x <4 x float>] }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %16 = extractvalue %dx.types.CBufRet.f32 %15, 0
  %17 = extractvalue %dx.types.CBufRet.f32 %15, 1
  %18 = extractvalue %dx.types.CBufRet.f32 %15, 2
  %19 = extractvalue %dx.types.CBufRet.f32 %15, 3
  %20 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %16, float %17, float %18, float %19)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %21 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %22 = extractvalue %dx.types.CBufRet.f32 %21, 0
  %23 = extractvalue %dx.types.CBufRet.f32 %21, 1
  %24 = extractvalue %dx.types.CBufRet.f32 %21, 2
  %25 = extractvalue %dx.types.CBufRet.f32 %21, 3
  %26 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %22, float %23, float %24, float %25)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %27 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %28 = extractvalue %dx.types.CBufRet.f32 %27, 0
  %29 = extractvalue %dx.types.CBufRet.f32 %27, 1
  %30 = extractvalue %dx.types.CBufRet.f32 %27, 2
  %31 = extractvalue %dx.types.CBufRet.f32 %27, 3
  %32 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %28, float %29, float %30, float %31)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %33 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %34 = extractvalue %dx.types.CBufRet.f32 %33, 0
  %35 = extractvalue %dx.types.CBufRet.f32 %33, 1
  %36 = extractvalue %dx.types.CBufRet.f32 %33, 2
  %37 = call float @dx.op.dot3.f32(i32 55, float %34, float %35, float %36, float %8, float %9, float %10)  ; Dot3(ax,ay,az,bx,by,bz)
  %38 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %39 = extractvalue %dx.types.CBufRet.f32 %38, 0
  %40 = extractvalue %dx.types.CBufRet.f32 %38, 1
  %41 = extractvalue %dx.types.CBufRet.f32 %38, 2
  %42 = call float @dx.op.dot3.f32(i32 55, float %39, float %40, float %41, float %8, float %9, float %10)  ; Dot3(ax,ay,az,bx,by,bz)
  %43 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %44 = extractvalue %dx.types.CBufRet.f32 %43, 0
  %45 = extractvalue %dx.types.CBufRet.f32 %43, 1
  %46 = extractvalue %dx.types.CBufRet.f32 %43, 2
  %47 = call float @dx.op.dot3.f32(i32 55, float %44, float %45, float %46, float %8, float %9, float %10)  ; Dot3(ax,ay,az,bx,by,bz)
  %48 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %16, float %17, float %18)  ; Dot3(ax,ay,az,bx,by,bz)
  %49 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %22, float %23, float %24)  ; Dot3(ax,ay,az,bx,by,bz)
  %50 = call float @dx.op.dot3.f32(i32 55, float %2, float %3, float %4, float %28, float %29, float %30)  ; Dot3(ax,ay,az,bx,by,bz)
  %51 = call float @dx.op.dot3.f32(i32 55, float %37, float %42, float %47, float %37, float %42, float %47)  ; Dot3(ax,ay,az,bx,by,bz)
  %52 = call float @dx.op.unary.f32(i32 25, float %51)  ; Rsqrt(value)
  %53 = fmul fast float %52, %37
  %54 = fmul fast float %52, %42
  %55 = fmul fast float %52, %47
  %56 = call float @dx.op.dot3.f32(i32 55, float %48, float %49, float %50, float %48, float %49, float %50)  ; Dot3(ax,ay,az,bx,by,bz)
  %57 = call float @dx.op.unary.f32(i32 25, float %56)  ; Rsqrt(value)
  %58 = fmul fast float %57, %48
  %59 = fmul fast float %57, %49
  %60 = fmul fast float %57, %50
  %61 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %62 = extractvalue %dx.types.CBufRet.f32 %61, 0
  %63 = extractvalue %dx.types.CBufRet.f32 %61, 1
  %64 = extractvalue %dx.types.CBufRet.f32 %61, 2
  %65 = extractvalue %dx.types.CBufRet.f32 %61, 3
  %66 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %67 = extractvalue %dx.types.CBufRet.f32 %66, 0
  %68 = extractvalue %dx.types.CBufRet.f32 %66, 1
  %69 = extractvalue %dx.types.CBufRet.f32 %66, 2
  %70 = extractvalue %dx.types.CBufRet.f32 %66, 3
  %71 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %72 = extractvalue %dx.types.CBufRet.f32 %71, 0
  %73 = extractvalue %dx.types.CBufRet.f32 %71, 1
  %74 = extractvalue %dx.types.CBufRet.f32 %71, 2
  %75 = extractvalue %dx.types.CBufRet.f32 %71, 3
  %76 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %77 = extractvalue %dx.types.CBufRet.f32 %76, 0
  %78 = extractvalue %dx.types.CBufRet.f32 %76, 1
  %79 = extractvalue %dx.types.CBufRet.f32 %76, 2
  %80 = extractvalue %dx.types.CBufRet.f32 %76, 3
  %81 = fmul fast float %62, %20
  %82 = call float @dx.op.tertiary.f32(i32 46, float %26, float %67, float %81)  ; FMad(a,b,c)
  %83 = call float @dx.op.tertiary.f32(i32 46, float %32, float %72, float %82)  ; FMad(a,b,c)
  %84 = call float @dx.op.tertiary.f32(i32 46, float %14, float %77, float %83)  ; FMad(a,b,c)
  %85 = fmul fast float %63, %20
  %86 = call float @dx.op.tertiary.f32(i32 46, float %26, float %68, float %85)  ; FMad(a,b,c)
  %87 = call float @dx.op.tertiary.f32(i32 46, float %32, float %73, float %86)  ; FMad(a,b,c)
  %88 = call float @dx.op.tertiary.f32(i32 46, float %14, float %78, float %87)  ; FMad(a,b,c)
  %89 = fmul fast float %64, %20
  %90 = call float @dx.op.tertiary.f32(i32 46, float %26, float %69, float %89)  ; FMad(a,b,c)
  %91 = call float @dx.op.tertiary.f32(i32 46, float %32, float %74, float %90)  ; FMad(a,b,c)
  %92 = call float @dx.op.tertiary.f32(i32 46, float %14, float %79, float %91)  ; FMad(a,b,c)
  %93 = fmul fast float %65, %20
  %94 = call float @dx.op.tertiary.f32(i32 46, float %26, float %70, float %93)  ; FMad(a,b,c)
  %95 = call float @dx.op.tertiary.f32(i32 46, float %32, float %75, float %94)  ; FMad(a,b,c)
  %96 = call float @dx.op.tertiary.f32(i32 46, float %14, float %80, float %95)  ; FMad(a,b,c)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 0, float %53)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 1, float %54)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 2, float %55)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %58)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %59)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %60)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %84)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %88)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %92)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %96)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 0, float %6)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 1, float %7)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  %97 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %98 = extractvalue %dx.types.CBufRet.f32 %97, 0
  %99 = extractvalue %dx.types.CBufRet.f32 %97, 1
  %100 = extractvalue %dx.types.CBufRet.f32 %97, 2
  %101 = extractvalue %dx.types.CBufRet.f32 %97, 3
  %102 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %98, float %99, float %100, float %101)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %103 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %104 = extractvalue %dx.types.CBufRet.f32 %103, 0
  %105 = extractvalue %dx.types.CBufRet.f32 %103, 1
  %106 = extractvalue %dx.types.CBufRet.f32 %103, 2
  %107 = extractvalue %dx.types.CBufRet.f32 %103, 3
  %108 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %104, float %105, float %106, float %107)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %110 = extractvalue %dx.types.CBufRet.f32 %109, 0
  %111 = extractvalue %dx.types.CBufRet.f32 %109, 1
  %112 = extractvalue %dx.types.CBufRet.f32 %109, 2
  %113 = extractvalue %dx.types.CBufRet.f32 %109, 3
  %114 = call float @dx.op.dot4.f32(i32 56, float %11, float %12, float %13, float %14, float %110, float %111, float %112, float %113)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %115 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %116 = extractvalue %dx.types.CBufRet.f32 %115, 0
  %117 = extractvalue %dx.types.CBufRet.f32 %115, 1
  %118 = extractvalue %dx.types.CBufRet.f32 %115, 3
  %119 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %120 = extractvalue %dx.types.CBufRet.f32 %119, 0
  %121 = extractvalue %dx.types.CBufRet.f32 %119, 1
  %122 = extractvalue %dx.types.CBufRet.f32 %119, 3
  %123 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %124 = extractvalue %dx.types.CBufRet.f32 %123, 0
  %125 = extractvalue %dx.types.CBufRet.f32 %123, 1
  %126 = extractvalue %dx.types.CBufRet.f32 %123, 3
  %127 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %128 = extractvalue %dx.types.CBufRet.f32 %127, 0
  %129 = extractvalue %dx.types.CBufRet.f32 %127, 1
  %130 = extractvalue %dx.types.CBufRet.f32 %127, 3
  %131 = fmul fast float %116, %102
  %132 = call float @dx.op.tertiary.f32(i32 46, float %108, float %120, float %131)  ; FMad(a,b,c)
  %133 = call float @dx.op.tertiary.f32(i32 46, float %114, float %124, float %132)  ; FMad(a,b,c)
  %134 = call float @dx.op.tertiary.f32(i32 46, float %14, float %128, float %133)  ; FMad(a,b,c)
  %135 = fmul fast float %117, %102
  %136 = call float @dx.op.tertiary.f32(i32 46, float %108, float %121, float %135)  ; FMad(a,b,c)
  %137 = call float @dx.op.tertiary.f32(i32 46, float %114, float %125, float %136)  ; FMad(a,b,c)
  %138 = call float @dx.op.tertiary.f32(i32 46, float %14, float %129, float %137)  ; FMad(a,b,c)
  %139 = fmul fast float %118, %102
  %140 = call float @dx.op.tertiary.f32(i32 46, float %108, float %122, float %139)  ; FMad(a,b,c)
  %141 = call float @dx.op.tertiary.f32(i32 46, float %114, float %126, float %140)  ; FMad(a,b,c)
  %142 = call float @dx.op.tertiary.f32(i32 46, float %14, float %130, float %141)  ; FMad(a,b,c)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %84)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %88)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %92)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %96)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %134)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %138)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %142)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

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
!5 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 272, null}
!6 = !{[18 x i32] [i32 16, i32 22, i32 2047, i32 2047, i32 2047, i32 2047, i32 458752, i32 458752, i32 458752, i32 0, i32 1048576, i32 2097152, i32 0, i32 0, i32 28672, i32 28672, i32 28672, i32 2048]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !18, null}
!9 = !{!10, !13, !15, !17}
!10 = !{i32 0, !"POSITION", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 0, i8 0, !12}
!11 = !{i32 0}
!12 = !{i32 3, i32 15}
!13 = !{i32 1, !"NORMAL", i8 9, i8 0, !11, i8 0, i32 1, i8 3, i32 1, i8 0, !14}
!14 = !{i32 3, i32 7}
!15 = !{i32 2, !"TEXCOORD", i8 9, i8 0, !11, i8 0, i32 1, i8 2, i32 2, i8 0, !16}
!16 = !{i32 3, i32 3}
!17 = !{i32 3, !"TANGENT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 3, i8 0, !12}
!18 = !{!19, !20, !21, !23, !25, !27}
!19 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}
!20 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !11, i8 2, i32 1, i8 4, i32 1, i8 0, !12}
!21 = !{i32 2, !"TEXCOORD", i8 9, i8 0, !22, i8 2, i32 1, i8 4, i32 2, i8 0, !12}
!22 = !{i32 1}
!23 = !{i32 3, !"TEXCOORD", i8 9, i8 0, !24, i8 2, i32 1, i8 3, i32 3, i8 0, !14}
!24 = !{i32 2}
!25 = !{i32 4, !"TEXCOORD", i8 9, i8 0, !26, i8 2, i32 1, i8 3, i32 4, i8 0, !14}
!26 = !{i32 3}
!27 = !{i32 5, !"TEXCOORD", i8 9, i8 0, !28, i8 2, i32 1, i8 2, i32 5, i8 0, !16}
!28 = !{i32 4}

