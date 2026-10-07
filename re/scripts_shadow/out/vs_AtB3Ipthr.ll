;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; NORMAL                   0   xyz         1     NONE   float   xyz 
; TEXCOORD                 0   xy          2     NONE   float   xy  
; TANGENT                  0   xyzw        3     NONE   float   xyzw
; PSIZE                    0   xyz         4     NONE   float   xyz 
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
; shader hash: 4dd43e04bdf02261dede4ba9ce5b8089
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 5
; SigOutputElements: 6
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 5
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
; PSIZE                    0                              
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
;   [284 x i8] (type annotation not present)
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
; Number of inputs: 19, outputs: 22
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 1 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 2 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 3 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 4 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 5 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 6 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 7 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 8 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 9 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 10 depends on inputs: { 0, 1, 2, 3, 16, 17, 18 }
;   output 11 depends on inputs: { 15 }
;   output 12 depends on inputs: { 0, 1, 2, 3, 12, 13, 14 }
;   output 13 depends on inputs: { 0, 1, 2, 3, 12, 13, 14 }
;   output 14 depends on inputs: { 0, 1, 2, 3, 12, 13, 14 }
;   output 16 depends on inputs: { 0, 1, 2, 3, 4, 5, 6 }
;   output 17 depends on inputs: { 0, 1, 2, 3, 4, 5, 6 }
;   output 18 depends on inputs: { 0, 1, 2, 3, 4, 5, 6 }
;   output 20 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 8 }
;   output 21 depends on inputs: { 0, 1, 2, 3, 4, 5, 6, 9 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%"dx.alignment.legacy.$Globals" = type { [4 x <4 x float>], [6 x <4 x float>], <3 x float>, <3 x float>, <3 x float>, [4 x <4 x float>], i32, <2 x float> }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 4, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 3, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %17 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %18 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %19 = extractvalue %dx.types.CBufRet.f32 %18, 0
  %20 = extractvalue %dx.types.CBufRet.f32 %18, 1
  %21 = extractvalue %dx.types.CBufRet.f32 %18, 2
  %22 = extractvalue %dx.types.CBufRet.f32 %18, 3
  %23 = call float @dx.op.dot4.f32(i32 56, float %14, float %15, float %16, float %17, float %19, float %20, float %21, float %22)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %24 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %25 = extractvalue %dx.types.CBufRet.f32 %24, 0
  %26 = extractvalue %dx.types.CBufRet.f32 %24, 1
  %27 = extractvalue %dx.types.CBufRet.f32 %24, 2
  %28 = extractvalue %dx.types.CBufRet.f32 %24, 3
  %29 = call float @dx.op.dot4.f32(i32 56, float %14, float %15, float %16, float %17, float %25, float %26, float %27, float %28)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %30 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %31 = extractvalue %dx.types.CBufRet.f32 %30, 0
  %32 = extractvalue %dx.types.CBufRet.f32 %30, 1
  %33 = extractvalue %dx.types.CBufRet.f32 %30, 2
  %34 = extractvalue %dx.types.CBufRet.f32 %30, 3
  %35 = call float @dx.op.dot4.f32(i32 56, float %14, float %15, float %16, float %17, float %31, float %32, float %33, float %34)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %36 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %37 = extractvalue %dx.types.CBufRet.f32 %36, 0
  %38 = extractvalue %dx.types.CBufRet.f32 %36, 1
  %39 = extractvalue %dx.types.CBufRet.f32 %36, 2
  %40 = fsub fast float %37, %23
  %41 = fsub fast float %38, %29
  %42 = fsub fast float %39, %35
  %43 = call float @dx.op.dot3.f32(i32 55, float %40, float %41, float %42, float %40, float %41, float %42)  ; Dot3(ax,ay,az,bx,by,bz)
  %44 = call float @dx.op.unary.f32(i32 25, float %43)  ; Rsqrt(value)
  %45 = fmul fast float %40, %44
  %46 = fmul fast float %41, %44
  %47 = fmul fast float %42, %44
  %48 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %49 = extractvalue %dx.types.CBufRet.f32 %48, 0
  %50 = extractvalue %dx.types.CBufRet.f32 %48, 1
  %51 = extractvalue %dx.types.CBufRet.f32 %48, 2
  %52 = call float @dx.op.dot3.f32(i32 55, float %49, float %50, float %51, float %45, float %46, float %47)  ; Dot3(ax,ay,az,bx,by,bz)
  %53 = call float @dx.op.unary.f32(i32 6, float %52)  ; FAbs(value)
  %54 = fcmp fast ult float %53, 0x3FEFFFE700000000
  %55 = select i1 %54, float %49, float %21
  %56 = select i1 %54, float %50, float %27
  %57 = select i1 %54, float %51, float %33
  %58 = fmul fast float %56, %47
  %59 = fmul fast float %57, %46
  %60 = fsub fast float %58, %59
  %61 = fmul fast float %57, %45
  %62 = fmul fast float %55, %47
  %63 = fsub fast float %61, %62
  %64 = fmul fast float %55, %46
  %65 = fmul fast float %56, %45
  %66 = fsub fast float %64, %65
  %67 = call float @dx.op.dot3.f32(i32 55, float %60, float %63, float %66, float %60, float %63, float %66)  ; Dot3(ax,ay,az,bx,by,bz)
  %68 = call float @dx.op.unary.f32(i32 25, float %67)  ; Rsqrt(value)
  %69 = fmul fast float %60, %68
  %70 = fmul fast float %63, %68
  %71 = fmul fast float %68, %66
  %72 = fmul fast float %71, %46
  %73 = fmul fast float %70, %47
  %74 = fsub fast float %72, %73
  %75 = fmul fast float %69, %47
  %76 = fmul fast float %71, %45
  %77 = fsub fast float %75, %76
  %78 = fmul fast float %70, %45
  %79 = fmul fast float %69, %46
  %80 = fsub fast float %78, %79
  %81 = fmul fast float %69, %11
  %82 = fmul fast float %70, %11
  %83 = fmul fast float %71, %11
  %84 = fmul fast float %74, %12
  %85 = fmul fast float %77, %12
  %86 = fmul fast float %80, %12
  %87 = fmul fast float %45, %13
  %88 = fmul fast float %46, %13
  %89 = fmul fast float %47, %13
  %90 = fadd fast float %81, %87
  %91 = fadd fast float %90, %84
  %92 = fadd fast float %82, %88
  %93 = fadd fast float %92, %85
  %94 = fadd fast float %83, %89
  %95 = fadd fast float %94, %86
  %96 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %97 = extractvalue %dx.types.CBufRet.f32 %96, 0
  %98 = extractvalue %dx.types.CBufRet.f32 %96, 1
  %99 = extractvalue %dx.types.CBufRet.f32 %96, 2
  %100 = fmul fast float %91, %97
  %101 = fmul fast float %93, %98
  %102 = fmul fast float %95, %99
  %103 = call float @dx.op.dot3.f32(i32 55, float %100, float %101, float %102, float %100, float %101, float %102)  ; Dot3(ax,ay,az,bx,by,bz)
  %104 = call float @dx.op.unary.f32(i32 25, float %103)  ; Rsqrt(value)
  %105 = fmul fast float %100, %104
  %106 = fmul fast float %101, %104
  %107 = fmul fast float %102, %104
  %108 = fmul fast float %69, %5
  %109 = fmul fast float %70, %5
  %110 = fmul fast float %71, %5
  %111 = fmul fast float %74, %6
  %112 = fmul fast float %77, %6
  %113 = fmul fast float %80, %6
  %114 = fmul fast float %45, %7
  %115 = fmul fast float %46, %7
  %116 = fmul fast float %47, %7
  %117 = fadd fast float %108, %114
  %118 = fadd fast float %117, %111
  %119 = fadd fast float %109, %115
  %120 = fadd fast float %119, %112
  %121 = fadd fast float %110, %116
  %122 = fadd fast float %121, %113
  %123 = fmul fast float %118, %97
  %124 = fmul fast float %120, %98
  %125 = fmul fast float %122, %99
  %126 = call float @dx.op.dot3.f32(i32 55, float %123, float %124, float %125, float %123, float %124, float %125)  ; Dot3(ax,ay,az,bx,by,bz)
  %127 = call float @dx.op.unary.f32(i32 25, float %126)  ; Rsqrt(value)
  %128 = fmul fast float %123, %127
  %129 = fmul fast float %124, %127
  %130 = fmul fast float %125, %127
  %131 = fmul fast float %69, %2
  %132 = fmul fast float %70, %2
  %133 = fmul fast float %71, %2
  %134 = fmul fast float %74, %3
  %135 = fmul fast float %77, %3
  %136 = fmul fast float %80, %3
  %137 = fmul fast float %45, %4
  %138 = fmul fast float %46, %4
  %139 = fmul fast float %47, %4
  %140 = fadd fast float %131, %137
  %141 = fadd fast float %140, %134
  %142 = fadd fast float %132, %138
  %143 = fadd fast float %142, %135
  %144 = fadd fast float %133, %139
  %145 = fadd fast float %144, %136
  %146 = fmul fast float %97, %141
  %147 = fmul fast float %98, %143
  %148 = fmul fast float %99, %145
  %149 = fadd fast float %146, %23
  %150 = fadd fast float %147, %29
  %151 = fadd fast float %148, %35
  %152 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %153 = extractvalue %dx.types.CBufRet.f32 %152, 0
  %154 = extractvalue %dx.types.CBufRet.f32 %152, 1
  %155 = extractvalue %dx.types.CBufRet.f32 %152, 2
  %156 = extractvalue %dx.types.CBufRet.f32 %152, 3
  %157 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %158 = extractvalue %dx.types.CBufRet.f32 %157, 0
  %159 = extractvalue %dx.types.CBufRet.f32 %157, 1
  %160 = extractvalue %dx.types.CBufRet.f32 %157, 2
  %161 = extractvalue %dx.types.CBufRet.f32 %157, 3
  %162 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %163 = extractvalue %dx.types.CBufRet.f32 %162, 0
  %164 = extractvalue %dx.types.CBufRet.f32 %162, 1
  %165 = extractvalue %dx.types.CBufRet.f32 %162, 2
  %166 = extractvalue %dx.types.CBufRet.f32 %162, 3
  %167 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %168 = extractvalue %dx.types.CBufRet.f32 %167, 0
  %169 = extractvalue %dx.types.CBufRet.f32 %167, 1
  %170 = extractvalue %dx.types.CBufRet.f32 %167, 2
  %171 = extractvalue %dx.types.CBufRet.f32 %167, 3
  %172 = fmul fast float %149, %153
  %173 = call float @dx.op.tertiary.f32(i32 46, float %150, float %158, float %172)  ; FMad(a,b,c)
  %174 = call float @dx.op.tertiary.f32(i32 46, float %151, float %163, float %173)  ; FMad(a,b,c)
  %175 = call float @dx.op.tertiary.f32(i32 46, float %17, float %168, float %174)  ; FMad(a,b,c)
  %176 = fmul fast float %149, %154
  %177 = call float @dx.op.tertiary.f32(i32 46, float %150, float %159, float %176)  ; FMad(a,b,c)
  %178 = call float @dx.op.tertiary.f32(i32 46, float %151, float %164, float %177)  ; FMad(a,b,c)
  %179 = call float @dx.op.tertiary.f32(i32 46, float %17, float %169, float %178)  ; FMad(a,b,c)
  %180 = fmul fast float %149, %155
  %181 = call float @dx.op.tertiary.f32(i32 46, float %150, float %160, float %180)  ; FMad(a,b,c)
  %182 = call float @dx.op.tertiary.f32(i32 46, float %151, float %165, float %181)  ; FMad(a,b,c)
  %183 = call float @dx.op.tertiary.f32(i32 46, float %17, float %170, float %182)  ; FMad(a,b,c)
  %184 = fmul fast float %149, %156
  %185 = call float @dx.op.tertiary.f32(i32 46, float %150, float %161, float %184)  ; FMad(a,b,c)
  %186 = call float @dx.op.tertiary.f32(i32 46, float %151, float %166, float %185)  ; FMad(a,b,c)
  %187 = call float @dx.op.tertiary.f32(i32 46, float %17, float %171, float %186)  ; FMad(a,b,c)
  %188 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %189 = extractvalue %dx.types.CBufRet.f32 %188, 0
  %190 = extractvalue %dx.types.CBufRet.f32 %188, 1
  %191 = extractvalue %dx.types.CBufRet.f32 %188, 2
  %192 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %193 = extractvalue %dx.types.CBufRet.f32 %192, 0
  %194 = extractvalue %dx.types.CBufRet.f32 %192, 1
  %195 = extractvalue %dx.types.CBufRet.f32 %192, 2
  %196 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %197 = extractvalue %dx.types.CBufRet.f32 %196, 0
  %198 = extractvalue %dx.types.CBufRet.f32 %196, 1
  %199 = extractvalue %dx.types.CBufRet.f32 %196, 2
  %200 = fmul fast float %189, %105
  %201 = call float @dx.op.tertiary.f32(i32 46, float %106, float %193, float %200)  ; FMad(a,b,c)
  %202 = call float @dx.op.tertiary.f32(i32 46, float %107, float %197, float %201)  ; FMad(a,b,c)
  %203 = fmul fast float %190, %105
  %204 = call float @dx.op.tertiary.f32(i32 46, float %106, float %194, float %203)  ; FMad(a,b,c)
  %205 = call float @dx.op.tertiary.f32(i32 46, float %107, float %198, float %204)  ; FMad(a,b,c)
  %206 = fmul fast float %191, %105
  %207 = call float @dx.op.tertiary.f32(i32 46, float %106, float %195, float %206)  ; FMad(a,b,c)
  %208 = call float @dx.op.tertiary.f32(i32 46, float %107, float %199, float %207)  ; FMad(a,b,c)
  %209 = call float @dx.op.dot3.f32(i32 55, float %202, float %205, float %208, float %202, float %205, float %208)  ; Dot3(ax,ay,az,bx,by,bz)
  %210 = call float @dx.op.unary.f32(i32 25, float %209)  ; Rsqrt(value)
  %211 = fmul fast float %210, %202
  %212 = fmul fast float %210, %205
  %213 = fmul fast float %210, %208
  %214 = call float @dx.op.unary.f32(i32 7, float %212)  ; Saturate(value)
  %215 = call float @dx.op.dot3.f32(i32 55, float %211, float %214, float %213, float %211, float %214, float %213)  ; Dot3(ax,ay,az,bx,by,bz)
  %216 = call float @dx.op.unary.f32(i32 25, float %215)  ; Rsqrt(value)
  %217 = fmul fast float %216, %211
  %218 = fmul fast float %216, %214
  %219 = fmul fast float %213, %216
  %220 = fsub fast float -0.000000e+00, %219
  %221 = call float @dx.op.unary.f32(i32 6, float %217)  ; FAbs(value)
  %222 = call float @dx.op.unary.f32(i32 6, float %220)  ; FAbs(value)
  %223 = call float @dx.op.unary.f32(i32 6, float %218)  ; FAbs(value)
  %224 = call float @dx.op.dot3.f32(i32 55, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float %221, float %222, float %223)  ; Dot3(ax,ay,az,bx,by,bz)
  %225 = fsub fast float %202, %208
  %226 = fmul fast float %210, %225
  %227 = fmul fast float %226, %216
  %228 = fdiv fast float %227, %224
  %229 = fadd fast float %208, %202
  %230 = fmul fast float %210, %229
  %231 = fmul fast float %230, %216
  %232 = fdiv fast float %231, %224
  %233 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %1, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %234 = extractvalue %dx.types.CBufRet.i32 %233, 0
  %235 = sitofp i32 %234 to float
  %236 = fadd fast float %235, -1.000000e+00
  %237 = fmul fast float %236, 5.000000e-01
  %238 = fdiv fast float %237, %235
  %239 = fmul fast float %238, %228
  %240 = fmul fast float %238, %232
  %241 = fadd fast float %239, 5.000000e-01
  %242 = fadd fast float %240, 5.000000e-01
  %243 = fmul fast float %241, %235
  %244 = fmul fast float %242, %235
  %245 = call float @dx.op.unary.f32(i32 27, float %243)  ; Round_ni(value)
  %246 = call float @dx.op.unary.f32(i32 27, float %244)  ; Round_ni(value)
  %247 = fadd fast float %9, %245
  %248 = fdiv fast float %247, %235
  %249 = fadd fast float %10, %246
  %250 = fdiv fast float %249, %235
  %251 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %252 = extractvalue %dx.types.CBufRet.f32 %251, 1
  %253 = extractvalue %dx.types.CBufRet.f32 %251, 2
  %254 = fmul fast float %252, %248
  %255 = fmul fast float %253, %250
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 0, float %105)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 1, float %106)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 4, i32 0, i8 2, float %107)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %128)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %129)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %130)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %175)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %179)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %183)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %187)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 0, float %254)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 1, float %255)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  %256 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %257 = extractvalue %dx.types.CBufRet.f32 %256, 0
  %258 = extractvalue %dx.types.CBufRet.f32 %256, 1
  %259 = extractvalue %dx.types.CBufRet.f32 %256, 3
  %260 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %261 = extractvalue %dx.types.CBufRet.f32 %260, 0
  %262 = extractvalue %dx.types.CBufRet.f32 %260, 1
  %263 = extractvalue %dx.types.CBufRet.f32 %260, 3
  %264 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %265 = extractvalue %dx.types.CBufRet.f32 %264, 0
  %266 = extractvalue %dx.types.CBufRet.f32 %264, 1
  %267 = extractvalue %dx.types.CBufRet.f32 %264, 3
  %268 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %269 = extractvalue %dx.types.CBufRet.f32 %268, 0
  %270 = extractvalue %dx.types.CBufRet.f32 %268, 1
  %271 = extractvalue %dx.types.CBufRet.f32 %268, 3
  %272 = fmul fast float %257, %149
  %273 = call float @dx.op.tertiary.f32(i32 46, float %150, float %261, float %272)  ; FMad(a,b,c)
  %274 = call float @dx.op.tertiary.f32(i32 46, float %151, float %265, float %273)  ; FMad(a,b,c)
  %275 = call float @dx.op.tertiary.f32(i32 46, float %17, float %269, float %274)  ; FMad(a,b,c)
  %276 = fmul fast float %258, %149
  %277 = call float @dx.op.tertiary.f32(i32 46, float %150, float %262, float %276)  ; FMad(a,b,c)
  %278 = call float @dx.op.tertiary.f32(i32 46, float %151, float %266, float %277)  ; FMad(a,b,c)
  %279 = call float @dx.op.tertiary.f32(i32 46, float %17, float %270, float %278)  ; FMad(a,b,c)
  %280 = fmul fast float %259, %149
  %281 = call float @dx.op.tertiary.f32(i32 46, float %150, float %263, float %280)  ; FMad(a,b,c)
  %282 = call float @dx.op.tertiary.f32(i32 46, float %151, float %267, float %281)  ; FMad(a,b,c)
  %283 = call float @dx.op.tertiary.f32(i32 46, float %17, float %271, float %282)  ; FMad(a,b,c)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %175)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %179)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %183)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %187)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %275)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %279)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %283)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float %8)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

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
!5 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 284, null}
!6 = !{[21 x i32] [i32 19, i32 22, i32 3635199, i32 3635199, i32 3635199, i32 3635199, i32 3604480, i32 3604480, i32 3604480, i32 0, i32 1048576, i32 2097152, i32 0, i32 0, i32 28672, i32 28672, i32 28672, i32 2048, i32 2047, i32 2047, i32 2047]}
!7 = !{void ()* @main, !"main", !8, !3, null}
!8 = !{!9, !19, null}
!9 = !{!10, !13, !15, !17, !18}
!10 = !{i32 0, !"POSITION", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 0, i8 0, !12}
!11 = !{i32 0}
!12 = !{i32 3, i32 15}
!13 = !{i32 1, !"NORMAL", i8 9, i8 0, !11, i8 0, i32 1, i8 3, i32 1, i8 0, !14}
!14 = !{i32 3, i32 7}
!15 = !{i32 2, !"TEXCOORD", i8 9, i8 0, !11, i8 0, i32 1, i8 2, i32 2, i8 0, !16}
!16 = !{i32 3, i32 3}
!17 = !{i32 3, !"TANGENT", i8 9, i8 0, !11, i8 0, i32 1, i8 4, i32 3, i8 0, !12}
!18 = !{i32 4, !"PSIZE", i8 9, i8 0, !11, i8 0, i32 1, i8 3, i32 4, i8 0, !14}
!19 = !{!20, !21, !22, !24, !26, !28}
!20 = !{i32 0, !"SV_Position", i8 9, i8 3, !11, i8 4, i32 1, i8 4, i32 0, i8 0, !12}
!21 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !11, i8 2, i32 1, i8 4, i32 1, i8 0, !12}
!22 = !{i32 2, !"TEXCOORD", i8 9, i8 0, !23, i8 2, i32 1, i8 4, i32 2, i8 0, !12}
!23 = !{i32 1}
!24 = !{i32 3, !"TEXCOORD", i8 9, i8 0, !25, i8 2, i32 1, i8 3, i32 3, i8 0, !14}
!25 = !{i32 2}
!26 = !{i32 4, !"TEXCOORD", i8 9, i8 0, !27, i8 2, i32 1, i8 3, i32 4, i8 0, !14}
!27 = !{i32 3}
!28 = !{i32 5, !"TEXCOORD", i8 9, i8 0, !29, i8 2, i32 1, i8 2, i32 5, i8 0, !16}
!29 = !{i32 4}

