; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan -riscv-tp-lcd-memory-beat-bits=256 -riscv-tp-lcd-slide-startup=8 -riscv-tp-lcd-slide-quadratic-cost=3 -riscv-tp-lcd-latency-startup=5 -riscv-tp-lcd-data-index-startup=true < %s 2>&1 | FileCheck %s --check-prefix=TUNED-TP
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan -riscv-tp-lcd-memory-beat-bits=256 -riscv-tp-lcd-slide-startup=8 -riscv-tp-lcd-slide-quadratic-cost=3 -riscv-tp-lcd-latency-startup=5 -riscv-tp-lcd-data-index-startup=true -cost-kind=latency < %s 2>&1 | FileCheck %s --check-prefix=TUNED-LAT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan -riscv-tp-lcd-memory-beat-bits=256 -riscv-tp-lcd-slide-startup=8 -riscv-tp-lcd-slide-quadratic-cost=3 -riscv-tp-lcd-latency-startup=5 -riscv-tp-lcd-data-index-startup=true -cost-kind=code-size < %s 2>&1 | FileCheck %s --check-prefix=SIZE
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=none -riscv-tp-lcd-memory-beat-bits=256 -riscv-tp-lcd-slide-startup=8 -riscv-tp-lcd-slide-quadratic-cost=3 -riscv-tp-lcd-latency-startup=5 -riscv-tp-lcd-data-index-startup=true < %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b < %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=none < %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mcpu=xiangshan-kunminghu -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan < %s 2>&1 | FileCheck %s --check-prefix=TUNED-TP
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mcpu=generic-rv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=saturn < %s 2>&1 | FileCheck %s --check-prefix=TP
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan -cost-kind=latency < %s 2>&1 | FileCheck %s --check-prefix=TUNED-LAT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=saturn -cost-kind=latency < %s 2>&1 | FileCheck %s --check-prefix=LAT
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=none -cost-kind=code-size < %s 2>&1 | FileCheck %s --check-prefix=SIZE
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=xiangshan -cost-kind=code-size < %s 2>&1 | FileCheck %s --check-prefix=SIZE
; RUN: opt -passes='print<cost-model>' -disable-output -mtriple=riscv64 -mattr=+v,+f,+d,+zvl128b -riscv-tp-lcd-profile=saturn -cost-kind=code-size < %s 2>&1 | FileCheck %s --check-prefix=SIZE

; Profiles are explicitly selected software assumptions, including on a generic
; host CPU. Saturn retains independent, unvalidated initial assumptions.
define void @costs(ptr %p, <vscale x 4 x float> %a, <vscale x 4 x float> %b) {
; DEFAULT: cost of 4 for instruction: %add = fadd
; TP: cost of 2 for instruction: %add = fadd
; LAT: cost of 5 for instruction: %add = fadd
; SIZE: cost of 1 for instruction: %add = fadd
; TUNED-TP: cost of 2 for instruction: %add = fadd
; TUNED-LAT: cost of 7 for instruction: %add = fadd
  %add = fadd <vscale x 4 x float> %a, %b
; DEFAULT: cost of 2 for instruction: %load = load
; TP: cost of 2 for instruction: %load = load
; LAT: cost of 5 for instruction: %load = load
; SIZE: cost of 1 for instruction: %load = load
; TUNED-TP: cost of 1 for instruction: %load = load
; TUNED-LAT: cost of 6 for instruction: %load = load
  %load = load <vscale x 4 x float>, ptr %p, align 4
; DEFAULT: cost of 2 for instruction: %fma = call
; TP: cost of 2 for instruction: %fma = call
; LAT: cost of 5 for instruction: %fma = call
; SIZE: cost of 1 for instruction: %fma = call
; TUNED-TP: cost of 2 for instruction: %fma = call
; TUNED-LAT: cost of 7 for instruction: %fma = call
  %fma = call <vscale x 4 x float> @llvm.fma.nxv4f32(<vscale x 4 x float> %a, <vscale x 4 x float> %b, <vscale x 4 x float> %load)
; DEFAULT: cost of 4 for instruction: %splice = call
; TP: cost of 4 for instruction: %splice = call
; LAT: cost of 10 for instruction: %splice = call
; SIZE: cost of 2 for instruction: %splice = call
; TUNED-TP: cost of 22 for instruction: %splice = call
; TUNED-LAT: cost of 32 for instruction: %splice = call
  %splice = call <vscale x 4 x float> @llvm.vector.splice.right.nxv4f32(<vscale x 4 x float> %a, <vscale x 4 x float> %b, i32 1)
; DEFAULT: cost of 2 for instruction: %scalar = fadd
; TP: cost of 2 for instruction: %scalar = fadd
; LAT: cost of 3 for instruction: %scalar = fadd
; SIZE: cost of 1 for instruction: %scalar = fadd
; TUNED-TP: cost of 2 for instruction: %scalar = fadd
; TUNED-LAT: cost of 3 for instruction: %scalar = fadd
  %scalar = fadd float 1.0, 2.0
; DEFAULT: cost of 16 for instruction: %split = load
; TP: cost of 16 for instruction: %split = load
; LAT: cost of 22 for instruction: %split = load
; SIZE: cost of 2 for instruction: %split = load
; TUNED-TP: cost of 8 for instruction: %split = load
; TUNED-LAT: cost of 18 for instruction: %split = load
  %split = load <vscale x 32 x float>, ptr %p, align 4
  ret void
}

; Native VP loads use the same target memory latency rather than a generic
; instruction latency. Indexed costs retain their existing per-lane scaling.
define void @vp_costs(ptr %p, <vscale x 4 x ptr> %ptrs, <vscale x 4 x i1> %mask, i32 %evl) {
; TP: cost of 2 for instruction: %vp = call
; LAT: cost of 5 for instruction: %vp = call
; TUNED-TP: cost of 1 for instruction: %vp = call
; TUNED-LAT: cost of 6 for instruction: %vp = call
  %vp = call <vscale x 4 x float> @llvm.vp.load.nxv4f32(ptr %p, <vscale x 4 x i1> %mask, i32 %evl)
; TP: cost of 8 for instruction: %gather = call
; LAT: cost of 11 for instruction: %gather = call
; TUNED-TP: cost of 8 for instruction: %gather = call
; TUNED-LAT: cost of 13 for instruction: %gather = call
  %gather = call <vscale x 4 x float> @llvm.masked.gather.nxv4f32.nxv4p0(<vscale x 4 x ptr> %ptrs, i32 4, <vscale x 4 x i1> %mask, <vscale x 4 x float> poison)
  ret void
}

; A loaded index, including an integer extension, pays the common startup.
; The argument-only index in vp_costs above must not acquire this startup.
define void @loaded_index(ptr %base, ptr %indices, <vscale x 4 x i1> %mask) {
  %idx = load <vscale x 4 x i32>, ptr %indices, align 4
  %ext = sext <vscale x 4 x i32> %idx to <vscale x 4 x i64>
  %addresses = getelementptr float, ptr %base, <vscale x 4 x i64> %ext
; TP: cost of 8 for instruction: %indexed = call
; LAT: cost of 11 for instruction: %indexed = call
; TUNED-TP: cost of 13 for instruction: %indexed = call
; TUNED-LAT: cost of 13 for instruction: %indexed = call
  %indexed = call <vscale x 4 x float> @llvm.masked.gather.nxv4f32.nxv4p0(<vscale x 4 x ptr> %addresses, i32 4, <vscale x 4 x i1> %mask, <vscale x 4 x float> poison)
  ret void
}
