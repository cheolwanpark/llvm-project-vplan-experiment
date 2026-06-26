//===-- RISCVDLMULAnalysis.h - RISC-V DLMUL Analysis -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVDLMULANALYSIS_H
#define LLVM_LIB_TARGET_RISCV_RISCVDLMULANALYSIS_H

namespace llvm {
class FunctionPass;
class PassRegistry;

enum class RISCVDLMULStage {
  PostISelRaw,
  PostRVVOpt,
  PreRAInput,
};

bool isRISCVDLMULAnalysisEnabled();

FunctionPass *createRISCVDLMULCollectorPass(RISCVDLMULStage Stage);
FunctionPass *createRISCVDLMULReportPass();

void initializeRISCVDLMULCollectorPass(PassRegistry &);
void initializeRISCVDLMULReportPass(PassRegistry &);
} // namespace llvm

#endif
