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
; shader debug name: 36608e0b5f698841356cf8f3e85bcdd7.pdb
; shader hash: 36608e0b5f698841356cf8f3e85bcdd7
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
;   
;   } $Globals;                                       ; Offset:    0 Size:   120
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
%dx.types.Dimensions = type { i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%"$Globals" = type { [4 x i32], [4 x i32], i32 }
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
  br i1 %25, label %26, label %196

; <label>:26                                      ; preds = %0
  %27 = urem i32 %12, %20
  %28 = urem i32 %14, %22
  %29 = or i32 %28, %27
  %30 = icmp eq i32 %29, 0
  %31 = extractvalue %dx.types.CBufRet.i32 %21, 1
  %32 = icmp eq i32 %31, 0
  br i1 %30, label %33, label %89

; <label>:33                                      ; preds = %26
  %34 = udiv i32 %12, %20
  %35 = udiv i32 %14, %22
  %36 = icmp eq i32 %35, 0
  br i1 %32, label %63, label %37

; <label>:37                                      ; preds = %33
  br i1 %36, label %192, label %38

; <label>:38                                      ; preds = %37
  br label %39

; <label>:39                                      ; preds = %59, %38
  %40 = phi float [ %60, %59 ], [ 2.000000e+00, %38 ]
  %41 = phi i32 [ %61, %59 ], [ 0, %38 ]
  %42 = icmp eq i32 %34, 0
  br i1 %42, label %59, label %43

; <label>:43                                      ; preds = %39
  br label %44

; <label>:44                                      ; preds = %44, %43
  %45 = phi float [ %55, %44 ], [ %40, %43 ]
  %46 = phi i32 [ %56, %44 ], [ 0, %43 ]
  %47 = mul i32 %34, %5
  %48 = add i32 %47, %8
  %49 = add i32 %48, %46
  %50 = mul i32 %35, %6
  %51 = add i32 %50, %10
  %52 = add i32 %51, %41
  %53 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %49, i32 %52, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %54 = extractvalue %dx.types.ResRet.f32 %53, 0
  %55 = call float @dx.op.binary.f32(i32 36, float %54, float %45)  ; FMin(a,b)
  %56 = add nuw i32 %46, 1
  %57 = icmp ult i32 %56, %34
  br i1 %57, label %44, label %58

; <label>:58                                      ; preds = %44
  br label %59

; <label>:59                                      ; preds = %58, %39
  %60 = phi float [ %40, %39 ], [ %55, %58 ]
  %61 = add nuw i32 %41, 1
  %62 = icmp ult i32 %61, %35
  br i1 %62, label %39, label %189

; <label>:63                                      ; preds = %33
  br i1 %36, label %192, label %64

; <label>:64                                      ; preds = %63
  br label %65

; <label>:65                                      ; preds = %85, %64
  %66 = phi float [ %86, %85 ], [ -2.000000e+00, %64 ]
  %67 = phi i32 [ %87, %85 ], [ 0, %64 ]
  %68 = icmp eq i32 %34, 0
  br i1 %68, label %85, label %69

; <label>:69                                      ; preds = %65
  br label %70

; <label>:70                                      ; preds = %70, %69
  %71 = phi float [ %81, %70 ], [ %66, %69 ]
  %72 = phi i32 [ %82, %70 ], [ 0, %69 ]
  %73 = mul i32 %34, %5
  %74 = add i32 %73, %8
  %75 = add i32 %74, %72
  %76 = mul i32 %35, %6
  %77 = add i32 %76, %10
  %78 = add i32 %77, %67
  %79 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2, i32 0, i32 %75, i32 %78, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %80 = extractvalue %dx.types.ResRet.f32 %79, 0
  %81 = call float @dx.op.binary.f32(i32 35, float %80, float %71)  ; FMax(a,b)
  %82 = add nuw i32 %72, 1
  %83 = icmp ult i32 %82, %34
  br i1 %83, label %70, label %84

; <label>:84                                      ; preds = %70
  br label %85

; <label>:85                                      ; preds = %84, %65
  %86 = phi float [ %66, %65 ], [ %81, %84 ]
  %87 = add nuw i32 %67, 1
  %88 = icmp ult i32 %87, %35
  br i1 %88, label %65, label %188

; <label>:89                                      ; preds = %26
  %90 = call %dx.types.Dimensions @dx.op.getDimensions(i32 72, %dx.types.Handle %2, i32 0)  ; GetDimensions(handle,mipLevel)
  %91 = extractvalue %dx.types.Dimensions %90, 0
  %92 = sitofp i32 %91 to float
  %93 = extractvalue %dx.types.Dimensions %90, 1
  %94 = sitofp i32 %93 to float
  %95 = uitofp i32 %12 to float
  %96 = uitofp i32 %14 to float
  %97 = uitofp i32 %8 to float
  %98 = uitofp i32 %10 to float
  %99 = fdiv fast float %97, %92
  %100 = fdiv fast float %98, %94
  %101 = uitofp i32 %20 to float
  %102 = uitofp i32 %22 to float
  %103 = uitofp i32 %5 to float
  %104 = uitofp i32 %6 to float
  %105 = fadd fast float %103, 5.000000e-01
  %106 = fadd fast float %104, 5.000000e-01
  %107 = fdiv fast float %105, %101
  %108 = fdiv fast float %106, %102
  %109 = fadd fast float %95, -5.000000e-01
  %110 = fadd fast float %96, -5.000000e-01
  %111 = fdiv fast float %109, %92
  %112 = fdiv fast float %110, %94
  %113 = fdiv fast float %95, %92
  %114 = fdiv fast float %96, %94
  %115 = fmul fast float %113, %107
  %116 = fmul fast float %114, %108
  %117 = fdiv fast float %95, %101
  %118 = fdiv fast float %96, %102
  %119 = call float @dx.op.unary.f32(i32 28, float %117)  ; Round_pi(value)
  %120 = call float @dx.op.unary.f32(i32 28, float %118)  ; Round_pi(value)
  %121 = fmul fast float %119, %101
  %122 = fmul fast float %120, %102
  %123 = fdiv fast float 1.000000e+00, %121
  %124 = fdiv fast float 1.000000e+00, %122
  %125 = fptoui float %119 to i32
  %126 = fptoui float %120 to i32
  %127 = icmp eq i32 %126, 0
  br i1 %32, label %158, label %128

; <label>:128                                     ; preds = %89
  br i1 %127, label %192, label %129

; <label>:129                                     ; preds = %128
  br label %130

; <label>:130                                     ; preds = %154, %129
  %131 = phi float [ %155, %154 ], [ 2.000000e+00, %129 ]
  %132 = phi i32 [ %156, %154 ], [ 0, %129 ]
  %133 = icmp eq i32 %125, 0
  br i1 %133, label %154, label %134

; <label>:134                                     ; preds = %130
  br label %135

; <label>:135                                     ; preds = %135, %134
  %136 = phi float [ %150, %135 ], [ %131, %134 ]
  %137 = phi i32 [ %151, %135 ], [ 0, %134 ]
  %138 = fadd fast float %115, %99
  %139 = uitofp i32 %137 to float
  %140 = fmul fast float %139, %123
  %141 = fadd fast float %138, %140
  %142 = fadd fast float %116, %100
  %143 = uitofp i32 %132 to float
  %144 = fmul fast float %143, %124
  %145 = fadd fast float %142, %144
  %146 = call float @dx.op.binary.f32(i32 36, float %141, float %111)  ; FMin(a,b)
  %147 = call float @dx.op.binary.f32(i32 36, float %145, float %112)  ; FMin(a,b)
  %148 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %146, float %147, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %149 = extractvalue %dx.types.ResRet.f32 %148, 0
  %150 = call float @dx.op.binary.f32(i32 36, float %149, float %136)  ; FMin(a,b)
  %151 = add nuw i32 %137, 1
  %152 = icmp eq i32 %151, %125
  br i1 %152, label %153, label %135

; <label>:153                                     ; preds = %135
  br label %154

; <label>:154                                     ; preds = %153, %130
  %155 = phi float [ %131, %130 ], [ %150, %153 ]
  %156 = add nuw i32 %132, 1
  %157 = icmp eq i32 %156, %126
  br i1 %157, label %191, label %130

; <label>:158                                     ; preds = %89
  br i1 %127, label %192, label %159

; <label>:159                                     ; preds = %158
  br label %160

; <label>:160                                     ; preds = %184, %159
  %161 = phi float [ %185, %184 ], [ -2.000000e+00, %159 ]
  %162 = phi i32 [ %186, %184 ], [ 0, %159 ]
  %163 = icmp eq i32 %125, 0
  br i1 %163, label %184, label %164

; <label>:164                                     ; preds = %160
  br label %165

; <label>:165                                     ; preds = %165, %164
  %166 = phi float [ %180, %165 ], [ %161, %164 ]
  %167 = phi i32 [ %181, %165 ], [ 0, %164 ]
  %168 = fadd fast float %115, %99
  %169 = uitofp i32 %167 to float
  %170 = fmul fast float %169, %123
  %171 = fadd fast float %168, %170
  %172 = fadd fast float %116, %100
  %173 = uitofp i32 %162 to float
  %174 = fmul fast float %173, %124
  %175 = fadd fast float %172, %174
  %176 = call float @dx.op.binary.f32(i32 36, float %171, float %111)  ; FMin(a,b)
  %177 = call float @dx.op.binary.f32(i32 36, float %175, float %112)  ; FMin(a,b)
  %178 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %2, %dx.types.Handle %3, float %176, float %177, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %179 = extractvalue %dx.types.ResRet.f32 %178, 0
  %180 = call float @dx.op.binary.f32(i32 35, float %179, float %166)  ; FMax(a,b)
  %181 = add nuw i32 %167, 1
  %182 = icmp eq i32 %181, %125
  br i1 %182, label %183, label %165

; <label>:183                                     ; preds = %165
  br label %184

; <label>:184                                     ; preds = %183, %160
  %185 = phi float [ %161, %160 ], [ %180, %183 ]
  %186 = add nuw i32 %162, 1
  %187 = icmp eq i32 %186, %126
  br i1 %187, label %190, label %160

; <label>:188                                     ; preds = %85
  br label %192

; <label>:189                                     ; preds = %59
  br label %192

; <label>:190                                     ; preds = %184
  br label %192

; <label>:191                                     ; preds = %154
  br label %192

; <label>:192                                     ; preds = %191, %190, %189, %188, %158, %128, %63, %37
  %193 = phi float [ 2.000000e+00, %37 ], [ -2.000000e+00, %63 ], [ 2.000000e+00, %128 ], [ -2.000000e+00, %158 ], [ %86, %188 ], [ %60, %189 ], [ %185, %190 ], [ %155, %191 ]
  %194 = add i32 %16, %5
  %195 = add i32 %18, %6
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %1, i32 %194, i32 %195, i32 undef, float %193, float %193, float %193, float %193, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %196

; <label>:196                                     ; preds = %192, %0
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
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #1

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
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!10 = !{!11}
!11 = !{i32 0, %"$Globals"* undef, !"", i32 0, i32 0, i32 1, i32 120, null}
!12 = !{!13}
!13 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 8, i32 1, i32 0, null}
!14 = !{void ()* @main, !"main", null, !4, !15}
!15 = !{i32 4, !16}
!16 = !{i32 8, i32 8, i32 1}

