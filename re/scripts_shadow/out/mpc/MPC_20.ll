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
; shader debug name: ddbf1da434d08334e2c6b5d16716500e.pdb
; shader hash: ddbf1da434d08334e2c6b5d16716500e
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
;   struct dx.alignment.legacy.$Globals
;   {
;
;       float4 vEye;                                  ; Offset:    0
;       row_major float4x4 mW2P;                      ; Offset:   16
;       float4 vFrustumPlanes[6];                     ; Offset:   80
;       float4 vViewport;                             ; Offset:  176
;       float4 vScrInfo;                              ; Offset:  192
;       float4 vHiZViewport;                          ; Offset:  208
;       int nCullingFlag;                             ; Offset:  224
;       int nEnableMergeClip;                         ; Offset:  228
;       row_major float4x4 mW2PPrev;                  ; Offset:  240
;       int nEnableOcclusionCulling;                  ; Offset:  304
;       int nEnableExcludeOccCullBasedOnScreen;       ; Offset:  308
;       float fExcludeOccCullScreenRatio;             ; Offset:  312
;       int4 nMshltPrm0;                              ; Offset:  320
;       int4 nMshltPrm1;                              ; Offset:  336
;       int4 nMshltPrm2;                              ; Offset:  352
;       uint4 nVSDrawPrm;                             ; Offset:  368
;   
;   } $Globals;                                       ; Offset:    0 Size:   384
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
; Resource bind info for tMeshletMatrixPalette
; {
;
;   float4 $Element;                                  ; Offset:    0 Size:    16
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
; Resource bind info for tPrimIdToMtrIdConvertTable
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for tToMshltCullPayloads
; {
;
;   struct struct.S_PAYLOAD_TO_MSHLTCULL
;   {
;
;       uint nData[2];                                ; Offset:    0
;   
;   } $Element;                                       ; Offset:    0 Size:     8
;
; }
;
; Resource bind info for uCullCodes
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
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; $Globals                          cbuffer      NA          NA     CB0            cb0     1
; __smptHiZ                         sampler      NA          NA      S0             s0     1
; tHiZ                              texture     f32          2d      T0             t0     1
; tMeshletG1mBuffer                 texture    byte         r/o      T1             t1    16
; tG1mOffsets                       texture  struct         r/o      T2            t17     1
; tMeshletMatrixPalette             texture  struct         r/o      T3            t18     1
; tMeshletInstanceOffsets           texture  struct         r/o      T4            t19     1
; tMaterialIndexPayloadOffsets      texture  struct         r/o      T5            t21     1
; tPrimIdToMtrIdConvertTable        texture  struct         r/o      T6            t22     1
; tToMshltCullPayloads              texture  struct         r/o      T7            t23     1
; tValidRectCountCumulativeMap      texture     u32         buf      T8            t24     1
; uCullCodes                            UAV  struct         r/w      U0             u0     1
; uArguments                            UAV     u32         buf      U1             u2     1
; uToMshltDrawPayloads                  UAV  struct         r/w      U2             u3     1
; uVSDrawArguments                      UAV     u32         buf      U3             u9     1
; uVSDrawMshltMtrOffsets                UAV     u32         buf      U4            u11     1
; uVSDrawSubPayload                     UAV     u32         buf      U5            u13     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.Dimensions = type { i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.ByteAddressBuffer = type { i32 }
%"class.StructuredBuffer<S_G1M_OFFSET>" = type { %struct.S_G1M_OFFSET }
%struct.S_G1M_OFFSET = type { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%"class.StructuredBuffer<vector<float, 4> >" = type { <4 x float> }
%"class.StructuredBuffer<S_INSTANCE_OFFSET>" = type { %struct.S_INSTANCE_OFFSET }
%struct.S_INSTANCE_OFFSET = type { i32, i32, i32, i32 }
%"class.StructuredBuffer<unsigned int>" = type { i32 }
%"class.StructuredBuffer<S_PAYLOAD_TO_MSHLTCULL>" = type { %struct.S_PAYLOAD_TO_MSHLTCULL }
%struct.S_PAYLOAD_TO_MSHLTCULL = type { [2 x i32] }
%"class.Buffer<unsigned int>" = type { i32 }
%"class.RWStructuredBuffer<unsigned int>" = type { i32 }
%"class.RWBuffer<unsigned int>" = type { i32 }
%"class.RWStructuredBuffer<S_PAYLOAD_TO_MSHLTDRAW>" = type { %struct.S_PAYLOAD_TO_MSHLTDRAW }
%struct.S_PAYLOAD_TO_MSHLTDRAW = type { [2 x i32] }
%"dx.alignment.legacy.$Globals" = type { <4 x float>, [4 x <4 x float>], [6 x <4 x float>], <4 x float>, <4 x float>, <4 x float>, i32, i32, [4 x <4 x float>], i32, i32, float, <4 x i32>, <4 x i32>, <4 x i32>, <4 x i32> }
%struct.SamplerState = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 5, i32 13, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 4, i32 11, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 3, i32 9, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 2, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 2, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 8, i32 24, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 7, i32 23, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 6, i32 22, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 5, i32 21, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 4, i32 19, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 3, i32 18, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 2, i32 17, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %17 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %18 = alloca [8 x float], align 4
  %19 = alloca [8 x float], align 4
  %20 = alloca [8 x float], align 4
  %21 = alloca [8 x float], align 4
  %22 = alloca [8 x float], align 4
  %23 = alloca [8 x float], align 4
  %24 = alloca [8 x float], align 4
  %25 = alloca [8 x float], align 4
  %26 = alloca [8 x float], align 4
  %27 = alloca [8 x float], align 4
  %28 = alloca [8 x float], align 4
  %29 = alloca [8 x float], align 4
  %30 = alloca [8 x float], align 4
  %31 = alloca [8 x float], align 4
  %32 = alloca [8 x float], align 4
  %33 = alloca [8 x float], align 4
  %34 = alloca [8 x float], align 4
  %35 = alloca [8 x float], align 4
  %36 = alloca [8 x float], align 4
  %37 = alloca [8 x float], align 4
  %38 = alloca [8 x float], align 4
  %39 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %8, i32 %17, i32 0)  ; BufferLoad(srv,index,wot)
  %40 = extractvalue %dx.types.ResRet.i32 %39, 0
  %41 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %8, i32 %17, i32 4)  ; BufferLoad(srv,index,wot)
  %42 = extractvalue %dx.types.ResRet.i32 %41, 0
  %43 = icmp eq i32 %42, -1
  br i1 %43, label %3005, label %44

; <label>:44                                      ; preds = %0
  %45 = lshr i32 %40, 6
  %46 = and i32 %45, 262143
  %47 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 0)  ; BufferLoad(srv,index,wot)
  %48 = extractvalue %dx.types.ResRet.i32 %47, 0
  %49 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 4)  ; BufferLoad(srv,index,wot)
  %50 = extractvalue %dx.types.ResRet.i32 %49, 0
  %51 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 24)  ; BufferLoad(srv,index,wot)
  %52 = extractvalue %dx.types.ResRet.i32 %51, 0
  %53 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 32)  ; BufferLoad(srv,index,wot)
  %54 = extractvalue %dx.types.ResRet.i32 %53, 0
  %55 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 36)  ; BufferLoad(srv,index,wot)
  %56 = extractvalue %dx.types.ResRet.i32 %55, 0
  %57 = shl i32 %40, 14
  %58 = and i32 %57, 1032192
  %59 = lshr i32 %42, 18
  %60 = or i32 %58, %59
  %61 = and i32 %42, 262143
  %62 = mul nuw nsw i32 %61, 12
  %63 = add i32 %50, %62
  %64 = add i32 %63, %56
  %65 = add i32 %48, 1
  %66 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 1, i32 %65, i1 true)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %67 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %64, i32 undef)  ; BufferLoad(srv,index,wot)
  %68 = extractvalue %dx.types.ResRet.i32 %67, 0
  %69 = extractvalue %dx.types.ResRet.i32 %67, 2
  %70 = lshr i32 %69, 16
  %71 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %11, i32 %60, i32 12)  ; BufferLoad(srv,index,wot)
  %72 = extractvalue %dx.types.ResRet.i32 %71, 0
  %73 = and i32 %72, 4194303
  %74 = add nuw nsw i32 %73, %70
  %75 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %9, i32 %74, i32 0)  ; BufferLoad(srv,index,wot)
  %76 = extractvalue %dx.types.ResRet.i32 %75, 0
  %77 = and i32 %76, 1073741823
  %78 = lshr i32 %76, 30
  %79 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %80 = extractvalue %dx.types.CBufRet.i32 %79, 1
  %81 = and i32 %80, 128
  %82 = icmp ne i32 %81, 0
  %83 = lshr i32 %80, 4
  %84 = and i32 %83, 7
  %85 = select i1 %82, i32 %84, i32 %78
  %86 = icmp eq i32 %77, 1073741823
  br i1 %86, label %3005, label %87

; <label>:87                                      ; preds = %44
  %88 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 44)  ; BufferLoad(srv,index,wot)
  %89 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %13, i32 %46, i32 16)  ; BufferLoad(srv,index,wot)
  %90 = extractvalue %dx.types.ResRet.i32 %88, 0
  %91 = extractvalue %dx.types.ResRet.i32 %89, 0
  %92 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %93 = extractvalue %dx.types.CBufRet.i32 %92, 0
  %94 = add nsw i32 %93, %60
  %95 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %11, i32 %94, i32 4)  ; BufferLoad(srv,index,wot)
  %96 = extractvalue %dx.types.ResRet.i32 %95, 0
  %97 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %11, i32 %94, i32 8)  ; BufferLoad(srv,index,wot)
  %98 = extractvalue %dx.types.ResRet.i32 %97, 0
  %99 = and i32 %69, 65535
  %100 = mul nuw nsw i32 %99, 44
  %101 = add i32 %50, 40
  %102 = add i32 %101, %100
  %103 = add i32 %102, %91
  %104 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %103, i32 undef)  ; BufferLoad(srv,index,wot)
  %105 = extractvalue %dx.types.ResRet.i32 %104, 0
  %106 = lshr i32 %105, 17
  %107 = add i32 %90, %50
  %108 = shl nuw nsw i32 %106, 2
  %109 = add i32 %107, %108
  %110 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %109, i32 undef)  ; BufferLoad(srv,index,wot)
  %111 = extractvalue %dx.types.ResRet.i32 %110, 0
  %112 = extractvalue %dx.types.CBufRet.i32 %79, 2
  %113 = add i32 %96, %99
  %114 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %6, i32 %113, i32 0)  ; BufferLoad(srv,index,wot)
  %115 = extractvalue %dx.types.ResRet.i32 %114, 0
  %116 = and i32 %112, 31
  %117 = lshr i32 %115, %116
  %118 = and i32 %117, 15
  %119 = icmp eq i32 %118, 5
  %120 = select i1 %119, i32 3, i32 15
  %121 = add i32 %98, %61
  %122 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %6, i32 %121, i32 0)  ; BufferLoad(srv,index,wot)
  %123 = extractvalue %dx.types.ResRet.i32 %122, 0
  %124 = icmp eq i32 %118, 0
  br i1 %124, label %125, label %2935

; <label>:125                                     ; preds = %87
  %126 = and i32 %105, 65536
  %127 = icmp ne i32 %126, 0
  %128 = lshr i32 %111, 16
  %129 = and i32 %111, 65535
  %130 = select i1 %127, i32 %128, i32 %129
  %131 = and i32 %105, 65535
  %132 = mul i32 %68, 60
  %133 = add i32 %54, %50
  %134 = add i32 %133, %132
  %135 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %134, i32 undef)  ; BufferLoad(srv,index,wot)
  %136 = extractvalue %dx.types.ResRet.i32 %135, 0
  %137 = extractvalue %dx.types.ResRet.i32 %135, 1
  %138 = extractvalue %dx.types.ResRet.i32 %135, 2
  %139 = extractvalue %dx.types.ResRet.i32 %135, 3
  %140 = bitcast i32 %136 to float
  %141 = bitcast i32 %137 to float
  %142 = bitcast i32 %138 to float
  %143 = bitcast i32 %139 to float
  %144 = add i32 %50, 16
  %145 = add i32 %144, %54
  %146 = add i32 %145, %132
  %147 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %146, i32 undef)  ; BufferLoad(srv,index,wot)
  %148 = extractvalue %dx.types.ResRet.i32 %147, 0
  %149 = extractvalue %dx.types.ResRet.i32 %147, 1
  %150 = extractvalue %dx.types.ResRet.i32 %147, 2
  %151 = extractvalue %dx.types.ResRet.i32 %147, 3
  %152 = bitcast i32 %148 to float
  %153 = bitcast i32 %149 to float
  %154 = bitcast i32 %150 to float
  %155 = bitcast i32 %151 to float
  %156 = add i32 %50, 32
  %157 = add i32 %156, %54
  %158 = add i32 %157, %132
  %159 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %158, i32 undef)  ; BufferLoad(srv,index,wot)
  %160 = extractvalue %dx.types.ResRet.i32 %159, 0
  %161 = extractvalue %dx.types.ResRet.i32 %159, 1
  %162 = extractvalue %dx.types.ResRet.i32 %159, 2
  %163 = extractvalue %dx.types.ResRet.i32 %159, 3
  %164 = bitcast i32 %160 to float
  %165 = bitcast i32 %161 to float
  %166 = bitcast i32 %162 to float
  %167 = bitcast i32 %163 to float
  %168 = add i32 %50, 48
  %169 = add i32 %168, %54
  %170 = add i32 %169, %132
  %171 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %170, i32 undef)  ; BufferLoad(srv,index,wot)
  %172 = extractvalue %dx.types.ResRet.i32 %171, 0
  %173 = extractvalue %dx.types.ResRet.i32 %171, 1
  %174 = extractvalue %dx.types.ResRet.i32 %171, 2
  %175 = bitcast i32 %172 to float
  %176 = bitcast i32 %173 to float
  %177 = bitcast i32 %174 to float
  %178 = add nuw nsw i32 %130, %131
  %179 = mul nuw nsw i32 %178, 3
  %180 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %11, i32 %94, i32 0)  ; BufferLoad(srv,index,wot)
  %181 = extractvalue %dx.types.ResRet.i32 %180, 0
  %182 = and i32 %80, 256
  %183 = icmp ne i32 %182, 0
  %184 = and i32 %80, 15
  %185 = icmp eq i32 %184, 0
  br i1 %185, label %186, label %658

; <label>:186                                     ; preds = %125
  %187 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %188 = extractvalue %dx.types.CBufRet.f32 %187, 0
  %189 = extractvalue %dx.types.CBufRet.f32 %187, 1
  %190 = extractvalue %dx.types.CBufRet.f32 %187, 3
  %191 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %192 = extractvalue %dx.types.CBufRet.f32 %191, 0
  %193 = extractvalue %dx.types.CBufRet.f32 %191, 1
  %194 = extractvalue %dx.types.CBufRet.f32 %191, 3
  %195 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %196 = extractvalue %dx.types.CBufRet.f32 %195, 0
  %197 = extractvalue %dx.types.CBufRet.f32 %195, 1
  %198 = extractvalue %dx.types.CBufRet.f32 %195, 3
  %199 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %200 = extractvalue %dx.types.CBufRet.f32 %199, 0
  %201 = extractvalue %dx.types.CBufRet.f32 %199, 1
  %202 = extractvalue %dx.types.CBufRet.f32 %199, 3
  %203 = add i32 %181, %179
  %204 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %203, i32 0)  ; BufferLoad(srv,index,wot)
  %205 = extractvalue %dx.types.ResRet.f32 %204, 0
  %206 = extractvalue %dx.types.ResRet.f32 %204, 1
  %207 = extractvalue %dx.types.ResRet.f32 %204, 2
  %208 = extractvalue %dx.types.ResRet.f32 %204, 3
  %209 = add nuw nsw i32 %179, 1
  %210 = add i32 %209, %181
  %211 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %210, i32 0)  ; BufferLoad(srv,index,wot)
  %212 = extractvalue %dx.types.ResRet.f32 %211, 0
  %213 = extractvalue %dx.types.ResRet.f32 %211, 1
  %214 = extractvalue %dx.types.ResRet.f32 %211, 2
  %215 = extractvalue %dx.types.ResRet.f32 %211, 3
  %216 = add nuw nsw i32 %179, 2
  %217 = add i32 %216, %181
  %218 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %217, i32 0)  ; BufferLoad(srv,index,wot)
  %219 = extractvalue %dx.types.ResRet.f32 %218, 0
  %220 = extractvalue %dx.types.ResRet.f32 %218, 1
  %221 = extractvalue %dx.types.ResRet.f32 %218, 2
  %222 = extractvalue %dx.types.ResRet.f32 %218, 3
  %223 = call float @dx.op.dot3.f32(i32 55, float %219, float %220, float %221, float %219, float %220, float %221)  ; Dot3(ax,ay,az,bx,by,bz)
  %224 = call float @dx.op.dot3.f32(i32 55, float %212, float %213, float %214, float %212, float %213, float %214)  ; Dot3(ax,ay,az,bx,by,bz)
  %225 = call float @dx.op.binary.f32(i32 35, float %224, float %223)  ; FMax(a,b)
  %226 = call float @dx.op.dot3.f32(i32 55, float %205, float %206, float %207, float %205, float %206, float %207)  ; Dot3(ax,ay,az,bx,by,bz)
  %227 = call float @dx.op.binary.f32(i32 35, float %226, float %225)  ; FMax(a,b)
  %228 = call float @dx.op.unary.f32(i32 24, float %227)  ; Sqrt(value)
  %229 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %205, float %206, float %207, float %208)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %230 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %212, float %213, float %214, float %215)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %231 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %219, float %220, float %221, float %222)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %232 = fmul fast float %228, %143
  %233 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %234 = extractvalue %dx.types.CBufRet.f32 %233, 0
  %235 = extractvalue %dx.types.CBufRet.f32 %233, 1
  %236 = extractvalue %dx.types.CBufRet.f32 %233, 2
  %237 = extractvalue %dx.types.CBufRet.f32 %233, 3
  %238 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %234, float %235, float %236, float %237)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %239 = fsub fast float -0.000000e+00, %232
  %240 = fcmp fast olt float %238, %239
  br i1 %240, label %2932, label %241

; <label>:241                                     ; preds = %186
  %242 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %243 = extractvalue %dx.types.CBufRet.f32 %242, 0
  %244 = extractvalue %dx.types.CBufRet.f32 %242, 1
  %245 = extractvalue %dx.types.CBufRet.f32 %242, 2
  %246 = extractvalue %dx.types.CBufRet.f32 %242, 3
  %247 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %243, float %244, float %245, float %246)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %248 = fcmp fast olt float %247, %239
  br i1 %248, label %2932, label %3006

; <label>:249                                     ; preds = %3038
  %250 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %205, float %206, float %207)  ; Dot3(ax,ay,az,bx,by,bz)
  %251 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %212, float %213, float %214)  ; Dot3(ax,ay,az,bx,by,bz)
  %252 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %219, float %220, float %221)  ; Dot3(ax,ay,az,bx,by,bz)
  %253 = call float @dx.op.dot3.f32(i32 55, float %250, float %251, float %252, float %250, float %251, float %252)  ; Dot3(ax,ay,az,bx,by,bz)
  %254 = call float @dx.op.unary.f32(i32 25, float %253)  ; Rsqrt(value)
  %255 = fmul fast float %254, %250
  %256 = fmul fast float %254, %251
  %257 = fmul fast float %254, %252
  %258 = icmp eq i32 %85, 2
  %259 = fsub fast float -0.000000e+00, %255
  %260 = fsub fast float -0.000000e+00, %256
  %261 = fsub fast float -0.000000e+00, %257
  %262 = select i1 %258, float %259, float %255
  %263 = select i1 %258, float %260, float %256
  %264 = select i1 %258, float %261, float %257
  %265 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %266 = extractvalue %dx.types.CBufRet.f32 %265, 0
  %267 = extractvalue %dx.types.CBufRet.f32 %265, 1
  %268 = extractvalue %dx.types.CBufRet.f32 %265, 2
  %269 = fmul fast float %228, %164
  %270 = fmul fast float %269, %262
  %271 = fmul fast float %269, %263
  %272 = fmul fast float %269, %264
  %273 = fsub fast float %229, %270
  %274 = fsub fast float %230, %271
  %275 = fsub fast float %231, %272
  %276 = fsub fast float %266, %273
  %277 = fsub fast float %267, %274
  %278 = fsub fast float %268, %275
  %279 = call float @dx.op.dot3.f32(i32 55, float %276, float %277, float %278, float %276, float %277, float %278)  ; Dot3(ax,ay,az,bx,by,bz)
  %280 = call float @dx.op.unary.f32(i32 25, float %279)  ; Rsqrt(value)
  %281 = fmul fast float %276, %280
  %282 = fmul fast float %277, %280
  %283 = fmul fast float %278, %280
  %284 = fsub fast float -0.000000e+00, %262
  %285 = fsub fast float -0.000000e+00, %263
  %286 = fsub fast float -0.000000e+00, %264
  %287 = call float @dx.op.dot3.f32(i32 55, float %281, float %282, float %283, float %284, float %285, float %286)  ; Dot3(ax,ay,az,bx,by,bz)
  %288 = fcmp fast ogt float %287, %155
  br i1 %288, label %2932, label %289

; <label>:289                                     ; preds = %3038, %249
  %290 = and i32 %80, 512
  %291 = icmp eq i32 %290, 0
  br i1 %291, label %501, label %292

; <label>:292                                     ; preds = %289
  br label %293

; <label>:293                                     ; preds = %293, %292
  %294 = phi i32 [ %338, %293 ], [ 0, %292 ]
  %295 = and i32 %294, 1
  %296 = icmp ne i32 %295, 0
  %297 = select i1 %296, float %175, float %165
  %298 = and i32 %294, 2
  %299 = icmp ne i32 %298, 0
  %300 = select i1 %299, float %176, float %166
  %301 = and i32 %294, 4
  %302 = icmp ne i32 %301, 0
  %303 = select i1 %302, float %177, float %167
  %304 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %203, i32 0)  ; BufferLoad(srv,index,wot)
  %305 = extractvalue %dx.types.ResRet.f32 %304, 0
  %306 = extractvalue %dx.types.ResRet.f32 %304, 1
  %307 = extractvalue %dx.types.ResRet.f32 %304, 2
  %308 = extractvalue %dx.types.ResRet.f32 %304, 3
  %309 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %210, i32 0)  ; BufferLoad(srv,index,wot)
  %310 = extractvalue %dx.types.ResRet.f32 %309, 0
  %311 = extractvalue %dx.types.ResRet.f32 %309, 1
  %312 = extractvalue %dx.types.ResRet.f32 %309, 2
  %313 = extractvalue %dx.types.ResRet.f32 %309, 3
  %314 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %217, i32 0)  ; BufferLoad(srv,index,wot)
  %315 = extractvalue %dx.types.ResRet.f32 %314, 0
  %316 = extractvalue %dx.types.ResRet.f32 %314, 1
  %317 = extractvalue %dx.types.ResRet.f32 %314, 2
  %318 = extractvalue %dx.types.ResRet.f32 %314, 3
  %319 = call float @dx.op.dot4.f32(i32 56, float %297, float %300, float %303, float 1.000000e+00, float %305, float %306, float %307, float %308)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %320 = call float @dx.op.dot4.f32(i32 56, float %297, float %300, float %303, float 1.000000e+00, float %310, float %311, float %312, float %313)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %321 = call float @dx.op.dot4.f32(i32 56, float %297, float %300, float %303, float 1.000000e+00, float %315, float %316, float %317, float %318)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %322 = fmul fast float %319, %188
  %323 = call float @dx.op.tertiary.f32(i32 46, float %320, float %192, float %322)  ; FMad(a,b,c)
  %324 = call float @dx.op.tertiary.f32(i32 46, float %321, float %196, float %323)  ; FMad(a,b,c)
  %325 = fadd fast float %324, %200
  %326 = fmul fast float %319, %189
  %327 = call float @dx.op.tertiary.f32(i32 46, float %320, float %193, float %326)  ; FMad(a,b,c)
  %328 = call float @dx.op.tertiary.f32(i32 46, float %321, float %197, float %327)  ; FMad(a,b,c)
  %329 = fadd fast float %328, %201
  %330 = fmul fast float %319, %190
  %331 = call float @dx.op.tertiary.f32(i32 46, float %320, float %194, float %330)  ; FMad(a,b,c)
  %332 = call float @dx.op.tertiary.f32(i32 46, float %321, float %198, float %331)  ; FMad(a,b,c)
  %333 = fadd fast float %332, %202
  %334 = fdiv fast float %325, %333
  %335 = fdiv fast float %329, %333
  %336 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 %294
  %337 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 %294
  store float %334, float* %336, align 4
  store float %335, float* %337, align 4
  %338 = add nuw nsw i32 %294, 1
  %339 = icmp eq i32 %338, 8
  br i1 %339, label %340, label %293

; <label>:340                                     ; preds = %293
  %341 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 3
  %342 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 3
  %343 = load float, float* %341, align 4
  %344 = load float, float* %342, align 4
  %345 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 2
  %346 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 2
  %347 = load float, float* %345, align 4
  %348 = load float, float* %346, align 4
  %349 = call float @dx.op.binary.f32(i32 36, float %347, float %343)  ; FMin(a,b)
  %350 = call float @dx.op.binary.f32(i32 36, float %348, float %344)  ; FMin(a,b)
  %351 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 1
  %352 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 1
  %353 = load float, float* %351, align 4
  %354 = load float, float* %352, align 4
  %355 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 0
  %356 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 0
  %357 = load float, float* %355, align 4
  %358 = load float, float* %356, align 4
  %359 = call float @dx.op.binary.f32(i32 36, float %357, float %353)  ; FMin(a,b)
  %360 = call float @dx.op.binary.f32(i32 36, float %358, float %354)  ; FMin(a,b)
  %361 = call float @dx.op.binary.f32(i32 36, float %359, float %349)  ; FMin(a,b)
  %362 = call float @dx.op.binary.f32(i32 36, float %360, float %350)  ; FMin(a,b)
  %363 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 7
  %364 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 7
  %365 = load float, float* %363, align 4
  %366 = load float, float* %364, align 4
  %367 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 6
  %368 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 6
  %369 = load float, float* %367, align 4
  %370 = load float, float* %368, align 4
  %371 = call float @dx.op.binary.f32(i32 36, float %369, float %365)  ; FMin(a,b)
  %372 = call float @dx.op.binary.f32(i32 36, float %370, float %366)  ; FMin(a,b)
  %373 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 5
  %374 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 5
  %375 = load float, float* %373, align 4
  %376 = load float, float* %374, align 4
  %377 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 4
  %378 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 4
  %379 = load float, float* %377, align 4
  %380 = load float, float* %378, align 4
  %381 = call float @dx.op.binary.f32(i32 36, float %379, float %375)  ; FMin(a,b)
  %382 = call float @dx.op.binary.f32(i32 36, float %380, float %376)  ; FMin(a,b)
  %383 = call float @dx.op.binary.f32(i32 36, float %381, float %371)  ; FMin(a,b)
  %384 = call float @dx.op.binary.f32(i32 36, float %382, float %372)  ; FMin(a,b)
  %385 = call float @dx.op.binary.f32(i32 36, float %361, float %383)  ; FMin(a,b)
  %386 = call float @dx.op.binary.f32(i32 36, float %362, float %384)  ; FMin(a,b)
  %387 = call float @dx.op.binary.f32(i32 35, float %347, float %343)  ; FMax(a,b)
  %388 = call float @dx.op.binary.f32(i32 35, float %348, float %344)  ; FMax(a,b)
  %389 = call float @dx.op.binary.f32(i32 35, float %357, float %353)  ; FMax(a,b)
  %390 = call float @dx.op.binary.f32(i32 35, float %358, float %354)  ; FMax(a,b)
  %391 = call float @dx.op.binary.f32(i32 35, float %389, float %387)  ; FMax(a,b)
  %392 = call float @dx.op.binary.f32(i32 35, float %390, float %388)  ; FMax(a,b)
  %393 = call float @dx.op.binary.f32(i32 35, float %369, float %365)  ; FMax(a,b)
  %394 = call float @dx.op.binary.f32(i32 35, float %370, float %366)  ; FMax(a,b)
  %395 = call float @dx.op.binary.f32(i32 35, float %379, float %375)  ; FMax(a,b)
  %396 = call float @dx.op.binary.f32(i32 35, float %380, float %376)  ; FMax(a,b)
  %397 = call float @dx.op.binary.f32(i32 35, float %395, float %393)  ; FMax(a,b)
  %398 = call float @dx.op.binary.f32(i32 35, float %396, float %394)  ; FMax(a,b)
  %399 = call float @dx.op.binary.f32(i32 35, float %391, float %397)  ; FMax(a,b)
  %400 = call float @dx.op.binary.f32(i32 35, float %392, float %398)  ; FMax(a,b)
  %401 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %402 = extractvalue %dx.types.CBufRet.i32 %401, 0
  %403 = and i32 %402, 15
  %404 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %405 = extractvalue %dx.types.CBufRet.f32 %404, 0
  %406 = extractvalue %dx.types.CBufRet.f32 %404, 1
  %407 = extractvalue %dx.types.CBufRet.f32 %404, 2
  %408 = extractvalue %dx.types.CBufRet.f32 %404, 3
  %409 = fmul fast float %385, 5.000000e-01
  %410 = fmul fast float %400, 5.000000e-01
  %411 = fadd fast float %409, 5.000000e-01
  %412 = fsub fast float 5.000000e-01, %410
  %413 = fmul fast float %399, 5.000000e-01
  %414 = fmul fast float %386, 5.000000e-01
  %415 = fadd fast float %413, 5.000000e-01
  %416 = fsub fast float 5.000000e-01, %414
  %417 = fmul fast float %405, %411
  %418 = fmul fast float %406, %412
  %419 = fadd fast float %417, %407
  %420 = fadd fast float %418, %408
  %421 = fmul fast float %405, %415
  %422 = fmul fast float %406, %416
  %423 = fadd fast float %421, %407
  %424 = fadd fast float %422, %408
  %425 = call float @dx.op.binary.f32(i32 35, float %419, float 0.000000e+00)  ; FMax(a,b)
  %426 = call float @dx.op.binary.f32(i32 35, float %420, float 0.000000e+00)  ; FMax(a,b)
  %427 = call float @dx.op.binary.f32(i32 36, float %425, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %428 = call float @dx.op.binary.f32(i32 36, float %426, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %429 = call float @dx.op.binary.f32(i32 35, float %423, float 0.000000e+00)  ; FMax(a,b)
  %430 = call float @dx.op.binary.f32(i32 35, float %424, float 0.000000e+00)  ; FMax(a,b)
  %431 = call float @dx.op.binary.f32(i32 36, float %429, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %432 = call float @dx.op.binary.f32(i32 36, float %430, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %433 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %434 = extractvalue %dx.types.ResRet.i32 %433, 0
  %435 = icmp slt i32 %434, 1
  br i1 %435, label %501, label %436

; <label>:436                                     ; preds = %340
  %437 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %438 = extractvalue %dx.types.ResRet.i32 %437, 0
  %439 = icmp ult i32 %403, %438
  br i1 %439, label %440, label %501

; <label>:440                                     ; preds = %436
  %441 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %442 = extractvalue %dx.types.ResRet.i32 %441, 0
  %443 = uitofp i32 %434 to float
  %444 = uitofp i32 %442 to float
  %445 = fmul fast float %443, %427
  %446 = fmul fast float %444, %428
  %447 = fptoui float %445 to i32
  %448 = fptoui float %446 to i32
  %449 = mul i32 %448, %434
  %450 = add i32 %449, %447
  %451 = fmul fast float %443, %431
  %452 = fptoui float %451 to i32
  %453 = add i32 %449, %452
  %454 = fmul fast float %444, %432
  %455 = fptoui float %454 to i32
  %456 = mul i32 %455, %434
  %457 = add i32 %456, %447
  %458 = mul i32 %434, %403
  %459 = mul i32 %458, %442
  %460 = add nsw i32 %459, 3
  %461 = icmp sge i32 %450, %434
  %462 = srem i32 %450, %434
  %463 = icmp ne i32 %462, 0
  %464 = and i1 %463, %461
  br i1 %464, label %465, label %471

; <label>:465                                     ; preds = %440
  %466 = xor i32 %434, -1
  %467 = add i32 %460, %466
  %468 = add i32 %467, %450
  %469 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %468, i32 undef)  ; BufferLoad(srv,index,wot)
  %470 = extractvalue %dx.types.ResRet.i32 %469, 0
  br label %471

; <label>:471                                     ; preds = %465, %440
  %472 = phi i32 [ %470, %465 ], [ 0, %440 ]
  %473 = icmp slt i32 %453, %434
  br i1 %473, label %479, label %474

; <label>:474                                     ; preds = %471
  %475 = sub i32 %460, %434
  %476 = add i32 %475, %453
  %477 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %476, i32 undef)  ; BufferLoad(srv,index,wot)
  %478 = extractvalue %dx.types.ResRet.i32 %477, 0
  br label %479

; <label>:479                                     ; preds = %474, %471
  %480 = phi i32 [ %478, %474 ], [ 0, %471 ]
  %481 = srem i32 %457, %434
  %482 = icmp eq i32 %481, 0
  br i1 %482, label %488, label %483

; <label>:483                                     ; preds = %479
  %484 = add i32 %459, 2
  %485 = add i32 %484, %457
  %486 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %485, i32 undef)  ; BufferLoad(srv,index,wot)
  %487 = extractvalue %dx.types.ResRet.i32 %486, 0
  br label %488

; <label>:488                                     ; preds = %483, %479
  %489 = phi i32 [ %487, %483 ], [ 0, %479 ]
  %490 = add i32 %460, %452
  %491 = add i32 %490, %456
  %492 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %491, i32 undef)  ; BufferLoad(srv,index,wot)
  %493 = extractvalue %dx.types.ResRet.i32 %492, 0
  %494 = add i32 %489, %480
  %495 = sub i32 %493, %494
  %496 = sub i32 0, %472
  %497 = icmp eq i32 %495, %496
  %498 = xor i1 %183, true
  %499 = or i1 %497, %498
  %500 = select i1 %497, i32 11, i32 0
  br i1 %499, label %2932, label %502

; <label>:501                                     ; preds = %436, %340, %289
  br i1 %183, label %502, label %2932

; <label>:502                                     ; preds = %501, %488
  %503 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %203, i32 0)  ; BufferLoad(srv,index,wot)
  %504 = extractvalue %dx.types.ResRet.f32 %503, 0
  %505 = extractvalue %dx.types.ResRet.f32 %503, 1
  %506 = extractvalue %dx.types.ResRet.f32 %503, 2
  %507 = extractvalue %dx.types.ResRet.f32 %503, 3
  %508 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %210, i32 0)  ; BufferLoad(srv,index,wot)
  %509 = extractvalue %dx.types.ResRet.f32 %508, 0
  %510 = extractvalue %dx.types.ResRet.f32 %508, 1
  %511 = extractvalue %dx.types.ResRet.f32 %508, 2
  %512 = extractvalue %dx.types.ResRet.f32 %508, 3
  %513 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %217, i32 0)  ; BufferLoad(srv,index,wot)
  %514 = extractvalue %dx.types.ResRet.f32 %513, 0
  %515 = extractvalue %dx.types.ResRet.f32 %513, 1
  %516 = extractvalue %dx.types.ResRet.f32 %513, 2
  %517 = extractvalue %dx.types.ResRet.f32 %513, 3
  %518 = call float @dx.op.dot3.f32(i32 55, float %514, float %515, float %516, float %514, float %515, float %516)  ; Dot3(ax,ay,az,bx,by,bz)
  %519 = call float @dx.op.dot3.f32(i32 55, float %509, float %510, float %511, float %509, float %510, float %511)  ; Dot3(ax,ay,az,bx,by,bz)
  %520 = call float @dx.op.binary.f32(i32 35, float %519, float %518)  ; FMax(a,b)
  %521 = call float @dx.op.dot3.f32(i32 55, float %504, float %505, float %506, float %504, float %505, float %506)  ; Dot3(ax,ay,az,bx,by,bz)
  %522 = call float @dx.op.binary.f32(i32 35, float %521, float %520)  ; FMax(a,b)
  %523 = call float @dx.op.unary.f32(i32 24, float %522)  ; Sqrt(value)
  %524 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %504, float %505, float %506, float %507)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %525 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %509, float %510, float %511, float %512)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %526 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %514, float %515, float %516, float %517)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %527 = fmul fast float %523, %143
  %528 = fmul fast float %524, %188
  %529 = call float @dx.op.tertiary.f32(i32 46, float %525, float %192, float %528)  ; FMad(a,b,c)
  %530 = call float @dx.op.tertiary.f32(i32 46, float %526, float %196, float %529)  ; FMad(a,b,c)
  %531 = fadd fast float %530, %200
  %532 = fmul fast float %524, %189
  %533 = call float @dx.op.tertiary.f32(i32 46, float %525, float %193, float %532)  ; FMad(a,b,c)
  %534 = call float @dx.op.tertiary.f32(i32 46, float %526, float %197, float %533)  ; FMad(a,b,c)
  %535 = fadd fast float %534, %201
  %536 = fmul fast float %524, %190
  %537 = call float @dx.op.tertiary.f32(i32 46, float %525, float %194, float %536)  ; FMad(a,b,c)
  %538 = call float @dx.op.tertiary.f32(i32 46, float %526, float %198, float %537)  ; FMad(a,b,c)
  %539 = fadd fast float %538, %202
  %540 = fdiv fast float %531, %539
  %541 = fdiv fast float %535, %539
  %542 = fmul fast float %540, 5.000000e-01
  %543 = fmul fast float %541, 5.000000e-01
  %544 = fadd fast float %542, 5.000000e-01
  %545 = fadd fast float %543, 5.000000e-01
  %546 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %547 = extractvalue %dx.types.CBufRet.f32 %546, 0
  %548 = extractvalue %dx.types.CBufRet.f32 %546, 1
  %549 = extractvalue %dx.types.CBufRet.f32 %546, 2
  %550 = fmul fast float %547, 5.000000e-01
  %551 = fmul fast float %548, 5.000000e-01
  %552 = fmul fast float %547, %544
  %553 = call float @dx.op.unary.f32(i32 26, float %552)  ; Round_ne(value)
  %554 = fsub fast float %553, %550
  %555 = fmul fast float %548, %545
  %556 = call float @dx.op.unary.f32(i32 26, float %555)  ; Round_ne(value)
  %557 = fsub fast float %556, %551
  %558 = fsub fast float -0.000000e+00, %549
  %559 = fadd fast float %554, -5.000000e-01
  %560 = fadd fast float %557, -5.000000e-01
  %561 = fadd fast float %557, 5.000000e-01
  %562 = fadd fast float %554, 5.000000e-01
  %563 = call float @dx.op.dot3.f32(i32 55, float %559, float %560, float %558, float %559, float %560, float %558)  ; Dot3(ax,ay,az,bx,by,bz)
  %564 = call float @dx.op.unary.f32(i32 25, float %563)  ; Rsqrt(value)
  %565 = fmul fast float %564, %559
  %566 = fmul fast float %560, %564
  %567 = fmul fast float %564, %558
  %568 = call float @dx.op.dot3.f32(i32 55, float %559, float %561, float %558, float %559, float %561, float %558)  ; Dot3(ax,ay,az,bx,by,bz)
  %569 = call float @dx.op.unary.f32(i32 25, float %568)  ; Rsqrt(value)
  %570 = fmul fast float %569, %559
  %571 = fmul fast float %569, %561
  %572 = fmul fast float %569, %558
  %573 = call float @dx.op.dot3.f32(i32 55, float %562, float %560, float %558, float %562, float %560, float %558)  ; Dot3(ax,ay,az,bx,by,bz)
  %574 = call float @dx.op.unary.f32(i32 25, float %573)  ; Rsqrt(value)
  %575 = fmul fast float %574, %562
  %576 = fmul fast float %574, %560
  %577 = fmul fast float %574, %558
  %578 = call float @dx.op.dot3.f32(i32 55, float %562, float %561, float %558, float %562, float %561, float %558)  ; Dot3(ax,ay,az,bx,by,bz)
  %579 = call float @dx.op.unary.f32(i32 25, float %578)  ; Rsqrt(value)
  %580 = fmul fast float %579, %562
  %581 = fmul fast float %579, %561
  %582 = fmul fast float %579, %558
  %583 = fmul fast float %539, %540
  %584 = fmul fast float %583, %550
  %585 = fdiv fast float %584, %549
  %586 = fmul fast float %541, %539
  %587 = fmul fast float %586, %551
  %588 = fdiv fast float %587, %549
  %589 = fsub fast float -0.000000e+00, %539
  %590 = fmul fast float %588, %567
  %591 = fmul fast float %566, %589
  %592 = fsub fast float %590, %591
  %593 = fmul fast float %565, %589
  %594 = fmul fast float %585, %567
  %595 = fsub fast float %593, %594
  %596 = fmul fast float %585, %566
  %597 = fmul fast float %588, %565
  %598 = fsub fast float %596, %597
  %599 = fmul fast float %592, %592
  %600 = fmul fast float %595, %595
  %601 = fadd fast float %599, %600
  %602 = fmul fast float %598, %598
  %603 = fadd fast float %601, %602
  %604 = call float @dx.op.unary.f32(i32 24, float %603)  ; Sqrt(value)
  %605 = fcmp fast olt float %604, %527
  br i1 %605, label %2932, label %606

; <label>:606                                     ; preds = %502
  %607 = fmul fast float %588, %572
  %608 = fmul fast float %571, %589
  %609 = fsub fast float %607, %608
  %610 = fmul fast float %570, %589
  %611 = fmul fast float %585, %572
  %612 = fsub fast float %610, %611
  %613 = fmul fast float %585, %571
  %614 = fmul fast float %588, %570
  %615 = fsub fast float %613, %614
  %616 = fmul fast float %609, %609
  %617 = fmul fast float %612, %612
  %618 = fadd fast float %616, %617
  %619 = fmul fast float %615, %615
  %620 = fadd fast float %618, %619
  %621 = call float @dx.op.unary.f32(i32 24, float %620)  ; Sqrt(value)
  %622 = fcmp fast olt float %621, %527
  br i1 %622, label %2932, label %623

; <label>:623                                     ; preds = %606
  %624 = fmul fast float %588, %577
  %625 = fmul fast float %576, %589
  %626 = fsub fast float %624, %625
  %627 = fmul fast float %575, %589
  %628 = fmul fast float %585, %577
  %629 = fsub fast float %627, %628
  %630 = fmul fast float %585, %576
  %631 = fmul fast float %588, %575
  %632 = fsub fast float %630, %631
  %633 = fmul fast float %626, %626
  %634 = fmul fast float %629, %629
  %635 = fadd fast float %633, %634
  %636 = fmul fast float %632, %632
  %637 = fadd fast float %635, %636
  %638 = call float @dx.op.unary.f32(i32 24, float %637)  ; Sqrt(value)
  %639 = fcmp fast olt float %638, %527
  br i1 %639, label %2932, label %640

; <label>:640                                     ; preds = %623
  %641 = fmul fast float %588, %582
  %642 = fmul fast float %581, %589
  %643 = fsub fast float %641, %642
  %644 = fmul fast float %580, %589
  %645 = fmul fast float %585, %582
  %646 = fsub fast float %644, %645
  %647 = fmul fast float %585, %581
  %648 = fmul fast float %588, %580
  %649 = fsub fast float %647, %648
  %650 = fmul fast float %643, %643
  %651 = fmul fast float %646, %646
  %652 = fadd fast float %650, %651
  %653 = fmul fast float %649, %649
  %654 = fadd fast float %652, %653
  %655 = call float @dx.op.unary.f32(i32 24, float %654)  ; Sqrt(value)
  %656 = fcmp fast olt float %655, %527
  br i1 %656, label %2932, label %657

; <label>:657                                     ; preds = %640
  br label %2932

; <label>:658                                     ; preds = %125
  %659 = icmp eq i32 %184, 1
  br i1 %659, label %660, label %1646

; <label>:660                                     ; preds = %658
  %661 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %662 = extractvalue %dx.types.CBufRet.f32 %661, 0
  %663 = extractvalue %dx.types.CBufRet.f32 %661, 1
  %664 = extractvalue %dx.types.CBufRet.f32 %661, 2
  %665 = extractvalue %dx.types.CBufRet.f32 %661, 3
  %666 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %667 = extractvalue %dx.types.CBufRet.f32 %666, 0
  %668 = extractvalue %dx.types.CBufRet.f32 %666, 1
  %669 = extractvalue %dx.types.CBufRet.f32 %666, 2
  %670 = extractvalue %dx.types.CBufRet.f32 %666, 3
  %671 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %672 = extractvalue %dx.types.CBufRet.f32 %671, 0
  %673 = extractvalue %dx.types.CBufRet.f32 %671, 1
  %674 = extractvalue %dx.types.CBufRet.f32 %671, 2
  %675 = extractvalue %dx.types.CBufRet.f32 %671, 3
  %676 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %677 = extractvalue %dx.types.CBufRet.f32 %676, 0
  %678 = extractvalue %dx.types.CBufRet.f32 %676, 1
  %679 = extractvalue %dx.types.CBufRet.f32 %676, 2
  %680 = extractvalue %dx.types.CBufRet.f32 %676, 3
  %681 = extractvalue %dx.types.CBufRet.i32 %79, 0
  %682 = and i32 %681, 1
  %683 = add i32 %181, %179
  %684 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %683, i32 0)  ; BufferLoad(srv,index,wot)
  %685 = extractvalue %dx.types.ResRet.f32 %684, 0
  %686 = extractvalue %dx.types.ResRet.f32 %684, 1
  %687 = extractvalue %dx.types.ResRet.f32 %684, 2
  %688 = extractvalue %dx.types.ResRet.f32 %684, 3
  %689 = add nuw nsw i32 %179, 1
  %690 = add i32 %689, %181
  %691 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %690, i32 0)  ; BufferLoad(srv,index,wot)
  %692 = extractvalue %dx.types.ResRet.f32 %691, 0
  %693 = extractvalue %dx.types.ResRet.f32 %691, 1
  %694 = extractvalue %dx.types.ResRet.f32 %691, 2
  %695 = extractvalue %dx.types.ResRet.f32 %691, 3
  %696 = add nuw nsw i32 %179, 2
  %697 = add i32 %696, %181
  %698 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %697, i32 0)  ; BufferLoad(srv,index,wot)
  %699 = extractvalue %dx.types.ResRet.f32 %698, 0
  %700 = extractvalue %dx.types.ResRet.f32 %698, 1
  %701 = extractvalue %dx.types.ResRet.f32 %698, 2
  %702 = extractvalue %dx.types.ResRet.f32 %698, 3
  %703 = call float @dx.op.dot3.f32(i32 55, float %699, float %700, float %701, float %699, float %700, float %701)  ; Dot3(ax,ay,az,bx,by,bz)
  %704 = call float @dx.op.dot3.f32(i32 55, float %692, float %693, float %694, float %692, float %693, float %694)  ; Dot3(ax,ay,az,bx,by,bz)
  %705 = call float @dx.op.binary.f32(i32 35, float %704, float %703)  ; FMax(a,b)
  %706 = call float @dx.op.dot3.f32(i32 55, float %685, float %686, float %687, float %685, float %686, float %687)  ; Dot3(ax,ay,az,bx,by,bz)
  %707 = call float @dx.op.binary.f32(i32 35, float %706, float %705)  ; FMax(a,b)
  %708 = call float @dx.op.unary.f32(i32 24, float %707)  ; Sqrt(value)
  %709 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %685, float %686, float %687, float %688)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %710 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %692, float %693, float %694, float %695)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %711 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %699, float %700, float %701, float %702)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %712 = fmul fast float %708, %143
  %713 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %714 = extractvalue %dx.types.CBufRet.f32 %713, 0
  %715 = extractvalue %dx.types.CBufRet.f32 %713, 1
  %716 = extractvalue %dx.types.CBufRet.f32 %713, 2
  %717 = extractvalue %dx.types.CBufRet.f32 %713, 3
  %718 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %714, float %715, float %716, float %717)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %719 = fsub fast float -0.000000e+00, %712
  %720 = fcmp fast olt float %718, %719
  br i1 %720, label %2932, label %721

; <label>:721                                     ; preds = %660
  %722 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %723 = extractvalue %dx.types.CBufRet.f32 %722, 0
  %724 = extractvalue %dx.types.CBufRet.f32 %722, 1
  %725 = extractvalue %dx.types.CBufRet.f32 %722, 2
  %726 = extractvalue %dx.types.CBufRet.f32 %722, 3
  %727 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %723, float %724, float %725, float %726)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %728 = fcmp fast olt float %727, %719
  br i1 %728, label %2932, label %3040

; <label>:729                                     ; preds = %3072
  %730 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %685, float %686, float %687)  ; Dot3(ax,ay,az,bx,by,bz)
  %731 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %692, float %693, float %694)  ; Dot3(ax,ay,az,bx,by,bz)
  %732 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %699, float %700, float %701)  ; Dot3(ax,ay,az,bx,by,bz)
  %733 = call float @dx.op.dot3.f32(i32 55, float %730, float %731, float %732, float %730, float %731, float %732)  ; Dot3(ax,ay,az,bx,by,bz)
  %734 = call float @dx.op.unary.f32(i32 25, float %733)  ; Rsqrt(value)
  %735 = fmul fast float %734, %730
  %736 = fmul fast float %734, %731
  %737 = fmul fast float %734, %732
  %738 = icmp eq i32 %85, 2
  %739 = fsub fast float -0.000000e+00, %735
  %740 = fsub fast float -0.000000e+00, %736
  %741 = fsub fast float -0.000000e+00, %737
  %742 = select i1 %738, float %739, float %735
  %743 = select i1 %738, float %740, float %736
  %744 = select i1 %738, float %741, float %737
  %745 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %746 = extractvalue %dx.types.CBufRet.f32 %745, 0
  %747 = extractvalue %dx.types.CBufRet.f32 %745, 1
  %748 = extractvalue %dx.types.CBufRet.f32 %745, 2
  %749 = fmul fast float %708, %164
  %750 = fmul fast float %749, %742
  %751 = fmul fast float %749, %743
  %752 = fmul fast float %749, %744
  %753 = fsub fast float %709, %750
  %754 = fsub fast float %710, %751
  %755 = fsub fast float %711, %752
  %756 = fsub fast float %746, %753
  %757 = fsub fast float %747, %754
  %758 = fsub fast float %748, %755
  %759 = call float @dx.op.dot3.f32(i32 55, float %756, float %757, float %758, float %756, float %757, float %758)  ; Dot3(ax,ay,az,bx,by,bz)
  %760 = call float @dx.op.unary.f32(i32 25, float %759)  ; Rsqrt(value)
  %761 = fmul fast float %756, %760
  %762 = fmul fast float %757, %760
  %763 = fmul fast float %758, %760
  %764 = fsub fast float -0.000000e+00, %742
  %765 = fsub fast float -0.000000e+00, %743
  %766 = fsub fast float -0.000000e+00, %744
  %767 = call float @dx.op.dot3.f32(i32 55, float %761, float %762, float %763, float %764, float %765, float %766)  ; Dot3(ax,ay,az,bx,by,bz)
  %768 = fcmp fast ogt float %767, %155
  br i1 %768, label %2932, label %769

; <label>:769                                     ; preds = %3072, %729
  %770 = and i32 %80, 512
  %771 = icmp eq i32 %770, 0
  br i1 %771, label %1014, label %772

; <label>:772                                     ; preds = %769
  br label %773

; <label>:773                                     ; preds = %773, %772
  %774 = phi i32 [ %824, %773 ], [ 0, %772 ]
  %775 = and i32 %774, 1
  %776 = icmp ne i32 %775, 0
  %777 = select i1 %776, float %175, float %165
  %778 = and i32 %774, 2
  %779 = icmp ne i32 %778, 0
  %780 = select i1 %779, float %176, float %166
  %781 = and i32 %774, 4
  %782 = icmp ne i32 %781, 0
  %783 = select i1 %782, float %177, float %167
  %784 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %683, i32 0)  ; BufferLoad(srv,index,wot)
  %785 = extractvalue %dx.types.ResRet.f32 %784, 0
  %786 = extractvalue %dx.types.ResRet.f32 %784, 1
  %787 = extractvalue %dx.types.ResRet.f32 %784, 2
  %788 = extractvalue %dx.types.ResRet.f32 %784, 3
  %789 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %690, i32 0)  ; BufferLoad(srv,index,wot)
  %790 = extractvalue %dx.types.ResRet.f32 %789, 0
  %791 = extractvalue %dx.types.ResRet.f32 %789, 1
  %792 = extractvalue %dx.types.ResRet.f32 %789, 2
  %793 = extractvalue %dx.types.ResRet.f32 %789, 3
  %794 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %697, i32 0)  ; BufferLoad(srv,index,wot)
  %795 = extractvalue %dx.types.ResRet.f32 %794, 0
  %796 = extractvalue %dx.types.ResRet.f32 %794, 1
  %797 = extractvalue %dx.types.ResRet.f32 %794, 2
  %798 = extractvalue %dx.types.ResRet.f32 %794, 3
  %799 = call float @dx.op.dot4.f32(i32 56, float %777, float %780, float %783, float 1.000000e+00, float %785, float %786, float %787, float %788)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %800 = call float @dx.op.dot4.f32(i32 56, float %777, float %780, float %783, float 1.000000e+00, float %790, float %791, float %792, float %793)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %801 = call float @dx.op.dot4.f32(i32 56, float %777, float %780, float %783, float 1.000000e+00, float %795, float %796, float %797, float %798)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %802 = fmul fast float %799, %662
  %803 = call float @dx.op.tertiary.f32(i32 46, float %800, float %667, float %802)  ; FMad(a,b,c)
  %804 = call float @dx.op.tertiary.f32(i32 46, float %801, float %672, float %803)  ; FMad(a,b,c)
  %805 = fadd fast float %804, %677
  %806 = fmul fast float %799, %663
  %807 = call float @dx.op.tertiary.f32(i32 46, float %800, float %668, float %806)  ; FMad(a,b,c)
  %808 = call float @dx.op.tertiary.f32(i32 46, float %801, float %673, float %807)  ; FMad(a,b,c)
  %809 = fadd fast float %808, %678
  %810 = fmul fast float %799, %664
  %811 = call float @dx.op.tertiary.f32(i32 46, float %800, float %669, float %810)  ; FMad(a,b,c)
  %812 = call float @dx.op.tertiary.f32(i32 46, float %801, float %674, float %811)  ; FMad(a,b,c)
  %813 = fadd fast float %812, %679
  %814 = fmul fast float %799, %665
  %815 = call float @dx.op.tertiary.f32(i32 46, float %800, float %670, float %814)  ; FMad(a,b,c)
  %816 = call float @dx.op.tertiary.f32(i32 46, float %801, float %675, float %815)  ; FMad(a,b,c)
  %817 = fadd fast float %816, %680
  %818 = fdiv fast float %805, %817
  %819 = fdiv fast float %809, %817
  %820 = fdiv fast float %813, %817
  %821 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 %774
  %822 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 %774
  %823 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 %774
  store float %818, float* %821, align 4
  store float %819, float* %822, align 4
  store float %820, float* %823, align 4
  %824 = add nuw nsw i32 %774, 1
  %825 = icmp eq i32 %824, 8
  br i1 %825, label %826, label %773

; <label>:826                                     ; preds = %773
  %827 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 3
  %828 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 3
  %829 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 3
  %830 = load float, float* %827, align 4
  %831 = load float, float* %828, align 4
  %832 = load float, float* %829, align 4
  %833 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 2
  %834 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 2
  %835 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 2
  %836 = load float, float* %833, align 4
  %837 = load float, float* %834, align 4
  %838 = load float, float* %835, align 4
  %839 = call float @dx.op.binary.f32(i32 36, float %836, float %830)  ; FMin(a,b)
  %840 = call float @dx.op.binary.f32(i32 36, float %837, float %831)  ; FMin(a,b)
  %841 = call float @dx.op.binary.f32(i32 36, float %838, float %832)  ; FMin(a,b)
  %842 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 1
  %843 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 1
  %844 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 1
  %845 = load float, float* %842, align 4
  %846 = load float, float* %843, align 4
  %847 = load float, float* %844, align 4
  %848 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 0
  %849 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 0
  %850 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 0
  %851 = load float, float* %848, align 4
  %852 = load float, float* %849, align 4
  %853 = load float, float* %850, align 4
  %854 = call float @dx.op.binary.f32(i32 36, float %851, float %845)  ; FMin(a,b)
  %855 = call float @dx.op.binary.f32(i32 36, float %852, float %846)  ; FMin(a,b)
  %856 = call float @dx.op.binary.f32(i32 36, float %853, float %847)  ; FMin(a,b)
  %857 = call float @dx.op.binary.f32(i32 36, float %854, float %839)  ; FMin(a,b)
  %858 = call float @dx.op.binary.f32(i32 36, float %855, float %840)  ; FMin(a,b)
  %859 = call float @dx.op.binary.f32(i32 36, float %856, float %841)  ; FMin(a,b)
  %860 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 7
  %861 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 7
  %862 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 7
  %863 = load float, float* %860, align 4
  %864 = load float, float* %861, align 4
  %865 = load float, float* %862, align 4
  %866 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 6
  %867 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 6
  %868 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 6
  %869 = load float, float* %866, align 4
  %870 = load float, float* %867, align 4
  %871 = load float, float* %868, align 4
  %872 = call float @dx.op.binary.f32(i32 36, float %869, float %863)  ; FMin(a,b)
  %873 = call float @dx.op.binary.f32(i32 36, float %870, float %864)  ; FMin(a,b)
  %874 = call float @dx.op.binary.f32(i32 36, float %871, float %865)  ; FMin(a,b)
  %875 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 5
  %876 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 5
  %877 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 5
  %878 = load float, float* %875, align 4
  %879 = load float, float* %876, align 4
  %880 = load float, float* %877, align 4
  %881 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 4
  %882 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 4
  %883 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 4
  %884 = load float, float* %881, align 4
  %885 = load float, float* %882, align 4
  %886 = load float, float* %883, align 4
  %887 = call float @dx.op.binary.f32(i32 36, float %884, float %878)  ; FMin(a,b)
  %888 = call float @dx.op.binary.f32(i32 36, float %885, float %879)  ; FMin(a,b)
  %889 = call float @dx.op.binary.f32(i32 36, float %886, float %880)  ; FMin(a,b)
  %890 = call float @dx.op.binary.f32(i32 36, float %887, float %872)  ; FMin(a,b)
  %891 = call float @dx.op.binary.f32(i32 36, float %888, float %873)  ; FMin(a,b)
  %892 = call float @dx.op.binary.f32(i32 36, float %889, float %874)  ; FMin(a,b)
  %893 = call float @dx.op.binary.f32(i32 36, float %857, float %890)  ; FMin(a,b)
  %894 = call float @dx.op.binary.f32(i32 36, float %858, float %891)  ; FMin(a,b)
  %895 = call float @dx.op.binary.f32(i32 36, float %859, float %892)  ; FMin(a,b)
  %896 = call float @dx.op.binary.f32(i32 35, float %836, float %830)  ; FMax(a,b)
  %897 = call float @dx.op.binary.f32(i32 35, float %837, float %831)  ; FMax(a,b)
  %898 = call float @dx.op.binary.f32(i32 35, float %838, float %832)  ; FMax(a,b)
  %899 = call float @dx.op.binary.f32(i32 35, float %851, float %845)  ; FMax(a,b)
  %900 = call float @dx.op.binary.f32(i32 35, float %852, float %846)  ; FMax(a,b)
  %901 = call float @dx.op.binary.f32(i32 35, float %853, float %847)  ; FMax(a,b)
  %902 = call float @dx.op.binary.f32(i32 35, float %899, float %896)  ; FMax(a,b)
  %903 = call float @dx.op.binary.f32(i32 35, float %900, float %897)  ; FMax(a,b)
  %904 = call float @dx.op.binary.f32(i32 35, float %901, float %898)  ; FMax(a,b)
  %905 = call float @dx.op.binary.f32(i32 35, float %869, float %863)  ; FMax(a,b)
  %906 = call float @dx.op.binary.f32(i32 35, float %870, float %864)  ; FMax(a,b)
  %907 = call float @dx.op.binary.f32(i32 35, float %871, float %865)  ; FMax(a,b)
  %908 = call float @dx.op.binary.f32(i32 35, float %884, float %878)  ; FMax(a,b)
  %909 = call float @dx.op.binary.f32(i32 35, float %885, float %879)  ; FMax(a,b)
  %910 = call float @dx.op.binary.f32(i32 35, float %886, float %880)  ; FMax(a,b)
  %911 = call float @dx.op.binary.f32(i32 35, float %908, float %905)  ; FMax(a,b)
  %912 = call float @dx.op.binary.f32(i32 35, float %909, float %906)  ; FMax(a,b)
  %913 = call float @dx.op.binary.f32(i32 35, float %910, float %907)  ; FMax(a,b)
  %914 = call float @dx.op.binary.f32(i32 35, float %902, float %911)  ; FMax(a,b)
  %915 = call float @dx.op.binary.f32(i32 35, float %903, float %912)  ; FMax(a,b)
  %916 = call float @dx.op.binary.f32(i32 35, float %904, float %913)  ; FMax(a,b)
  %917 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %918 = extractvalue %dx.types.CBufRet.i32 %917, 0
  %919 = and i32 %918, 15
  %920 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %921 = extractvalue %dx.types.CBufRet.f32 %920, 0
  %922 = extractvalue %dx.types.CBufRet.f32 %920, 1
  %923 = extractvalue %dx.types.CBufRet.f32 %920, 2
  %924 = extractvalue %dx.types.CBufRet.f32 %920, 3
  %925 = fmul fast float %893, 5.000000e-01
  %926 = fmul fast float %915, 5.000000e-01
  %927 = fadd fast float %925, 5.000000e-01
  %928 = fsub fast float 5.000000e-01, %926
  %929 = fmul fast float %914, 5.000000e-01
  %930 = fmul fast float %894, 5.000000e-01
  %931 = fadd fast float %929, 5.000000e-01
  %932 = fsub fast float 5.000000e-01, %930
  %933 = fmul fast float %921, %927
  %934 = fmul fast float %922, %928
  %935 = fadd fast float %933, %923
  %936 = fadd fast float %934, %924
  %937 = fmul fast float %921, %931
  %938 = fmul fast float %922, %932
  %939 = fadd fast float %937, %923
  %940 = fadd fast float %938, %924
  %941 = call float @dx.op.binary.f32(i32 35, float %935, float 0.000000e+00)  ; FMax(a,b)
  %942 = call float @dx.op.binary.f32(i32 35, float %936, float 0.000000e+00)  ; FMax(a,b)
  %943 = call float @dx.op.binary.f32(i32 36, float %941, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %944 = call float @dx.op.binary.f32(i32 36, float %942, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %945 = call float @dx.op.binary.f32(i32 35, float %939, float 0.000000e+00)  ; FMax(a,b)
  %946 = call float @dx.op.binary.f32(i32 35, float %940, float 0.000000e+00)  ; FMax(a,b)
  %947 = call float @dx.op.binary.f32(i32 36, float %945, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %948 = call float @dx.op.binary.f32(i32 36, float %946, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %949 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %950 = extractvalue %dx.types.ResRet.i32 %949, 0
  %951 = icmp slt i32 %950, 1
  br i1 %951, label %1014, label %952

; <label>:952                                     ; preds = %826
  %953 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %954 = extractvalue %dx.types.ResRet.i32 %953, 0
  %955 = icmp ult i32 %919, %954
  br i1 %955, label %956, label %1014

; <label>:956                                     ; preds = %952
  %957 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %958 = extractvalue %dx.types.ResRet.i32 %957, 0
  %959 = uitofp i32 %950 to float
  %960 = uitofp i32 %958 to float
  %961 = fmul fast float %959, %943
  %962 = fmul fast float %960, %944
  %963 = fptoui float %961 to i32
  %964 = fptoui float %962 to i32
  %965 = mul i32 %964, %950
  %966 = add i32 %965, %963
  %967 = fmul fast float %959, %947
  %968 = fptoui float %967 to i32
  %969 = add i32 %965, %968
  %970 = fmul fast float %960, %948
  %971 = fptoui float %970 to i32
  %972 = mul i32 %971, %950
  %973 = add i32 %972, %963
  %974 = mul i32 %950, %919
  %975 = mul i32 %974, %958
  %976 = add nsw i32 %975, 3
  %977 = icmp sge i32 %966, %950
  %978 = srem i32 %966, %950
  %979 = icmp ne i32 %978, 0
  %980 = and i1 %979, %977
  br i1 %980, label %981, label %987

; <label>:981                                     ; preds = %956
  %982 = xor i32 %950, -1
  %983 = add i32 %976, %982
  %984 = add i32 %983, %966
  %985 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %984, i32 undef)  ; BufferLoad(srv,index,wot)
  %986 = extractvalue %dx.types.ResRet.i32 %985, 0
  br label %987

; <label>:987                                     ; preds = %981, %956
  %988 = phi i32 [ %986, %981 ], [ 0, %956 ]
  %989 = icmp slt i32 %969, %950
  br i1 %989, label %995, label %990

; <label>:990                                     ; preds = %987
  %991 = sub i32 %976, %950
  %992 = add i32 %991, %969
  %993 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %992, i32 undef)  ; BufferLoad(srv,index,wot)
  %994 = extractvalue %dx.types.ResRet.i32 %993, 0
  br label %995

; <label>:995                                     ; preds = %990, %987
  %996 = phi i32 [ %994, %990 ], [ 0, %987 ]
  %997 = srem i32 %973, %950
  %998 = icmp eq i32 %997, 0
  br i1 %998, label %1004, label %999

; <label>:999                                     ; preds = %995
  %1000 = add i32 %975, 2
  %1001 = add i32 %1000, %973
  %1002 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %1001, i32 undef)  ; BufferLoad(srv,index,wot)
  %1003 = extractvalue %dx.types.ResRet.i32 %1002, 0
  br label %1004

; <label>:1004                                    ; preds = %999, %995
  %1005 = phi i32 [ %1003, %999 ], [ 0, %995 ]
  %1006 = add i32 %976, %968
  %1007 = add i32 %1006, %972
  %1008 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %1007, i32 undef)  ; BufferLoad(srv,index,wot)
  %1009 = extractvalue %dx.types.ResRet.i32 %1008, 0
  %1010 = add i32 %1005, %996
  %1011 = sub i32 %1009, %1010
  %1012 = sub i32 0, %988
  %1013 = icmp eq i32 %1011, %1012
  br i1 %1013, label %2932, label %1014

; <label>:1014                                    ; preds = %1004, %952, %826, %769
  %1015 = phi i32 [ 0, %769 ], [ 1, %826 ], [ 1, %952 ], [ 1, %1004 ]
  %1016 = phi float [ 0.000000e+00, %769 ], [ %893, %826 ], [ %893, %952 ], [ %893, %1004 ]
  %1017 = phi float [ 0.000000e+00, %769 ], [ %894, %826 ], [ %894, %952 ], [ %894, %1004 ]
  %1018 = phi float [ 0.000000e+00, %769 ], [ %895, %826 ], [ %895, %952 ], [ %895, %1004 ]
  %1019 = phi float [ 0.000000e+00, %769 ], [ %914, %826 ], [ %914, %952 ], [ %914, %1004 ]
  %1020 = phi float [ 0.000000e+00, %769 ], [ %915, %826 ], [ %915, %952 ], [ %915, %1004 ]
  %1021 = phi float [ 0.000000e+00, %769 ], [ %916, %826 ], [ %916, %952 ], [ %916, %1004 ]
  br i1 %183, label %1022, label %1177

; <label>:1022                                    ; preds = %1014
  %1023 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %683, i32 0)  ; BufferLoad(srv,index,wot)
  %1024 = extractvalue %dx.types.ResRet.f32 %1023, 0
  %1025 = extractvalue %dx.types.ResRet.f32 %1023, 1
  %1026 = extractvalue %dx.types.ResRet.f32 %1023, 2
  %1027 = extractvalue %dx.types.ResRet.f32 %1023, 3
  %1028 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %690, i32 0)  ; BufferLoad(srv,index,wot)
  %1029 = extractvalue %dx.types.ResRet.f32 %1028, 0
  %1030 = extractvalue %dx.types.ResRet.f32 %1028, 1
  %1031 = extractvalue %dx.types.ResRet.f32 %1028, 2
  %1032 = extractvalue %dx.types.ResRet.f32 %1028, 3
  %1033 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %697, i32 0)  ; BufferLoad(srv,index,wot)
  %1034 = extractvalue %dx.types.ResRet.f32 %1033, 0
  %1035 = extractvalue %dx.types.ResRet.f32 %1033, 1
  %1036 = extractvalue %dx.types.ResRet.f32 %1033, 2
  %1037 = extractvalue %dx.types.ResRet.f32 %1033, 3
  %1038 = call float @dx.op.dot3.f32(i32 55, float %1034, float %1035, float %1036, float %1034, float %1035, float %1036)  ; Dot3(ax,ay,az,bx,by,bz)
  %1039 = call float @dx.op.dot3.f32(i32 55, float %1029, float %1030, float %1031, float %1029, float %1030, float %1031)  ; Dot3(ax,ay,az,bx,by,bz)
  %1040 = call float @dx.op.binary.f32(i32 35, float %1039, float %1038)  ; FMax(a,b)
  %1041 = call float @dx.op.dot3.f32(i32 55, float %1024, float %1025, float %1026, float %1024, float %1025, float %1026)  ; Dot3(ax,ay,az,bx,by,bz)
  %1042 = call float @dx.op.binary.f32(i32 35, float %1041, float %1040)  ; FMax(a,b)
  %1043 = call float @dx.op.unary.f32(i32 24, float %1042)  ; Sqrt(value)
  %1044 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1024, float %1025, float %1026, float %1027)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1045 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1029, float %1030, float %1031, float %1032)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1046 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1034, float %1035, float %1036, float %1037)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1047 = fmul fast float %1043, %143
  %1048 = fmul fast float %1044, %662
  %1049 = call float @dx.op.tertiary.f32(i32 46, float %1045, float %667, float %1048)  ; FMad(a,b,c)
  %1050 = call float @dx.op.tertiary.f32(i32 46, float %1046, float %672, float %1049)  ; FMad(a,b,c)
  %1051 = fadd fast float %1050, %677
  %1052 = fmul fast float %1044, %663
  %1053 = call float @dx.op.tertiary.f32(i32 46, float %1045, float %668, float %1052)  ; FMad(a,b,c)
  %1054 = call float @dx.op.tertiary.f32(i32 46, float %1046, float %673, float %1053)  ; FMad(a,b,c)
  %1055 = fadd fast float %1054, %678
  %1056 = fmul fast float %1044, %665
  %1057 = call float @dx.op.tertiary.f32(i32 46, float %1045, float %670, float %1056)  ; FMad(a,b,c)
  %1058 = call float @dx.op.tertiary.f32(i32 46, float %1046, float %675, float %1057)  ; FMad(a,b,c)
  %1059 = fadd fast float %1058, %680
  %1060 = fdiv fast float %1051, %1059
  %1061 = fdiv fast float %1055, %1059
  %1062 = fmul fast float %1060, 5.000000e-01
  %1063 = fmul fast float %1061, 5.000000e-01
  %1064 = fadd fast float %1062, 5.000000e-01
  %1065 = fadd fast float %1063, 5.000000e-01
  %1066 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1067 = extractvalue %dx.types.CBufRet.f32 %1066, 0
  %1068 = extractvalue %dx.types.CBufRet.f32 %1066, 1
  %1069 = extractvalue %dx.types.CBufRet.f32 %1066, 2
  %1070 = fmul fast float %1067, 5.000000e-01
  %1071 = fmul fast float %1068, 5.000000e-01
  %1072 = fmul fast float %1067, %1064
  %1073 = call float @dx.op.unary.f32(i32 26, float %1072)  ; Round_ne(value)
  %1074 = fsub fast float %1073, %1070
  %1075 = fmul fast float %1068, %1065
  %1076 = call float @dx.op.unary.f32(i32 26, float %1075)  ; Round_ne(value)
  %1077 = fsub fast float %1076, %1071
  %1078 = fsub fast float -0.000000e+00, %1069
  %1079 = fadd fast float %1074, -5.000000e-01
  %1080 = fadd fast float %1077, -5.000000e-01
  %1081 = fadd fast float %1077, 5.000000e-01
  %1082 = fadd fast float %1074, 5.000000e-01
  %1083 = call float @dx.op.dot3.f32(i32 55, float %1079, float %1080, float %1078, float %1079, float %1080, float %1078)  ; Dot3(ax,ay,az,bx,by,bz)
  %1084 = call float @dx.op.unary.f32(i32 25, float %1083)  ; Rsqrt(value)
  %1085 = fmul fast float %1084, %1079
  %1086 = fmul fast float %1080, %1084
  %1087 = fmul fast float %1084, %1078
  %1088 = call float @dx.op.dot3.f32(i32 55, float %1079, float %1081, float %1078, float %1079, float %1081, float %1078)  ; Dot3(ax,ay,az,bx,by,bz)
  %1089 = call float @dx.op.unary.f32(i32 25, float %1088)  ; Rsqrt(value)
  %1090 = fmul fast float %1089, %1079
  %1091 = fmul fast float %1089, %1081
  %1092 = fmul fast float %1089, %1078
  %1093 = call float @dx.op.dot3.f32(i32 55, float %1082, float %1080, float %1078, float %1082, float %1080, float %1078)  ; Dot3(ax,ay,az,bx,by,bz)
  %1094 = call float @dx.op.unary.f32(i32 25, float %1093)  ; Rsqrt(value)
  %1095 = fmul fast float %1094, %1082
  %1096 = fmul fast float %1094, %1080
  %1097 = fmul fast float %1094, %1078
  %1098 = call float @dx.op.dot3.f32(i32 55, float %1082, float %1081, float %1078, float %1082, float %1081, float %1078)  ; Dot3(ax,ay,az,bx,by,bz)
  %1099 = call float @dx.op.unary.f32(i32 25, float %1098)  ; Rsqrt(value)
  %1100 = fmul fast float %1099, %1082
  %1101 = fmul fast float %1099, %1081
  %1102 = fmul fast float %1099, %1078
  %1103 = fmul fast float %1059, %1060
  %1104 = fmul fast float %1103, %1070
  %1105 = fdiv fast float %1104, %1069
  %1106 = fmul fast float %1061, %1059
  %1107 = fmul fast float %1106, %1071
  %1108 = fdiv fast float %1107, %1069
  %1109 = fsub fast float -0.000000e+00, %1059
  %1110 = fmul fast float %1108, %1087
  %1111 = fmul fast float %1086, %1109
  %1112 = fsub fast float %1110, %1111
  %1113 = fmul fast float %1085, %1109
  %1114 = fmul fast float %1105, %1087
  %1115 = fsub fast float %1113, %1114
  %1116 = fmul fast float %1105, %1086
  %1117 = fmul fast float %1108, %1085
  %1118 = fsub fast float %1116, %1117
  %1119 = fmul fast float %1112, %1112
  %1120 = fmul fast float %1115, %1115
  %1121 = fadd fast float %1119, %1120
  %1122 = fmul fast float %1118, %1118
  %1123 = fadd fast float %1121, %1122
  %1124 = call float @dx.op.unary.f32(i32 24, float %1123)  ; Sqrt(value)
  %1125 = fcmp fast olt float %1124, %1047
  br i1 %1125, label %1177, label %1126

; <label>:1126                                    ; preds = %1022
  %1127 = fmul fast float %1108, %1092
  %1128 = fmul fast float %1091, %1109
  %1129 = fsub fast float %1127, %1128
  %1130 = fmul fast float %1090, %1109
  %1131 = fmul fast float %1105, %1092
  %1132 = fsub fast float %1130, %1131
  %1133 = fmul fast float %1105, %1091
  %1134 = fmul fast float %1108, %1090
  %1135 = fsub fast float %1133, %1134
  %1136 = fmul fast float %1129, %1129
  %1137 = fmul fast float %1132, %1132
  %1138 = fadd fast float %1136, %1137
  %1139 = fmul fast float %1135, %1135
  %1140 = fadd fast float %1138, %1139
  %1141 = call float @dx.op.unary.f32(i32 24, float %1140)  ; Sqrt(value)
  %1142 = fcmp fast olt float %1141, %1047
  br i1 %1142, label %1177, label %1143

; <label>:1143                                    ; preds = %1126
  %1144 = fmul fast float %1108, %1097
  %1145 = fmul fast float %1096, %1109
  %1146 = fsub fast float %1144, %1145
  %1147 = fmul fast float %1095, %1109
  %1148 = fmul fast float %1105, %1097
  %1149 = fsub fast float %1147, %1148
  %1150 = fmul fast float %1105, %1096
  %1151 = fmul fast float %1108, %1095
  %1152 = fsub fast float %1150, %1151
  %1153 = fmul fast float %1146, %1146
  %1154 = fmul fast float %1149, %1149
  %1155 = fadd fast float %1153, %1154
  %1156 = fmul fast float %1152, %1152
  %1157 = fadd fast float %1155, %1156
  %1158 = call float @dx.op.unary.f32(i32 24, float %1157)  ; Sqrt(value)
  %1159 = fcmp fast olt float %1158, %1047
  br i1 %1159, label %1177, label %1160

; <label>:1160                                    ; preds = %1143
  %1161 = fmul fast float %1108, %1102
  %1162 = fmul fast float %1101, %1109
  %1163 = fsub fast float %1161, %1162
  %1164 = fmul fast float %1100, %1109
  %1165 = fmul fast float %1105, %1102
  %1166 = fsub fast float %1164, %1165
  %1167 = fmul fast float %1105, %1101
  %1168 = fmul fast float %1108, %1100
  %1169 = fsub fast float %1167, %1168
  %1170 = fmul fast float %1163, %1163
  %1171 = fmul fast float %1166, %1166
  %1172 = fadd fast float %1170, %1171
  %1173 = fmul fast float %1169, %1169
  %1174 = fadd fast float %1172, %1173
  %1175 = call float @dx.op.unary.f32(i32 24, float %1174)  ; Sqrt(value)
  %1176 = fcmp fast olt float %1175, %1047
  br i1 %1176, label %1177, label %2932

; <label>:1177                                    ; preds = %1160, %1143, %1126, %1022, %1014
  %1178 = fcmp uge float %3080, %712
  %1179 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1180 = extractvalue %dx.types.CBufRet.i32 %1179, 0
  %1181 = icmp sgt i32 %1180, 0
  %1182 = and i1 %1178, %1181
  br i1 %1182, label %1183, label %1645

; <label>:1183                                    ; preds = %1177
  %1184 = icmp eq i32 %1015, 0
  br i1 %1184, label %1185, label %1330

; <label>:1185                                    ; preds = %1183
  br label %1186

; <label>:1186                                    ; preds = %1186, %1185
  %1187 = phi i32 [ %1237, %1186 ], [ 0, %1185 ]
  %1188 = and i32 %1187, 1
  %1189 = icmp ne i32 %1188, 0
  %1190 = select i1 %1189, float %175, float %165
  %1191 = and i32 %1187, 2
  %1192 = icmp ne i32 %1191, 0
  %1193 = select i1 %1192, float %176, float %166
  %1194 = and i32 %1187, 4
  %1195 = icmp ne i32 %1194, 0
  %1196 = select i1 %1195, float %177, float %167
  %1197 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %683, i32 0)  ; BufferLoad(srv,index,wot)
  %1198 = extractvalue %dx.types.ResRet.f32 %1197, 0
  %1199 = extractvalue %dx.types.ResRet.f32 %1197, 1
  %1200 = extractvalue %dx.types.ResRet.f32 %1197, 2
  %1201 = extractvalue %dx.types.ResRet.f32 %1197, 3
  %1202 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %690, i32 0)  ; BufferLoad(srv,index,wot)
  %1203 = extractvalue %dx.types.ResRet.f32 %1202, 0
  %1204 = extractvalue %dx.types.ResRet.f32 %1202, 1
  %1205 = extractvalue %dx.types.ResRet.f32 %1202, 2
  %1206 = extractvalue %dx.types.ResRet.f32 %1202, 3
  %1207 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %697, i32 0)  ; BufferLoad(srv,index,wot)
  %1208 = extractvalue %dx.types.ResRet.f32 %1207, 0
  %1209 = extractvalue %dx.types.ResRet.f32 %1207, 1
  %1210 = extractvalue %dx.types.ResRet.f32 %1207, 2
  %1211 = extractvalue %dx.types.ResRet.f32 %1207, 3
  %1212 = call float @dx.op.dot4.f32(i32 56, float %1190, float %1193, float %1196, float 1.000000e+00, float %1198, float %1199, float %1200, float %1201)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1213 = call float @dx.op.dot4.f32(i32 56, float %1190, float %1193, float %1196, float 1.000000e+00, float %1203, float %1204, float %1205, float %1206)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1214 = call float @dx.op.dot4.f32(i32 56, float %1190, float %1193, float %1196, float 1.000000e+00, float %1208, float %1209, float %1210, float %1211)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1215 = fmul fast float %1212, %662
  %1216 = call float @dx.op.tertiary.f32(i32 46, float %1213, float %667, float %1215)  ; FMad(a,b,c)
  %1217 = call float @dx.op.tertiary.f32(i32 46, float %1214, float %672, float %1216)  ; FMad(a,b,c)
  %1218 = fadd fast float %1217, %677
  %1219 = fmul fast float %1212, %663
  %1220 = call float @dx.op.tertiary.f32(i32 46, float %1213, float %668, float %1219)  ; FMad(a,b,c)
  %1221 = call float @dx.op.tertiary.f32(i32 46, float %1214, float %673, float %1220)  ; FMad(a,b,c)
  %1222 = fadd fast float %1221, %678
  %1223 = fmul fast float %1212, %664
  %1224 = call float @dx.op.tertiary.f32(i32 46, float %1213, float %669, float %1223)  ; FMad(a,b,c)
  %1225 = call float @dx.op.tertiary.f32(i32 46, float %1214, float %674, float %1224)  ; FMad(a,b,c)
  %1226 = fadd fast float %1225, %679
  %1227 = fmul fast float %1212, %665
  %1228 = call float @dx.op.tertiary.f32(i32 46, float %1213, float %670, float %1227)  ; FMad(a,b,c)
  %1229 = call float @dx.op.tertiary.f32(i32 46, float %1214, float %675, float %1228)  ; FMad(a,b,c)
  %1230 = fadd fast float %1229, %680
  %1231 = fdiv fast float %1218, %1230
  %1232 = fdiv fast float %1222, %1230
  %1233 = fdiv fast float %1226, %1230
  %1234 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 %1187
  %1235 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 %1187
  %1236 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 %1187
  store float %1231, float* %1234, align 4
  store float %1232, float* %1235, align 4
  store float %1233, float* %1236, align 4
  %1237 = add nuw nsw i32 %1187, 1
  %1238 = icmp eq i32 %1237, 8
  br i1 %1238, label %1239, label %1186

; <label>:1239                                    ; preds = %1186
  %1240 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 3
  %1241 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 3
  %1242 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 3
  %1243 = load float, float* %1240, align 4
  %1244 = load float, float* %1241, align 4
  %1245 = load float, float* %1242, align 4
  %1246 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 2
  %1247 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 2
  %1248 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 2
  %1249 = load float, float* %1246, align 4
  %1250 = load float, float* %1247, align 4
  %1251 = load float, float* %1248, align 4
  %1252 = call float @dx.op.binary.f32(i32 36, float %1249, float %1243)  ; FMin(a,b)
  %1253 = call float @dx.op.binary.f32(i32 36, float %1250, float %1244)  ; FMin(a,b)
  %1254 = call float @dx.op.binary.f32(i32 36, float %1251, float %1245)  ; FMin(a,b)
  %1255 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 1
  %1256 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 1
  %1257 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 1
  %1258 = load float, float* %1255, align 4
  %1259 = load float, float* %1256, align 4
  %1260 = load float, float* %1257, align 4
  %1261 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 0
  %1262 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 0
  %1263 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 0
  %1264 = load float, float* %1261, align 4
  %1265 = load float, float* %1262, align 4
  %1266 = load float, float* %1263, align 4
  %1267 = call float @dx.op.binary.f32(i32 36, float %1264, float %1258)  ; FMin(a,b)
  %1268 = call float @dx.op.binary.f32(i32 36, float %1265, float %1259)  ; FMin(a,b)
  %1269 = call float @dx.op.binary.f32(i32 36, float %1266, float %1260)  ; FMin(a,b)
  %1270 = call float @dx.op.binary.f32(i32 36, float %1267, float %1252)  ; FMin(a,b)
  %1271 = call float @dx.op.binary.f32(i32 36, float %1268, float %1253)  ; FMin(a,b)
  %1272 = call float @dx.op.binary.f32(i32 36, float %1269, float %1254)  ; FMin(a,b)
  %1273 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 7
  %1274 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 7
  %1275 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 7
  %1276 = load float, float* %1273, align 4
  %1277 = load float, float* %1274, align 4
  %1278 = load float, float* %1275, align 4
  %1279 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 6
  %1280 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 6
  %1281 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 6
  %1282 = load float, float* %1279, align 4
  %1283 = load float, float* %1280, align 4
  %1284 = load float, float* %1281, align 4
  %1285 = call float @dx.op.binary.f32(i32 36, float %1282, float %1276)  ; FMin(a,b)
  %1286 = call float @dx.op.binary.f32(i32 36, float %1283, float %1277)  ; FMin(a,b)
  %1287 = call float @dx.op.binary.f32(i32 36, float %1284, float %1278)  ; FMin(a,b)
  %1288 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 5
  %1289 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 5
  %1290 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 5
  %1291 = load float, float* %1288, align 4
  %1292 = load float, float* %1289, align 4
  %1293 = load float, float* %1290, align 4
  %1294 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 4
  %1295 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 4
  %1296 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 4
  %1297 = load float, float* %1294, align 4
  %1298 = load float, float* %1295, align 4
  %1299 = load float, float* %1296, align 4
  %1300 = call float @dx.op.binary.f32(i32 36, float %1297, float %1291)  ; FMin(a,b)
  %1301 = call float @dx.op.binary.f32(i32 36, float %1298, float %1292)  ; FMin(a,b)
  %1302 = call float @dx.op.binary.f32(i32 36, float %1299, float %1293)  ; FMin(a,b)
  %1303 = call float @dx.op.binary.f32(i32 36, float %1300, float %1285)  ; FMin(a,b)
  %1304 = call float @dx.op.binary.f32(i32 36, float %1301, float %1286)  ; FMin(a,b)
  %1305 = call float @dx.op.binary.f32(i32 36, float %1302, float %1287)  ; FMin(a,b)
  %1306 = call float @dx.op.binary.f32(i32 36, float %1270, float %1303)  ; FMin(a,b)
  %1307 = call float @dx.op.binary.f32(i32 36, float %1271, float %1304)  ; FMin(a,b)
  %1308 = call float @dx.op.binary.f32(i32 36, float %1272, float %1305)  ; FMin(a,b)
  %1309 = call float @dx.op.binary.f32(i32 35, float %1249, float %1243)  ; FMax(a,b)
  %1310 = call float @dx.op.binary.f32(i32 35, float %1250, float %1244)  ; FMax(a,b)
  %1311 = call float @dx.op.binary.f32(i32 35, float %1251, float %1245)  ; FMax(a,b)
  %1312 = call float @dx.op.binary.f32(i32 35, float %1264, float %1258)  ; FMax(a,b)
  %1313 = call float @dx.op.binary.f32(i32 35, float %1265, float %1259)  ; FMax(a,b)
  %1314 = call float @dx.op.binary.f32(i32 35, float %1266, float %1260)  ; FMax(a,b)
  %1315 = call float @dx.op.binary.f32(i32 35, float %1312, float %1309)  ; FMax(a,b)
  %1316 = call float @dx.op.binary.f32(i32 35, float %1313, float %1310)  ; FMax(a,b)
  %1317 = call float @dx.op.binary.f32(i32 35, float %1314, float %1311)  ; FMax(a,b)
  %1318 = call float @dx.op.binary.f32(i32 35, float %1282, float %1276)  ; FMax(a,b)
  %1319 = call float @dx.op.binary.f32(i32 35, float %1283, float %1277)  ; FMax(a,b)
  %1320 = call float @dx.op.binary.f32(i32 35, float %1284, float %1278)  ; FMax(a,b)
  %1321 = call float @dx.op.binary.f32(i32 35, float %1297, float %1291)  ; FMax(a,b)
  %1322 = call float @dx.op.binary.f32(i32 35, float %1298, float %1292)  ; FMax(a,b)
  %1323 = call float @dx.op.binary.f32(i32 35, float %1299, float %1293)  ; FMax(a,b)
  %1324 = call float @dx.op.binary.f32(i32 35, float %1321, float %1318)  ; FMax(a,b)
  %1325 = call float @dx.op.binary.f32(i32 35, float %1322, float %1319)  ; FMax(a,b)
  %1326 = call float @dx.op.binary.f32(i32 35, float %1323, float %1320)  ; FMax(a,b)
  %1327 = call float @dx.op.binary.f32(i32 35, float %1315, float %1324)  ; FMax(a,b)
  %1328 = call float @dx.op.binary.f32(i32 35, float %1316, float %1325)  ; FMax(a,b)
  %1329 = call float @dx.op.binary.f32(i32 35, float %1317, float %1326)  ; FMax(a,b)
  br label %1330

; <label>:1330                                    ; preds = %1239, %1183
  %1331 = phi float [ %1016, %1183 ], [ %1306, %1239 ]
  %1332 = phi float [ %1017, %1183 ], [ %1307, %1239 ]
  %1333 = phi float [ %1018, %1183 ], [ %1308, %1239 ]
  %1334 = phi float [ %1019, %1183 ], [ %1327, %1239 ]
  %1335 = phi float [ %1020, %1183 ], [ %1328, %1239 ]
  %1336 = phi float [ %1021, %1183 ], [ %1329, %1239 ]
  %1337 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1338 = extractvalue %dx.types.CBufRet.i32 %1337, 1
  %1339 = icmp sgt i32 %1338, 0
  br i1 %1339, label %1340, label %1511

; <label>:1340                                    ; preds = %1330
  %1341 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1342 = extractvalue %dx.types.CBufRet.f32 %1341, 0
  %1343 = extractvalue %dx.types.CBufRet.f32 %1341, 1
  %1344 = extractvalue %dx.types.CBufRet.f32 %1341, 2
  %1345 = extractvalue %dx.types.CBufRet.f32 %1341, 3
  %1346 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %1347 = extractvalue %dx.types.CBufRet.f32 %1346, 0
  %1348 = extractvalue %dx.types.CBufRet.f32 %1346, 1
  %1349 = extractvalue %dx.types.CBufRet.f32 %1346, 2
  %1350 = extractvalue %dx.types.CBufRet.f32 %1346, 3
  %1351 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1352 = extractvalue %dx.types.CBufRet.f32 %1351, 0
  %1353 = extractvalue %dx.types.CBufRet.f32 %1351, 1
  %1354 = extractvalue %dx.types.CBufRet.f32 %1351, 2
  %1355 = extractvalue %dx.types.CBufRet.f32 %1351, 3
  %1356 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1357 = extractvalue %dx.types.CBufRet.f32 %1356, 0
  %1358 = extractvalue %dx.types.CBufRet.f32 %1356, 1
  %1359 = extractvalue %dx.types.CBufRet.f32 %1356, 2
  %1360 = extractvalue %dx.types.CBufRet.f32 %1356, 3
  br label %1361

; <label>:1361                                    ; preds = %1361, %1340
  %1362 = phi i32 [ 0, %1340 ], [ %1412, %1361 ]
  %1363 = and i32 %1362, 1
  %1364 = icmp ne i32 %1363, 0
  %1365 = select i1 %1364, float %175, float %165
  %1366 = and i32 %1362, 2
  %1367 = icmp ne i32 %1366, 0
  %1368 = select i1 %1367, float %176, float %166
  %1369 = and i32 %1362, 4
  %1370 = icmp ne i32 %1369, 0
  %1371 = select i1 %1370, float %177, float %167
  %1372 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %683, i32 0)  ; BufferLoad(srv,index,wot)
  %1373 = extractvalue %dx.types.ResRet.f32 %1372, 0
  %1374 = extractvalue %dx.types.ResRet.f32 %1372, 1
  %1375 = extractvalue %dx.types.ResRet.f32 %1372, 2
  %1376 = extractvalue %dx.types.ResRet.f32 %1372, 3
  %1377 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %690, i32 0)  ; BufferLoad(srv,index,wot)
  %1378 = extractvalue %dx.types.ResRet.f32 %1377, 0
  %1379 = extractvalue %dx.types.ResRet.f32 %1377, 1
  %1380 = extractvalue %dx.types.ResRet.f32 %1377, 2
  %1381 = extractvalue %dx.types.ResRet.f32 %1377, 3
  %1382 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %697, i32 0)  ; BufferLoad(srv,index,wot)
  %1383 = extractvalue %dx.types.ResRet.f32 %1382, 0
  %1384 = extractvalue %dx.types.ResRet.f32 %1382, 1
  %1385 = extractvalue %dx.types.ResRet.f32 %1382, 2
  %1386 = extractvalue %dx.types.ResRet.f32 %1382, 3
  %1387 = call float @dx.op.dot4.f32(i32 56, float %1365, float %1368, float %1371, float 1.000000e+00, float %1373, float %1374, float %1375, float %1376)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1388 = call float @dx.op.dot4.f32(i32 56, float %1365, float %1368, float %1371, float 1.000000e+00, float %1378, float %1379, float %1380, float %1381)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1389 = call float @dx.op.dot4.f32(i32 56, float %1365, float %1368, float %1371, float 1.000000e+00, float %1383, float %1384, float %1385, float %1386)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1390 = fmul fast float %1387, %1342
  %1391 = call float @dx.op.tertiary.f32(i32 46, float %1388, float %1347, float %1390)  ; FMad(a,b,c)
  %1392 = call float @dx.op.tertiary.f32(i32 46, float %1389, float %1352, float %1391)  ; FMad(a,b,c)
  %1393 = fadd fast float %1392, %1357
  %1394 = fmul fast float %1387, %1343
  %1395 = call float @dx.op.tertiary.f32(i32 46, float %1388, float %1348, float %1394)  ; FMad(a,b,c)
  %1396 = call float @dx.op.tertiary.f32(i32 46, float %1389, float %1353, float %1395)  ; FMad(a,b,c)
  %1397 = fadd fast float %1396, %1358
  %1398 = fmul fast float %1387, %1344
  %1399 = call float @dx.op.tertiary.f32(i32 46, float %1388, float %1349, float %1398)  ; FMad(a,b,c)
  %1400 = call float @dx.op.tertiary.f32(i32 46, float %1389, float %1354, float %1399)  ; FMad(a,b,c)
  %1401 = fadd fast float %1400, %1359
  %1402 = fmul fast float %1387, %1345
  %1403 = call float @dx.op.tertiary.f32(i32 46, float %1388, float %1350, float %1402)  ; FMad(a,b,c)
  %1404 = call float @dx.op.tertiary.f32(i32 46, float %1389, float %1355, float %1403)  ; FMad(a,b,c)
  %1405 = fadd fast float %1404, %1360
  %1406 = fdiv fast float %1393, %1405
  %1407 = fdiv fast float %1397, %1405
  %1408 = fdiv fast float %1401, %1405
  %1409 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 %1362
  %1410 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 %1362
  %1411 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 %1362
  store float %1406, float* %1409, align 4
  store float %1407, float* %1410, align 4
  store float %1408, float* %1411, align 4
  %1412 = add nuw nsw i32 %1362, 1
  %1413 = icmp eq i32 %1412, 8
  br i1 %1413, label %1414, label %1361

; <label>:1414                                    ; preds = %1361
  %1415 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 3
  %1416 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 3
  %1417 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 3
  %1418 = load float, float* %1415, align 4
  %1419 = load float, float* %1416, align 4
  %1420 = load float, float* %1417, align 4
  %1421 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 2
  %1422 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 2
  %1423 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 2
  %1424 = load float, float* %1421, align 4
  %1425 = load float, float* %1422, align 4
  %1426 = load float, float* %1423, align 4
  %1427 = call float @dx.op.binary.f32(i32 36, float %1424, float %1418)  ; FMin(a,b)
  %1428 = call float @dx.op.binary.f32(i32 36, float %1425, float %1419)  ; FMin(a,b)
  %1429 = call float @dx.op.binary.f32(i32 36, float %1426, float %1420)  ; FMin(a,b)
  %1430 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 1
  %1431 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 1
  %1432 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 1
  %1433 = load float, float* %1430, align 4
  %1434 = load float, float* %1431, align 4
  %1435 = load float, float* %1432, align 4
  %1436 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 0
  %1437 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 0
  %1438 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 0
  %1439 = load float, float* %1436, align 4
  %1440 = load float, float* %1437, align 4
  %1441 = load float, float* %1438, align 4
  %1442 = call float @dx.op.binary.f32(i32 36, float %1439, float %1433)  ; FMin(a,b)
  %1443 = call float @dx.op.binary.f32(i32 36, float %1440, float %1434)  ; FMin(a,b)
  %1444 = call float @dx.op.binary.f32(i32 36, float %1441, float %1435)  ; FMin(a,b)
  %1445 = call float @dx.op.binary.f32(i32 36, float %1442, float %1427)  ; FMin(a,b)
  %1446 = call float @dx.op.binary.f32(i32 36, float %1443, float %1428)  ; FMin(a,b)
  %1447 = call float @dx.op.binary.f32(i32 36, float %1444, float %1429)  ; FMin(a,b)
  %1448 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 7
  %1449 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 7
  %1450 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 7
  %1451 = load float, float* %1448, align 4
  %1452 = load float, float* %1449, align 4
  %1453 = load float, float* %1450, align 4
  %1454 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 6
  %1455 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 6
  %1456 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 6
  %1457 = load float, float* %1454, align 4
  %1458 = load float, float* %1455, align 4
  %1459 = load float, float* %1456, align 4
  %1460 = call float @dx.op.binary.f32(i32 36, float %1457, float %1451)  ; FMin(a,b)
  %1461 = call float @dx.op.binary.f32(i32 36, float %1458, float %1452)  ; FMin(a,b)
  %1462 = call float @dx.op.binary.f32(i32 36, float %1459, float %1453)  ; FMin(a,b)
  %1463 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 5
  %1464 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 5
  %1465 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 5
  %1466 = load float, float* %1463, align 4
  %1467 = load float, float* %1464, align 4
  %1468 = load float, float* %1465, align 4
  %1469 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 4
  %1470 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 4
  %1471 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 4
  %1472 = load float, float* %1469, align 4
  %1473 = load float, float* %1470, align 4
  %1474 = load float, float* %1471, align 4
  %1475 = call float @dx.op.binary.f32(i32 36, float %1472, float %1466)  ; FMin(a,b)
  %1476 = call float @dx.op.binary.f32(i32 36, float %1473, float %1467)  ; FMin(a,b)
  %1477 = call float @dx.op.binary.f32(i32 36, float %1474, float %1468)  ; FMin(a,b)
  %1478 = call float @dx.op.binary.f32(i32 36, float %1475, float %1460)  ; FMin(a,b)
  %1479 = call float @dx.op.binary.f32(i32 36, float %1476, float %1461)  ; FMin(a,b)
  %1480 = call float @dx.op.binary.f32(i32 36, float %1477, float %1462)  ; FMin(a,b)
  %1481 = call float @dx.op.binary.f32(i32 36, float %1445, float %1478)  ; FMin(a,b)
  %1482 = call float @dx.op.binary.f32(i32 36, float %1446, float %1479)  ; FMin(a,b)
  %1483 = call float @dx.op.binary.f32(i32 36, float %1447, float %1480)  ; FMin(a,b)
  %1484 = call float @dx.op.binary.f32(i32 35, float %1424, float %1418)  ; FMax(a,b)
  %1485 = call float @dx.op.binary.f32(i32 35, float %1425, float %1419)  ; FMax(a,b)
  %1486 = call float @dx.op.binary.f32(i32 35, float %1426, float %1420)  ; FMax(a,b)
  %1487 = call float @dx.op.binary.f32(i32 35, float %1439, float %1433)  ; FMax(a,b)
  %1488 = call float @dx.op.binary.f32(i32 35, float %1440, float %1434)  ; FMax(a,b)
  %1489 = call float @dx.op.binary.f32(i32 35, float %1441, float %1435)  ; FMax(a,b)
  %1490 = call float @dx.op.binary.f32(i32 35, float %1487, float %1484)  ; FMax(a,b)
  %1491 = call float @dx.op.binary.f32(i32 35, float %1488, float %1485)  ; FMax(a,b)
  %1492 = call float @dx.op.binary.f32(i32 35, float %1489, float %1486)  ; FMax(a,b)
  %1493 = call float @dx.op.binary.f32(i32 35, float %1457, float %1451)  ; FMax(a,b)
  %1494 = call float @dx.op.binary.f32(i32 35, float %1458, float %1452)  ; FMax(a,b)
  %1495 = call float @dx.op.binary.f32(i32 35, float %1459, float %1453)  ; FMax(a,b)
  %1496 = call float @dx.op.binary.f32(i32 35, float %1472, float %1466)  ; FMax(a,b)
  %1497 = call float @dx.op.binary.f32(i32 35, float %1473, float %1467)  ; FMax(a,b)
  %1498 = call float @dx.op.binary.f32(i32 35, float %1474, float %1468)  ; FMax(a,b)
  %1499 = call float @dx.op.binary.f32(i32 35, float %1496, float %1493)  ; FMax(a,b)
  %1500 = call float @dx.op.binary.f32(i32 35, float %1497, float %1494)  ; FMax(a,b)
  %1501 = call float @dx.op.binary.f32(i32 35, float %1498, float %1495)  ; FMax(a,b)
  %1502 = call float @dx.op.binary.f32(i32 35, float %1490, float %1499)  ; FMax(a,b)
  %1503 = call float @dx.op.binary.f32(i32 35, float %1491, float %1500)  ; FMax(a,b)
  %1504 = call float @dx.op.binary.f32(i32 35, float %1492, float %1501)  ; FMax(a,b)
  %1505 = call float @dx.op.binary.f32(i32 36, float %1331, float %1481)  ; FMin(a,b)
  %1506 = call float @dx.op.binary.f32(i32 36, float %1332, float %1482)  ; FMin(a,b)
  %1507 = call float @dx.op.binary.f32(i32 36, float %1333, float %1483)  ; FMin(a,b)
  %1508 = call float @dx.op.binary.f32(i32 35, float %1334, float %1502)  ; FMax(a,b)
  %1509 = call float @dx.op.binary.f32(i32 35, float %1335, float %1503)  ; FMax(a,b)
  %1510 = call float @dx.op.binary.f32(i32 35, float %1336, float %1504)  ; FMax(a,b)
  br label %1511

; <label>:1511                                    ; preds = %1414, %1330
  %1512 = phi float [ %1505, %1414 ], [ %1331, %1330 ]
  %1513 = phi float [ %1506, %1414 ], [ %1332, %1330 ]
  %1514 = phi float [ %1507, %1414 ], [ %1333, %1330 ]
  %1515 = phi float [ %1508, %1414 ], [ %1334, %1330 ]
  %1516 = phi float [ %1509, %1414 ], [ %1335, %1330 ]
  %1517 = phi float [ %1510, %1414 ], [ %1336, %1330 ]
  %1518 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1519 = extractvalue %dx.types.CBufRet.f32 %1518, 0
  %1520 = extractvalue %dx.types.CBufRet.f32 %1518, 1
  %1521 = extractvalue %dx.types.CBufRet.f32 %1518, 2
  %1522 = extractvalue %dx.types.CBufRet.f32 %1518, 3
  %1523 = fcmp fast olt float %1514, 0.000000e+00
  %1524 = fcmp fast olt float %1517, 0.000000e+00
  %1525 = or i1 %1523, %1524
  br i1 %1525, label %1645, label %1526

; <label>:1526                                    ; preds = %1511
  %1527 = extractvalue %dx.types.CBufRet.i32 %1179, 1
  %1528 = icmp sgt i32 %1527, 0
  br i1 %1528, label %1529, label %1539

; <label>:1529                                    ; preds = %1526
  %1530 = fsub fast float %1515, %1512
  %1531 = fsub fast float %1516, %1513
  %1532 = call float @dx.op.unary.f32(i32 6, float %1530)  ; FAbs(value)
  %1533 = call float @dx.op.unary.f32(i32 6, float %1531)  ; FAbs(value)
  %1534 = fmul fast float %1532, 2.500000e-01
  %1535 = fmul fast float %1534, %1533
  %1536 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1537 = extractvalue %dx.types.CBufRet.f32 %1536, 2
  %1538 = fcmp fast ogt float %1535, %1537
  br i1 %1538, label %1645, label %1539

; <label>:1539                                    ; preds = %1529, %1526
  %1540 = fmul fast float %1512, 5.000000e-01
  %1541 = fmul fast float %1516, 5.000000e-01
  %1542 = fadd fast float %1540, 5.000000e-01
  %1543 = fsub fast float 5.000000e-01, %1541
  %1544 = call float @dx.op.unary.f32(i32 7, float %1542)  ; Saturate(value)
  %1545 = call float @dx.op.unary.f32(i32 7, float %1543)  ; Saturate(value)
  %1546 = fmul fast float %1515, 5.000000e-01
  %1547 = fmul fast float %1513, 5.000000e-01
  %1548 = fadd fast float %1546, 5.000000e-01
  %1549 = fsub fast float 5.000000e-01, %1547
  %1550 = call float @dx.op.unary.f32(i32 7, float %1548)  ; Saturate(value)
  %1551 = call float @dx.op.unary.f32(i32 7, float %1549)  ; Saturate(value)
  %1552 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %14, i32 0)  ; GetDimensions(handle,mipLevel)
  %1553 = extractvalue %dx.types.Dimensions %1552, 0
  %1554 = sitofp i32 %1553 to float
  %1555 = extractvalue %dx.types.Dimensions %1552, 1
  %1556 = sitofp i32 %1555 to float
  %1557 = fmul fast float %1554, %1519
  %1558 = fmul fast float %1556, %1520
  %1559 = fsub fast float %1550, %1544
  %1560 = fsub fast float %1551, %1545
  %1561 = fmul fast float %1557, %1559
  %1562 = fmul fast float %1558, %1560
  %1563 = call float @dx.op.binary.f32(i32 35, float %1561, float %1562)  ; FMax(a,b)
  %1564 = call float @dx.op.unary.f32(i32 23, float %1563)  ; Log(value)
  %1565 = call float @dx.op.unary.f32(i32 28, float %1564)  ; Round_pi(value)
  %1566 = call float @dx.op.binary.f32(i32 36, float %1557, float %1558)  ; FMin(a,b)
  %1567 = call float @dx.op.unary.f32(i32 23, float %1566)  ; Log(value)
  %1568 = call float @dx.op.unary.f32(i32 28, float %1567)  ; Round_pi(value)
  %1569 = fadd fast float %1568, -1.000000e+00
  %1570 = call float @dx.op.binary.f32(i32 36, float %1565, float %1569)  ; FMin(a,b)
  %1571 = fadd fast float %1570, -1.000000e+00
  %1572 = call float @dx.op.binary.f32(i32 35, float %1571, float 0.000000e+00)  ; FMax(a,b)
  %1573 = fptoui float %1557 to i32
  %1574 = fptoui float %1572 to i32
  %1575 = and i32 %1574, 31
  %1576 = lshr i32 %1573, %1575
  %1577 = uitofp i32 %1576 to float
  %1578 = fptoui float %1558 to i32
  %1579 = lshr i32 %1578, %1575
  %1580 = uitofp i32 %1579 to float
  %1581 = fmul fast float %1577, %1550
  %1582 = fmul fast float %1580, %1551
  %1583 = call float @dx.op.unary.f32(i32 28, float %1581)  ; Round_pi(value)
  %1584 = call float @dx.op.unary.f32(i32 28, float %1582)  ; Round_pi(value)
  %1585 = fmul fast float %1577, %1544
  %1586 = fmul fast float %1580, %1545
  %1587 = call float @dx.op.unary.f32(i32 27, float %1585)  ; Round_ni(value)
  %1588 = call float @dx.op.unary.f32(i32 27, float %1586)  ; Round_ni(value)
  %1589 = fsub fast float %1583, %1587
  %1590 = fsub fast float %1584, %1588
  %1591 = fcmp fast ole float %1589, 2.000000e+00
  %1592 = fcmp fast ole float %1590, 2.000000e+00
  %1593 = and i1 %1591, %1592
  %1594 = fadd fast float %1572, 1.000000e+00
  %1595 = select i1 %1593, float %1572, float %1594
  %1596 = fmul fast float %1544, %1519
  %1597 = fmul fast float %1545, %1520
  %1598 = fadd fast float %1596, %1521
  %1599 = fadd fast float %1597, %1522
  %1600 = fmul fast float %1550, %1519
  %1601 = fmul fast float %1551, %1520
  %1602 = fadd fast float %1600, %1521
  %1603 = fadd fast float %1601, %1522
  %1604 = fptoui float %1554 to i32
  %1605 = fptoui float %1595 to i32
  %1606 = and i32 %1605, 31
  %1607 = lshr i32 %1604, %1606
  %1608 = uitofp i32 %1607 to float
  %1609 = fptoui float %1556 to i32
  %1610 = lshr i32 %1609, %1606
  %1611 = uitofp i32 %1610 to float
  %1612 = call float @dx.op.binary.f32(i32 35, float %1608, float 1.000000e+00)  ; FMax(a,b)
  %1613 = call float @dx.op.binary.f32(i32 35, float %1611, float 1.000000e+00)  ; FMax(a,b)
  %1614 = fadd fast float %1612, -5.000000e-01
  %1615 = fadd fast float %1613, -5.000000e-01
  %1616 = fdiv fast float %1614, %1612
  %1617 = fdiv fast float %1615, %1613
  %1618 = fmul fast float %1616, %1519
  %1619 = fmul fast float %1617, %1520
  %1620 = call float @dx.op.binary.f32(i32 36, float %1618, float %1598)  ; FMin(a,b)
  %1621 = call float @dx.op.binary.f32(i32 36, float %1619, float %1599)  ; FMin(a,b)
  %1622 = call float @dx.op.binary.f32(i32 36, float %1618, float %1602)  ; FMin(a,b)
  %1623 = call float @dx.op.binary.f32(i32 36, float %1619, float %1603)  ; FMin(a,b)
  %1624 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1620, float %1621, float undef, float undef, i32 0, i32 0, i32 undef, float %1595)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1625 = extractvalue %dx.types.ResRet.f32 %1624, 0
  %1626 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1620, float %1623, float undef, float undef, i32 0, i32 0, i32 undef, float %1595)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1627 = extractvalue %dx.types.ResRet.f32 %1626, 0
  %1628 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1622, float %1621, float undef, float undef, i32 0, i32 0, i32 undef, float %1595)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1629 = extractvalue %dx.types.ResRet.f32 %1628, 0
  %1630 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1622, float %1623, float undef, float undef, i32 0, i32 0, i32 undef, float %1595)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1631 = extractvalue %dx.types.ResRet.f32 %1630, 0
  %1632 = icmp eq i32 %682, 0
  br i1 %1632, label %1638, label %1633

; <label>:1633                                    ; preds = %1539
  %1634 = call float @dx.op.binary.f32(i32 36, float %1629, float %1631)  ; FMin(a,b)
  %1635 = call float @dx.op.binary.f32(i32 36, float %1625, float %1627)  ; FMin(a,b)
  %1636 = call float @dx.op.binary.f32(i32 36, float %1635, float %1634)  ; FMin(a,b)
  %1637 = fcmp fast olt float %1517, %1636
  br label %1643

; <label>:1638                                    ; preds = %1539
  %1639 = call float @dx.op.binary.f32(i32 35, float %1629, float %1631)  ; FMax(a,b)
  %1640 = call float @dx.op.binary.f32(i32 35, float %1625, float %1627)  ; FMax(a,b)
  %1641 = call float @dx.op.binary.f32(i32 35, float %1640, float %1639)  ; FMax(a,b)
  %1642 = fcmp fast olt float %1641, %1514
  br label %1643

; <label>:1643                                    ; preds = %1638, %1633
  %1644 = phi i1 [ %1637, %1633 ], [ %1642, %1638 ]
  br i1 %1644, label %2932, label %1645

; <label>:1645                                    ; preds = %1643, %1529, %1511, %1177
  br label %2932

; <label>:1646                                    ; preds = %658
  %1647 = icmp eq i32 %184, 2
  br i1 %1647, label %1648, label %2139

; <label>:1648                                    ; preds = %1646
  %1649 = extractvalue %dx.types.CBufRet.i32 %79, 3
  %1650 = and i32 %1649, 31
  %1651 = lshr i32 %123, %1650
  %1652 = and i32 %1651, 15
  %1653 = icmp eq i32 %1652, 3
  br i1 %1653, label %1654, label %2932

; <label>:1654                                    ; preds = %1648
  %1655 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %1656 = extractvalue %dx.types.CBufRet.f32 %1655, 0
  %1657 = extractvalue %dx.types.CBufRet.f32 %1655, 1
  %1658 = extractvalue %dx.types.CBufRet.f32 %1655, 2
  %1659 = extractvalue %dx.types.CBufRet.f32 %1655, 3
  %1660 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1661 = extractvalue %dx.types.CBufRet.f32 %1660, 0
  %1662 = extractvalue %dx.types.CBufRet.f32 %1660, 1
  %1663 = extractvalue %dx.types.CBufRet.f32 %1660, 2
  %1664 = extractvalue %dx.types.CBufRet.f32 %1660, 3
  %1665 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %1666 = extractvalue %dx.types.CBufRet.f32 %1665, 0
  %1667 = extractvalue %dx.types.CBufRet.f32 %1665, 1
  %1668 = extractvalue %dx.types.CBufRet.f32 %1665, 2
  %1669 = extractvalue %dx.types.CBufRet.f32 %1665, 3
  %1670 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %1671 = extractvalue %dx.types.CBufRet.f32 %1670, 0
  %1672 = extractvalue %dx.types.CBufRet.f32 %1670, 1
  %1673 = extractvalue %dx.types.CBufRet.f32 %1670, 2
  %1674 = extractvalue %dx.types.CBufRet.f32 %1670, 3
  %1675 = extractvalue %dx.types.CBufRet.i32 %79, 0
  %1676 = and i32 %1675, 1
  %1677 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1678 = extractvalue %dx.types.CBufRet.i32 %1677, 0
  %1679 = icmp sgt i32 %1678, 0
  br i1 %1679, label %1680, label %2138

; <label>:1680                                    ; preds = %1654
  br label %1681

; <label>:1681                                    ; preds = %1681, %1680
  %1682 = phi i32 [ %1737, %1681 ], [ 0, %1680 ]
  %1683 = and i32 %1682, 1
  %1684 = icmp ne i32 %1683, 0
  %1685 = select i1 %1684, float %175, float %165
  %1686 = and i32 %1682, 2
  %1687 = icmp ne i32 %1686, 0
  %1688 = select i1 %1687, float %176, float %166
  %1689 = and i32 %1682, 4
  %1690 = icmp ne i32 %1689, 0
  %1691 = select i1 %1690, float %177, float %167
  %1692 = add i32 %181, %179
  %1693 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1692, i32 0)  ; BufferLoad(srv,index,wot)
  %1694 = extractvalue %dx.types.ResRet.f32 %1693, 0
  %1695 = extractvalue %dx.types.ResRet.f32 %1693, 1
  %1696 = extractvalue %dx.types.ResRet.f32 %1693, 2
  %1697 = extractvalue %dx.types.ResRet.f32 %1693, 3
  %1698 = add nuw nsw i32 %179, 1
  %1699 = add i32 %1698, %181
  %1700 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1699, i32 0)  ; BufferLoad(srv,index,wot)
  %1701 = extractvalue %dx.types.ResRet.f32 %1700, 0
  %1702 = extractvalue %dx.types.ResRet.f32 %1700, 1
  %1703 = extractvalue %dx.types.ResRet.f32 %1700, 2
  %1704 = extractvalue %dx.types.ResRet.f32 %1700, 3
  %1705 = add nuw nsw i32 %179, 2
  %1706 = add i32 %1705, %181
  %1707 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1706, i32 0)  ; BufferLoad(srv,index,wot)
  %1708 = extractvalue %dx.types.ResRet.f32 %1707, 0
  %1709 = extractvalue %dx.types.ResRet.f32 %1707, 1
  %1710 = extractvalue %dx.types.ResRet.f32 %1707, 2
  %1711 = extractvalue %dx.types.ResRet.f32 %1707, 3
  %1712 = call float @dx.op.dot4.f32(i32 56, float %1685, float %1688, float %1691, float 1.000000e+00, float %1694, float %1695, float %1696, float %1697)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1713 = call float @dx.op.dot4.f32(i32 56, float %1685, float %1688, float %1691, float 1.000000e+00, float %1701, float %1702, float %1703, float %1704)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1714 = call float @dx.op.dot4.f32(i32 56, float %1685, float %1688, float %1691, float 1.000000e+00, float %1708, float %1709, float %1710, float %1711)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1715 = fmul fast float %1712, %1656
  %1716 = call float @dx.op.tertiary.f32(i32 46, float %1713, float %1661, float %1715)  ; FMad(a,b,c)
  %1717 = call float @dx.op.tertiary.f32(i32 46, float %1714, float %1666, float %1716)  ; FMad(a,b,c)
  %1718 = fadd fast float %1717, %1671
  %1719 = fmul fast float %1712, %1657
  %1720 = call float @dx.op.tertiary.f32(i32 46, float %1713, float %1662, float %1719)  ; FMad(a,b,c)
  %1721 = call float @dx.op.tertiary.f32(i32 46, float %1714, float %1667, float %1720)  ; FMad(a,b,c)
  %1722 = fadd fast float %1721, %1672
  %1723 = fmul fast float %1712, %1658
  %1724 = call float @dx.op.tertiary.f32(i32 46, float %1713, float %1663, float %1723)  ; FMad(a,b,c)
  %1725 = call float @dx.op.tertiary.f32(i32 46, float %1714, float %1668, float %1724)  ; FMad(a,b,c)
  %1726 = fadd fast float %1725, %1673
  %1727 = fmul fast float %1712, %1659
  %1728 = call float @dx.op.tertiary.f32(i32 46, float %1713, float %1664, float %1727)  ; FMad(a,b,c)
  %1729 = call float @dx.op.tertiary.f32(i32 46, float %1714, float %1669, float %1728)  ; FMad(a,b,c)
  %1730 = fadd fast float %1729, %1674
  %1731 = fdiv fast float %1718, %1730
  %1732 = fdiv fast float %1722, %1730
  %1733 = fdiv fast float %1726, %1730
  %1734 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 %1682
  %1735 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 %1682
  %1736 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 %1682
  store float %1731, float* %1734, align 4
  store float %1732, float* %1735, align 4
  store float %1733, float* %1736, align 4
  %1737 = add nuw nsw i32 %1682, 1
  %1738 = icmp eq i32 %1737, 8
  br i1 %1738, label %1739, label %1681

; <label>:1739                                    ; preds = %1681
  %1740 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 3
  %1741 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 3
  %1742 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 3
  %1743 = load float, float* %1740, align 4
  %1744 = load float, float* %1741, align 4
  %1745 = load float, float* %1742, align 4
  %1746 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 2
  %1747 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 2
  %1748 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 2
  %1749 = load float, float* %1746, align 4
  %1750 = load float, float* %1747, align 4
  %1751 = load float, float* %1748, align 4
  %1752 = call float @dx.op.binary.f32(i32 36, float %1749, float %1743)  ; FMin(a,b)
  %1753 = call float @dx.op.binary.f32(i32 36, float %1750, float %1744)  ; FMin(a,b)
  %1754 = call float @dx.op.binary.f32(i32 36, float %1751, float %1745)  ; FMin(a,b)
  %1755 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 1
  %1756 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 1
  %1757 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 1
  %1758 = load float, float* %1755, align 4
  %1759 = load float, float* %1756, align 4
  %1760 = load float, float* %1757, align 4
  %1761 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 0
  %1762 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 0
  %1763 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 0
  %1764 = load float, float* %1761, align 4
  %1765 = load float, float* %1762, align 4
  %1766 = load float, float* %1763, align 4
  %1767 = call float @dx.op.binary.f32(i32 36, float %1764, float %1758)  ; FMin(a,b)
  %1768 = call float @dx.op.binary.f32(i32 36, float %1765, float %1759)  ; FMin(a,b)
  %1769 = call float @dx.op.binary.f32(i32 36, float %1766, float %1760)  ; FMin(a,b)
  %1770 = call float @dx.op.binary.f32(i32 36, float %1767, float %1752)  ; FMin(a,b)
  %1771 = call float @dx.op.binary.f32(i32 36, float %1768, float %1753)  ; FMin(a,b)
  %1772 = call float @dx.op.binary.f32(i32 36, float %1769, float %1754)  ; FMin(a,b)
  %1773 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 7
  %1774 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 7
  %1775 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 7
  %1776 = load float, float* %1773, align 4
  %1777 = load float, float* %1774, align 4
  %1778 = load float, float* %1775, align 4
  %1779 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 6
  %1780 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 6
  %1781 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 6
  %1782 = load float, float* %1779, align 4
  %1783 = load float, float* %1780, align 4
  %1784 = load float, float* %1781, align 4
  %1785 = call float @dx.op.binary.f32(i32 36, float %1782, float %1776)  ; FMin(a,b)
  %1786 = call float @dx.op.binary.f32(i32 36, float %1783, float %1777)  ; FMin(a,b)
  %1787 = call float @dx.op.binary.f32(i32 36, float %1784, float %1778)  ; FMin(a,b)
  %1788 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 5
  %1789 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 5
  %1790 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 5
  %1791 = load float, float* %1788, align 4
  %1792 = load float, float* %1789, align 4
  %1793 = load float, float* %1790, align 4
  %1794 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 4
  %1795 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 4
  %1796 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 4
  %1797 = load float, float* %1794, align 4
  %1798 = load float, float* %1795, align 4
  %1799 = load float, float* %1796, align 4
  %1800 = call float @dx.op.binary.f32(i32 36, float %1797, float %1791)  ; FMin(a,b)
  %1801 = call float @dx.op.binary.f32(i32 36, float %1798, float %1792)  ; FMin(a,b)
  %1802 = call float @dx.op.binary.f32(i32 36, float %1799, float %1793)  ; FMin(a,b)
  %1803 = call float @dx.op.binary.f32(i32 36, float %1800, float %1785)  ; FMin(a,b)
  %1804 = call float @dx.op.binary.f32(i32 36, float %1801, float %1786)  ; FMin(a,b)
  %1805 = call float @dx.op.binary.f32(i32 36, float %1802, float %1787)  ; FMin(a,b)
  %1806 = call float @dx.op.binary.f32(i32 36, float %1770, float %1803)  ; FMin(a,b)
  %1807 = call float @dx.op.binary.f32(i32 36, float %1771, float %1804)  ; FMin(a,b)
  %1808 = call float @dx.op.binary.f32(i32 36, float %1772, float %1805)  ; FMin(a,b)
  %1809 = call float @dx.op.binary.f32(i32 35, float %1749, float %1743)  ; FMax(a,b)
  %1810 = call float @dx.op.binary.f32(i32 35, float %1750, float %1744)  ; FMax(a,b)
  %1811 = call float @dx.op.binary.f32(i32 35, float %1751, float %1745)  ; FMax(a,b)
  %1812 = call float @dx.op.binary.f32(i32 35, float %1764, float %1758)  ; FMax(a,b)
  %1813 = call float @dx.op.binary.f32(i32 35, float %1765, float %1759)  ; FMax(a,b)
  %1814 = call float @dx.op.binary.f32(i32 35, float %1766, float %1760)  ; FMax(a,b)
  %1815 = call float @dx.op.binary.f32(i32 35, float %1812, float %1809)  ; FMax(a,b)
  %1816 = call float @dx.op.binary.f32(i32 35, float %1813, float %1810)  ; FMax(a,b)
  %1817 = call float @dx.op.binary.f32(i32 35, float %1814, float %1811)  ; FMax(a,b)
  %1818 = call float @dx.op.binary.f32(i32 35, float %1782, float %1776)  ; FMax(a,b)
  %1819 = call float @dx.op.binary.f32(i32 35, float %1783, float %1777)  ; FMax(a,b)
  %1820 = call float @dx.op.binary.f32(i32 35, float %1784, float %1778)  ; FMax(a,b)
  %1821 = call float @dx.op.binary.f32(i32 35, float %1797, float %1791)  ; FMax(a,b)
  %1822 = call float @dx.op.binary.f32(i32 35, float %1798, float %1792)  ; FMax(a,b)
  %1823 = call float @dx.op.binary.f32(i32 35, float %1799, float %1793)  ; FMax(a,b)
  %1824 = call float @dx.op.binary.f32(i32 35, float %1821, float %1818)  ; FMax(a,b)
  %1825 = call float @dx.op.binary.f32(i32 35, float %1822, float %1819)  ; FMax(a,b)
  %1826 = call float @dx.op.binary.f32(i32 35, float %1823, float %1820)  ; FMax(a,b)
  %1827 = call float @dx.op.binary.f32(i32 35, float %1815, float %1824)  ; FMax(a,b)
  %1828 = call float @dx.op.binary.f32(i32 35, float %1816, float %1825)  ; FMax(a,b)
  %1829 = call float @dx.op.binary.f32(i32 35, float %1817, float %1826)  ; FMax(a,b)
  %1830 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1831 = extractvalue %dx.types.CBufRet.i32 %1830, 1
  %1832 = icmp sgt i32 %1831, 0
  br i1 %1832, label %1833, label %2004

; <label>:1833                                    ; preds = %1739
  %1834 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1835 = extractvalue %dx.types.CBufRet.f32 %1834, 0
  %1836 = extractvalue %dx.types.CBufRet.f32 %1834, 1
  %1837 = extractvalue %dx.types.CBufRet.f32 %1834, 2
  %1838 = extractvalue %dx.types.CBufRet.f32 %1834, 3
  %1839 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %1840 = extractvalue %dx.types.CBufRet.f32 %1839, 0
  %1841 = extractvalue %dx.types.CBufRet.f32 %1839, 1
  %1842 = extractvalue %dx.types.CBufRet.f32 %1839, 2
  %1843 = extractvalue %dx.types.CBufRet.f32 %1839, 3
  %1844 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1845 = extractvalue %dx.types.CBufRet.f32 %1844, 0
  %1846 = extractvalue %dx.types.CBufRet.f32 %1844, 1
  %1847 = extractvalue %dx.types.CBufRet.f32 %1844, 2
  %1848 = extractvalue %dx.types.CBufRet.f32 %1844, 3
  %1849 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1850 = extractvalue %dx.types.CBufRet.f32 %1849, 0
  %1851 = extractvalue %dx.types.CBufRet.f32 %1849, 1
  %1852 = extractvalue %dx.types.CBufRet.f32 %1849, 2
  %1853 = extractvalue %dx.types.CBufRet.f32 %1849, 3
  br label %1854

; <label>:1854                                    ; preds = %1854, %1833
  %1855 = phi i32 [ 0, %1833 ], [ %1905, %1854 ]
  %1856 = and i32 %1855, 1
  %1857 = icmp ne i32 %1856, 0
  %1858 = select i1 %1857, float %175, float %165
  %1859 = and i32 %1855, 2
  %1860 = icmp ne i32 %1859, 0
  %1861 = select i1 %1860, float %176, float %166
  %1862 = and i32 %1855, 4
  %1863 = icmp ne i32 %1862, 0
  %1864 = select i1 %1863, float %177, float %167
  %1865 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1692, i32 0)  ; BufferLoad(srv,index,wot)
  %1866 = extractvalue %dx.types.ResRet.f32 %1865, 0
  %1867 = extractvalue %dx.types.ResRet.f32 %1865, 1
  %1868 = extractvalue %dx.types.ResRet.f32 %1865, 2
  %1869 = extractvalue %dx.types.ResRet.f32 %1865, 3
  %1870 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1699, i32 0)  ; BufferLoad(srv,index,wot)
  %1871 = extractvalue %dx.types.ResRet.f32 %1870, 0
  %1872 = extractvalue %dx.types.ResRet.f32 %1870, 1
  %1873 = extractvalue %dx.types.ResRet.f32 %1870, 2
  %1874 = extractvalue %dx.types.ResRet.f32 %1870, 3
  %1875 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1706, i32 0)  ; BufferLoad(srv,index,wot)
  %1876 = extractvalue %dx.types.ResRet.f32 %1875, 0
  %1877 = extractvalue %dx.types.ResRet.f32 %1875, 1
  %1878 = extractvalue %dx.types.ResRet.f32 %1875, 2
  %1879 = extractvalue %dx.types.ResRet.f32 %1875, 3
  %1880 = call float @dx.op.dot4.f32(i32 56, float %1858, float %1861, float %1864, float 1.000000e+00, float %1866, float %1867, float %1868, float %1869)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1881 = call float @dx.op.dot4.f32(i32 56, float %1858, float %1861, float %1864, float 1.000000e+00, float %1871, float %1872, float %1873, float %1874)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1882 = call float @dx.op.dot4.f32(i32 56, float %1858, float %1861, float %1864, float 1.000000e+00, float %1876, float %1877, float %1878, float %1879)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1883 = fmul fast float %1880, %1835
  %1884 = call float @dx.op.tertiary.f32(i32 46, float %1881, float %1840, float %1883)  ; FMad(a,b,c)
  %1885 = call float @dx.op.tertiary.f32(i32 46, float %1882, float %1845, float %1884)  ; FMad(a,b,c)
  %1886 = fadd fast float %1885, %1850
  %1887 = fmul fast float %1880, %1836
  %1888 = call float @dx.op.tertiary.f32(i32 46, float %1881, float %1841, float %1887)  ; FMad(a,b,c)
  %1889 = call float @dx.op.tertiary.f32(i32 46, float %1882, float %1846, float %1888)  ; FMad(a,b,c)
  %1890 = fadd fast float %1889, %1851
  %1891 = fmul fast float %1880, %1837
  %1892 = call float @dx.op.tertiary.f32(i32 46, float %1881, float %1842, float %1891)  ; FMad(a,b,c)
  %1893 = call float @dx.op.tertiary.f32(i32 46, float %1882, float %1847, float %1892)  ; FMad(a,b,c)
  %1894 = fadd fast float %1893, %1852
  %1895 = fmul fast float %1880, %1838
  %1896 = call float @dx.op.tertiary.f32(i32 46, float %1881, float %1843, float %1895)  ; FMad(a,b,c)
  %1897 = call float @dx.op.tertiary.f32(i32 46, float %1882, float %1848, float %1896)  ; FMad(a,b,c)
  %1898 = fadd fast float %1897, %1853
  %1899 = fdiv fast float %1886, %1898
  %1900 = fdiv fast float %1890, %1898
  %1901 = fdiv fast float %1894, %1898
  %1902 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 %1855
  %1903 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 %1855
  %1904 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 %1855
  store float %1899, float* %1902, align 4
  store float %1900, float* %1903, align 4
  store float %1901, float* %1904, align 4
  %1905 = add nuw nsw i32 %1855, 1
  %1906 = icmp eq i32 %1905, 8
  br i1 %1906, label %1907, label %1854

; <label>:1907                                    ; preds = %1854
  %1908 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 3
  %1909 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 3
  %1910 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 3
  %1911 = load float, float* %1908, align 4
  %1912 = load float, float* %1909, align 4
  %1913 = load float, float* %1910, align 4
  %1914 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 2
  %1915 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 2
  %1916 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 2
  %1917 = load float, float* %1914, align 4
  %1918 = load float, float* %1915, align 4
  %1919 = load float, float* %1916, align 4
  %1920 = call float @dx.op.binary.f32(i32 36, float %1917, float %1911)  ; FMin(a,b)
  %1921 = call float @dx.op.binary.f32(i32 36, float %1918, float %1912)  ; FMin(a,b)
  %1922 = call float @dx.op.binary.f32(i32 36, float %1919, float %1913)  ; FMin(a,b)
  %1923 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 1
  %1924 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 1
  %1925 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 1
  %1926 = load float, float* %1923, align 4
  %1927 = load float, float* %1924, align 4
  %1928 = load float, float* %1925, align 4
  %1929 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 0
  %1930 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 0
  %1931 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 0
  %1932 = load float, float* %1929, align 4
  %1933 = load float, float* %1930, align 4
  %1934 = load float, float* %1931, align 4
  %1935 = call float @dx.op.binary.f32(i32 36, float %1932, float %1926)  ; FMin(a,b)
  %1936 = call float @dx.op.binary.f32(i32 36, float %1933, float %1927)  ; FMin(a,b)
  %1937 = call float @dx.op.binary.f32(i32 36, float %1934, float %1928)  ; FMin(a,b)
  %1938 = call float @dx.op.binary.f32(i32 36, float %1935, float %1920)  ; FMin(a,b)
  %1939 = call float @dx.op.binary.f32(i32 36, float %1936, float %1921)  ; FMin(a,b)
  %1940 = call float @dx.op.binary.f32(i32 36, float %1937, float %1922)  ; FMin(a,b)
  %1941 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 7
  %1942 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 7
  %1943 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 7
  %1944 = load float, float* %1941, align 4
  %1945 = load float, float* %1942, align 4
  %1946 = load float, float* %1943, align 4
  %1947 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 6
  %1948 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 6
  %1949 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 6
  %1950 = load float, float* %1947, align 4
  %1951 = load float, float* %1948, align 4
  %1952 = load float, float* %1949, align 4
  %1953 = call float @dx.op.binary.f32(i32 36, float %1950, float %1944)  ; FMin(a,b)
  %1954 = call float @dx.op.binary.f32(i32 36, float %1951, float %1945)  ; FMin(a,b)
  %1955 = call float @dx.op.binary.f32(i32 36, float %1952, float %1946)  ; FMin(a,b)
  %1956 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 5
  %1957 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 5
  %1958 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 5
  %1959 = load float, float* %1956, align 4
  %1960 = load float, float* %1957, align 4
  %1961 = load float, float* %1958, align 4
  %1962 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 4
  %1963 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 4
  %1964 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 4
  %1965 = load float, float* %1962, align 4
  %1966 = load float, float* %1963, align 4
  %1967 = load float, float* %1964, align 4
  %1968 = call float @dx.op.binary.f32(i32 36, float %1965, float %1959)  ; FMin(a,b)
  %1969 = call float @dx.op.binary.f32(i32 36, float %1966, float %1960)  ; FMin(a,b)
  %1970 = call float @dx.op.binary.f32(i32 36, float %1967, float %1961)  ; FMin(a,b)
  %1971 = call float @dx.op.binary.f32(i32 36, float %1968, float %1953)  ; FMin(a,b)
  %1972 = call float @dx.op.binary.f32(i32 36, float %1969, float %1954)  ; FMin(a,b)
  %1973 = call float @dx.op.binary.f32(i32 36, float %1970, float %1955)  ; FMin(a,b)
  %1974 = call float @dx.op.binary.f32(i32 36, float %1938, float %1971)  ; FMin(a,b)
  %1975 = call float @dx.op.binary.f32(i32 36, float %1939, float %1972)  ; FMin(a,b)
  %1976 = call float @dx.op.binary.f32(i32 36, float %1940, float %1973)  ; FMin(a,b)
  %1977 = call float @dx.op.binary.f32(i32 35, float %1917, float %1911)  ; FMax(a,b)
  %1978 = call float @dx.op.binary.f32(i32 35, float %1918, float %1912)  ; FMax(a,b)
  %1979 = call float @dx.op.binary.f32(i32 35, float %1919, float %1913)  ; FMax(a,b)
  %1980 = call float @dx.op.binary.f32(i32 35, float %1932, float %1926)  ; FMax(a,b)
  %1981 = call float @dx.op.binary.f32(i32 35, float %1933, float %1927)  ; FMax(a,b)
  %1982 = call float @dx.op.binary.f32(i32 35, float %1934, float %1928)  ; FMax(a,b)
  %1983 = call float @dx.op.binary.f32(i32 35, float %1980, float %1977)  ; FMax(a,b)
  %1984 = call float @dx.op.binary.f32(i32 35, float %1981, float %1978)  ; FMax(a,b)
  %1985 = call float @dx.op.binary.f32(i32 35, float %1982, float %1979)  ; FMax(a,b)
  %1986 = call float @dx.op.binary.f32(i32 35, float %1950, float %1944)  ; FMax(a,b)
  %1987 = call float @dx.op.binary.f32(i32 35, float %1951, float %1945)  ; FMax(a,b)
  %1988 = call float @dx.op.binary.f32(i32 35, float %1952, float %1946)  ; FMax(a,b)
  %1989 = call float @dx.op.binary.f32(i32 35, float %1965, float %1959)  ; FMax(a,b)
  %1990 = call float @dx.op.binary.f32(i32 35, float %1966, float %1960)  ; FMax(a,b)
  %1991 = call float @dx.op.binary.f32(i32 35, float %1967, float %1961)  ; FMax(a,b)
  %1992 = call float @dx.op.binary.f32(i32 35, float %1989, float %1986)  ; FMax(a,b)
  %1993 = call float @dx.op.binary.f32(i32 35, float %1990, float %1987)  ; FMax(a,b)
  %1994 = call float @dx.op.binary.f32(i32 35, float %1991, float %1988)  ; FMax(a,b)
  %1995 = call float @dx.op.binary.f32(i32 35, float %1983, float %1992)  ; FMax(a,b)
  %1996 = call float @dx.op.binary.f32(i32 35, float %1984, float %1993)  ; FMax(a,b)
  %1997 = call float @dx.op.binary.f32(i32 35, float %1985, float %1994)  ; FMax(a,b)
  %1998 = call float @dx.op.binary.f32(i32 36, float %1806, float %1974)  ; FMin(a,b)
  %1999 = call float @dx.op.binary.f32(i32 36, float %1807, float %1975)  ; FMin(a,b)
  %2000 = call float @dx.op.binary.f32(i32 36, float %1808, float %1976)  ; FMin(a,b)
  %2001 = call float @dx.op.binary.f32(i32 35, float %1827, float %1995)  ; FMax(a,b)
  %2002 = call float @dx.op.binary.f32(i32 35, float %1828, float %1996)  ; FMax(a,b)
  %2003 = call float @dx.op.binary.f32(i32 35, float %1829, float %1997)  ; FMax(a,b)
  br label %2004

; <label>:2004                                    ; preds = %1907, %1739
  %2005 = phi float [ %1998, %1907 ], [ %1806, %1739 ]
  %2006 = phi float [ %1999, %1907 ], [ %1807, %1739 ]
  %2007 = phi float [ %2000, %1907 ], [ %1808, %1739 ]
  %2008 = phi float [ %2001, %1907 ], [ %1827, %1739 ]
  %2009 = phi float [ %2002, %1907 ], [ %1828, %1739 ]
  %2010 = phi float [ %2003, %1907 ], [ %1829, %1739 ]
  %2011 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %2012 = extractvalue %dx.types.CBufRet.f32 %2011, 0
  %2013 = extractvalue %dx.types.CBufRet.f32 %2011, 1
  %2014 = extractvalue %dx.types.CBufRet.f32 %2011, 2
  %2015 = extractvalue %dx.types.CBufRet.f32 %2011, 3
  %2016 = fcmp fast olt float %2007, 0.000000e+00
  %2017 = fcmp fast olt float %2010, 0.000000e+00
  %2018 = or i1 %2016, %2017
  br i1 %2018, label %2138, label %2019

; <label>:2019                                    ; preds = %2004
  %2020 = extractvalue %dx.types.CBufRet.i32 %1677, 1
  %2021 = icmp sgt i32 %2020, 0
  br i1 %2021, label %2022, label %2032

; <label>:2022                                    ; preds = %2019
  %2023 = fsub fast float %2008, %2005
  %2024 = fsub fast float %2009, %2006
  %2025 = call float @dx.op.unary.f32(i32 6, float %2023)  ; FAbs(value)
  %2026 = call float @dx.op.unary.f32(i32 6, float %2024)  ; FAbs(value)
  %2027 = fmul fast float %2025, 2.500000e-01
  %2028 = fmul fast float %2027, %2026
  %2029 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %2030 = extractvalue %dx.types.CBufRet.f32 %2029, 2
  %2031 = fcmp fast ogt float %2028, %2030
  br i1 %2031, label %2138, label %2032

; <label>:2032                                    ; preds = %2022, %2019
  %2033 = fmul fast float %2005, 5.000000e-01
  %2034 = fmul fast float %2009, 5.000000e-01
  %2035 = fadd fast float %2033, 5.000000e-01
  %2036 = fsub fast float 5.000000e-01, %2034
  %2037 = call float @dx.op.unary.f32(i32 7, float %2035)  ; Saturate(value)
  %2038 = call float @dx.op.unary.f32(i32 7, float %2036)  ; Saturate(value)
  %2039 = fmul fast float %2008, 5.000000e-01
  %2040 = fmul fast float %2006, 5.000000e-01
  %2041 = fadd fast float %2039, 5.000000e-01
  %2042 = fsub fast float 5.000000e-01, %2040
  %2043 = call float @dx.op.unary.f32(i32 7, float %2041)  ; Saturate(value)
  %2044 = call float @dx.op.unary.f32(i32 7, float %2042)  ; Saturate(value)
  %2045 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %14, i32 0)  ; GetDimensions(handle,mipLevel)
  %2046 = extractvalue %dx.types.Dimensions %2045, 0
  %2047 = sitofp i32 %2046 to float
  %2048 = extractvalue %dx.types.Dimensions %2045, 1
  %2049 = sitofp i32 %2048 to float
  %2050 = fmul fast float %2047, %2012
  %2051 = fmul fast float %2049, %2013
  %2052 = fsub fast float %2043, %2037
  %2053 = fsub fast float %2044, %2038
  %2054 = fmul fast float %2050, %2052
  %2055 = fmul fast float %2051, %2053
  %2056 = call float @dx.op.binary.f32(i32 35, float %2054, float %2055)  ; FMax(a,b)
  %2057 = call float @dx.op.unary.f32(i32 23, float %2056)  ; Log(value)
  %2058 = call float @dx.op.unary.f32(i32 28, float %2057)  ; Round_pi(value)
  %2059 = call float @dx.op.binary.f32(i32 36, float %2050, float %2051)  ; FMin(a,b)
  %2060 = call float @dx.op.unary.f32(i32 23, float %2059)  ; Log(value)
  %2061 = call float @dx.op.unary.f32(i32 28, float %2060)  ; Round_pi(value)
  %2062 = fadd fast float %2061, -1.000000e+00
  %2063 = call float @dx.op.binary.f32(i32 36, float %2058, float %2062)  ; FMin(a,b)
  %2064 = fadd fast float %2063, -1.000000e+00
  %2065 = call float @dx.op.binary.f32(i32 35, float %2064, float 0.000000e+00)  ; FMax(a,b)
  %2066 = fptoui float %2050 to i32
  %2067 = fptoui float %2065 to i32
  %2068 = and i32 %2067, 31
  %2069 = lshr i32 %2066, %2068
  %2070 = uitofp i32 %2069 to float
  %2071 = fptoui float %2051 to i32
  %2072 = lshr i32 %2071, %2068
  %2073 = uitofp i32 %2072 to float
  %2074 = fmul fast float %2070, %2043
  %2075 = fmul fast float %2073, %2044
  %2076 = call float @dx.op.unary.f32(i32 28, float %2074)  ; Round_pi(value)
  %2077 = call float @dx.op.unary.f32(i32 28, float %2075)  ; Round_pi(value)
  %2078 = fmul fast float %2070, %2037
  %2079 = fmul fast float %2073, %2038
  %2080 = call float @dx.op.unary.f32(i32 27, float %2078)  ; Round_ni(value)
  %2081 = call float @dx.op.unary.f32(i32 27, float %2079)  ; Round_ni(value)
  %2082 = fsub fast float %2076, %2080
  %2083 = fsub fast float %2077, %2081
  %2084 = fcmp fast ole float %2082, 2.000000e+00
  %2085 = fcmp fast ole float %2083, 2.000000e+00
  %2086 = and i1 %2084, %2085
  %2087 = fadd fast float %2065, 1.000000e+00
  %2088 = select i1 %2086, float %2065, float %2087
  %2089 = fmul fast float %2037, %2012
  %2090 = fmul fast float %2038, %2013
  %2091 = fadd fast float %2089, %2014
  %2092 = fadd fast float %2090, %2015
  %2093 = fmul fast float %2043, %2012
  %2094 = fmul fast float %2044, %2013
  %2095 = fadd fast float %2093, %2014
  %2096 = fadd fast float %2094, %2015
  %2097 = fptoui float %2047 to i32
  %2098 = fptoui float %2088 to i32
  %2099 = and i32 %2098, 31
  %2100 = lshr i32 %2097, %2099
  %2101 = uitofp i32 %2100 to float
  %2102 = fptoui float %2049 to i32
  %2103 = lshr i32 %2102, %2099
  %2104 = uitofp i32 %2103 to float
  %2105 = call float @dx.op.binary.f32(i32 35, float %2101, float 1.000000e+00)  ; FMax(a,b)
  %2106 = call float @dx.op.binary.f32(i32 35, float %2104, float 1.000000e+00)  ; FMax(a,b)
  %2107 = fadd fast float %2105, -5.000000e-01
  %2108 = fadd fast float %2106, -5.000000e-01
  %2109 = fdiv fast float %2107, %2105
  %2110 = fdiv fast float %2108, %2106
  %2111 = fmul fast float %2109, %2012
  %2112 = fmul fast float %2110, %2013
  %2113 = call float @dx.op.binary.f32(i32 36, float %2111, float %2091)  ; FMin(a,b)
  %2114 = call float @dx.op.binary.f32(i32 36, float %2112, float %2092)  ; FMin(a,b)
  %2115 = call float @dx.op.binary.f32(i32 36, float %2111, float %2095)  ; FMin(a,b)
  %2116 = call float @dx.op.binary.f32(i32 36, float %2112, float %2096)  ; FMin(a,b)
  %2117 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %2113, float %2114, float undef, float undef, i32 0, i32 0, i32 undef, float %2088)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2118 = extractvalue %dx.types.ResRet.f32 %2117, 0
  %2119 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %2113, float %2116, float undef, float undef, i32 0, i32 0, i32 undef, float %2088)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2120 = extractvalue %dx.types.ResRet.f32 %2119, 0
  %2121 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %2115, float %2114, float undef, float undef, i32 0, i32 0, i32 undef, float %2088)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2122 = extractvalue %dx.types.ResRet.f32 %2121, 0
  %2123 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %2115, float %2116, float undef, float undef, i32 0, i32 0, i32 undef, float %2088)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %2124 = extractvalue %dx.types.ResRet.f32 %2123, 0
  %2125 = icmp eq i32 %1676, 0
  br i1 %2125, label %2131, label %2126

; <label>:2126                                    ; preds = %2032
  %2127 = call float @dx.op.binary.f32(i32 36, float %2122, float %2124)  ; FMin(a,b)
  %2128 = call float @dx.op.binary.f32(i32 36, float %2118, float %2120)  ; FMin(a,b)
  %2129 = call float @dx.op.binary.f32(i32 36, float %2128, float %2127)  ; FMin(a,b)
  %2130 = fcmp fast olt float %2010, %2129
  br label %2136

; <label>:2131                                    ; preds = %2032
  %2132 = call float @dx.op.binary.f32(i32 35, float %2122, float %2124)  ; FMax(a,b)
  %2133 = call float @dx.op.binary.f32(i32 35, float %2118, float %2120)  ; FMax(a,b)
  %2134 = call float @dx.op.binary.f32(i32 35, float %2133, float %2132)  ; FMax(a,b)
  %2135 = fcmp fast olt float %2134, %2007
  br label %2136

; <label>:2136                                    ; preds = %2131, %2126
  %2137 = phi i1 [ %2130, %2126 ], [ %2135, %2131 ]
  br i1 %2137, label %2932, label %2138

; <label>:2138                                    ; preds = %2136, %2022, %2004, %1654
  br label %2932

; <label>:2139                                    ; preds = %1646
  %2140 = icmp eq i32 %184, 3
  br i1 %2140, label %2141, label %2613

; <label>:2141                                    ; preds = %2139
  %2142 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2143 = extractvalue %dx.types.CBufRet.f32 %2142, 0
  %2144 = extractvalue %dx.types.CBufRet.f32 %2142, 1
  %2145 = extractvalue %dx.types.CBufRet.f32 %2142, 3
  %2146 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %2147 = extractvalue %dx.types.CBufRet.f32 %2146, 0
  %2148 = extractvalue %dx.types.CBufRet.f32 %2146, 1
  %2149 = extractvalue %dx.types.CBufRet.f32 %2146, 3
  %2150 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %2151 = extractvalue %dx.types.CBufRet.f32 %2150, 0
  %2152 = extractvalue %dx.types.CBufRet.f32 %2150, 1
  %2153 = extractvalue %dx.types.CBufRet.f32 %2150, 3
  %2154 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %2155 = extractvalue %dx.types.CBufRet.f32 %2154, 0
  %2156 = extractvalue %dx.types.CBufRet.f32 %2154, 1
  %2157 = extractvalue %dx.types.CBufRet.f32 %2154, 3
  %2158 = add i32 %181, %179
  %2159 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2158, i32 0)  ; BufferLoad(srv,index,wot)
  %2160 = extractvalue %dx.types.ResRet.f32 %2159, 0
  %2161 = extractvalue %dx.types.ResRet.f32 %2159, 1
  %2162 = extractvalue %dx.types.ResRet.f32 %2159, 2
  %2163 = extractvalue %dx.types.ResRet.f32 %2159, 3
  %2164 = add nuw nsw i32 %179, 1
  %2165 = add i32 %2164, %181
  %2166 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2165, i32 0)  ; BufferLoad(srv,index,wot)
  %2167 = extractvalue %dx.types.ResRet.f32 %2166, 0
  %2168 = extractvalue %dx.types.ResRet.f32 %2166, 1
  %2169 = extractvalue %dx.types.ResRet.f32 %2166, 2
  %2170 = extractvalue %dx.types.ResRet.f32 %2166, 3
  %2171 = add nuw nsw i32 %179, 2
  %2172 = add i32 %2171, %181
  %2173 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2172, i32 0)  ; BufferLoad(srv,index,wot)
  %2174 = extractvalue %dx.types.ResRet.f32 %2173, 0
  %2175 = extractvalue %dx.types.ResRet.f32 %2173, 1
  %2176 = extractvalue %dx.types.ResRet.f32 %2173, 2
  %2177 = extractvalue %dx.types.ResRet.f32 %2173, 3
  %2178 = call float @dx.op.dot3.f32(i32 55, float %2174, float %2175, float %2176, float %2174, float %2175, float %2176)  ; Dot3(ax,ay,az,bx,by,bz)
  %2179 = call float @dx.op.dot3.f32(i32 55, float %2167, float %2168, float %2169, float %2167, float %2168, float %2169)  ; Dot3(ax,ay,az,bx,by,bz)
  %2180 = call float @dx.op.binary.f32(i32 35, float %2179, float %2178)  ; FMax(a,b)
  %2181 = call float @dx.op.dot3.f32(i32 55, float %2160, float %2161, float %2162, float %2160, float %2161, float %2162)  ; Dot3(ax,ay,az,bx,by,bz)
  %2182 = call float @dx.op.binary.f32(i32 35, float %2181, float %2180)  ; FMax(a,b)
  %2183 = call float @dx.op.unary.f32(i32 24, float %2182)  ; Sqrt(value)
  %2184 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2160, float %2161, float %2162, float %2163)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2185 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2167, float %2168, float %2169, float %2170)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2186 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2174, float %2175, float %2176, float %2177)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2187 = fmul fast float %2183, %143
  %2188 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %2189 = extractvalue %dx.types.CBufRet.f32 %2188, 0
  %2190 = extractvalue %dx.types.CBufRet.f32 %2188, 1
  %2191 = extractvalue %dx.types.CBufRet.f32 %2188, 2
  %2192 = extractvalue %dx.types.CBufRet.f32 %2188, 3
  %2193 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %2189, float %2190, float %2191, float %2192)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2194 = fsub fast float -0.000000e+00, %2187
  %2195 = fcmp fast olt float %2193, %2194
  br i1 %2195, label %2932, label %2196

; <label>:2196                                    ; preds = %2141
  %2197 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %2198 = extractvalue %dx.types.CBufRet.f32 %2197, 0
  %2199 = extractvalue %dx.types.CBufRet.f32 %2197, 1
  %2200 = extractvalue %dx.types.CBufRet.f32 %2197, 2
  %2201 = extractvalue %dx.types.CBufRet.f32 %2197, 3
  %2202 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %2198, float %2199, float %2200, float %2201)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2203 = fcmp fast olt float %2202, %2194
  br i1 %2203, label %2932, label %3082

; <label>:2204                                    ; preds = %3114
  %2205 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2160, float %2161, float %2162)  ; Dot3(ax,ay,az,bx,by,bz)
  %2206 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2167, float %2168, float %2169)  ; Dot3(ax,ay,az,bx,by,bz)
  %2207 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2174, float %2175, float %2176)  ; Dot3(ax,ay,az,bx,by,bz)
  %2208 = call float @dx.op.dot3.f32(i32 55, float %2205, float %2206, float %2207, float %2205, float %2206, float %2207)  ; Dot3(ax,ay,az,bx,by,bz)
  %2209 = call float @dx.op.unary.f32(i32 25, float %2208)  ; Rsqrt(value)
  %2210 = fmul fast float %2209, %2205
  %2211 = fmul fast float %2209, %2206
  %2212 = fmul fast float %2209, %2207
  %2213 = icmp eq i32 %85, 2
  %2214 = fsub fast float -0.000000e+00, %2210
  %2215 = fsub fast float -0.000000e+00, %2211
  %2216 = fsub fast float -0.000000e+00, %2212
  %2217 = select i1 %2213, float %2214, float %2210
  %2218 = select i1 %2213, float %2215, float %2211
  %2219 = select i1 %2213, float %2216, float %2212
  %2220 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2221 = extractvalue %dx.types.CBufRet.f32 %2220, 0
  %2222 = extractvalue %dx.types.CBufRet.f32 %2220, 1
  %2223 = extractvalue %dx.types.CBufRet.f32 %2220, 2
  %2224 = fmul fast float %2183, %164
  %2225 = fmul fast float %2224, %2217
  %2226 = fmul fast float %2224, %2218
  %2227 = fmul fast float %2224, %2219
  %2228 = fsub fast float %2184, %2225
  %2229 = fsub fast float %2185, %2226
  %2230 = fsub fast float %2186, %2227
  %2231 = fsub fast float %2221, %2228
  %2232 = fsub fast float %2222, %2229
  %2233 = fsub fast float %2223, %2230
  %2234 = call float @dx.op.dot3.f32(i32 55, float %2231, float %2232, float %2233, float %2231, float %2232, float %2233)  ; Dot3(ax,ay,az,bx,by,bz)
  %2235 = call float @dx.op.unary.f32(i32 25, float %2234)  ; Rsqrt(value)
  %2236 = fmul fast float %2231, %2235
  %2237 = fmul fast float %2232, %2235
  %2238 = fmul fast float %2233, %2235
  %2239 = fsub fast float -0.000000e+00, %2217
  %2240 = fsub fast float -0.000000e+00, %2218
  %2241 = fsub fast float -0.000000e+00, %2219
  %2242 = call float @dx.op.dot3.f32(i32 55, float %2236, float %2237, float %2238, float %2239, float %2240, float %2241)  ; Dot3(ax,ay,az,bx,by,bz)
  %2243 = fcmp fast ogt float %2242, %155
  br i1 %2243, label %2932, label %2244

; <label>:2244                                    ; preds = %3114, %2204
  %2245 = and i32 %80, 512
  %2246 = icmp eq i32 %2245, 0
  br i1 %2246, label %2456, label %2247

; <label>:2247                                    ; preds = %2244
  br label %2248

; <label>:2248                                    ; preds = %2248, %2247
  %2249 = phi i32 [ %2293, %2248 ], [ 0, %2247 ]
  %2250 = and i32 %2249, 1
  %2251 = icmp ne i32 %2250, 0
  %2252 = select i1 %2251, float %175, float %165
  %2253 = and i32 %2249, 2
  %2254 = icmp ne i32 %2253, 0
  %2255 = select i1 %2254, float %176, float %166
  %2256 = and i32 %2249, 4
  %2257 = icmp ne i32 %2256, 0
  %2258 = select i1 %2257, float %177, float %167
  %2259 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2158, i32 0)  ; BufferLoad(srv,index,wot)
  %2260 = extractvalue %dx.types.ResRet.f32 %2259, 0
  %2261 = extractvalue %dx.types.ResRet.f32 %2259, 1
  %2262 = extractvalue %dx.types.ResRet.f32 %2259, 2
  %2263 = extractvalue %dx.types.ResRet.f32 %2259, 3
  %2264 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2165, i32 0)  ; BufferLoad(srv,index,wot)
  %2265 = extractvalue %dx.types.ResRet.f32 %2264, 0
  %2266 = extractvalue %dx.types.ResRet.f32 %2264, 1
  %2267 = extractvalue %dx.types.ResRet.f32 %2264, 2
  %2268 = extractvalue %dx.types.ResRet.f32 %2264, 3
  %2269 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2172, i32 0)  ; BufferLoad(srv,index,wot)
  %2270 = extractvalue %dx.types.ResRet.f32 %2269, 0
  %2271 = extractvalue %dx.types.ResRet.f32 %2269, 1
  %2272 = extractvalue %dx.types.ResRet.f32 %2269, 2
  %2273 = extractvalue %dx.types.ResRet.f32 %2269, 3
  %2274 = call float @dx.op.dot4.f32(i32 56, float %2252, float %2255, float %2258, float 1.000000e+00, float %2260, float %2261, float %2262, float %2263)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2275 = call float @dx.op.dot4.f32(i32 56, float %2252, float %2255, float %2258, float 1.000000e+00, float %2265, float %2266, float %2267, float %2268)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2276 = call float @dx.op.dot4.f32(i32 56, float %2252, float %2255, float %2258, float 1.000000e+00, float %2270, float %2271, float %2272, float %2273)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2277 = fmul fast float %2274, %2143
  %2278 = call float @dx.op.tertiary.f32(i32 46, float %2275, float %2147, float %2277)  ; FMad(a,b,c)
  %2279 = call float @dx.op.tertiary.f32(i32 46, float %2276, float %2151, float %2278)  ; FMad(a,b,c)
  %2280 = fadd fast float %2279, %2155
  %2281 = fmul fast float %2274, %2144
  %2282 = call float @dx.op.tertiary.f32(i32 46, float %2275, float %2148, float %2281)  ; FMad(a,b,c)
  %2283 = call float @dx.op.tertiary.f32(i32 46, float %2276, float %2152, float %2282)  ; FMad(a,b,c)
  %2284 = fadd fast float %2283, %2156
  %2285 = fmul fast float %2274, %2145
  %2286 = call float @dx.op.tertiary.f32(i32 46, float %2275, float %2149, float %2285)  ; FMad(a,b,c)
  %2287 = call float @dx.op.tertiary.f32(i32 46, float %2276, float %2153, float %2286)  ; FMad(a,b,c)
  %2288 = fadd fast float %2287, %2157
  %2289 = fdiv fast float %2280, %2288
  %2290 = fdiv fast float %2284, %2288
  %2291 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 %2249
  %2292 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 %2249
  store float %2289, float* %2291, align 4
  store float %2290, float* %2292, align 4
  %2293 = add nuw nsw i32 %2249, 1
  %2294 = icmp eq i32 %2293, 8
  br i1 %2294, label %2295, label %2248

; <label>:2295                                    ; preds = %2248
  %2296 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 3
  %2297 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 3
  %2298 = load float, float* %2296, align 4
  %2299 = load float, float* %2297, align 4
  %2300 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 2
  %2301 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 2
  %2302 = load float, float* %2300, align 4
  %2303 = load float, float* %2301, align 4
  %2304 = call float @dx.op.binary.f32(i32 36, float %2302, float %2298)  ; FMin(a,b)
  %2305 = call float @dx.op.binary.f32(i32 36, float %2303, float %2299)  ; FMin(a,b)
  %2306 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 1
  %2307 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 1
  %2308 = load float, float* %2306, align 4
  %2309 = load float, float* %2307, align 4
  %2310 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 0
  %2311 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 0
  %2312 = load float, float* %2310, align 4
  %2313 = load float, float* %2311, align 4
  %2314 = call float @dx.op.binary.f32(i32 36, float %2312, float %2308)  ; FMin(a,b)
  %2315 = call float @dx.op.binary.f32(i32 36, float %2313, float %2309)  ; FMin(a,b)
  %2316 = call float @dx.op.binary.f32(i32 36, float %2314, float %2304)  ; FMin(a,b)
  %2317 = call float @dx.op.binary.f32(i32 36, float %2315, float %2305)  ; FMin(a,b)
  %2318 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 7
  %2319 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 7
  %2320 = load float, float* %2318, align 4
  %2321 = load float, float* %2319, align 4
  %2322 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 6
  %2323 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 6
  %2324 = load float, float* %2322, align 4
  %2325 = load float, float* %2323, align 4
  %2326 = call float @dx.op.binary.f32(i32 36, float %2324, float %2320)  ; FMin(a,b)
  %2327 = call float @dx.op.binary.f32(i32 36, float %2325, float %2321)  ; FMin(a,b)
  %2328 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 5
  %2329 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 5
  %2330 = load float, float* %2328, align 4
  %2331 = load float, float* %2329, align 4
  %2332 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 4
  %2333 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 4
  %2334 = load float, float* %2332, align 4
  %2335 = load float, float* %2333, align 4
  %2336 = call float @dx.op.binary.f32(i32 36, float %2334, float %2330)  ; FMin(a,b)
  %2337 = call float @dx.op.binary.f32(i32 36, float %2335, float %2331)  ; FMin(a,b)
  %2338 = call float @dx.op.binary.f32(i32 36, float %2336, float %2326)  ; FMin(a,b)
  %2339 = call float @dx.op.binary.f32(i32 36, float %2337, float %2327)  ; FMin(a,b)
  %2340 = call float @dx.op.binary.f32(i32 36, float %2316, float %2338)  ; FMin(a,b)
  %2341 = call float @dx.op.binary.f32(i32 36, float %2317, float %2339)  ; FMin(a,b)
  %2342 = call float @dx.op.binary.f32(i32 35, float %2302, float %2298)  ; FMax(a,b)
  %2343 = call float @dx.op.binary.f32(i32 35, float %2303, float %2299)  ; FMax(a,b)
  %2344 = call float @dx.op.binary.f32(i32 35, float %2312, float %2308)  ; FMax(a,b)
  %2345 = call float @dx.op.binary.f32(i32 35, float %2313, float %2309)  ; FMax(a,b)
  %2346 = call float @dx.op.binary.f32(i32 35, float %2344, float %2342)  ; FMax(a,b)
  %2347 = call float @dx.op.binary.f32(i32 35, float %2345, float %2343)  ; FMax(a,b)
  %2348 = call float @dx.op.binary.f32(i32 35, float %2324, float %2320)  ; FMax(a,b)
  %2349 = call float @dx.op.binary.f32(i32 35, float %2325, float %2321)  ; FMax(a,b)
  %2350 = call float @dx.op.binary.f32(i32 35, float %2334, float %2330)  ; FMax(a,b)
  %2351 = call float @dx.op.binary.f32(i32 35, float %2335, float %2331)  ; FMax(a,b)
  %2352 = call float @dx.op.binary.f32(i32 35, float %2350, float %2348)  ; FMax(a,b)
  %2353 = call float @dx.op.binary.f32(i32 35, float %2351, float %2349)  ; FMax(a,b)
  %2354 = call float @dx.op.binary.f32(i32 35, float %2346, float %2352)  ; FMax(a,b)
  %2355 = call float @dx.op.binary.f32(i32 35, float %2347, float %2353)  ; FMax(a,b)
  %2356 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2357 = extractvalue %dx.types.CBufRet.i32 %2356, 0
  %2358 = and i32 %2357, 15
  %2359 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2360 = extractvalue %dx.types.CBufRet.f32 %2359, 0
  %2361 = extractvalue %dx.types.CBufRet.f32 %2359, 1
  %2362 = extractvalue %dx.types.CBufRet.f32 %2359, 2
  %2363 = extractvalue %dx.types.CBufRet.f32 %2359, 3
  %2364 = fmul fast float %2340, 5.000000e-01
  %2365 = fmul fast float %2355, 5.000000e-01
  %2366 = fadd fast float %2364, 5.000000e-01
  %2367 = fsub fast float 5.000000e-01, %2365
  %2368 = fmul fast float %2354, 5.000000e-01
  %2369 = fmul fast float %2341, 5.000000e-01
  %2370 = fadd fast float %2368, 5.000000e-01
  %2371 = fsub fast float 5.000000e-01, %2369
  %2372 = fmul fast float %2360, %2366
  %2373 = fmul fast float %2361, %2367
  %2374 = fadd fast float %2372, %2362
  %2375 = fadd fast float %2373, %2363
  %2376 = fmul fast float %2360, %2370
  %2377 = fmul fast float %2361, %2371
  %2378 = fadd fast float %2376, %2362
  %2379 = fadd fast float %2377, %2363
  %2380 = call float @dx.op.binary.f32(i32 35, float %2374, float 0.000000e+00)  ; FMax(a,b)
  %2381 = call float @dx.op.binary.f32(i32 35, float %2375, float 0.000000e+00)  ; FMax(a,b)
  %2382 = call float @dx.op.binary.f32(i32 36, float %2380, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2383 = call float @dx.op.binary.f32(i32 36, float %2381, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2384 = call float @dx.op.binary.f32(i32 35, float %2378, float 0.000000e+00)  ; FMax(a,b)
  %2385 = call float @dx.op.binary.f32(i32 35, float %2379, float 0.000000e+00)  ; FMax(a,b)
  %2386 = call float @dx.op.binary.f32(i32 36, float %2384, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2387 = call float @dx.op.binary.f32(i32 36, float %2385, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2388 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %2389 = extractvalue %dx.types.ResRet.i32 %2388, 0
  %2390 = icmp slt i32 %2389, 1
  br i1 %2390, label %2456, label %2391

; <label>:2391                                    ; preds = %2295
  %2392 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %2393 = extractvalue %dx.types.ResRet.i32 %2392, 0
  %2394 = icmp ult i32 %2358, %2393
  br i1 %2394, label %2395, label %2456

; <label>:2395                                    ; preds = %2391
  %2396 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %2397 = extractvalue %dx.types.ResRet.i32 %2396, 0
  %2398 = uitofp i32 %2389 to float
  %2399 = uitofp i32 %2397 to float
  %2400 = fmul fast float %2398, %2382
  %2401 = fmul fast float %2399, %2383
  %2402 = fptoui float %2400 to i32
  %2403 = fptoui float %2401 to i32
  %2404 = mul i32 %2403, %2389
  %2405 = add i32 %2404, %2402
  %2406 = fmul fast float %2398, %2386
  %2407 = fptoui float %2406 to i32
  %2408 = add i32 %2404, %2407
  %2409 = fmul fast float %2399, %2387
  %2410 = fptoui float %2409 to i32
  %2411 = mul i32 %2410, %2389
  %2412 = add i32 %2411, %2402
  %2413 = mul i32 %2389, %2358
  %2414 = mul i32 %2413, %2397
  %2415 = add nsw i32 %2414, 3
  %2416 = icmp sge i32 %2405, %2389
  %2417 = srem i32 %2405, %2389
  %2418 = icmp ne i32 %2417, 0
  %2419 = and i1 %2418, %2416
  br i1 %2419, label %2420, label %2426

; <label>:2420                                    ; preds = %2395
  %2421 = xor i32 %2389, -1
  %2422 = add i32 %2415, %2421
  %2423 = add i32 %2422, %2405
  %2424 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2423, i32 undef)  ; BufferLoad(srv,index,wot)
  %2425 = extractvalue %dx.types.ResRet.i32 %2424, 0
  br label %2426

; <label>:2426                                    ; preds = %2420, %2395
  %2427 = phi i32 [ %2425, %2420 ], [ 0, %2395 ]
  %2428 = icmp slt i32 %2408, %2389
  br i1 %2428, label %2434, label %2429

; <label>:2429                                    ; preds = %2426
  %2430 = sub i32 %2415, %2389
  %2431 = add i32 %2430, %2408
  %2432 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2431, i32 undef)  ; BufferLoad(srv,index,wot)
  %2433 = extractvalue %dx.types.ResRet.i32 %2432, 0
  br label %2434

; <label>:2434                                    ; preds = %2429, %2426
  %2435 = phi i32 [ %2433, %2429 ], [ 0, %2426 ]
  %2436 = srem i32 %2412, %2389
  %2437 = icmp eq i32 %2436, 0
  br i1 %2437, label %2443, label %2438

; <label>:2438                                    ; preds = %2434
  %2439 = add i32 %2414, 2
  %2440 = add i32 %2439, %2412
  %2441 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2440, i32 undef)  ; BufferLoad(srv,index,wot)
  %2442 = extractvalue %dx.types.ResRet.i32 %2441, 0
  br label %2443

; <label>:2443                                    ; preds = %2438, %2434
  %2444 = phi i32 [ %2442, %2438 ], [ 0, %2434 ]
  %2445 = add i32 %2415, %2407
  %2446 = add i32 %2445, %2411
  %2447 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2446, i32 undef)  ; BufferLoad(srv,index,wot)
  %2448 = extractvalue %dx.types.ResRet.i32 %2447, 0
  %2449 = add i32 %2444, %2435
  %2450 = sub i32 %2448, %2449
  %2451 = sub i32 0, %2427
  %2452 = icmp eq i32 %2450, %2451
  %2453 = xor i1 %183, true
  %2454 = or i1 %2452, %2453
  %2455 = select i1 %2452, i32 11, i32 0
  br i1 %2454, label %2932, label %2457

; <label>:2456                                    ; preds = %2391, %2295, %2244
  br i1 %183, label %2457, label %2932

; <label>:2457                                    ; preds = %2456, %2443
  %2458 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2158, i32 0)  ; BufferLoad(srv,index,wot)
  %2459 = extractvalue %dx.types.ResRet.f32 %2458, 0
  %2460 = extractvalue %dx.types.ResRet.f32 %2458, 1
  %2461 = extractvalue %dx.types.ResRet.f32 %2458, 2
  %2462 = extractvalue %dx.types.ResRet.f32 %2458, 3
  %2463 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2165, i32 0)  ; BufferLoad(srv,index,wot)
  %2464 = extractvalue %dx.types.ResRet.f32 %2463, 0
  %2465 = extractvalue %dx.types.ResRet.f32 %2463, 1
  %2466 = extractvalue %dx.types.ResRet.f32 %2463, 2
  %2467 = extractvalue %dx.types.ResRet.f32 %2463, 3
  %2468 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2172, i32 0)  ; BufferLoad(srv,index,wot)
  %2469 = extractvalue %dx.types.ResRet.f32 %2468, 0
  %2470 = extractvalue %dx.types.ResRet.f32 %2468, 1
  %2471 = extractvalue %dx.types.ResRet.f32 %2468, 2
  %2472 = extractvalue %dx.types.ResRet.f32 %2468, 3
  %2473 = call float @dx.op.dot3.f32(i32 55, float %2469, float %2470, float %2471, float %2469, float %2470, float %2471)  ; Dot3(ax,ay,az,bx,by,bz)
  %2474 = call float @dx.op.dot3.f32(i32 55, float %2464, float %2465, float %2466, float %2464, float %2465, float %2466)  ; Dot3(ax,ay,az,bx,by,bz)
  %2475 = call float @dx.op.binary.f32(i32 35, float %2474, float %2473)  ; FMax(a,b)
  %2476 = call float @dx.op.dot3.f32(i32 55, float %2459, float %2460, float %2461, float %2459, float %2460, float %2461)  ; Dot3(ax,ay,az,bx,by,bz)
  %2477 = call float @dx.op.binary.f32(i32 35, float %2476, float %2475)  ; FMax(a,b)
  %2478 = call float @dx.op.unary.f32(i32 24, float %2477)  ; Sqrt(value)
  %2479 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2459, float %2460, float %2461, float %2462)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2480 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2464, float %2465, float %2466, float %2467)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2481 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2469, float %2470, float %2471, float %2472)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2482 = fmul fast float %2478, %143
  %2483 = fmul fast float %2479, %2143
  %2484 = call float @dx.op.tertiary.f32(i32 46, float %2480, float %2147, float %2483)  ; FMad(a,b,c)
  %2485 = call float @dx.op.tertiary.f32(i32 46, float %2481, float %2151, float %2484)  ; FMad(a,b,c)
  %2486 = fadd fast float %2485, %2155
  %2487 = fmul fast float %2479, %2144
  %2488 = call float @dx.op.tertiary.f32(i32 46, float %2480, float %2148, float %2487)  ; FMad(a,b,c)
  %2489 = call float @dx.op.tertiary.f32(i32 46, float %2481, float %2152, float %2488)  ; FMad(a,b,c)
  %2490 = fadd fast float %2489, %2156
  %2491 = fmul fast float %2479, %2145
  %2492 = call float @dx.op.tertiary.f32(i32 46, float %2480, float %2149, float %2491)  ; FMad(a,b,c)
  %2493 = call float @dx.op.tertiary.f32(i32 46, float %2481, float %2153, float %2492)  ; FMad(a,b,c)
  %2494 = fadd fast float %2493, %2157
  %2495 = fdiv fast float %2486, %2494
  %2496 = fdiv fast float %2490, %2494
  %2497 = fmul fast float %2495, 5.000000e-01
  %2498 = fmul fast float %2496, 5.000000e-01
  %2499 = fadd fast float %2497, 5.000000e-01
  %2500 = fadd fast float %2498, 5.000000e-01
  %2501 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2502 = extractvalue %dx.types.CBufRet.f32 %2501, 0
  %2503 = extractvalue %dx.types.CBufRet.f32 %2501, 1
  %2504 = extractvalue %dx.types.CBufRet.f32 %2501, 2
  %2505 = fmul fast float %2502, 5.000000e-01
  %2506 = fmul fast float %2503, 5.000000e-01
  %2507 = fmul fast float %2502, %2499
  %2508 = call float @dx.op.unary.f32(i32 26, float %2507)  ; Round_ne(value)
  %2509 = fsub fast float %2508, %2505
  %2510 = fmul fast float %2503, %2500
  %2511 = call float @dx.op.unary.f32(i32 26, float %2510)  ; Round_ne(value)
  %2512 = fsub fast float %2511, %2506
  %2513 = fsub fast float -0.000000e+00, %2504
  %2514 = fadd fast float %2509, -5.000000e-01
  %2515 = fadd fast float %2512, -5.000000e-01
  %2516 = fadd fast float %2512, 5.000000e-01
  %2517 = fadd fast float %2509, 5.000000e-01
  %2518 = call float @dx.op.dot3.f32(i32 55, float %2514, float %2515, float %2513, float %2514, float %2515, float %2513)  ; Dot3(ax,ay,az,bx,by,bz)
  %2519 = call float @dx.op.unary.f32(i32 25, float %2518)  ; Rsqrt(value)
  %2520 = fmul fast float %2519, %2514
  %2521 = fmul fast float %2515, %2519
  %2522 = fmul fast float %2519, %2513
  %2523 = call float @dx.op.dot3.f32(i32 55, float %2514, float %2516, float %2513, float %2514, float %2516, float %2513)  ; Dot3(ax,ay,az,bx,by,bz)
  %2524 = call float @dx.op.unary.f32(i32 25, float %2523)  ; Rsqrt(value)
  %2525 = fmul fast float %2524, %2514
  %2526 = fmul fast float %2524, %2516
  %2527 = fmul fast float %2524, %2513
  %2528 = call float @dx.op.dot3.f32(i32 55, float %2517, float %2515, float %2513, float %2517, float %2515, float %2513)  ; Dot3(ax,ay,az,bx,by,bz)
  %2529 = call float @dx.op.unary.f32(i32 25, float %2528)  ; Rsqrt(value)
  %2530 = fmul fast float %2529, %2517
  %2531 = fmul fast float %2529, %2515
  %2532 = fmul fast float %2529, %2513
  %2533 = call float @dx.op.dot3.f32(i32 55, float %2517, float %2516, float %2513, float %2517, float %2516, float %2513)  ; Dot3(ax,ay,az,bx,by,bz)
  %2534 = call float @dx.op.unary.f32(i32 25, float %2533)  ; Rsqrt(value)
  %2535 = fmul fast float %2534, %2517
  %2536 = fmul fast float %2534, %2516
  %2537 = fmul fast float %2534, %2513
  %2538 = fmul fast float %2494, %2495
  %2539 = fmul fast float %2538, %2505
  %2540 = fdiv fast float %2539, %2504
  %2541 = fmul fast float %2496, %2494
  %2542 = fmul fast float %2541, %2506
  %2543 = fdiv fast float %2542, %2504
  %2544 = fsub fast float -0.000000e+00, %2494
  %2545 = fmul fast float %2543, %2522
  %2546 = fmul fast float %2521, %2544
  %2547 = fsub fast float %2545, %2546
  %2548 = fmul fast float %2520, %2544
  %2549 = fmul fast float %2540, %2522
  %2550 = fsub fast float %2548, %2549
  %2551 = fmul fast float %2540, %2521
  %2552 = fmul fast float %2543, %2520
  %2553 = fsub fast float %2551, %2552
  %2554 = fmul fast float %2547, %2547
  %2555 = fmul fast float %2550, %2550
  %2556 = fadd fast float %2554, %2555
  %2557 = fmul fast float %2553, %2553
  %2558 = fadd fast float %2556, %2557
  %2559 = call float @dx.op.unary.f32(i32 24, float %2558)  ; Sqrt(value)
  %2560 = fcmp fast olt float %2559, %2482
  br i1 %2560, label %2932, label %2561

; <label>:2561                                    ; preds = %2457
  %2562 = fmul fast float %2543, %2527
  %2563 = fmul fast float %2526, %2544
  %2564 = fsub fast float %2562, %2563
  %2565 = fmul fast float %2525, %2544
  %2566 = fmul fast float %2540, %2527
  %2567 = fsub fast float %2565, %2566
  %2568 = fmul fast float %2540, %2526
  %2569 = fmul fast float %2543, %2525
  %2570 = fsub fast float %2568, %2569
  %2571 = fmul fast float %2564, %2564
  %2572 = fmul fast float %2567, %2567
  %2573 = fadd fast float %2571, %2572
  %2574 = fmul fast float %2570, %2570
  %2575 = fadd fast float %2573, %2574
  %2576 = call float @dx.op.unary.f32(i32 24, float %2575)  ; Sqrt(value)
  %2577 = fcmp fast olt float %2576, %2482
  br i1 %2577, label %2932, label %2578

; <label>:2578                                    ; preds = %2561
  %2579 = fmul fast float %2543, %2532
  %2580 = fmul fast float %2531, %2544
  %2581 = fsub fast float %2579, %2580
  %2582 = fmul fast float %2530, %2544
  %2583 = fmul fast float %2540, %2532
  %2584 = fsub fast float %2582, %2583
  %2585 = fmul fast float %2540, %2531
  %2586 = fmul fast float %2543, %2530
  %2587 = fsub fast float %2585, %2586
  %2588 = fmul fast float %2581, %2581
  %2589 = fmul fast float %2584, %2584
  %2590 = fadd fast float %2588, %2589
  %2591 = fmul fast float %2587, %2587
  %2592 = fadd fast float %2590, %2591
  %2593 = call float @dx.op.unary.f32(i32 24, float %2592)  ; Sqrt(value)
  %2594 = fcmp fast olt float %2593, %2482
  br i1 %2594, label %2932, label %2595

; <label>:2595                                    ; preds = %2578
  %2596 = fmul fast float %2543, %2537
  %2597 = fmul fast float %2536, %2544
  %2598 = fsub fast float %2596, %2597
  %2599 = fmul fast float %2535, %2544
  %2600 = fmul fast float %2540, %2537
  %2601 = fsub fast float %2599, %2600
  %2602 = fmul fast float %2540, %2536
  %2603 = fmul fast float %2543, %2535
  %2604 = fsub fast float %2602, %2603
  %2605 = fmul fast float %2598, %2598
  %2606 = fmul fast float %2601, %2601
  %2607 = fadd fast float %2605, %2606
  %2608 = fmul fast float %2604, %2604
  %2609 = fadd fast float %2607, %2608
  %2610 = call float @dx.op.unary.f32(i32 24, float %2609)  ; Sqrt(value)
  %2611 = fcmp fast olt float %2610, %2482
  br i1 %2611, label %2932, label %2612

; <label>:2612                                    ; preds = %2595
  br label %2932

; <label>:2613                                    ; preds = %2139
  %2614 = icmp eq i32 %184, 5
  br i1 %2614, label %2615, label %2932

; <label>:2615                                    ; preds = %2613
  %2616 = lshr i32 %123, 16
  %2617 = and i32 %2616, 15
  %2618 = icmp eq i32 %2617, 0
  br i1 %2618, label %2619, label %2932

; <label>:2619                                    ; preds = %2615
  %2620 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2621 = extractvalue %dx.types.CBufRet.f32 %2620, 0
  %2622 = extractvalue %dx.types.CBufRet.f32 %2620, 1
  %2623 = extractvalue %dx.types.CBufRet.f32 %2620, 3
  %2624 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %2625 = extractvalue %dx.types.CBufRet.f32 %2624, 0
  %2626 = extractvalue %dx.types.CBufRet.f32 %2624, 1
  %2627 = extractvalue %dx.types.CBufRet.f32 %2624, 3
  %2628 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %2629 = extractvalue %dx.types.CBufRet.f32 %2628, 0
  %2630 = extractvalue %dx.types.CBufRet.f32 %2628, 1
  %2631 = extractvalue %dx.types.CBufRet.f32 %2628, 3
  %2632 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %2633 = extractvalue %dx.types.CBufRet.f32 %2632, 0
  %2634 = extractvalue %dx.types.CBufRet.f32 %2632, 1
  %2635 = extractvalue %dx.types.CBufRet.f32 %2632, 3
  %2636 = add i32 %181, %179
  %2637 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2636, i32 0)  ; BufferLoad(srv,index,wot)
  %2638 = extractvalue %dx.types.ResRet.f32 %2637, 0
  %2639 = extractvalue %dx.types.ResRet.f32 %2637, 1
  %2640 = extractvalue %dx.types.ResRet.f32 %2637, 2
  %2641 = extractvalue %dx.types.ResRet.f32 %2637, 3
  %2642 = add nuw nsw i32 %179, 1
  %2643 = add i32 %2642, %181
  %2644 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2643, i32 0)  ; BufferLoad(srv,index,wot)
  %2645 = extractvalue %dx.types.ResRet.f32 %2644, 0
  %2646 = extractvalue %dx.types.ResRet.f32 %2644, 1
  %2647 = extractvalue %dx.types.ResRet.f32 %2644, 2
  %2648 = extractvalue %dx.types.ResRet.f32 %2644, 3
  %2649 = add nuw nsw i32 %179, 2
  %2650 = add i32 %2649, %181
  %2651 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2650, i32 0)  ; BufferLoad(srv,index,wot)
  %2652 = extractvalue %dx.types.ResRet.f32 %2651, 0
  %2653 = extractvalue %dx.types.ResRet.f32 %2651, 1
  %2654 = extractvalue %dx.types.ResRet.f32 %2651, 2
  %2655 = extractvalue %dx.types.ResRet.f32 %2651, 3
  %2656 = call float @dx.op.dot3.f32(i32 55, float %2652, float %2653, float %2654, float %2652, float %2653, float %2654)  ; Dot3(ax,ay,az,bx,by,bz)
  %2657 = call float @dx.op.dot3.f32(i32 55, float %2645, float %2646, float %2647, float %2645, float %2646, float %2647)  ; Dot3(ax,ay,az,bx,by,bz)
  %2658 = call float @dx.op.binary.f32(i32 35, float %2657, float %2656)  ; FMax(a,b)
  %2659 = call float @dx.op.dot3.f32(i32 55, float %2638, float %2639, float %2640, float %2638, float %2639, float %2640)  ; Dot3(ax,ay,az,bx,by,bz)
  %2660 = call float @dx.op.binary.f32(i32 35, float %2659, float %2658)  ; FMax(a,b)
  %2661 = call float @dx.op.unary.f32(i32 24, float %2660)  ; Sqrt(value)
  %2662 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2638, float %2639, float %2640, float %2641)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2663 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2645, float %2646, float %2647, float %2648)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2664 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2652, float %2653, float %2654, float %2655)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2665 = fmul fast float %2661, %143
  %2666 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %2667 = extractvalue %dx.types.CBufRet.f32 %2666, 0
  %2668 = extractvalue %dx.types.CBufRet.f32 %2666, 1
  %2669 = extractvalue %dx.types.CBufRet.f32 %2666, 2
  %2670 = extractvalue %dx.types.CBufRet.f32 %2666, 3
  %2671 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %2667, float %2668, float %2669, float %2670)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2672 = fsub fast float -0.000000e+00, %2665
  %2673 = fcmp fast olt float %2671, %2672
  br i1 %2673, label %2932, label %2674

; <label>:2674                                    ; preds = %2619
  %2675 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %2676 = extractvalue %dx.types.CBufRet.f32 %2675, 0
  %2677 = extractvalue %dx.types.CBufRet.f32 %2675, 1
  %2678 = extractvalue %dx.types.CBufRet.f32 %2675, 2
  %2679 = extractvalue %dx.types.CBufRet.f32 %2675, 3
  %2680 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %2676, float %2677, float %2678, float %2679)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2681 = fcmp fast olt float %2680, %2672
  br i1 %2681, label %2932, label %3116

; <label>:2682                                    ; preds = %3148
  %2683 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2638, float %2639, float %2640)  ; Dot3(ax,ay,az,bx,by,bz)
  %2684 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2645, float %2646, float %2647)  ; Dot3(ax,ay,az,bx,by,bz)
  %2685 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2652, float %2653, float %2654)  ; Dot3(ax,ay,az,bx,by,bz)
  %2686 = call float @dx.op.dot3.f32(i32 55, float %2683, float %2684, float %2685, float %2683, float %2684, float %2685)  ; Dot3(ax,ay,az,bx,by,bz)
  %2687 = call float @dx.op.unary.f32(i32 25, float %2686)  ; Rsqrt(value)
  %2688 = fmul fast float %2687, %2683
  %2689 = fmul fast float %2687, %2684
  %2690 = fmul fast float %2687, %2685
  %2691 = icmp eq i32 %85, 2
  %2692 = fsub fast float -0.000000e+00, %2688
  %2693 = fsub fast float -0.000000e+00, %2689
  %2694 = fsub fast float -0.000000e+00, %2690
  %2695 = select i1 %2691, float %2692, float %2688
  %2696 = select i1 %2691, float %2693, float %2689
  %2697 = select i1 %2691, float %2694, float %2690
  %2698 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2699 = extractvalue %dx.types.CBufRet.f32 %2698, 0
  %2700 = extractvalue %dx.types.CBufRet.f32 %2698, 1
  %2701 = extractvalue %dx.types.CBufRet.f32 %2698, 2
  %2702 = fmul fast float %2661, %164
  %2703 = fmul fast float %2702, %2695
  %2704 = fmul fast float %2702, %2696
  %2705 = fmul fast float %2702, %2697
  %2706 = fsub fast float %2662, %2703
  %2707 = fsub fast float %2663, %2704
  %2708 = fsub fast float %2664, %2705
  %2709 = fsub fast float %2699, %2706
  %2710 = fsub fast float %2700, %2707
  %2711 = fsub fast float %2701, %2708
  %2712 = call float @dx.op.dot3.f32(i32 55, float %2709, float %2710, float %2711, float %2709, float %2710, float %2711)  ; Dot3(ax,ay,az,bx,by,bz)
  %2713 = call float @dx.op.unary.f32(i32 25, float %2712)  ; Rsqrt(value)
  %2714 = fmul fast float %2709, %2713
  %2715 = fmul fast float %2710, %2713
  %2716 = fmul fast float %2711, %2713
  %2717 = fsub fast float -0.000000e+00, %2695
  %2718 = fsub fast float -0.000000e+00, %2696
  %2719 = fsub fast float -0.000000e+00, %2697
  %2720 = call float @dx.op.dot3.f32(i32 55, float %2714, float %2715, float %2716, float %2717, float %2718, float %2719)  ; Dot3(ax,ay,az,bx,by,bz)
  %2721 = fcmp fast ogt float %2720, %155
  br i1 %2721, label %2932, label %2722

; <label>:2722                                    ; preds = %3148, %2682
  %2723 = and i32 %80, 512
  %2724 = icmp eq i32 %2723, 0
  br i1 %2724, label %2932, label %2725

; <label>:2725                                    ; preds = %2722
  br label %2726

; <label>:2726                                    ; preds = %2726, %2725
  %2727 = phi i32 [ %2771, %2726 ], [ 0, %2725 ]
  %2728 = and i32 %2727, 1
  %2729 = icmp ne i32 %2728, 0
  %2730 = select i1 %2729, float %175, float %165
  %2731 = and i32 %2727, 2
  %2732 = icmp ne i32 %2731, 0
  %2733 = select i1 %2732, float %176, float %166
  %2734 = and i32 %2727, 4
  %2735 = icmp ne i32 %2734, 0
  %2736 = select i1 %2735, float %177, float %167
  %2737 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2636, i32 0)  ; BufferLoad(srv,index,wot)
  %2738 = extractvalue %dx.types.ResRet.f32 %2737, 0
  %2739 = extractvalue %dx.types.ResRet.f32 %2737, 1
  %2740 = extractvalue %dx.types.ResRet.f32 %2737, 2
  %2741 = extractvalue %dx.types.ResRet.f32 %2737, 3
  %2742 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2643, i32 0)  ; BufferLoad(srv,index,wot)
  %2743 = extractvalue %dx.types.ResRet.f32 %2742, 0
  %2744 = extractvalue %dx.types.ResRet.f32 %2742, 1
  %2745 = extractvalue %dx.types.ResRet.f32 %2742, 2
  %2746 = extractvalue %dx.types.ResRet.f32 %2742, 3
  %2747 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2650, i32 0)  ; BufferLoad(srv,index,wot)
  %2748 = extractvalue %dx.types.ResRet.f32 %2747, 0
  %2749 = extractvalue %dx.types.ResRet.f32 %2747, 1
  %2750 = extractvalue %dx.types.ResRet.f32 %2747, 2
  %2751 = extractvalue %dx.types.ResRet.f32 %2747, 3
  %2752 = call float @dx.op.dot4.f32(i32 56, float %2730, float %2733, float %2736, float 1.000000e+00, float %2738, float %2739, float %2740, float %2741)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2753 = call float @dx.op.dot4.f32(i32 56, float %2730, float %2733, float %2736, float 1.000000e+00, float %2743, float %2744, float %2745, float %2746)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2754 = call float @dx.op.dot4.f32(i32 56, float %2730, float %2733, float %2736, float 1.000000e+00, float %2748, float %2749, float %2750, float %2751)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2755 = fmul fast float %2752, %2621
  %2756 = call float @dx.op.tertiary.f32(i32 46, float %2753, float %2625, float %2755)  ; FMad(a,b,c)
  %2757 = call float @dx.op.tertiary.f32(i32 46, float %2754, float %2629, float %2756)  ; FMad(a,b,c)
  %2758 = fadd fast float %2757, %2633
  %2759 = fmul fast float %2752, %2622
  %2760 = call float @dx.op.tertiary.f32(i32 46, float %2753, float %2626, float %2759)  ; FMad(a,b,c)
  %2761 = call float @dx.op.tertiary.f32(i32 46, float %2754, float %2630, float %2760)  ; FMad(a,b,c)
  %2762 = fadd fast float %2761, %2634
  %2763 = fmul fast float %2752, %2623
  %2764 = call float @dx.op.tertiary.f32(i32 46, float %2753, float %2627, float %2763)  ; FMad(a,b,c)
  %2765 = call float @dx.op.tertiary.f32(i32 46, float %2754, float %2631, float %2764)  ; FMad(a,b,c)
  %2766 = fadd fast float %2765, %2635
  %2767 = fdiv fast float %2758, %2766
  %2768 = fdiv fast float %2762, %2766
  %2769 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 %2727
  %2770 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 %2727
  store float %2767, float* %2769, align 4
  store float %2768, float* %2770, align 4
  %2771 = add nuw nsw i32 %2727, 1
  %2772 = icmp eq i32 %2771, 8
  br i1 %2772, label %2773, label %2726

; <label>:2773                                    ; preds = %2726
  %2774 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 3
  %2775 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 3
  %2776 = load float, float* %2774, align 4
  %2777 = load float, float* %2775, align 4
  %2778 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 2
  %2779 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 2
  %2780 = load float, float* %2778, align 4
  %2781 = load float, float* %2779, align 4
  %2782 = call float @dx.op.binary.f32(i32 36, float %2780, float %2776)  ; FMin(a,b)
  %2783 = call float @dx.op.binary.f32(i32 36, float %2781, float %2777)  ; FMin(a,b)
  %2784 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 1
  %2785 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 1
  %2786 = load float, float* %2784, align 4
  %2787 = load float, float* %2785, align 4
  %2788 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 0
  %2789 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 0
  %2790 = load float, float* %2788, align 4
  %2791 = load float, float* %2789, align 4
  %2792 = call float @dx.op.binary.f32(i32 36, float %2790, float %2786)  ; FMin(a,b)
  %2793 = call float @dx.op.binary.f32(i32 36, float %2791, float %2787)  ; FMin(a,b)
  %2794 = call float @dx.op.binary.f32(i32 36, float %2792, float %2782)  ; FMin(a,b)
  %2795 = call float @dx.op.binary.f32(i32 36, float %2793, float %2783)  ; FMin(a,b)
  %2796 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 7
  %2797 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 7
  %2798 = load float, float* %2796, align 4
  %2799 = load float, float* %2797, align 4
  %2800 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 6
  %2801 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 6
  %2802 = load float, float* %2800, align 4
  %2803 = load float, float* %2801, align 4
  %2804 = call float @dx.op.binary.f32(i32 36, float %2802, float %2798)  ; FMin(a,b)
  %2805 = call float @dx.op.binary.f32(i32 36, float %2803, float %2799)  ; FMin(a,b)
  %2806 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 5
  %2807 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 5
  %2808 = load float, float* %2806, align 4
  %2809 = load float, float* %2807, align 4
  %2810 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 4
  %2811 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 4
  %2812 = load float, float* %2810, align 4
  %2813 = load float, float* %2811, align 4
  %2814 = call float @dx.op.binary.f32(i32 36, float %2812, float %2808)  ; FMin(a,b)
  %2815 = call float @dx.op.binary.f32(i32 36, float %2813, float %2809)  ; FMin(a,b)
  %2816 = call float @dx.op.binary.f32(i32 36, float %2814, float %2804)  ; FMin(a,b)
  %2817 = call float @dx.op.binary.f32(i32 36, float %2815, float %2805)  ; FMin(a,b)
  %2818 = call float @dx.op.binary.f32(i32 36, float %2794, float %2816)  ; FMin(a,b)
  %2819 = call float @dx.op.binary.f32(i32 36, float %2795, float %2817)  ; FMin(a,b)
  %2820 = call float @dx.op.binary.f32(i32 35, float %2780, float %2776)  ; FMax(a,b)
  %2821 = call float @dx.op.binary.f32(i32 35, float %2781, float %2777)  ; FMax(a,b)
  %2822 = call float @dx.op.binary.f32(i32 35, float %2790, float %2786)  ; FMax(a,b)
  %2823 = call float @dx.op.binary.f32(i32 35, float %2791, float %2787)  ; FMax(a,b)
  %2824 = call float @dx.op.binary.f32(i32 35, float %2822, float %2820)  ; FMax(a,b)
  %2825 = call float @dx.op.binary.f32(i32 35, float %2823, float %2821)  ; FMax(a,b)
  %2826 = call float @dx.op.binary.f32(i32 35, float %2802, float %2798)  ; FMax(a,b)
  %2827 = call float @dx.op.binary.f32(i32 35, float %2803, float %2799)  ; FMax(a,b)
  %2828 = call float @dx.op.binary.f32(i32 35, float %2812, float %2808)  ; FMax(a,b)
  %2829 = call float @dx.op.binary.f32(i32 35, float %2813, float %2809)  ; FMax(a,b)
  %2830 = call float @dx.op.binary.f32(i32 35, float %2828, float %2826)  ; FMax(a,b)
  %2831 = call float @dx.op.binary.f32(i32 35, float %2829, float %2827)  ; FMax(a,b)
  %2832 = call float @dx.op.binary.f32(i32 35, float %2824, float %2830)  ; FMax(a,b)
  %2833 = call float @dx.op.binary.f32(i32 35, float %2825, float %2831)  ; FMax(a,b)
  %2834 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2835 = extractvalue %dx.types.CBufRet.i32 %2834, 0
  %2836 = and i32 %2835, 15
  %2837 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2838 = extractvalue %dx.types.CBufRet.f32 %2837, 0
  %2839 = extractvalue %dx.types.CBufRet.f32 %2837, 1
  %2840 = extractvalue %dx.types.CBufRet.f32 %2837, 2
  %2841 = extractvalue %dx.types.CBufRet.f32 %2837, 3
  %2842 = fmul fast float %2818, 5.000000e-01
  %2843 = fmul fast float %2833, 5.000000e-01
  %2844 = fadd fast float %2842, 5.000000e-01
  %2845 = fsub fast float 5.000000e-01, %2843
  %2846 = fmul fast float %2832, 5.000000e-01
  %2847 = fmul fast float %2819, 5.000000e-01
  %2848 = fadd fast float %2846, 5.000000e-01
  %2849 = fsub fast float 5.000000e-01, %2847
  %2850 = fmul fast float %2838, %2844
  %2851 = fmul fast float %2839, %2845
  %2852 = fadd fast float %2850, %2840
  %2853 = fadd fast float %2851, %2841
  %2854 = fmul fast float %2838, %2848
  %2855 = fmul fast float %2839, %2849
  %2856 = fadd fast float %2854, %2840
  %2857 = fadd fast float %2855, %2841
  %2858 = call float @dx.op.binary.f32(i32 35, float %2852, float 0.000000e+00)  ; FMax(a,b)
  %2859 = call float @dx.op.binary.f32(i32 35, float %2853, float 0.000000e+00)  ; FMax(a,b)
  %2860 = call float @dx.op.binary.f32(i32 36, float %2858, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2861 = call float @dx.op.binary.f32(i32 36, float %2859, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2862 = call float @dx.op.binary.f32(i32 35, float %2856, float 0.000000e+00)  ; FMax(a,b)
  %2863 = call float @dx.op.binary.f32(i32 35, float %2857, float 0.000000e+00)  ; FMax(a,b)
  %2864 = call float @dx.op.binary.f32(i32 36, float %2862, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2865 = call float @dx.op.binary.f32(i32 36, float %2863, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2866 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %2867 = extractvalue %dx.types.ResRet.i32 %2866, 0
  %2868 = icmp slt i32 %2867, 1
  br i1 %2868, label %2932, label %2869

; <label>:2869                                    ; preds = %2773
  %2870 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %2871 = extractvalue %dx.types.ResRet.i32 %2870, 0
  %2872 = icmp ult i32 %2836, %2871
  br i1 %2872, label %2873, label %2932

; <label>:2873                                    ; preds = %2869
  %2874 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %2875 = extractvalue %dx.types.ResRet.i32 %2874, 0
  %2876 = uitofp i32 %2867 to float
  %2877 = uitofp i32 %2875 to float
  %2878 = fmul fast float %2876, %2860
  %2879 = fmul fast float %2877, %2861
  %2880 = fptoui float %2878 to i32
  %2881 = fptoui float %2879 to i32
  %2882 = mul i32 %2881, %2867
  %2883 = add i32 %2882, %2880
  %2884 = fmul fast float %2876, %2864
  %2885 = fptoui float %2884 to i32
  %2886 = add i32 %2882, %2885
  %2887 = fmul fast float %2877, %2865
  %2888 = fptoui float %2887 to i32
  %2889 = mul i32 %2888, %2867
  %2890 = add i32 %2889, %2880
  %2891 = mul i32 %2867, %2836
  %2892 = mul i32 %2891, %2875
  %2893 = add nsw i32 %2892, 3
  %2894 = icmp sge i32 %2883, %2867
  %2895 = srem i32 %2883, %2867
  %2896 = icmp ne i32 %2895, 0
  %2897 = and i1 %2896, %2894
  br i1 %2897, label %2898, label %2904

; <label>:2898                                    ; preds = %2873
  %2899 = xor i32 %2867, -1
  %2900 = add i32 %2893, %2899
  %2901 = add i32 %2900, %2883
  %2902 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2901, i32 undef)  ; BufferLoad(srv,index,wot)
  %2903 = extractvalue %dx.types.ResRet.i32 %2902, 0
  br label %2904

; <label>:2904                                    ; preds = %2898, %2873
  %2905 = phi i32 [ %2903, %2898 ], [ 0, %2873 ]
  %2906 = icmp slt i32 %2886, %2867
  br i1 %2906, label %2912, label %2907

; <label>:2907                                    ; preds = %2904
  %2908 = sub i32 %2893, %2867
  %2909 = add i32 %2908, %2886
  %2910 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2909, i32 undef)  ; BufferLoad(srv,index,wot)
  %2911 = extractvalue %dx.types.ResRet.i32 %2910, 0
  br label %2912

; <label>:2912                                    ; preds = %2907, %2904
  %2913 = phi i32 [ %2911, %2907 ], [ 0, %2904 ]
  %2914 = srem i32 %2890, %2867
  %2915 = icmp eq i32 %2914, 0
  br i1 %2915, label %2921, label %2916

; <label>:2916                                    ; preds = %2912
  %2917 = add i32 %2892, 2
  %2918 = add i32 %2917, %2890
  %2919 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2918, i32 undef)  ; BufferLoad(srv,index,wot)
  %2920 = extractvalue %dx.types.ResRet.i32 %2919, 0
  br label %2921

; <label>:2921                                    ; preds = %2916, %2912
  %2922 = phi i32 [ %2920, %2916 ], [ 0, %2912 ]
  %2923 = add i32 %2893, %2885
  %2924 = add i32 %2923, %2889
  %2925 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2924, i32 undef)  ; BufferLoad(srv,index,wot)
  %2926 = extractvalue %dx.types.ResRet.i32 %2925, 0
  %2927 = add i32 %2922, %2913
  %2928 = sub i32 %2926, %2927
  %2929 = sub i32 0, %2905
  %2930 = icmp eq i32 %2928, %2929
  %2931 = select i1 %2930, i32 11, i32 0
  br label %2932

; <label>:2932                                    ; preds = %3140, %3132, %3124, %3116, %3106, %3098, %3090, %3082, %3064, %3056, %3048, %3040, %3030, %3022, %3014, %3006, %2921, %2869, %2773, %2722, %2682, %2674, %2619, %2615, %2613, %2612, %2595, %2578, %2561, %2457, %2456, %2443, %2204, %2196, %2141, %2138, %2136, %1648, %1645, %1643, %1160, %1004, %729, %721, %660, %657, %640, %623, %606, %502, %501, %488, %249, %241, %186
  %2933 = phi i32 [ %2617, %2615 ], [ %120, %2613 ], [ 10, %657 ], [ 2, %249 ], [ %500, %488 ], [ 0, %501 ], [ 0, %640 ], [ 0, %623 ], [ 0, %606 ], [ 0, %502 ], [ 0, %1645 ], [ 2, %729 ], [ 11, %1004 ], [ 3, %1643 ], [ 0, %2138 ], [ 3, %2136 ], [ 15, %1648 ], [ 10, %2612 ], [ 2, %2204 ], [ %2455, %2443 ], [ 0, %2456 ], [ 0, %2595 ], [ 0, %2578 ], [ 0, %2561 ], [ 0, %2457 ], [ 2, %2682 ], [ 0, %2869 ], [ 0, %2773 ], [ 0, %2722 ], [ 10, %1160 ], [ %2931, %2921 ], [ 1, %3030 ], [ 1, %3022 ], [ 1, %3014 ], [ 1, %3006 ], [ 1, %241 ], [ 1, %186 ], [ 1, %3064 ], [ 1, %3056 ], [ 1, %3048 ], [ 1, %3040 ], [ 1, %721 ], [ 1, %660 ], [ 1, %3106 ], [ 1, %3098 ], [ 1, %3090 ], [ 1, %3082 ], [ 1, %2196 ], [ 1, %2141 ], [ 1, %3140 ], [ 1, %3132 ], [ 1, %3124 ], [ 1, %3116 ], [ 1, %2674 ], [ 1, %2619 ]
  %2934 = icmp eq i32 %2933, 0
  br label %2935

; <label>:2935                                    ; preds = %2932, %87
  %2936 = phi i1 [ %2934, %2932 ], [ false, %87 ]
  %2937 = phi i32 [ %2933, %2932 ], [ %120, %87 ]
  br i1 %2936, label %2938, label %2992

; <label>:2938                                    ; preds = %2935
  %2939 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %10, i32 %77, i32 0)  ; BufferLoad(srv,index,wot)
  %2940 = extractvalue %dx.types.ResRet.i32 %2939, 0
  %2941 = add nuw nsw i32 %77, 1
  %2942 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %10, i32 %2941, i32 0)  ; BufferLoad(srv,index,wot)
  %2943 = extractvalue %dx.types.ResRet.i32 %2942, 0
  %2944 = icmp ugt i32 %2943, %2940
  %2945 = sub i32 %2943, %2940
  %2946 = select i1 %2944, i32 %2945, i32 0
  %2947 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %2948 = extractvalue %dx.types.CBufRet.i32 %2947, 0
  %2949 = shl i32 %76, 2
  %2950 = add i32 %2948, %2949
  %2951 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %5, i32 0, i32 %2950, i32 undef, i32 undef, i32 1)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2952 = icmp ult i32 %2951, %2946
  br i1 %2952, label %2953, label %2982

; <label>:2953                                    ; preds = %2938
  %2954 = shl nuw nsw i32 %46, 6
  %2955 = and i32 %40, 63
  %2956 = or i32 %2954, %2955
  %2957 = add i32 %2951, %2940
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %2957, i32 0, i32 %2956, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %2957, i32 4, i32 %42, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %2958 = shl i32 %68, 4
  %2959 = add i32 %52, %50
  %2960 = add i32 %2959, %2958
  %2961 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %2960, i32 undef)  ; BufferLoad(srv,index,wot)
  %2962 = extractvalue %dx.types.ResRet.i32 %2961, 1
  %2963 = mul nuw i32 %77, 3
  %2964 = add i32 %2963, 2
  %2965 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %2, i32 0, i32 %2964, i32 undef, i32 undef, i32 %2962)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2966 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %3, i32 0, i32 0, i32 undef, i32 undef, i32 1)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2967 = shl i32 %2966, 3
  %2968 = add i32 %2967, 8
  %2969 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2970 = extractvalue %dx.types.CBufRet.i32 %2969, 1
  %2971 = icmp ugt i32 %2968, %2970
  br i1 %2971, label %2972, label %2979

; <label>:2972                                    ; preds = %2953
  %2973 = add i32 %2963, 1
  %2974 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %2, i32 6, i32 %2973, i32 undef, i32 undef, i32 %2951)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2975 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2976 = extractvalue %dx.types.CBufRet.i32 %2975, 1
  %2977 = lshr i32 %2976, 3
  %2978 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %3, i32 6, i32 0, i32 undef, i32 undef, i32 %2977)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  br label %2992

; <label>:2979                                    ; preds = %2953
  %2980 = shl i32 %2966, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %2980, i32 undef, i32 %77, i32 %77, i32 %77, i32 %77, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %2981 = or i32 %2980, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %2981, i32 undef, i32 %2951, i32 %2951, i32 %2951, i32 %2951, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %2992

; <label>:2982                                    ; preds = %2938
  %2983 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %2984 = extractvalue %dx.types.CBufRet.i32 %2983, 0
  %2985 = add i32 %2984, %2949
  %2986 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %5, i32 6, i32 %2985, i32 undef, i32 undef, i32 %2946)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2987 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %2988 = extractvalue %dx.types.CBufRet.i32 %2987, 1
  %2989 = and i32 %2988, 15
  %2990 = icmp eq i32 %2989, 1
  %2991 = select i1 %2990, i32 3, i32 %2937
  br label %2992

; <label>:2992                                    ; preds = %2982, %2979, %2972, %2935
  %2993 = phi i32 [ %2937, %2972 ], [ %2937, %2979 ], [ %2937, %2935 ], [ %2991, %2982 ]
  %2994 = icmp eq i32 %2993, 15
  br i1 %2994, label %3005, label %2995

; <label>:2995                                    ; preds = %2992
  %2996 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %2997 = extractvalue %dx.types.CBufRet.i32 %2996, 3
  %2998 = and i32 %2997, 31
  %2999 = shl i32 15, %2998
  %3000 = xor i32 %2999, -1
  %3001 = and i32 %123, %3000
  %3002 = and i32 %2993, 15
  %3003 = shl i32 %3002, %2998
  %3004 = or i32 %3001, %3003
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %6, i32 %121, i32 0, i32 %3004, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %3005

; <label>:3005                                    ; preds = %2995, %2992, %44, %0
  ret void

; <label>:3006                                    ; preds = %241
  %3007 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %3008 = extractvalue %dx.types.CBufRet.f32 %3007, 0
  %3009 = extractvalue %dx.types.CBufRet.f32 %3007, 1
  %3010 = extractvalue %dx.types.CBufRet.f32 %3007, 2
  %3011 = extractvalue %dx.types.CBufRet.f32 %3007, 3
  %3012 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %3008, float %3009, float %3010, float %3011)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3013 = fcmp fast olt float %3012, %239
  br i1 %3013, label %2932, label %3014

; <label>:3014                                    ; preds = %3006
  %3015 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %3016 = extractvalue %dx.types.CBufRet.f32 %3015, 0
  %3017 = extractvalue %dx.types.CBufRet.f32 %3015, 1
  %3018 = extractvalue %dx.types.CBufRet.f32 %3015, 2
  %3019 = extractvalue %dx.types.CBufRet.f32 %3015, 3
  %3020 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %3016, float %3017, float %3018, float %3019)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3021 = fcmp fast olt float %3020, %239
  br i1 %3021, label %2932, label %3022

; <label>:3022                                    ; preds = %3014
  %3023 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %3024 = extractvalue %dx.types.CBufRet.f32 %3023, 0
  %3025 = extractvalue %dx.types.CBufRet.f32 %3023, 1
  %3026 = extractvalue %dx.types.CBufRet.f32 %3023, 2
  %3027 = extractvalue %dx.types.CBufRet.f32 %3023, 3
  %3028 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %3024, float %3025, float %3026, float %3027)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3029 = fcmp fast olt float %3028, %239
  br i1 %3029, label %2932, label %3030

; <label>:3030                                    ; preds = %3022
  %3031 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %3032 = extractvalue %dx.types.CBufRet.f32 %3031, 0
  %3033 = extractvalue %dx.types.CBufRet.f32 %3031, 1
  %3034 = extractvalue %dx.types.CBufRet.f32 %3031, 2
  %3035 = extractvalue %dx.types.CBufRet.f32 %3031, 3
  %3036 = call float @dx.op.dot4.f32(i32 56, float %229, float %230, float %231, float 1.000000e+00, float %3032, float %3033, float %3034, float %3035)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3037 = fcmp fast olt float %3036, %239
  br i1 %3037, label %2932, label %3038

; <label>:3038                                    ; preds = %3030
  %3039 = icmp eq i32 %85, 0
  br i1 %3039, label %289, label %249

; <label>:3040                                    ; preds = %721
  %3041 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %3042 = extractvalue %dx.types.CBufRet.f32 %3041, 0
  %3043 = extractvalue %dx.types.CBufRet.f32 %3041, 1
  %3044 = extractvalue %dx.types.CBufRet.f32 %3041, 2
  %3045 = extractvalue %dx.types.CBufRet.f32 %3041, 3
  %3046 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %3042, float %3043, float %3044, float %3045)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3047 = fcmp fast olt float %3046, %719
  br i1 %3047, label %2932, label %3048

; <label>:3048                                    ; preds = %3040
  %3049 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %3050 = extractvalue %dx.types.CBufRet.f32 %3049, 0
  %3051 = extractvalue %dx.types.CBufRet.f32 %3049, 1
  %3052 = extractvalue %dx.types.CBufRet.f32 %3049, 2
  %3053 = extractvalue %dx.types.CBufRet.f32 %3049, 3
  %3054 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %3050, float %3051, float %3052, float %3053)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3055 = fcmp fast olt float %3054, %719
  br i1 %3055, label %2932, label %3056

; <label>:3056                                    ; preds = %3048
  %3057 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %3058 = extractvalue %dx.types.CBufRet.f32 %3057, 0
  %3059 = extractvalue %dx.types.CBufRet.f32 %3057, 1
  %3060 = extractvalue %dx.types.CBufRet.f32 %3057, 2
  %3061 = extractvalue %dx.types.CBufRet.f32 %3057, 3
  %3062 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %3058, float %3059, float %3060, float %3061)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3063 = fcmp fast olt float %3062, %719
  br i1 %3063, label %2932, label %3064

; <label>:3064                                    ; preds = %3056
  %3065 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %3066 = extractvalue %dx.types.CBufRet.f32 %3065, 0
  %3067 = extractvalue %dx.types.CBufRet.f32 %3065, 1
  %3068 = extractvalue %dx.types.CBufRet.f32 %3065, 2
  %3069 = extractvalue %dx.types.CBufRet.f32 %3065, 3
  %3070 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %3066, float %3067, float %3068, float %3069)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3071 = fcmp fast olt float %3070, %719
  br i1 %3071, label %2932, label %3072

; <label>:3072                                    ; preds = %3064
  %3073 = or i32 %682, 4
  %3074 = add nuw nsw i32 %3073, 5
  %3075 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 %3074)  ; CBufferLoadLegacy(handle,regIndex)
  %3076 = extractvalue %dx.types.CBufRet.f32 %3075, 0
  %3077 = extractvalue %dx.types.CBufRet.f32 %3075, 1
  %3078 = extractvalue %dx.types.CBufRet.f32 %3075, 2
  %3079 = extractvalue %dx.types.CBufRet.f32 %3075, 3
  %3080 = call float @dx.op.dot4.f32(i32 56, float %709, float %710, float %711, float 1.000000e+00, float %3076, float %3077, float %3078, float %3079)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3081 = icmp eq i32 %85, 0
  br i1 %3081, label %769, label %729

; <label>:3082                                    ; preds = %2196
  %3083 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %3084 = extractvalue %dx.types.CBufRet.f32 %3083, 0
  %3085 = extractvalue %dx.types.CBufRet.f32 %3083, 1
  %3086 = extractvalue %dx.types.CBufRet.f32 %3083, 2
  %3087 = extractvalue %dx.types.CBufRet.f32 %3083, 3
  %3088 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %3084, float %3085, float %3086, float %3087)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3089 = fcmp fast olt float %3088, %2194
  br i1 %3089, label %2932, label %3090

; <label>:3090                                    ; preds = %3082
  %3091 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %3092 = extractvalue %dx.types.CBufRet.f32 %3091, 0
  %3093 = extractvalue %dx.types.CBufRet.f32 %3091, 1
  %3094 = extractvalue %dx.types.CBufRet.f32 %3091, 2
  %3095 = extractvalue %dx.types.CBufRet.f32 %3091, 3
  %3096 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %3092, float %3093, float %3094, float %3095)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3097 = fcmp fast olt float %3096, %2194
  br i1 %3097, label %2932, label %3098

; <label>:3098                                    ; preds = %3090
  %3099 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %3100 = extractvalue %dx.types.CBufRet.f32 %3099, 0
  %3101 = extractvalue %dx.types.CBufRet.f32 %3099, 1
  %3102 = extractvalue %dx.types.CBufRet.f32 %3099, 2
  %3103 = extractvalue %dx.types.CBufRet.f32 %3099, 3
  %3104 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %3100, float %3101, float %3102, float %3103)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3105 = fcmp fast olt float %3104, %2194
  br i1 %3105, label %2932, label %3106

; <label>:3106                                    ; preds = %3098
  %3107 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %3108 = extractvalue %dx.types.CBufRet.f32 %3107, 0
  %3109 = extractvalue %dx.types.CBufRet.f32 %3107, 1
  %3110 = extractvalue %dx.types.CBufRet.f32 %3107, 2
  %3111 = extractvalue %dx.types.CBufRet.f32 %3107, 3
  %3112 = call float @dx.op.dot4.f32(i32 56, float %2184, float %2185, float %2186, float 1.000000e+00, float %3108, float %3109, float %3110, float %3111)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3113 = fcmp fast olt float %3112, %2194
  br i1 %3113, label %2932, label %3114

; <label>:3114                                    ; preds = %3106
  %3115 = icmp eq i32 %85, 0
  br i1 %3115, label %2244, label %2204

; <label>:3116                                    ; preds = %2674
  %3117 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %3118 = extractvalue %dx.types.CBufRet.f32 %3117, 0
  %3119 = extractvalue %dx.types.CBufRet.f32 %3117, 1
  %3120 = extractvalue %dx.types.CBufRet.f32 %3117, 2
  %3121 = extractvalue %dx.types.CBufRet.f32 %3117, 3
  %3122 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %3118, float %3119, float %3120, float %3121)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3123 = fcmp fast olt float %3122, %2672
  br i1 %3123, label %2932, label %3124

; <label>:3124                                    ; preds = %3116
  %3125 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %3126 = extractvalue %dx.types.CBufRet.f32 %3125, 0
  %3127 = extractvalue %dx.types.CBufRet.f32 %3125, 1
  %3128 = extractvalue %dx.types.CBufRet.f32 %3125, 2
  %3129 = extractvalue %dx.types.CBufRet.f32 %3125, 3
  %3130 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %3126, float %3127, float %3128, float %3129)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3131 = fcmp fast olt float %3130, %2672
  br i1 %3131, label %2932, label %3132

; <label>:3132                                    ; preds = %3124
  %3133 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %3134 = extractvalue %dx.types.CBufRet.f32 %3133, 0
  %3135 = extractvalue %dx.types.CBufRet.f32 %3133, 1
  %3136 = extractvalue %dx.types.CBufRet.f32 %3133, 2
  %3137 = extractvalue %dx.types.CBufRet.f32 %3133, 3
  %3138 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %3134, float %3135, float %3136, float %3137)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3139 = fcmp fast olt float %3138, %2672
  br i1 %3139, label %2932, label %3140

; <label>:3140                                    ; preds = %3132
  %3141 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %3142 = extractvalue %dx.types.CBufRet.f32 %3141, 0
  %3143 = extractvalue %dx.types.CBufRet.f32 %3141, 1
  %3144 = extractvalue %dx.types.CBufRet.f32 %3141, 2
  %3145 = extractvalue %dx.types.CBufRet.f32 %3141, 3
  %3146 = call float @dx.op.dot4.f32(i32 56, float %2662, float %2663, float %2664, float 1.000000e+00, float %3142, float %3143, float %3144, float %3145)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %3147 = fcmp fast olt float %3146, %2672
  br i1 %3147, label %2932, label %3148

; <label>:3148                                    ; preds = %3140
  %3149 = icmp eq i32 %85, 0
  br i1 %3149, label %2722, label %2682
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind
declare i32 @dx.op.atomicBinOp.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32, %dx.types.Handle, i32, i32) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!32}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 5}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !21, !28, !30}
!5 = !{!6, !8, !9, !11, !13, !14, !16, !17, !19}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, [16 x %struct.ByteAddressBuffer]* undef, !"", i32 0, i32 1, i32 16, i32 11, i32 0, null}
!9 = !{i32 2, %"class.StructuredBuffer<S_G1M_OFFSET>"* undef, !"", i32 0, i32 17, i32 1, i32 12, i32 0, !10}
!10 = !{i32 1, i32 48}
!11 = !{i32 3, %"class.StructuredBuffer<vector<float, 4> >"* undef, !"", i32 0, i32 18, i32 1, i32 12, i32 0, !12}
!12 = !{i32 1, i32 16}
!13 = !{i32 4, %"class.StructuredBuffer<S_INSTANCE_OFFSET>"* undef, !"", i32 0, i32 19, i32 1, i32 12, i32 0, !12}
!14 = !{i32 5, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 21, i32 1, i32 12, i32 0, !15}
!15 = !{i32 1, i32 4}
!16 = !{i32 6, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 22, i32 1, i32 12, i32 0, !15}
!17 = !{i32 7, %"class.StructuredBuffer<S_PAYLOAD_TO_MSHLTCULL>"* undef, !"", i32 0, i32 23, i32 1, i32 12, i32 0, !18}
!18 = !{i32 1, i32 8}
!19 = !{i32 8, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 24, i32 1, i32 10, i32 0, !20}
!20 = !{i32 0, i32 5}
!21 = !{!22, !23, !24, !25, !26, !27}
!22 = !{i32 0, %"class.RWStructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 0, i32 1, i32 12, i1 false, i1 false, i1 false, !15}
!23 = !{i32 1, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 2, i32 1, i32 10, i1 false, i1 false, i1 false, !20}
!24 = !{i32 2, %"class.RWStructuredBuffer<S_PAYLOAD_TO_MSHLTDRAW>"* undef, !"", i32 0, i32 3, i32 1, i32 12, i1 false, i1 false, i1 false, !18}
!25 = !{i32 3, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 9, i32 1, i32 10, i1 false, i1 false, i1 false, !20}
!26 = !{i32 4, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 11, i32 1, i32 10, i1 false, i1 false, i1 false, !20}
!27 = !{i32 5, %"class.RWBuffer<unsigned int>"* undef, !"", i32 0, i32 13, i32 1, i32 10, i1 false, i1 false, i1 false, !20}
!28 = !{!29}
!29 = !{i32 0, %"dx.alignment.legacy.$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 384, null}
!30 = !{!31}
!31 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 0, i32 1, i32 0, null}
!32 = !{void ()* @main, !"main", null, !4, !33}
!33 = !{i32 0, i64 16, i32 4, !34}
!34 = !{i32 32, i32 1, i32 1}

