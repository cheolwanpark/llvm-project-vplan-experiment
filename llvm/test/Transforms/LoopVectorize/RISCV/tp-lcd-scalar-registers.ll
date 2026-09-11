; RUN: opt %s -disable-output -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd=rank -vplan-tp-lcd-strided-addresses=true 2>&1 | FileCheck %s --check-prefix=ON
; RUN: opt %s -disable-output -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -scalable-vectorization=on -riscv-v-register-bit-width-lmul=8 -riscv-tp-lcd-profile=xiangshan -vplan-tp-lcd=rank -vplan-tp-lcd-strided-addresses=false 2>&1 | FileCheck %s --check-prefix=OFF
;
; The affine, address-only chain becomes a scalar strided address. Scalar EVL
; must remain a GPR even when a widened induction consumes it. Four genuine
; float groups fit in 32 registers at VF16; the ordinary estimate counts 56.
; An extra vector use of the integer index invalidates that scalar proof.
;
; ON: "eligible":true{{.*}}"function":"address_only"{{.*}}"ordinary_pressure_excluded":true{{.*}}"scalar_address_register_recipes":3{{.*}}"vf":16
; OFF: "eligible":false{{.*}}"exclusion_reason":"register pressure"{{.*}}"function":"address_only"{{.*}}"vf":16
; ON: "function":"vector_index_use"{{.*}}"scalar_address_register_recipes":1{{.*}}"vf":8
; ON-NOT: "function":"vector_index_use"{{.*}}"vf":16

target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "riscv64"

define void @address_only(ptr noalias %a, ptr noalias %b, ptr noalias %c, ptr noalias %d, ptr noalias %out) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %cp = getelementptr float, ptr %c, i64 %iv
  %cv = load float, ptr %cp, align 4
  %bp = getelementptr float, ptr %b, i64 %iv
  %bv = load float, ptr %bp, align 4
  %dp = getelementptr float, ptr %d, i64 %iv
  %dv = load float, ptr %dp, align 4
  %twice = fmul fast float %bv, 2.0
  %factor = fmul fast float %twice, %dv
  %sum1 = fadd fast float %bv, %cv
  %sum2 = fadd fast float %sum1, %dv
  %product = fmul fast float %sum2, %cv
  %value = fadd fast float %product, %factor
  %offset = shl nuw nsw i64 %iv, 3
  %ap = getelementptr float, ptr %a, i64 %offset
  store float %value, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

define void @vector_index_use(ptr noalias %a, ptr noalias %b, ptr noalias %c, ptr noalias %d, ptr noalias %out) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %cp = getelementptr float, ptr %c, i64 %iv
  %cv = load float, ptr %cp, align 4
  %bp = getelementptr float, ptr %b, i64 %iv
  %bv = load float, ptr %bp, align 4
  %dp = getelementptr float, ptr %d, i64 %iv
  %dv = load float, ptr %dp, align 4
  %twice = fmul fast float %bv, 2.0
  %factor = fmul fast float %twice, %dv
  %sum1 = fadd fast float %bv, %cv
  %sum2 = fadd fast float %sum1, %dv
  %product = fmul fast float %sum2, %cv
  %value = fadd fast float %product, %factor
  %offset = shl nuw nsw i64 %iv, 3
  %ap = getelementptr float, ptr %a, i64 %offset
  store float %value, ptr %ap, align 4
  %op = getelementptr i64, ptr %out, i64 %iv
  store i64 %offset, ptr %op, align 8
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}
