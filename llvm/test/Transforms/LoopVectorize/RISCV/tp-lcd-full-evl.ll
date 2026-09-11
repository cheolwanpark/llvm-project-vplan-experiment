; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd-full-evl=true -vplan-tp-lcd=rank -force-target-num-vector-regs=16 -o %t.full 2>%t.full.log
; RUN: FileCheck %s --check-prefix=FULL-IR < %t.full
; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd-full-evl=true -vplan-tp-lcd=rank -o %t.rank 2>%t.log
; RUN: FileCheck %s --check-prefix=PROOF < %t.log
; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd-full-evl=true -vplan-tp-lcd=observe -o %t.observe 2>/dev/null
; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd-full-evl=true -vplan-tp-lcd=off -o %t.off
; RUN: diff %t.off %t.observe
; RUN: opt %t.rank -passes=verify -disable-output
;
; Full width must hold at the largest legal vscale, not just tuning vscale2.
; At the architectural maximum (1024), 4096 covers VF4 but not VF8.
; A bounded vscale range admits VF16. Nonmultiples and unknown counts fail.
;
; PROOF: "full_width_iterations":true{{.*}}"function":"architectural_max"{{.*}}"vf":4
; PROOF: "full_width_iterations":false{{.*}}"function":"architectural_max"{{.*}}"vf":8
; PROOF: "full_width_iterations":true{{.*}}"function":"bounded_max"{{.*}}"vf":16
; PROOF: "full_width_iterations":false{{.*}}"function":"nonmultiple"
; PROOF: "full_width_iterations":false{{.*}}"function":"unknown_count"

target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "riscv64"

define void @architectural_max(ptr noalias %a, ptr noalias %b) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %sum = fadd float %v, 1.0
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

define void @bounded_max(ptr noalias %a, ptr noalias %b) vscale_range(2, 8) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %sum = fadd float %v, 1.0
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

define void @nonmultiple(ptr noalias %a, ptr noalias %b) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %sum = fadd float %v, 1.0
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4098
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

define void @unknown_count(ptr noalias %a, ptr noalias %b, i64 %n) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %sum = fadd float %v, 1.0
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}


; Limit register pressure so VF4 is actually selected from an EVL plan spanning
; larger candidates. Check vector code exists and its EVL/merge cleanup occurs.
; FULL-IR-LABEL: define float @cleanup
; FULL-IR-NOT: @llvm.experimental.get.vector.length
; FULL-IR-NOT: index.evl
; FULL-IR: call <vscale x 4 x float> @llvm.vp.load
; FULL-IR-NOT: @llvm.vp.merge
; FULL-IR-NOT: index.evl
; FULL-IR: ret float
define float @cleanup(ptr noalias %a, ptr noalias %b, ptr noalias %x) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %sum.a = phi float [ 0.0, %entry ], [ %add.a, %loop ]
  %sum.b = phi float [ 0.0, %entry ], [ %add.b, %loop ]
  %ap = getelementptr float, ptr %a, i64 %iv
  %av = load float, ptr %ap, align 4
  %bp = getelementptr float, ptr %b, i64 %iv
  %bv = load float, ptr %bp, align 4
  %xp = getelementptr float, ptr %x, i64 %iv
  %xv = load float, ptr %xp, align 4
  %mul.a = fmul fast float %av, %xv
  %mul.b = fmul fast float %bv, %xv
  %add.a = fadd fast float %sum.a, %mul.a
  %add.b = fadd fast float %sum.b, %mul.b
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  %result = fadd fast float %add.a, %add.b
  ret float %result
}
