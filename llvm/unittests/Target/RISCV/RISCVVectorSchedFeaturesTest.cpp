//===- RISCVVectorSchedFeaturesTest.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVVectorSchedFeatures.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::RISCVVSPacking;

namespace {

static SmallVector<uint32_t, 32> singletonMasks(unsigned Count = 32) {
  SmallVector<uint32_t, 32> Masks;
  for (unsigned I = 0; I != Count; ++I)
    Masks.push_back(uint32_t(1) << I);
  return Masks;
}

TEST(RISCVVectorSchedFeaturesTest, RejectsExcessFootprintWithoutSearch) {
  SmallVector<uint32_t, 32> Masks = singletonMasks();
  Demand D{Masks, 1, 33};
  Budget B(100);
  Result R = solve(D, UINT32_MAX, B);
  EXPECT_EQ(R.PackingStatus, Status::Infeasible);
  EXPECT_EQ(R.SearchStates, 0u);
  EXPECT_FALSE(R.BudgetExhausted);
}

TEST(RISCVVectorSchedFeaturesTest, PacksManyIdenticalLMUL1Demands) {
  SmallVector<uint32_t, 32> Masks = singletonMasks();
  Demand D{Masks, 1, 32};
  Budget B(1000);
  Result R = solve(D, UINT32_MAX, B);
  EXPECT_EQ(R.PackingStatus, Status::Feasible);
  EXPECT_LT(R.SearchStates, 100u);
}

TEST(RISCVVectorSchedFeaturesTest, FixedDemandClampsCopySearch) {
  SmallVector<uint32_t, 32> Masks = singletonMasks();
  SmallVector<Demand, 1> Fixed{{Masks, 1, 1}};
  SmallVector<Demand, 1> Component{{Masks, 1, 1}};
  Budget B(1000);
  MaxCopiesResult R = maxPackableCopies(Fixed, Component, UINT32_MAX, 32, B);
  EXPECT_TRUE(R.isExact());
  EXPECT_EQ(R.LowerBound, 31u);
  EXPECT_LT(B.Used, 1000u);
  EXPECT_FALSE(B.Exhausted);
}

TEST(RISCVVectorSchedFeaturesTest, HonorsNoV0Capacity) {
  SmallVector<uint32_t, 4> Masks = {0x0000ff00, 0x00ff0000, 0xff000000};
  SmallVector<Demand, 1> Component{{Masks, 8, 1}};
  Budget B(100);
  MaxCopiesResult R = maxPackableCopies({}, Component, UINT32_MAX, 4, B);
  EXPECT_TRUE(R.isExact());
  EXPECT_EQ(R.LowerBound, 3u);
}

TEST(RISCVVectorSchedFeaturesTest, ProvesMaskConflictInfeasible) {
  SmallVector<uint32_t, 2> Horizontal = {0x3, 0xc};
  SmallVector<uint32_t, 2> Vertical = {0x5, 0xa};
  SmallVector<Demand, 2> Demands = {{Horizontal, 2, 1}, {Vertical, 2, 1}};
  Budget B(100);
  Result R = solve(Demands, 0xf, B);
  EXPECT_EQ(R.PackingStatus, Status::Infeasible);
  EXPECT_GT(R.SearchStates, 0u);
  EXPECT_FALSE(R.BudgetExhausted);
}

TEST(RISCVVectorSchedFeaturesTest, ReturnsUnknownAtDeterministicBudget) {
  SmallVector<uint32_t, 2> Horizontal = {0x3, 0xc};
  SmallVector<uint32_t, 2> Vertical = {0x5, 0xa};
  SmallVector<Demand, 2> Demands = {{Horizontal, 2, 1}, {Vertical, 2, 1}};
  Budget B(1);
  Result R = solve(Demands, 0xf, B);
  EXPECT_EQ(R.PackingStatus, Status::Unknown);
  EXPECT_EQ(R.SearchStates, 1u);
  EXPECT_TRUE(R.BudgetExhausted);
}

TEST(RISCVVectorSchedFeaturesTest, ReturnsSoundBoundsAtBudget) {
  SmallVector<uint32_t, 4> Masks = {0x0000ff00, 0x00ff0000, 0xff000000};
  SmallVector<Demand, 1> Component{{Masks, 8, 1}};
  Budget B(1);
  MaxCopiesResult R = maxPackableCopies({}, Component, UINT32_MAX, 4, B);
  EXPECT_FALSE(R.isExact());
  EXPECT_EQ(R.LowerBound, 0u);
  EXPECT_EQ(R.UpperBound, 4u);
  EXPECT_TRUE(B.Exhausted);
}

TEST(RISCVVectorSchedFeaturesTest, SearchIsDeterministic) {
  SmallVector<uint32_t, 2> Horizontal = {0x3, 0xc};
  SmallVector<uint32_t, 2> Vertical = {0x5, 0xa};
  SmallVector<Demand, 2> Demands = {{Horizontal, 2, 1}, {Vertical, 2, 1}};
  Budget A(100);
  Budget B(100);
  Result First = solve(Demands, 0xf, A);
  Result Second = solve(Demands, 0xf, B);
  EXPECT_EQ(First.PackingStatus, Second.PackingStatus);
  EXPECT_EQ(First.SearchStates, Second.SearchStates);
}

} // namespace
