//===- VPlanRecurrenceTest.cpp - Closed recurrence latency tests
//------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../lib/Transforms/Vectorize/VPlanAnalysis.h"
#include "VPlanTestBase.h"

namespace llvm {
namespace {

class VPlanRecurrenceTest : public VPlanTestBase {
protected:
  VPBasicBlock *makeLoop(VPlan &Plan) {
    auto *Body = Plan.createVPBasicBlock("body");
    auto *Region = Plan.createLoopRegion("loop", Body, Body);
    VPBlockUtils::connectBlocks(Plan.getEntry(), Region);
    VPBlockUtils::connectBlocks(Region, Plan.getScalarHeader());
    return Body;
  }

  VPPhi *phi(VPlan &Plan, VPBasicBlock *Body) {
    auto *Zero = Plan.getConstantInt(32, 0);
    auto *Phi = new VPPhi({Zero, Zero}, {});
    Body->appendRecipe(Phi);
    return Phi;
  }

  VPInstruction *op(VPBasicBlock *Body, VPValue *Value,
                    unsigned Opcode = Instruction::Add) {
    auto *R = new VPInstruction(Opcode, {Value, Value});
    Body->appendRecipe(R);
    return R;
  }
};

TEST_F(VPlanRecurrenceTest, IndependentLongChainDoesNotSetLCD) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *Phi = phi(Plan, Body);
  auto *Update = op(Body, Phi);
  Phi->setOperand(1, Update);
  VPValue *Chain = Plan.getConstantInt(32, 1);
  for (unsigned I = 0; I != 20; ++I)
    Chain = op(Body, Chain, Instruction::Mul);
  auto Result = computeVPlanRecurrenceLatency(Plan, [&](VPRecipeBase &R) {
    return InstructionCost(isa<VPPhi>(&R) ? 0 : &R == Update ? 7 : 100);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 7);
  EXPECT_EQ(Result.Recurrence, Phi);
  EXPECT_EQ(Result.Distance, 1u);
}

TEST_F(VPlanRecurrenceTest, MutualPhisUseIterationDistance) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *A = phi(Plan, Body);
  auto *B = phi(Plan, Body);
  auto *UpdateA = op(Body, B);
  auto *UpdateB = op(Body, A);
  A->setOperand(1, UpdateA);
  B->setOperand(1, UpdateB);
  auto Result = computeVPlanRecurrenceLatency(Plan, [&](VPRecipeBase &R) {
    return InstructionCost(isa<VPPhi>(&R) ? 0 : &R == UpdateA ? 4 : 8);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 6);
  EXPECT_EQ(Result.Distance, 2u);
}

TEST_F(VPlanRecurrenceTest, IndependentAccumulatorsUseMaximum) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *A = phi(Plan, Body);
  auto *B = phi(Plan, Body);
  auto *UpdateA = op(Body, A);
  auto *UpdateB = op(Body, B);
  A->setOperand(1, UpdateA);
  B->setOperand(1, UpdateB);
  auto Result = computeVPlanRecurrenceLatency(Plan, [&](VPRecipeBase &R) {
    return InstructionCost(isa<VPPhi>(&R) ? 0 : &R == UpdateA ? 4 : 8);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 8);
  EXPECT_EQ(Result.Recurrence, B);
}

TEST_F(VPlanRecurrenceTest, DistanceThreeCycle) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *A = phi(Plan, Body);
  auto *B = phi(Plan, Body);
  auto *C = phi(Plan, Body);
  A->setOperand(1, op(Body, B));
  B->setOperand(1, op(Body, C));
  C->setOperand(1, op(Body, A));
  auto Result = computeVPlanRecurrenceLatency(Plan, [](VPRecipeBase &R) {
    return InstructionCost(isa<VPPhi>(&R) ? 0 : 5);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 5);
  EXPECT_EQ(Result.Distance, 3u);
}

TEST_F(VPlanRecurrenceTest, WidenedInductionHasImplicitBackedge) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  InductionDescriptor Descriptor;
  auto *IV = new VPWidenIntOrFpInductionRecipe(
      nullptr, Plan.getConstantInt(32, 0), Plan.getConstantInt(32, 1),
      Plan.getConstantInt(32, 4), Descriptor, {}, {});
  Body->appendRecipe(IV);
  auto Result = computeVPlanRecurrenceLatency(
      Plan, [](VPRecipeBase &) { return InstructionCost(3); });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 3);
  EXPECT_EQ(Result.Recurrence, IV);
  EXPECT_EQ(Result.Distance, 1u);
}

TEST_F(VPlanRecurrenceTest, ClosedFirstOrderSplice) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  VPValue *Start = Plan.getConstantInt(32, 0);
  auto *Phi = new VPFirstOrderRecurrencePHIRecipe(nullptr, *Start, *Start);
  Body->appendRecipe(Phi);
  auto *Splice = new VPInstruction(VPInstruction::FirstOrderRecurrenceSplice,
                                   {Phi, Start});
  Body->appendRecipe(Splice);
  Phi->setBackedgeValue(Splice);
  auto Result = computeVPlanRecurrenceLatency(Plan, [&](VPRecipeBase &R) {
    return InstructionCost(&R == Splice ? 12 : 0);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 12);
  EXPECT_EQ(Result.Recurrence, Phi);
}

TEST_F(VPlanRecurrenceTest, LoadFedSpliceIsNotClosed) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *Phi = phi(Plan, Body);
  auto *Feed = op(Body, Plan.getConstantInt(32, 1));
  Phi->setOperand(1, Feed);
  auto *Splice =
      new VPInstruction(VPInstruction::FirstOrderRecurrenceSplice, {Phi, Feed});
  Body->appendRecipe(Splice);
  op(Body, Splice);
  auto Result = computeVPlanRecurrenceLatency(Plan, [](VPRecipeBase &R) {
    return InstructionCost(isa<VPPhi>(&R) ? 0 : 100);
  });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Cycles, 0);
  EXPECT_EQ(Result.Recurrence, nullptr);
}

TEST_F(VPlanRecurrenceTest, UnsupportedAndInvalidHaveReasons) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *Phi = phi(Plan, Body);
  Phi->setOperand(1, op(Body, Phi));
  auto Result = computeVPlanRecurrenceLatency(
      Plan, [](VPRecipeBase &) { return InstructionCost::getInvalid(); });
  EXPECT_EQ(Result.FallbackReason, "unsupported-or-invalid-recipe-latency");
}

TEST_F(VPlanRecurrenceTest, SameIterationCycleHasReason) {
  VPlan &Plan = getPlan();
  auto *Body = makeLoop(Plan);
  auto *A = op(Body, Plan.getConstantInt(32, 0));
  auto *B = op(Body, A);
  A->setOperand(0, B);
  auto Result = computeVPlanRecurrenceLatency(
      Plan, [](VPRecipeBase &) { return InstructionCost(1); });
  EXPECT_EQ(Result.FallbackReason, "same-iteration-dependency-cycle");
}

} // namespace
} // namespace llvm
