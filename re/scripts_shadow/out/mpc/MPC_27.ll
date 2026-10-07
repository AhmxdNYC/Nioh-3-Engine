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
; shader debug name: 71ab6ae905d2113919ea633b314f4dbc.pdb
; shader hash: 71ab6ae905d2113919ea633b314f4dbc
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
;       int4 nMshltPrm0;                              ; Offset:    0
;   
;   } $Globals;                                       ; Offset:    0 Size:    16
;
; }
;
; Resource bind info for tG1mOffsets
; {
;
;   struct struct.S_G1M_OFFSET
;   {
;
;       uint rootBufferIdx;                           ; Offset:    0
;       uint rootOffset;                              ; Offset:    4
;       uint vtxRscInfoOff;                           ; Offset:    8
;       uint idxRscInfoOff;                           ; Offset:   12
;       uint mshltInstanceBoundingRscOff;             ; Offset:   16
;       uint mshltPrimitiveIdxRscOff;                 ; Offset:   20
;       uint mshltRscOff;                             ; Offset:   24
;       uint mshltLODRscOff;                          ; Offset:   28
;       uint mshltCullRscOff;                         ; Offset:   32
;       uint mshltIdxRscOff;                          ; Offset:   36
;       uint primitivePaletteOff;                     ; Offset:   40
;       uint mshltMtxsetOffsetOff;                    ; Offset:   44
;   
;   } $Element;                                       ; Offset:    0 Size:    48
;
; }
;
; Resource bind info for tMeshletInstanceOffsets
; {
;
;   struct struct.S_INSTANCE_OFFSET
;   {
;
;       uint nMtxPltOff;                              ; Offset:    0
;       uint nInstCullCodeOff;                        ; Offset:    4
;       uint nMshltCullCodeOff;                       ; Offset:    8
;       uint nPrimToMtrIdConvertTableOff;             ; Offset:   12
;   
;   } $Element;                                       ; Offset:    0 Size:    16
;
; }
;
; Resource bind info for tMaterialIndexPayloadOffsets
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for uToMshltDrawPayloads
; {
;
;   struct struct.S_PAYLOAD_TO_MSHLTDRAW
;   {
;
;       uint nData[2];                                ; Offset:    0
;   
;   } $Element;                                       ; Offset:    0 Size:     8
;
; }
;
; Resource bind info for uVSDrawMshltInfo
; {
;
;   struct struct.S_VS_DRAW_MSHLT_INFO
;   {
;
;       float4 fMeshletInstanceBSphere;               ; Offset:    0
;       uint nMeshletMatrixPaletteIndex;              ; Offset:   16
;       uint nMeshletMatrixPaletteOffset;             ; Offset:   20
;       uint nRootIdx;                                ; Offset:   24
;       uint nVtxInfoIdx;                             ; Offset:   28
;       uint nIdxInfoIdx;                             ; Offset:   32
;       uint nG1mRootOffset;                          ; Offset:   36
;       uint nMeshletVertexOffset;                    ; Offset:   40
;       uint nMeshletMatrixPaletteIndexForSkinning;   ; Offset:   44
;       uint nBTexRegInfoIdx;                         ; Offset:   48
;   
;   } $Element;                                       ; Offset:    0 Size:    52
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; tMeshletG1mBuffer                 texture    byte         r/o      T0             t1    16
; tG1mOffsets                       texture  struct         r/o      T1            t17     1
; tMeshletInstanceOffsets           texture  struct         r/o      T2            t19     1
; tMaterialIndexPayloadOffsets      texture  struct         r/o      T3            t21     1
; uToMshltDrawPayloads                  UAV  struct         r/w      U0             u3     1
; uVSDrawArguments                      UAV     u32         buf      U1             u9     1
; uVSDrawIndexStream                    UAV     u32         buf      U2            u10     1
; uVSDrawMshltMtrOffsets                UAV     u32         buf      U3            u11     1
; uVSDrawMshltInfo                      UAV  struct         r/w      U4            u12     1
; uVSDrawSubPayload                     UAV     u32         buf      U5            u13     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%struct.ByteAddressBuffer = type { i32 }
%"class.StructuredBuffer<S_G1M_OFFSET>" = type { %struct.S_G1M_OFFSET }
%struct.S_G1M_OFFSET = type { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%"class.StructuredBuffer<S_INSTANCE_OFFSET>" = type { %struct.S_INSTANCE_OFFSET }
%struct.S_INSTANCE_OFFSET = type { i32, i32, i32, i32 }
%"class.StructuredBuffer<unsigned int>" = type { i32 }
%"class.RWStructuredBuffer<S_PAYLOAD_TO_MSHLTDRAW>" = type { %struct.S_PAYLOAD_TO_MSHLTDRAW }
%struct.S_PAYLOAD_TO_MSHLTDRAW = type { [2 x i32] }
%"class.RWBuffer<unsigned int>" = type { i32 }
%"class.RWStructuredBuffer<S_VS_DRAW_MSHLT_INFO>" = type { %struct.S_VS_DRAW_MSHLT_INFO }
%struct.S_VS_DRAW_MSHLT_INFO = type { <4 x float>, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%"$Globals" = type { <4 x i32> }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 5, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 4, i32 12, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 3, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 2, i32 10, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 21, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 19, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 17, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %12 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %13 = call i32 @dx.op.threadId.i32(i32 93, i32 2)  ; ThreadId(component)
  %14 = shl i32 %11, 11
  %15 = add i32 %13, %14
  %16 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %17 = extractvalue %dx.types.ResRet.i32 %16, 0
  %18 = icmp ult i32 %15, %17
  br i1 %18, label %19, label %169

; <label>:19                                      ; preds = %0
  %20 = shl i32 %15, 1
  %21 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %20, i32 undef)  ; BufferLoad(srv,index,wot)
  %22 = extractvalue %dx.types.ResRet.i32 %21, 0
  %23 = or i32 %20, 1
  %24 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %1, i32 %23, i32 undef)  ; BufferLoad(srv,index,wot)
  %25 = extractvalue %dx.types.ResRet.i32 %24, 0
  %26 = mul i32 %22, 3
  %27 = add i32 %26, 1
  %28 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %3, i32 %27, i32 undef)  ; BufferLoad(srv,index,wot)
  %29 = extractvalue %dx.types.ResRet.i32 %28, 0
  %30 = icmp ult i32 %25, %29
  br i1 %30, label %31, label %169

; <label>:31                                      ; preds = %19
  %32 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %22, i32 0)  ; BufferLoad(srv,index,wot)
  %33 = extractvalue %dx.types.ResRet.i32 %32, 0
  %34 = add i32 %33, %25
  %35 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %6, i32 %34, i32 0)  ; BufferLoad(srv,index,wot)
  %36 = extractvalue %dx.types.ResRet.i32 %35, 0
  %37 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %6, i32 %34, i32 4)  ; BufferLoad(srv,index,wot)
  %38 = extractvalue %dx.types.ResRet.i32 %37, 0
  %39 = lshr i32 %36, 6
  %40 = and i32 %39, 262143
  %41 = shl i32 %36, 14
  %42 = and i32 %41, 1032192
  %43 = lshr i32 %38, 18
  %44 = or i32 %42, %43
  %45 = and i32 %38, 262143
  %46 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 0)  ; BufferLoad(srv,index,wot)
  %47 = extractvalue %dx.types.ResRet.i32 %46, 0
  %48 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 4)  ; BufferLoad(srv,index,wot)
  %49 = extractvalue %dx.types.ResRet.i32 %48, 0
  %50 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 8)  ; BufferLoad(srv,index,wot)
  %51 = extractvalue %dx.types.ResRet.i32 %50, 0
  %52 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 12)  ; BufferLoad(srv,index,wot)
  %53 = extractvalue %dx.types.ResRet.i32 %52, 0
  %54 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 16)  ; BufferLoad(srv,index,wot)
  %55 = extractvalue %dx.types.ResRet.i32 %54, 0
  %56 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 20)  ; BufferLoad(srv,index,wot)
  %57 = extractvalue %dx.types.ResRet.i32 %56, 0
  %58 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 24)  ; BufferLoad(srv,index,wot)
  %59 = extractvalue %dx.types.ResRet.i32 %58, 0
  %60 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 36)  ; BufferLoad(srv,index,wot)
  %61 = extractvalue %dx.types.ResRet.i32 %60, 0
  %62 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 40)  ; BufferLoad(srv,index,wot)
  %63 = extractvalue %dx.types.ResRet.i32 %62, 0
  %64 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %40, i32 44)  ; BufferLoad(srv,index,wot)
  %65 = extractvalue %dx.types.ResRet.i32 %64, 0
  %66 = mul nuw nsw i32 %45, 12
  %67 = add i32 %49, %66
  %68 = add i32 %67, %61
  %69 = add i32 %47, 1
  %70 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 %69, i1 true)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %71 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %68, i32 undef)  ; BufferLoad(srv,index,wot)
  %72 = extractvalue %dx.types.ResRet.i32 %71, 0
  %73 = extractvalue %dx.types.ResRet.i32 %71, 2
  %74 = lshr i32 %73, 16
  %75 = add i32 %63, %49
  %76 = mul nuw nsw i32 %74, 56
  %77 = add i32 %75, %76
  %78 = add i32 %77, 4
  %79 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %78, i32 undef)  ; BufferLoad(srv,index,wot)
  %80 = extractvalue %dx.types.ResRet.i32 %79, 0
  %81 = add i32 %77, 28
  %82 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %81, i32 undef)  ; BufferLoad(srv,index,wot)
  %83 = extractvalue %dx.types.ResRet.i32 %82, 0
  %84 = shl i32 %72, 4
  %85 = add i32 %59, %49
  %86 = add i32 %85, %84
  %87 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %86, i32 undef)  ; BufferLoad(srv,index,wot)
  %88 = extractvalue %dx.types.ResRet.i32 %87, 0
  %89 = extractvalue %dx.types.ResRet.i32 %87, 1
  %90 = extractvalue %dx.types.ResRet.i32 %87, 2
  %91 = icmp ult i32 %12, %89
  br i1 %91, label %92, label %169

; <label>:92                                      ; preds = %31
  %93 = icmp eq i32 %12, 0
  br i1 %93, label %94, label %142

; <label>:94                                      ; preds = %92
  %95 = and i32 %73, 65535
  %96 = mul nuw nsw i32 %95, 44
  %97 = add i32 %55, %49
  %98 = add i32 %97, %96
  %99 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %98, i32 undef)  ; BufferLoad(srv,index,wot)
  %100 = extractvalue %dx.types.ResRet.i32 %99, 0
  %101 = extractvalue %dx.types.ResRet.i32 %99, 1
  %102 = extractvalue %dx.types.ResRet.i32 %99, 2
  %103 = extractvalue %dx.types.ResRet.i32 %99, 3
  %104 = bitcast i32 %100 to float
  %105 = bitcast i32 %101 to float
  %106 = bitcast i32 %102 to float
  %107 = bitcast i32 %103 to float
  %108 = add i32 %49, 40
  %109 = add i32 %108, %55
  %110 = add i32 %109, %96
  %111 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %110, i32 undef)  ; BufferLoad(srv,index,wot)
  %112 = extractvalue %dx.types.ResRet.i32 %111, 0
  %113 = and i32 %112, 65535
  %114 = lshr i32 %112, 17
  %115 = and i32 %112, 65536
  %116 = icmp ne i32 %115, 0
  %117 = add i32 %65, %49
  %118 = shl nuw nsw i32 %114, 2
  %119 = add i32 %117, %118
  %120 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %70, i32 %119, i32 undef)  ; BufferLoad(srv,index,wot)
  %121 = extractvalue %dx.types.ResRet.i32 %120, 0
  %122 = lshr i32 %121, 16
  %123 = and i32 %121, 65535
  %124 = select i1 %116, i32 %122, i32 %123
  %125 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %10, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %126 = extractvalue %dx.types.CBufRet.i32 %125, 0
  %127 = add nsw i32 %126, %44
  %128 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %8, i32 %127, i32 0)  ; BufferLoad(srv,index,wot)
  %129 = extractvalue %dx.types.ResRet.i32 %128, 0
  %130 = add nuw nsw i32 %124, %113
  %131 = mul nuw nsw i32 %130, 3
  %132 = mul nuw nsw i32 %124, 3
  %133 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %8, i32 %127, i32 12)  ; BufferLoad(srv,index,wot)
  %134 = extractvalue %dx.types.ResRet.i32 %133, 0
  %135 = and i32 %134, 4194303
  %136 = add nuw nsw i32 %135, %74
  %137 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %3, i32 %26, i32 undef)  ; BufferLoad(srv,index,wot)
  %138 = extractvalue %dx.types.ResRet.i32 %137, 0
  %139 = add i32 %80, %51
  %140 = add i32 %83, %53
  %141 = add i32 %138, %25
  call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %2, i32 %141, i32 0, float %104, float %105, float %106, float %107, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 16, i32 %131, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 20, i32 %129, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 24, i32 %47, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 28, i32 %139, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 32, i32 %140, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 36, i32 %49, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 40, i32 %90, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 44, i32 %132, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %2, i32 %141, i32 48, i32 %136, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %142

; <label>:142                                     ; preds = %94, %92
  %143 = add i32 %88, %12
  %144 = shl i32 %143, 2
  %145 = add i32 %57, %49
  %146 = add i32 %145, %144
  %147 = add i32 %47, 1
  %148 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 %147, i1 true)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %149 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %148, i32 %146, i32 undef)  ; BufferLoad(srv,index,wot)
  %150 = extractvalue %dx.types.ResRet.i32 %149, 0
  %151 = and i32 %150, 1023
  %152 = lshr i32 %150, 10
  %153 = and i32 %152, 1023
  %154 = lshr i32 %150, 20
  %155 = and i32 %154, 1023
  %156 = mul i32 %22, 9
  %157 = add i32 %156, 7
  %158 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %5, i32 0, i32 %157, i32 undef, i32 undef, i32 3)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %159 = shl i32 %25, 9
  %160 = add i32 %151, %159
  %161 = add i32 %153, %159
  %162 = add i32 %155, %159
  %163 = add i32 %156, 9
  %164 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %5, i32 %163, i32 undef)  ; BufferLoad(srv,index,wot)
  %165 = extractvalue %dx.types.ResRet.i32 %164, 0
  %166 = add i32 %165, %158
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %166, i32 undef, i32 %160, i32 %160, i32 %160, i32 %160, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %167 = add i32 %166, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %167, i32 undef, i32 %161, i32 %161, i32 %161, i32 %161, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %168 = add i32 %166, 2
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %168, i32 undef, i32 %162, i32 %162, i32 %162, i32 %162, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %169

; <label>:169                                     ; preds = %142, %31, %19, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind
declare i32 @dx.op.atomicBinOp.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind
declare void @dx.op.bufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!25}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !13, !23, null}
!5 = !{!6, !7, !9, !11}
!6 = !{i32 0, [16 x %struct.ByteAddressBuffer]* undef, !"", i32 0, i32 1, i32 16, i32 11, i32 0, null}
!7 = !{i32 1, %"class.StructuredBuffer<S_G1M_OFFSET>"* undef, !"", i32 0, i32 17, i32 1, i32 12, i32 0, !8}
!8 = !{i32 1, i32 48}
!9 = !{i32 2, %"class.StructuredBuffer<S_INSTANCE_OFFSET>"* undef, !"", i32 0, i32 19, i32 1, i32 12, i32 0, !10}
!10 = !{i32 1, i32 16}
!11 = !{i32 3, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 21, i32 1, i32 12, i32 0, !12}
!12 = !{i32 1, i32 4}
!13 = !{!14, !16, !18, !19, !20, !22}
!14 = !{i32 0, %"class.RWStructuredBuffer<S_PAYLOAD_TO_MSHLTDRAW>"* undef, !"", i32 0, i32 3, i32 1, i32 12, i1 false, i1 false, i1 false, !15}
!15 = !{i32 1, i32 8}
!16 = !{i32 1, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 9, i32 1, i32 10, i1 false, i1 false, i1 false, !17}
!17 = !{i32 0, i32 5}
!18 = !{i32 2, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 10, i32 1, i32 10, i1 false, i1 false, i1 false, !17}
!19 = !{i32 3, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 11, i32 1, i32 10, i1 false, i1 false, i1 false, !17}
!20 = !{i32 4, %"class.RWStructuredBuffer<S_VS_DRAW_MSHLT_INFO>"* undef, !"", i32 0, i32 12, i32 1, i32 12, i1 false, i1 false, i1 false, !21}
!21 = !{i32 1, i32 52}
!22 = !{i32 5, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 13, i32 1, i32 10, i1 false, i1 false, i1 false, !17}
!23 = !{!24}
!24 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 16, null}
!25 = !{void ()* @main, !"main", null, !4, !26}
!26 = !{i32 0, i64 16, i32 4, !27}
!27 = !{i32 1, i32 128, i32 8}

