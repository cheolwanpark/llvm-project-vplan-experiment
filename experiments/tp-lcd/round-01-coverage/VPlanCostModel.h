//===- VPlanCostModel.h - Experimental throughput/recurrence cost -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_VECTORIZE_VPLANCOSTMODEL_H
#define LLVM_TRANSFORMS_VECTORIZE_VPLANCOSTMODEL_H

#include "VPlanAnalysis.h"
#include "llvm/Support/TypeSize.h"

namespace llvm {
struct VPCostContext;

struct VPTPLCDCost {
  InstructionCost Memory = 0;
  InstructionCost Compute = 0;
  InstructionCost Other = 0;
  unsigned MemoryRecipes = 0;
  unsigned ComputeRecipes = 0;
  unsigned OtherRecipes = 0;
  unsigned ZeroCostRecipes = 0;
  VPRecurrenceLatency Recurrence;
  std::string FallbackReason;

  double throughput() const;
  double loopScore() const;
};

/// Sum domains over the whole loop before taking their maximum. GetThroughput
/// owns the legacy skip state; Precomputed includes the loop branch. LatencyCtx
/// must be a fresh latency context with no precomputed/skip state.
VPTPLCDCost computeVPlanTPLCDCost(
    VPlan &Plan, ElementCount VF, InstructionCost Precomputed,
    function_ref<InstructionCost(VPRecipeBase &)> GetThroughput,
    VPCostContext &LatencyCtx);
} // namespace llvm

#endif
