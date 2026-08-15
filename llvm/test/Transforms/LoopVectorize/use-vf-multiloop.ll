; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-list -use-vf=f2,- -disable-output < %s 2>&1 | FileCheck %s --check-prefix=LIST
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S -use-vf=f2,- < %s | FileCheck %s --check-prefix=IR

target triple = "aarch64-unknown-linux-gnu"

; LIST: vplan-list schema=1 record="plan" function="test_multiloop" loop=0 path="inner" plan=0 vf="f2" selected=true
; LIST: vplan-list schema=1 record="plan" function="test_multiloop" loop=1 path="inner" plan={{[0-9]+}} vf="f4" selected=true

define void @test_multiloop(ptr noalias %a, ptr noalias %b, ptr readonly %c,
                            ptr readonly %d) #0 {
; IR-LABEL: @test_multiloop(
; IR:       %wide.load = load <2 x i64>
; IR:       call <2 x i64> @foo_vector_fixed2_nomask(
; IR:       %wide.load4 = load <4 x i64>
; IR:       call <4 x i64> @bar_vector_fixed4_nomask(
entry:
  br label %loop0.body

loop0.body:
  %iv0 = phi i64 [ 0, %entry ], [ %iv0.next, %loop0.body ]
  %src0 = getelementptr i64, ptr %c, i64 %iv0
  %load0 = load i64, ptr %src0, align 8
  %call0 = call i64 @foo(i64 %load0) #1
  %dst0 = getelementptr inbounds i64, ptr %a, i64 %iv0
  store i64 %call0, ptr %dst0, align 8
  %iv0.next = add nuw nsw i64 %iv0, 1
  %exit0 = icmp eq i64 %iv0.next, 1024
  br i1 %exit0, label %loop1.preheader, label %loop0.body

loop1.preheader:
  br label %loop1.body

loop1.body:
  %iv1 = phi i64 [ 0, %loop1.preheader ], [ %iv1.next, %loop1.body ]
  %src1 = getelementptr i64, ptr %d, i64 %iv1
  %load1 = load i64, ptr %src1, align 8
  %call1 = call i64 @bar(i64 %load1) #2
  %dst1 = getelementptr inbounds i64, ptr %b, i64 %iv1
  store i64 %call1, ptr %dst1, align 8
  %iv1.next = add nuw nsw i64 %iv1, 1
  %exit1 = icmp eq i64 %iv1.next, 1024
  br i1 %exit1, label %exit, label %loop1.body

exit:
  ret void
}

declare i64 @foo(i64)
declare <2 x i64> @foo_vector_fixed2_nomask(<2 x i64>)
declare <4 x i64> @foo_vector_fixed4_nomask(<4 x i64>)
declare i64 @bar(i64)
declare <2 x i64> @bar_vector_fixed2_nomask(<2 x i64>)
declare <4 x i64> @bar_vector_fixed4_nomask(<4 x i64>)

attributes #0 = { "target-features"="+sve" vscale_range(2,16) "no-trapping-math"="false" }
attributes #1 = { nounwind "vector-function-abi-variant"="_ZGV_LLVM_N2v_foo(foo_vector_fixed2_nomask),_ZGV_LLVM_N4v_foo(foo_vector_fixed4_nomask)" }
attributes #2 = { nounwind "vector-function-abi-variant"="_ZGV_LLVM_N2v_bar(bar_vector_fixed2_nomask),_ZGV_LLVM_N4v_bar(bar_vector_fixed4_nomask)" }
