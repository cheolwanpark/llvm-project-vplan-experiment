; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-candidate-id=lit-candidate %s
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.jsonl')]; print([(r['candidate_id'],r['function'],r['stage'],r['schema_version']) for r in rows])" \
; RUN:   | FileCheck %s --check-prefix=ROWS
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=obj %s -o %t.base.o
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=obj \
; RUN:   -riscv-vsched-feature-output=%t.object.jsonl %s -o %t.features.o
; RUN: cmp %t.base.o %t.features.o
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.verbose-a.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-emit-instructions -riscv-vsched-emit-edges \
; RUN:   -riscv-vsched-trace-picks %s
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.verbose-b.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-emit-instructions -riscv-vsched-emit-edges \
; RUN:   -riscv-vsched-trace-picks %s
; RUN: cmp %t.verbose-a.jsonl %t.verbose-b.jsonl
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.verbose-a.jsonl')]; pre=rows[0]; post=rows[1]; postra=rows[2]; final=rows[3]; print(pre['max_lmul'],pre['peak_live_vec_units'] > 0,pre['num_sched_nodes'] > 0,'window_ilp_8' in pre,post['scheduler_pick_count'] > 0,post['scheduler_edge_count'] > 0,'scheduler_picks' in post,'scheduler_edges' in post,'stack_size_bytes' in postra,final['num_sched_nodes'] > 0,final['pre_to_final_matched_instruction_count'] > 0)" \
; RUN:   | FileCheck %s --check-prefix=METRICS
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.nested.jsonl \
; RUN:   -riscv-vsched-only-func=nested_vector_loop %s
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.nested.jsonl')]; print(len(rows),sorted(set(r['loop_depth'] for r in rows)))" \
; RUN:   | FileCheck %s --check-prefix=NESTED
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.filtered.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-min-vector-inst=2 %s
; RUN: %python -c "print(len(open(r'%t.filtered.jsonl').read()))" \
; RUN:   | FileCheck %s --check-prefix=FILTERED
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -enable-misched=false \
; RUN:   -riscv-vsched-feature-output=%t.no-sched.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop %s
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.no-sched.jsonl')]; print(len(rows),rows[1]['scheduler_executed'],rows[1]['scheduler_pick_count'])" \
; RUN:   | FileCheck %s --check-prefix=NO-SCHED
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -misched-prera-direction=topdown \
; RUN:   -riscv-vsched-feature-output=%t.topdown.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop %s
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.topdown.jsonl')]; print(rows[1]['scheduler_direction'])" \
; RUN:   | FileCheck %s --check-prefix=TOPDOWN
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.fractional.jsonl \
; RUN:   -riscv-vsched-only-func=fractional_loop %s
; RUN: %python -c "import json; row=json.loads(open(r'%t.fractional.jsonl').readline()); print(row['max_lmul'],row['max_allocation_units'])" \
; RUN:   | FileCheck %s --check-prefix=FRACTIONAL
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.pipeline.jsonl \
; RUN:   -debug-pass=Structure %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=PIPELINE

; REQUIRES: asserts

; ROWS: [('lit-candidate', 'vector_loop', 'pre-sched', 1), ('lit-candidate', 'vector_loop', 'post-sched', 1), ('lit-candidate', 'vector_loop', 'post-rvv-ra', 1), ('lit-candidate', 'vector_loop', 'final-sched', 1)]
; METRICS: m2 True True True True True True True True True True
; NESTED: 4 [2]
; FILTERED: 0
; NO-SCHED: 4 False 0
; TOPDOWN: topdown
; FRACTIONAL: mf8 1

; PIPELINE: Rename Disconnected Subregister Components
; PIPELINE-NEXT: RISC-V Vector Scheduling Features (pre-schedule)
; PIPELINE: Machine Instruction Scheduler
; PIPELINE-NEXT: RISC-V Vector Scheduling Features (post-schedule)
; PIPELINE: RISC-V Insert VSETVLI pass
; PIPELINE-NEXT: RISC-V Vector Scheduling Features (post-RVV-RA)
; PIPELINE: PostRA Machine Instruction Scheduler
; PIPELINE-NEXT: RISC-V Vector Scheduling Features (final schedule)

define void @vector_loop(ptr %out, ptr %in) {
entry:
  br label %loop

loop:
  %value = load <vscale x 4 x i32>, ptr %in, align 4
  %sum = add <vscale x 4 x i32> %value, splat (i32 1)
  store <vscale x 4 x i32> %sum, ptr %out, align 4
  br label %loop
}

define void @nested_vector_loop(ptr %out, ptr %in, i64 %n, i64 %m) {
entry:
  br label %outer

outer:
  %i = phi i64 [ 0, %entry ], [ %i.next, %outer.latch ]
  br label %inner

inner:
  %j = phi i64 [ 0, %outer ], [ %j.next, %inner ]
  %value = load <vscale x 4 x i32>, ptr %in, align 4
  %sum = add <vscale x 4 x i32> %value, splat (i32 1)
  store <vscale x 4 x i32> %sum, ptr %out, align 4
  %j.next = add nuw i64 %j, 1
  %inner.done = icmp eq i64 %j.next, %m
  br i1 %inner.done, label %outer.latch, label %inner

outer.latch:
  %i.next = add nuw i64 %i, 1
  %outer.done = icmp eq i64 %i.next, %n
  br i1 %outer.done, label %exit, label %outer

exit:
  ret void
}

define void @fractional_loop(ptr %out, ptr %in) {
entry:
  br label %loop

loop:
  %value = load <vscale x 1 x i8>, ptr %in, align 1
  %sum = add <vscale x 1 x i8> %value, splat (i8 1)
  store <vscale x 1 x i8> %sum, ptr %out, align 1
  br label %loop
}
