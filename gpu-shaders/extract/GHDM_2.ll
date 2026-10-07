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
; shader debug name: 347f7f9dae13a4f86cc3e253a9d179ec.pdb
; shader hash: 347f7f9dae13a4f86cc3e253a9d179ec
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
;       uint nSrcSlice;                               ; Offset:  120
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
; __smptSrcArray                    sampler      NA          NA      S0             s8     1
; tSrcArray                         texture     f32     2darray      T0             t8     1
; uDst                                  UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%"class.Texture2DArray<vector<float, 4> >" = type { <4 x float>, %"class.Texture2DArray<vector<float, 4> >::mips_type" }
%"class.Texture2DArray<vector<float, 4> >::mips_type" = type { i32 }
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
  br i1 %25, label %26, label %183

; <label>:26                                      ; preds = %0
  %27 = urem i32 %12, %20
  %28 = urem i32 %14, %22
  %29 = or i32 %28, %27
  %30 = icmp eq i32 %29, 0
  %31 = extractvalue %dx.types.CBufRet.i32 %21, 1
  %32 = icmp eq i32 %31, 0
  br i1 %30, label %33, label %93

; <label>:33                                      ; preds = %26
  %34 = udiv i32 %12, %20
  %35 = udiv i32 %14, %22
  %36 = icmp eq i32 %35, 0
  br i1 %32, label %65, label %37

; <label>:37                                      ; preds = %33
  br i1 %36, label %179, label %38

; <label>:38                                      ; preds = %37
  br label %39

; <label>:39                                      ; preds = %61, %38
  %40 = phi float [ %62, %61 ], [ 2.000000e+00, %38 ]
  %41 = phi i32 [ %63, %61 ], [ 0, %38 ]
  %42 = icmp eq i32 %34, 0
  br i1 %42, label %61, label %43

; <label>:43                                      ; preds = %39
  br label %44

; <label>:44                                      ; preds = %44, %43
  %45 = phi float [ %57, %44 ], [ %40, %43 ]
  %46 = phi i32 [ %58, %44 ], [ 0, %43 ]
  %47 = mul i32 %34, %5
  %48 = add i32 %47, %8
  %49 = add i32 %48, %46
  %50 = mul i32 %35, %6
  %51 = add i32 %50, %10
  %52 = add i32 %51, %41
  %53 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %54 = extractvalue %dx.types.CBufRet.i32 %53, 2
  %55 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %49, i32 %52, i32 %54, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %56 = extractvalue %dx.types.ResRet.f32 %55, 0
  %57 = call float @dx.op.binary.f32(i32 36, float %56, float %45)  ; FMin(a,b)
  %58 = add nuw i32 %46, 1
  %59 = icmp ult i32 %58, %34
  br i1 %59, label %44, label %60

; <label>:60                                      ; preds = %44
  br label %61

; <label>:61                                      ; preds = %60, %39
  %62 = phi float [ %40, %39 ], [ %57, %60 ]
  %63 = add nuw i32 %41, 1
  %64 = icmp ult i32 %63, %35
  br i1 %64, label %39, label %176

; <label>:65                                      ; preds = %33
  br i1 %36, label %179, label %66

; <label>:66                                      ; preds = %65
  br label %67

; <label>:67                                      ; preds = %89, %66
  %68 = phi float [ %90, %89 ], [ -2.000000e+00, %66 ]
  %69 = phi i32 [ %91, %89 ], [ 0, %66 ]
  %70 = icmp eq i32 %34, 0
  br i1 %70, label %89, label %71

; <label>:71                                      ; preds = %67
  br label %72

; <label>:72                                      ; preds = %72, %71
  %73 = phi float [ %85, %72 ], [ %68, %71 ]
  %74 = phi i32 [ %86, %72 ], [ 0, %71 ]
  %75 = mul i32 %34, %5
  %76 = add i32 %75, %8
  %77 = add i32 %76, %74
  %78 = mul i32 %35, %6
  %79 = add i32 %78, %10
  %80 = add i32 %79, %69
  %81 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %4, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %82 = extractvalue %dx.types.CBufRet.i32 %81, 2
  %83 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %77, i32 %80, i32 %82, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %84 = extractvalue %dx.types.ResRet.f32 %83, 0
  %85 = call float @dx.op.binary.f32(i32 35, float %84, float %73)  ; FMax(a,b)
  %86 = add nuw i32 %74, 1
  %87 = icmp ult i32 %86, %34
  br i1 %87, label %72, label %88

; <label>:88                                      ; preds = %72
  br label %89

; <label>:89                                      ; preds = %88, %67
  %90 = phi float [ %68, %67 ], [ %85, %88 ]
  %91 = add nuw i32 %69, 1
  %92 = icmp ult i32 %91, %35
  br i1 %92, label %67, label %175

; <label>:93                                      ; preds = %26
  %94 = uitofp i32 %8 to float
  %95 = uitofp i32 %10 to float
  %96 = uitofp i32 %20 to float
  %97 = uitofp i32 %22 to float
  %98 = uitofp i32 %5 to float
  %99 = uitofp i32 %6 to float
  %100 = fadd fast float %98, 5.000000e-01
  %101 = fadd fast float %99, 5.000000e-01
  %102 = fdiv fast float %100, %96
  %103 = fdiv fast float %101, %97
  %104 = uitofp i32 %12 to float
  %105 = uitofp i32 %14 to float
  %106 = fdiv fast float %104, %96
  %107 = fdiv fast float %105, %97
  %108 = call float @dx.op.unary.f32(i32 28, float %106)  ; Round_pi(value)
  %109 = call float @dx.op.unary.f32(i32 28, float %107)  ; Round_pi(value)
  %110 = fmul fast float %108, %96
  %111 = fmul fast float %109, %97
  %112 = fdiv fast float 1.000000e+00, %110
  %113 = fdiv fast float 1.000000e+00, %111
  %114 = fptoui float %108 to i32
  %115 = fptoui float %109 to i32
  %116 = extractvalue %dx.types.CBufRet.i32 %21, 2
  %117 = uitofp i32 %116 to float
  %118 = icmp eq i32 %115, 0
  br i1 %32, label %147, label %119

; <label>:119                                     ; preds = %93
  br i1 %118, label %179, label %120

; <label>:120                                     ; preds = %119
  br label %121

; <label>:121                                     ; preds = %143, %120
  %122 = phi float [ %144, %143 ], [ 2.000000e+00, %120 ]
  %123 = phi i32 [ %145, %143 ], [ 0, %120 ]
  %124 = icmp eq i32 %114, 0
  br i1 %124, label %143, label %125

; <label>:125                                     ; preds = %121
  br label %126

; <label>:126                                     ; preds = %126, %125
  %127 = phi float [ %139, %126 ], [ %122, %125 ]
  %128 = phi i32 [ %140, %126 ], [ 0, %125 ]
  %129 = fadd fast float %102, %94
  %130 = uitofp i32 %128 to float
  %131 = fmul fast float %130, %112
  %132 = fadd fast float %129, %131
  %133 = fadd fast float %103, %95
  %134 = uitofp i32 %123 to float
  %135 = fmul fast float %134, %113
  %136 = fadd fast float %133, %135
  %137 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %132, float %136, float %117, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %138 = extractvalue %dx.types.ResRet.f32 %137, 0
  %139 = call float @dx.op.binary.f32(i32 36, float %138, float %127)  ; FMin(a,b)
  %140 = add nuw i32 %128, 1
  %141 = icmp eq i32 %140, %114
  br i1 %141, label %142, label %126

; <label>:142                                     ; preds = %126
  br label %143

; <label>:143                                     ; preds = %142, %121
  %144 = phi float [ %122, %121 ], [ %139, %142 ]
  %145 = add nuw i32 %123, 1
  %146 = icmp eq i32 %145, %115
  br i1 %146, label %178, label %121

; <label>:147                                     ; preds = %93
  br i1 %118, label %179, label %148

; <label>:148                                     ; preds = %147
  br label %149

; <label>:149                                     ; preds = %171, %148
  %150 = phi float [ %172, %171 ], [ -2.000000e+00, %148 ]
  %151 = phi i32 [ %173, %171 ], [ 0, %148 ]
  %152 = icmp eq i32 %114, 0
  br i1 %152, label %171, label %153

; <label>:153                                     ; preds = %149
  br label %154

; <label>:154                                     ; preds = %154, %153
  %155 = phi float [ %167, %154 ], [ %150, %153 ]
  %156 = phi i32 [ %168, %154 ], [ 0, %153 ]
  %157 = fadd fast float %102, %94
  %158 = uitofp i32 %156 to float
  %159 = fmul fast float %158, %112
  %160 = fadd fast float %157, %159
  %161 = fadd fast float %103, %95
  %162 = uitofp i32 %151 to float
  %163 = fmul fast float %162, %113
  %164 = fadd fast float %161, %163
  %165 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %160, float %164, float %117, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %166 = extractvalue %dx.types.ResRet.f32 %165, 0
  %167 = call float @dx.op.binary.f32(i32 35, float %166, float %155)  ; FMax(a,b)
  %168 = add nuw i32 %156, 1
  %169 = icmp eq i32 %168, %114
  br i1 %169, label %170, label %154

; <label>:170                                     ; preds = %154
  br label %171

; <label>:171                                     ; preds = %170, %149
  %172 = phi float [ %150, %149 ], [ %167, %170 ]
  %173 = add nuw i32 %151, 1
  %174 = icmp eq i32 %173, %115
  br i1 %174, label %177, label %149

; <label>:175                                     ; preds = %89
  br label %179

; <label>:176                                     ; preds = %61
  br label %179

; <label>:177                                     ; preds = %171
  br label %179

; <label>:178                                     ; preds = %143
  br label %179

; <label>:179                                     ; preds = %178, %177, %176, %175, %147, %119, %65, %37
  %180 = phi float [ 2.000000e+00, %37 ], [ -2.000000e+00, %65 ], [ 2.000000e+00, %119 ], [ -2.000000e+00, %147 ], [ %90, %175 ], [ %62, %176 ], [ %172, %177 ], [ %144, %178 ]
  %181 = add i32 %16, %5
  %182 = add i32 %18, %6
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %181, i32 %182, i32 undef, float %180, float %180, float %180, float %180, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %183

; <label>:183                                     ; preds = %179, %0
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

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

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
!6 = !{i32 0, %"class.Texture2DArray<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 7, i32 0, !7}
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

