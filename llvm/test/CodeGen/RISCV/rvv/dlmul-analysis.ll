; RUN: llc -mtriple=riscv64 -mattr=+v --dlmul-analysis %t.md -o /dev/null %s
; RUN: FileCheck %s < %t.md

; CHECK: # DLMUL analysis summary
; CHECK: - VRM8 seeds:
; CHECK: ## Top candidates
; CHECK: "function": "add_m8"
; CHECK: "stage":
; CHECK: "from_lmul": "m8"
; CHECK: "to_lmul": "m4"
; CHECK: "blockers":

define <vscale x 16 x i32> @add_m8(<vscale x 16 x i32> %a,
                                   <vscale x 16 x i32> %b) {
entry:
  %sum0 = add <vscale x 16 x i32> %a, %b
  %sum1 = add <vscale x 16 x i32> %sum0, %b
  ret <vscale x 16 x i32> %sum1
}

define void @load_add_store_m8(ptr %a, ptr %b, ptr %out, i64 %vl) nounwind {
entry:
  %va = load <vscale x 16 x i32>, ptr %a, align 4
  %vb = load <vscale x 16 x i32>, ptr %b, align 4
  %sum = add <vscale x 16 x i32> %va, %vb
  store <vscale x 16 x i32> %sum, ptr %out, align 4
  ret void
}
