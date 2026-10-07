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
; shader debug name: 9af3c486cc2eb4d912fc985545b85f73.pdb
; shader hash: 9af3c486cc2eb4d912fc985545b85f73
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
;       uint nSrcOffsetSize[4];                       ; Offset:    0
;       uint nDstOffsetSize[4];                       ; Offset:   64
;       uint nIsReverseZ;                             ; Offset:  116
;       float fSrcSign;                               ; Offset:  120
;       float fSrcFace;                               ; Offset:  124
;       uint nSrcSlice;                               ; Offset:  128
;   
;   } $Globals;                                       ; Offset:    0 Size:   132
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smptSrcCubeArray                sampler      NA          NA      S0             s8     1
; tSrcCubeArray                     texture     f32   cubearray      T0             t8     1
; uDst                                  UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%"class.TextureCubeArray<vector<float, 4> >" = type { <4 x float> }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%"$Globals" = type { [4 x i32], [4 x i32], i32, float, float, i32 }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %6 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %7 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %8 = extractvalue %dx.types.CBufRet.i32 %7, 0
  %9 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %10 = extractvalue %dx.types.CBufRet.i32 %9, 0
  %11 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %12 = extractvalue %dx.types.CBufRet.i32 %11, 0
  %13 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %14 = extractvalue %dx.types.CBufRet.i32 %13, 0
  %15 = icmp ult i32 %5, %12
  %16 = icmp ult i32 %6, %14
  %17 = and i1 %15, %16
  br i1 %17, label %18, label %170

; <label>:18                                      ; preds = %0
  %19 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %20 = extractvalue %dx.types.CBufRet.i32 %19, 0
  %21 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %22 = extractvalue %dx.types.CBufRet.i32 %21, 0
  %23 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %24 = extractvalue %dx.types.CBufRet.i32 %23, 0
  %25 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %26 = extractvalue %dx.types.CBufRet.i32 %25, 0
  %27 = uitofp i32 %26 to float
  %28 = uitofp i32 %24 to float
  %29 = uitofp i32 %12 to float
  %30 = uitofp i32 %14 to float
  %31 = uitofp i32 %5 to float
  %32 = uitofp i32 %6 to float
  %33 = fadd fast float %31, 5.000000e-01
  %34 = fadd fast float %32, 5.000000e-01
  %35 = fdiv fast float %33, %29
  %36 = fdiv fast float %34, %30
  %37 = uitofp i32 %22 to float
  %38 = uitofp i32 %20 to float
  %39 = fdiv fast float %37, %29
  %40 = fdiv fast float %38, %30
  %41 = call float @dx.op.unary.f32(i32 28, float %39)  ; Round_pi(value)
  %42 = call float @dx.op.unary.f32(i32 28, float %40)  ; Round_pi(value)
  %43 = fmul fast float %41, %29
  %44 = fmul fast float %42, %30
  %45 = fdiv fast float 1.000000e+00, %43
  %46 = fdiv fast float 1.000000e+00, %44
  %47 = fptoui float %41 to i32
  %48 = fptoui float %42 to i32
  %49 = extractvalue %dx.types.CBufRet.i32 %13, 1
  %50 = icmp eq i32 %49, 0
  %51 = icmp eq i32 %48, 0
  br i1 %50, label %108, label %52

; <label>:52                                      ; preds = %18
  br i1 %51, label %166, label %53

; <label>:53                                      ; preds = %52
  br label %54

; <label>:54                                      ; preds = %104, %53
  %55 = phi float [ %105, %104 ], [ 2.000000e+00, %53 ]
  %56 = phi i32 [ %106, %104 ], [ 0, %53 ]
  %57 = icmp eq i32 %47, 0
  br i1 %57, label %104, label %58

; <label>:58                                      ; preds = %54
  br label %59

; <label>:59                                      ; preds = %91, %58
  %60 = phi float [ %100, %91 ], [ %55, %58 ]
  %61 = phi i32 [ %101, %91 ], [ 0, %58 ]
  %62 = fadd fast float %27, %35
  %63 = uitofp i32 %61 to float
  %64 = fmul fast float %63, %45
  %65 = fadd fast float %62, %64
  %66 = fadd fast float %36, %28
  %67 = uitofp i32 %56 to float
  %68 = fmul fast float %67, %46
  %69 = fadd fast float %66, %68
  %70 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %71 = extractvalue %dx.types.CBufRet.f32 %70, 3
  %72 = fcmp fast olt float %71, 1.500000e+00
  %73 = extractvalue %dx.types.CBufRet.f32 %70, 2
  %74 = fmul fast float %69, 2.000000e+00
  br i1 %72, label %75, label %80

; <label>:75                                      ; preds = %59
  %76 = fmul fast float %65, -2.000000e+00
  %77 = fmul fast float %76, %73
  %78 = fsub fast float 1.000000e+00, %74
  %79 = fadd fast float %77, %73
  br label %91

; <label>:80                                      ; preds = %59
  %81 = fcmp fast olt float %71, 3.500000e+00
  %82 = fmul fast float %65, 2.000000e+00
  br i1 %81, label %83, label %87

; <label>:83                                      ; preds = %80
  %84 = fmul fast float %74, %73
  %85 = fadd fast float %82, -1.000000e+00
  %86 = fsub fast float %84, %73
  br label %91

; <label>:87                                      ; preds = %80
  %88 = fmul fast float %82, %73
  %89 = fsub fast float %88, %73
  %90 = fsub fast float 1.000000e+00, %74
  br label %91

; <label>:91                                      ; preds = %87, %83, %75
  %92 = phi float [ %73, %75 ], [ %85, %83 ], [ %89, %87 ]
  %93 = phi float [ %78, %75 ], [ %73, %83 ], [ %90, %87 ]
  %94 = phi float [ %79, %75 ], [ %86, %83 ], [ %73, %87 ]
  %95 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %96 = extractvalue %dx.types.CBufRet.i32 %95, 0
  %97 = uitofp i32 %96 to float
  %98 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %92, float %93, float %94, float %97, i32 undef, i32 undef, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %99 = extractvalue %dx.types.ResRet.f32 %98, 0
  %100 = call float @dx.op.binary.f32(i32 36, float %99, float %60)  ; FMin(a,b)
  %101 = add nuw i32 %61, 1
  %102 = icmp eq i32 %101, %47
  br i1 %102, label %103, label %59

; <label>:103                                     ; preds = %91
  br label %104

; <label>:104                                     ; preds = %103, %54
  %105 = phi float [ %55, %54 ], [ %100, %103 ]
  %106 = add nuw i32 %56, 1
  %107 = icmp eq i32 %106, %48
  br i1 %107, label %165, label %54

; <label>:108                                     ; preds = %18
  br i1 %51, label %166, label %109

; <label>:109                                     ; preds = %108
  br label %110

; <label>:110                                     ; preds = %160, %109
  %111 = phi float [ %161, %160 ], [ -2.000000e+00, %109 ]
  %112 = phi i32 [ %162, %160 ], [ 0, %109 ]
  %113 = icmp eq i32 %47, 0
  br i1 %113, label %160, label %114

; <label>:114                                     ; preds = %110
  br label %115

; <label>:115                                     ; preds = %147, %114
  %116 = phi float [ %156, %147 ], [ %111, %114 ]
  %117 = phi i32 [ %157, %147 ], [ 0, %114 ]
  %118 = fadd fast float %27, %35
  %119 = uitofp i32 %117 to float
  %120 = fmul fast float %119, %45
  %121 = fadd fast float %118, %120
  %122 = fadd fast float %36, %28
  %123 = uitofp i32 %112 to float
  %124 = fmul fast float %123, %46
  %125 = fadd fast float %122, %124
  %126 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %127 = extractvalue %dx.types.CBufRet.f32 %126, 3
  %128 = fcmp fast olt float %127, 1.500000e+00
  %129 = extractvalue %dx.types.CBufRet.f32 %126, 2
  %130 = fmul fast float %125, 2.000000e+00
  br i1 %128, label %131, label %136

; <label>:131                                     ; preds = %115
  %132 = fmul fast float %121, -2.000000e+00
  %133 = fmul fast float %132, %129
  %134 = fsub fast float 1.000000e+00, %130
  %135 = fadd fast float %133, %129
  br label %147

; <label>:136                                     ; preds = %115
  %137 = fcmp fast olt float %127, 3.500000e+00
  %138 = fmul fast float %121, 2.000000e+00
  br i1 %137, label %139, label %143

; <label>:139                                     ; preds = %136
  %140 = fmul fast float %130, %129
  %141 = fadd fast float %138, -1.000000e+00
  %142 = fsub fast float %140, %129
  br label %147

; <label>:143                                     ; preds = %136
  %144 = fmul fast float %138, %129
  %145 = fsub fast float %144, %129
  %146 = fsub fast float 1.000000e+00, %130
  br label %147

; <label>:147                                     ; preds = %143, %139, %131
  %148 = phi float [ %129, %131 ], [ %141, %139 ], [ %145, %143 ]
  %149 = phi float [ %134, %131 ], [ %129, %139 ], [ %146, %143 ]
  %150 = phi float [ %135, %131 ], [ %142, %139 ], [ %129, %143 ]
  %151 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %152 = extractvalue %dx.types.CBufRet.i32 %151, 0
  %153 = uitofp i32 %152 to float
  %154 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %148, float %149, float %150, float %153, i32 undef, i32 undef, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %155 = extractvalue %dx.types.ResRet.f32 %154, 0
  %156 = call float @dx.op.binary.f32(i32 35, float %155, float %116)  ; FMax(a,b)
  %157 = add nuw i32 %117, 1
  %158 = icmp eq i32 %157, %47
  br i1 %158, label %159, label %115

; <label>:159                                     ; preds = %147
  br label %160

; <label>:160                                     ; preds = %159, %110
  %161 = phi float [ %111, %110 ], [ %156, %159 ]
  %162 = add nuw i32 %112, 1
  %163 = icmp eq i32 %162, %48
  br i1 %163, label %164, label %110

; <label>:164                                     ; preds = %160
  br label %166

; <label>:165                                     ; preds = %104
  br label %166

; <label>:166                                     ; preds = %165, %164, %108, %52
  %167 = phi float [ 2.000000e+00, %52 ], [ -2.000000e+00, %108 ], [ %161, %164 ], [ %105, %165 ]
  %168 = add i32 %8, %5
  %169 = add i32 %10, %6
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %168, i32 %169, i32 undef, float %167, float %167, float %167, float %167, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %170

; <label>:170                                     ; preds = %166, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

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
!dx.entryPoints = !{!14}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !8, !10, !12}
!5 = !{!6}
!6 = !{i32 0, %"class.TextureCubeArray<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 9, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!10 = !{!11}
!11 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 132, null}
!12 = !{!13}
!13 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!14 = !{void ()* @main, !"main", null, !4, !15}
!15 = !{i32 4, !16}
!16 = !{i32 8, i32 8, i32 1}

