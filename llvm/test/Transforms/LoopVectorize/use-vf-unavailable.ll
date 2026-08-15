; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -use-vf=s4 < %s | FileCheck %s --check-prefix=SCALAR

target triple = "x86_64-unknown-linux-gnu"

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
