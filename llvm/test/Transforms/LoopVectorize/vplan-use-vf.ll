; REQUIRES: asserts

; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -debug-only=loop-vectorize-vplan-use-vf -vplan-explain -vplan-use-vf=fixed:2 -disable-output < %s 2>&1 | FileCheck %s --check-prefix=FORCED
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -vplan-use-vf=fixed:2 < %s | FileCheck %s --check-prefix=IR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -debug-only=loop-vectorize-vplan-use-vf -vplan-use-vf=fixed:3 -disable-output < %s 2>&1 | FileCheck %s --check-prefix=INVALID

target triple = "aarch64-unknown-linux-gnu"

; FORCED:      LV: Loop[0] forcing VF 2
; FORCED:      LV: Loop[0] path=inner plans=1
; FORCED-NEXT: LV:   VPlan[0] VFs={2}
; FORCED-NEXT: LV:   selected VF=2 plan=0
; FORCED:      LV: Loop[0] bypassing interleave selection for forced VF
; FORCED:      LV: Loop[0] bypassing outside-loop work profitability for forced VF
; FORCED:      LV: Loop[0] disabling epilogue vectorization for forced VF

; INVALID: LV: Not vectorizing: invalid -vplan-use-vf entry 'fixed:3' for loop index 0.

define void @test_v2_v4(ptr noalias %a, ptr readonly %b) #0 {
; IR-LABEL: @test_v2_v4(
; IR:       vector.body:
; IR:       %wide.load = load <2 x i64>
; IR:       call <2 x i64> @foo_vector_fixed2_nomask(
; IR-NOT:   call <4 x i64> @foo_vector_fixed4_nomask(
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
