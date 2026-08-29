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
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.verbose-a.jsonl')]; pre,post,postra,final=rows; old=['lookahead_to_ready_1_p50','window_ready_vec_units_8','window_independent_chain_count_8','window_ilp_8','resident_chain_upper_bound','resident_chain_upper_bound_after_nonrec_live','stage_order_hash','pre_to_post_rvv_ra_matched_instruction_count','pre_to_final_matched_instruction_count','post_to_final_matched_instruction_count']; overlap=['min_free_vec_units_while_recurrence_live','p10_free_vec_units_while_recurrence_live','peak_total_units_while_recurrence_live','resident_chain_bound_under_overlap_arch','resident_chain_bound_under_overlap_allocator']; inst=pre['instructions'][0]; print(pre['max_lmul'],pre['peak_live_vec_units'] > 0,pre['num_sched_nodes'] > 0,'window_weighted_work_ilp_8' in pre,not any(k in r for r in rows for k in old),inst['weighted_work_units'] > 0,inst['distinct_macro_ops']==1,'weight' not in inst,'stable_mi_id' in inst,post['stable_mi_unmatched_count']==0,post['pre_to_post_matched_instruction_count'] > 0,post['scheduler_pick_count'] > 0,post['scheduler_edge_count'] > 0,'stack_size_bytes' in postra,final['num_sched_nodes'] > 0,all('stage_instruction_sequence_hash' in r for r in rows),all(pre[k] is None for k in overlap),pre['allocator_vector_register_count']==32)" \
; RUN:   | FileCheck %s --check-prefix=METRICS
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v \
; RUN:   -stop-before=rename-independent-subregs %s -o %t.pre.mir
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -start-before=rename-independent-subregs \
; RUN:   -riscv-vsched-candidate-id=replay \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-feature-output=%t.replay-a.jsonl %t.pre.mir
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -start-before=rename-independent-subregs \
; RUN:   -riscv-vsched-candidate-id=replay \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-feature-output=%t.replay-b.jsonl %t.pre.mir
; RUN: cmp %t.replay-a.jsonl %t.replay-b.jsonl
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.replay-a.jsonl')]; pre,post=rows[:2]; print(pre['stable_mi_unmatched_count'],post['stable_mi_unmatched_count'],post['pre_to_post_matched_instruction_count']==post['pre_to_post_baseline_instruction_count'])" \
; RUN:   | FileCheck %s --check-prefix=REPLAY
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.nested.jsonl \
; RUN:   -riscv-vsched-only-func=nested_vector_loop %s
; RUN: %python -c "import json; rows=[json.loads(x) for x in open(r'%t.nested.jsonl')]; print(len(rows),sorted(set(r['loop_depth'] for r in rows)))" \
; RUN:   | FileCheck %s --check-prefix=NESTED
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.filtered.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop \
; RUN:   -riscv-vsched-min-vector-inst=4 %s
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
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -misched-prera-direction=bottomup \
; RUN:   -riscv-vsched-feature-output=%t.bottomup-a.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop %s
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -misched-prera-direction=bottomup \
; RUN:   -riscv-vsched-feature-output=%t.bottomup-b.jsonl \
; RUN:   -riscv-vsched-only-func=vector_loop %s
; RUN: cmp %t.bottomup-a.jsonl %t.bottomup-b.jsonl
; RUN: %python -c "import json; default=[json.loads(x) for x in open(r'%t.verbose-a.jsonl')]; top=[json.loads(x) for x in open(r'%t.topdown.jsonl')]; bottom=[json.loads(x) for x in open(r'%t.bottomup-a.jsonl')]; print(top[1]['scheduler_direction'],bottom[1]['scheduler_direction'],default[0]['stable_mi_ids']==top[0]['stable_mi_ids']==bottom[0]['stable_mi_ids'],top[1]['stable_mi_unmatched_count'],bottom[1]['stable_mi_unmatched_count'])" \
; RUN:   | FileCheck %s --check-prefix=TOPDOWN
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.fractional.jsonl \
; RUN:   -riscv-vsched-only-func=fractional_loop %s
; RUN: %python -c "import json; row=json.loads(open(r'%t.fractional.jsonl').readline()); print(row['max_lmul'],row['max_allocation_units'])" \
; RUN:   | FileCheck %s --check-prefix=FRACTIONAL
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.m8-work.jsonl \
; RUN:   -riscv-vsched-only-func=m8_independent \
; RUN:   -riscv-vsched-emit-instructions %s
; RUN: %python -c "import json; r=json.loads(open(r'%t.m8-work.jsonl').readline()); m=max(r['instructions'],key=lambda i:i['weighted_work_units']); print(m['weighted_work_units'],m['distinct_macro_ops'],r['window_ready_weighted_work_units_8'] > r['window_ready_macroop_count_8'],r['window_weighted_work_ilp_8'] > r['window_macro_ilp_8'],r['window_distinct_dependency_components_8'] <= r['vector_instruction_count'])" \
; RUN:   | FileCheck %s --check-prefix=M8-WORK
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.m8-overlap.jsonl \
; RUN:   -riscv-vsched-only-func=m8_overlap %s
; RUN: %python -c "import json; r=json.loads(open(r'%t.m8-overlap.jsonl').readline()); print(r['loop_carried_live_units'],r['num_recurrences'],r['resident_chain_upper_bound_arch'],r['resident_chain_upper_bound_allocator'],r['peak_total_units_while_recurrence_live'] > 32,r['min_free_vec_units_while_recurrence_live'],r['resident_chain_bound_under_overlap_arch'],r['resident_chain_bound_under_overlap_allocator'])" \
; RUN:   | FileCheck %s --check-prefix=M8-OVERLAP
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.m8-nov0.jsonl \
; RUN:   -riscv-vsched-only-func=m8_nov0_recurrence %s
; RUN: %python -c "import json; r=json.loads(open(r'%t.m8-nov0.jsonl').readline()); print(r['vector_register_class_histogram'].get('VRM8NoV0',0) > 0,r['resident_chain_upper_bound_arch'],r['resident_chain_upper_bound_allocator'])" \
; RUN:   | FileCheck %s --check-prefix=M8-NOV0
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.n2m4.jsonl \
; RUN:   -riscv-vsched-only-func=n2m4_recurrence %s
; RUN: %python -c "import json; r=json.loads(open(r'%t.n2m4.jsonl').readline()); print(r['vector_register_class_histogram'].get('VRN2M4',0) > 0,r['max_allocation_units'],r['resident_chain_upper_bound_arch'],r['resident_chain_upper_bound_allocator'])" \
; RUN:   | FileCheck %s --check-prefix=N2M4
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v -filetype=null \
; RUN:   -riscv-vsched-feature-output=%t.pipeline.jsonl \
; RUN:   -debug-pass=Structure %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=PIPELINE

; REQUIRES: asserts

; ROWS: [('lit-candidate', 'vector_loop', 'pre-sched', 2), ('lit-candidate', 'vector_loop', 'post-sched', 2), ('lit-candidate', 'vector_loop', 'post-rvv-ra', 2), ('lit-candidate', 'vector_loop', 'final-sched', 2)]
; METRICS: m2 True True True True True True True True True True True True True True True True True
; REPLAY: 0 0 True
; NESTED: 4 [2]
; FILTERED: 0
; NO-SCHED: 4 False 0
; TOPDOWN: topdown bottomup True 0 0
; FRACTIONAL: mf8 1
; M8-WORK: 8 1 True True True
; M8-OVERLAP: 32 4 4 4 True 0 0 0
; M8-NOV0: True 4 3
; N2M4: True 8 4 4

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

define void @m8_independent(ptr %out0, ptr %out1, ptr %in0, ptr %in1) {
entry:
  br label %loop

loop:
  %a = load <vscale x 16 x i32>, ptr %in0, align 4
  %b = load <vscale x 16 x i32>, ptr %in1, align 4
  %x = add <vscale x 16 x i32> %a, splat (i32 1)
  %y = add <vscale x 16 x i32> %b, splat (i32 2)
  store <vscale x 16 x i32> %x, ptr %out0, align 4
  store <vscale x 16 x i32> %y, ptr %out1, align 4
  br label %loop
}

define void @m8_overlap(ptr %out0, ptr %out1, ptr %out2, ptr %out3,
                        ptr %in0, ptr %in1, ptr %in2, ptr %in3) {
entry:
  br label %loop

loop:
  %acc0 = phi <vscale x 16 x i32> [ zeroinitializer, %entry ], [ %next0, %loop ]
  %acc1 = phi <vscale x 16 x i32> [ zeroinitializer, %entry ], [ %next1, %loop ]
  %acc2 = phi <vscale x 16 x i32> [ zeroinitializer, %entry ], [ %next2, %loop ]
  %acc3 = phi <vscale x 16 x i32> [ zeroinitializer, %entry ], [ %next3, %loop ]
  %value0 = load <vscale x 16 x i32>, ptr %in0, align 4
  %value1 = load <vscale x 16 x i32>, ptr %in1, align 4
  %value2 = load <vscale x 16 x i32>, ptr %in2, align 4
  %value3 = load <vscale x 16 x i32>, ptr %in3, align 4
  %next0 = add <vscale x 16 x i32> %acc0, %value0
  %next1 = add <vscale x 16 x i32> %acc1, %value1
  %next2 = add <vscale x 16 x i32> %acc2, %value2
  %next3 = add <vscale x 16 x i32> %acc3, %value3
  store <vscale x 16 x i32> %next0, ptr %out0, align 4
  store <vscale x 16 x i32> %next1, ptr %out1, align 4
  store <vscale x 16 x i32> %next2, ptr %out2, align 4
  store <vscale x 16 x i32> %next3, ptr %out3, align 4
  br label %loop
}

define void @m8_nov0_recurrence(ptr %out, <vscale x 16 x i32> %rhs,
                                <vscale x 16 x i1> %mask, i64 %vl) {
entry:
  br label %loop

loop:
  %acc = phi <vscale x 16 x i32> [ zeroinitializer, %entry ], [ %next, %loop ]
  %next = call <vscale x 16 x i32> @llvm.riscv.vadd.mask.nxv16i32.nxv16i32(
      <vscale x 16 x i32> %acc, <vscale x 16 x i32> %acc,
      <vscale x 16 x i32> %rhs, <vscale x 16 x i1> %mask, i64 %vl, i64 1)
  store <vscale x 16 x i32> %next, ptr %out, align 4
  br label %loop
}

define target("riscv.vector.tuple", <vscale x 32 x i8>, 2)
    @n2m4_recurrence(ptr %base, i64 %vl,
                     target("riscv.vector.tuple", <vscale x 32 x i8>, 2) %init) {
entry:
  br label %loop

loop:
  %acc = phi target("riscv.vector.tuple", <vscale x 32 x i8>, 2)
      [ %init, %entry ], [ %next, %loop ]
  %next = call target("riscv.vector.tuple", <vscale x 32 x i8>, 2)
      @llvm.riscv.vlseg2.triscv.vector.tuple_nxv32i8_2t(
          target("riscv.vector.tuple", <vscale x 32 x i8>, 2) %acc,
          ptr %base, i64 %vl, i64 5)
  %done = icmp eq i64 %vl, 0
  br i1 %done, label %exit, label %loop

exit:
  ret target("riscv.vector.tuple", <vscale x 32 x i8>, 2) %next
}
