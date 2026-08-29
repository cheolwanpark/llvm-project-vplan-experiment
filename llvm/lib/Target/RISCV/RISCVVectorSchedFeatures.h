//===-- RISCVVectorSchedFeatures.h - RVV scheduling features ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVVECTORSCHEDFEATURES_H
#define LLVM_LIB_TARGET_RISCV_RISCVVECTORSCHEDFEATURES_H

#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace llvm {
class FunctionPass;
class MachineFunction;
class PassRegistry;
struct RegPressureDelta;
class ScheduleDAGMILive;
class SUnit;

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
