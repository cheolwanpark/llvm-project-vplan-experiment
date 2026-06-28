; RUN: rm -f %t.mmd %t-all.mmd %t-all.0.mmd %t-all.1.mmd %t.s
; RUN: opt < %s -passes=loop-vectorize -force-target-supports-scalable-vectors=true -scalable-vectorization=on -S | llc -mtriple=riscv64 -mattr=+v --dag-analysis=%t.mmd -o %t.s -
; RUN: FileCheck %s --input-file=%t.mmd --check-prefix=REPORT
; RUN: opt < %s -passes=loop-vectorize -force-target-supports-scalable-vectors=true -scalable-vectorization=on -S | llc -mtriple=riscv64 -mattr=+v --dag-analysis=%t-all.mmd --dag-analysis-all-blocks -o %t.s -
; RUN: test -s %t-all.0.mmd
; RUN: test -s %t-all.1.mmd

; REPORT: %% SelectionDAG graph: reduction_add_trunc::vector.body
; REPORT: %% machine block: bb.
; REPORT: %% vector loop header: vector.body
; REPORT: flowchart TD
; REPORT: PseudoVWADDU_WV_M2_TIED
; REPORT: PseudoVLE8_V_M1
; REPORT: machine_mmo0: load size={{[0-9]+}} scalable align=4 as=0 value=
; REPORT: ch chain

target datalayout = "e-p:64:64:64-i1:8:8-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-v64:64-v128:128-a0:0:64-n8:16:32:64-S128"

define i8 @reduction_add_trunc(ptr noalias nocapture %A) {
entry:
  br label %loop

loop:
  %iv = phi i32 [ %iv.next, %loop ], [ 0, %entry ]
  %sum.prev = phi i32 [ %sum.next, %loop ], [ 255, %entry ]
  %sum.masked = and i32 %sum.prev, 255
  %ptr = getelementptr inbounds i8, ptr %A, i32 %iv
  %load = load i8, ptr %ptr, align 4
  %load.ext = zext i8 %load to i32
  %sum.next = add i32 %sum.masked, %load.ext
  %iv.next = add i32 %iv, 1
  %exitcond = icmp eq i32 %iv.next, 256
  br i1 %exitcond, label %exit, label %loop, !llvm.loop !0

exit:
  %sum.lcssa = phi i32 [ %sum.next, %loop ]
  %ret = trunc i32 %sum.lcssa to i8
  ret i8 %ret
}

!0 = distinct !{!0, !1, !2, !3, !4}
!1 = !{!"llvm.loop.vectorize.width", i32 8}
!2 = !{!"llvm.loop.vectorize.scalable.enable", i1 true}
!3 = !{!"llvm.loop.interleave.count", i32 2}
!4 = !{!"llvm.loop.vectorize.enable", i1 true}
