; RUN: opt %s -disable-output -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=2 -force-vector-width=2 -scalable-vectorization=on -vplan-tp-lcd=rank 2>&1 | FileCheck %s --check-prefix=IC2
; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -force-vector-width=2 -scalable-vectorization=on -vplan-tp-lcd=off -o %t.off
; RUN: opt %s -S -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -force-vector-width=2 -scalable-vectorization=on -vplan-tp-lcd=observe -o %t.observe 2>%t.log
; RUN: diff %t.off %t.observe
; RUN: FileCheck %s --check-prefix=OBSERVE < %t.log
; RUN: opt %s -disable-output -passes=loop-vectorize -mtriple=riscv64 -mattr=+v,+zvl128b -force-vector-interleave=1 -force-vector-width=2 -scalable-vectorization=on -vplan-tp-lcd=rank -riscv-tp-lcd-profile=xiangshan 2>&1 | FileCheck %s --check-prefix=RANK
;
; IC2: VPLAN-TP-LCD {{.*}}"fallback_reason":"experimental ranking requires explicit IC=1"{{.*}}"function":"stream"
;
; Diagnostics must also cover forced VFs, whose ordinary selection bypasses
; computeBestVF's candidate loop. Observation leaves emitted IR unchanged.
; Ordinary legality allows this distant memory recurrence, but the SSA-only
; latency graph cannot model its memory edge and must explicitly fall back.
;
; OBSERVE: VPLAN-TP-LCD {{.*}}"fallback_reason":""{{.*}}"function":"stream"{{.*}}"runtime_vf":4{{.*}}"vf":2
; OBSERVE: VPLAN-TP-LCD {{.*}}"fallback_reason":"memory-carried or unresolved dependence"{{.*}}"function":"memory_recurrence"
; RANK: VPLAN-TP-LCD {{.*}}"fallback_reason":""{{.*}}"function":"stream"{{.*}}"lcd":1
; RANK: VPLAN-TP-LCD {{.*}}"fallback_reason":"memory-carried or unresolved dependence"{{.*}}"function":"memory_recurrence"
; RANK: VPLAN-TP-LCD {{.*}}"fma_pairs":1{{.*}}"function":"contractable"
; RANK: VPLAN-TP-LCD {{.*}}"fma_pairs":0{{.*}}"function":"no_contract"
; RANK: VPLAN-TP-LCD {{.*}}"fma_pairs":0{{.*}}"function":"shared_multiply"
; RANK: VPLAN-TP-LCD {{.*}}"fallback_reason":"size optimization retains ordinary costs"{{.*}}"function":"size_only"

target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "riscv64"

define void @stream(ptr noalias %a, ptr noalias %b) {
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

define void @memory_recurrence(ptr %a) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 32, %entry ], [ %next, %loop ]
  %previous = sub i64 %iv, 32
  %p = getelementptr float, ptr %a, i64 %previous
  %v = load float, ptr %p, align 4
  %sum = fadd float %v, 1.0
  %q = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %q, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Contraction needs both flags and exactly one use of the multiply.
define void @contractable(ptr noalias %a, ptr noalias %b, ptr noalias %c, float %alpha, float %beta) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %mul = fmul contract float %v, %alpha
  %sum = fadd contract float %mul, %beta
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Contraction needs both flags and exactly one use of the multiply.
define void @no_contract(ptr noalias %a, ptr noalias %b, ptr noalias %c, float %alpha, float %beta) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %mul = fmul float %v, %alpha
  %sum = fadd float %mul, %beta
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Contraction needs both flags and exactly one use of the multiply.
define void @shared_multiply(ptr noalias %a, ptr noalias %b, ptr noalias %c, float %alpha, float %beta) {
entry:
  br label %loop
loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr float, ptr %b, i64 %iv
  %v = load float, ptr %bp, align 4
  %mul = fmul contract float %v, %alpha
  %sum = fadd contract float %mul, %beta
  %ap = getelementptr float, ptr %a, i64 %iv
  store float %sum, ptr %ap, align 4
  %cp = getelementptr float, ptr %c, i64 %iv
  store float %mul, ptr %cp, align 4
  %next = add nuw i64 %iv, 1
  %done = icmp eq i64 %next, 4096
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Code-size profitability and ranking keep ordinary costs.
define void @size_only(ptr noalias %a, ptr noalias %b) minsize {
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
