; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-list -disable-output < %s 2>&1 | FileCheck %s --check-prefix=LIST
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-report -disable-output < %s 2>&1 | FileCheck %s --check-prefix=REPORT
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-report -vplan-list -disable-output < %s 2>&1 | FileCheck %s --check-prefix=REPORT-ONLY
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-report -vplan-jsonl -disable-output < %s 2>&1 | %python -c "import json, sys; [json.loads(line) for line in sys.stdin]"
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -S < %s | FileCheck %s --check-prefix=IR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -vplan-report -S < %s | FileCheck %s --check-prefix=IR
; RUN: opt -passes=loop-vectorize -force-vector-interleave=1 -use-vf=s2 -S < %s | FileCheck %s --check-prefix=SCALABLE

target triple = "aarch64-unknown-linux-gnu"

; LIST-DAG: vplan-list schema=1 record="plan" function="test_v2_v4" loop=0 path="inner" plan={{[0-9]+}} vf="f1" selected=false status="evaluated" compare_kind="trip_count" compare_num={{[0-9]+}} compare_den=1 trip_count=1024
; LIST-DAG: vplan-list schema=1 record="plan" function="test_v2_v4" loop=0 path="inner" plan={{[0-9]+}} vf="f2" selected=false status="evaluated" compare_kind="trip_count" compare_num={{[0-9]+}} compare_den=1 trip_count=1024
; LIST-DAG: vplan-list schema=1 record="plan" function="test_v2_v4" loop=0 path="inner" plan={{[0-9]+}} vf="f4" selected=true status="evaluated" compare_kind="trip_count" compare_num={{[0-9]+}} compare_den=1 trip_count=1024
; LIST: vplan-list schema=1 record="plan" function="unknown_tc" loop=0 path="inner" plan={{[0-9]+}} vf="f{{[124]}}" selected={{true|false}} status="evaluated" compare_kind="per_lane" compare_num={{[0-9]+}} compare_den={{[124]}}
; LIST-NOT: record="component"
; LIST-NOT: record="recipe"

; REPORT: vplan-report schema=1 record="plan" function="test_v2_v4"
; REPORT: record="component" function="test_v2_v4"{{.*}}scope="loop" name="induction" cost={{[0-9]+}}
; REPORT: record="component" function="test_v2_v4"{{.*}}scope="recipe" name="memory" cost={{[0-9]+}}
; REPORT: record="recipe" function="test_v2_v4"{{.*}}vf="f2"{{.*}}group="memory" source="vplan" cost={{[0-9]+}}

; REPORT-ONLY: vplan-report schema=1
; REPORT-ONLY-NOT: vplan-list

define void @test_v2_v4(ptr noalias %a, ptr readonly %b) #0 {
; IR-LABEL: @test_v2_v4(
; IR:       vector.body:
; IR-NEXT:    [[INDEX:%.*]] = phi i64 [ 0, %vector.ph ], [ [[INDEX_NEXT:%.*]], %vector.body ]
; IR-NEXT:    [[TMP1:%.*]] = getelementptr i64, ptr [[B:%.*]], i64 [[INDEX]]
; IR-NEXT:    [[WIDE_LOAD:%.*]] = load <4 x i64>, ptr [[TMP1]], align 8
; IR-NEXT:    [[TMP3:%.*]] = call <4 x i64> @foo_vector_fixed4_nomask(<4 x i64> [[WIDE_LOAD]])
; IR-NEXT:    [[TMP4:%.*]] = getelementptr inbounds i64, ptr [[A:%.*]], i64 [[INDEX]]
; IR-NEXT:    store <4 x i64> [[TMP3]], ptr [[TMP4]], align 8
; IR-NEXT:    [[INDEX_NEXT]] = add nuw i64 [[INDEX]], 4
; IR-NEXT:    [[TMP6:%.*]] = icmp eq i64 [[INDEX_NEXT]], 1024
; IR-NEXT:    br i1 [[TMP6]], label %middle.block, label %vector.body, !llvm.loop [[LOOP4:![0-9]+]]
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

define void @unknown_tc(ptr noalias %a, ptr readonly %b, i64 %n) #0 {
; SCALABLE-LABEL: @unknown_tc(
; SCALABLE:       vector.body:
; SCALABLE:       load <vscale x 2 x i64>
; SCALABLE:       store <vscale x 2 x i64>
entry:
  br label %loop

loop:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %loop ]
  %src = getelementptr i64, ptr %b, i64 %iv
  %value = load i64, ptr %src, align 8
  %dst = getelementptr i64, ptr %a, i64 %iv
  store i64 %value, ptr %dst, align 8
  %iv.next = add nuw i64 %iv, 1
  %done = icmp eq i64 %iv.next, %n
  br i1 %done, label %exit, label %loop

exit:
  ret void
}
