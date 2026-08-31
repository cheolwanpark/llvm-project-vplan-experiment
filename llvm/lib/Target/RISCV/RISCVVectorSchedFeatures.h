//===-- RISCVVectorSchedFeatures.h - RVV scheduling features ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVVECTORSCHEDFEATURES_H
#define LLVM_LIB_TARGET_RISCV_RISCVVECTORSCHEDFEATURES_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace llvm {
class FunctionPass;
class MachineFunction;
class PassRegistry;
struct RegPressureDelta;
class ScheduleDAGMILive;
class SUnit;

namespace RISCVVSPacking {

enum class Status { Feasible, Infeasible, Unknown };

struct Demand {
  ArrayRef<uint32_t> Candidates;
  unsigned Footprint = 0;
  unsigned Multiplicity = 1;
};

struct Budget {
  explicit Budget(uint64_t Limit) : Limit(Limit) {}

  uint64_t Limit = 0;
  uint64_t Used = 0;
  bool Exhausted = false;
};

struct Result {
  Status PackingStatus = Status::Unknown;
  uint64_t SearchStates = 0;
  bool BudgetExhausted = false;
};

struct MaxCopiesResult {
  unsigned LowerBound = 0;
  unsigned UpperBound = 0;

  bool isExact() const { return LowerBound == UpperBound; }
};

Result solve(ArrayRef<Demand> Demands, uint32_t AvailableMask, Budget &Budget);

MaxCopiesResult maxPackableCopies(ArrayRef<Demand> FixedDemands,
                                  ArrayRef<Demand> ComponentDemands,
                                  uint32_t AvailableMask, unsigned UpperBound,
                                  Budget &Budget);

} // namespace RISCVVSPacking

enum class RISCVVectorSchedStage {
  PreSched,
  PostSched,
  PostRVVRA,
  FinalSched,
};

bool isRISCVVectorSchedFeatureEnabled();
bool shouldCollectRISCVVectorSchedFeatures(const MachineFunction &MF);

FunctionPass *createRISCVVectorSchedPrePass();
FunctionPass *createRISCVVectorSchedPostPass();
FunctionPass *createRISCVVectorPostRAPass();
FunctionPass *createRISCVVectorFinalSchedPass();
FunctionPass *createRISCVVectorSchedReportPass();

void initializeRISCVVectorSchedPrePass(PassRegistry &);
void initializeRISCVVectorSchedPostPass(PassRegistry &);
void initializeRISCVVectorPostRAPass(PassRegistry &);
void initializeRISCVVectorFinalSchedPass(PassRegistry &);
void initializeRISCVVectorSchedReportPass(PassRegistry &);

extern char &RISCVVectorSchedPreID;
extern char &RISCVVectorSchedPostID;
extern char &RISCVVectorPostRAID;
extern char &RISCVVectorFinalSchedID;

/// Snapshot the fully-mutated scheduler DAG before scheduling begins. Returns
/// an opaque token used by the following pick/end callbacks.
uint64_t beginRISCVVectorSchedTrace(const ScheduleDAGMILive &DAG,
                                    StringRef Direction, unsigned CriticalPath,
                                    unsigned CyclicCriticalPath);

/// Record one node selected by GenericScheduler. Ready counts describe the
/// candidate queues at selection time, including the selected node.
void recordRISCVVectorSchedPick(uint64_t Token, const SUnit &SU,
                                StringRef Reason, bool IsTop, unsigned TopReady,
                                unsigned BottomReady, unsigned UniqueReady,
                                bool PressureNotLatencyBest,
                                const RegPressureDelta *RPDelta);

void endRISCVVectorSchedTrace(uint64_t Token);

} // namespace llvm

#endif
