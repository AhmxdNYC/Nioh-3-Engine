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
; shader debug name: 3fb17034e1ff3898e194ba3b1005f693.pdb
; shader hash: 3fb17034e1ff3898e194ba3b1005f693
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
;       uint4 nVSDrawPrm;                             ; Offset:    0
;   
;   } $Globals;                                       ; Offset:    0 Size:    16
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; uVSDrawArguments                      UAV     u32         buf      U0             u9     1
; uVSDrawMshltMtrOffsets                UAV     u32         buf      U1            u11     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.RWBuffer<unsigned int>" = type { i32 }
%"$Globals" = type { <4 x i32> }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %5 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %6 = shl i32 %4, 10
  %7 = shl i32 %5, 6
  %8 = add i32 %7, %6
  %9 = or i32 %6, 64
  %10 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %3, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %11 = extractvalue %dx.types.CBufRet.i32 %10, 1
  %12 = icmp ult i32 %8, %11
  br i1 %12, label %13, label %44

; <label>:13                                      ; preds = %0
  %14 = icmp eq i32 %5, 0
  %15 = add i32 %9, -1024
  %16 = select i1 %14, i32 %15, i32 %9
  %17 = icmp ugt i32 %16, %8
  br i1 %17, label %35, label %18

; <label>:18                                      ; preds = %13
  br label %19

; <label>:19                                      ; preds = %19, %18
  %20 = phi i32 [ %26, %19 ], [ 0, %18 ]
  %21 = phi i32 [ %31, %19 ], [ 0, %18 ]
  %22 = phi i32 [ %32, %19 ], [ %16, %18 ]
  %23 = mul i32 %22, 3
  %24 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %23, i32 undef)  ; BufferLoad(srv,index,wot)
  %25 = extractvalue %dx.types.ResRet.i32 %24, 0
  %26 = add i32 %25, %20
  %27 = mul i32 %22, 9
  %28 = or i32 %27, 9
  %29 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %2, i32 %28, i32 undef)  ; BufferLoad(srv,index,wot)
  %30 = extractvalue %dx.types.ResRet.i32 %29, 0
  %31 = add i32 %30, %21
  %32 = add i32 %22, 64
  %33 = icmp ugt i32 %32, %8
  br i1 %33, label %34, label %19

; <label>:34                                      ; preds = %19
  br label %35

; <label>:35                                      ; preds = %34, %13
  %36 = phi i32 [ 0, %13 ], [ %26, %34 ]
  %37 = phi i32 [ 0, %13 ], [ %31, %34 ]
  %38 = mul i32 %11, 3
  %39 = shl i32 %4, 4
  %40 = add i32 %39, %5
  %41 = shl i32 %40, 1
  %42 = add i32 %38, %41
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %42, i32 undef, i32 %36, i32 %36, i32 %36, i32 %36, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %43 = add i32 %42, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %43, i32 undef, i32 %37, i32 %37, i32 %37, i32 %37, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %44

; <label>:44                                      ; preds = %35, %0
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
!dx.entryPoints = !{!11}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{null, !5, !9, null}
!5 = !{!6, !8}
!6 = !{i32 0, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 9, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!7 = !{i32 0, i32 5}
!8 = !{i32 1, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 11, i32 1, i32 10, i1 false, i1 false, i1 false, !7}
!9 = !{!10}
!10 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 16, null}
!11 = !{void ()* @main, !"main", null, !4, !12}
!12 = !{i32 4, !13}
!13 = !{i32 32, i32 16, i32 1}

