; REQUIRES: asserts

; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -debug-only=loop-vectorize -vplan-explain -disable-output < %s 2>&1 | FileCheck %s --check-prefix=DBG
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S < %s | FileCheck %s --check-prefix=IR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-explain -S < %s | FileCheck %s --check-prefix=IR

target triple = "aarch64-unknown-linux-gnu"

; DBG:      LV: Loop[0] path=inner plans={{[0-9]+}}
; DBG-DAG:  LV:   VPlan[{{[0-9]+}}] VFs={2}
; DBG-DAG:  LV:   VPlan[{{[0-9]+}}] VFs={4}
; DBG:      LV:   selected VF=4 plan={{[0-9]+}}

define void @test_v2_v4(ptr noalias %a, ptr readonly %b) #0 {
; IR-LABEL: @test_v2_v4(
; IR:       vector.body:
; IR-NEXT:    [[INDEX:%.*]] = phi i64 [ 0, %vector.ph ], [ [[INDEX_NEXT:%.*]], %vector.body ]
; IR-NEXT:    [[TMP1:%.*]] = getelementptr i64, ptr [[B:%.*]], i64 [[INDEX]]
; IR-NEXT:    [[WIDE_LOAD:%.*]] = load <4 x i64>, ptr [[TMP1]], align 8
; IR-NEXT:    [[TMP3:%.*]] = call <4 x i64> @foo_vector_fixed4_nomask(<4 x i64> [[WIDE_LOAD]])
; IR-NEXT:    [[TMP4:%.*]] = getelementptr inbounds i64, ptr [[A:%.*]], i64 [[INDEX]]
; IR-NEXT:    store <4 x i64> [[TMP3]], ptr [[TMP4]], align 8
; IR-NEXT:    [[INDEX_NEXT]] = add nuw i64 [[INDEX]], 4
; IR-NEXT:    [[TMP6:%.*]] = icmp eq i64 [[INDEX_NEXT]], 1024
; IR-NEXT:    br i1 [[TMP6]], label %middle.block, label %vector.body, !llvm.loop [[LOOP4:![0-9]+]]
entry:
  br label %for.body

for.body:
  %indvars.iv = phi i64 [ 0, %entry ], [ %indvars.iv.next, %for.body ]
  %gep = getelementptr i64, ptr %b, i64 %indvars.iv
  %load = load i64, ptr %gep
  %call = call i64 @foo(i64 %load) #1
  %arrayidx = getelementptr inbounds i64, ptr %a, i64 %indvars.iv
  store i64 %call, ptr %arrayidx
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, 1024
  br i1 %exitcond, label %for.cond.cleanup, label %for.body

for.cond.cleanup:
  ret void
}

declare i64 @foo(i64)
declare <2 x i64> @foo_vector_fixed2_nomask(<2 x i64>)
declare <4 x i64> @foo_vector_fixed4_nomask(<4 x i64>)

attributes #0 = { "target-features"="+sve" vscale_range(2,16) "no-trapping-math"="false" }
attributes #1 = { nounwind "vector-function-abi-variant"="_ZGV_LLVM_N2v_foo(foo_vector_fixed2_nomask),_ZGV_LLVM_N4v_foo(foo_vector_fixed4_nomask)" }
