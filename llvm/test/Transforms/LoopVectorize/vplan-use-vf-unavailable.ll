; REQUIRES: asserts

; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -debug-only=loop-vectorize-vplan-use-vf -vplan-use-vf=scalable:4 -disable-output < %s 2>&1 | FileCheck %s --check-prefix=UNAVAILABLE
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -vplan-use-vf=scalable:4 < %s | FileCheck %s --check-prefix=SCALAR

target triple = "x86_64-unknown-linux-gnu"

; UNAVAILABLE: LV: Loop[0] forcing VF vscale x 4
; UNAVAILABLE: LV: Not vectorizing: requested -vplan-use-vf is not available 'scalable:4' for loop index 0.

define void @test_unavailable(ptr noalias %a, ptr readonly %b) {
; SCALAR-LABEL: @test_unavailable(
; SCALAR-NOT: vector.body:
entry:
  br label %for.body

for.body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %for.body ]
  %src = getelementptr i64, ptr %b, i64 %iv
  %load = load i64, ptr %src, align 8
  %add = add nsw i64 %load, 1
  %dst = getelementptr inbounds i64, ptr %a, i64 %iv
  store i64 %add, ptr %dst, align 8
  %iv.next = add nuw nsw i64 %iv, 1
  %exitcond = icmp eq i64 %iv.next, 1024
  br i1 %exitcond, label %exit, label %for.body

exit:
  ret void
}
