;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float       
; TEXCOORD                 0   xyzw        1     NONE   float   xy  
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Target                0   xyzw        0   TARGET   float   xyzw
;
; shader debug name: 6a0f7c689eacc711c9b291396c4c877f.pdb
; shader hash: 6a0f7c689eacc711c9b291396c4c877f
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Pixel Shader
; DepthOutput=0
; SampleFrequency=0
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 2
; SigOutputElements: 1
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 2
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
; SV_Position              0          noperspective       
; TEXCOORD                 0                 linear       
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Target                0                              
;
; Buffer Definitions:
;
; cbuffer $Globals
; {
;
;   struct $Globals
;   {
;
;       float4 vP2T;                                  ; Offset:    0
;       float4 vResolutionParam;                      ; Offset:   16
;       float4 vBlurParam;                            ; Offset:   32
;       float4 vDepthParam;                           ; Offset:   48
;       float4 vViewportParam;                        ; Offset:   64
;   
;   } $Globals;                                       ; Offset:    0 Size:    80
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smpsDepth                       sampler      NA          NA      S0             s1     1
; __smpsColor                       sampler      NA          NA      S1             s3     1
; sDepth                            texture     f32          2d      T0             t1     1
; sColor                            texture     f32     2darray      T1             t3     1
;
;
; ViewId state:
;
; Number of inputs: 8, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 3 depends on inputs: { 4, 5 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
%"$Globals" = type { <4 x float>, <4 x float>, <4 x float>, <4 x float>, <4 x float> }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 1, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %9 = extractvalue %dx.types.CBufRet.f32 %8, 0
  %10 = extractvalue %dx.types.CBufRet.f32 %8, 1
  %11 = fmul fast float %9, %6
  %12 = fmul fast float %10, %7
  %13 = extractvalue %dx.types.CBufRet.f32 %8, 2
  %14 = extractvalue %dx.types.CBufRet.f32 %8, 3
  %15 = fadd fast float %11, %13
  %16 = fadd fast float %12, %14
  %17 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %18 = extractvalue %dx.types.CBufRet.f32 %17, 3
  %19 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %1, %dx.types.Handle %3, float %15, float %16, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %20 = extractvalue %dx.types.ResRet.f32 %19, 3
  %21 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %4, float %15, float %16, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %22 = extractvalue %dx.types.ResRet.f32 %21, 0
  %23 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %24 = extractvalue %dx.types.CBufRet.f32 %23, 1
  %25 = extractvalue %dx.types.CBufRet.f32 %23, 0
  %26 = call float @dx.op.tertiary.f32(i32 46, float %22, float %25, float %24)  ; FMad(a,b,c)
  %27 = fdiv fast float 1.000000e+00, %26
  %28 = extractvalue %dx.types.CBufRet.f32 %23, 2
  %29 = fcmp fast ole float %28, %27
  %30 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %31 = extractvalue %dx.types.CBufRet.f32 %30, 2
  %32 = fcmp fast oge float %15, %31
  %33 = extractvalue %dx.types.CBufRet.f32 %30, 0
  %34 = fadd fast float %31, %33
  %35 = fcmp fast ole float %15, %34
  %36 = and i1 %32, %35
  %37 = extractvalue %dx.types.CBufRet.f32 %30, 3
  %38 = fcmp fast oge float %16, %37
  %39 = extractvalue %dx.types.CBufRet.f32 %30, 1
  %40 = fadd fast float %37, %39
  %41 = fcmp fast ole float %16, %40
  %42 = and i1 %38, %41
  %43 = and i1 %36, %42
  %44 = xor i1 %43, true
  %45 = or i1 %29, %44
  br i1 %45, label %173, label %46

; <label>:46                                      ; preds = %0
  %47 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %48 = extractvalue %dx.types.CBufRet.f32 %47, 0
  %49 = fcmp ogt float %48, -0.000000e+00
  br i1 %49, label %50, label %106

; <label>:50                                      ; preds = %46
  %51 = fsub fast float -0.000000e+00, %48
  br label %52

; <label>:52                                      ; preds = %100, %50
  %53 = phi float [ %101, %100 ], [ %20, %50 ]
  %54 = phi float [ %102, %100 ], [ 1.000000e+00, %50 ]
  %55 = phi float [ %103, %100 ], [ %51, %50 ]
  %56 = fmul fast float %55, %18
  %57 = fadd fast float %56, %16
  %58 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %59 = extractvalue %dx.types.CBufRet.f32 %58, 2
  %60 = fcmp fast oge float %15, %59
  %61 = extractvalue %dx.types.CBufRet.f32 %58, 0
  %62 = fadd fast float %59, %61
  %63 = fcmp fast ole float %15, %62
  %64 = and i1 %60, %63
  %65 = extractvalue %dx.types.CBufRet.f32 %58, 3
  %66 = fcmp fast oge float %57, %65
  %67 = extractvalue %dx.types.CBufRet.f32 %58, 1
  %68 = fadd fast float %65, %67
  %69 = fcmp fast ole float %57, %68
  %70 = and i1 %66, %69
  %71 = and i1 %64, %70
  br i1 %71, label %72, label %100

; <label>:72                                      ; preds = %52
  %73 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %4, float %15, float %57, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %74 = extractvalue %dx.types.ResRet.f32 %73, 0
  %75 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %76 = extractvalue %dx.types.CBufRet.f32 %75, 1
  %77 = extractvalue %dx.types.CBufRet.f32 %75, 0
  %78 = call float @dx.op.tertiary.f32(i32 46, float %74, float %77, float %76)  ; FMad(a,b,c)
  %79 = fdiv fast float 1.000000e+00, %78
  %80 = extractvalue %dx.types.CBufRet.f32 %75, 2
  %81 = fcmp fast ugt float %80, %79
  br i1 %81, label %82, label %100

; <label>:82                                      ; preds = %72
  %83 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %1, %dx.types.Handle %3, float %15, float %57, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %84 = extractvalue %dx.types.ResRet.f32 %83, 3
  %85 = fsub fast float %79, %27
  %86 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %87 = extractvalue %dx.types.CBufRet.f32 %86, 1
  %88 = fmul fast float %55, %55
  %89 = fmul fast float %88, %87
  %90 = fsub fast float -0.000000e+00, %89
  %91 = call float @dx.op.unary.f32(i32 21, float %90)  ; Exp(value)
  %92 = call float @dx.op.unary.f32(i32 6, float %85)  ; FAbs(value)
  %93 = extractvalue %dx.types.CBufRet.f32 %86, 2
  %94 = fcmp fast olt float %92, %93
  %95 = select i1 %94, float 1.000000e+00, float 0.000000e+00
  %96 = fmul fast float %95, %91
  %97 = fmul fast float %96, %84
  %98 = fadd fast float %97, %53
  %99 = fadd fast float %96, %54
  br label %100

; <label>:100                                     ; preds = %82, %72, %52
  %101 = phi float [ %53, %52 ], [ %53, %72 ], [ %98, %82 ]
  %102 = phi float [ %54, %52 ], [ %54, %72 ], [ %99, %82 ]
  %103 = fadd fast float %55, 1.000000e+00
  %104 = fcmp fast olt float %103, 0.000000e+00
  br i1 %104, label %52, label %105

; <label>:105                                     ; preds = %100
  br label %106

; <label>:106                                     ; preds = %105, %46
  %107 = phi float [ %20, %46 ], [ %101, %105 ]
  %108 = phi float [ 1.000000e+00, %46 ], [ %102, %105 ]
  %109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %110 = extractvalue %dx.types.CBufRet.f32 %109, 0
  %111 = fcmp fast ult float %110, 1.000000e+00
  br i1 %111, label %169, label %112

; <label>:112                                     ; preds = %106
  br label %113

; <label>:113                                     ; preds = %161, %112
  %114 = phi float [ %162, %161 ], [ %107, %112 ]
  %115 = phi float [ %163, %161 ], [ %108, %112 ]
  %116 = phi float [ %164, %161 ], [ 1.000000e+00, %112 ]
  %117 = fmul fast float %116, %18
  %118 = fadd fast float %117, %16
  %119 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %120 = extractvalue %dx.types.CBufRet.f32 %119, 2
  %121 = fcmp fast oge float %15, %120
  %122 = extractvalue %dx.types.CBufRet.f32 %119, 0
  %123 = fadd fast float %120, %122
  %124 = fcmp fast ole float %15, %123
  %125 = and i1 %121, %124
  %126 = extractvalue %dx.types.CBufRet.f32 %119, 3
  %127 = fcmp fast oge float %118, %126
  %128 = extractvalue %dx.types.CBufRet.f32 %119, 1
  %129 = fadd fast float %126, %128
  %130 = fcmp fast ole float %118, %129
  %131 = and i1 %127, %130
  %132 = and i1 %125, %131
  br i1 %132, label %133, label %161

; <label>:133                                     ; preds = %113
  %134 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %4, float %15, float %118, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %135 = extractvalue %dx.types.ResRet.f32 %134, 0
  %136 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %137 = extractvalue %dx.types.CBufRet.f32 %136, 1
  %138 = extractvalue %dx.types.CBufRet.f32 %136, 0
  %139 = call float @dx.op.tertiary.f32(i32 46, float %135, float %138, float %137)  ; FMad(a,b,c)
  %140 = fdiv fast float 1.000000e+00, %139
  %141 = extractvalue %dx.types.CBufRet.f32 %136, 2
  %142 = fcmp fast ugt float %141, %140
  br i1 %142, label %143, label %161

; <label>:143                                     ; preds = %133
  %144 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %1, %dx.types.Handle %3, float %15, float %118, float 0.000000e+00, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %145 = extractvalue %dx.types.ResRet.f32 %144, 3
  %146 = fsub fast float %140, %27
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %149 = fmul fast float %116, %116
  %150 = fmul fast float %149, %148
  %151 = fsub fast float -0.000000e+00, %150
  %152 = call float @dx.op.unary.f32(i32 21, float %151)  ; Exp(value)
  %153 = call float @dx.op.unary.f32(i32 6, float %146)  ; FAbs(value)
  %154 = extractvalue %dx.types.CBufRet.f32 %147, 2
  %155 = fcmp fast olt float %153, %154
  %156 = select i1 %155, float 1.000000e+00, float 0.000000e+00
  %157 = fmul fast float %156, %152
  %158 = fmul fast float %157, %145
  %159 = fadd fast float %158, %114
  %160 = fadd fast float %157, %115
  br label %161

; <label>:161                                     ; preds = %143, %133, %113
  %162 = phi float [ %114, %113 ], [ %114, %133 ], [ %159, %143 ]
  %163 = phi float [ %115, %113 ], [ %115, %133 ], [ %160, %143 ]
  %164 = fadd fast float %116, 1.000000e+00
  %165 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %5, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %166 = extractvalue %dx.types.CBufRet.f32 %165, 0
  %167 = fcmp fast ugt float %164, %166
  br i1 %167, label %168, label %113

; <label>:168                                     ; preds = %161
  br label %169

; <label>:169                                     ; preds = %168, %106
  %170 = phi float [ %107, %106 ], [ %162, %168 ]
  %171 = phi float [ %108, %106 ], [ %163, %168 ]
  %172 = fdiv fast float %170, %171
  br label %173

; <label>:173                                     ; preds = %169, %0
  %174 = phi float [ %172, %169 ], [ %20, %0 ]
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %174)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

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
!dx.viewIdState = !{!13}
!dx.entryPoints = !{!14}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 5}
!2 = !{!"ps", i32 6, i32 5}
!3 = !{!4, null, !8, !10}
!4 = !{!5, !7}
!5 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i32 0, !6}
!6 = !{i32 0, i32 9}
!7 = !{i32 1, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 3, i32 1, i32 7, i32 0, !6}
!8 = !{!9}
!9 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 80, null}
!10 = !{!11, !12}
!11 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 1, i32 1, i32 0, null}
!12 = !{i32 1, %struct.SamplerState* undef, !"", i32 0, i32 3, i32 1, i32 0, null}
!13 = !{[10 x i32] [i32 8, i32 4, i32 0, i32 0, i32 0, i32 0, i32 8, i32 8, i32 0, i32 0]}
!14 = !{void ()* @main, !"main", !15, !3, null}
!15 = !{!16, !21, null}
!16 = !{!17, !19}
!17 = !{i32 0, !"SV_Position", i8 9, i8 3, !18, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!18 = !{i32 0}
!19 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !18, i8 2, i32 1, i8 4, i32 1, i8 0, !20}
!20 = !{i32 3, i32 3}
!21 = !{!22}
!22 = !{i32 0, !"SV_Target", i8 9, i8 16, !18, i8 0, i32 1, i8 4, i32 0, i8 0, !23}
!23 = !{i32 3, i32 15}

