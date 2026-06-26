; RUN: llc -mtriple=riscv64 -mattr=+v --dlmul-analysis %t.md -o /dev/null %s
; RUN: FileCheck --check-prefix=SUMMARY %s < %t.md
; RUN: FileCheck --check-prefix=M8 %s < %t.md
; RUN: FileCheck --check-prefix=M4 %s < %t.md
; RUN: FileCheck --check-prefix=M2 %s < %t.md

; SUMMARY: # DLMUL analysis summary
; SUMMARY: - RVV seeds:
; SUMMARY: - VRM8 seeds:
; SUMMARY: - LMUL m1 seeds:
; SUMMARY: - LMUL m2 seeds:
; SUMMARY: - LMUL m4 seeds:
; SUMMARY: - LMUL m8 seeds:
; SUMMARY: ## Top candidates
; SUMMARY: "blockers":

; M8: "function": "add_m8"
; M8: "stage":
; M8: "shape": { "from_lmul": "m8", "to_lmul": "m4", "split_factor": 2, "is_downsplit_candidate": true

; M4: "function": "add_m4"
; M4: "stage":
; M4: "shape": { "from_lmul": "m4", "to_lmul": "m2", "split_factor": 2, "is_downsplit_candidate": true

; M2: "function": "add_m2"
; M2: "stage":
; M2: "shape": { "from_lmul": "m2", "to_lmul": "m1", "split_factor": 2, "is_downsplit_candidate": true

define <vscale x 16 x i32> @add_m8(<vscale x 16 x i32> %a,
                                   <vscale x 16 x i32> %b) {
entry:
  %sum0 = add <vscale x 16 x i32> %a, %b
  %sum1 = add <vscale x 16 x i32> %sum0, %b
  ret <vscale x 16 x i32> %sum1
}

define <vscale x 8 x i32> @add_m4(<vscale x 8 x i32> %a,
                                  <vscale x 8 x i32> %b) {
entry:
  %sum0 = add <vscale x 8 x i32> %a, %b
  %sum1 = add <vscale x 8 x i32> %sum0, %b
  ret <vscale x 8 x i32> %sum1
}

define <vscale x 4 x i32> @add_m2(<vscale x 4 x i32> %a,
                                  <vscale x 4 x i32> %b) {
entry:
  %sum0 = add <vscale x 4 x i32> %a, %b
  %sum1 = add <vscale x 4 x i32> %sum0, %b
  ret <vscale x 4 x i32> %sum1
}

define void @load_add_store_m8(ptr %a, ptr %b, ptr %out, i64 %vl) nounwind {
entry:
  %va = load <vscale x 16 x i32>, ptr %a, align 4
  %vb = load <vscale x 16 x i32>, ptr %b, align 4
  %sum = add <vscale x 16 x i32> %va, %vb
  store <vscale x 16 x i32> %sum, ptr %out, align 4
  ret void
}
