//===- VPlanCostModelTest.cpp - Whole-loop TP domain tests
//------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../lib/Transforms/Vectorize/VPlanCostModel.h"
#include "VPlanTestBase.h"
#include "llvm/IR/IRBuilder.h"

namespace llvm {
namespace {
class VPlanCostModelTest : public VPlanTestBase {
protected:
  std::pair<VPBasicBlock *, VPBasicBlock *> makeLoop(VPlan &Plan) {
    auto *A = Plan.createVPBasicBlock("first");
    auto *B = Plan.createVPBasicBlock("second");
    VPBlockUtils::connectBlocks(A, B);
    auto *Region = Plan.createLoopRegion("loop", A, B);
    VPBlockUtils::connectBlocks(Plan.getEntry(), Region);
    VPBlockUtils::connectBlocks(Region, Plan.getScalarHeader());
    return {A, B};
  }
};

TEST_F(VPlanCostModelTest, DomainsAreSummedBeforeTakingMaximum) {
  auto &Plan = getPlan();
  auto [A, B] = makeLoop(Plan);
  IRBuilder<> Builder(ScalarHeader->getTerminator());
  auto *Ptr = ConstantPointerNull::get(PointerType::get(C, 0));
  auto *Load = Builder.CreateLoad(Builder.getInt32Ty(), Ptr);
  auto *Add = cast<Instruction>(Builder.CreateAdd(Load, Load));
  auto *Mul = cast<Instruction>(Builder.CreateMul(Add, Add));
  auto *Store = Builder.CreateStore(Mul, Ptr);
  auto *VPPtr = Plan.getOrAddLiveIn(Ptr);
  auto *L = new VPWidenLoadRecipe(*Load, VPPtr, nullptr, true, false, {}, {});
  auto *X = new VPWidenRecipe(*Add, {L, L});
  auto *Y = new VPWidenRecipe(*Mul, {X, X});
  auto *S =
      new VPWidenStoreRecipe(*Store, VPPtr, Y, nullptr, true, false, {}, {});
  A->appendRecipe(L);
  A->appendRecipe(X);
  B->appendRecipe(Y);
  B->appendRecipe(S);
  unsigned Calls = 0;
  auto Result = computeVPlanTPLCDCost(
      Plan, ElementCount::getFixed(4), 3,
      [&](VPRecipeBase &R) {
        ++Calls;
        return InstructionCost(&R == L ? 8 : &R == X ? 1 : &R == Y ? 9 : 2);
      },
      [](VPRecipeBase &) { return InstructionCost(0); });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Calls, 4u);
  EXPECT_EQ(Result.Memory, 10);
  EXPECT_EQ(Result.Compute, 10);
  EXPECT_EQ(Result.Other, 3);
  EXPECT_EQ(Result.throughput(), 13);
  EXPECT_EQ(Result.loopScore(), 13);
  EXPECT_EQ(Result.MemoryRecipes, 2u);
  EXPECT_EQ(Result.ComputeRecipes, 2u);
}

TEST_F(VPlanCostModelTest, FloatingCompareIsComputeAndReplicateIsOther) {
  auto &Plan = getPlan();
  auto [A, B] = makeLoop(Plan);
  IRBuilder<> Builder(ScalarHeader->getTerminator());
  auto *Ptr = ConstantPointerNull::get(PointerType::get(C, 0));
  auto *Load = Builder.CreateLoad(Builder.getFloatTy(), Ptr);
  auto *Cmp = cast<Instruction>(Builder.CreateFCmpOLT(Load, Load));
  auto *Cast = cast<Instruction>(Builder.CreateZExt(Cmp, Builder.getInt32Ty()));
  auto *Store = Builder.CreateStore(Builder.getFalse(), Ptr);
  auto *VPPtr = Plan.getOrAddLiveIn(Ptr);
  auto *L = new VPWidenLoadRecipe(*Load, VPPtr, nullptr, true, false, {}, {});
  auto *X = new VPWidenRecipe(*Cmp, {L, L}, VPIRFlags(CmpInst::FCMP_OLT));
  auto *S = new VPWidenStoreRecipe(*Store, VPPtr,
                                   Plan.getOrAddLiveIn(Builder.getFalse()),
                                   nullptr, true, false, {}, {});
  // Only lane zero is consumed, but the widened compare still executes as a
  // vector recipe. The scalar consumer is independently classified as Other.
  auto *Scalar = new VPReplicateRecipe(Cast, {X}, true);
  A->appendRecipe(L);
  A->appendRecipe(X);
  B->appendRecipe(S);
  B->appendRecipe(Scalar);
  auto Result = computeVPlanTPLCDCost(
      Plan, ElementCount::getFixed(4), 7,
      [&](VPRecipeBase &R) {
        return InstructionCost(&R == X ? 5 : &R == Scalar ? 3 : 0);
      },
      [](VPRecipeBase &) { return InstructionCost(0); });
  EXPECT_TRUE(Result.FallbackReason.empty());
  EXPECT_EQ(Result.Compute, 5);
  EXPECT_EQ(Result.Other, 10);
  EXPECT_EQ(Result.ZeroCostRecipes, 2u);
}

TEST_F(VPlanCostModelTest, InvalidCostsHaveExplicitFallback) {
  auto &Plan = getPlan();
  auto [A, B] = makeLoop(Plan);
  auto *V = Plan.getConstantInt(32, 1);
  auto *R = new VPInstruction(Instruction::Add, {V, V});
  A->appendRecipe(R);
  auto Result = computeVPlanTPLCDCost(
      Plan, ElementCount::getFixed(4), 0,
      [](VPRecipeBase &) { return InstructionCost::getInvalid(); },
      [](VPRecipeBase &) { return InstructionCost(1); });
  EXPECT_EQ(Result.FallbackReason, "invalid recipe throughput");
  Result = computeVPlanTPLCDCost(
      Plan, ElementCount::getFixed(4), 0,
      [](VPRecipeBase &) { return InstructionCost(0); },
      [](VPRecipeBase &) { return InstructionCost::getInvalid(); });
  EXPECT_FALSE(Result.FallbackReason.empty());
  Result = computeVPlanTPLCDCost(
      Plan, ElementCount::getFixed(4), InstructionCost::getInvalid(),
      [](VPRecipeBase &) { return InstructionCost(0); },
      [](VPRecipeBase &) { return InstructionCost(1); });
  EXPECT_EQ(Result.FallbackReason, "invalid precomputed throughput");
}
} // namespace
} // namespace llvm
