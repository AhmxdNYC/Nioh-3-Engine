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
; shader debug name: 66fc85ef63dd346a1cf71ef505b2ea47.pdb
; shader hash: 66fc85ef63dd346a1cf71ef505b2ea47
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
;       int4 nMshltPrm1;                              ; Offset:    0
;       int4 nMshltPrm2;                              ; Offset:   16
;       uint4 nVSDrawPrm;                             ; Offset:   32
;   
;   } $Globals;                                       ; Offset:    0 Size:    48
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; uArguments                            UAV     u32         buf      U0             u2     1
; uVSDrawArguments                      UAV     u32         buf      U1             u9     1
; uVSDrawMshltMtrOffsets                UAV     u32         buf      U2            u11     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.RWBuffer<unsigned int>" = type { i32 }
%"$Globals" = type { <4 x i32>, <4 x i32>, <4 x i32> }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 2, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %6 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %7 = extractvalue %dx.types.CBufRet.i32 %6, 1
  %8 = icmp ult i32 %5, %7
  br i1 %8, label %9, label %79

; <label>:9                                       ; preds = %0
  %10 = mul i32 %5, 3
  %11 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %10, i32 undef)  ; BufferLoad(srv,index,wot)
  %12 = extractvalue %dx.types.ResRet.i32 %11, 0
  %13 = mul i32 %5, 9
  %14 = add i32 %13, 9
  %15 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %2, i32 %14, i32 undef)  ; BufferLoad(srv,index,wot)
  %16 = extractvalue %dx.types.ResRet.i32 %15, 0
  %17 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %18 = extractvalue %dx.types.CBufRet.i32 %17, 0
  %19 = lshr i32 %18, 1
  %20 = and i32 %19, 15
  %21 = add nuw nsw i32 %20, 1
  %22 = add i32 %13, 7
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %22, i32 undef, i32 0, i32 0, i32 0, i32 0, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %23 = add i32 %13, 8
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %23, i32 undef, i32 %21, i32 %21, i32 %21, i32 %21, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %24 = mul i32 %16, 3
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %14, i32 undef, i32 %24, i32 %24, i32 %24, i32 %24, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %25 = add i32 %13, 10
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %25, i32 undef, i32 0, i32 0, i32 0, i32 0, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %26 = add i32 %13, 11
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %26, i32 undef, i32 0, i32 0, i32 0, i32 0, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %27 = mul i32 %16, 12
  %28 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %29 = extractvalue %dx.types.CBufRet.i32 %28, 2
  %30 = call i32 @dx.op.binary.i32(i32 39, i32 %29, i32 %27)  ; UMax(a,b)
  %31 = sub i32 %30, %27
  %32 = add i32 %10, 2
  %33 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %32, i32 undef)  ; BufferLoad(srv,index,wot)
  %34 = extractvalue %dx.types.ResRet.i32 %33, 0
  %35 = mul i32 %34, 12
  %36 = extractvalue %dx.types.CBufRet.i32 %28, 3
  %37 = udiv i32 %36, 52
  %38 = call i32 @dx.op.binary.i32(i32 39, i32 %12, i32 %37)  ; UMax(a,b)
  %39 = sub i32 %38, %12
  %40 = add i32 %10, 1
  %41 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %40, i32 undef)  ; BufferLoad(srv,index,wot)
  %42 = extractvalue %dx.types.ResRet.i32 %41, 0
  %43 = call i32 @dx.op.binary.i32(i32 40, i32 %42, i32 %39)  ; UMin(a,b)
  %44 = icmp ugt i32 %35, %31
  br i1 %44, label %45, label %48

; <label>:45                                      ; preds = %9
  %46 = udiv i32 %31, 1536
  %47 = call i32 @dx.op.binary.i32(i32 40, i32 %43, i32 %46)  ; UMin(a,b)
  br label %48

; <label>:48                                      ; preds = %45, %9
  %49 = phi i32 [ %47, %45 ], [ %43, %9 ]
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %40, i32 undef, i32 %49, i32 %49, i32 %49, i32 %49, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %50 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %51 = extractvalue %dx.types.CBufRet.i32 %50, 0
  %52 = shl i32 %5, 2
  %53 = add i32 %51, %52
  %54 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %3, i32 %53, i32 undef)  ; BufferLoad(srv,index,wot)
  %55 = extractvalue %dx.types.ResRet.i32 %54, 0
  %56 = icmp ult i32 %49, %55
  br i1 %56, label %57, label %62

; <label>:57                                      ; preds = %48
  %58 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %59 = extractvalue %dx.types.CBufRet.i32 %58, 1
  %60 = add i32 %59, 15
  %61 = lshr i32 %60, 4
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 4, i32 undef, i32 %61, i32 %61, i32 %61, i32 %61, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 5, i32 undef, i32 1, i32 1, i32 1, i32 1, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 6, i32 undef, i32 1, i32 1, i32 1, i32 1, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %62

; <label>:62                                      ; preds = %57, %48
  %63 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %64 = extractvalue %dx.types.CBufRet.i32 %63, 1
  %65 = and i32 %64, 15
  %66 = icmp ne i32 %65, 1
  %67 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %68 = extractvalue %dx.types.CBufRet.i32 %67, 0
  %69 = icmp ne i32 %68, 0
  %70 = or i1 %66, %69
  br i1 %70, label %71, label %79

; <label>:71                                      ; preds = %62
  %72 = call i32 @dx.op.binary.i32(i32 39, i32 %55, i32 %49)  ; UMax(a,b)
  %73 = sub i32 %72, %49
  %74 = mul i32 %73, 384
  %75 = add i32 %13, 12
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %75, i32 undef, i32 %74, i32 %74, i32 %74, i32 %74, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %76 = add i32 %13, 13
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %76, i32 undef, i32 %21, i32 %21, i32 %21, i32 %21, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %77 = add i32 %13, 14
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %77, i32 undef, i32 0, i32 0, i32 0, i32 0, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %78 = add i32 %13, 15
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %78, i32 undef, i32 0, i32 0, i32 0, i32 0, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %79

; <label>:79                                      ; preds = %71, %62, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #2

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

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
!dx.entryPoints = !{!12}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{null, !5, !10, null}
!5 = !{!6, !8, !9}
!6 = !{i32 0, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 2, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!7 = !{i32 0, i32 5}
!8 = !{i32 1, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 9, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!9 = !{i32 2, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 11, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!10 = !{!11}
!11 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 48, null}
!12 = !{void ()* @main, !"main", null, !4, !13}
!13 = !{i32 4, !14}
!14 = !{i32 1024, i32 1, i32 1}

