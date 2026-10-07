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
; shader debug name: aaa4d66d80497425617c02582cdbbd8a.pdb
; shader hash: aaa4d66d80497425617c02582cdbbd8a
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
;       uint nSrcLevel;                               ; Offset:  120
;   
;   } $Globals;                                       ; Offset:    0 Size:   124
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smptSrc                         sampler      NA          NA      S0             s8     1
; tSrc                              texture     f32          2d      T0             t8     1
; uDst                                  UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%"$Globals" = type { [4 x i32], [4 x i32], i32, i32 }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 8, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %6 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %7 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %8 = extractvalue %dx.types.CBufRet.i32 %7, 0
  %9 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %10 = extractvalue %dx.types.CBufRet.i32 %9, 0
  %11 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %12 = extractvalue %dx.types.CBufRet.i32 %11, 0
  %13 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %14 = extractvalue %dx.types.CBufRet.i32 %13, 0
  %15 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %16 = extractvalue %dx.types.CBufRet.i32 %15, 0
  %17 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %18 = extractvalue %dx.types.CBufRet.i32 %17, 0
  %19 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %20 = extractvalue %dx.types.CBufRet.i32 %19, 0
  %21 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %22 = extractvalue %dx.types.CBufRet.i32 %21, 0
  %23 = icmp ult i32 %5, %20
  %24 = icmp ult i32 %6, %22
  %25 = and i1 %23, %24
  br i1 %25, label %26, label %123

; <label>:26                                      ; preds = %0
  %27 = icmp eq i32 %12, %20
  %28 = icmp eq i32 %14, %22
  %29 = and i1 %27, %28
  %30 = extractvalue %dx.types.CBufRet.i32 %21, 2
  br i1 %29, label %31, label %34

; <label>:31                                      ; preds = %26
  %32 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 %30, i32 %5, i32 %6, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %33 = extractvalue %dx.types.ResRet.f32 %32, 0
  br label %119

; <label>:34                                      ; preds = %26
  %35 = shl i32 %20, 1
  %36 = icmp eq i32 %12, %35
  %37 = shl i32 %22, 1
  %38 = icmp eq i32 %14, %37
  %39 = and i1 %36, %38
  %40 = extractvalue %dx.types.CBufRet.i32 %21, 1
  %41 = icmp eq i32 %40, 0
  br i1 %39, label %42, label %65

; <label>:42                                      ; preds = %34
  %43 = shl i32 %5, 1
  %44 = shl i32 %6, 1
  %45 = add i32 %8, %43
  %46 = add i32 %10, %44
  %47 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 %30, i32 %45, i32 %46, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %48 = extractvalue %dx.types.ResRet.f32 %47, 0
  %49 = add i32 %46, 1
  %50 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 %30, i32 %45, i32 %49, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %51 = extractvalue %dx.types.ResRet.f32 %50, 0
  %52 = add i32 %45, 1
  %53 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 %30, i32 %52, i32 %46, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %54 = extractvalue %dx.types.ResRet.f32 %53, 0
  %55 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 %30, i32 %52, i32 %49, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %56 = extractvalue %dx.types.ResRet.f32 %55, 0
  br i1 %41, label %61, label %57

; <label>:57                                      ; preds = %42
  %58 = call float @dx.op.binary.f32(i32 36, float %54, float %56)  ; FMin(a,b)
  %59 = call float @dx.op.binary.f32(i32 36, float %48, float %51)  ; FMin(a,b)
  %60 = call float @dx.op.binary.f32(i32 36, float %59, float %58)  ; FMin(a,b)
  br label %119

; <label>:61                                      ; preds = %42
  %62 = call float @dx.op.binary.f32(i32 35, float %54, float %56)  ; FMax(a,b)
  %63 = call float @dx.op.binary.f32(i32 35, float %48, float %51)  ; FMax(a,b)
  %64 = call float @dx.op.binary.f32(i32 35, float %63, float %62)  ; FMax(a,b)
  br label %119

; <label>:65                                      ; preds = %34
  %66 = uitofp i32 %5 to float
  %67 = uitofp i32 %6 to float
  %68 = fadd fast float %66, 5.000000e-01
  %69 = fadd fast float %67, 5.000000e-01
  %70 = uitofp i32 %20 to float
  %71 = uitofp i32 %22 to float
  %72 = fdiv fast float %68, %70
  %73 = fdiv fast float %69, %71
  %74 = uitofp i32 %30 to float
  %75 = fadd fast float %66, 2.500000e-01
  %76 = fdiv fast float %75, %70
  %77 = fadd fast float %67, 2.500000e-01
  %78 = fdiv fast float %77, %71
  %79 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %76, float %78, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %80 = extractvalue %dx.types.ResRet.f32 %79, 0
  %81 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %76, float %73, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %82 = extractvalue %dx.types.ResRet.f32 %81, 0
  %83 = fadd fast float %67, 7.500000e-01
  %84 = fdiv fast float %83, %71
  %85 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %76, float %84, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %86 = extractvalue %dx.types.ResRet.f32 %85, 0
  %87 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %72, float %78, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %88 = extractvalue %dx.types.ResRet.f32 %87, 0
  %89 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %72, float %73, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %90 = extractvalue %dx.types.ResRet.f32 %89, 0
  %91 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %72, float %84, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %92 = extractvalue %dx.types.ResRet.f32 %91, 0
  %93 = fadd fast float %66, 7.500000e-01
  %94 = fdiv fast float %93, %70
  %95 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %94, float %78, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %96 = extractvalue %dx.types.ResRet.f32 %95, 0
  %97 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %94, float %73, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %98 = extractvalue %dx.types.ResRet.f32 %97, 0
  %99 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %94, float %84, float undef, float undef, i32 0, i32 0, i32 undef, float %74)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %100 = extractvalue %dx.types.ResRet.f32 %99, 0
  br i1 %41, label %110, label %101

; <label>:101                                     ; preds = %65
  %102 = call float @dx.op.binary.f32(i32 36, float %86, float %88)  ; FMin(a,b)
  %103 = call float @dx.op.binary.f32(i32 36, float %80, float %82)  ; FMin(a,b)
  %104 = call float @dx.op.binary.f32(i32 36, float %103, float %102)  ; FMin(a,b)
  %105 = call float @dx.op.binary.f32(i32 36, float %96, float %98)  ; FMin(a,b)
  %106 = call float @dx.op.binary.f32(i32 36, float %90, float %92)  ; FMin(a,b)
  %107 = call float @dx.op.binary.f32(i32 36, float %106, float %105)  ; FMin(a,b)
  %108 = call float @dx.op.binary.f32(i32 36, float %104, float %107)  ; FMin(a,b)
  %109 = call float @dx.op.binary.f32(i32 36, float %108, float %100)  ; FMin(a,b)
  br label %119

; <label>:110                                     ; preds = %65
  %111 = call float @dx.op.binary.f32(i32 35, float %86, float %88)  ; FMax(a,b)
  %112 = call float @dx.op.binary.f32(i32 35, float %80, float %82)  ; FMax(a,b)
  %113 = call float @dx.op.binary.f32(i32 35, float %112, float %111)  ; FMax(a,b)
  %114 = call float @dx.op.binary.f32(i32 35, float %96, float %98)  ; FMax(a,b)
  %115 = call float @dx.op.binary.f32(i32 35, float %90, float %92)  ; FMax(a,b)
  %116 = call float @dx.op.binary.f32(i32 35, float %115, float %114)  ; FMax(a,b)
  %117 = call float @dx.op.binary.f32(i32 35, float %113, float %116)  ; FMax(a,b)
  %118 = call float @dx.op.binary.f32(i32 35, float %117, float %100)  ; FMax(a,b)
  br label %119

; <label>:119                                     ; preds = %110, %101, %61, %57, %31
  %120 = phi float [ %33, %31 ], [ %60, %57 ], [ %64, %61 ], [ %109, %101 ], [ %118, %110 ]
  %121 = add i32 %16, %5
  %122 = add i32 %18, %6
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %121, i32 %122, i32 undef, float %120, float %120, float %120, float %120, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %123

; <label>:123                                     ; preds = %119, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

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
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!10 = !{!11}
!11 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 124, null}
!12 = !{!13}
!13 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!14 = !{void ()* @main, !"main", null, !4, !15}
!15 = !{i32 4, !16}
!16 = !{i32 8, i32 8, i32 1}

