//===- VPlanCostModel.cpp - Experimental throughput/recurrence cost ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VPlanCostModel.h"
#include "VPlan.h"
#include "VPlanHelpers.h"
#include "VPlanUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;

double VPTPLCDCost::throughput() const {
  return Other.getValue() + std::max(Memory.getValue(), Compute.getValue());
}

double VPTPLCDCost::loopScore() const {
  return std::max(throughput(), Recurrence.Cycles);
}

DenseMap<VPRecipeBase *, VPFusedRecipeCost> llvm::computeVPlanFusedCosts(
    VPlan &Plan, ElementCount VF, VPCostContext &TPContext,
    VPCostContext &LatencyContext) {
  DenseMap<VPRecipeBase *, VPFusedRecipeCost> Costs;
  for (VPBasicBlock *BB : VPBlockUtils::blocksOnly<VPBasicBlock>(
           vp_depth_first_shallow(Plan.getVectorLoopRegion()->getEntry()))) {
    for (VPRecipeBase &R : *BB) {
      auto *Add = dyn_cast<VPWidenRecipe>(&R);
      if (!Add || Add->getOpcode() != Instruction::FAdd ||
          !Add->hasFastMathFlags() ||
          !Add->getFastMathFlags().allowContract() ||
          TPContext.skipCostComputation(Add->getUnderlyingInstr(), true))
        continue;
      Type *Ty = VectorType::get(TPContext.Types.inferScalarType(Add), VF);
      if (!TPContext.TTI.isTypeLegal(Ty))
        continue;
      for (VPValue *Op : Add->operands()) {
        auto *Mul = dyn_cast<VPWidenRecipe>(Op);
        if (!Mul || Mul->getOpcode() != Instruction::FMul ||
            Mul->getParent() != BB || Mul->getNumUsers() != 1 ||
            !Mul->hasFastMathFlags() ||
            !Mul->getFastMathFlags().allowContract() ||
            TPContext.skipCostComputation(Mul->getUnderlyingInstr(), true))
          continue;
        IntrinsicCostAttributes Attrs(Intrinsic::fma, Ty, {Ty, Ty, Ty},
                                      Add->getFastMathFlags());
        InstructionCost FMA = TPContext.TTI.getIntrinsicInstrCost(
            Attrs, TargetTransformInfo::TCK_RecipThroughput);
        InstructionCost Separate = Add->getCostForRecipeWithOpcode(
                                       Instruction::FAdd, VF, TPContext) +
                                   Mul->getCostForRecipeWithOpcode(
                                       Instruction::FMul, VF, TPContext);
        InstructionCost Latency = LatencyContext.TTI.getIntrinsicInstrCost(
            Attrs, TargetTransformInfo::TCK_Latency);
        // The legal vector type plus a strictly cheaper FMA cost is the TTI
        // evidence for native contraction, rather than target-name matching.
        if (!FMA.isValid() || !Separate.isValid() || !Latency.isValid() ||
            FMA < 0 || Latency < 0 || FMA >= Separate)
          continue;
        Costs[Mul] = {0, 0};
        Costs[Add] = {FMA, Latency};
        break;
      }
    }
  }
  return Costs;
}

namespace {
enum class Domain { Memory, Compute, Other };

Domain classify(const VPRecipeBase &R) {
  // Scalar execution belongs to Other even when the underlying opcode is FP
  // or memory. Do not classify comparisons by their i1 result type.
  if (isa<VPReplicateRecipe>(&R))
    return Domain::Other;
  if (isa<VPWidenMemoryRecipe>(&R))
    return Domain::Memory;
  if (isa<VPWidenRecipe, VPWidenCastRecipe, VPWidenIntrinsicRecipe,
          VPWidenCanonicalIVRecipe>(&R))
    return Domain::Compute;
  if (auto *I = dyn_cast<VPInstruction>(&R)) {
    if (I->canGenerateScalarForFirstLane() &&
        (vputils::isSingleScalar(I) || vputils::onlyFirstLaneUsed(I)))
      return Domain::Other;
    unsigned Op = I->getOpcode();
    if (Instruction::isBinaryOp(Op) || Instruction::isCast(Op) ||
        Op == Instruction::ICmp || Op == Instruction::FCmp ||
        Op == Instruction::Select ||
        Op == VPInstruction::FirstOrderRecurrenceSplice ||
        Op == VPInstruction::Reverse)
      return Domain::Compute;
  }
  // Composite recipes remain additive until their components can be costed
  // separately. In particular, do not silently treat interleaves as pure loads.
  return Domain::Other;
}

} // namespace

InstructionCost llvm::computeVPlanRecipeLatency(VPRecipeBase &R, ElementCount VF,
                                              VPCostContext &Ctx) {
  assert(Ctx.CostKind == TargetTransformInfo::TCK_Latency &&
         Ctx.SkipCostComputation.empty());
  // Widened induction synthesizes an update, rather than exposing a def-use
  // backedge. A scalar-only induction uses a scalar add.
  if (auto *IV = dyn_cast<VPWidenInductionRecipe>(&R)) {
    Type *Ty = Ctx.Types.inferScalarType(IV);
    if (Ty->isPointerTy())
      Ty = Type::getIntNTy(
          Ctx.LLVMCtx,
          Ctx.L->getHeader()->getModule()->getDataLayout().getPointerSizeInBits(
              Ty->getPointerAddressSpace()));
    if (!vputils::onlyFirstLaneUsed(IV))
      Ty = VectorType::get(Ty, VF);
    unsigned Opcode = IV->getInductionDescriptor().getInductionOpcode();
    if (Opcode == Instruction::BinaryOpsEnd)
      Opcode = Instruction::Add;
    return Ctx.TTI.getArithmeticInstrCost(Opcode, Ty, Ctx.CostKind);
  }
  if (R.isPhi())
    return 0;
  if (auto *W = dyn_cast<VPWidenRecipe>(&R))
    return W->getCostForRecipeWithOpcode(W->getOpcode(), VF, Ctx);
  if (auto *C = dyn_cast<VPWidenCastRecipe>(&R))
    return C->getCostForRecipeWithOpcode(C->getOpcode(), VF, Ctx);
  if (auto *I = dyn_cast<VPWidenIntrinsicRecipe>(&R))
    return I->computeCost(VF, Ctx);
  if (auto *M = dyn_cast<VPWidenLoadEVLRecipe>(&R))
    return M->computeCost(VF, Ctx);
  if (auto *M = dyn_cast<VPWidenStoreEVLRecipe>(&R))
    return M->computeCost(VF, Ctx);
  if (auto *M = dyn_cast<VPWidenMemoryRecipe>(&R))
    return M->computeCost(VF, Ctx);
  if (auto *I = dyn_cast<VPInstruction>(&R)) {
    unsigned Op = I->getOpcode();
    if (Instruction::isBinaryOp(Op) || Instruction::isCast(Op) ||
        Op == Instruction::ICmp || Op == Instruction::FCmp ||
        Op == Instruction::Freeze) {
      ElementCount Width = vputils::isSingleScalar(I) ||
                                   vputils::onlyFirstLaneUsed(I)
                               ? ElementCount::getFixed(1)
                               : VF;
      return I->getCostForRecipeWithOpcode(Op, Width, Ctx);
    }
    switch (Op) {
    case VPInstruction::FirstOrderRecurrenceSplice:
    case VPInstruction::ExplicitVectorLength:
    case VPInstruction::ActiveLaneMask:
    case VPInstruction::Reverse:
    case Instruction::Select:
      return I->computeCost(VF, Ctx);
    case VPInstruction::BranchOnCount:
    case VPInstruction::BranchOnCond:
      // These have no result and cannot participate in an SSA recurrence.
      return 0;
    default:
      return InstructionCost::getInvalid();
    }
  }
  if (auto *Rep = dyn_cast<VPReplicateRecipe>(&R)) {
    if (!Rep->isSingleScalar() || Rep->isPredicated())
      return InstructionCost::getInvalid();
    Instruction *I = Rep->getUnderlyingInstr();
    if (!I->isBinaryOp() && !I->isCast() && !isa<GetElementPtrInst, CmpInst>(I))
      return InstructionCost::getInvalid();
    SmallVector<const Value *> Ops(I->operands());
    return Ctx.TTI.getInstructionCost(I, Ops, Ctx.CostKind);
  }
  if (auto *S = dyn_cast<VPScalarIVStepsRecipe>(&R)) {
    // With IC=1 and only lane zero demanded, this forwards its start operand.
    if (vputils::onlyFirstLaneUsed(S))
      return 0;
    return InstructionCost::getInvalid();
  }
  if (auto *IV = dyn_cast<VPDerivedIVRecipe>(&R)) {
    Type *Ty = IV->getScalarType();
    if (!Ty->isIntegerTy())
      return InstructionCost::getInvalid();
    // Start + IV * Step; the unit step is an identity multiplication.
    auto *Step = dyn_cast<VPIRValue>(IV->getStepValue());
    auto *C = Step ? dyn_cast<ConstantInt>(Step->getValue()) : nullptr;
    InstructionCost Cost = Ctx.TTI.getArithmeticInstrCost(
        Instruction::Add, Ty, Ctx.CostKind);
    if (!C || !C->isOne())
      Cost += Ctx.TTI.getArithmeticInstrCost(Instruction::Mul, Ty, Ctx.CostKind);
    return Cost;
  }
  if (isa<VPWidenGEPRecipe, VPVectorPointerRecipe>(&R)) {
    // Address formation is approximated by one add, not by a memory latency.
    auto *V = cast<VPSingleDefRecipe>(&R);
    Type *PtrTy = Ctx.Types.inferScalarType(V);
    Type *Ty = Type::getIntNTy(
        Ctx.LLVMCtx,
        Ctx.L->getHeader()->getModule()->getDataLayout().getPointerSizeInBits(
            PtrTy->getPointerAddressSpace()));
    if (!vputils::isSingleScalar(V))
      Ty = VectorType::get(Ty, VF);
    return Ctx.TTI.getArithmeticInstrCost(Instruction::Add, Ty, Ctx.CostKind);
  }
  return InstructionCost::getInvalid();
}

VPTPLCDCost llvm::computeVPlanTPLCDCost(
    VPlan &Plan, ElementCount VF, InstructionCost Precomputed,
    function_ref<InstructionCost(VPRecipeBase &)> GetThroughput,
    function_ref<InstructionCost(VPRecipeBase &)> GetLatency) {
  VPTPLCDCost Result;
  Result.Other = Precomputed;
  auto *Region = Plan.getVectorLoopRegion();
  if (!Region) {
    Result.FallbackReason = "no vector loop region";
    return Result;
  }
  for (VPBlockBase *B : vp_depth_first_shallow(Region->getEntry())) {
    auto *BB = dyn_cast<VPBasicBlock>(B);
    if (!BB) {
      Result.FallbackReason = "nested or replicated region";
      return Result;
    }
    for (VPRecipeBase &R : *BB) {
      InstructionCost Cost = GetThroughput(R);
      if (!Cost.isValid() || Cost < 0) {
        Result.FallbackReason = "invalid recipe throughput";
        return Result;
      }
      Result.ZeroCostRecipes += Cost == 0;
      switch (classify(R)) {
      case Domain::Memory:
        Result.Memory += Cost;
        ++Result.MemoryRecipes;
        break;
      case Domain::Compute:
        Result.Compute += Cost;
        ++Result.ComputeRecipes;
        break;
      case Domain::Other:
        Result.Other += Cost;
        ++Result.OtherRecipes;
        break;
      }
    }
  }
  if (!Result.Other.isValid() || Result.Other < 0) {
    Result.FallbackReason = "invalid precomputed throughput";
    return Result;
  }
  std::string Unsupported;
  Result.Recurrence = computeVPlanRecurrenceLatency(Plan, [&](VPRecipeBase &R) {
    InstructionCost L = GetLatency(R);
    if (!L.isValid() || L < 0) {
      Unsupported = "unsupported-or-invalid-recipe-latency: recipe-id=" +
                    std::to_string(R.getVPDefID());
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
      raw_string_ostream OS(Unsupported);
      VPSlotTracker Slots(&Plan);
      R.print(OS, " ", Slots);
#endif
    }
    return L;
  });
  Result.FallbackReason = Result.Recurrence.FallbackReason;
  if (!Unsupported.empty())
    Result.FallbackReason = std::move(Unsupported);
  return Result;
}
