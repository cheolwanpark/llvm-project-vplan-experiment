; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-list -use-vf=f2 -disable-output < %s 2>&1 | FileCheck %s --check-prefix=FORCED
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -use-vf=f2 < %s | FileCheck %s --check-prefix=IR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -use-vf=f3 < %s | FileCheck %s --check-prefix=SCALAR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -use-vf=fixed:2 < %s | FileCheck %s --check-prefix=SCALAR

target triple = "aarch64-unknown-linux-gnu"

; FORCED: vplan-list schema=1 record="plan" function="test_v2_v4" loop=0 path="inner" plan=0 vf="f2" selected=true status="evaluated" compare_kind="trip_count" compare_num={{[0-9]+}} compare_den=1 trip_count=1024

define void @test_v2_v4(ptr noalias %a, ptr readonly %b) #0 {
; IR-LABEL: @test_v2_v4(
; IR:       vector.body:
; IR:       %wide.load = load <2 x i64>
; IR:       call <2 x i64> @foo_vector_fixed2_nomask(
; IR-NOT:   call <4 x i64> @foo_vector_fixed4_nomask(
; SCALAR-NOT: vector.body:
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
