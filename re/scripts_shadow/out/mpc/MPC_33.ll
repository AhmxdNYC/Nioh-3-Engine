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
; shader debug name: d3e6df6a6164d28d7c0f8ff4ec25f69d.pdb
; shader hash: d3e6df6a6164d28d7c0f8ff4ec25f69d
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
  br i1 %43, label %2612, label %44

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
  br i1 %86, label %2612, label %87

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
  br i1 %124, label %125, label %2542

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
  %160 = extractvalue %dx.types.ResRet.i32 %159, 1
  %161 = extractvalue %dx.types.ResRet.i32 %159, 2
  %162 = extractvalue %dx.types.ResRet.i32 %159, 3
  %163 = bitcast i32 %160 to float
  %164 = bitcast i32 %161 to float
  %165 = bitcast i32 %162 to float
  %166 = add i32 %50, 48
  %167 = add i32 %166, %54
  %168 = add i32 %167, %132
  %169 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %168, i32 undef)  ; BufferLoad(srv,index,wot)
  %170 = extractvalue %dx.types.ResRet.i32 %169, 0
  %171 = extractvalue %dx.types.ResRet.i32 %169, 1
  %172 = extractvalue %dx.types.ResRet.i32 %169, 2
  %173 = bitcast i32 %170 to float
  %174 = bitcast i32 %171 to float
  %175 = bitcast i32 %172 to float
  %176 = add nuw nsw i32 %130, %131
  %177 = mul nuw nsw i32 %176, 3
  %178 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %11, i32 %94, i32 0)  ; BufferLoad(srv,index,wot)
  %179 = extractvalue %dx.types.ResRet.i32 %178, 0
  %180 = and i32 %80, 256
  %181 = icmp ne i32 %180, 0
  %182 = and i32 %80, 15
  %183 = icmp eq i32 %182, 0
  br i1 %183, label %184, label %545

; <label>:184                                     ; preds = %125
  %185 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %186 = extractvalue %dx.types.CBufRet.f32 %185, 0
  %187 = extractvalue %dx.types.CBufRet.f32 %185, 1
  %188 = extractvalue %dx.types.CBufRet.f32 %185, 3
  %189 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %190 = extractvalue %dx.types.CBufRet.f32 %189, 0
  %191 = extractvalue %dx.types.CBufRet.f32 %189, 1
  %192 = extractvalue %dx.types.CBufRet.f32 %189, 3
  %193 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %194 = extractvalue %dx.types.CBufRet.f32 %193, 0
  %195 = extractvalue %dx.types.CBufRet.f32 %193, 1
  %196 = extractvalue %dx.types.CBufRet.f32 %193, 3
  %197 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %198 = extractvalue %dx.types.CBufRet.f32 %197, 0
  %199 = extractvalue %dx.types.CBufRet.f32 %197, 1
  %200 = extractvalue %dx.types.CBufRet.f32 %197, 3
  %201 = add i32 %179, %177
  %202 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %201, i32 0)  ; BufferLoad(srv,index,wot)
  %203 = extractvalue %dx.types.ResRet.f32 %202, 0
  %204 = extractvalue %dx.types.ResRet.f32 %202, 1
  %205 = extractvalue %dx.types.ResRet.f32 %202, 2
  %206 = extractvalue %dx.types.ResRet.f32 %202, 3
  %207 = add nuw nsw i32 %177, 1
  %208 = add i32 %207, %179
  %209 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %208, i32 0)  ; BufferLoad(srv,index,wot)
  %210 = extractvalue %dx.types.ResRet.f32 %209, 0
  %211 = extractvalue %dx.types.ResRet.f32 %209, 1
  %212 = extractvalue %dx.types.ResRet.f32 %209, 2
  %213 = extractvalue %dx.types.ResRet.f32 %209, 3
  %214 = add nuw nsw i32 %177, 2
  %215 = add i32 %214, %179
  %216 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %215, i32 0)  ; BufferLoad(srv,index,wot)
  %217 = extractvalue %dx.types.ResRet.f32 %216, 0
  %218 = extractvalue %dx.types.ResRet.f32 %216, 1
  %219 = extractvalue %dx.types.ResRet.f32 %216, 2
  %220 = extractvalue %dx.types.ResRet.f32 %216, 3
  %221 = call float @dx.op.dot3.f32(i32 55, float %217, float %218, float %219, float %217, float %218, float %219)  ; Dot3(ax,ay,az,bx,by,bz)
  %222 = call float @dx.op.dot3.f32(i32 55, float %210, float %211, float %212, float %210, float %211, float %212)  ; Dot3(ax,ay,az,bx,by,bz)
  %223 = call float @dx.op.binary.f32(i32 35, float %222, float %221)  ; FMax(a,b)
  %224 = call float @dx.op.dot3.f32(i32 55, float %203, float %204, float %205, float %203, float %204, float %205)  ; Dot3(ax,ay,az,bx,by,bz)
  %225 = call float @dx.op.binary.f32(i32 35, float %224, float %223)  ; FMax(a,b)
  %226 = call float @dx.op.unary.f32(i32 24, float %225)  ; Sqrt(value)
  %227 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %203, float %204, float %205, float %206)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %228 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %210, float %211, float %212, float %213)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %229 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %217, float %218, float %219, float %220)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %230 = fmul fast float %226, %143
  %231 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %232 = extractvalue %dx.types.CBufRet.f32 %231, 0
  %233 = extractvalue %dx.types.CBufRet.f32 %231, 1
  %234 = extractvalue %dx.types.CBufRet.f32 %231, 2
  %235 = extractvalue %dx.types.CBufRet.f32 %231, 3
  %236 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %232, float %233, float %234, float %235)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %237 = fsub fast float -0.000000e+00, %230
  %238 = fcmp fast olt float %236, %237
  br i1 %238, label %2539, label %239

; <label>:239                                     ; preds = %184
  %240 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %241 = extractvalue %dx.types.CBufRet.f32 %240, 0
  %242 = extractvalue %dx.types.CBufRet.f32 %240, 1
  %243 = extractvalue %dx.types.CBufRet.f32 %240, 2
  %244 = extractvalue %dx.types.CBufRet.f32 %240, 3
  %245 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %241, float %242, float %243, float %244)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %246 = fcmp fast olt float %245, %237
  br i1 %246, label %2539, label %2613

; <label>:247                                     ; preds = %2645
  %248 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %203, float %204, float %205)  ; Dot3(ax,ay,az,bx,by,bz)
  %249 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %210, float %211, float %212)  ; Dot3(ax,ay,az,bx,by,bz)
  %250 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %217, float %218, float %219)  ; Dot3(ax,ay,az,bx,by,bz)
  %251 = call float @dx.op.dot3.f32(i32 55, float %248, float %249, float %250, float %248, float %249, float %250)  ; Dot3(ax,ay,az,bx,by,bz)
  %252 = call float @dx.op.unary.f32(i32 25, float %251)  ; Rsqrt(value)
  %253 = fmul fast float %252, %248
  %254 = fmul fast float %252, %249
  %255 = fmul fast float %252, %250
  %256 = icmp eq i32 %85, 2
  %257 = fsub fast float -0.000000e+00, %253
  %258 = fsub fast float -0.000000e+00, %254
  %259 = fsub fast float -0.000000e+00, %255
  %260 = select i1 %256, float %257, float %253
  %261 = select i1 %256, float %258, float %254
  %262 = select i1 %256, float %259, float %255
  %263 = fsub fast float -0.000000e+00, %2631
  %264 = fsub fast float -0.000000e+00, %2632
  %265 = fsub fast float -0.000000e+00, %2633
  %266 = call float @dx.op.dot3.f32(i32 55, float %263, float %264, float %265, float %263, float %264, float %265)  ; Dot3(ax,ay,az,bx,by,bz)
  %267 = call float @dx.op.unary.f32(i32 25, float %266)  ; Rsqrt(value)
  %268 = fmul fast float %267, %263
  %269 = fmul fast float %267, %264
  %270 = fmul fast float %267, %265
  %271 = fsub fast float -0.000000e+00, %260
  %272 = fsub fast float -0.000000e+00, %261
  %273 = fsub fast float -0.000000e+00, %262
  %274 = call float @dx.op.dot3.f32(i32 55, float %268, float %269, float %270, float %271, float %272, float %273)  ; Dot3(ax,ay,az,bx,by,bz)
  %275 = fcmp fast ogt float %274, %155
  br i1 %275, label %2539, label %276

; <label>:276                                     ; preds = %2645, %247
  %277 = and i32 %80, 512
  %278 = icmp eq i32 %277, 0
  br i1 %278, label %485, label %279

; <label>:279                                     ; preds = %276
  br label %280

; <label>:280                                     ; preds = %280, %279
  %281 = phi i32 [ %325, %280 ], [ 0, %279 ]
  %282 = and i32 %281, 1
  %283 = icmp ne i32 %282, 0
  %284 = select i1 %283, float %173, float %163
  %285 = and i32 %281, 2
  %286 = icmp ne i32 %285, 0
  %287 = select i1 %286, float %174, float %164
  %288 = and i32 %281, 4
  %289 = icmp ne i32 %288, 0
  %290 = select i1 %289, float %175, float %165
  %291 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %201, i32 0)  ; BufferLoad(srv,index,wot)
  %292 = extractvalue %dx.types.ResRet.f32 %291, 0
  %293 = extractvalue %dx.types.ResRet.f32 %291, 1
  %294 = extractvalue %dx.types.ResRet.f32 %291, 2
  %295 = extractvalue %dx.types.ResRet.f32 %291, 3
  %296 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %208, i32 0)  ; BufferLoad(srv,index,wot)
  %297 = extractvalue %dx.types.ResRet.f32 %296, 0
  %298 = extractvalue %dx.types.ResRet.f32 %296, 1
  %299 = extractvalue %dx.types.ResRet.f32 %296, 2
  %300 = extractvalue %dx.types.ResRet.f32 %296, 3
  %301 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %215, i32 0)  ; BufferLoad(srv,index,wot)
  %302 = extractvalue %dx.types.ResRet.f32 %301, 0
  %303 = extractvalue %dx.types.ResRet.f32 %301, 1
  %304 = extractvalue %dx.types.ResRet.f32 %301, 2
  %305 = extractvalue %dx.types.ResRet.f32 %301, 3
  %306 = call float @dx.op.dot4.f32(i32 56, float %284, float %287, float %290, float 1.000000e+00, float %292, float %293, float %294, float %295)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %307 = call float @dx.op.dot4.f32(i32 56, float %284, float %287, float %290, float 1.000000e+00, float %297, float %298, float %299, float %300)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %308 = call float @dx.op.dot4.f32(i32 56, float %284, float %287, float %290, float 1.000000e+00, float %302, float %303, float %304, float %305)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %309 = fmul fast float %306, %186
  %310 = call float @dx.op.tertiary.f32(i32 46, float %307, float %190, float %309)  ; FMad(a,b,c)
  %311 = call float @dx.op.tertiary.f32(i32 46, float %308, float %194, float %310)  ; FMad(a,b,c)
  %312 = fadd fast float %311, %198
  %313 = fmul fast float %306, %187
  %314 = call float @dx.op.tertiary.f32(i32 46, float %307, float %191, float %313)  ; FMad(a,b,c)
  %315 = call float @dx.op.tertiary.f32(i32 46, float %308, float %195, float %314)  ; FMad(a,b,c)
  %316 = fadd fast float %315, %199
  %317 = fmul fast float %306, %188
  %318 = call float @dx.op.tertiary.f32(i32 46, float %307, float %192, float %317)  ; FMad(a,b,c)
  %319 = call float @dx.op.tertiary.f32(i32 46, float %308, float %196, float %318)  ; FMad(a,b,c)
  %320 = fadd fast float %319, %200
  %321 = fdiv fast float %312, %320
  %322 = fdiv fast float %316, %320
  %323 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 %281
  %324 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 %281
  store float %321, float* %323, align 4
  store float %322, float* %324, align 4
  %325 = add nuw nsw i32 %281, 1
  %326 = icmp eq i32 %325, 8
  br i1 %326, label %327, label %280

; <label>:327                                     ; preds = %280
  %328 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 3
  %329 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 3
  %330 = load float, float* %328, align 4
  %331 = load float, float* %329, align 4
  %332 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 2
  %333 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 2
  %334 = load float, float* %332, align 4
  %335 = load float, float* %333, align 4
  %336 = call float @dx.op.binary.f32(i32 36, float %334, float %330)  ; FMin(a,b)
  %337 = call float @dx.op.binary.f32(i32 36, float %335, float %331)  ; FMin(a,b)
  %338 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 1
  %339 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 1
  %340 = load float, float* %338, align 4
  %341 = load float, float* %339, align 4
  %342 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 0
  %343 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 0
  %344 = load float, float* %342, align 4
  %345 = load float, float* %343, align 4
  %346 = call float @dx.op.binary.f32(i32 36, float %344, float %340)  ; FMin(a,b)
  %347 = call float @dx.op.binary.f32(i32 36, float %345, float %341)  ; FMin(a,b)
  %348 = call float @dx.op.binary.f32(i32 36, float %346, float %336)  ; FMin(a,b)
  %349 = call float @dx.op.binary.f32(i32 36, float %347, float %337)  ; FMin(a,b)
  %350 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 7
  %351 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 7
  %352 = load float, float* %350, align 4
  %353 = load float, float* %351, align 4
  %354 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 6
  %355 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 6
  %356 = load float, float* %354, align 4
  %357 = load float, float* %355, align 4
  %358 = call float @dx.op.binary.f32(i32 36, float %356, float %352)  ; FMin(a,b)
  %359 = call float @dx.op.binary.f32(i32 36, float %357, float %353)  ; FMin(a,b)
  %360 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 5
  %361 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 5
  %362 = load float, float* %360, align 4
  %363 = load float, float* %361, align 4
  %364 = getelementptr [8 x float], [8 x float]* %24, i32 0, i32 4
  %365 = getelementptr [8 x float], [8 x float]* %25, i32 0, i32 4
  %366 = load float, float* %364, align 4
  %367 = load float, float* %365, align 4
  %368 = call float @dx.op.binary.f32(i32 36, float %366, float %362)  ; FMin(a,b)
  %369 = call float @dx.op.binary.f32(i32 36, float %367, float %363)  ; FMin(a,b)
  %370 = call float @dx.op.binary.f32(i32 36, float %368, float %358)  ; FMin(a,b)
  %371 = call float @dx.op.binary.f32(i32 36, float %369, float %359)  ; FMin(a,b)
  %372 = call float @dx.op.binary.f32(i32 36, float %348, float %370)  ; FMin(a,b)
  %373 = call float @dx.op.binary.f32(i32 36, float %349, float %371)  ; FMin(a,b)
  %374 = call float @dx.op.binary.f32(i32 35, float %334, float %330)  ; FMax(a,b)
  %375 = call float @dx.op.binary.f32(i32 35, float %335, float %331)  ; FMax(a,b)
  %376 = call float @dx.op.binary.f32(i32 35, float %344, float %340)  ; FMax(a,b)
  %377 = call float @dx.op.binary.f32(i32 35, float %345, float %341)  ; FMax(a,b)
  %378 = call float @dx.op.binary.f32(i32 35, float %376, float %374)  ; FMax(a,b)
  %379 = call float @dx.op.binary.f32(i32 35, float %377, float %375)  ; FMax(a,b)
  %380 = call float @dx.op.binary.f32(i32 35, float %356, float %352)  ; FMax(a,b)
  %381 = call float @dx.op.binary.f32(i32 35, float %357, float %353)  ; FMax(a,b)
  %382 = call float @dx.op.binary.f32(i32 35, float %366, float %362)  ; FMax(a,b)
  %383 = call float @dx.op.binary.f32(i32 35, float %367, float %363)  ; FMax(a,b)
  %384 = call float @dx.op.binary.f32(i32 35, float %382, float %380)  ; FMax(a,b)
  %385 = call float @dx.op.binary.f32(i32 35, float %383, float %381)  ; FMax(a,b)
  %386 = call float @dx.op.binary.f32(i32 35, float %378, float %384)  ; FMax(a,b)
  %387 = call float @dx.op.binary.f32(i32 35, float %379, float %385)  ; FMax(a,b)
  %388 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %389 = extractvalue %dx.types.CBufRet.i32 %388, 0
  %390 = and i32 %389, 15
  %391 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %392 = extractvalue %dx.types.CBufRet.f32 %391, 0
  %393 = extractvalue %dx.types.CBufRet.f32 %391, 1
  %394 = extractvalue %dx.types.CBufRet.f32 %391, 2
  %395 = extractvalue %dx.types.CBufRet.f32 %391, 3
  %396 = fmul fast float %372, 5.000000e-01
  %397 = fmul fast float %387, 5.000000e-01
  %398 = fadd fast float %396, 5.000000e-01
  %399 = fsub fast float 5.000000e-01, %397
  %400 = fmul fast float %386, 5.000000e-01
  %401 = fmul fast float %373, 5.000000e-01
  %402 = fadd fast float %400, 5.000000e-01
  %403 = fsub fast float 5.000000e-01, %401
  %404 = fmul fast float %392, %398
  %405 = fmul fast float %393, %399
  %406 = fadd fast float %404, %394
  %407 = fadd fast float %405, %395
  %408 = fmul fast float %392, %402
  %409 = fmul fast float %393, %403
  %410 = fadd fast float %408, %394
  %411 = fadd fast float %409, %395
  %412 = call float @dx.op.binary.f32(i32 35, float %406, float 0.000000e+00)  ; FMax(a,b)
  %413 = call float @dx.op.binary.f32(i32 35, float %407, float 0.000000e+00)  ; FMax(a,b)
  %414 = call float @dx.op.binary.f32(i32 36, float %412, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %415 = call float @dx.op.binary.f32(i32 36, float %413, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %416 = call float @dx.op.binary.f32(i32 35, float %410, float 0.000000e+00)  ; FMax(a,b)
  %417 = call float @dx.op.binary.f32(i32 35, float %411, float 0.000000e+00)  ; FMax(a,b)
  %418 = call float @dx.op.binary.f32(i32 36, float %416, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %419 = call float @dx.op.binary.f32(i32 36, float %417, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %420 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %421 = extractvalue %dx.types.ResRet.i32 %420, 0
  %422 = icmp slt i32 %421, 1
  br i1 %422, label %485, label %423

; <label>:423                                     ; preds = %327
  %424 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %425 = extractvalue %dx.types.ResRet.i32 %424, 0
  %426 = icmp ult i32 %390, %425
  br i1 %426, label %427, label %485

; <label>:427                                     ; preds = %423
  %428 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %429 = extractvalue %dx.types.ResRet.i32 %428, 0
  %430 = uitofp i32 %421 to float
  %431 = uitofp i32 %429 to float
  %432 = fmul fast float %430, %414
  %433 = fmul fast float %431, %415
  %434 = fptoui float %432 to i32
  %435 = fptoui float %433 to i32
  %436 = mul i32 %435, %421
  %437 = add i32 %436, %434
  %438 = fmul fast float %430, %418
  %439 = fptoui float %438 to i32
  %440 = add i32 %436, %439
  %441 = fmul fast float %431, %419
  %442 = fptoui float %441 to i32
  %443 = mul i32 %442, %421
  %444 = add i32 %443, %434
  %445 = mul i32 %421, %390
  %446 = mul i32 %445, %429
  %447 = add nsw i32 %446, 3
  %448 = icmp sge i32 %437, %421
  %449 = srem i32 %437, %421
  %450 = icmp ne i32 %449, 0
  %451 = and i1 %450, %448
  br i1 %451, label %452, label %458

; <label>:452                                     ; preds = %427
  %453 = xor i32 %421, -1
  %454 = add i32 %447, %453
  %455 = add i32 %454, %437
  %456 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %455, i32 undef)  ; BufferLoad(srv,index,wot)
  %457 = extractvalue %dx.types.ResRet.i32 %456, 0
  br label %458

; <label>:458                                     ; preds = %452, %427
  %459 = phi i32 [ %457, %452 ], [ 0, %427 ]
  %460 = icmp slt i32 %440, %421
  br i1 %460, label %466, label %461

; <label>:461                                     ; preds = %458
  %462 = sub i32 %447, %421
  %463 = add i32 %462, %440
  %464 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %463, i32 undef)  ; BufferLoad(srv,index,wot)
  %465 = extractvalue %dx.types.ResRet.i32 %464, 0
  br label %466

; <label>:466                                     ; preds = %461, %458
  %467 = phi i32 [ %465, %461 ], [ 0, %458 ]
  %468 = srem i32 %444, %421
  %469 = icmp eq i32 %468, 0
  br i1 %469, label %475, label %470

; <label>:470                                     ; preds = %466
  %471 = add i32 %446, 2
  %472 = add i32 %471, %444
  %473 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %472, i32 undef)  ; BufferLoad(srv,index,wot)
  %474 = extractvalue %dx.types.ResRet.i32 %473, 0
  br label %475

; <label>:475                                     ; preds = %470, %466
  %476 = phi i32 [ %474, %470 ], [ 0, %466 ]
  %477 = add i32 %447, %439
  %478 = add i32 %477, %443
  %479 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %478, i32 undef)  ; BufferLoad(srv,index,wot)
  %480 = extractvalue %dx.types.ResRet.i32 %479, 0
  %481 = add i32 %476, %467
  %482 = sub i32 %480, %481
  %483 = sub i32 0, %459
  %484 = icmp eq i32 %482, %483
  br i1 %484, label %2539, label %485

; <label>:485                                     ; preds = %475, %423, %327, %276
  br i1 %181, label %486, label %544

; <label>:486                                     ; preds = %485
  %487 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %201, i32 0)  ; BufferLoad(srv,index,wot)
  %488 = extractvalue %dx.types.ResRet.f32 %487, 0
  %489 = extractvalue %dx.types.ResRet.f32 %487, 1
  %490 = extractvalue %dx.types.ResRet.f32 %487, 2
  %491 = extractvalue %dx.types.ResRet.f32 %487, 3
  %492 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %208, i32 0)  ; BufferLoad(srv,index,wot)
  %493 = extractvalue %dx.types.ResRet.f32 %492, 0
  %494 = extractvalue %dx.types.ResRet.f32 %492, 1
  %495 = extractvalue %dx.types.ResRet.f32 %492, 2
  %496 = extractvalue %dx.types.ResRet.f32 %492, 3
  %497 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %215, i32 0)  ; BufferLoad(srv,index,wot)
  %498 = extractvalue %dx.types.ResRet.f32 %497, 0
  %499 = extractvalue %dx.types.ResRet.f32 %497, 1
  %500 = extractvalue %dx.types.ResRet.f32 %497, 2
  %501 = extractvalue %dx.types.ResRet.f32 %497, 3
  %502 = call float @dx.op.dot3.f32(i32 55, float %498, float %499, float %500, float %498, float %499, float %500)  ; Dot3(ax,ay,az,bx,by,bz)
  %503 = call float @dx.op.dot3.f32(i32 55, float %493, float %494, float %495, float %493, float %494, float %495)  ; Dot3(ax,ay,az,bx,by,bz)
  %504 = call float @dx.op.binary.f32(i32 35, float %503, float %502)  ; FMax(a,b)
  %505 = call float @dx.op.dot3.f32(i32 55, float %488, float %489, float %490, float %488, float %489, float %490)  ; Dot3(ax,ay,az,bx,by,bz)
  %506 = call float @dx.op.binary.f32(i32 35, float %505, float %504)  ; FMax(a,b)
  %507 = call float @dx.op.unary.f32(i32 24, float %506)  ; Sqrt(value)
  %508 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %488, float %489, float %490, float %491)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %509 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %493, float %494, float %495, float %496)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %510 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %498, float %499, float %500, float %501)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %511 = fmul fast float %507, %143
  %512 = fmul fast float %508, %186
  %513 = call float @dx.op.tertiary.f32(i32 46, float %509, float %190, float %512)  ; FMad(a,b,c)
  %514 = call float @dx.op.tertiary.f32(i32 46, float %510, float %194, float %513)  ; FMad(a,b,c)
  %515 = fadd fast float %514, %198
  %516 = fmul fast float %508, %187
  %517 = call float @dx.op.tertiary.f32(i32 46, float %509, float %191, float %516)  ; FMad(a,b,c)
  %518 = call float @dx.op.tertiary.f32(i32 46, float %510, float %195, float %517)  ; FMad(a,b,c)
  %519 = fadd fast float %518, %199
  %520 = fmul fast float %515, 5.000000e-01
  %521 = fmul fast float %519, 5.000000e-01
  %522 = fadd fast float %520, 5.000000e-01
  %523 = fadd fast float %521, 5.000000e-01
  %524 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %525 = extractvalue %dx.types.CBufRet.f32 %524, 0
  %526 = extractvalue %dx.types.CBufRet.f32 %524, 1
  %527 = fmul fast float %525, %522
  %528 = fmul fast float %523, %526
  %529 = call float @dx.op.unary.f32(i32 27, float %527)  ; Round_ni(value)
  %530 = fadd fast float %529, 5.000000e-01
  %531 = call float @dx.op.unary.f32(i32 27, float %528)  ; Round_ni(value)
  %532 = fadd fast float %531, 5.000000e-01
  %533 = fsub fast float %530, %527
  %534 = extractvalue %dx.types.CBufRet.f32 %524, 2
  %535 = fmul fast float %534, %533
  %536 = fsub fast float %532, %528
  %537 = extractvalue %dx.types.CBufRet.f32 %524, 3
  %538 = fmul fast float %537, %536
  %539 = fmul fast float %535, %535
  %540 = fmul fast float %538, %538
  %541 = fadd fast float %540, %539
  %542 = fmul fast float %511, %511
  %543 = fcmp uge float %541, %542
  br i1 %543, label %2539, label %544

; <label>:544                                     ; preds = %486, %485
  br label %2539

; <label>:545                                     ; preds = %125
  %546 = icmp eq i32 %182, 1
  br i1 %546, label %547, label %1400

; <label>:547                                     ; preds = %545
  %548 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %549 = extractvalue %dx.types.CBufRet.f32 %548, 0
  %550 = extractvalue %dx.types.CBufRet.f32 %548, 1
  %551 = extractvalue %dx.types.CBufRet.f32 %548, 2
  %552 = extractvalue %dx.types.CBufRet.f32 %548, 3
  %553 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %554 = extractvalue %dx.types.CBufRet.f32 %553, 0
  %555 = extractvalue %dx.types.CBufRet.f32 %553, 1
  %556 = extractvalue %dx.types.CBufRet.f32 %553, 2
  %557 = extractvalue %dx.types.CBufRet.f32 %553, 3
  %558 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %559 = extractvalue %dx.types.CBufRet.f32 %558, 0
  %560 = extractvalue %dx.types.CBufRet.f32 %558, 1
  %561 = extractvalue %dx.types.CBufRet.f32 %558, 2
  %562 = extractvalue %dx.types.CBufRet.f32 %558, 3
  %563 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %564 = extractvalue %dx.types.CBufRet.f32 %563, 0
  %565 = extractvalue %dx.types.CBufRet.f32 %563, 1
  %566 = extractvalue %dx.types.CBufRet.f32 %563, 2
  %567 = extractvalue %dx.types.CBufRet.f32 %563, 3
  %568 = extractvalue %dx.types.CBufRet.i32 %79, 0
  %569 = and i32 %568, 1
  %570 = add i32 %179, %177
  %571 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %570, i32 0)  ; BufferLoad(srv,index,wot)
  %572 = extractvalue %dx.types.ResRet.f32 %571, 0
  %573 = extractvalue %dx.types.ResRet.f32 %571, 1
  %574 = extractvalue %dx.types.ResRet.f32 %571, 2
  %575 = extractvalue %dx.types.ResRet.f32 %571, 3
  %576 = add nuw nsw i32 %177, 1
  %577 = add i32 %576, %179
  %578 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %577, i32 0)  ; BufferLoad(srv,index,wot)
  %579 = extractvalue %dx.types.ResRet.f32 %578, 0
  %580 = extractvalue %dx.types.ResRet.f32 %578, 1
  %581 = extractvalue %dx.types.ResRet.f32 %578, 2
  %582 = extractvalue %dx.types.ResRet.f32 %578, 3
  %583 = add nuw nsw i32 %177, 2
  %584 = add i32 %583, %179
  %585 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %584, i32 0)  ; BufferLoad(srv,index,wot)
  %586 = extractvalue %dx.types.ResRet.f32 %585, 0
  %587 = extractvalue %dx.types.ResRet.f32 %585, 1
  %588 = extractvalue %dx.types.ResRet.f32 %585, 2
  %589 = extractvalue %dx.types.ResRet.f32 %585, 3
  %590 = call float @dx.op.dot3.f32(i32 55, float %586, float %587, float %588, float %586, float %587, float %588)  ; Dot3(ax,ay,az,bx,by,bz)
  %591 = call float @dx.op.dot3.f32(i32 55, float %579, float %580, float %581, float %579, float %580, float %581)  ; Dot3(ax,ay,az,bx,by,bz)
  %592 = call float @dx.op.binary.f32(i32 35, float %591, float %590)  ; FMax(a,b)
  %593 = call float @dx.op.dot3.f32(i32 55, float %572, float %573, float %574, float %572, float %573, float %574)  ; Dot3(ax,ay,az,bx,by,bz)
  %594 = call float @dx.op.binary.f32(i32 35, float %593, float %592)  ; FMax(a,b)
  %595 = call float @dx.op.unary.f32(i32 24, float %594)  ; Sqrt(value)
  %596 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %572, float %573, float %574, float %575)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %597 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %579, float %580, float %581, float %582)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %598 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %586, float %587, float %588, float %589)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %599 = fmul fast float %595, %143
  %600 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %601 = extractvalue %dx.types.CBufRet.f32 %600, 0
  %602 = extractvalue %dx.types.CBufRet.f32 %600, 1
  %603 = extractvalue %dx.types.CBufRet.f32 %600, 2
  %604 = extractvalue %dx.types.CBufRet.f32 %600, 3
  %605 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %601, float %602, float %603, float %604)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %606 = fsub fast float -0.000000e+00, %599
  %607 = fcmp fast olt float %605, %606
  br i1 %607, label %2539, label %608

; <label>:608                                     ; preds = %547
  %609 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %610 = extractvalue %dx.types.CBufRet.f32 %609, 0
  %611 = extractvalue %dx.types.CBufRet.f32 %609, 1
  %612 = extractvalue %dx.types.CBufRet.f32 %609, 2
  %613 = extractvalue %dx.types.CBufRet.f32 %609, 3
  %614 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %610, float %611, float %612, float %613)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %615 = fcmp fast olt float %614, %606
  br i1 %615, label %2539, label %2647

; <label>:616                                     ; preds = %2679
  %617 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %572, float %573, float %574)  ; Dot3(ax,ay,az,bx,by,bz)
  %618 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %579, float %580, float %581)  ; Dot3(ax,ay,az,bx,by,bz)
  %619 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %586, float %587, float %588)  ; Dot3(ax,ay,az,bx,by,bz)
  %620 = call float @dx.op.dot3.f32(i32 55, float %617, float %618, float %619, float %617, float %618, float %619)  ; Dot3(ax,ay,az,bx,by,bz)
  %621 = call float @dx.op.unary.f32(i32 25, float %620)  ; Rsqrt(value)
  %622 = fmul fast float %621, %617
  %623 = fmul fast float %621, %618
  %624 = fmul fast float %621, %619
  %625 = icmp eq i32 %85, 2
  %626 = fsub fast float -0.000000e+00, %622
  %627 = fsub fast float -0.000000e+00, %623
  %628 = fsub fast float -0.000000e+00, %624
  %629 = select i1 %625, float %626, float %622
  %630 = select i1 %625, float %627, float %623
  %631 = select i1 %625, float %628, float %624
  %632 = fsub fast float -0.000000e+00, %2665
  %633 = fsub fast float -0.000000e+00, %2666
  %634 = fsub fast float -0.000000e+00, %2667
  %635 = call float @dx.op.dot3.f32(i32 55, float %632, float %633, float %634, float %632, float %633, float %634)  ; Dot3(ax,ay,az,bx,by,bz)
  %636 = call float @dx.op.unary.f32(i32 25, float %635)  ; Rsqrt(value)
  %637 = fmul fast float %636, %632
  %638 = fmul fast float %636, %633
  %639 = fmul fast float %636, %634
  %640 = fsub fast float -0.000000e+00, %629
  %641 = fsub fast float -0.000000e+00, %630
  %642 = fsub fast float -0.000000e+00, %631
  %643 = call float @dx.op.dot3.f32(i32 55, float %637, float %638, float %639, float %640, float %641, float %642)  ; Dot3(ax,ay,az,bx,by,bz)
  %644 = fcmp fast ogt float %643, %155
  br i1 %644, label %2539, label %645

; <label>:645                                     ; preds = %2679, %616
  %646 = and i32 %80, 512
  %647 = icmp eq i32 %646, 0
  br i1 %647, label %890, label %648

; <label>:648                                     ; preds = %645
  br label %649

; <label>:649                                     ; preds = %649, %648
  %650 = phi i32 [ %700, %649 ], [ 0, %648 ]
  %651 = and i32 %650, 1
  %652 = icmp ne i32 %651, 0
  %653 = select i1 %652, float %173, float %163
  %654 = and i32 %650, 2
  %655 = icmp ne i32 %654, 0
  %656 = select i1 %655, float %174, float %164
  %657 = and i32 %650, 4
  %658 = icmp ne i32 %657, 0
  %659 = select i1 %658, float %175, float %165
  %660 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %570, i32 0)  ; BufferLoad(srv,index,wot)
  %661 = extractvalue %dx.types.ResRet.f32 %660, 0
  %662 = extractvalue %dx.types.ResRet.f32 %660, 1
  %663 = extractvalue %dx.types.ResRet.f32 %660, 2
  %664 = extractvalue %dx.types.ResRet.f32 %660, 3
  %665 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %577, i32 0)  ; BufferLoad(srv,index,wot)
  %666 = extractvalue %dx.types.ResRet.f32 %665, 0
  %667 = extractvalue %dx.types.ResRet.f32 %665, 1
  %668 = extractvalue %dx.types.ResRet.f32 %665, 2
  %669 = extractvalue %dx.types.ResRet.f32 %665, 3
  %670 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %584, i32 0)  ; BufferLoad(srv,index,wot)
  %671 = extractvalue %dx.types.ResRet.f32 %670, 0
  %672 = extractvalue %dx.types.ResRet.f32 %670, 1
  %673 = extractvalue %dx.types.ResRet.f32 %670, 2
  %674 = extractvalue %dx.types.ResRet.f32 %670, 3
  %675 = call float @dx.op.dot4.f32(i32 56, float %653, float %656, float %659, float 1.000000e+00, float %661, float %662, float %663, float %664)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %676 = call float @dx.op.dot4.f32(i32 56, float %653, float %656, float %659, float 1.000000e+00, float %666, float %667, float %668, float %669)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %677 = call float @dx.op.dot4.f32(i32 56, float %653, float %656, float %659, float 1.000000e+00, float %671, float %672, float %673, float %674)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %678 = fmul fast float %675, %549
  %679 = call float @dx.op.tertiary.f32(i32 46, float %676, float %554, float %678)  ; FMad(a,b,c)
  %680 = call float @dx.op.tertiary.f32(i32 46, float %677, float %559, float %679)  ; FMad(a,b,c)
  %681 = fadd fast float %680, %564
  %682 = fmul fast float %675, %550
  %683 = call float @dx.op.tertiary.f32(i32 46, float %676, float %555, float %682)  ; FMad(a,b,c)
  %684 = call float @dx.op.tertiary.f32(i32 46, float %677, float %560, float %683)  ; FMad(a,b,c)
  %685 = fadd fast float %684, %565
  %686 = fmul fast float %675, %551
  %687 = call float @dx.op.tertiary.f32(i32 46, float %676, float %556, float %686)  ; FMad(a,b,c)
  %688 = call float @dx.op.tertiary.f32(i32 46, float %677, float %561, float %687)  ; FMad(a,b,c)
  %689 = fadd fast float %688, %566
  %690 = fmul fast float %675, %552
  %691 = call float @dx.op.tertiary.f32(i32 46, float %676, float %557, float %690)  ; FMad(a,b,c)
  %692 = call float @dx.op.tertiary.f32(i32 46, float %677, float %562, float %691)  ; FMad(a,b,c)
  %693 = fadd fast float %692, %567
  %694 = fdiv fast float %681, %693
  %695 = fdiv fast float %685, %693
  %696 = fdiv fast float %689, %693
  %697 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 %650
  %698 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 %650
  %699 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 %650
  store float %694, float* %697, align 4
  store float %695, float* %698, align 4
  store float %696, float* %699, align 4
  %700 = add nuw nsw i32 %650, 1
  %701 = icmp eq i32 %700, 8
  br i1 %701, label %702, label %649

; <label>:702                                     ; preds = %649
  %703 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 3
  %704 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 3
  %705 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 3
  %706 = load float, float* %703, align 4
  %707 = load float, float* %704, align 4
  %708 = load float, float* %705, align 4
  %709 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 2
  %710 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 2
  %711 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 2
  %712 = load float, float* %709, align 4
  %713 = load float, float* %710, align 4
  %714 = load float, float* %711, align 4
  %715 = call float @dx.op.binary.f32(i32 36, float %712, float %706)  ; FMin(a,b)
  %716 = call float @dx.op.binary.f32(i32 36, float %713, float %707)  ; FMin(a,b)
  %717 = call float @dx.op.binary.f32(i32 36, float %714, float %708)  ; FMin(a,b)
  %718 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 1
  %719 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 1
  %720 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 1
  %721 = load float, float* %718, align 4
  %722 = load float, float* %719, align 4
  %723 = load float, float* %720, align 4
  %724 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 0
  %725 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 0
  %726 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 0
  %727 = load float, float* %724, align 4
  %728 = load float, float* %725, align 4
  %729 = load float, float* %726, align 4
  %730 = call float @dx.op.binary.f32(i32 36, float %727, float %721)  ; FMin(a,b)
  %731 = call float @dx.op.binary.f32(i32 36, float %728, float %722)  ; FMin(a,b)
  %732 = call float @dx.op.binary.f32(i32 36, float %729, float %723)  ; FMin(a,b)
  %733 = call float @dx.op.binary.f32(i32 36, float %730, float %715)  ; FMin(a,b)
  %734 = call float @dx.op.binary.f32(i32 36, float %731, float %716)  ; FMin(a,b)
  %735 = call float @dx.op.binary.f32(i32 36, float %732, float %717)  ; FMin(a,b)
  %736 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 7
  %737 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 7
  %738 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 7
  %739 = load float, float* %736, align 4
  %740 = load float, float* %737, align 4
  %741 = load float, float* %738, align 4
  %742 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 6
  %743 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 6
  %744 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 6
  %745 = load float, float* %742, align 4
  %746 = load float, float* %743, align 4
  %747 = load float, float* %744, align 4
  %748 = call float @dx.op.binary.f32(i32 36, float %745, float %739)  ; FMin(a,b)
  %749 = call float @dx.op.binary.f32(i32 36, float %746, float %740)  ; FMin(a,b)
  %750 = call float @dx.op.binary.f32(i32 36, float %747, float %741)  ; FMin(a,b)
  %751 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 5
  %752 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 5
  %753 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 5
  %754 = load float, float* %751, align 4
  %755 = load float, float* %752, align 4
  %756 = load float, float* %753, align 4
  %757 = getelementptr [8 x float], [8 x float]* %18, i32 0, i32 4
  %758 = getelementptr [8 x float], [8 x float]* %19, i32 0, i32 4
  %759 = getelementptr [8 x float], [8 x float]* %20, i32 0, i32 4
  %760 = load float, float* %757, align 4
  %761 = load float, float* %758, align 4
  %762 = load float, float* %759, align 4
  %763 = call float @dx.op.binary.f32(i32 36, float %760, float %754)  ; FMin(a,b)
  %764 = call float @dx.op.binary.f32(i32 36, float %761, float %755)  ; FMin(a,b)
  %765 = call float @dx.op.binary.f32(i32 36, float %762, float %756)  ; FMin(a,b)
  %766 = call float @dx.op.binary.f32(i32 36, float %763, float %748)  ; FMin(a,b)
  %767 = call float @dx.op.binary.f32(i32 36, float %764, float %749)  ; FMin(a,b)
  %768 = call float @dx.op.binary.f32(i32 36, float %765, float %750)  ; FMin(a,b)
  %769 = call float @dx.op.binary.f32(i32 36, float %733, float %766)  ; FMin(a,b)
  %770 = call float @dx.op.binary.f32(i32 36, float %734, float %767)  ; FMin(a,b)
  %771 = call float @dx.op.binary.f32(i32 36, float %735, float %768)  ; FMin(a,b)
  %772 = call float @dx.op.binary.f32(i32 35, float %712, float %706)  ; FMax(a,b)
  %773 = call float @dx.op.binary.f32(i32 35, float %713, float %707)  ; FMax(a,b)
  %774 = call float @dx.op.binary.f32(i32 35, float %714, float %708)  ; FMax(a,b)
  %775 = call float @dx.op.binary.f32(i32 35, float %727, float %721)  ; FMax(a,b)
  %776 = call float @dx.op.binary.f32(i32 35, float %728, float %722)  ; FMax(a,b)
  %777 = call float @dx.op.binary.f32(i32 35, float %729, float %723)  ; FMax(a,b)
  %778 = call float @dx.op.binary.f32(i32 35, float %775, float %772)  ; FMax(a,b)
  %779 = call float @dx.op.binary.f32(i32 35, float %776, float %773)  ; FMax(a,b)
  %780 = call float @dx.op.binary.f32(i32 35, float %777, float %774)  ; FMax(a,b)
  %781 = call float @dx.op.binary.f32(i32 35, float %745, float %739)  ; FMax(a,b)
  %782 = call float @dx.op.binary.f32(i32 35, float %746, float %740)  ; FMax(a,b)
  %783 = call float @dx.op.binary.f32(i32 35, float %747, float %741)  ; FMax(a,b)
  %784 = call float @dx.op.binary.f32(i32 35, float %760, float %754)  ; FMax(a,b)
  %785 = call float @dx.op.binary.f32(i32 35, float %761, float %755)  ; FMax(a,b)
  %786 = call float @dx.op.binary.f32(i32 35, float %762, float %756)  ; FMax(a,b)
  %787 = call float @dx.op.binary.f32(i32 35, float %784, float %781)  ; FMax(a,b)
  %788 = call float @dx.op.binary.f32(i32 35, float %785, float %782)  ; FMax(a,b)
  %789 = call float @dx.op.binary.f32(i32 35, float %786, float %783)  ; FMax(a,b)
  %790 = call float @dx.op.binary.f32(i32 35, float %778, float %787)  ; FMax(a,b)
  %791 = call float @dx.op.binary.f32(i32 35, float %779, float %788)  ; FMax(a,b)
  %792 = call float @dx.op.binary.f32(i32 35, float %780, float %789)  ; FMax(a,b)
  %793 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %794 = extractvalue %dx.types.CBufRet.i32 %793, 0
  %795 = and i32 %794, 15
  %796 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %797 = extractvalue %dx.types.CBufRet.f32 %796, 0
  %798 = extractvalue %dx.types.CBufRet.f32 %796, 1
  %799 = extractvalue %dx.types.CBufRet.f32 %796, 2
  %800 = extractvalue %dx.types.CBufRet.f32 %796, 3
  %801 = fmul fast float %769, 5.000000e-01
  %802 = fmul fast float %791, 5.000000e-01
  %803 = fadd fast float %801, 5.000000e-01
  %804 = fsub fast float 5.000000e-01, %802
  %805 = fmul fast float %790, 5.000000e-01
  %806 = fmul fast float %770, 5.000000e-01
  %807 = fadd fast float %805, 5.000000e-01
  %808 = fsub fast float 5.000000e-01, %806
  %809 = fmul fast float %797, %803
  %810 = fmul fast float %798, %804
  %811 = fadd fast float %809, %799
  %812 = fadd fast float %810, %800
  %813 = fmul fast float %797, %807
  %814 = fmul fast float %798, %808
  %815 = fadd fast float %813, %799
  %816 = fadd fast float %814, %800
  %817 = call float @dx.op.binary.f32(i32 35, float %811, float 0.000000e+00)  ; FMax(a,b)
  %818 = call float @dx.op.binary.f32(i32 35, float %812, float 0.000000e+00)  ; FMax(a,b)
  %819 = call float @dx.op.binary.f32(i32 36, float %817, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %820 = call float @dx.op.binary.f32(i32 36, float %818, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %821 = call float @dx.op.binary.f32(i32 35, float %815, float 0.000000e+00)  ; FMax(a,b)
  %822 = call float @dx.op.binary.f32(i32 35, float %816, float 0.000000e+00)  ; FMax(a,b)
  %823 = call float @dx.op.binary.f32(i32 36, float %821, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %824 = call float @dx.op.binary.f32(i32 36, float %822, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %825 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %826 = extractvalue %dx.types.ResRet.i32 %825, 0
  %827 = icmp slt i32 %826, 1
  br i1 %827, label %890, label %828

; <label>:828                                     ; preds = %702
  %829 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %830 = extractvalue %dx.types.ResRet.i32 %829, 0
  %831 = icmp ult i32 %795, %830
  br i1 %831, label %832, label %890

; <label>:832                                     ; preds = %828
  %833 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %834 = extractvalue %dx.types.ResRet.i32 %833, 0
  %835 = uitofp i32 %826 to float
  %836 = uitofp i32 %834 to float
  %837 = fmul fast float %835, %819
  %838 = fmul fast float %836, %820
  %839 = fptoui float %837 to i32
  %840 = fptoui float %838 to i32
  %841 = mul i32 %840, %826
  %842 = add i32 %841, %839
  %843 = fmul fast float %835, %823
  %844 = fptoui float %843 to i32
  %845 = add i32 %841, %844
  %846 = fmul fast float %836, %824
  %847 = fptoui float %846 to i32
  %848 = mul i32 %847, %826
  %849 = add i32 %848, %839
  %850 = mul i32 %826, %795
  %851 = mul i32 %850, %834
  %852 = add nsw i32 %851, 3
  %853 = icmp sge i32 %842, %826
  %854 = srem i32 %842, %826
  %855 = icmp ne i32 %854, 0
  %856 = and i1 %855, %853
  br i1 %856, label %857, label %863

; <label>:857                                     ; preds = %832
  %858 = xor i32 %826, -1
  %859 = add i32 %852, %858
  %860 = add i32 %859, %842
  %861 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %860, i32 undef)  ; BufferLoad(srv,index,wot)
  %862 = extractvalue %dx.types.ResRet.i32 %861, 0
  br label %863

; <label>:863                                     ; preds = %857, %832
  %864 = phi i32 [ %862, %857 ], [ 0, %832 ]
  %865 = icmp slt i32 %845, %826
  br i1 %865, label %871, label %866

; <label>:866                                     ; preds = %863
  %867 = sub i32 %852, %826
  %868 = add i32 %867, %845
  %869 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %868, i32 undef)  ; BufferLoad(srv,index,wot)
  %870 = extractvalue %dx.types.ResRet.i32 %869, 0
  br label %871

; <label>:871                                     ; preds = %866, %863
  %872 = phi i32 [ %870, %866 ], [ 0, %863 ]
  %873 = srem i32 %849, %826
  %874 = icmp eq i32 %873, 0
  br i1 %874, label %880, label %875

; <label>:875                                     ; preds = %871
  %876 = add i32 %851, 2
  %877 = add i32 %876, %849
  %878 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %877, i32 undef)  ; BufferLoad(srv,index,wot)
  %879 = extractvalue %dx.types.ResRet.i32 %878, 0
  br label %880

; <label>:880                                     ; preds = %875, %871
  %881 = phi i32 [ %879, %875 ], [ 0, %871 ]
  %882 = add i32 %852, %844
  %883 = add i32 %882, %848
  %884 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %883, i32 undef)  ; BufferLoad(srv,index,wot)
  %885 = extractvalue %dx.types.ResRet.i32 %884, 0
  %886 = add i32 %881, %872
  %887 = sub i32 %885, %886
  %888 = sub i32 0, %864
  %889 = icmp eq i32 %887, %888
  br i1 %889, label %2539, label %890

; <label>:890                                     ; preds = %880, %828, %702, %645
  %891 = phi i32 [ 0, %645 ], [ 1, %702 ], [ 1, %828 ], [ 1, %880 ]
  %892 = phi float [ 0.000000e+00, %645 ], [ %769, %702 ], [ %769, %828 ], [ %769, %880 ]
  %893 = phi float [ 0.000000e+00, %645 ], [ %770, %702 ], [ %770, %828 ], [ %770, %880 ]
  %894 = phi float [ 0.000000e+00, %645 ], [ %771, %702 ], [ %771, %828 ], [ %771, %880 ]
  %895 = phi float [ 0.000000e+00, %645 ], [ %790, %702 ], [ %790, %828 ], [ %790, %880 ]
  %896 = phi float [ 0.000000e+00, %645 ], [ %791, %702 ], [ %791, %828 ], [ %791, %880 ]
  %897 = phi float [ 0.000000e+00, %645 ], [ %792, %702 ], [ %792, %828 ], [ %792, %880 ]
  br i1 %181, label %898, label %956

; <label>:898                                     ; preds = %890
  %899 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %570, i32 0)  ; BufferLoad(srv,index,wot)
  %900 = extractvalue %dx.types.ResRet.f32 %899, 0
  %901 = extractvalue %dx.types.ResRet.f32 %899, 1
  %902 = extractvalue %dx.types.ResRet.f32 %899, 2
  %903 = extractvalue %dx.types.ResRet.f32 %899, 3
  %904 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %577, i32 0)  ; BufferLoad(srv,index,wot)
  %905 = extractvalue %dx.types.ResRet.f32 %904, 0
  %906 = extractvalue %dx.types.ResRet.f32 %904, 1
  %907 = extractvalue %dx.types.ResRet.f32 %904, 2
  %908 = extractvalue %dx.types.ResRet.f32 %904, 3
  %909 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %584, i32 0)  ; BufferLoad(srv,index,wot)
  %910 = extractvalue %dx.types.ResRet.f32 %909, 0
  %911 = extractvalue %dx.types.ResRet.f32 %909, 1
  %912 = extractvalue %dx.types.ResRet.f32 %909, 2
  %913 = extractvalue %dx.types.ResRet.f32 %909, 3
  %914 = call float @dx.op.dot3.f32(i32 55, float %910, float %911, float %912, float %910, float %911, float %912)  ; Dot3(ax,ay,az,bx,by,bz)
  %915 = call float @dx.op.dot3.f32(i32 55, float %905, float %906, float %907, float %905, float %906, float %907)  ; Dot3(ax,ay,az,bx,by,bz)
  %916 = call float @dx.op.binary.f32(i32 35, float %915, float %914)  ; FMax(a,b)
  %917 = call float @dx.op.dot3.f32(i32 55, float %900, float %901, float %902, float %900, float %901, float %902)  ; Dot3(ax,ay,az,bx,by,bz)
  %918 = call float @dx.op.binary.f32(i32 35, float %917, float %916)  ; FMax(a,b)
  %919 = call float @dx.op.unary.f32(i32 24, float %918)  ; Sqrt(value)
  %920 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %900, float %901, float %902, float %903)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %921 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %905, float %906, float %907, float %908)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %922 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %910, float %911, float %912, float %913)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %923 = fmul fast float %919, %143
  %924 = fmul fast float %920, %549
  %925 = call float @dx.op.tertiary.f32(i32 46, float %921, float %554, float %924)  ; FMad(a,b,c)
  %926 = call float @dx.op.tertiary.f32(i32 46, float %922, float %559, float %925)  ; FMad(a,b,c)
  %927 = fadd fast float %926, %564
  %928 = fmul fast float %920, %550
  %929 = call float @dx.op.tertiary.f32(i32 46, float %921, float %555, float %928)  ; FMad(a,b,c)
  %930 = call float @dx.op.tertiary.f32(i32 46, float %922, float %560, float %929)  ; FMad(a,b,c)
  %931 = fadd fast float %930, %565
  %932 = fmul fast float %927, 5.000000e-01
  %933 = fmul fast float %931, 5.000000e-01
  %934 = fadd fast float %932, 5.000000e-01
  %935 = fadd fast float %933, 5.000000e-01
  %936 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %937 = extractvalue %dx.types.CBufRet.f32 %936, 0
  %938 = extractvalue %dx.types.CBufRet.f32 %936, 1
  %939 = fmul fast float %937, %934
  %940 = fmul fast float %935, %938
  %941 = call float @dx.op.unary.f32(i32 27, float %939)  ; Round_ni(value)
  %942 = fadd fast float %941, 5.000000e-01
  %943 = call float @dx.op.unary.f32(i32 27, float %940)  ; Round_ni(value)
  %944 = fadd fast float %943, 5.000000e-01
  %945 = fsub fast float %942, %939
  %946 = extractvalue %dx.types.CBufRet.f32 %936, 2
  %947 = fmul fast float %946, %945
  %948 = fsub fast float %944, %940
  %949 = extractvalue %dx.types.CBufRet.f32 %936, 3
  %950 = fmul fast float %949, %948
  %951 = fmul fast float %947, %947
  %952 = fmul fast float %950, %950
  %953 = fadd fast float %952, %951
  %954 = fmul fast float %923, %923
  %955 = fcmp uge float %953, %954
  br i1 %955, label %2539, label %956

; <label>:956                                     ; preds = %898, %890
  %957 = fcmp uge float %2687, %599
  %958 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %959 = extractvalue %dx.types.CBufRet.i32 %958, 0
  %960 = icmp sgt i32 %959, 0
  %961 = and i1 %957, %960
  br i1 %961, label %962, label %1399

; <label>:962                                     ; preds = %956
  %963 = icmp eq i32 %891, 0
  br i1 %963, label %964, label %1109

; <label>:964                                     ; preds = %962
  br label %965

; <label>:965                                     ; preds = %965, %964
  %966 = phi i32 [ %1016, %965 ], [ 0, %964 ]
  %967 = and i32 %966, 1
  %968 = icmp ne i32 %967, 0
  %969 = select i1 %968, float %173, float %163
  %970 = and i32 %966, 2
  %971 = icmp ne i32 %970, 0
  %972 = select i1 %971, float %174, float %164
  %973 = and i32 %966, 4
  %974 = icmp ne i32 %973, 0
  %975 = select i1 %974, float %175, float %165
  %976 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %570, i32 0)  ; BufferLoad(srv,index,wot)
  %977 = extractvalue %dx.types.ResRet.f32 %976, 0
  %978 = extractvalue %dx.types.ResRet.f32 %976, 1
  %979 = extractvalue %dx.types.ResRet.f32 %976, 2
  %980 = extractvalue %dx.types.ResRet.f32 %976, 3
  %981 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %577, i32 0)  ; BufferLoad(srv,index,wot)
  %982 = extractvalue %dx.types.ResRet.f32 %981, 0
  %983 = extractvalue %dx.types.ResRet.f32 %981, 1
  %984 = extractvalue %dx.types.ResRet.f32 %981, 2
  %985 = extractvalue %dx.types.ResRet.f32 %981, 3
  %986 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %584, i32 0)  ; BufferLoad(srv,index,wot)
  %987 = extractvalue %dx.types.ResRet.f32 %986, 0
  %988 = extractvalue %dx.types.ResRet.f32 %986, 1
  %989 = extractvalue %dx.types.ResRet.f32 %986, 2
  %990 = extractvalue %dx.types.ResRet.f32 %986, 3
  %991 = call float @dx.op.dot4.f32(i32 56, float %969, float %972, float %975, float 1.000000e+00, float %977, float %978, float %979, float %980)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %992 = call float @dx.op.dot4.f32(i32 56, float %969, float %972, float %975, float 1.000000e+00, float %982, float %983, float %984, float %985)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %993 = call float @dx.op.dot4.f32(i32 56, float %969, float %972, float %975, float 1.000000e+00, float %987, float %988, float %989, float %990)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %994 = fmul fast float %991, %549
  %995 = call float @dx.op.tertiary.f32(i32 46, float %992, float %554, float %994)  ; FMad(a,b,c)
  %996 = call float @dx.op.tertiary.f32(i32 46, float %993, float %559, float %995)  ; FMad(a,b,c)
  %997 = fadd fast float %996, %564
  %998 = fmul fast float %991, %550
  %999 = call float @dx.op.tertiary.f32(i32 46, float %992, float %555, float %998)  ; FMad(a,b,c)
  %1000 = call float @dx.op.tertiary.f32(i32 46, float %993, float %560, float %999)  ; FMad(a,b,c)
  %1001 = fadd fast float %1000, %565
  %1002 = fmul fast float %991, %551
  %1003 = call float @dx.op.tertiary.f32(i32 46, float %992, float %556, float %1002)  ; FMad(a,b,c)
  %1004 = call float @dx.op.tertiary.f32(i32 46, float %993, float %561, float %1003)  ; FMad(a,b,c)
  %1005 = fadd fast float %1004, %566
  %1006 = fmul fast float %991, %552
  %1007 = call float @dx.op.tertiary.f32(i32 46, float %992, float %557, float %1006)  ; FMad(a,b,c)
  %1008 = call float @dx.op.tertiary.f32(i32 46, float %993, float %562, float %1007)  ; FMad(a,b,c)
  %1009 = fadd fast float %1008, %567
  %1010 = fdiv fast float %997, %1009
  %1011 = fdiv fast float %1001, %1009
  %1012 = fdiv fast float %1005, %1009
  %1013 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 %966
  %1014 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 %966
  %1015 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 %966
  store float %1010, float* %1013, align 4
  store float %1011, float* %1014, align 4
  store float %1012, float* %1015, align 4
  %1016 = add nuw nsw i32 %966, 1
  %1017 = icmp eq i32 %1016, 8
  br i1 %1017, label %1018, label %965

; <label>:1018                                    ; preds = %965
  %1019 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 3
  %1020 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 3
  %1021 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 3
  %1022 = load float, float* %1019, align 4
  %1023 = load float, float* %1020, align 4
  %1024 = load float, float* %1021, align 4
  %1025 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 2
  %1026 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 2
  %1027 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 2
  %1028 = load float, float* %1025, align 4
  %1029 = load float, float* %1026, align 4
  %1030 = load float, float* %1027, align 4
  %1031 = call float @dx.op.binary.f32(i32 36, float %1028, float %1022)  ; FMin(a,b)
  %1032 = call float @dx.op.binary.f32(i32 36, float %1029, float %1023)  ; FMin(a,b)
  %1033 = call float @dx.op.binary.f32(i32 36, float %1030, float %1024)  ; FMin(a,b)
  %1034 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 1
  %1035 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 1
  %1036 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 1
  %1037 = load float, float* %1034, align 4
  %1038 = load float, float* %1035, align 4
  %1039 = load float, float* %1036, align 4
  %1040 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 0
  %1041 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 0
  %1042 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 0
  %1043 = load float, float* %1040, align 4
  %1044 = load float, float* %1041, align 4
  %1045 = load float, float* %1042, align 4
  %1046 = call float @dx.op.binary.f32(i32 36, float %1043, float %1037)  ; FMin(a,b)
  %1047 = call float @dx.op.binary.f32(i32 36, float %1044, float %1038)  ; FMin(a,b)
  %1048 = call float @dx.op.binary.f32(i32 36, float %1045, float %1039)  ; FMin(a,b)
  %1049 = call float @dx.op.binary.f32(i32 36, float %1046, float %1031)  ; FMin(a,b)
  %1050 = call float @dx.op.binary.f32(i32 36, float %1047, float %1032)  ; FMin(a,b)
  %1051 = call float @dx.op.binary.f32(i32 36, float %1048, float %1033)  ; FMin(a,b)
  %1052 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 7
  %1053 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 7
  %1054 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 7
  %1055 = load float, float* %1052, align 4
  %1056 = load float, float* %1053, align 4
  %1057 = load float, float* %1054, align 4
  %1058 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 6
  %1059 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 6
  %1060 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 6
  %1061 = load float, float* %1058, align 4
  %1062 = load float, float* %1059, align 4
  %1063 = load float, float* %1060, align 4
  %1064 = call float @dx.op.binary.f32(i32 36, float %1061, float %1055)  ; FMin(a,b)
  %1065 = call float @dx.op.binary.f32(i32 36, float %1062, float %1056)  ; FMin(a,b)
  %1066 = call float @dx.op.binary.f32(i32 36, float %1063, float %1057)  ; FMin(a,b)
  %1067 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 5
  %1068 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 5
  %1069 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 5
  %1070 = load float, float* %1067, align 4
  %1071 = load float, float* %1068, align 4
  %1072 = load float, float* %1069, align 4
  %1073 = getelementptr [8 x float], [8 x float]* %31, i32 0, i32 4
  %1074 = getelementptr [8 x float], [8 x float]* %32, i32 0, i32 4
  %1075 = getelementptr [8 x float], [8 x float]* %33, i32 0, i32 4
  %1076 = load float, float* %1073, align 4
  %1077 = load float, float* %1074, align 4
  %1078 = load float, float* %1075, align 4
  %1079 = call float @dx.op.binary.f32(i32 36, float %1076, float %1070)  ; FMin(a,b)
  %1080 = call float @dx.op.binary.f32(i32 36, float %1077, float %1071)  ; FMin(a,b)
  %1081 = call float @dx.op.binary.f32(i32 36, float %1078, float %1072)  ; FMin(a,b)
  %1082 = call float @dx.op.binary.f32(i32 36, float %1079, float %1064)  ; FMin(a,b)
  %1083 = call float @dx.op.binary.f32(i32 36, float %1080, float %1065)  ; FMin(a,b)
  %1084 = call float @dx.op.binary.f32(i32 36, float %1081, float %1066)  ; FMin(a,b)
  %1085 = call float @dx.op.binary.f32(i32 36, float %1049, float %1082)  ; FMin(a,b)
  %1086 = call float @dx.op.binary.f32(i32 36, float %1050, float %1083)  ; FMin(a,b)
  %1087 = call float @dx.op.binary.f32(i32 36, float %1051, float %1084)  ; FMin(a,b)
  %1088 = call float @dx.op.binary.f32(i32 35, float %1028, float %1022)  ; FMax(a,b)
  %1089 = call float @dx.op.binary.f32(i32 35, float %1029, float %1023)  ; FMax(a,b)
  %1090 = call float @dx.op.binary.f32(i32 35, float %1030, float %1024)  ; FMax(a,b)
  %1091 = call float @dx.op.binary.f32(i32 35, float %1043, float %1037)  ; FMax(a,b)
  %1092 = call float @dx.op.binary.f32(i32 35, float %1044, float %1038)  ; FMax(a,b)
  %1093 = call float @dx.op.binary.f32(i32 35, float %1045, float %1039)  ; FMax(a,b)
  %1094 = call float @dx.op.binary.f32(i32 35, float %1091, float %1088)  ; FMax(a,b)
  %1095 = call float @dx.op.binary.f32(i32 35, float %1092, float %1089)  ; FMax(a,b)
  %1096 = call float @dx.op.binary.f32(i32 35, float %1093, float %1090)  ; FMax(a,b)
  %1097 = call float @dx.op.binary.f32(i32 35, float %1061, float %1055)  ; FMax(a,b)
  %1098 = call float @dx.op.binary.f32(i32 35, float %1062, float %1056)  ; FMax(a,b)
  %1099 = call float @dx.op.binary.f32(i32 35, float %1063, float %1057)  ; FMax(a,b)
  %1100 = call float @dx.op.binary.f32(i32 35, float %1076, float %1070)  ; FMax(a,b)
  %1101 = call float @dx.op.binary.f32(i32 35, float %1077, float %1071)  ; FMax(a,b)
  %1102 = call float @dx.op.binary.f32(i32 35, float %1078, float %1072)  ; FMax(a,b)
  %1103 = call float @dx.op.binary.f32(i32 35, float %1100, float %1097)  ; FMax(a,b)
  %1104 = call float @dx.op.binary.f32(i32 35, float %1101, float %1098)  ; FMax(a,b)
  %1105 = call float @dx.op.binary.f32(i32 35, float %1102, float %1099)  ; FMax(a,b)
  %1106 = call float @dx.op.binary.f32(i32 35, float %1094, float %1103)  ; FMax(a,b)
  %1107 = call float @dx.op.binary.f32(i32 35, float %1095, float %1104)  ; FMax(a,b)
  %1108 = call float @dx.op.binary.f32(i32 35, float %1096, float %1105)  ; FMax(a,b)
  br label %1109

; <label>:1109                                    ; preds = %1018, %962
  %1110 = phi float [ %892, %962 ], [ %1085, %1018 ]
  %1111 = phi float [ %893, %962 ], [ %1086, %1018 ]
  %1112 = phi float [ %894, %962 ], [ %1087, %1018 ]
  %1113 = phi float [ %895, %962 ], [ %1106, %1018 ]
  %1114 = phi float [ %896, %962 ], [ %1107, %1018 ]
  %1115 = phi float [ %897, %962 ], [ %1108, %1018 ]
  %1116 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1117 = extractvalue %dx.types.CBufRet.i32 %1116, 1
  %1118 = icmp sgt i32 %1117, 0
  br i1 %1118, label %1119, label %1290

; <label>:1119                                    ; preds = %1109
  %1120 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1121 = extractvalue %dx.types.CBufRet.f32 %1120, 0
  %1122 = extractvalue %dx.types.CBufRet.f32 %1120, 1
  %1123 = extractvalue %dx.types.CBufRet.f32 %1120, 2
  %1124 = extractvalue %dx.types.CBufRet.f32 %1120, 3
  %1125 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %1126 = extractvalue %dx.types.CBufRet.f32 %1125, 0
  %1127 = extractvalue %dx.types.CBufRet.f32 %1125, 1
  %1128 = extractvalue %dx.types.CBufRet.f32 %1125, 2
  %1129 = extractvalue %dx.types.CBufRet.f32 %1125, 3
  %1130 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1131 = extractvalue %dx.types.CBufRet.f32 %1130, 0
  %1132 = extractvalue %dx.types.CBufRet.f32 %1130, 1
  %1133 = extractvalue %dx.types.CBufRet.f32 %1130, 2
  %1134 = extractvalue %dx.types.CBufRet.f32 %1130, 3
  %1135 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1136 = extractvalue %dx.types.CBufRet.f32 %1135, 0
  %1137 = extractvalue %dx.types.CBufRet.f32 %1135, 1
  %1138 = extractvalue %dx.types.CBufRet.f32 %1135, 2
  %1139 = extractvalue %dx.types.CBufRet.f32 %1135, 3
  br label %1140

; <label>:1140                                    ; preds = %1140, %1119
  %1141 = phi i32 [ 0, %1119 ], [ %1191, %1140 ]
  %1142 = and i32 %1141, 1
  %1143 = icmp ne i32 %1142, 0
  %1144 = select i1 %1143, float %173, float %163
  %1145 = and i32 %1141, 2
  %1146 = icmp ne i32 %1145, 0
  %1147 = select i1 %1146, float %174, float %164
  %1148 = and i32 %1141, 4
  %1149 = icmp ne i32 %1148, 0
  %1150 = select i1 %1149, float %175, float %165
  %1151 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %570, i32 0)  ; BufferLoad(srv,index,wot)
  %1152 = extractvalue %dx.types.ResRet.f32 %1151, 0
  %1153 = extractvalue %dx.types.ResRet.f32 %1151, 1
  %1154 = extractvalue %dx.types.ResRet.f32 %1151, 2
  %1155 = extractvalue %dx.types.ResRet.f32 %1151, 3
  %1156 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %577, i32 0)  ; BufferLoad(srv,index,wot)
  %1157 = extractvalue %dx.types.ResRet.f32 %1156, 0
  %1158 = extractvalue %dx.types.ResRet.f32 %1156, 1
  %1159 = extractvalue %dx.types.ResRet.f32 %1156, 2
  %1160 = extractvalue %dx.types.ResRet.f32 %1156, 3
  %1161 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %584, i32 0)  ; BufferLoad(srv,index,wot)
  %1162 = extractvalue %dx.types.ResRet.f32 %1161, 0
  %1163 = extractvalue %dx.types.ResRet.f32 %1161, 1
  %1164 = extractvalue %dx.types.ResRet.f32 %1161, 2
  %1165 = extractvalue %dx.types.ResRet.f32 %1161, 3
  %1166 = call float @dx.op.dot4.f32(i32 56, float %1144, float %1147, float %1150, float 1.000000e+00, float %1152, float %1153, float %1154, float %1155)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1167 = call float @dx.op.dot4.f32(i32 56, float %1144, float %1147, float %1150, float 1.000000e+00, float %1157, float %1158, float %1159, float %1160)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1168 = call float @dx.op.dot4.f32(i32 56, float %1144, float %1147, float %1150, float 1.000000e+00, float %1162, float %1163, float %1164, float %1165)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1169 = fmul fast float %1166, %1121
  %1170 = call float @dx.op.tertiary.f32(i32 46, float %1167, float %1126, float %1169)  ; FMad(a,b,c)
  %1171 = call float @dx.op.tertiary.f32(i32 46, float %1168, float %1131, float %1170)  ; FMad(a,b,c)
  %1172 = fadd fast float %1171, %1136
  %1173 = fmul fast float %1166, %1122
  %1174 = call float @dx.op.tertiary.f32(i32 46, float %1167, float %1127, float %1173)  ; FMad(a,b,c)
  %1175 = call float @dx.op.tertiary.f32(i32 46, float %1168, float %1132, float %1174)  ; FMad(a,b,c)
  %1176 = fadd fast float %1175, %1137
  %1177 = fmul fast float %1166, %1123
  %1178 = call float @dx.op.tertiary.f32(i32 46, float %1167, float %1128, float %1177)  ; FMad(a,b,c)
  %1179 = call float @dx.op.tertiary.f32(i32 46, float %1168, float %1133, float %1178)  ; FMad(a,b,c)
  %1180 = fadd fast float %1179, %1138
  %1181 = fmul fast float %1166, %1124
  %1182 = call float @dx.op.tertiary.f32(i32 46, float %1167, float %1129, float %1181)  ; FMad(a,b,c)
  %1183 = call float @dx.op.tertiary.f32(i32 46, float %1168, float %1134, float %1182)  ; FMad(a,b,c)
  %1184 = fadd fast float %1183, %1139
  %1185 = fdiv fast float %1172, %1184
  %1186 = fdiv fast float %1176, %1184
  %1187 = fdiv fast float %1180, %1184
  %1188 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 %1141
  %1189 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 %1141
  %1190 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 %1141
  store float %1185, float* %1188, align 4
  store float %1186, float* %1189, align 4
  store float %1187, float* %1190, align 4
  %1191 = add nuw nsw i32 %1141, 1
  %1192 = icmp eq i32 %1191, 8
  br i1 %1192, label %1193, label %1140

; <label>:1193                                    ; preds = %1140
  %1194 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 3
  %1195 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 3
  %1196 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 3
  %1197 = load float, float* %1194, align 4
  %1198 = load float, float* %1195, align 4
  %1199 = load float, float* %1196, align 4
  %1200 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 2
  %1201 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 2
  %1202 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 2
  %1203 = load float, float* %1200, align 4
  %1204 = load float, float* %1201, align 4
  %1205 = load float, float* %1202, align 4
  %1206 = call float @dx.op.binary.f32(i32 36, float %1203, float %1197)  ; FMin(a,b)
  %1207 = call float @dx.op.binary.f32(i32 36, float %1204, float %1198)  ; FMin(a,b)
  %1208 = call float @dx.op.binary.f32(i32 36, float %1205, float %1199)  ; FMin(a,b)
  %1209 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 1
  %1210 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 1
  %1211 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 1
  %1212 = load float, float* %1209, align 4
  %1213 = load float, float* %1210, align 4
  %1214 = load float, float* %1211, align 4
  %1215 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 0
  %1216 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 0
  %1217 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 0
  %1218 = load float, float* %1215, align 4
  %1219 = load float, float* %1216, align 4
  %1220 = load float, float* %1217, align 4
  %1221 = call float @dx.op.binary.f32(i32 36, float %1218, float %1212)  ; FMin(a,b)
  %1222 = call float @dx.op.binary.f32(i32 36, float %1219, float %1213)  ; FMin(a,b)
  %1223 = call float @dx.op.binary.f32(i32 36, float %1220, float %1214)  ; FMin(a,b)
  %1224 = call float @dx.op.binary.f32(i32 36, float %1221, float %1206)  ; FMin(a,b)
  %1225 = call float @dx.op.binary.f32(i32 36, float %1222, float %1207)  ; FMin(a,b)
  %1226 = call float @dx.op.binary.f32(i32 36, float %1223, float %1208)  ; FMin(a,b)
  %1227 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 7
  %1228 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 7
  %1229 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 7
  %1230 = load float, float* %1227, align 4
  %1231 = load float, float* %1228, align 4
  %1232 = load float, float* %1229, align 4
  %1233 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 6
  %1234 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 6
  %1235 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 6
  %1236 = load float, float* %1233, align 4
  %1237 = load float, float* %1234, align 4
  %1238 = load float, float* %1235, align 4
  %1239 = call float @dx.op.binary.f32(i32 36, float %1236, float %1230)  ; FMin(a,b)
  %1240 = call float @dx.op.binary.f32(i32 36, float %1237, float %1231)  ; FMin(a,b)
  %1241 = call float @dx.op.binary.f32(i32 36, float %1238, float %1232)  ; FMin(a,b)
  %1242 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 5
  %1243 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 5
  %1244 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 5
  %1245 = load float, float* %1242, align 4
  %1246 = load float, float* %1243, align 4
  %1247 = load float, float* %1244, align 4
  %1248 = getelementptr [8 x float], [8 x float]* %34, i32 0, i32 4
  %1249 = getelementptr [8 x float], [8 x float]* %35, i32 0, i32 4
  %1250 = getelementptr [8 x float], [8 x float]* %36, i32 0, i32 4
  %1251 = load float, float* %1248, align 4
  %1252 = load float, float* %1249, align 4
  %1253 = load float, float* %1250, align 4
  %1254 = call float @dx.op.binary.f32(i32 36, float %1251, float %1245)  ; FMin(a,b)
  %1255 = call float @dx.op.binary.f32(i32 36, float %1252, float %1246)  ; FMin(a,b)
  %1256 = call float @dx.op.binary.f32(i32 36, float %1253, float %1247)  ; FMin(a,b)
  %1257 = call float @dx.op.binary.f32(i32 36, float %1254, float %1239)  ; FMin(a,b)
  %1258 = call float @dx.op.binary.f32(i32 36, float %1255, float %1240)  ; FMin(a,b)
  %1259 = call float @dx.op.binary.f32(i32 36, float %1256, float %1241)  ; FMin(a,b)
  %1260 = call float @dx.op.binary.f32(i32 36, float %1224, float %1257)  ; FMin(a,b)
  %1261 = call float @dx.op.binary.f32(i32 36, float %1225, float %1258)  ; FMin(a,b)
  %1262 = call float @dx.op.binary.f32(i32 36, float %1226, float %1259)  ; FMin(a,b)
  %1263 = call float @dx.op.binary.f32(i32 35, float %1203, float %1197)  ; FMax(a,b)
  %1264 = call float @dx.op.binary.f32(i32 35, float %1204, float %1198)  ; FMax(a,b)
  %1265 = call float @dx.op.binary.f32(i32 35, float %1205, float %1199)  ; FMax(a,b)
  %1266 = call float @dx.op.binary.f32(i32 35, float %1218, float %1212)  ; FMax(a,b)
  %1267 = call float @dx.op.binary.f32(i32 35, float %1219, float %1213)  ; FMax(a,b)
  %1268 = call float @dx.op.binary.f32(i32 35, float %1220, float %1214)  ; FMax(a,b)
  %1269 = call float @dx.op.binary.f32(i32 35, float %1266, float %1263)  ; FMax(a,b)
  %1270 = call float @dx.op.binary.f32(i32 35, float %1267, float %1264)  ; FMax(a,b)
  %1271 = call float @dx.op.binary.f32(i32 35, float %1268, float %1265)  ; FMax(a,b)
  %1272 = call float @dx.op.binary.f32(i32 35, float %1236, float %1230)  ; FMax(a,b)
  %1273 = call float @dx.op.binary.f32(i32 35, float %1237, float %1231)  ; FMax(a,b)
  %1274 = call float @dx.op.binary.f32(i32 35, float %1238, float %1232)  ; FMax(a,b)
  %1275 = call float @dx.op.binary.f32(i32 35, float %1251, float %1245)  ; FMax(a,b)
  %1276 = call float @dx.op.binary.f32(i32 35, float %1252, float %1246)  ; FMax(a,b)
  %1277 = call float @dx.op.binary.f32(i32 35, float %1253, float %1247)  ; FMax(a,b)
  %1278 = call float @dx.op.binary.f32(i32 35, float %1275, float %1272)  ; FMax(a,b)
  %1279 = call float @dx.op.binary.f32(i32 35, float %1276, float %1273)  ; FMax(a,b)
  %1280 = call float @dx.op.binary.f32(i32 35, float %1277, float %1274)  ; FMax(a,b)
  %1281 = call float @dx.op.binary.f32(i32 35, float %1269, float %1278)  ; FMax(a,b)
  %1282 = call float @dx.op.binary.f32(i32 35, float %1270, float %1279)  ; FMax(a,b)
  %1283 = call float @dx.op.binary.f32(i32 35, float %1271, float %1280)  ; FMax(a,b)
  %1284 = call float @dx.op.binary.f32(i32 36, float %1110, float %1260)  ; FMin(a,b)
  %1285 = call float @dx.op.binary.f32(i32 36, float %1111, float %1261)  ; FMin(a,b)
  %1286 = call float @dx.op.binary.f32(i32 36, float %1112, float %1262)  ; FMin(a,b)
  %1287 = call float @dx.op.binary.f32(i32 35, float %1113, float %1281)  ; FMax(a,b)
  %1288 = call float @dx.op.binary.f32(i32 35, float %1114, float %1282)  ; FMax(a,b)
  %1289 = call float @dx.op.binary.f32(i32 35, float %1115, float %1283)  ; FMax(a,b)
  br label %1290

; <label>:1290                                    ; preds = %1193, %1109
  %1291 = phi float [ %1284, %1193 ], [ %1110, %1109 ]
  %1292 = phi float [ %1285, %1193 ], [ %1111, %1109 ]
  %1293 = phi float [ %1286, %1193 ], [ %1112, %1109 ]
  %1294 = phi float [ %1287, %1193 ], [ %1113, %1109 ]
  %1295 = phi float [ %1288, %1193 ], [ %1114, %1109 ]
  %1296 = phi float [ %1289, %1193 ], [ %1115, %1109 ]
  %1297 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1298 = extractvalue %dx.types.CBufRet.f32 %1297, 0
  %1299 = extractvalue %dx.types.CBufRet.f32 %1297, 1
  %1300 = extractvalue %dx.types.CBufRet.f32 %1297, 2
  %1301 = extractvalue %dx.types.CBufRet.f32 %1297, 3
  %1302 = fcmp fast olt float %1293, 0.000000e+00
  %1303 = fcmp fast olt float %1296, 0.000000e+00
  %1304 = or i1 %1302, %1303
  br i1 %1304, label %1399, label %1305

; <label>:1305                                    ; preds = %1290
  %1306 = extractvalue %dx.types.CBufRet.i32 %958, 1
  %1307 = icmp sgt i32 %1306, 0
  br i1 %1307, label %1308, label %1318

; <label>:1308                                    ; preds = %1305
  %1309 = fsub fast float %1294, %1291
  %1310 = fsub fast float %1295, %1292
  %1311 = call float @dx.op.unary.f32(i32 6, float %1309)  ; FAbs(value)
  %1312 = call float @dx.op.unary.f32(i32 6, float %1310)  ; FAbs(value)
  %1313 = fmul fast float %1311, 2.500000e-01
  %1314 = fmul fast float %1313, %1312
  %1315 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1316 = extractvalue %dx.types.CBufRet.f32 %1315, 2
  %1317 = fcmp fast ogt float %1314, %1316
  br i1 %1317, label %1399, label %1318

; <label>:1318                                    ; preds = %1308, %1305
  %1319 = fmul fast float %1291, 5.000000e-01
  %1320 = fmul fast float %1295, 5.000000e-01
  %1321 = fadd fast float %1319, 5.000000e-01
  %1322 = fsub fast float 5.000000e-01, %1320
  %1323 = call float @dx.op.unary.f32(i32 7, float %1321)  ; Saturate(value)
  %1324 = call float @dx.op.unary.f32(i32 7, float %1322)  ; Saturate(value)
  %1325 = fmul fast float %1294, 5.000000e-01
  %1326 = fmul fast float %1292, 5.000000e-01
  %1327 = fadd fast float %1325, 5.000000e-01
  %1328 = fsub fast float 5.000000e-01, %1326
  %1329 = call float @dx.op.unary.f32(i32 7, float %1327)  ; Saturate(value)
  %1330 = call float @dx.op.unary.f32(i32 7, float %1328)  ; Saturate(value)
  %1331 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %14, i32 0)  ; GetDimensions(handle,mipLevel)
  %1332 = extractvalue %dx.types.Dimensions %1331, 0
  %1333 = sitofp i32 %1332 to float
  %1334 = extractvalue %dx.types.Dimensions %1331, 1
  %1335 = sitofp i32 %1334 to float
  %1336 = fmul fast float %1333, %1298
  %1337 = fmul fast float %1335, %1299
  %1338 = fsub fast float %1329, %1323
  %1339 = fsub fast float %1330, %1324
  %1340 = fmul fast float %1333, %1338
  %1341 = fmul fast float %1335, %1339
  %1342 = call float @dx.op.binary.f32(i32 35, float %1340, float %1341)  ; FMax(a,b)
  %1343 = call float @dx.op.unary.f32(i32 23, float %1342)  ; Log(value)
  %1344 = call float @dx.op.unary.f32(i32 28, float %1343)  ; Round_pi(value)
  %1345 = fadd fast float %1344, -1.000000e+00
  %1346 = call float @dx.op.binary.f32(i32 35, float %1345, float 0.000000e+00)  ; FMax(a,b)
  %1347 = fptoui float %1336 to i32
  %1348 = fptoui float %1346 to i32
  %1349 = and i32 %1348, 31
  %1350 = lshr i32 %1347, %1349
  %1351 = uitofp i32 %1350 to float
  %1352 = fptoui float %1337 to i32
  %1353 = lshr i32 %1352, %1349
  %1354 = uitofp i32 %1353 to float
  %1355 = fmul fast float %1351, %1329
  %1356 = fmul fast float %1354, %1330
  %1357 = call float @dx.op.unary.f32(i32 28, float %1355)  ; Round_pi(value)
  %1358 = call float @dx.op.unary.f32(i32 28, float %1356)  ; Round_pi(value)
  %1359 = fmul fast float %1351, %1323
  %1360 = fmul fast float %1354, %1324
  %1361 = call float @dx.op.unary.f32(i32 27, float %1359)  ; Round_ni(value)
  %1362 = call float @dx.op.unary.f32(i32 27, float %1360)  ; Round_ni(value)
  %1363 = fsub fast float %1357, %1361
  %1364 = fsub fast float %1358, %1362
  %1365 = fcmp fast ole float %1363, 2.000000e+00
  %1366 = fcmp fast ole float %1364, 2.000000e+00
  %1367 = and i1 %1365, %1366
  %1368 = fadd fast float %1346, 1.000000e+00
  %1369 = select i1 %1367, float %1346, float %1368
  %1370 = fmul fast float %1323, %1298
  %1371 = fmul fast float %1324, %1299
  %1372 = fadd fast float %1370, %1300
  %1373 = fadd fast float %1371, %1301
  %1374 = fmul fast float %1329, %1298
  %1375 = fmul fast float %1330, %1299
  %1376 = fadd fast float %1374, %1300
  %1377 = fadd fast float %1375, %1301
  %1378 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1372, float %1373, float undef, float undef, i32 0, i32 0, i32 undef, float %1369)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1379 = extractvalue %dx.types.ResRet.f32 %1378, 0
  %1380 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1372, float %1377, float undef, float undef, i32 0, i32 0, i32 undef, float %1369)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1381 = extractvalue %dx.types.ResRet.f32 %1380, 0
  %1382 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1376, float %1373, float undef, float undef, i32 0, i32 0, i32 undef, float %1369)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1383 = extractvalue %dx.types.ResRet.f32 %1382, 0
  %1384 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1376, float %1377, float undef, float undef, i32 0, i32 0, i32 undef, float %1369)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1385 = extractvalue %dx.types.ResRet.f32 %1384, 0
  %1386 = icmp eq i32 %569, 0
  br i1 %1386, label %1392, label %1387

; <label>:1387                                    ; preds = %1318
  %1388 = call float @dx.op.binary.f32(i32 36, float %1383, float %1385)  ; FMin(a,b)
  %1389 = call float @dx.op.binary.f32(i32 36, float %1379, float %1381)  ; FMin(a,b)
  %1390 = call float @dx.op.binary.f32(i32 36, float %1389, float %1388)  ; FMin(a,b)
  %1391 = fcmp fast olt float %1296, %1390
  br label %1397

; <label>:1392                                    ; preds = %1318
  %1393 = call float @dx.op.binary.f32(i32 35, float %1383, float %1385)  ; FMax(a,b)
  %1394 = call float @dx.op.binary.f32(i32 35, float %1379, float %1381)  ; FMax(a,b)
  %1395 = call float @dx.op.binary.f32(i32 35, float %1394, float %1393)  ; FMax(a,b)
  %1396 = fcmp fast olt float %1395, %1293
  br label %1397

; <label>:1397                                    ; preds = %1392, %1387
  %1398 = phi i1 [ %1391, %1387 ], [ %1396, %1392 ]
  br i1 %1398, label %2539, label %1399

; <label>:1399                                    ; preds = %1397, %1308, %1290, %956
  br label %2539

; <label>:1400                                    ; preds = %545
  %1401 = icmp eq i32 %182, 2
  br i1 %1401, label %1402, label %1868

; <label>:1402                                    ; preds = %1400
  %1403 = extractvalue %dx.types.CBufRet.i32 %79, 3
  %1404 = and i32 %1403, 31
  %1405 = lshr i32 %123, %1404
  %1406 = and i32 %1405, 15
  %1407 = icmp eq i32 %1406, 3
  br i1 %1407, label %1408, label %2539

; <label>:1408                                    ; preds = %1402
  %1409 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %1410 = extractvalue %dx.types.CBufRet.f32 %1409, 0
  %1411 = extractvalue %dx.types.CBufRet.f32 %1409, 1
  %1412 = extractvalue %dx.types.CBufRet.f32 %1409, 2
  %1413 = extractvalue %dx.types.CBufRet.f32 %1409, 3
  %1414 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1415 = extractvalue %dx.types.CBufRet.f32 %1414, 0
  %1416 = extractvalue %dx.types.CBufRet.f32 %1414, 1
  %1417 = extractvalue %dx.types.CBufRet.f32 %1414, 2
  %1418 = extractvalue %dx.types.CBufRet.f32 %1414, 3
  %1419 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %1420 = extractvalue %dx.types.CBufRet.f32 %1419, 0
  %1421 = extractvalue %dx.types.CBufRet.f32 %1419, 1
  %1422 = extractvalue %dx.types.CBufRet.f32 %1419, 2
  %1423 = extractvalue %dx.types.CBufRet.f32 %1419, 3
  %1424 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %1425 = extractvalue %dx.types.CBufRet.f32 %1424, 0
  %1426 = extractvalue %dx.types.CBufRet.f32 %1424, 1
  %1427 = extractvalue %dx.types.CBufRet.f32 %1424, 2
  %1428 = extractvalue %dx.types.CBufRet.f32 %1424, 3
  %1429 = extractvalue %dx.types.CBufRet.i32 %79, 0
  %1430 = and i32 %1429, 1
  %1431 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1432 = extractvalue %dx.types.CBufRet.i32 %1431, 0
  %1433 = icmp sgt i32 %1432, 0
  br i1 %1433, label %1434, label %1867

; <label>:1434                                    ; preds = %1408
  br label %1435

; <label>:1435                                    ; preds = %1435, %1434
  %1436 = phi i32 [ %1491, %1435 ], [ 0, %1434 ]
  %1437 = and i32 %1436, 1
  %1438 = icmp ne i32 %1437, 0
  %1439 = select i1 %1438, float %173, float %163
  %1440 = and i32 %1436, 2
  %1441 = icmp ne i32 %1440, 0
  %1442 = select i1 %1441, float %174, float %164
  %1443 = and i32 %1436, 4
  %1444 = icmp ne i32 %1443, 0
  %1445 = select i1 %1444, float %175, float %165
  %1446 = add i32 %179, %177
  %1447 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1446, i32 0)  ; BufferLoad(srv,index,wot)
  %1448 = extractvalue %dx.types.ResRet.f32 %1447, 0
  %1449 = extractvalue %dx.types.ResRet.f32 %1447, 1
  %1450 = extractvalue %dx.types.ResRet.f32 %1447, 2
  %1451 = extractvalue %dx.types.ResRet.f32 %1447, 3
  %1452 = add nuw nsw i32 %177, 1
  %1453 = add i32 %1452, %179
  %1454 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1453, i32 0)  ; BufferLoad(srv,index,wot)
  %1455 = extractvalue %dx.types.ResRet.f32 %1454, 0
  %1456 = extractvalue %dx.types.ResRet.f32 %1454, 1
  %1457 = extractvalue %dx.types.ResRet.f32 %1454, 2
  %1458 = extractvalue %dx.types.ResRet.f32 %1454, 3
  %1459 = add nuw nsw i32 %177, 2
  %1460 = add i32 %1459, %179
  %1461 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1460, i32 0)  ; BufferLoad(srv,index,wot)
  %1462 = extractvalue %dx.types.ResRet.f32 %1461, 0
  %1463 = extractvalue %dx.types.ResRet.f32 %1461, 1
  %1464 = extractvalue %dx.types.ResRet.f32 %1461, 2
  %1465 = extractvalue %dx.types.ResRet.f32 %1461, 3
  %1466 = call float @dx.op.dot4.f32(i32 56, float %1439, float %1442, float %1445, float 1.000000e+00, float %1448, float %1449, float %1450, float %1451)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1467 = call float @dx.op.dot4.f32(i32 56, float %1439, float %1442, float %1445, float 1.000000e+00, float %1455, float %1456, float %1457, float %1458)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1468 = call float @dx.op.dot4.f32(i32 56, float %1439, float %1442, float %1445, float 1.000000e+00, float %1462, float %1463, float %1464, float %1465)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1469 = fmul fast float %1466, %1410
  %1470 = call float @dx.op.tertiary.f32(i32 46, float %1467, float %1415, float %1469)  ; FMad(a,b,c)
  %1471 = call float @dx.op.tertiary.f32(i32 46, float %1468, float %1420, float %1470)  ; FMad(a,b,c)
  %1472 = fadd fast float %1471, %1425
  %1473 = fmul fast float %1466, %1411
  %1474 = call float @dx.op.tertiary.f32(i32 46, float %1467, float %1416, float %1473)  ; FMad(a,b,c)
  %1475 = call float @dx.op.tertiary.f32(i32 46, float %1468, float %1421, float %1474)  ; FMad(a,b,c)
  %1476 = fadd fast float %1475, %1426
  %1477 = fmul fast float %1466, %1412
  %1478 = call float @dx.op.tertiary.f32(i32 46, float %1467, float %1417, float %1477)  ; FMad(a,b,c)
  %1479 = call float @dx.op.tertiary.f32(i32 46, float %1468, float %1422, float %1478)  ; FMad(a,b,c)
  %1480 = fadd fast float %1479, %1427
  %1481 = fmul fast float %1466, %1413
  %1482 = call float @dx.op.tertiary.f32(i32 46, float %1467, float %1418, float %1481)  ; FMad(a,b,c)
  %1483 = call float @dx.op.tertiary.f32(i32 46, float %1468, float %1423, float %1482)  ; FMad(a,b,c)
  %1484 = fadd fast float %1483, %1428
  %1485 = fdiv fast float %1472, %1484
  %1486 = fdiv fast float %1476, %1484
  %1487 = fdiv fast float %1480, %1484
  %1488 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 %1436
  %1489 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 %1436
  %1490 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 %1436
  store float %1485, float* %1488, align 4
  store float %1486, float* %1489, align 4
  store float %1487, float* %1490, align 4
  %1491 = add nuw nsw i32 %1436, 1
  %1492 = icmp eq i32 %1491, 8
  br i1 %1492, label %1493, label %1435

; <label>:1493                                    ; preds = %1435
  %1494 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 3
  %1495 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 3
  %1496 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 3
  %1497 = load float, float* %1494, align 4
  %1498 = load float, float* %1495, align 4
  %1499 = load float, float* %1496, align 4
  %1500 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 2
  %1501 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 2
  %1502 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 2
  %1503 = load float, float* %1500, align 4
  %1504 = load float, float* %1501, align 4
  %1505 = load float, float* %1502, align 4
  %1506 = call float @dx.op.binary.f32(i32 36, float %1503, float %1497)  ; FMin(a,b)
  %1507 = call float @dx.op.binary.f32(i32 36, float %1504, float %1498)  ; FMin(a,b)
  %1508 = call float @dx.op.binary.f32(i32 36, float %1505, float %1499)  ; FMin(a,b)
  %1509 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 1
  %1510 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 1
  %1511 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 1
  %1512 = load float, float* %1509, align 4
  %1513 = load float, float* %1510, align 4
  %1514 = load float, float* %1511, align 4
  %1515 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 0
  %1516 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 0
  %1517 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 0
  %1518 = load float, float* %1515, align 4
  %1519 = load float, float* %1516, align 4
  %1520 = load float, float* %1517, align 4
  %1521 = call float @dx.op.binary.f32(i32 36, float %1518, float %1512)  ; FMin(a,b)
  %1522 = call float @dx.op.binary.f32(i32 36, float %1519, float %1513)  ; FMin(a,b)
  %1523 = call float @dx.op.binary.f32(i32 36, float %1520, float %1514)  ; FMin(a,b)
  %1524 = call float @dx.op.binary.f32(i32 36, float %1521, float %1506)  ; FMin(a,b)
  %1525 = call float @dx.op.binary.f32(i32 36, float %1522, float %1507)  ; FMin(a,b)
  %1526 = call float @dx.op.binary.f32(i32 36, float %1523, float %1508)  ; FMin(a,b)
  %1527 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 7
  %1528 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 7
  %1529 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 7
  %1530 = load float, float* %1527, align 4
  %1531 = load float, float* %1528, align 4
  %1532 = load float, float* %1529, align 4
  %1533 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 6
  %1534 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 6
  %1535 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 6
  %1536 = load float, float* %1533, align 4
  %1537 = load float, float* %1534, align 4
  %1538 = load float, float* %1535, align 4
  %1539 = call float @dx.op.binary.f32(i32 36, float %1536, float %1530)  ; FMin(a,b)
  %1540 = call float @dx.op.binary.f32(i32 36, float %1537, float %1531)  ; FMin(a,b)
  %1541 = call float @dx.op.binary.f32(i32 36, float %1538, float %1532)  ; FMin(a,b)
  %1542 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 5
  %1543 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 5
  %1544 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 5
  %1545 = load float, float* %1542, align 4
  %1546 = load float, float* %1543, align 4
  %1547 = load float, float* %1544, align 4
  %1548 = getelementptr [8 x float], [8 x float]* %26, i32 0, i32 4
  %1549 = getelementptr [8 x float], [8 x float]* %27, i32 0, i32 4
  %1550 = getelementptr [8 x float], [8 x float]* %28, i32 0, i32 4
  %1551 = load float, float* %1548, align 4
  %1552 = load float, float* %1549, align 4
  %1553 = load float, float* %1550, align 4
  %1554 = call float @dx.op.binary.f32(i32 36, float %1551, float %1545)  ; FMin(a,b)
  %1555 = call float @dx.op.binary.f32(i32 36, float %1552, float %1546)  ; FMin(a,b)
  %1556 = call float @dx.op.binary.f32(i32 36, float %1553, float %1547)  ; FMin(a,b)
  %1557 = call float @dx.op.binary.f32(i32 36, float %1554, float %1539)  ; FMin(a,b)
  %1558 = call float @dx.op.binary.f32(i32 36, float %1555, float %1540)  ; FMin(a,b)
  %1559 = call float @dx.op.binary.f32(i32 36, float %1556, float %1541)  ; FMin(a,b)
  %1560 = call float @dx.op.binary.f32(i32 36, float %1524, float %1557)  ; FMin(a,b)
  %1561 = call float @dx.op.binary.f32(i32 36, float %1525, float %1558)  ; FMin(a,b)
  %1562 = call float @dx.op.binary.f32(i32 36, float %1526, float %1559)  ; FMin(a,b)
  %1563 = call float @dx.op.binary.f32(i32 35, float %1503, float %1497)  ; FMax(a,b)
  %1564 = call float @dx.op.binary.f32(i32 35, float %1504, float %1498)  ; FMax(a,b)
  %1565 = call float @dx.op.binary.f32(i32 35, float %1505, float %1499)  ; FMax(a,b)
  %1566 = call float @dx.op.binary.f32(i32 35, float %1518, float %1512)  ; FMax(a,b)
  %1567 = call float @dx.op.binary.f32(i32 35, float %1519, float %1513)  ; FMax(a,b)
  %1568 = call float @dx.op.binary.f32(i32 35, float %1520, float %1514)  ; FMax(a,b)
  %1569 = call float @dx.op.binary.f32(i32 35, float %1566, float %1563)  ; FMax(a,b)
  %1570 = call float @dx.op.binary.f32(i32 35, float %1567, float %1564)  ; FMax(a,b)
  %1571 = call float @dx.op.binary.f32(i32 35, float %1568, float %1565)  ; FMax(a,b)
  %1572 = call float @dx.op.binary.f32(i32 35, float %1536, float %1530)  ; FMax(a,b)
  %1573 = call float @dx.op.binary.f32(i32 35, float %1537, float %1531)  ; FMax(a,b)
  %1574 = call float @dx.op.binary.f32(i32 35, float %1538, float %1532)  ; FMax(a,b)
  %1575 = call float @dx.op.binary.f32(i32 35, float %1551, float %1545)  ; FMax(a,b)
  %1576 = call float @dx.op.binary.f32(i32 35, float %1552, float %1546)  ; FMax(a,b)
  %1577 = call float @dx.op.binary.f32(i32 35, float %1553, float %1547)  ; FMax(a,b)
  %1578 = call float @dx.op.binary.f32(i32 35, float %1575, float %1572)  ; FMax(a,b)
  %1579 = call float @dx.op.binary.f32(i32 35, float %1576, float %1573)  ; FMax(a,b)
  %1580 = call float @dx.op.binary.f32(i32 35, float %1577, float %1574)  ; FMax(a,b)
  %1581 = call float @dx.op.binary.f32(i32 35, float %1569, float %1578)  ; FMax(a,b)
  %1582 = call float @dx.op.binary.f32(i32 35, float %1570, float %1579)  ; FMax(a,b)
  %1583 = call float @dx.op.binary.f32(i32 35, float %1571, float %1580)  ; FMax(a,b)
  %1584 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1585 = extractvalue %dx.types.CBufRet.i32 %1584, 1
  %1586 = icmp sgt i32 %1585, 0
  br i1 %1586, label %1587, label %1758

; <label>:1587                                    ; preds = %1493
  %1588 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1589 = extractvalue %dx.types.CBufRet.f32 %1588, 0
  %1590 = extractvalue %dx.types.CBufRet.f32 %1588, 1
  %1591 = extractvalue %dx.types.CBufRet.f32 %1588, 2
  %1592 = extractvalue %dx.types.CBufRet.f32 %1588, 3
  %1593 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %1594 = extractvalue %dx.types.CBufRet.f32 %1593, 0
  %1595 = extractvalue %dx.types.CBufRet.f32 %1593, 1
  %1596 = extractvalue %dx.types.CBufRet.f32 %1593, 2
  %1597 = extractvalue %dx.types.CBufRet.f32 %1593, 3
  %1598 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %1599 = extractvalue %dx.types.CBufRet.f32 %1598, 0
  %1600 = extractvalue %dx.types.CBufRet.f32 %1598, 1
  %1601 = extractvalue %dx.types.CBufRet.f32 %1598, 2
  %1602 = extractvalue %dx.types.CBufRet.f32 %1598, 3
  %1603 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 18)  ; CBufferLoadLegacy(handle,regIndex)
  %1604 = extractvalue %dx.types.CBufRet.f32 %1603, 0
  %1605 = extractvalue %dx.types.CBufRet.f32 %1603, 1
  %1606 = extractvalue %dx.types.CBufRet.f32 %1603, 2
  %1607 = extractvalue %dx.types.CBufRet.f32 %1603, 3
  br label %1608

; <label>:1608                                    ; preds = %1608, %1587
  %1609 = phi i32 [ 0, %1587 ], [ %1659, %1608 ]
  %1610 = and i32 %1609, 1
  %1611 = icmp ne i32 %1610, 0
  %1612 = select i1 %1611, float %173, float %163
  %1613 = and i32 %1609, 2
  %1614 = icmp ne i32 %1613, 0
  %1615 = select i1 %1614, float %174, float %164
  %1616 = and i32 %1609, 4
  %1617 = icmp ne i32 %1616, 0
  %1618 = select i1 %1617, float %175, float %165
  %1619 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1446, i32 0)  ; BufferLoad(srv,index,wot)
  %1620 = extractvalue %dx.types.ResRet.f32 %1619, 0
  %1621 = extractvalue %dx.types.ResRet.f32 %1619, 1
  %1622 = extractvalue %dx.types.ResRet.f32 %1619, 2
  %1623 = extractvalue %dx.types.ResRet.f32 %1619, 3
  %1624 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1453, i32 0)  ; BufferLoad(srv,index,wot)
  %1625 = extractvalue %dx.types.ResRet.f32 %1624, 0
  %1626 = extractvalue %dx.types.ResRet.f32 %1624, 1
  %1627 = extractvalue %dx.types.ResRet.f32 %1624, 2
  %1628 = extractvalue %dx.types.ResRet.f32 %1624, 3
  %1629 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1460, i32 0)  ; BufferLoad(srv,index,wot)
  %1630 = extractvalue %dx.types.ResRet.f32 %1629, 0
  %1631 = extractvalue %dx.types.ResRet.f32 %1629, 1
  %1632 = extractvalue %dx.types.ResRet.f32 %1629, 2
  %1633 = extractvalue %dx.types.ResRet.f32 %1629, 3
  %1634 = call float @dx.op.dot4.f32(i32 56, float %1612, float %1615, float %1618, float 1.000000e+00, float %1620, float %1621, float %1622, float %1623)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1635 = call float @dx.op.dot4.f32(i32 56, float %1612, float %1615, float %1618, float 1.000000e+00, float %1625, float %1626, float %1627, float %1628)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1636 = call float @dx.op.dot4.f32(i32 56, float %1612, float %1615, float %1618, float 1.000000e+00, float %1630, float %1631, float %1632, float %1633)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1637 = fmul fast float %1634, %1589
  %1638 = call float @dx.op.tertiary.f32(i32 46, float %1635, float %1594, float %1637)  ; FMad(a,b,c)
  %1639 = call float @dx.op.tertiary.f32(i32 46, float %1636, float %1599, float %1638)  ; FMad(a,b,c)
  %1640 = fadd fast float %1639, %1604
  %1641 = fmul fast float %1634, %1590
  %1642 = call float @dx.op.tertiary.f32(i32 46, float %1635, float %1595, float %1641)  ; FMad(a,b,c)
  %1643 = call float @dx.op.tertiary.f32(i32 46, float %1636, float %1600, float %1642)  ; FMad(a,b,c)
  %1644 = fadd fast float %1643, %1605
  %1645 = fmul fast float %1634, %1591
  %1646 = call float @dx.op.tertiary.f32(i32 46, float %1635, float %1596, float %1645)  ; FMad(a,b,c)
  %1647 = call float @dx.op.tertiary.f32(i32 46, float %1636, float %1601, float %1646)  ; FMad(a,b,c)
  %1648 = fadd fast float %1647, %1606
  %1649 = fmul fast float %1634, %1592
  %1650 = call float @dx.op.tertiary.f32(i32 46, float %1635, float %1597, float %1649)  ; FMad(a,b,c)
  %1651 = call float @dx.op.tertiary.f32(i32 46, float %1636, float %1602, float %1650)  ; FMad(a,b,c)
  %1652 = fadd fast float %1651, %1607
  %1653 = fdiv fast float %1640, %1652
  %1654 = fdiv fast float %1644, %1652
  %1655 = fdiv fast float %1648, %1652
  %1656 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 %1609
  %1657 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 %1609
  %1658 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 %1609
  store float %1653, float* %1656, align 4
  store float %1654, float* %1657, align 4
  store float %1655, float* %1658, align 4
  %1659 = add nuw nsw i32 %1609, 1
  %1660 = icmp eq i32 %1659, 8
  br i1 %1660, label %1661, label %1608

; <label>:1661                                    ; preds = %1608
  %1662 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 3
  %1663 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 3
  %1664 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 3
  %1665 = load float, float* %1662, align 4
  %1666 = load float, float* %1663, align 4
  %1667 = load float, float* %1664, align 4
  %1668 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 2
  %1669 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 2
  %1670 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 2
  %1671 = load float, float* %1668, align 4
  %1672 = load float, float* %1669, align 4
  %1673 = load float, float* %1670, align 4
  %1674 = call float @dx.op.binary.f32(i32 36, float %1671, float %1665)  ; FMin(a,b)
  %1675 = call float @dx.op.binary.f32(i32 36, float %1672, float %1666)  ; FMin(a,b)
  %1676 = call float @dx.op.binary.f32(i32 36, float %1673, float %1667)  ; FMin(a,b)
  %1677 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 1
  %1678 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 1
  %1679 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 1
  %1680 = load float, float* %1677, align 4
  %1681 = load float, float* %1678, align 4
  %1682 = load float, float* %1679, align 4
  %1683 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 0
  %1684 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 0
  %1685 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 0
  %1686 = load float, float* %1683, align 4
  %1687 = load float, float* %1684, align 4
  %1688 = load float, float* %1685, align 4
  %1689 = call float @dx.op.binary.f32(i32 36, float %1686, float %1680)  ; FMin(a,b)
  %1690 = call float @dx.op.binary.f32(i32 36, float %1687, float %1681)  ; FMin(a,b)
  %1691 = call float @dx.op.binary.f32(i32 36, float %1688, float %1682)  ; FMin(a,b)
  %1692 = call float @dx.op.binary.f32(i32 36, float %1689, float %1674)  ; FMin(a,b)
  %1693 = call float @dx.op.binary.f32(i32 36, float %1690, float %1675)  ; FMin(a,b)
  %1694 = call float @dx.op.binary.f32(i32 36, float %1691, float %1676)  ; FMin(a,b)
  %1695 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 7
  %1696 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 7
  %1697 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 7
  %1698 = load float, float* %1695, align 4
  %1699 = load float, float* %1696, align 4
  %1700 = load float, float* %1697, align 4
  %1701 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 6
  %1702 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 6
  %1703 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 6
  %1704 = load float, float* %1701, align 4
  %1705 = load float, float* %1702, align 4
  %1706 = load float, float* %1703, align 4
  %1707 = call float @dx.op.binary.f32(i32 36, float %1704, float %1698)  ; FMin(a,b)
  %1708 = call float @dx.op.binary.f32(i32 36, float %1705, float %1699)  ; FMin(a,b)
  %1709 = call float @dx.op.binary.f32(i32 36, float %1706, float %1700)  ; FMin(a,b)
  %1710 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 5
  %1711 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 5
  %1712 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 5
  %1713 = load float, float* %1710, align 4
  %1714 = load float, float* %1711, align 4
  %1715 = load float, float* %1712, align 4
  %1716 = getelementptr [8 x float], [8 x float]* %21, i32 0, i32 4
  %1717 = getelementptr [8 x float], [8 x float]* %22, i32 0, i32 4
  %1718 = getelementptr [8 x float], [8 x float]* %23, i32 0, i32 4
  %1719 = load float, float* %1716, align 4
  %1720 = load float, float* %1717, align 4
  %1721 = load float, float* %1718, align 4
  %1722 = call float @dx.op.binary.f32(i32 36, float %1719, float %1713)  ; FMin(a,b)
  %1723 = call float @dx.op.binary.f32(i32 36, float %1720, float %1714)  ; FMin(a,b)
  %1724 = call float @dx.op.binary.f32(i32 36, float %1721, float %1715)  ; FMin(a,b)
  %1725 = call float @dx.op.binary.f32(i32 36, float %1722, float %1707)  ; FMin(a,b)
  %1726 = call float @dx.op.binary.f32(i32 36, float %1723, float %1708)  ; FMin(a,b)
  %1727 = call float @dx.op.binary.f32(i32 36, float %1724, float %1709)  ; FMin(a,b)
  %1728 = call float @dx.op.binary.f32(i32 36, float %1692, float %1725)  ; FMin(a,b)
  %1729 = call float @dx.op.binary.f32(i32 36, float %1693, float %1726)  ; FMin(a,b)
  %1730 = call float @dx.op.binary.f32(i32 36, float %1694, float %1727)  ; FMin(a,b)
  %1731 = call float @dx.op.binary.f32(i32 35, float %1671, float %1665)  ; FMax(a,b)
  %1732 = call float @dx.op.binary.f32(i32 35, float %1672, float %1666)  ; FMax(a,b)
  %1733 = call float @dx.op.binary.f32(i32 35, float %1673, float %1667)  ; FMax(a,b)
  %1734 = call float @dx.op.binary.f32(i32 35, float %1686, float %1680)  ; FMax(a,b)
  %1735 = call float @dx.op.binary.f32(i32 35, float %1687, float %1681)  ; FMax(a,b)
  %1736 = call float @dx.op.binary.f32(i32 35, float %1688, float %1682)  ; FMax(a,b)
  %1737 = call float @dx.op.binary.f32(i32 35, float %1734, float %1731)  ; FMax(a,b)
  %1738 = call float @dx.op.binary.f32(i32 35, float %1735, float %1732)  ; FMax(a,b)
  %1739 = call float @dx.op.binary.f32(i32 35, float %1736, float %1733)  ; FMax(a,b)
  %1740 = call float @dx.op.binary.f32(i32 35, float %1704, float %1698)  ; FMax(a,b)
  %1741 = call float @dx.op.binary.f32(i32 35, float %1705, float %1699)  ; FMax(a,b)
  %1742 = call float @dx.op.binary.f32(i32 35, float %1706, float %1700)  ; FMax(a,b)
  %1743 = call float @dx.op.binary.f32(i32 35, float %1719, float %1713)  ; FMax(a,b)
  %1744 = call float @dx.op.binary.f32(i32 35, float %1720, float %1714)  ; FMax(a,b)
  %1745 = call float @dx.op.binary.f32(i32 35, float %1721, float %1715)  ; FMax(a,b)
  %1746 = call float @dx.op.binary.f32(i32 35, float %1743, float %1740)  ; FMax(a,b)
  %1747 = call float @dx.op.binary.f32(i32 35, float %1744, float %1741)  ; FMax(a,b)
  %1748 = call float @dx.op.binary.f32(i32 35, float %1745, float %1742)  ; FMax(a,b)
  %1749 = call float @dx.op.binary.f32(i32 35, float %1737, float %1746)  ; FMax(a,b)
  %1750 = call float @dx.op.binary.f32(i32 35, float %1738, float %1747)  ; FMax(a,b)
  %1751 = call float @dx.op.binary.f32(i32 35, float %1739, float %1748)  ; FMax(a,b)
  %1752 = call float @dx.op.binary.f32(i32 36, float %1560, float %1728)  ; FMin(a,b)
  %1753 = call float @dx.op.binary.f32(i32 36, float %1561, float %1729)  ; FMin(a,b)
  %1754 = call float @dx.op.binary.f32(i32 36, float %1562, float %1730)  ; FMin(a,b)
  %1755 = call float @dx.op.binary.f32(i32 35, float %1581, float %1749)  ; FMax(a,b)
  %1756 = call float @dx.op.binary.f32(i32 35, float %1582, float %1750)  ; FMax(a,b)
  %1757 = call float @dx.op.binary.f32(i32 35, float %1583, float %1751)  ; FMax(a,b)
  br label %1758

; <label>:1758                                    ; preds = %1661, %1493
  %1759 = phi float [ %1752, %1661 ], [ %1560, %1493 ]
  %1760 = phi float [ %1753, %1661 ], [ %1561, %1493 ]
  %1761 = phi float [ %1754, %1661 ], [ %1562, %1493 ]
  %1762 = phi float [ %1755, %1661 ], [ %1581, %1493 ]
  %1763 = phi float [ %1756, %1661 ], [ %1582, %1493 ]
  %1764 = phi float [ %1757, %1661 ], [ %1583, %1493 ]
  %1765 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1766 = extractvalue %dx.types.CBufRet.f32 %1765, 0
  %1767 = extractvalue %dx.types.CBufRet.f32 %1765, 1
  %1768 = extractvalue %dx.types.CBufRet.f32 %1765, 2
  %1769 = extractvalue %dx.types.CBufRet.f32 %1765, 3
  %1770 = fcmp fast olt float %1761, 0.000000e+00
  %1771 = fcmp fast olt float %1764, 0.000000e+00
  %1772 = or i1 %1770, %1771
  br i1 %1772, label %1867, label %1773

; <label>:1773                                    ; preds = %1758
  %1774 = extractvalue %dx.types.CBufRet.i32 %1431, 1
  %1775 = icmp sgt i32 %1774, 0
  br i1 %1775, label %1776, label %1786

; <label>:1776                                    ; preds = %1773
  %1777 = fsub fast float %1762, %1759
  %1778 = fsub fast float %1763, %1760
  %1779 = call float @dx.op.unary.f32(i32 6, float %1777)  ; FAbs(value)
  %1780 = call float @dx.op.unary.f32(i32 6, float %1778)  ; FAbs(value)
  %1781 = fmul fast float %1779, 2.500000e-01
  %1782 = fmul fast float %1781, %1780
  %1783 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %1784 = extractvalue %dx.types.CBufRet.f32 %1783, 2
  %1785 = fcmp fast ogt float %1782, %1784
  br i1 %1785, label %1867, label %1786

; <label>:1786                                    ; preds = %1776, %1773
  %1787 = fmul fast float %1759, 5.000000e-01
  %1788 = fmul fast float %1763, 5.000000e-01
  %1789 = fadd fast float %1787, 5.000000e-01
  %1790 = fsub fast float 5.000000e-01, %1788
  %1791 = call float @dx.op.unary.f32(i32 7, float %1789)  ; Saturate(value)
  %1792 = call float @dx.op.unary.f32(i32 7, float %1790)  ; Saturate(value)
  %1793 = fmul fast float %1762, 5.000000e-01
  %1794 = fmul fast float %1760, 5.000000e-01
  %1795 = fadd fast float %1793, 5.000000e-01
  %1796 = fsub fast float 5.000000e-01, %1794
  %1797 = call float @dx.op.unary.f32(i32 7, float %1795)  ; Saturate(value)
  %1798 = call float @dx.op.unary.f32(i32 7, float %1796)  ; Saturate(value)
  %1799 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %14, i32 0)  ; GetDimensions(handle,mipLevel)
  %1800 = extractvalue %dx.types.Dimensions %1799, 0
  %1801 = sitofp i32 %1800 to float
  %1802 = extractvalue %dx.types.Dimensions %1799, 1
  %1803 = sitofp i32 %1802 to float
  %1804 = fmul fast float %1801, %1766
  %1805 = fmul fast float %1803, %1767
  %1806 = fsub fast float %1797, %1791
  %1807 = fsub fast float %1798, %1792
  %1808 = fmul fast float %1801, %1806
  %1809 = fmul fast float %1803, %1807
  %1810 = call float @dx.op.binary.f32(i32 35, float %1808, float %1809)  ; FMax(a,b)
  %1811 = call float @dx.op.unary.f32(i32 23, float %1810)  ; Log(value)
  %1812 = call float @dx.op.unary.f32(i32 28, float %1811)  ; Round_pi(value)
  %1813 = fadd fast float %1812, -1.000000e+00
  %1814 = call float @dx.op.binary.f32(i32 35, float %1813, float 0.000000e+00)  ; FMax(a,b)
  %1815 = fptoui float %1804 to i32
  %1816 = fptoui float %1814 to i32
  %1817 = and i32 %1816, 31
  %1818 = lshr i32 %1815, %1817
  %1819 = uitofp i32 %1818 to float
  %1820 = fptoui float %1805 to i32
  %1821 = lshr i32 %1820, %1817
  %1822 = uitofp i32 %1821 to float
  %1823 = fmul fast float %1819, %1797
  %1824 = fmul fast float %1822, %1798
  %1825 = call float @dx.op.unary.f32(i32 28, float %1823)  ; Round_pi(value)
  %1826 = call float @dx.op.unary.f32(i32 28, float %1824)  ; Round_pi(value)
  %1827 = fmul fast float %1819, %1791
  %1828 = fmul fast float %1822, %1792
  %1829 = call float @dx.op.unary.f32(i32 27, float %1827)  ; Round_ni(value)
  %1830 = call float @dx.op.unary.f32(i32 27, float %1828)  ; Round_ni(value)
  %1831 = fsub fast float %1825, %1829
  %1832 = fsub fast float %1826, %1830
  %1833 = fcmp fast ole float %1831, 2.000000e+00
  %1834 = fcmp fast ole float %1832, 2.000000e+00
  %1835 = and i1 %1833, %1834
  %1836 = fadd fast float %1814, 1.000000e+00
  %1837 = select i1 %1835, float %1814, float %1836
  %1838 = fmul fast float %1791, %1766
  %1839 = fmul fast float %1792, %1767
  %1840 = fadd fast float %1838, %1768
  %1841 = fadd fast float %1839, %1769
  %1842 = fmul fast float %1797, %1766
  %1843 = fmul fast float %1798, %1767
  %1844 = fadd fast float %1842, %1768
  %1845 = fadd fast float %1843, %1769
  %1846 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1840, float %1841, float undef, float undef, i32 0, i32 0, i32 undef, float %1837)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1847 = extractvalue %dx.types.ResRet.f32 %1846, 0
  %1848 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1840, float %1845, float undef, float undef, i32 0, i32 0, i32 undef, float %1837)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1849 = extractvalue %dx.types.ResRet.f32 %1848, 0
  %1850 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1844, float %1841, float undef, float undef, i32 0, i32 0, i32 undef, float %1837)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1851 = extractvalue %dx.types.ResRet.f32 %1850, 0
  %1852 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %14, %dx.types.Handle %15, float %1844, float %1845, float undef, float undef, i32 0, i32 0, i32 undef, float %1837)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %1853 = extractvalue %dx.types.ResRet.f32 %1852, 0
  %1854 = icmp eq i32 %1430, 0
  br i1 %1854, label %1860, label %1855

; <label>:1855                                    ; preds = %1786
  %1856 = call float @dx.op.binary.f32(i32 36, float %1851, float %1853)  ; FMin(a,b)
  %1857 = call float @dx.op.binary.f32(i32 36, float %1847, float %1849)  ; FMin(a,b)
  %1858 = call float @dx.op.binary.f32(i32 36, float %1857, float %1856)  ; FMin(a,b)
  %1859 = fcmp fast olt float %1764, %1858
  br label %1865

; <label>:1860                                    ; preds = %1786
  %1861 = call float @dx.op.binary.f32(i32 35, float %1851, float %1853)  ; FMax(a,b)
  %1862 = call float @dx.op.binary.f32(i32 35, float %1847, float %1849)  ; FMax(a,b)
  %1863 = call float @dx.op.binary.f32(i32 35, float %1862, float %1861)  ; FMax(a,b)
  %1864 = fcmp fast olt float %1863, %1761
  br label %1865

; <label>:1865                                    ; preds = %1860, %1855
  %1866 = phi i1 [ %1859, %1855 ], [ %1864, %1860 ]
  br i1 %1866, label %2539, label %1867

; <label>:1867                                    ; preds = %1865, %1776, %1758, %1408
  br label %2539

; <label>:1868                                    ; preds = %1400
  %1869 = icmp eq i32 %182, 3
  br i1 %1869, label %1870, label %2231

; <label>:1870                                    ; preds = %1868
  %1871 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %1872 = extractvalue %dx.types.CBufRet.f32 %1871, 0
  %1873 = extractvalue %dx.types.CBufRet.f32 %1871, 1
  %1874 = extractvalue %dx.types.CBufRet.f32 %1871, 3
  %1875 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1876 = extractvalue %dx.types.CBufRet.f32 %1875, 0
  %1877 = extractvalue %dx.types.CBufRet.f32 %1875, 1
  %1878 = extractvalue %dx.types.CBufRet.f32 %1875, 3
  %1879 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %1880 = extractvalue %dx.types.CBufRet.f32 %1879, 0
  %1881 = extractvalue %dx.types.CBufRet.f32 %1879, 1
  %1882 = extractvalue %dx.types.CBufRet.f32 %1879, 3
  %1883 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %1884 = extractvalue %dx.types.CBufRet.f32 %1883, 0
  %1885 = extractvalue %dx.types.CBufRet.f32 %1883, 1
  %1886 = extractvalue %dx.types.CBufRet.f32 %1883, 3
  %1887 = add i32 %179, %177
  %1888 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1887, i32 0)  ; BufferLoad(srv,index,wot)
  %1889 = extractvalue %dx.types.ResRet.f32 %1888, 0
  %1890 = extractvalue %dx.types.ResRet.f32 %1888, 1
  %1891 = extractvalue %dx.types.ResRet.f32 %1888, 2
  %1892 = extractvalue %dx.types.ResRet.f32 %1888, 3
  %1893 = add nuw nsw i32 %177, 1
  %1894 = add i32 %1893, %179
  %1895 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1894, i32 0)  ; BufferLoad(srv,index,wot)
  %1896 = extractvalue %dx.types.ResRet.f32 %1895, 0
  %1897 = extractvalue %dx.types.ResRet.f32 %1895, 1
  %1898 = extractvalue %dx.types.ResRet.f32 %1895, 2
  %1899 = extractvalue %dx.types.ResRet.f32 %1895, 3
  %1900 = add nuw nsw i32 %177, 2
  %1901 = add i32 %1900, %179
  %1902 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1901, i32 0)  ; BufferLoad(srv,index,wot)
  %1903 = extractvalue %dx.types.ResRet.f32 %1902, 0
  %1904 = extractvalue %dx.types.ResRet.f32 %1902, 1
  %1905 = extractvalue %dx.types.ResRet.f32 %1902, 2
  %1906 = extractvalue %dx.types.ResRet.f32 %1902, 3
  %1907 = call float @dx.op.dot3.f32(i32 55, float %1903, float %1904, float %1905, float %1903, float %1904, float %1905)  ; Dot3(ax,ay,az,bx,by,bz)
  %1908 = call float @dx.op.dot3.f32(i32 55, float %1896, float %1897, float %1898, float %1896, float %1897, float %1898)  ; Dot3(ax,ay,az,bx,by,bz)
  %1909 = call float @dx.op.binary.f32(i32 35, float %1908, float %1907)  ; FMax(a,b)
  %1910 = call float @dx.op.dot3.f32(i32 55, float %1889, float %1890, float %1891, float %1889, float %1890, float %1891)  ; Dot3(ax,ay,az,bx,by,bz)
  %1911 = call float @dx.op.binary.f32(i32 35, float %1910, float %1909)  ; FMax(a,b)
  %1912 = call float @dx.op.unary.f32(i32 24, float %1911)  ; Sqrt(value)
  %1913 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1889, float %1890, float %1891, float %1892)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1914 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1896, float %1897, float %1898, float %1899)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1915 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %1903, float %1904, float %1905, float %1906)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1916 = fmul fast float %1912, %143
  %1917 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %1918 = extractvalue %dx.types.CBufRet.f32 %1917, 0
  %1919 = extractvalue %dx.types.CBufRet.f32 %1917, 1
  %1920 = extractvalue %dx.types.CBufRet.f32 %1917, 2
  %1921 = extractvalue %dx.types.CBufRet.f32 %1917, 3
  %1922 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %1918, float %1919, float %1920, float %1921)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1923 = fsub fast float -0.000000e+00, %1916
  %1924 = fcmp fast olt float %1922, %1923
  br i1 %1924, label %2539, label %1925

; <label>:1925                                    ; preds = %1870
  %1926 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %1927 = extractvalue %dx.types.CBufRet.f32 %1926, 0
  %1928 = extractvalue %dx.types.CBufRet.f32 %1926, 1
  %1929 = extractvalue %dx.types.CBufRet.f32 %1926, 2
  %1930 = extractvalue %dx.types.CBufRet.f32 %1926, 3
  %1931 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %1927, float %1928, float %1929, float %1930)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1932 = fcmp fast olt float %1931, %1923
  br i1 %1932, label %2539, label %2689

; <label>:1933                                    ; preds = %2721
  %1934 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %1889, float %1890, float %1891)  ; Dot3(ax,ay,az,bx,by,bz)
  %1935 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %1896, float %1897, float %1898)  ; Dot3(ax,ay,az,bx,by,bz)
  %1936 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %1903, float %1904, float %1905)  ; Dot3(ax,ay,az,bx,by,bz)
  %1937 = call float @dx.op.dot3.f32(i32 55, float %1934, float %1935, float %1936, float %1934, float %1935, float %1936)  ; Dot3(ax,ay,az,bx,by,bz)
  %1938 = call float @dx.op.unary.f32(i32 25, float %1937)  ; Rsqrt(value)
  %1939 = fmul fast float %1938, %1934
  %1940 = fmul fast float %1938, %1935
  %1941 = fmul fast float %1938, %1936
  %1942 = icmp eq i32 %85, 2
  %1943 = fsub fast float -0.000000e+00, %1939
  %1944 = fsub fast float -0.000000e+00, %1940
  %1945 = fsub fast float -0.000000e+00, %1941
  %1946 = select i1 %1942, float %1943, float %1939
  %1947 = select i1 %1942, float %1944, float %1940
  %1948 = select i1 %1942, float %1945, float %1941
  %1949 = fsub fast float -0.000000e+00, %2707
  %1950 = fsub fast float -0.000000e+00, %2708
  %1951 = fsub fast float -0.000000e+00, %2709
  %1952 = call float @dx.op.dot3.f32(i32 55, float %1949, float %1950, float %1951, float %1949, float %1950, float %1951)  ; Dot3(ax,ay,az,bx,by,bz)
  %1953 = call float @dx.op.unary.f32(i32 25, float %1952)  ; Rsqrt(value)
  %1954 = fmul fast float %1953, %1949
  %1955 = fmul fast float %1953, %1950
  %1956 = fmul fast float %1953, %1951
  %1957 = fsub fast float -0.000000e+00, %1946
  %1958 = fsub fast float -0.000000e+00, %1947
  %1959 = fsub fast float -0.000000e+00, %1948
  %1960 = call float @dx.op.dot3.f32(i32 55, float %1954, float %1955, float %1956, float %1957, float %1958, float %1959)  ; Dot3(ax,ay,az,bx,by,bz)
  %1961 = fcmp fast ogt float %1960, %155
  br i1 %1961, label %2539, label %1962

; <label>:1962                                    ; preds = %2721, %1933
  %1963 = and i32 %80, 512
  %1964 = icmp eq i32 %1963, 0
  br i1 %1964, label %2171, label %1965

; <label>:1965                                    ; preds = %1962
  br label %1966

; <label>:1966                                    ; preds = %1966, %1965
  %1967 = phi i32 [ %2011, %1966 ], [ 0, %1965 ]
  %1968 = and i32 %1967, 1
  %1969 = icmp ne i32 %1968, 0
  %1970 = select i1 %1969, float %173, float %163
  %1971 = and i32 %1967, 2
  %1972 = icmp ne i32 %1971, 0
  %1973 = select i1 %1972, float %174, float %164
  %1974 = and i32 %1967, 4
  %1975 = icmp ne i32 %1974, 0
  %1976 = select i1 %1975, float %175, float %165
  %1977 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1887, i32 0)  ; BufferLoad(srv,index,wot)
  %1978 = extractvalue %dx.types.ResRet.f32 %1977, 0
  %1979 = extractvalue %dx.types.ResRet.f32 %1977, 1
  %1980 = extractvalue %dx.types.ResRet.f32 %1977, 2
  %1981 = extractvalue %dx.types.ResRet.f32 %1977, 3
  %1982 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1894, i32 0)  ; BufferLoad(srv,index,wot)
  %1983 = extractvalue %dx.types.ResRet.f32 %1982, 0
  %1984 = extractvalue %dx.types.ResRet.f32 %1982, 1
  %1985 = extractvalue %dx.types.ResRet.f32 %1982, 2
  %1986 = extractvalue %dx.types.ResRet.f32 %1982, 3
  %1987 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1901, i32 0)  ; BufferLoad(srv,index,wot)
  %1988 = extractvalue %dx.types.ResRet.f32 %1987, 0
  %1989 = extractvalue %dx.types.ResRet.f32 %1987, 1
  %1990 = extractvalue %dx.types.ResRet.f32 %1987, 2
  %1991 = extractvalue %dx.types.ResRet.f32 %1987, 3
  %1992 = call float @dx.op.dot4.f32(i32 56, float %1970, float %1973, float %1976, float 1.000000e+00, float %1978, float %1979, float %1980, float %1981)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1993 = call float @dx.op.dot4.f32(i32 56, float %1970, float %1973, float %1976, float 1.000000e+00, float %1983, float %1984, float %1985, float %1986)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1994 = call float @dx.op.dot4.f32(i32 56, float %1970, float %1973, float %1976, float 1.000000e+00, float %1988, float %1989, float %1990, float %1991)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %1995 = fmul fast float %1992, %1872
  %1996 = call float @dx.op.tertiary.f32(i32 46, float %1993, float %1876, float %1995)  ; FMad(a,b,c)
  %1997 = call float @dx.op.tertiary.f32(i32 46, float %1994, float %1880, float %1996)  ; FMad(a,b,c)
  %1998 = fadd fast float %1997, %1884
  %1999 = fmul fast float %1992, %1873
  %2000 = call float @dx.op.tertiary.f32(i32 46, float %1993, float %1877, float %1999)  ; FMad(a,b,c)
  %2001 = call float @dx.op.tertiary.f32(i32 46, float %1994, float %1881, float %2000)  ; FMad(a,b,c)
  %2002 = fadd fast float %2001, %1885
  %2003 = fmul fast float %1992, %1874
  %2004 = call float @dx.op.tertiary.f32(i32 46, float %1993, float %1878, float %2003)  ; FMad(a,b,c)
  %2005 = call float @dx.op.tertiary.f32(i32 46, float %1994, float %1882, float %2004)  ; FMad(a,b,c)
  %2006 = fadd fast float %2005, %1886
  %2007 = fdiv fast float %1998, %2006
  %2008 = fdiv fast float %2002, %2006
  %2009 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 %1967
  %2010 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 %1967
  store float %2007, float* %2009, align 4
  store float %2008, float* %2010, align 4
  %2011 = add nuw nsw i32 %1967, 1
  %2012 = icmp eq i32 %2011, 8
  br i1 %2012, label %2013, label %1966

; <label>:2013                                    ; preds = %1966
  %2014 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 3
  %2015 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 3
  %2016 = load float, float* %2014, align 4
  %2017 = load float, float* %2015, align 4
  %2018 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 2
  %2019 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 2
  %2020 = load float, float* %2018, align 4
  %2021 = load float, float* %2019, align 4
  %2022 = call float @dx.op.binary.f32(i32 36, float %2020, float %2016)  ; FMin(a,b)
  %2023 = call float @dx.op.binary.f32(i32 36, float %2021, float %2017)  ; FMin(a,b)
  %2024 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 1
  %2025 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 1
  %2026 = load float, float* %2024, align 4
  %2027 = load float, float* %2025, align 4
  %2028 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 0
  %2029 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 0
  %2030 = load float, float* %2028, align 4
  %2031 = load float, float* %2029, align 4
  %2032 = call float @dx.op.binary.f32(i32 36, float %2030, float %2026)  ; FMin(a,b)
  %2033 = call float @dx.op.binary.f32(i32 36, float %2031, float %2027)  ; FMin(a,b)
  %2034 = call float @dx.op.binary.f32(i32 36, float %2032, float %2022)  ; FMin(a,b)
  %2035 = call float @dx.op.binary.f32(i32 36, float %2033, float %2023)  ; FMin(a,b)
  %2036 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 7
  %2037 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 7
  %2038 = load float, float* %2036, align 4
  %2039 = load float, float* %2037, align 4
  %2040 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 6
  %2041 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 6
  %2042 = load float, float* %2040, align 4
  %2043 = load float, float* %2041, align 4
  %2044 = call float @dx.op.binary.f32(i32 36, float %2042, float %2038)  ; FMin(a,b)
  %2045 = call float @dx.op.binary.f32(i32 36, float %2043, float %2039)  ; FMin(a,b)
  %2046 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 5
  %2047 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 5
  %2048 = load float, float* %2046, align 4
  %2049 = load float, float* %2047, align 4
  %2050 = getelementptr [8 x float], [8 x float]* %37, i32 0, i32 4
  %2051 = getelementptr [8 x float], [8 x float]* %38, i32 0, i32 4
  %2052 = load float, float* %2050, align 4
  %2053 = load float, float* %2051, align 4
  %2054 = call float @dx.op.binary.f32(i32 36, float %2052, float %2048)  ; FMin(a,b)
  %2055 = call float @dx.op.binary.f32(i32 36, float %2053, float %2049)  ; FMin(a,b)
  %2056 = call float @dx.op.binary.f32(i32 36, float %2054, float %2044)  ; FMin(a,b)
  %2057 = call float @dx.op.binary.f32(i32 36, float %2055, float %2045)  ; FMin(a,b)
  %2058 = call float @dx.op.binary.f32(i32 36, float %2034, float %2056)  ; FMin(a,b)
  %2059 = call float @dx.op.binary.f32(i32 36, float %2035, float %2057)  ; FMin(a,b)
  %2060 = call float @dx.op.binary.f32(i32 35, float %2020, float %2016)  ; FMax(a,b)
  %2061 = call float @dx.op.binary.f32(i32 35, float %2021, float %2017)  ; FMax(a,b)
  %2062 = call float @dx.op.binary.f32(i32 35, float %2030, float %2026)  ; FMax(a,b)
  %2063 = call float @dx.op.binary.f32(i32 35, float %2031, float %2027)  ; FMax(a,b)
  %2064 = call float @dx.op.binary.f32(i32 35, float %2062, float %2060)  ; FMax(a,b)
  %2065 = call float @dx.op.binary.f32(i32 35, float %2063, float %2061)  ; FMax(a,b)
  %2066 = call float @dx.op.binary.f32(i32 35, float %2042, float %2038)  ; FMax(a,b)
  %2067 = call float @dx.op.binary.f32(i32 35, float %2043, float %2039)  ; FMax(a,b)
  %2068 = call float @dx.op.binary.f32(i32 35, float %2052, float %2048)  ; FMax(a,b)
  %2069 = call float @dx.op.binary.f32(i32 35, float %2053, float %2049)  ; FMax(a,b)
  %2070 = call float @dx.op.binary.f32(i32 35, float %2068, float %2066)  ; FMax(a,b)
  %2071 = call float @dx.op.binary.f32(i32 35, float %2069, float %2067)  ; FMax(a,b)
  %2072 = call float @dx.op.binary.f32(i32 35, float %2064, float %2070)  ; FMax(a,b)
  %2073 = call float @dx.op.binary.f32(i32 35, float %2065, float %2071)  ; FMax(a,b)
  %2074 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2075 = extractvalue %dx.types.CBufRet.i32 %2074, 0
  %2076 = and i32 %2075, 15
  %2077 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2078 = extractvalue %dx.types.CBufRet.f32 %2077, 0
  %2079 = extractvalue %dx.types.CBufRet.f32 %2077, 1
  %2080 = extractvalue %dx.types.CBufRet.f32 %2077, 2
  %2081 = extractvalue %dx.types.CBufRet.f32 %2077, 3
  %2082 = fmul fast float %2058, 5.000000e-01
  %2083 = fmul fast float %2073, 5.000000e-01
  %2084 = fadd fast float %2082, 5.000000e-01
  %2085 = fsub fast float 5.000000e-01, %2083
  %2086 = fmul fast float %2072, 5.000000e-01
  %2087 = fmul fast float %2059, 5.000000e-01
  %2088 = fadd fast float %2086, 5.000000e-01
  %2089 = fsub fast float 5.000000e-01, %2087
  %2090 = fmul fast float %2078, %2084
  %2091 = fmul fast float %2079, %2085
  %2092 = fadd fast float %2090, %2080
  %2093 = fadd fast float %2091, %2081
  %2094 = fmul fast float %2078, %2088
  %2095 = fmul fast float %2079, %2089
  %2096 = fadd fast float %2094, %2080
  %2097 = fadd fast float %2095, %2081
  %2098 = call float @dx.op.binary.f32(i32 35, float %2092, float 0.000000e+00)  ; FMax(a,b)
  %2099 = call float @dx.op.binary.f32(i32 35, float %2093, float 0.000000e+00)  ; FMax(a,b)
  %2100 = call float @dx.op.binary.f32(i32 36, float %2098, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2101 = call float @dx.op.binary.f32(i32 36, float %2099, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2102 = call float @dx.op.binary.f32(i32 35, float %2096, float 0.000000e+00)  ; FMax(a,b)
  %2103 = call float @dx.op.binary.f32(i32 35, float %2097, float 0.000000e+00)  ; FMax(a,b)
  %2104 = call float @dx.op.binary.f32(i32 36, float %2102, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2105 = call float @dx.op.binary.f32(i32 36, float %2103, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2106 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %2107 = extractvalue %dx.types.ResRet.i32 %2106, 0
  %2108 = icmp slt i32 %2107, 1
  br i1 %2108, label %2171, label %2109

; <label>:2109                                    ; preds = %2013
  %2110 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %2111 = extractvalue %dx.types.ResRet.i32 %2110, 0
  %2112 = icmp ult i32 %2076, %2111
  br i1 %2112, label %2113, label %2171

; <label>:2113                                    ; preds = %2109
  %2114 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %2115 = extractvalue %dx.types.ResRet.i32 %2114, 0
  %2116 = uitofp i32 %2107 to float
  %2117 = uitofp i32 %2115 to float
  %2118 = fmul fast float %2116, %2100
  %2119 = fmul fast float %2117, %2101
  %2120 = fptoui float %2118 to i32
  %2121 = fptoui float %2119 to i32
  %2122 = mul i32 %2121, %2107
  %2123 = add i32 %2122, %2120
  %2124 = fmul fast float %2116, %2104
  %2125 = fptoui float %2124 to i32
  %2126 = add i32 %2122, %2125
  %2127 = fmul fast float %2117, %2105
  %2128 = fptoui float %2127 to i32
  %2129 = mul i32 %2128, %2107
  %2130 = add i32 %2129, %2120
  %2131 = mul i32 %2107, %2076
  %2132 = mul i32 %2131, %2115
  %2133 = add nsw i32 %2132, 3
  %2134 = icmp sge i32 %2123, %2107
  %2135 = srem i32 %2123, %2107
  %2136 = icmp ne i32 %2135, 0
  %2137 = and i1 %2136, %2134
  br i1 %2137, label %2138, label %2144

; <label>:2138                                    ; preds = %2113
  %2139 = xor i32 %2107, -1
  %2140 = add i32 %2133, %2139
  %2141 = add i32 %2140, %2123
  %2142 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2141, i32 undef)  ; BufferLoad(srv,index,wot)
  %2143 = extractvalue %dx.types.ResRet.i32 %2142, 0
  br label %2144

; <label>:2144                                    ; preds = %2138, %2113
  %2145 = phi i32 [ %2143, %2138 ], [ 0, %2113 ]
  %2146 = icmp slt i32 %2126, %2107
  br i1 %2146, label %2152, label %2147

; <label>:2147                                    ; preds = %2144
  %2148 = sub i32 %2133, %2107
  %2149 = add i32 %2148, %2126
  %2150 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2149, i32 undef)  ; BufferLoad(srv,index,wot)
  %2151 = extractvalue %dx.types.ResRet.i32 %2150, 0
  br label %2152

; <label>:2152                                    ; preds = %2147, %2144
  %2153 = phi i32 [ %2151, %2147 ], [ 0, %2144 ]
  %2154 = srem i32 %2130, %2107
  %2155 = icmp eq i32 %2154, 0
  br i1 %2155, label %2161, label %2156

; <label>:2156                                    ; preds = %2152
  %2157 = add i32 %2132, 2
  %2158 = add i32 %2157, %2130
  %2159 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2158, i32 undef)  ; BufferLoad(srv,index,wot)
  %2160 = extractvalue %dx.types.ResRet.i32 %2159, 0
  br label %2161

; <label>:2161                                    ; preds = %2156, %2152
  %2162 = phi i32 [ %2160, %2156 ], [ 0, %2152 ]
  %2163 = add i32 %2133, %2125
  %2164 = add i32 %2163, %2129
  %2165 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2164, i32 undef)  ; BufferLoad(srv,index,wot)
  %2166 = extractvalue %dx.types.ResRet.i32 %2165, 0
  %2167 = add i32 %2162, %2153
  %2168 = sub i32 %2166, %2167
  %2169 = sub i32 0, %2145
  %2170 = icmp eq i32 %2168, %2169
  br i1 %2170, label %2539, label %2171

; <label>:2171                                    ; preds = %2161, %2109, %2013, %1962
  br i1 %181, label %2172, label %2230

; <label>:2172                                    ; preds = %2171
  %2173 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1887, i32 0)  ; BufferLoad(srv,index,wot)
  %2174 = extractvalue %dx.types.ResRet.f32 %2173, 0
  %2175 = extractvalue %dx.types.ResRet.f32 %2173, 1
  %2176 = extractvalue %dx.types.ResRet.f32 %2173, 2
  %2177 = extractvalue %dx.types.ResRet.f32 %2173, 3
  %2178 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1894, i32 0)  ; BufferLoad(srv,index,wot)
  %2179 = extractvalue %dx.types.ResRet.f32 %2178, 0
  %2180 = extractvalue %dx.types.ResRet.f32 %2178, 1
  %2181 = extractvalue %dx.types.ResRet.f32 %2178, 2
  %2182 = extractvalue %dx.types.ResRet.f32 %2178, 3
  %2183 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %1901, i32 0)  ; BufferLoad(srv,index,wot)
  %2184 = extractvalue %dx.types.ResRet.f32 %2183, 0
  %2185 = extractvalue %dx.types.ResRet.f32 %2183, 1
  %2186 = extractvalue %dx.types.ResRet.f32 %2183, 2
  %2187 = extractvalue %dx.types.ResRet.f32 %2183, 3
  %2188 = call float @dx.op.dot3.f32(i32 55, float %2184, float %2185, float %2186, float %2184, float %2185, float %2186)  ; Dot3(ax,ay,az,bx,by,bz)
  %2189 = call float @dx.op.dot3.f32(i32 55, float %2179, float %2180, float %2181, float %2179, float %2180, float %2181)  ; Dot3(ax,ay,az,bx,by,bz)
  %2190 = call float @dx.op.binary.f32(i32 35, float %2189, float %2188)  ; FMax(a,b)
  %2191 = call float @dx.op.dot3.f32(i32 55, float %2174, float %2175, float %2176, float %2174, float %2175, float %2176)  ; Dot3(ax,ay,az,bx,by,bz)
  %2192 = call float @dx.op.binary.f32(i32 35, float %2191, float %2190)  ; FMax(a,b)
  %2193 = call float @dx.op.unary.f32(i32 24, float %2192)  ; Sqrt(value)
  %2194 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2174, float %2175, float %2176, float %2177)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2195 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2179, float %2180, float %2181, float %2182)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2196 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2184, float %2185, float %2186, float %2187)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2197 = fmul fast float %2193, %143
  %2198 = fmul fast float %2194, %1872
  %2199 = call float @dx.op.tertiary.f32(i32 46, float %2195, float %1876, float %2198)  ; FMad(a,b,c)
  %2200 = call float @dx.op.tertiary.f32(i32 46, float %2196, float %1880, float %2199)  ; FMad(a,b,c)
  %2201 = fadd fast float %2200, %1884
  %2202 = fmul fast float %2194, %1873
  %2203 = call float @dx.op.tertiary.f32(i32 46, float %2195, float %1877, float %2202)  ; FMad(a,b,c)
  %2204 = call float @dx.op.tertiary.f32(i32 46, float %2196, float %1881, float %2203)  ; FMad(a,b,c)
  %2205 = fadd fast float %2204, %1885
  %2206 = fmul fast float %2201, 5.000000e-01
  %2207 = fmul fast float %2205, 5.000000e-01
  %2208 = fadd fast float %2206, 5.000000e-01
  %2209 = fadd fast float %2207, 5.000000e-01
  %2210 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %2211 = extractvalue %dx.types.CBufRet.f32 %2210, 0
  %2212 = extractvalue %dx.types.CBufRet.f32 %2210, 1
  %2213 = fmul fast float %2211, %2208
  %2214 = fmul fast float %2209, %2212
  %2215 = call float @dx.op.unary.f32(i32 27, float %2213)  ; Round_ni(value)
  %2216 = fadd fast float %2215, 5.000000e-01
  %2217 = call float @dx.op.unary.f32(i32 27, float %2214)  ; Round_ni(value)
  %2218 = fadd fast float %2217, 5.000000e-01
  %2219 = fsub fast float %2216, %2213
  %2220 = extractvalue %dx.types.CBufRet.f32 %2210, 2
  %2221 = fmul fast float %2220, %2219
  %2222 = fsub fast float %2218, %2214
  %2223 = extractvalue %dx.types.CBufRet.f32 %2210, 3
  %2224 = fmul fast float %2223, %2222
  %2225 = fmul fast float %2221, %2221
  %2226 = fmul fast float %2224, %2224
  %2227 = fadd fast float %2226, %2225
  %2228 = fmul fast float %2197, %2197
  %2229 = fcmp uge float %2227, %2228
  br i1 %2229, label %2539, label %2230

; <label>:2230                                    ; preds = %2172, %2171
  br label %2539

; <label>:2231                                    ; preds = %1868
  %2232 = icmp eq i32 %182, 5
  br i1 %2232, label %2233, label %2539

; <label>:2233                                    ; preds = %2231
  %2234 = lshr i32 %123, 16
  %2235 = and i32 %2234, 15
  %2236 = icmp eq i32 %2235, 0
  br i1 %2236, label %2237, label %2539

; <label>:2237                                    ; preds = %2233
  %2238 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2239 = extractvalue %dx.types.CBufRet.f32 %2238, 0
  %2240 = extractvalue %dx.types.CBufRet.f32 %2238, 1
  %2241 = extractvalue %dx.types.CBufRet.f32 %2238, 3
  %2242 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %2243 = extractvalue %dx.types.CBufRet.f32 %2242, 0
  %2244 = extractvalue %dx.types.CBufRet.f32 %2242, 1
  %2245 = extractvalue %dx.types.CBufRet.f32 %2242, 3
  %2246 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %2247 = extractvalue %dx.types.CBufRet.f32 %2246, 0
  %2248 = extractvalue %dx.types.CBufRet.f32 %2246, 1
  %2249 = extractvalue %dx.types.CBufRet.f32 %2246, 3
  %2250 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %2251 = extractvalue %dx.types.CBufRet.f32 %2250, 0
  %2252 = extractvalue %dx.types.CBufRet.f32 %2250, 1
  %2253 = extractvalue %dx.types.CBufRet.f32 %2250, 3
  %2254 = add i32 %179, %177
  %2255 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2254, i32 0)  ; BufferLoad(srv,index,wot)
  %2256 = extractvalue %dx.types.ResRet.f32 %2255, 0
  %2257 = extractvalue %dx.types.ResRet.f32 %2255, 1
  %2258 = extractvalue %dx.types.ResRet.f32 %2255, 2
  %2259 = extractvalue %dx.types.ResRet.f32 %2255, 3
  %2260 = add nuw nsw i32 %177, 1
  %2261 = add i32 %2260, %179
  %2262 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2261, i32 0)  ; BufferLoad(srv,index,wot)
  %2263 = extractvalue %dx.types.ResRet.f32 %2262, 0
  %2264 = extractvalue %dx.types.ResRet.f32 %2262, 1
  %2265 = extractvalue %dx.types.ResRet.f32 %2262, 2
  %2266 = extractvalue %dx.types.ResRet.f32 %2262, 3
  %2267 = add nuw nsw i32 %177, 2
  %2268 = add i32 %2267, %179
  %2269 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2268, i32 0)  ; BufferLoad(srv,index,wot)
  %2270 = extractvalue %dx.types.ResRet.f32 %2269, 0
  %2271 = extractvalue %dx.types.ResRet.f32 %2269, 1
  %2272 = extractvalue %dx.types.ResRet.f32 %2269, 2
  %2273 = extractvalue %dx.types.ResRet.f32 %2269, 3
  %2274 = call float @dx.op.dot3.f32(i32 55, float %2270, float %2271, float %2272, float %2270, float %2271, float %2272)  ; Dot3(ax,ay,az,bx,by,bz)
  %2275 = call float @dx.op.dot3.f32(i32 55, float %2263, float %2264, float %2265, float %2263, float %2264, float %2265)  ; Dot3(ax,ay,az,bx,by,bz)
  %2276 = call float @dx.op.binary.f32(i32 35, float %2275, float %2274)  ; FMax(a,b)
  %2277 = call float @dx.op.dot3.f32(i32 55, float %2256, float %2257, float %2258, float %2256, float %2257, float %2258)  ; Dot3(ax,ay,az,bx,by,bz)
  %2278 = call float @dx.op.binary.f32(i32 35, float %2277, float %2276)  ; FMax(a,b)
  %2279 = call float @dx.op.unary.f32(i32 24, float %2278)  ; Sqrt(value)
  %2280 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2256, float %2257, float %2258, float %2259)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2281 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2263, float %2264, float %2265, float %2266)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2282 = call float @dx.op.dot4.f32(i32 56, float %140, float %141, float %142, float 1.000000e+00, float %2270, float %2271, float %2272, float %2273)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2283 = fmul fast float %2279, %143
  %2284 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %2285 = extractvalue %dx.types.CBufRet.f32 %2284, 0
  %2286 = extractvalue %dx.types.CBufRet.f32 %2284, 1
  %2287 = extractvalue %dx.types.CBufRet.f32 %2284, 2
  %2288 = extractvalue %dx.types.CBufRet.f32 %2284, 3
  %2289 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2285, float %2286, float %2287, float %2288)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2290 = fsub fast float -0.000000e+00, %2283
  %2291 = fcmp fast olt float %2289, %2290
  br i1 %2291, label %2539, label %2292

; <label>:2292                                    ; preds = %2237
  %2293 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %2294 = extractvalue %dx.types.CBufRet.f32 %2293, 0
  %2295 = extractvalue %dx.types.CBufRet.f32 %2293, 1
  %2296 = extractvalue %dx.types.CBufRet.f32 %2293, 2
  %2297 = extractvalue %dx.types.CBufRet.f32 %2293, 3
  %2298 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2294, float %2295, float %2296, float %2297)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2299 = fcmp fast olt float %2298, %2290
  br i1 %2299, label %2539, label %2723

; <label>:2300                                    ; preds = %2755
  %2301 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2256, float %2257, float %2258)  ; Dot3(ax,ay,az,bx,by,bz)
  %2302 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2263, float %2264, float %2265)  ; Dot3(ax,ay,az,bx,by,bz)
  %2303 = call float @dx.op.dot3.f32(i32 55, float %152, float %153, float %154, float %2270, float %2271, float %2272)  ; Dot3(ax,ay,az,bx,by,bz)
  %2304 = call float @dx.op.dot3.f32(i32 55, float %2301, float %2302, float %2303, float %2301, float %2302, float %2303)  ; Dot3(ax,ay,az,bx,by,bz)
  %2305 = call float @dx.op.unary.f32(i32 25, float %2304)  ; Rsqrt(value)
  %2306 = fmul fast float %2305, %2301
  %2307 = fmul fast float %2305, %2302
  %2308 = fmul fast float %2305, %2303
  %2309 = icmp eq i32 %85, 2
  %2310 = fsub fast float -0.000000e+00, %2306
  %2311 = fsub fast float -0.000000e+00, %2307
  %2312 = fsub fast float -0.000000e+00, %2308
  %2313 = select i1 %2309, float %2310, float %2306
  %2314 = select i1 %2309, float %2311, float %2307
  %2315 = select i1 %2309, float %2312, float %2308
  %2316 = fsub fast float -0.000000e+00, %2741
  %2317 = fsub fast float -0.000000e+00, %2742
  %2318 = fsub fast float -0.000000e+00, %2743
  %2319 = call float @dx.op.dot3.f32(i32 55, float %2316, float %2317, float %2318, float %2316, float %2317, float %2318)  ; Dot3(ax,ay,az,bx,by,bz)
  %2320 = call float @dx.op.unary.f32(i32 25, float %2319)  ; Rsqrt(value)
  %2321 = fmul fast float %2320, %2316
  %2322 = fmul fast float %2320, %2317
  %2323 = fmul fast float %2320, %2318
  %2324 = fsub fast float -0.000000e+00, %2313
  %2325 = fsub fast float -0.000000e+00, %2314
  %2326 = fsub fast float -0.000000e+00, %2315
  %2327 = call float @dx.op.dot3.f32(i32 55, float %2321, float %2322, float %2323, float %2324, float %2325, float %2326)  ; Dot3(ax,ay,az,bx,by,bz)
  %2328 = fcmp fast ogt float %2327, %155
  br i1 %2328, label %2539, label %2329

; <label>:2329                                    ; preds = %2755, %2300
  %2330 = and i32 %80, 512
  %2331 = icmp eq i32 %2330, 0
  br i1 %2331, label %2539, label %2332

; <label>:2332                                    ; preds = %2329
  br label %2333

; <label>:2333                                    ; preds = %2333, %2332
  %2334 = phi i32 [ %2378, %2333 ], [ 0, %2332 ]
  %2335 = and i32 %2334, 1
  %2336 = icmp ne i32 %2335, 0
  %2337 = select i1 %2336, float %173, float %163
  %2338 = and i32 %2334, 2
  %2339 = icmp ne i32 %2338, 0
  %2340 = select i1 %2339, float %174, float %164
  %2341 = and i32 %2334, 4
  %2342 = icmp ne i32 %2341, 0
  %2343 = select i1 %2342, float %175, float %165
  %2344 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2254, i32 0)  ; BufferLoad(srv,index,wot)
  %2345 = extractvalue %dx.types.ResRet.f32 %2344, 0
  %2346 = extractvalue %dx.types.ResRet.f32 %2344, 1
  %2347 = extractvalue %dx.types.ResRet.f32 %2344, 2
  %2348 = extractvalue %dx.types.ResRet.f32 %2344, 3
  %2349 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2261, i32 0)  ; BufferLoad(srv,index,wot)
  %2350 = extractvalue %dx.types.ResRet.f32 %2349, 0
  %2351 = extractvalue %dx.types.ResRet.f32 %2349, 1
  %2352 = extractvalue %dx.types.ResRet.f32 %2349, 2
  %2353 = extractvalue %dx.types.ResRet.f32 %2349, 3
  %2354 = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %12, i32 %2268, i32 0)  ; BufferLoad(srv,index,wot)
  %2355 = extractvalue %dx.types.ResRet.f32 %2354, 0
  %2356 = extractvalue %dx.types.ResRet.f32 %2354, 1
  %2357 = extractvalue %dx.types.ResRet.f32 %2354, 2
  %2358 = extractvalue %dx.types.ResRet.f32 %2354, 3
  %2359 = call float @dx.op.dot4.f32(i32 56, float %2337, float %2340, float %2343, float 1.000000e+00, float %2345, float %2346, float %2347, float %2348)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2360 = call float @dx.op.dot4.f32(i32 56, float %2337, float %2340, float %2343, float 1.000000e+00, float %2350, float %2351, float %2352, float %2353)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2361 = call float @dx.op.dot4.f32(i32 56, float %2337, float %2340, float %2343, float 1.000000e+00, float %2355, float %2356, float %2357, float %2358)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2362 = fmul fast float %2359, %2239
  %2363 = call float @dx.op.tertiary.f32(i32 46, float %2360, float %2243, float %2362)  ; FMad(a,b,c)
  %2364 = call float @dx.op.tertiary.f32(i32 46, float %2361, float %2247, float %2363)  ; FMad(a,b,c)
  %2365 = fadd fast float %2364, %2251
  %2366 = fmul fast float %2359, %2240
  %2367 = call float @dx.op.tertiary.f32(i32 46, float %2360, float %2244, float %2366)  ; FMad(a,b,c)
  %2368 = call float @dx.op.tertiary.f32(i32 46, float %2361, float %2248, float %2367)  ; FMad(a,b,c)
  %2369 = fadd fast float %2368, %2252
  %2370 = fmul fast float %2359, %2241
  %2371 = call float @dx.op.tertiary.f32(i32 46, float %2360, float %2245, float %2370)  ; FMad(a,b,c)
  %2372 = call float @dx.op.tertiary.f32(i32 46, float %2361, float %2249, float %2371)  ; FMad(a,b,c)
  %2373 = fadd fast float %2372, %2253
  %2374 = fdiv fast float %2365, %2373
  %2375 = fdiv fast float %2369, %2373
  %2376 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 %2334
  %2377 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 %2334
  store float %2374, float* %2376, align 4
  store float %2375, float* %2377, align 4
  %2378 = add nuw nsw i32 %2334, 1
  %2379 = icmp eq i32 %2378, 8
  br i1 %2379, label %2380, label %2333

; <label>:2380                                    ; preds = %2333
  %2381 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 3
  %2382 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 3
  %2383 = load float, float* %2381, align 4
  %2384 = load float, float* %2382, align 4
  %2385 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 2
  %2386 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 2
  %2387 = load float, float* %2385, align 4
  %2388 = load float, float* %2386, align 4
  %2389 = call float @dx.op.binary.f32(i32 36, float %2387, float %2383)  ; FMin(a,b)
  %2390 = call float @dx.op.binary.f32(i32 36, float %2388, float %2384)  ; FMin(a,b)
  %2391 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 1
  %2392 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 1
  %2393 = load float, float* %2391, align 4
  %2394 = load float, float* %2392, align 4
  %2395 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 0
  %2396 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 0
  %2397 = load float, float* %2395, align 4
  %2398 = load float, float* %2396, align 4
  %2399 = call float @dx.op.binary.f32(i32 36, float %2397, float %2393)  ; FMin(a,b)
  %2400 = call float @dx.op.binary.f32(i32 36, float %2398, float %2394)  ; FMin(a,b)
  %2401 = call float @dx.op.binary.f32(i32 36, float %2399, float %2389)  ; FMin(a,b)
  %2402 = call float @dx.op.binary.f32(i32 36, float %2400, float %2390)  ; FMin(a,b)
  %2403 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 7
  %2404 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 7
  %2405 = load float, float* %2403, align 4
  %2406 = load float, float* %2404, align 4
  %2407 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 6
  %2408 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 6
  %2409 = load float, float* %2407, align 4
  %2410 = load float, float* %2408, align 4
  %2411 = call float @dx.op.binary.f32(i32 36, float %2409, float %2405)  ; FMin(a,b)
  %2412 = call float @dx.op.binary.f32(i32 36, float %2410, float %2406)  ; FMin(a,b)
  %2413 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 5
  %2414 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 5
  %2415 = load float, float* %2413, align 4
  %2416 = load float, float* %2414, align 4
  %2417 = getelementptr [8 x float], [8 x float]* %29, i32 0, i32 4
  %2418 = getelementptr [8 x float], [8 x float]* %30, i32 0, i32 4
  %2419 = load float, float* %2417, align 4
  %2420 = load float, float* %2418, align 4
  %2421 = call float @dx.op.binary.f32(i32 36, float %2419, float %2415)  ; FMin(a,b)
  %2422 = call float @dx.op.binary.f32(i32 36, float %2420, float %2416)  ; FMin(a,b)
  %2423 = call float @dx.op.binary.f32(i32 36, float %2421, float %2411)  ; FMin(a,b)
  %2424 = call float @dx.op.binary.f32(i32 36, float %2422, float %2412)  ; FMin(a,b)
  %2425 = call float @dx.op.binary.f32(i32 36, float %2401, float %2423)  ; FMin(a,b)
  %2426 = call float @dx.op.binary.f32(i32 36, float %2402, float %2424)  ; FMin(a,b)
  %2427 = call float @dx.op.binary.f32(i32 35, float %2387, float %2383)  ; FMax(a,b)
  %2428 = call float @dx.op.binary.f32(i32 35, float %2388, float %2384)  ; FMax(a,b)
  %2429 = call float @dx.op.binary.f32(i32 35, float %2397, float %2393)  ; FMax(a,b)
  %2430 = call float @dx.op.binary.f32(i32 35, float %2398, float %2394)  ; FMax(a,b)
  %2431 = call float @dx.op.binary.f32(i32 35, float %2429, float %2427)  ; FMax(a,b)
  %2432 = call float @dx.op.binary.f32(i32 35, float %2430, float %2428)  ; FMax(a,b)
  %2433 = call float @dx.op.binary.f32(i32 35, float %2409, float %2405)  ; FMax(a,b)
  %2434 = call float @dx.op.binary.f32(i32 35, float %2410, float %2406)  ; FMax(a,b)
  %2435 = call float @dx.op.binary.f32(i32 35, float %2419, float %2415)  ; FMax(a,b)
  %2436 = call float @dx.op.binary.f32(i32 35, float %2420, float %2416)  ; FMax(a,b)
  %2437 = call float @dx.op.binary.f32(i32 35, float %2435, float %2433)  ; FMax(a,b)
  %2438 = call float @dx.op.binary.f32(i32 35, float %2436, float %2434)  ; FMax(a,b)
  %2439 = call float @dx.op.binary.f32(i32 35, float %2431, float %2437)  ; FMax(a,b)
  %2440 = call float @dx.op.binary.f32(i32 35, float %2432, float %2438)  ; FMax(a,b)
  %2441 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %2442 = extractvalue %dx.types.CBufRet.i32 %2441, 0
  %2443 = and i32 %2442, 15
  %2444 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %2445 = extractvalue %dx.types.CBufRet.f32 %2444, 0
  %2446 = extractvalue %dx.types.CBufRet.f32 %2444, 1
  %2447 = extractvalue %dx.types.CBufRet.f32 %2444, 2
  %2448 = extractvalue %dx.types.CBufRet.f32 %2444, 3
  %2449 = fmul fast float %2425, 5.000000e-01
  %2450 = fmul fast float %2440, 5.000000e-01
  %2451 = fadd fast float %2449, 5.000000e-01
  %2452 = fsub fast float 5.000000e-01, %2450
  %2453 = fmul fast float %2439, 5.000000e-01
  %2454 = fmul fast float %2426, 5.000000e-01
  %2455 = fadd fast float %2453, 5.000000e-01
  %2456 = fsub fast float 5.000000e-01, %2454
  %2457 = fmul fast float %2445, %2451
  %2458 = fmul fast float %2446, %2452
  %2459 = fadd fast float %2457, %2447
  %2460 = fadd fast float %2458, %2448
  %2461 = fmul fast float %2445, %2455
  %2462 = fmul fast float %2446, %2456
  %2463 = fadd fast float %2461, %2447
  %2464 = fadd fast float %2462, %2448
  %2465 = call float @dx.op.binary.f32(i32 35, float %2459, float 0.000000e+00)  ; FMax(a,b)
  %2466 = call float @dx.op.binary.f32(i32 35, float %2460, float 0.000000e+00)  ; FMax(a,b)
  %2467 = call float @dx.op.binary.f32(i32 36, float %2465, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2468 = call float @dx.op.binary.f32(i32 36, float %2466, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2469 = call float @dx.op.binary.f32(i32 35, float %2463, float 0.000000e+00)  ; FMax(a,b)
  %2470 = call float @dx.op.binary.f32(i32 35, float %2464, float 0.000000e+00)  ; FMax(a,b)
  %2471 = call float @dx.op.binary.f32(i32 36, float %2469, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2472 = call float @dx.op.binary.f32(i32 36, float %2470, float 0x3FEFF7CEE0000000)  ; FMin(a,b)
  %2473 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
  %2474 = extractvalue %dx.types.ResRet.i32 %2473, 0
  %2475 = icmp slt i32 %2474, 1
  br i1 %2475, label %2539, label %2476

; <label>:2476                                    ; preds = %2380
  %2477 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 2, i32 undef)  ; BufferLoad(srv,index,wot)
  %2478 = extractvalue %dx.types.ResRet.i32 %2477, 0
  %2479 = icmp ult i32 %2443, %2478
  br i1 %2479, label %2480, label %2539

; <label>:2480                                    ; preds = %2476
  %2481 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 1, i32 undef)  ; BufferLoad(srv,index,wot)
  %2482 = extractvalue %dx.types.ResRet.i32 %2481, 0
  %2483 = uitofp i32 %2474 to float
  %2484 = uitofp i32 %2482 to float
  %2485 = fmul fast float %2483, %2467
  %2486 = fmul fast float %2484, %2468
  %2487 = fptoui float %2485 to i32
  %2488 = fptoui float %2486 to i32
  %2489 = mul i32 %2488, %2474
  %2490 = add i32 %2489, %2487
  %2491 = fmul fast float %2483, %2471
  %2492 = fptoui float %2491 to i32
  %2493 = add i32 %2489, %2492
  %2494 = fmul fast float %2484, %2472
  %2495 = fptoui float %2494 to i32
  %2496 = mul i32 %2495, %2474
  %2497 = add i32 %2496, %2487
  %2498 = mul i32 %2474, %2443
  %2499 = mul i32 %2498, %2482
  %2500 = add nsw i32 %2499, 3
  %2501 = icmp sge i32 %2490, %2474
  %2502 = srem i32 %2490, %2474
  %2503 = icmp ne i32 %2502, 0
  %2504 = and i1 %2503, %2501
  br i1 %2504, label %2505, label %2511

; <label>:2505                                    ; preds = %2480
  %2506 = xor i32 %2474, -1
  %2507 = add i32 %2500, %2506
  %2508 = add i32 %2507, %2490
  %2509 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2508, i32 undef)  ; BufferLoad(srv,index,wot)
  %2510 = extractvalue %dx.types.ResRet.i32 %2509, 0
  br label %2511

; <label>:2511                                    ; preds = %2505, %2480
  %2512 = phi i32 [ %2510, %2505 ], [ 0, %2480 ]
  %2513 = icmp slt i32 %2493, %2474
  br i1 %2513, label %2519, label %2514

; <label>:2514                                    ; preds = %2511
  %2515 = sub i32 %2500, %2474
  %2516 = add i32 %2515, %2493
  %2517 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2516, i32 undef)  ; BufferLoad(srv,index,wot)
  %2518 = extractvalue %dx.types.ResRet.i32 %2517, 0
  br label %2519

; <label>:2519                                    ; preds = %2514, %2511
  %2520 = phi i32 [ %2518, %2514 ], [ 0, %2511 ]
  %2521 = srem i32 %2497, %2474
  %2522 = icmp eq i32 %2521, 0
  br i1 %2522, label %2528, label %2523

; <label>:2523                                    ; preds = %2519
  %2524 = add i32 %2499, 2
  %2525 = add i32 %2524, %2497
  %2526 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2525, i32 undef)  ; BufferLoad(srv,index,wot)
  %2527 = extractvalue %dx.types.ResRet.i32 %2526, 0
  br label %2528

; <label>:2528                                    ; preds = %2523, %2519
  %2529 = phi i32 [ %2527, %2523 ], [ 0, %2519 ]
  %2530 = add i32 %2500, %2492
  %2531 = add i32 %2530, %2496
  %2532 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %7, i32 %2531, i32 undef)  ; BufferLoad(srv,index,wot)
  %2533 = extractvalue %dx.types.ResRet.i32 %2532, 0
  %2534 = add i32 %2529, %2520
  %2535 = sub i32 %2533, %2534
  %2536 = sub i32 0, %2512
  %2537 = icmp eq i32 %2535, %2536
  %2538 = select i1 %2537, i32 11, i32 0
  br label %2539

; <label>:2539                                    ; preds = %2747, %2739, %2731, %2723, %2713, %2705, %2697, %2689, %2671, %2663, %2655, %2647, %2637, %2629, %2621, %2613, %2528, %2476, %2380, %2329, %2300, %2292, %2237, %2233, %2231, %2230, %2172, %2161, %1933, %1925, %1870, %1867, %1865, %1402, %1399, %1397, %898, %880, %616, %608, %547, %544, %486, %475, %247, %239, %184
  %2540 = phi i32 [ %2235, %2233 ], [ %120, %2231 ], [ 0, %544 ], [ 2, %247 ], [ 11, %475 ], [ 10, %486 ], [ 0, %1399 ], [ 2, %616 ], [ 11, %880 ], [ 10, %898 ], [ 3, %1397 ], [ 0, %1867 ], [ 3, %1865 ], [ 15, %1402 ], [ 0, %2230 ], [ 2, %1933 ], [ 11, %2161 ], [ 10, %2172 ], [ 2, %2300 ], [ 0, %2476 ], [ 0, %2380 ], [ 0, %2329 ], [ %2538, %2528 ], [ 1, %2637 ], [ 1, %2629 ], [ 1, %2621 ], [ 1, %2613 ], [ 1, %239 ], [ 1, %184 ], [ 1, %2671 ], [ 1, %2663 ], [ 1, %2655 ], [ 1, %2647 ], [ 1, %608 ], [ 1, %547 ], [ 1, %2713 ], [ 1, %2705 ], [ 1, %2697 ], [ 1, %2689 ], [ 1, %1925 ], [ 1, %1870 ], [ 1, %2747 ], [ 1, %2739 ], [ 1, %2731 ], [ 1, %2723 ], [ 1, %2292 ], [ 1, %2237 ]
  %2541 = icmp eq i32 %2540, 0
  br label %2542

; <label>:2542                                    ; preds = %2539, %87
  %2543 = phi i1 [ %2541, %2539 ], [ false, %87 ]
  %2544 = phi i32 [ %2540, %2539 ], [ %120, %87 ]
  br i1 %2543, label %2545, label %2599

; <label>:2545                                    ; preds = %2542
  %2546 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %10, i32 %77, i32 0)  ; BufferLoad(srv,index,wot)
  %2547 = extractvalue %dx.types.ResRet.i32 %2546, 0
  %2548 = add nuw nsw i32 %77, 1
  %2549 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %10, i32 %2548, i32 0)  ; BufferLoad(srv,index,wot)
  %2550 = extractvalue %dx.types.ResRet.i32 %2549, 0
  %2551 = icmp ugt i32 %2550, %2547
  %2552 = sub i32 %2550, %2547
  %2553 = select i1 %2551, i32 %2552, i32 0
  %2554 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %2555 = extractvalue %dx.types.CBufRet.i32 %2554, 0
  %2556 = shl i32 %76, 2
  %2557 = add i32 %2555, %2556
  %2558 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %5, i32 0, i32 %2557, i32 undef, i32 undef, i32 1)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2559 = icmp ult i32 %2558, %2553
  br i1 %2559, label %2560, label %2589

; <label>:2560                                    ; preds = %2545
  %2561 = shl nuw nsw i32 %46, 6
  %2562 = and i32 %40, 63
  %2563 = or i32 %2561, %2562
  %2564 = add i32 %2558, %2547
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %2564, i32 0, i32 %2563, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %4, i32 %2564, i32 4, i32 %42, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %2565 = shl i32 %68, 4
  %2566 = add i32 %52, %50
  %2567 = add i32 %2566, %2565
  %2568 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %66, i32 %2567, i32 undef)  ; BufferLoad(srv,index,wot)
  %2569 = extractvalue %dx.types.ResRet.i32 %2568, 1
  %2570 = mul nuw i32 %77, 3
  %2571 = add i32 %2570, 2
  %2572 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %2, i32 0, i32 %2571, i32 undef, i32 undef, i32 %2569)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2573 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %3, i32 0, i32 0, i32 undef, i32 undef, i32 1)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2574 = shl i32 %2573, 3
  %2575 = add i32 %2574, 8
  %2576 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2577 = extractvalue %dx.types.CBufRet.i32 %2576, 1
  %2578 = icmp ugt i32 %2575, %2577
  br i1 %2578, label %2579, label %2586

; <label>:2579                                    ; preds = %2560
  %2580 = add i32 %2570, 1
  %2581 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %2, i32 6, i32 %2580, i32 undef, i32 undef, i32 %2558)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2582 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %2583 = extractvalue %dx.types.CBufRet.i32 %2582, 1
  %2584 = lshr i32 %2583, 3
  %2585 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %3, i32 6, i32 0, i32 undef, i32 undef, i32 %2584)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  br label %2599

; <label>:2586                                    ; preds = %2560
  %2587 = shl i32 %2573, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %2587, i32 undef, i32 %77, i32 %77, i32 %77, i32 %77, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  %2588 = or i32 %2587, 1
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %2588, i32 undef, i32 %2558, i32 %2558, i32 %2558, i32 %2558, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %2599

; <label>:2589                                    ; preds = %2545
  %2590 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %2591 = extractvalue %dx.types.CBufRet.i32 %2590, 0
  %2592 = add i32 %2591, %2556
  %2593 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %5, i32 6, i32 %2592, i32 undef, i32 undef, i32 %2553)  ; AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)
  %2594 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %2595 = extractvalue %dx.types.CBufRet.i32 %2594, 1
  %2596 = and i32 %2595, 15
  %2597 = icmp eq i32 %2596, 1
  %2598 = select i1 %2597, i32 3, i32 %2544
  br label %2599

; <label>:2599                                    ; preds = %2589, %2586, %2579, %2542
  %2600 = phi i32 [ %2544, %2579 ], [ %2544, %2586 ], [ %2544, %2542 ], [ %2598, %2589 ]
  %2601 = icmp eq i32 %2600, 15
  br i1 %2601, label %2612, label %2602

; <label>:2602                                    ; preds = %2599
  %2603 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %16, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %2604 = extractvalue %dx.types.CBufRet.i32 %2603, 3
  %2605 = and i32 %2604, 31
  %2606 = shl i32 15, %2605
  %2607 = xor i32 %2606, -1
  %2608 = and i32 %123, %2607
  %2609 = and i32 %2600, 15
  %2610 = shl i32 %2609, %2605
  %2611 = or i32 %2608, %2610
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %6, i32 %121, i32 0, i32 %2611, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  br label %2612

; <label>:2612                                    ; preds = %2602, %2599, %44, %0
  ret void

; <label>:2613                                    ; preds = %239
  %2614 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2615 = extractvalue %dx.types.CBufRet.f32 %2614, 0
  %2616 = extractvalue %dx.types.CBufRet.f32 %2614, 1
  %2617 = extractvalue %dx.types.CBufRet.f32 %2614, 2
  %2618 = extractvalue %dx.types.CBufRet.f32 %2614, 3
  %2619 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %2615, float %2616, float %2617, float %2618)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2620 = fcmp fast olt float %2619, %237
  br i1 %2620, label %2539, label %2621

; <label>:2621                                    ; preds = %2613
  %2622 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2623 = extractvalue %dx.types.CBufRet.f32 %2622, 0
  %2624 = extractvalue %dx.types.CBufRet.f32 %2622, 1
  %2625 = extractvalue %dx.types.CBufRet.f32 %2622, 2
  %2626 = extractvalue %dx.types.CBufRet.f32 %2622, 3
  %2627 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %2623, float %2624, float %2625, float %2626)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2628 = fcmp fast olt float %2627, %237
  br i1 %2628, label %2539, label %2629

; <label>:2629                                    ; preds = %2621
  %2630 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2631 = extractvalue %dx.types.CBufRet.f32 %2630, 0
  %2632 = extractvalue %dx.types.CBufRet.f32 %2630, 1
  %2633 = extractvalue %dx.types.CBufRet.f32 %2630, 2
  %2634 = extractvalue %dx.types.CBufRet.f32 %2630, 3
  %2635 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %2631, float %2632, float %2633, float %2634)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2636 = fcmp fast olt float %2635, %237
  br i1 %2636, label %2539, label %2637

; <label>:2637                                    ; preds = %2629
  %2638 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2639 = extractvalue %dx.types.CBufRet.f32 %2638, 0
  %2640 = extractvalue %dx.types.CBufRet.f32 %2638, 1
  %2641 = extractvalue %dx.types.CBufRet.f32 %2638, 2
  %2642 = extractvalue %dx.types.CBufRet.f32 %2638, 3
  %2643 = call float @dx.op.dot4.f32(i32 56, float %227, float %228, float %229, float 1.000000e+00, float %2639, float %2640, float %2641, float %2642)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2644 = fcmp fast olt float %2643, %237
  br i1 %2644, label %2539, label %2645

; <label>:2645                                    ; preds = %2637
  %2646 = icmp eq i32 %85, 0
  br i1 %2646, label %276, label %247

; <label>:2647                                    ; preds = %608
  %2648 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2649 = extractvalue %dx.types.CBufRet.f32 %2648, 0
  %2650 = extractvalue %dx.types.CBufRet.f32 %2648, 1
  %2651 = extractvalue %dx.types.CBufRet.f32 %2648, 2
  %2652 = extractvalue %dx.types.CBufRet.f32 %2648, 3
  %2653 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %2649, float %2650, float %2651, float %2652)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2654 = fcmp fast olt float %2653, %606
  br i1 %2654, label %2539, label %2655

; <label>:2655                                    ; preds = %2647
  %2656 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2657 = extractvalue %dx.types.CBufRet.f32 %2656, 0
  %2658 = extractvalue %dx.types.CBufRet.f32 %2656, 1
  %2659 = extractvalue %dx.types.CBufRet.f32 %2656, 2
  %2660 = extractvalue %dx.types.CBufRet.f32 %2656, 3
  %2661 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %2657, float %2658, float %2659, float %2660)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2662 = fcmp fast olt float %2661, %606
  br i1 %2662, label %2539, label %2663

; <label>:2663                                    ; preds = %2655
  %2664 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2665 = extractvalue %dx.types.CBufRet.f32 %2664, 0
  %2666 = extractvalue %dx.types.CBufRet.f32 %2664, 1
  %2667 = extractvalue %dx.types.CBufRet.f32 %2664, 2
  %2668 = extractvalue %dx.types.CBufRet.f32 %2664, 3
  %2669 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %2665, float %2666, float %2667, float %2668)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2670 = fcmp fast olt float %2669, %606
  br i1 %2670, label %2539, label %2671

; <label>:2671                                    ; preds = %2663
  %2672 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2673 = extractvalue %dx.types.CBufRet.f32 %2672, 0
  %2674 = extractvalue %dx.types.CBufRet.f32 %2672, 1
  %2675 = extractvalue %dx.types.CBufRet.f32 %2672, 2
  %2676 = extractvalue %dx.types.CBufRet.f32 %2672, 3
  %2677 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %2673, float %2674, float %2675, float %2676)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2678 = fcmp fast olt float %2677, %606
  br i1 %2678, label %2539, label %2679

; <label>:2679                                    ; preds = %2671
  %2680 = or i32 %569, 4
  %2681 = add nuw nsw i32 %2680, 5
  %2682 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 %2681)  ; CBufferLoadLegacy(handle,regIndex)
  %2683 = extractvalue %dx.types.CBufRet.f32 %2682, 0
  %2684 = extractvalue %dx.types.CBufRet.f32 %2682, 1
  %2685 = extractvalue %dx.types.CBufRet.f32 %2682, 2
  %2686 = extractvalue %dx.types.CBufRet.f32 %2682, 3
  %2687 = call float @dx.op.dot4.f32(i32 56, float %596, float %597, float %598, float 1.000000e+00, float %2683, float %2684, float %2685, float %2686)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2688 = icmp eq i32 %85, 0
  br i1 %2688, label %645, label %616

; <label>:2689                                    ; preds = %1925
  %2690 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2691 = extractvalue %dx.types.CBufRet.f32 %2690, 0
  %2692 = extractvalue %dx.types.CBufRet.f32 %2690, 1
  %2693 = extractvalue %dx.types.CBufRet.f32 %2690, 2
  %2694 = extractvalue %dx.types.CBufRet.f32 %2690, 3
  %2695 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %2691, float %2692, float %2693, float %2694)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2696 = fcmp fast olt float %2695, %1923
  br i1 %2696, label %2539, label %2697

; <label>:2697                                    ; preds = %2689
  %2698 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2699 = extractvalue %dx.types.CBufRet.f32 %2698, 0
  %2700 = extractvalue %dx.types.CBufRet.f32 %2698, 1
  %2701 = extractvalue %dx.types.CBufRet.f32 %2698, 2
  %2702 = extractvalue %dx.types.CBufRet.f32 %2698, 3
  %2703 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %2699, float %2700, float %2701, float %2702)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2704 = fcmp fast olt float %2703, %1923
  br i1 %2704, label %2539, label %2705

; <label>:2705                                    ; preds = %2697
  %2706 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2707 = extractvalue %dx.types.CBufRet.f32 %2706, 0
  %2708 = extractvalue %dx.types.CBufRet.f32 %2706, 1
  %2709 = extractvalue %dx.types.CBufRet.f32 %2706, 2
  %2710 = extractvalue %dx.types.CBufRet.f32 %2706, 3
  %2711 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %2707, float %2708, float %2709, float %2710)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2712 = fcmp fast olt float %2711, %1923
  br i1 %2712, label %2539, label %2713

; <label>:2713                                    ; preds = %2705
  %2714 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2715 = extractvalue %dx.types.CBufRet.f32 %2714, 0
  %2716 = extractvalue %dx.types.CBufRet.f32 %2714, 1
  %2717 = extractvalue %dx.types.CBufRet.f32 %2714, 2
  %2718 = extractvalue %dx.types.CBufRet.f32 %2714, 3
  %2719 = call float @dx.op.dot4.f32(i32 56, float %1913, float %1914, float %1915, float 1.000000e+00, float %2715, float %2716, float %2717, float %2718)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2720 = fcmp fast olt float %2719, %1923
  br i1 %2720, label %2539, label %2721

; <label>:2721                                    ; preds = %2713
  %2722 = icmp eq i32 %85, 0
  br i1 %2722, label %1962, label %1933

; <label>:2723                                    ; preds = %2292
  %2724 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2725 = extractvalue %dx.types.CBufRet.f32 %2724, 0
  %2726 = extractvalue %dx.types.CBufRet.f32 %2724, 1
  %2727 = extractvalue %dx.types.CBufRet.f32 %2724, 2
  %2728 = extractvalue %dx.types.CBufRet.f32 %2724, 3
  %2729 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2725, float %2726, float %2727, float %2728)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2730 = fcmp fast olt float %2729, %2290
  br i1 %2730, label %2539, label %2731

; <label>:2731                                    ; preds = %2723
  %2732 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2733 = extractvalue %dx.types.CBufRet.f32 %2732, 0
  %2734 = extractvalue %dx.types.CBufRet.f32 %2732, 1
  %2735 = extractvalue %dx.types.CBufRet.f32 %2732, 2
  %2736 = extractvalue %dx.types.CBufRet.f32 %2732, 3
  %2737 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2733, float %2734, float %2735, float %2736)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2738 = fcmp fast olt float %2737, %2290
  br i1 %2738, label %2539, label %2739

; <label>:2739                                    ; preds = %2731
  %2740 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %2741 = extractvalue %dx.types.CBufRet.f32 %2740, 0
  %2742 = extractvalue %dx.types.CBufRet.f32 %2740, 1
  %2743 = extractvalue %dx.types.CBufRet.f32 %2740, 2
  %2744 = extractvalue %dx.types.CBufRet.f32 %2740, 3
  %2745 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2741, float %2742, float %2743, float %2744)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2746 = fcmp fast olt float %2745, %2290
  br i1 %2746, label %2539, label %2747

; <label>:2747                                    ; preds = %2739
  %2748 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %16, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %2749 = extractvalue %dx.types.CBufRet.f32 %2748, 0
  %2750 = extractvalue %dx.types.CBufRet.f32 %2748, 1
  %2751 = extractvalue %dx.types.CBufRet.f32 %2748, 2
  %2752 = extractvalue %dx.types.CBufRet.f32 %2748, 3
  %2753 = call float @dx.op.dot4.f32(i32 56, float %2280, float %2281, float %2282, float 1.000000e+00, float %2749, float %2750, float %2751, float %2752)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %2754 = fcmp fast olt float %2753, %2290
  br i1 %2754, label %2539, label %2755

; <label>:2755                                    ; preds = %2747
  %2756 = icmp eq i32 %85, 0
  br i1 %2756, label %2329, label %2300
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

