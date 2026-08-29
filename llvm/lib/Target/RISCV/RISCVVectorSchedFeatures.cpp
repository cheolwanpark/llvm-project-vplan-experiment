//===-- RISCVVectorSchedFeatures.cpp - RVV scheduling features -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVVectorSchedFeatures.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/SHA256.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "riscv-vector-sched-features"

static cl::opt<std::string> RVVSchedFeatureOutput(
    "riscv-vsched-feature-output",
    cl::desc("Write RISC-V vector scheduling features as JSONL"),
    cl::value_desc("filename"), cl::init(""), cl::Hidden);

static cl::opt<std::string> RVVSchedCandidateID(
    "riscv-vsched-candidate-id",
    cl::desc("Candidate identifier for RISC-V vector scheduling features"),
    cl::value_desc("string"), cl::init(""), cl::Hidden);

static cl::opt<std::string> RVVSchedOnlyFunc(
    "riscv-vsched-only-func",
    cl::desc(
        "Collect RISC-V vector scheduling features only for this function"),
    cl::value_desc("name"), cl::init(""), cl::Hidden);

static cl::opt<unsigned> RVVSchedMinVectorInst(
    "riscv-vsched-min-vector-inst",
    cl::desc("Minimum RVV instruction count required to emit a loop row"),
    cl::init(1), cl::Hidden);

static cl::opt<bool> RVVSchedEmitInstructions(
    "riscv-vsched-emit-instructions",
    cl::desc("Include instruction records in RISC-V scheduling feature JSONL"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RVVSchedEmitEdges(
    "riscv-vsched-emit-edges",
    cl::desc(
        "Include dependency edge records in RISC-V scheduling feature JSONL"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RVVSchedTracePicks(
    "riscv-vsched-trace-picks",
    cl::desc(
        "Include scheduler pick records in RISC-V scheduling feature JSONL"),
    cl::init(false), cl::Hidden);

namespace {

static StringRef stageName(RISCVVectorSchedStage Stage) {
  switch (Stage) {
  case RISCVVectorSchedStage::PreSched:
    return "pre-sched";
  case RISCVVectorSchedStage::PostSched:
    return "post-sched";
  case RISCVVectorSchedStage::PostRVVRA:
    return "post-rvv-ra";
  case RISCVVectorSchedStage::FinalSched:
    return "final-sched";
  }
  llvm_unreachable("unknown RISC-V vector scheduling feature stage");
}

static unsigned stageOrder(StringRef Stage) {
  if (Stage == "pre-sched")
    return 0;
  if (Stage == "post-sched")
    return 1;
  if (Stage == "post-rvv-ra")
    return 2;
  return 3;
}

struct FeatureRow {
  std::string Function;
  std::string LoopUID;
  std::string Stage;
  json::Object Object;
};

struct SchedNodeRecord {
  unsigned Node = 0;
  unsigned Opcode = 0;
  std::string OpcodeName;
  unsigned Depth = 0;
  unsigned Height = 0;
  bool IsVector = false;
};

struct SchedEdgeRecord {
  unsigned From = 0;
  unsigned To = 0;
  std::string Kind;
  unsigned Latency = 0;
  bool IsWeak = false;
  bool IsCluster = false;
};

struct SchedPickRecord {
  unsigned Node = 0;
  std::string Reason;
  bool IsTop = false;
  unsigned TopReady = 0;
  unsigned BottomReady = 0;
  unsigned UniqueReady = 0;
  bool PressureNotLatencyBest = false;
  std::string ExcessSet;
  int ExcessDelta = 0;
  std::string CriticalSet;
  int CriticalDelta = 0;
  std::string CurrentMaxSet;
  int CurrentMaxDelta = 0;
};

struct SchedRegionRecord {
  uint64_t Token = 0;
  const MachineFunction *MF = nullptr;
  const MachineBasicBlock *MBB = nullptr;
  std::string Direction;
  unsigned CriticalPath = 0;
  unsigned CyclicCriticalPath = 0;
  std::vector<SchedNodeRecord> Nodes;
  std::vector<SchedEdgeRecord> Edges;
  std::vector<SchedPickRecord> Picks;
  bool Complete = false;
};

struct LoopOrderState {
  StringMap<SmallVector<std::string, 2>> PreIDsBySemanticKey;
  StringMap<unsigned> PrePositions;
};

struct InstructionIdentity {
  unsigned MBBNumber = 0;
  unsigned RegionOrdinal = 0;
  unsigned InstructionOrdinal = 0;
  std::string SemanticKey;
};

struct OrderComparison {
  unsigned Matched = 0;
  unsigned BaselineCount = 0;
  unsigned CurrentCount = 0;
  double MeanAbsoluteDisplacement = 0.0;
  unsigned MaxAbsoluteDisplacement = 0;
};

struct OrderCapture {
  SmallVector<std::string, 64> StableIDs;
  std::optional<OrderComparison> Comparison;
  unsigned UnmatchedCount = 0;
};

class FeatureDatabase {
  sys::SmartMutex<true> Mutex;
  DenseMap<const Module *, std::vector<FeatureRow>> Rows;
  DenseMap<uint64_t, SchedRegionRecord> Regions;
  DenseMap<const MachineFunction *, SmallVector<uint64_t, 4>> FunctionRegions;
  DenseMap<const MachineFunction *, StringMap<LoopOrderState>> Orders;
  uint64_t NextToken = 1;

public:
  void addRow(const Module *M, FeatureRow Row) {
    sys::SmartScopedLock<true> Lock(Mutex);
    Rows[M].push_back(std::move(Row));
  }

  std::vector<FeatureRow> takeRows(const Module *M) {
    sys::SmartScopedLock<true> Lock(Mutex);
    auto It = Rows.find(M);
    if (It == Rows.end())
      return {};
    std::vector<FeatureRow> Result = std::move(It->second);
    Rows.erase(It);
    return Result;
  }

  uint64_t addRegion(SchedRegionRecord Region) {
    sys::SmartScopedLock<true> Lock(Mutex);
    uint64_t Token = NextToken++;
    Region.Token = Token;
    FunctionRegions[Region.MF].push_back(Token);
    Regions.try_emplace(Token, std::move(Region));
    return Token;
  }

  void addPick(uint64_t Token, SchedPickRecord Pick) {
    sys::SmartScopedLock<true> Lock(Mutex);
    auto It = Regions.find(Token);
    if (It != Regions.end())
      It->second.Picks.push_back(std::move(Pick));
  }

  void finishRegion(uint64_t Token) {
    sys::SmartScopedLock<true> Lock(Mutex);
    auto It = Regions.find(Token);
    if (It != Regions.end())
      It->second.Complete = true;
  }

  std::vector<SchedRegionRecord> getRegions(const MachineFunction *MF) {
    sys::SmartScopedLock<true> Lock(Mutex);
    std::vector<SchedRegionRecord> Result;
    auto It = FunctionRegions.find(MF);
    if (It == FunctionRegions.end())
      return Result;
    for (uint64_t Token : It->second) {
      auto RegionIt = Regions.find(Token);
      if (RegionIt != Regions.end())
        Result.push_back(RegionIt->second);
    }
    return Result;
  }

  void eraseRegions(const MachineFunction *MF) {
    sys::SmartScopedLock<true> Lock(Mutex);
    auto It = FunctionRegions.find(MF);
    if (It == FunctionRegions.end())
      return;
    for (uint64_t Token : It->second)
      Regions.erase(Token);
    FunctionRegions.erase(It);
  }

  OrderCapture captureOrder(const MachineFunction *MF, StringRef LoopUID,
                            RISCVVectorSchedStage Stage,
                            ArrayRef<InstructionIdentity> Identities) {
    sys::SmartScopedLock<true> Lock(Mutex);
    LoopOrderState &State = Orders[MF][LoopUID];
    OrderCapture Capture;
    if (Stage == RISCVVectorSchedStage::PreSched) {
      for (unsigned Position = 0; Position != Identities.size(); ++Position) {
        const InstructionIdentity &Identity = Identities[Position];
        std::string Input =
            (Twine(LoopUID) + "|" + Twine(Identity.MBBNumber) + "|" +
             Twine(Identity.RegionOrdinal) + "|" +
             Twine(Identity.InstructionOrdinal) + "|" + Identity.SemanticKey)
                .str();
        SHA256 Hasher;
        Hasher.update(Input);
        std::string ID = toHex(Hasher.final(), /*LowerCase=*/true);
        State.PreIDsBySemanticKey[Identity.SemanticKey].push_back(ID);
        State.PrePositions[ID] = Position;
        Capture.StableIDs.push_back(std::move(ID));
      }
      return Capture;
    }
    if (Stage != RISCVVectorSchedStage::PostSched)
      return Capture;

    StringMap<unsigned> Occurrences;
    OrderComparison Comparison;
    Comparison.BaselineCount = State.PrePositions.size();
    Comparison.CurrentCount = Identities.size();
    uint64_t TotalDisplacement = 0;
    for (unsigned Position = 0; Position != Identities.size(); ++Position) {
      StringRef Key = Identities[Position].SemanticKey;
      unsigned Occurrence = Occurrences[Key]++;
      auto It = State.PreIDsBySemanticKey.find(Key);
      if (It == State.PreIDsBySemanticKey.end() ||
          Occurrence >= It->second.size()) {
        Capture.StableIDs.push_back(
            (Twine("post_unmatched:") + Key + ":" + Twine(Occurrence)).str());
        ++Capture.UnmatchedCount;
        continue;
      }
      StringRef ID = It->second[Occurrence];
      Capture.StableIDs.push_back(ID.str());
      auto BaselineIt = State.PrePositions.find(ID);
      if (BaselineIt == State.PrePositions.end())
        continue;
      unsigned Displacement = std::max(Position, BaselineIt->second) -
                              std::min(Position, BaselineIt->second);
      ++Comparison.Matched;
      TotalDisplacement += Displacement;
      Comparison.MaxAbsoluteDisplacement =
          std::max(Comparison.MaxAbsoluteDisplacement, Displacement);
    }
    if (Comparison.Matched)
      Comparison.MeanAbsoluteDisplacement =
          static_cast<double>(TotalDisplacement) / Comparison.Matched;
    Capture.Comparison = Comparison;
    return Capture;
  }

  void eraseOrders(const MachineFunction *MF) {
    sys::SmartScopedLock<true> Lock(Mutex);
    Orders.erase(MF);
  }
};

ManagedStatic<FeatureDatabase> FeatureDB;

static bool isRVVInstruction(const MachineInstr &MI) {
  // isRVVSpill also recognizes RVV whole-register loads/stores, even when no
  // frame index is present. Those opcodes are not in RISCVVPseudosTable.
  return RISCVVPseudosTable::getPseudoInfo(MI.getOpcode()) != nullptr ||
         RISCV::isRVVSpill(MI);
}

struct RVVInstrShape {
  bool IsVector = false;
  std::string BaseOpcode;
  std::string LMUL;
  int LMULLog2 = 0;
  unsigned WeightedWorkUnits = 1;
  unsigned SEW = 0;
};

static RVVInstrShape getRVVInstrShape(const MachineInstr &MI,
                                      const TargetInstrInfo &TII) {
  RVVInstrShape Shape;
  const auto *Info = RISCVVPseudosTable::getPseudoInfo(MI.getOpcode());
  if (!Info && !RISCV::isRVVSpill(MI))
    return Shape;
  Shape.IsVector = true;
  if (!Info) {
    Shape.BaseOpcode = TII.getName(MI.getOpcode()).str();
    const MachineRegisterInfo &MRI = MI.getMF()->getRegInfo();
    const TargetRegisterInfo &TRI =
        *MI.getMF()->getSubtarget().getRegisterInfo();
    unsigned Units = 1;
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.getReg())
        continue;
      Register Reg = MO.getReg();
      const TargetRegisterClass *RC = nullptr;
      if (Reg.isVirtual())
        RC = MRI.getRegClassOrNull(Reg);
      else if (Reg.isPhysical() && TRI.isInAllocatableClass(Reg))
        RC = TRI.getMinimalPhysRegClass(Reg);
      if (RC && RISCVRegisterInfo::isRVVRegClass(RC))
        Units = std::max(Units, (1u << static_cast<unsigned>(
                                     RISCVRI::getLMul(RC->TSFlags))) *
                                    RISCVRI::getNF(RC->TSFlags));
    }
    unsigned ExecutionLMUL = Units;
    if (auto Segment = RISCV::isRVVSpillForZvlsseg(MI.getOpcode()))
      ExecutionLMUL = Segment->second;
    Shape.LMUL = "m" + std::to_string(ExecutionLMUL);
    Shape.LMULLog2 = Log2_32(ExecutionLMUL);
    Shape.WeightedWorkUnits = Units;
    return Shape;
  }
  Shape.BaseOpcode = TII.getName(Info->BaseInstr).str();
  RISCVVType::VLMUL Encoded = RISCVII::getLMul(MI.getDesc().TSFlags);
  auto [LMUL, Fractional] = RISCVVType::decodeVLMUL(Encoded);
  Shape.LMUL = (Fractional ? "mf" : "m") + std::to_string(LMUL);
  Shape.LMULLog2 = Log2_32(LMUL) * (Fractional ? -1 : 1);
  Shape.WeightedWorkUnits = Fractional ? 1 : LMUL;
  if (RISCVII::hasSEWOp(MI.getDesc().TSFlags)) {
    const MachineOperand &SEWOp =
        MI.getOperand(RISCVII::getSEWOpNum(MI.getDesc()));
    if (SEWOp.isImm())
      Shape.SEW = SEWOp.getImm() ? 1u << SEWOp.getImm() : 8;
  }
  return Shape;
}

static bool isRealInstruction(const MachineInstr &MI) {
  return !MI.isMetaInstruction();
}

static std::string canonicalOperand(const MachineOperand &MO,
                                    const MachineInstr &MI,
                                    const TargetRegisterInfo &TRI) {
  std::string Result;
  raw_string_ostream OS(Result);
  if (MO.isReg()) {
    Register Reg = MO.getReg();
    OS << "reg:";
    if (!Reg)
      OS << "none";
    else if (Reg.isVirtual())
      OS << 'v' << Reg.virtRegIndex();
    else
      OS << 'p' << TRI.getName(Reg);
    OS << ':' << (MO.isDef() ? "def" : "use") << ":implicit=" << MO.isImplicit()
       << ":undef=" << MO.isUndef() << ":subreg=" << MO.getSubReg();
    if (MO.isTied())
      OS << ":tied=" << MI.findTiedOperandIdx(MO.getOperandNo());
  } else if (MO.isImm()) {
    OS << "imm:" << MO.getImm();
  } else if (MO.isMBB()) {
    OS << "mbb:" << MO.getMBB()->getNumber();
  } else if (MO.isFI()) {
    OS << "fi:" << MO.getIndex();
  } else if (MO.isCPI()) {
    OS << "cpi:" << MO.getIndex();
  } else if (MO.isJTI()) {
    OS << "jti:" << MO.getIndex();
  } else if (MO.isTargetIndex()) {
    OS << "target-index:" << MO.getIndex() << ':' << MO.getOffset();
  } else if (MO.isGlobal()) {
    OS << "global:" << MO.getGlobal()->getName() << ':' << MO.getOffset()
       << ":flags=" << MO.getTargetFlags();
  } else if (MO.isSymbol()) {
    OS << "symbol:" << MO.getSymbolName() << ':' << MO.getOffset()
       << ":flags=" << MO.getTargetFlags();
  } else if (MO.isRegMask()) {
    SHA256 Hasher;
    unsigned Words = MachineOperand::getRegMaskSize(TRI.getNumRegs());
    const uint32_t *Mask = MO.getRegMask();
    for (unsigned I = 0; I != Words; ++I) {
      Hasher.update(utostr(Mask[I]));
      Hasher.update("|");
    }
    OS << "regmask:" << toHex(Hasher.final(), /*LowerCase=*/true);
  } else if (MO.isCImm()) {
    OS << "cimm:" << MO.getCImm()->getValue();
  } else if (MO.isFPImm()) {
    OS << "fpimm:" << MO.getFPImm()->getValueAPF().bitcastToAPInt();
  } else if (MO.isMCSymbol()) {
    OS << "mcsymbol:" << MO.getMCSymbol()->getName();
  } else if (MO.isBlockAddress()) {
    const BlockAddress *Address = MO.getBlockAddress();
    OS << "block-address:" << Address->getFunction()->getName() << ':'
       << Address->getBasicBlock()->getName() << ':' << MO.getOffset()
       << ":flags=" << MO.getTargetFlags();
  } else if (MO.isRegLiveOut()) {
    SHA256 Hasher;
    unsigned Words = MachineOperand::getRegMaskSize(TRI.getNumRegs());
    const uint32_t *Mask = MO.getRegLiveOut();
    for (unsigned I = 0; I != Words; ++I) {
      Hasher.update(utostr(Mask[I]));
      Hasher.update("|");
    }
    OS << "reg-live-out:" << toHex(Hasher.final(), /*LowerCase=*/true);
  } else if (MO.isCFIIndex()) {
    OS << "cfi:" << MO.getCFIIndex();
  } else if (MO.isIntrinsicID()) {
    OS << "intrinsic:" << static_cast<unsigned>(MO.getIntrinsicID());
  } else if (MO.isPredicate()) {
    OS << "predicate:" << MO.getPredicate();
  } else if (MO.isDbgInstrRef()) {
    OS << "instr-ref:" << MO.getInstrRefInstrIndex() << ':'
       << MO.getInstrRefOpIndex();
  } else if (MO.isShuffleMask()) {
    OS << "shuffle:";
    for (int Element : MO.getShuffleMask())
      OS << Element << ',';
  } else if (MO.isLaneMask()) {
    OS << "lane-mask:" << MO.getLaneMask().getAsInteger();
  } else {
    OS << "operand-kind:" << static_cast<unsigned>(MO.getType());
  }
  return OS.str();
}

static std::string canonicalInstruction(const MachineInstr &MI,
                                        const TargetInstrInfo &TII,
                                        const TargetRegisterInfo &TRI) {
  std::string Result = TII.getName(MI.getOpcode()).str();
  for (const MachineOperand &MO : MI.operands()) {
    Result += '|';
    Result += canonicalOperand(MO, MI, TRI);
  }
  return Result;
}

static SmallVector<InstructionIdentity, 64>
getInstructionIdentities(MachineFunction &MF, const MachineLoop &Loop,
                         ArrayRef<MachineInstr *> Instructions) {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  SmallVector<InstructionIdentity, 64> Result;
  Result.reserve(Instructions.size());
  for (MachineBasicBlock &MBB : MF) {
    if (!Loop.contains(&MBB))
      continue;
    unsigned RegionOrdinal = 0;
    unsigned InstructionOrdinal = 0;
    for (MachineInstr &MI : MBB) {
      if (!isRealInstruction(MI))
        continue;
      std::string Canonical = canonicalInstruction(MI, TII, TRI);
      std::string SemanticKey = (Twine(MBB.getNumber()) + "|" +
                                 Twine(RegionOrdinal) + "|" + Canonical)
                                    .str();
      Result.push_back(InstructionIdentity{
          static_cast<unsigned>(MBB.getNumber()), RegionOrdinal,
          InstructionOrdinal++, std::move(SemanticKey)});
      if (MI.isCall() || TII.isSchedulingBoundary(MI, &MBB, MF) ||
          MI.isFakeUse()) {
        ++RegionOrdinal;
        InstructionOrdinal = 0;
      }
    }
  }
  assert(Result.size() == Instructions.size());
  return Result;
}

static std::string hashStrings(ArrayRef<std::string> Values) {
  SHA256 Hasher;
  for (StringRef Value : Values) {
    Hasher.update(Value);
    Hasher.update("\n");
  }
  return toHex(Hasher.final(), /*LowerCase=*/true);
}

static json::Object
mapToJSONObject(const std::map<std::string, unsigned> &Map) {
  json::Object Object;
  for (const auto &[Name, Count] : Map)
    Object[Name] = static_cast<int64_t>(Count);
  return Object;
}

static std::optional<double> percentile(std::vector<unsigned> Values,
                                        double P) {
  if (Values.empty())
    return std::nullopt;
  llvm::sort(Values);
  size_t Index = static_cast<size_t>(std::ceil(P * Values.size()));
  Index = std::clamp<size_t>(Index, 1, Values.size()) - 1;
  return Values[Index];
}

static void setOptional(json::Object &Object, StringRef Name,
                        std::optional<double> Value) {
  if (Value)
    Object[Name.str()] = *Value;
  else
    Object[Name.str()] = nullptr;
}

static unsigned allocationUnits(const TargetRegisterClass *RC) {
  return (1u << static_cast<unsigned>(RISCVRI::getLMul(RC->TSFlags))) *
         RISCVRI::getNF(RC->TSFlags);
}

static bool isVectorRelated(const MachineInstr &MI,
                            const MachineRegisterInfo &MRI,
                            const TargetRegisterInfo &TRI) {
  if (isRVVInstruction(MI))
    return true;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.getReg())
      continue;
    Register Reg = MO.getReg();
    const TargetRegisterClass *RC = nullptr;
    if (Reg.isVirtual())
      RC = MRI.getRegClassOrNull(Reg);
    else if (Reg.isPhysical() && TRI.isInAllocatableClass(Reg))
      RC = TRI.getMinimalPhysRegClass(Reg);
    if (RC && RISCVRegisterInfo::isRVVRegClass(RC))
      return true;
  }
  return false;
}

struct VectorVReg {
  Register Reg;
  const TargetRegisterClass *RC = nullptr;
  unsigned Units = 0;
  unsigned LMUL = 0;
  unsigned NF = 0;
  std::string ClassName;
};

struct TrueEdge {
  const MachineInstr *From = nullptr;
  const MachineInstr *To = nullptr;
  Register Reg;
  bool SameBlockForward = false;
  bool VectorRelated = false;
  unsigned Gap = 0;
  unsigned WeightedGap = 0;
};

static SmallVector<MachineInstr *, 64>
getLoopInstructions(MachineFunction &MF, const MachineLoop &Loop) {
  SmallVector<MachineInstr *, 64> Instructions;
  for (MachineBasicBlock &MBB : MF) {
    if (!Loop.contains(&MBB))
      continue;
    for (MachineInstr &MI : MBB)
      if (isRealInstruction(MI))
        Instructions.push_back(&MI);
  }
  return Instructions;
}

static SmallVector<VectorVReg, 32> getVectorVRegs(const MachineFunction &MF,
                                                  const MachineLoop &Loop,
                                                  LiveIntervals &LIS) {
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<VectorVReg, 32> Result;
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg);
    if (!RC || !RISCVRegisterInfo::isRVVRegClass(RC) || !LIS.hasInterval(Reg))
      continue;
    const LiveInterval &LI = LIS.getInterval(Reg);
    bool Intersects = llvm::any_of(Loop.blocks(), [&](MachineBasicBlock *MBB) {
      return LI.overlaps(LIS.getMBBStartIdx(MBB), LIS.getMBBEndIdx(MBB));
    });
    if (!Intersects)
      continue;
    Result.push_back(
        VectorVReg{Reg, RC, allocationUnits(RC),
                   1u << static_cast<unsigned>(RISCVRI::getLMul(RC->TSFlags)),
                   RISCVRI::getNF(RC->TSFlags),
                   MF.getSubtarget().getRegisterInfo()->getRegClassName(RC)});
  }
  return Result;
}

struct RecurrenceComponent {
  SmallVector<Register, 4> VRegs;
  SmallVector<const TargetRegisterClass *, 4> RegClasses;
  unsigned Units = 0;
};

using CandidateMaskCache =
    DenseMap<const TargetRegisterClass *, SmallVector<uint32_t, 32>>;

static ArrayRef<uint32_t> getCandidateMasks(const MachineFunction &MF,
                                            const TargetRegisterClass *RC,
                                            CandidateMaskCache &Cache) {
  auto [It, Inserted] = Cache.try_emplace(RC);
  if (!Inserted)
    return It->second;
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  BitVector Allocatable = TRI.getAllocatableSet(MF, RC);
  SmallVector<uint32_t, 32> &Masks = It->second;
  for (int Reg = Allocatable.find_first(); Reg >= 0;
       Reg = Allocatable.find_next(Reg)) {
    uint32_t Mask = 0;
    unsigned Bit = 0;
    for (MCPhysReg Base : RISCV::VRRegClass) {
      if (TRI.regsOverlap(static_cast<MCRegister>(Reg), Base))
        Mask |= uint32_t(1) << Bit;
      ++Bit;
    }
    if (Mask && !llvm::is_contained(Masks, Mask))
      Masks.push_back(Mask);
  }
  llvm::sort(Masks);
  return Masks;
}

static bool canExactlyPack(const MachineFunction &MF,
                           ArrayRef<const TargetRegisterClass *> RegClasses,
                           CandidateMaskCache &Cache) {
  struct Demand {
    ArrayRef<uint32_t> Candidates;
    unsigned Footprint = 0;
  };
  SmallVector<Demand, 32> Demands;
  for (const TargetRegisterClass *RC : RegClasses) {
    ArrayRef<uint32_t> Candidates = getCandidateMasks(MF, RC, Cache);
    if (Candidates.empty())
      return false;
    Demands.push_back({Candidates, allocationUnits(RC)});
  }
  llvm::sort(Demands, [](const Demand &A, const Demand &B) {
    if (A.Candidates.size() != B.Candidates.size())
      return A.Candidates.size() < B.Candidates.size();
    return A.Footprint > B.Footprint;
  });
  DenseSet<uint64_t> Failed;
  std::function<bool(unsigned, uint32_t)> Pack = [&](unsigned Index,
                                                     uint32_t Used) {
    if (Index == Demands.size())
      return true;
    uint64_t State = (uint64_t(Index) << 32) | Used;
    if (!Failed.insert(State).second)
      return false;
    for (uint32_t Candidate : Demands[Index].Candidates)
      if (!(Candidate & Used) && Pack(Index + 1, Used | Candidate))
        return true;
    return false;
  };
  return Pack(0, 0);
}

static unsigned
maxExactlyPackableCopies(const MachineFunction &MF,
                         ArrayRef<const TargetRegisterClass *> FixedRegClasses,
                         const RecurrenceComponent &Component,
                         CandidateMaskCache &Cache, unsigned UpperBound) {
  unsigned Low = 0;
  unsigned High = UpperBound;
  while (Low < High) {
    unsigned Copies = Low + (High - Low + 1) / 2;
    SmallVector<const TargetRegisterClass *, 32> Demands(FixedRegClasses);
    for (unsigned I = 0; I != Copies; ++I)
      append_range(Demands, Component.RegClasses);
    if (canExactlyPack(MF, Demands, Cache))
      Low = Copies;
    else
      High = Copies - 1;
  }
  return Low;
}

static SmallVector<TrueEdge, 64>
buildTrueEdges(const MachineLoop &Loop, ArrayRef<MachineInstr *> Instructions,
               LiveIntervals &LIS) {
  const MachineFunction &MF = *Loop.getHeader()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  DenseMap<const MachineInstr *, unsigned> Positions;
  DenseMap<const MachineInstr *, unsigned> BlockPositions;
  DenseMap<const MachineBasicBlock *, unsigned> NextBlockPosition;
  for (unsigned I = 0; I != Instructions.size(); ++I) {
    Positions[Instructions[I]] = I;
    BlockPositions[Instructions[I]] =
        NextBlockPosition[Instructions[I]->getParent()]++;
  }
  DenseSet<std::pair<const MachineInstr *, const MachineInstr *>> Seen;
  SmallVector<TrueEdge, 64> Edges;
  for (MachineInstr *UseMI : Instructions) {
    SlotIndex UseIndex = LIS.getInstructionIndex(*UseMI);
    for (const MachineOperand &MO : UseMI->uses()) {
      if (!MO.isReg() || !MO.getReg().isVirtual() || MO.isUndef())
        continue;
      Register Reg = MO.getReg();
      if (!LIS.hasInterval(Reg))
        continue;
      const LiveInterval &LI = LIS.getInterval(Reg);
      const VNInfo *VNI = LI.getVNInfoAt(UseIndex);
      if (!VNI || VNI->isPHIDef())
        continue;
      MachineInstr *DefMI = LIS.getInstructionFromIndex(VNI->def);
      if (!DefMI || DefMI == UseMI || !Loop.contains(DefMI->getParent()) ||
          !Seen.insert({DefMI, UseMI}).second)
        continue;
      bool SameBlockForward =
          DefMI->getParent() == UseMI->getParent() &&
          BlockPositions.lookup(DefMI) < BlockPositions.lookup(UseMI);
      unsigned Gap = 0;
      unsigned WeightedGap = 0;
      if (SameBlockForward) {
        unsigned From = Positions.lookup(DefMI);
        unsigned To = Positions.lookup(UseMI);
        for (unsigned P = From + 1; P < To; ++P) {
          if (Instructions[P]->getParent() != DefMI->getParent())
            continue;
          ++Gap;
          WeightedGap += getRVVInstrShape(*Instructions[P],
                                          *MF.getSubtarget().getInstrInfo())
                             .WeightedWorkUnits;
        }
      }
      Edges.push_back(TrueEdge{DefMI, UseMI, Reg, SameBlockForward,
                               isVectorRelated(*DefMI, MRI, TRI) ||
                                   isVectorRelated(*UseMI, MRI, TRI),
                               Gap, WeightedGap});
    }
  }
  return Edges;
}

static SmallVector<TrueEdge, 64>
buildPhysicalTrueEdges(const MachineLoop &Loop,
                       ArrayRef<MachineInstr *> Instructions) {
  const MachineFunction &MF = *Loop.getHeader()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DenseMap<const MachineInstr *, unsigned> Positions;
  DenseMap<const MachineInstr *, unsigned> BlockPositions;
  DenseMap<const MachineBasicBlock *, unsigned> NextBlockPosition;
  for (unsigned I = 0; I != Instructions.size(); ++I) {
    Positions[Instructions[I]] = I;
    BlockPositions[Instructions[I]] =
        NextBlockPosition[Instructions[I]->getParent()]++;
  }

  DenseMap<MCRegUnit, const MachineInstr *> LastDef;
  DenseSet<std::pair<const MachineInstr *, const MachineInstr *>> Seen;
  SmallVector<TrueEdge, 64> Edges;
  const MachineBasicBlock *CurrentMBB = nullptr;
  for (MachineInstr *MI : Instructions) {
    if (MI->getParent() != CurrentMBB) {
      CurrentMBB = MI->getParent();
      LastDef.clear();
    }
    for (const MachineOperand &MO : MI->uses()) {
      if (!MO.isReg() || !MO.getReg().isPhysical() || MO.isUndef())
        continue;
      Register Reg = MO.getReg();
      for (MCRegUnitIterator Units(Reg, &TRI); Units.isValid(); ++Units) {
        const MachineInstr *DefMI = LastDef.lookup(*Units);
        if (!DefMI || DefMI == MI || !Seen.insert({DefMI, MI}).second)
          continue;
        unsigned From = Positions.lookup(DefMI);
        unsigned To = Positions.lookup(MI);
        unsigned Gap = 0;
        unsigned WeightedGap = 0;
        for (unsigned P = From + 1; P < To; ++P) {
          ++Gap;
          WeightedGap +=
              getRVVInstrShape(*Instructions[P], TII).WeightedWorkUnits;
        }
        Edges.push_back(TrueEdge{DefMI, MI, Reg, true,
                                 isVectorRelated(*DefMI, MRI, TRI) ||
                                     isVectorRelated(*MI, MRI, TRI),
                                 Gap, WeightedGap});
      }
    }
    for (const MachineOperand &MO : MI->defs()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      for (MCRegUnitIterator Units(MO.getReg(), &TRI); Units.isValid(); ++Units)
        LastDef[*Units] = MI;
    }
  }
  return Edges;
}

static void addInstructionMetrics(json::Object &Row, MachineFunction &MF,
                                  const MachineLoop &Loop,
                                  ArrayRef<MachineInstr *> Instructions,
                                  ArrayRef<std::string> StableIDs) {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  std::map<std::string, unsigned> Opcodes;
  std::map<std::string, unsigned> LMULs;
  std::map<std::string, unsigned> SEWs;
  unsigned Loads = 0, Stores = 0, Copies = 0, Arithmetic = 0;
  int MaxLMULLog2 = std::numeric_limits<int>::min();
  std::string MaxLMUL;
  json::Array InstructionArray;
  unsigned Position = 0;
  for (MachineInstr *MI : Instructions) {
    RVVInstrShape Shape = getRVVInstrShape(*MI, TII);
    if (!Shape.IsVector) {
      ++Position;
      continue;
    }
    ++Opcodes[Shape.BaseOpcode];
    ++LMULs[Shape.LMUL];
    if (Shape.SEW)
      ++SEWs["e" + std::to_string(Shape.SEW)];
    Loads += MI->mayLoad();
    Stores += MI->mayStore();
    Copies += MI->isCopy();
    Arithmetic += !MI->mayLoad() && !MI->mayStore() && !MI->isCopy();
    if (Shape.LMULLog2 > MaxLMULLog2) {
      MaxLMULLog2 = Shape.LMULLog2;
      MaxLMUL = Shape.LMUL;
    }
    if (RVVSchedEmitInstructions) {
      json::Object Record{{"position", static_cast<int64_t>(Position)},
                          {"opcode", TII.getName(MI->getOpcode()).str()},
                          {"base_opcode", Shape.BaseOpcode},
                          {"lmul", Shape.LMUL},
                          {"lmul_log2", static_cast<int64_t>(Shape.LMULLog2)},
                          {"sew", static_cast<int64_t>(Shape.SEW)},
                          {"weighted_work_units",
                           static_cast<int64_t>(Shape.WeightedWorkUnits)},
                          {"distinct_macro_ops", 1}};
      if (!StableIDs.empty())
        Record["stable_mi_id"] = StableIDs[Position];
      InstructionArray.push_back(std::move(Record));
    }
    ++Position;
  }
  Row["vector_opcode_histogram"] = mapToJSONObject(Opcodes);
  Row["instruction_lmul_histogram"] = mapToJSONObject(LMULs);
  Row["sew_histogram"] = mapToJSONObject(SEWs);
  Row["vector_load_count"] = static_cast<int64_t>(Loads);
  Row["vector_store_count"] = static_cast<int64_t>(Stores);
  Row["vector_copy_count"] = static_cast<int64_t>(Copies);
  Row["vector_arithmetic_count"] = static_cast<int64_t>(Arithmetic);
  if (MaxLMULLog2 == std::numeric_limits<int>::min()) {
    Row["max_lmul"] = nullptr;
    Row["max_lmul_log2"] = nullptr;
  } else {
    Row["max_lmul"] = MaxLMUL;
    Row["max_lmul_log2"] = static_cast<int64_t>(MaxLMULLog2);
  }
  if (RVVSchedEmitInstructions)
    Row["instructions"] = std::move(InstructionArray);
}

static bool hasDefinitionInLoop(Register Reg, const MachineLoop &Loop,
                                LiveIntervals &LIS) {
  const LiveInterval &LI = LIS.getInterval(Reg);
  for (const VNInfo *VNI : LI.valnos) {
    if (VNI->isUnused() || VNI->isPHIDef())
      continue;
    const MachineInstr *DefMI = LIS.getInstructionFromIndex(VNI->def);
    if (DefMI && Loop.contains(DefMI->getParent()))
      return true;
  }
  return false;
}

static void addLiveStateMetrics(json::Object &Row, MachineFunction &MF,
                                const MachineLoop &Loop,
                                ArrayRef<MachineInstr *> Instructions,
                                LiveIntervals &LIS) {
  SmallVector<VectorVReg, 32> VRegs = getVectorVRegs(MF, Loop, LIS);
  std::map<std::string, unsigned> ClassHistogram;
  std::map<std::string, unsigned> AllocationHistogram;
  unsigned MaxAllocationUnits = 0;
  for (const VectorVReg &Info : VRegs) {
    ++ClassHistogram[Info.ClassName];
    ++AllocationHistogram[std::to_string(Info.Units)];
    MaxAllocationUnits = std::max(MaxAllocationUnits, Info.Units);
  }
  Row["vector_register_class_histogram"] = mapToJSONObject(ClassHistogram);
  Row["allocation_unit_histogram"] = mapToJSONObject(AllocationHistogram);
  Row["max_allocation_units"] = static_cast<int64_t>(MaxAllocationUnits);
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  BitVector AllocatableVRs = TRI.getAllocatableSet(MF, &RISCV::VRRegClass);
  unsigned AllocatorVectorRegisters = 0;
  for (MCPhysReg Base : RISCV::VRRegClass)
    AllocatorVectorRegisters += AllocatableVRs.test(Base);
  Row["allocator_vector_register_count"] =
      static_cast<int64_t>(AllocatorVectorRegisters);

  SmallVector<SlotIndex, 64> Samples;
  for (MachineBasicBlock *MBB : Loop.blocks())
    Samples.push_back(LIS.getMBBStartIdx(MBB));
  for (MachineInstr *MI : Instructions) {
    SlotIndex Index = LIS.getInstructionIndex(*MI);
    Samples.push_back(Index);
    Samples.push_back(Index.getDeadSlot());
  }

  std::vector<unsigned> UnitSamples;
  std::vector<unsigned> ValueSamples;
  UnitSamples.reserve(Samples.size());
  ValueSamples.reserve(Samples.size());
  uint64_t LiveArea = 0;
  unsigned PeakUnits = 0;
  unsigned PeakValues = 0;
  for (SlotIndex Sample : Samples) {
    unsigned Units = 0;
    unsigned Values = 0;
    for (const VectorVReg &Info : VRegs) {
      if (LIS.getInterval(Info.Reg).liveAt(Sample)) {
        Units += Info.Units;
        ++Values;
      }
    }
    UnitSamples.push_back(Units);
    ValueSamples.push_back(Values);
    LiveArea += Units;
    PeakUnits = std::max(PeakUnits, Units);
    PeakValues = std::max(PeakValues, Values);
  }
  auto Mean = [](ArrayRef<unsigned> Values) -> double {
    if (Values.empty())
      return 0.0;
    return static_cast<double>(
               std::accumulate(Values.begin(), Values.end(), uint64_t(0))) /
           Values.size();
  };
  Row["peak_live_vector_values"] = static_cast<int64_t>(PeakValues);
  Row["avg_live_vector_values"] = Mean(ValueSamples);
  Row["peak_live_vec_units"] = static_cast<int64_t>(PeakUnits);
  Row["avg_live_vec_units"] = Mean(UnitSamples);
  setOptional(Row, "p90_live_vec_units", percentile(UnitSamples, 0.90));
  Row["live_area"] = static_cast<int64_t>(LiveArea);
  Row["live_sample_count"] = static_cast<int64_t>(Samples.size());
  Row["peak_live_ratio"] = static_cast<double>(PeakUnits) / 32.0;

  MachineBasicBlock *Header = Loop.getHeader();
  SlotIndex HeaderStart = LIS.getMBBStartIdx(Header);
  SmallVector<MachineBasicBlock *, 4> Backedges;
  for (MachineBasicBlock *Pred : Header->predecessors())
    if (Loop.contains(Pred))
      Backedges.push_back(Pred);

  unsigned HeaderLiveInUnits = 0;
  unsigned LatchLiveOutUnits = 0;
  unsigned LoopCarriedUnits = 0;
  unsigned LoopInvariantUnits = 0;
  unsigned PeakNonRecurrenceUnits = 0;
  SmallVector<const VectorVReg *, 16> Carried;
  for (const VectorVReg &Info : VRegs) {
    const LiveInterval &LI = LIS.getInterval(Info.Reg);
    bool HeaderLive = LI.liveAt(HeaderStart);
    bool BackedgeLive = llvm::any_of(Backedges, [&](MachineBasicBlock *MBB) {
      return LI.liveAt(LIS.getMBBEndIdx(MBB).getPrevSlot());
    });
    HeaderLiveInUnits += HeaderLive ? Info.Units : 0;
    LatchLiveOutUnits += BackedgeLive ? Info.Units : 0;
    if (HeaderLive && BackedgeLive) {
      if (hasDefinitionInLoop(Info.Reg, Loop, LIS)) {
        LoopCarriedUnits += Info.Units;
        Carried.push_back(&Info);
      } else {
        LoopInvariantUnits += Info.Units;
      }
    }
  }
  for (SlotIndex Sample : Samples) {
    unsigned Units = 0;
    for (const VectorVReg &Info : VRegs) {
      if (!llvm::is_contained(Carried, &Info) &&
          LIS.getInterval(Info.Reg).liveAt(Sample))
        Units += Info.Units;
    }
    PeakNonRecurrenceUnits = std::max(PeakNonRecurrenceUnits, Units);
  }

  // Approximate independent recurrences by components of carried values that
  // occur together in one instruction's def/use relation.
  DenseMap<Register, unsigned> CarriedIndex;
  for (unsigned I = 0; I != Carried.size(); ++I)
    CarriedIndex[Carried[I]->Reg] = I;
  SmallVector<unsigned, 16> Parent(Carried.size());
  std::iota(Parent.begin(), Parent.end(), 0);
  auto Find = [&](unsigned X) {
    unsigned Root = X;
    while (Parent[Root] != Root)
      Root = Parent[Root];
    while (Parent[X] != X) {
      unsigned Next = Parent[X];
      Parent[X] = Root;
      X = Next;
    }
    return Root;
  };
  auto Unite = [&](unsigned A, unsigned B) {
    A = Find(A);
    B = Find(B);
    if (A != B)
      Parent[B] = A;
  };
  for (MachineInstr *MI : Instructions) {
    SmallVector<unsigned, 4> StateRegs;
    for (const MachineOperand &MO : MI->operands())
      if (MO.isReg() && MO.getReg().isVirtual())
        if (auto It = CarriedIndex.find(MO.getReg()); It != CarriedIndex.end())
          StateRegs.push_back(It->second);
    for (unsigned I = 1; I < StateRegs.size(); ++I)
      Unite(StateRegs[0], StateRegs[I]);
  }
  std::map<unsigned, RecurrenceComponent> ComponentMap;
  for (unsigned I = 0; I != Carried.size(); ++I) {
    RecurrenceComponent &Component = ComponentMap[Find(I)];
    Component.VRegs.push_back(Carried[I]->Reg);
    Component.RegClasses.push_back(Carried[I]->RC);
    Component.Units += Carried[I]->Units;
  }
  SmallVector<RecurrenceComponent, 8> Components;
  for (auto &[_, Component] : ComponentMap)
    Components.push_back(std::move(Component));
  unsigned RecurrenceCount = Components.size();
  unsigned MaxStateUnits = 0;
  for (const RecurrenceComponent &Component : Components)
    MaxStateUnits = std::max(MaxStateUnits, Component.Units);

  struct ActiveSample {
    unsigned TotalUnits = 0;
    unsigned NonRecurrenceUnits = 0;
    SmallVector<const TargetRegisterClass *, 16> NonRecurrenceRegClasses;
  };
  SmallVector<ActiveSample, 64> ActiveSamples;
  std::vector<unsigned> FreeUnitsWhileRecurrenceLive;
  unsigned PeakTotalUnitsWhileRecurrenceLive = 0;
  for (SlotIndex Sample : Samples) {
    ActiveSample State;
    bool RecurrenceActive = false;
    for (const VectorVReg &Info : VRegs) {
      if (!LIS.getInterval(Info.Reg).liveAt(Sample))
        continue;
      State.TotalUnits += Info.Units;
      if (llvm::is_contained(Carried, &Info)) {
        RecurrenceActive = true;
      } else {
        State.NonRecurrenceUnits += Info.Units;
        State.NonRecurrenceRegClasses.push_back(Info.RC);
      }
    }
    if (!RecurrenceActive)
      continue;
    PeakTotalUnitsWhileRecurrenceLive =
        std::max(PeakTotalUnitsWhileRecurrenceLive, State.TotalUnits);
    FreeUnitsWhileRecurrenceLive.push_back(
        State.TotalUnits >= 32 ? 0 : 32 - State.TotalUnits);
    ActiveSamples.push_back(std::move(State));
  }

  Row["header_live_in_vec_units"] = static_cast<int64_t>(HeaderLiveInUnits);
  Row["latch_live_out_vec_units"] = static_cast<int64_t>(LatchLiveOutUnits);
  Row["loop_carried_live_units"] = static_cast<int64_t>(LoopCarriedUnits);
  Row["loop_invariant_live_units"] = static_cast<int64_t>(LoopInvariantUnits);
  Row["peak_nonrecurrence_units"] =
      static_cast<int64_t>(PeakNonRecurrenceUnits);
  Row["num_recurrences"] = static_cast<int64_t>(RecurrenceCount);
  if (RecurrenceCount) {
    Row["state_units_per_recurrence"] =
        static_cast<double>(LoopCarriedUnits) / RecurrenceCount;
    Row["max_state_units_per_recurrence"] = static_cast<int64_t>(MaxStateUnits);
    unsigned ResidentArch = std::numeric_limits<unsigned>::max();
    unsigned ResidentAllocator = std::numeric_limits<unsigned>::max();
    CandidateMaskCache MaskCache;
    StringMap<unsigned> PackingCache;
    auto CachedMaxPack =
        [&](ArrayRef<const TargetRegisterClass *> FixedRegClasses,
            const RecurrenceComponent &Component) {
          SmallVector<StringRef, 32> FixedNames;
          for (const TargetRegisterClass *RC : FixedRegClasses)
            FixedNames.push_back(TRI.getRegClassName(RC));
          llvm::sort(FixedNames);
          SmallVector<StringRef, 8> ComponentNames;
          for (const TargetRegisterClass *RC : Component.RegClasses)
            ComponentNames.push_back(TRI.getRegClassName(RC));
          llvm::sort(ComponentNames);
          std::string Key;
          raw_string_ostream OS(Key);
          OS << "fixed:";
          for (StringRef Name : FixedNames)
            OS << Name << ',';
          OS << "|component:";
          for (StringRef Name : ComponentNames)
            OS << Name << ',';
          auto It = PackingCache.find(Key);
          if (It != PackingCache.end())
            return It->second;
          unsigned Result = maxExactlyPackableCopies(
              MF, FixedRegClasses, Component, MaskCache,
              32 / std::max(1u, Component.Units));
          PackingCache[Key] = Result;
          return Result;
        };
    for (const RecurrenceComponent &Component : Components) {
      unsigned ArchBound = 32 / std::max(1u, Component.Units);
      ResidentArch = std::min(ResidentArch, ArchBound);
      ResidentAllocator =
          std::min(ResidentAllocator, CachedMaxPack({}, Component));
    }
    Row["resident_chain_upper_bound_arch"] = static_cast<int64_t>(ResidentArch);
    Row["resident_chain_upper_bound_allocator"] =
        static_cast<int64_t>(ResidentAllocator);

    if (!ActiveSamples.empty()) {
      unsigned OverlapArch = std::numeric_limits<unsigned>::max();
      unsigned OverlapAllocator = std::numeric_limits<unsigned>::max();
      for (const ActiveSample &Sample : ActiveSamples) {
        unsigned Remaining = Sample.NonRecurrenceUnits >= 32
                                 ? 0
                                 : 32 - Sample.NonRecurrenceUnits;
        for (const RecurrenceComponent &Component : Components) {
          unsigned ArchBound = Remaining / std::max(1u, Component.Units);
          OverlapArch = std::min(OverlapArch, ArchBound);
          OverlapAllocator = std::min(
              OverlapAllocator,
              CachedMaxPack(Sample.NonRecurrenceRegClasses, Component));
        }
      }
      Row["min_free_vec_units_while_recurrence_live"] = static_cast<int64_t>(
          *llvm::min_element(FreeUnitsWhileRecurrenceLive));
      setOptional(Row, "p10_free_vec_units_while_recurrence_live",
                  percentile(FreeUnitsWhileRecurrenceLive, 0.10));
      Row["peak_total_units_while_recurrence_live"] =
          static_cast<int64_t>(PeakTotalUnitsWhileRecurrenceLive);
      Row["resident_chain_bound_under_overlap_arch"] =
          static_cast<int64_t>(OverlapArch);
      Row["resident_chain_bound_under_overlap_allocator"] =
          static_cast<int64_t>(OverlapAllocator);
    }
  } else {
    Row["state_units_per_recurrence"] = nullptr;
    Row["max_state_units_per_recurrence"] = nullptr;
    Row["resident_chain_upper_bound_arch"] = nullptr;
    Row["resident_chain_upper_bound_allocator"] = nullptr;
  }
  if (ActiveSamples.empty()) {
    Row["min_free_vec_units_while_recurrence_live"] = nullptr;
    Row["p10_free_vec_units_while_recurrence_live"] = nullptr;
    Row["peak_total_units_while_recurrence_live"] = nullptr;
    Row["resident_chain_bound_under_overlap_arch"] = nullptr;
    Row["resident_chain_bound_under_overlap_allocator"] = nullptr;
  }
  Row["architectural_vector_budget"] = 32;
}

static void addDFGMetrics(json::Object &Row, MachineFunction &MF,
                          const MachineLoop &Loop,
                          ArrayRef<MachineInstr *> Instructions,
                          ArrayRef<TrueEdge> Edges) {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  TargetSchedModel SchedModel;
  SchedModel.init(&MF.getSubtarget());
  DenseMap<const MachineInstr *, unsigned> Position;
  for (unsigned I = 0; I != Instructions.size(); ++I)
    Position[Instructions[I]] = I;
  SmallVector<SmallVector<unsigned, 4>, 64> Succs(Instructions.size());
  SmallVector<SmallVector<unsigned, 4>, 64> Preds(Instructions.size());
  unsigned SameBlockEdges = 0;
  unsigned CrossBlockEdges = 0;
  unsigned VectorEdges = 0;
  std::vector<unsigned> Gaps;
  std::vector<unsigned> WeightedGaps;
  json::Array EdgeArray;
  for (const TrueEdge &Edge : Edges) {
    unsigned From = Position.lookup(Edge.From);
    unsigned To = Position.lookup(Edge.To);
    if (Edge.SameBlockForward) {
      Succs[From].push_back(To);
      Preds[To].push_back(From);
      ++SameBlockEdges;
    } else {
      ++CrossBlockEdges;
    }
    if (Edge.VectorRelated) {
      ++VectorEdges;
      if (Edge.SameBlockForward) {
        Gaps.push_back(Edge.Gap);
        WeightedGaps.push_back(Edge.WeightedGap);
      }
    }
    if (RVVSchedEmitEdges)
      EdgeArray.push_back(json::Object{
          {"from", static_cast<int64_t>(From)},
          {"to", static_cast<int64_t>(To)},
          {"register", static_cast<int64_t>(Edge.Reg.id())},
          {"same_block_forward", Edge.SameBlockForward},
          {"vector_related", Edge.VectorRelated},
          {"instruction_gap", static_cast<int64_t>(Edge.Gap)},
          {"weighted_gap", static_cast<int64_t>(Edge.WeightedGap)}});
  }

  SmallVector<unsigned, 64> Latency(Instructions.size(), 1);
  SmallVector<unsigned, 64> Critical(Instructions.size(), 1);
  SmallVector<unsigned, 64> Level(Instructions.size(), 0);
  for (unsigned I = 0; I != Instructions.size(); ++I) {
    Latency[I] = std::max(1u, SchedModel.computeInstrLatency(Instructions[I]));
    Critical[I] = Latency[I];
    for (unsigned Pred : Preds[I]) {
      Critical[I] = std::max(Critical[I], Critical[Pred] + Latency[I]);
      Level[I] = std::max(Level[I], Level[Pred] + 1);
    }
  }
  unsigned CriticalPath = Critical.empty() ? 0 : *llvm::max_element(Critical);
  unsigned DAGDepth = Level.empty() ? 0 : *llvm::max_element(Level) + 1;
  std::map<unsigned, unsigned> LevelWidths;
  std::map<unsigned, unsigned> VectorLevelWidths;
  for (unsigned I = 0; I != Level.size(); ++I) {
    ++LevelWidths[Level[I]];
    if (getRVVInstrShape(*Instructions[I], TII).IsVector)
      ++VectorLevelWidths[Level[I]];
  }
  auto MaxMapValue = [](const std::map<unsigned, unsigned> &Map) {
    unsigned Max = 0;
    for (const auto &[_, Value] : Map)
      Max = std::max(Max, Value);
    return Max;
  };
  Row["num_sched_nodes"] = static_cast<int64_t>(Instructions.size());
  Row["num_true_edges"] = static_cast<int64_t>(Edges.size());
  Row["num_same_mbb_forward_true_edges"] = static_cast<int64_t>(SameBlockEdges);
  Row["num_cross_mbb_or_backedge_true_edges"] =
      static_cast<int64_t>(CrossBlockEdges);
  Row["num_vector_true_edges"] = static_cast<int64_t>(VectorEdges);
  Row["critical_path_latency_model"] = static_cast<int64_t>(CriticalPath);
  Row["dag_depth"] = static_cast<int64_t>(DAGDepth);
  Row["dag_width"] = static_cast<int64_t>(MaxMapValue(LevelWidths));
  Row["vector_dag_width"] =
      static_cast<int64_t>(MaxMapValue(VectorLevelWidths));
  setOptional(Row, "true_dep_gap_p10", percentile(Gaps, 0.10));
  setOptional(Row, "true_dep_gap_p50", percentile(Gaps, 0.50));
  setOptional(Row, "true_dep_gap_p90", percentile(Gaps, 0.90));
  setOptional(Row, "weighted_dep_gap_p10", percentile(WeightedGaps, 0.10));
  setOptional(Row, "weighted_dep_gap_p50", percentile(WeightedGaps, 0.50));
  setOptional(Row, "weighted_dep_gap_p90", percentile(WeightedGaps, 0.90));
  for (unsigned K : {1u, 2u, 4u, 8u, 16u}) {
    std::string Name = "close_dep_fraction_" + std::to_string(K);
    if (WeightedGaps.empty()) {
      Row[Name] = nullptr;
      continue;
    }
    unsigned Close =
        llvm::count_if(WeightedGaps, [&](unsigned Gap) { return Gap <= K; });
    Row[Name] = static_cast<double>(Close) / WeightedGaps.size();
  }

  DenseSet<std::pair<const MachineInstr *, const MachineInstr *>> Direct;
  for (const TrueEdge &Edge : Edges)
    if (Edge.SameBlockForward)
      Direct.insert({Edge.From, Edge.To});
  std::vector<unsigned> Runs;
  unsigned Run = 1;
  for (unsigned I = 1; I != Instructions.size(); ++I) {
    bool Adjacent =
        Instructions[I - 1]->getParent() == Instructions[I]->getParent() &&
        Direct.contains({Instructions[I - 1], Instructions[I]});
    if (Adjacent) {
      ++Run;
    } else {
      Runs.push_back(Run);
      Run = 1;
    }
  }
  if (!Instructions.empty())
    Runs.push_back(Run);
  setOptional(Row, "dependent_run_p50", percentile(Runs, 0.50));
  setOptional(Row, "dependent_run_p90", percentile(Runs, 0.90));
  Row["dependent_run_max"] =
      static_cast<int64_t>(Runs.empty() ? 0 : *llvm::max_element(Runs));
  if (RVVSchedEmitEdges)
    Row["true_dependency_edges"] = std::move(EdgeArray);
}

static void addLookaheadAndWindowMetrics(json::Object &Row, MachineFunction &MF,
                                         ArrayRef<MachineInstr *> Instructions,
                                         ArrayRef<TrueEdge> Edges) {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  TargetSchedModel SchedModel;
  SchedModel.init(&MF.getSubtarget());
  DenseMap<const MachineInstr *, unsigned> Position;
  for (unsigned I = 0; I != Instructions.size(); ++I)
    Position[Instructions[I]] = I;
  SmallVector<SmallVector<unsigned, 4>, 64> Succs(Instructions.size());
  SmallVector<SmallVector<unsigned, 4>, 64> Preds(Instructions.size());
  for (const TrueEdge &Edge : Edges) {
    if (!Edge.SameBlockForward)
      continue;
    unsigned From = Position.lookup(Edge.From);
    unsigned To = Position.lookup(Edge.To);
    Succs[From].push_back(To);
    Preds[To].push_back(From);
  }

  std::map<unsigned, std::vector<unsigned>> WeightedWorkLookaheads;
  std::map<unsigned, std::vector<unsigned>> MacroOpLookaheads;
  for (unsigned Start = 0; Start != Instructions.size(); ++Start) {
    if (!getRVVInstrShape(*Instructions[Start], TII).IsVector)
      continue;
    SmallVector<bool, 64> Reachable(Instructions.size(), false);
    SmallVector<unsigned, 16> Worklist(Succs[Start].begin(),
                                       Succs[Start].end());
    while (!Worklist.empty()) {
      unsigned Node = Worklist.pop_back_val();
      if (Reachable[Node])
        continue;
      Reachable[Node] = true;
      append_range(Worklist, Succs[Node]);
    }
    unsigned IndependentWeightedWork = 0;
    unsigned IndependentMacroOps = 0;
    unsigned ScannedWeight = 0;
    std::set<unsigned> PendingWeightedWork = {1, 2, 4};
    std::set<unsigned> PendingMacroOps = {1, 2, 4};
    for (unsigned I = Start + 1;
         I != Instructions.size() &&
         (!PendingWeightedWork.empty() || !PendingMacroOps.empty());
         ++I) {
      if (Instructions[I]->getParent() != Instructions[Start]->getParent())
        break;
      RVVInstrShape Shape = getRVVInstrShape(*Instructions[I], TII);
      ScannedWeight += Shape.WeightedWorkUnits;
      if (Shape.IsVector && !Reachable[I]) {
        IndependentWeightedWork += Shape.WeightedWorkUnits;
        ++IndependentMacroOps;
      }
      for (auto It = PendingWeightedWork.begin();
           It != PendingWeightedWork.end();) {
        if (IndependentWeightedWork >= *It) {
          WeightedWorkLookaheads[*It].push_back(ScannedWeight);
          It = PendingWeightedWork.erase(It);
        } else {
          ++It;
        }
      }
      for (auto It = PendingMacroOps.begin(); It != PendingMacroOps.end();) {
        if (IndependentMacroOps >= *It) {
          MacroOpLookaheads[*It].push_back(ScannedWeight);
          It = PendingMacroOps.erase(It);
        } else {
          ++It;
        }
      }
    }
  }
  for (unsigned Threshold : {1u, 2u, 4u}) {
    auto AddLookahead = [&](StringRef Kind,
                            const std::vector<unsigned> &Values) {
      std::string Base =
          (Twine("lookahead_to_independent_") + Kind + "_" + Twine(Threshold))
              .str();
      setOptional(Row, Base + "_p50", percentile(Values, 0.50));
      setOptional(Row, Base + "_p90", percentile(Values, 0.90));
      Row[Base + "_max"] =
          Values.empty()
              ? json::Value(nullptr)
              : json::Value(static_cast<int64_t>(*llvm::max_element(Values)));
    };
    AddLookahead("weighted_work", WeightedWorkLookaheads[Threshold]);
    AddLookahead("macroop", MacroOpLookaheads[Threshold]);
  }

  for (unsigned Window : {8u, 16u, 32u, 64u}) {
    std::vector<double> MacroILPs;
    std::vector<double> WeightedWorkILPs;
    std::vector<unsigned> ReadyWeightedWork;
    std::vector<unsigned> ReadyMacroOps;
    std::vector<unsigned> ComponentCounts;
    std::vector<unsigned> CriticalPaths;
    for (unsigned Start = 0; Start != Instructions.size(); ++Start) {
      if (!getRVVInstrShape(*Instructions[Start], TII).IsVector)
        continue;
      unsigned End = Start;
      unsigned StaticWeight = 0;
      while (End != Instructions.size() &&
             Instructions[End]->getParent() ==
                 Instructions[Start]->getParent() &&
             StaticWeight < Window) {
        StaticWeight +=
            getRVVInstrShape(*Instructions[End], TII).WeightedWorkUnits;
        ++End;
      }
      if (End == Start)
        continue;
      SmallVector<unsigned, 64> Critical(End - Start, 1);
      SmallVector<unsigned, 64> Parent(End - Start);
      std::iota(Parent.begin(), Parent.end(), 0);
      auto Find = [&](unsigned X) {
        unsigned Root = X;
        while (Parent[Root] != Root)
          Root = Parent[Root];
        while (Parent[X] != X) {
          unsigned Next = Parent[X];
          Parent[X] = Root;
          X = Next;
        }
        return Root;
      };
      auto Unite = [&](unsigned A, unsigned B) {
        A = Find(A);
        B = Find(B);
        if (A != B)
          Parent[B] = A;
      };
      unsigned WindowReadyWeightedWork = 0;
      unsigned WindowReadyMacroOps = 0;
      unsigned VectorWeightedWork = 0;
      unsigned VectorMacroOps = 0;
      for (unsigned I = Start; I != End; ++I) {
        RVVInstrShape Shape = getRVVInstrShape(*Instructions[I], TII);
        unsigned Latency =
            std::max(1u, SchedModel.computeInstrLatency(Instructions[I]));
        Critical[I - Start] = Latency;
        bool HasWindowPred = false;
        for (unsigned Pred : Preds[I]) {
          if (Pred < Start || Pred >= End)
            continue;
          HasWindowPred = true;
          Critical[I - Start] =
              std::max(Critical[I - Start], Critical[Pred - Start] + Latency);
          Unite(I - Start, Pred - Start);
        }
        if (Shape.IsVector) {
          VectorWeightedWork += Shape.WeightedWorkUnits;
          ++VectorMacroOps;
          if (!HasWindowPred) {
            WindowReadyWeightedWork += Shape.WeightedWorkUnits;
            ++WindowReadyMacroOps;
          }
        }
      }
      std::set<unsigned> VectorComponents;
      for (unsigned I = Start; I != End; ++I)
        if (getRVVInstrShape(*Instructions[I], TII).IsVector)
          VectorComponents.insert(Find(I - Start));
      unsigned CP = *llvm::max_element(Critical);
      ReadyWeightedWork.push_back(WindowReadyWeightedWork);
      ReadyMacroOps.push_back(WindowReadyMacroOps);
      ComponentCounts.push_back(VectorComponents.size());
      CriticalPaths.push_back(CP);
      MacroILPs.push_back(static_cast<double>(VectorMacroOps) /
                          std::max(1u, CP));
      WeightedWorkILPs.push_back(static_cast<double>(VectorWeightedWork) /
                                 std::max(1u, CP));
    }
    auto MeanUnsigned = [](ArrayRef<unsigned> Values) -> std::optional<double> {
      if (Values.empty())
        return std::nullopt;
      return static_cast<double>(
                 std::accumulate(Values.begin(), Values.end(), uint64_t(0))) /
             Values.size();
    };
    auto MeanDouble = [](ArrayRef<double> Values) -> std::optional<double> {
      if (Values.empty())
        return std::nullopt;
      return std::accumulate(Values.begin(), Values.end(), 0.0) / Values.size();
    };
    std::string Suffix = std::to_string(Window);
    setOptional(Row, "window_ready_weighted_work_units_" + Suffix,
                MeanUnsigned(ReadyWeightedWork));
    setOptional(Row, "window_ready_macroop_count_" + Suffix,
                MeanUnsigned(ReadyMacroOps));
    setOptional(Row, "window_distinct_dependency_components_" + Suffix,
                MeanUnsigned(ComponentCounts));
    setOptional(Row, "window_critical_path_" + Suffix,
                MeanUnsigned(CriticalPaths));
    setOptional(Row, "window_macro_ilp_" + Suffix, MeanDouble(MacroILPs));
    setOptional(Row, "window_weighted_work_ilp_" + Suffix,
                MeanDouble(WeightedWorkILPs));
  }
}

static std::string
instructionSequenceHash(ArrayRef<MachineInstr *> Instructions,
                        const TargetInstrInfo &TII) {
  SHA256 Hasher;
  for (MachineInstr *MI : Instructions) {
    RVVInstrShape Shape = getRVVInstrShape(*MI, TII);
    Hasher.update(Shape.IsVector ? Shape.BaseOpcode
                                 : TII.getName(MI->getOpcode()));
    Hasher.update("\n");
  }
  return toHex(Hasher.final(), /*LowerCase=*/true);
}

static std::string
stageInstructionSequenceHash(ArrayRef<MachineInstr *> Instructions,
                             const TargetInstrInfo &TII,
                             const TargetRegisterInfo &TRI) {
  SHA256 Hasher;
  for (const MachineInstr *MI : Instructions) {
    Hasher.update(canonicalInstruction(*MI, TII, TRI));
    Hasher.update("\n");
  }
  return toHex(Hasher.final(), /*LowerCase=*/true);
}

static bool isStandardVSET(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case RISCV::PseudoVSETVLI:
  case RISCV::PseudoVSETIVLI:
  case RISCV::PseudoVSETVLIX0:
  case RISCV::PseudoVSETVLIX0X0:
    return true;
  default:
    return false;
  }
}

static void addPhysicalMetrics(json::Object &Row, MachineFunction &MF,
                               const MachineLoop &Loop,
                               ArrayRef<MachineInstr *> Instructions) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const TargetRegisterInfo &TRI = *ST.getRegisterInfo();
  std::map<std::string, unsigned> GroupHistogram;
  std::set<MCPhysReg> BaseRegisters;
  unsigned MaxGroup = 0;
  unsigned SpillCount = 0;
  unsigned ReloadCount = 0;
  uint64_t SpillVLenBUnits = 0;
  uint64_t ReloadVLenBUnits = 0;
  unsigned VectorCopies = 0;
  unsigned VSETVLI = 0;
  unsigned VSETIVLI = 0;
  unsigned VTypeTransitions = 0;
  unsigned SEWTransitions = 0;
  unsigned LMULTransitions = 0;
  DenseMap<const MachineBasicBlock *, std::optional<unsigned>> PreviousVType;

  for (MachineInstr *MI : Instructions) {
    unsigned InstMaxGroup = 0;
    bool HasFrameIndex = llvm::any_of(
        MI->operands(), [](const MachineOperand &MO) { return MO.isFI(); });
    for (const MachineOperand &MO : MI->operands()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      Register Reg = MO.getReg();
      if (!TRI.isInAllocatableClass(Reg))
        continue;
      const TargetRegisterClass *RC = TRI.getMinimalPhysRegClass(Reg);
      if (!RC || !RISCVRegisterInfo::isRVVRegClass(RC))
        continue;
      unsigned Units = allocationUnits(RC);
      InstMaxGroup = std::max(InstMaxGroup, Units);
      MaxGroup = std::max(MaxGroup, Units);
      ++GroupHistogram[std::to_string(Units)];
      for (MCPhysReg Base : RISCV::VRRegClass)
        if (TRI.regsOverlap(Reg, Base))
          BaseRegisters.insert(Base);
    }
    if (MI->isCopy() && InstMaxGroup)
      ++VectorCopies;
    if (HasFrameIndex && RISCV::isRVVSpill(*MI)) {
      unsigned Units = std::max(1u, InstMaxGroup);
      if (MI->mayLoad()) {
        ++ReloadCount;
        ReloadVLenBUnits += Units;
      }
      if (MI->mayStore()) {
        ++SpillCount;
        SpillVLenBUnits += Units;
      }
    }
    if (!isStandardVSET(*MI))
      continue;
    VSETVLI += MI->getOpcode() != RISCV::PseudoVSETIVLI;
    VSETIVLI += MI->getOpcode() == RISCV::PseudoVSETIVLI;
    if (MI->getNumExplicitOperands() < 3 || !MI->getOperand(2).isImm())
      continue;
    unsigned VType = MI->getOperand(2).getImm();
    std::optional<unsigned> &Previous = PreviousVType[MI->getParent()];
    if (Previous && *Previous != VType) {
      ++VTypeTransitions;
      SEWTransitions +=
          RISCVVType::getSEW(*Previous) != RISCVVType::getSEW(VType);
      LMULTransitions +=
          RISCVVType::getVLMUL(*Previous) != RISCVVType::getVLMUL(VType);
    }
    Previous = VType;
  }
  Row["physical_vector_operand_group_histogram"] =
      mapToJSONObject(GroupHistogram);
  Row["distinct_physical_vector_registers"] =
      static_cast<int64_t>(BaseRegisters.size());
  Row["max_physical_group_size"] = static_cast<int64_t>(MaxGroup);
  Row["vector_spill_count"] = static_cast<int64_t>(SpillCount);
  Row["vector_reload_count"] = static_cast<int64_t>(ReloadCount);
  Row["vector_spill_vlenb_units"] = static_cast<int64_t>(SpillVLenBUnits);
  Row["vector_reload_vlenb_units"] = static_cast<int64_t>(ReloadVLenBUnits);
  Row["vector_spill_bytes_min"] =
      static_cast<int64_t>(SpillVLenBUnits * (ST.getRealMinVLen() / 8));
  Row["vector_spill_bytes_max"] =
      static_cast<int64_t>(SpillVLenBUnits * (ST.getRealMaxVLen() / 8));
  Row["vector_reload_bytes_min"] =
      static_cast<int64_t>(ReloadVLenBUnits * (ST.getRealMinVLen() / 8));
  Row["vector_reload_bytes_max"] =
      static_cast<int64_t>(ReloadVLenBUnits * (ST.getRealMaxVLen() / 8));
  if (std::optional<unsigned> VLen = ST.getRealVLen())
    Row["vector_spill_bytes_exact"] =
        static_cast<int64_t>(SpillVLenBUnits * (*VLen / 8));
  else
    Row["vector_spill_bytes_exact"] = nullptr;
  if (std::optional<unsigned> VLen = ST.getRealVLen())
    Row["vector_reload_bytes_exact"] =
        static_cast<int64_t>(ReloadVLenBUnits * (*VLen / 8));
  else
    Row["vector_reload_bytes_exact"] = nullptr;
  Row["physical_vector_copy_count"] = static_cast<int64_t>(VectorCopies);
  Row["vsetvli_count"] = static_cast<int64_t>(VSETVLI);
  Row["vsetivli_count"] = static_cast<int64_t>(VSETIVLI);
  Row["vtype_transition_count"] = static_cast<int64_t>(VTypeTransitions);
  Row["sew_transition_count"] = static_cast<int64_t>(SEWTransitions);
  Row["lmul_transition_count"] = static_cast<int64_t>(LMULTransitions);

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned ScalableObjectCount = 0;
  uint64_t ScalableObjectMinBytes = 0;
  for (int FI = MFI.getObjectIndexBegin(); FI != MFI.getObjectIndexEnd();
       ++FI) {
    if (MFI.isDeadObjectIndex(FI) ||
        MFI.getStackID(FI) != TargetStackID::ScalableVector)
      continue;
    ++ScalableObjectCount;
    ScalableObjectMinBytes += MFI.getObjectSize(FI);
  }
  Row["stack_size_bytes"] = static_cast<int64_t>(MFI.getStackSize());
  Row["scalable_stack_object_count"] =
      static_cast<int64_t>(ScalableObjectCount);
  Row["scalable_stack_object_min_bytes"] =
      static_cast<int64_t>(ScalableObjectMinBytes);
}

static void addOrderComparison(json::Object &Row, StringRef Prefix,
                               const OrderComparison &Comparison) {
  Row[(Prefix + "_matched_instruction_count").str()] =
      static_cast<int64_t>(Comparison.Matched);
  Row[(Prefix + "_baseline_instruction_count").str()] =
      static_cast<int64_t>(Comparison.BaselineCount);
  Row[(Prefix + "_current_instruction_count").str()] =
      static_cast<int64_t>(Comparison.CurrentCount);
  Row[(Prefix + "_matched_instruction_fraction").str()] =
      Comparison.BaselineCount
          ? static_cast<double>(Comparison.Matched) / Comparison.BaselineCount
          : 0.0;
  if (Comparison.Matched) {
    Row[(Prefix + "_mean_absolute_displacement").str()] =
        Comparison.MeanAbsoluteDisplacement;
    Row[(Prefix + "_max_absolute_displacement").str()] =
        static_cast<int64_t>(Comparison.MaxAbsoluteDisplacement);
  } else {
    Row[(Prefix + "_mean_absolute_displacement").str()] = nullptr;
    Row[(Prefix + "_max_absolute_displacement").str()] = nullptr;
  }
}

static unsigned countRVVInstructions(const MachineLoop &Loop) {
  unsigned Count = 0;
  for (MachineBasicBlock *MBB : Loop.blocks())
    for (const MachineInstr &MI : *MBB)
      Count += isRVVInstruction(MI);
  return Count;
}

static std::string sourceFile(const MachineFunction &MF) {
  if (const DISubprogram *SP = MF.getFunction().getSubprogram())
    return SP->getFilename().str();
  return "";
}

static std::pair<unsigned, unsigned> sourceLineRange(const MachineLoop &Loop) {
  unsigned MinLine = std::numeric_limits<unsigned>::max();
  unsigned MaxLine = 0;
  for (MachineBasicBlock *MBB : Loop.blocks()) {
    for (const MachineInstr &MI : *MBB) {
      if (DebugLoc DL = MI.getDebugLoc()) {
        MinLine = std::min(MinLine, DL.getLine());
        MaxLine = std::max(MaxLine, DL.getLine());
      }
    }
  }
  if (MinLine == std::numeric_limits<unsigned>::max())
    return {0, 0};
  return {MinLine, MaxLine};
}

static std::string makeLoopUID(const MachineFunction &MF,
                               const MachineLoop &Loop, unsigned Ordinal,
                               unsigned MinLine, unsigned MaxLine) {
  std::string Identity =
      (Twine(MF.getName()) + "|" + sourceFile(MF) + "|" + Twine(MinLine) + "|" +
       Twine(MaxLine) + "|" + Twine(Loop.getLoopDepth()) + "|" + Twine(Ordinal))
          .str();
  SHA256 Hash;
  Hash.update(Identity);
  return toHex(Hash.final(), /*LowerCase=*/true);
}

static void collectInnermostLoops(const MachineLoopInfo &MLI,
                                  SmallVectorImpl<MachineLoop *> &Loops) {
  SmallVector<MachineLoop *, 8> Worklist;
  append_range(Worklist, MLI);
  while (!Worklist.empty()) {
    MachineLoop *Loop = Worklist.pop_back_val();
    if (Loop->getSubLoops().empty()) {
      Loops.push_back(Loop);
      continue;
    }
    append_range(Worklist, Loop->getSubLoops());
  }
  llvm::sort(Loops, [](const MachineLoop *L, const MachineLoop *R) {
    return L->getHeader()->getNumber() < R->getHeader()->getNumber();
  });
}

static json::Object buildBaseRow(const MachineFunction &MF,
                                 const MachineLoop &Loop,
                                 RISCVVectorSchedStage Stage, unsigned Ordinal,
                                 unsigned VectorInstCount, StringRef LoopUID) {
  auto [MinLine, MaxLine] = sourceLineRange(Loop);
  json::Object Row;
  Row["schema_version"] = 2;
  Row["candidate_id"] = RVVSchedCandidateID;
  Row["module"] = MF.getFunction().getParent()->getModuleIdentifier();
  Row["function"] = MF.getName();
  Row["stage"] = stageName(Stage);
  Row["loop_uid"] = LoopUID.str();
  Row["loop_depth"] = static_cast<int64_t>(Loop.getLoopDepth());
  Row["loop_ordinal"] = static_cast<int64_t>(Ordinal);
  Row["header_mbb"] = static_cast<int64_t>(Loop.getHeader()->getNumber());
  Row["source_file"] = sourceFile(MF);
  if (MinLine) {
    Row["source_line_min"] = static_cast<int64_t>(MinLine);
    Row["source_line_max"] = static_cast<int64_t>(MaxLine);
  } else {
    Row["source_line_min"] = nullptr;
    Row["source_line_max"] = nullptr;
  }
  Row["vector_instruction_count"] = static_cast<int64_t>(VectorInstCount);
  return Row;
}

static void addSchedulerSummary(json::Object &Row, const MachineLoop &Loop,
                                ArrayRef<SchedRegionRecord> Regions) {
  unsigned RegionCount = 0;
  unsigned PickCount = 0;
  unsigned TopPicks = 0;
  uint64_t ReadySum = 0;
  unsigned ReadyMax = 0;
  unsigned PressureNotLatencyBest = 0;
  unsigned CriticalPath = 0;
  unsigned CyclicCriticalPath = 0;
  unsigned SchedulerNodeCount = 0;
  unsigned SchedulerEdgeCount = 0;
  StringMap<unsigned> Reasons;
  StringMap<unsigned> EdgeKinds;
  std::set<std::string> Directions;
  json::Array PickArray;
  json::Array EdgeArray;
  json::Array NodeArray;

  for (const SchedRegionRecord &Region : Regions) {
    if (!Region.Complete || !Loop.contains(Region.MBB))
      continue;
    ++RegionCount;
    Directions.insert(Region.Direction);
    SchedulerNodeCount += Region.Nodes.size();
    SchedulerEdgeCount += Region.Edges.size();
    CriticalPath = std::max(CriticalPath, Region.CriticalPath);
    CyclicCriticalPath =
        std::max(CyclicCriticalPath, Region.CyclicCriticalPath);
    for (const SchedPickRecord &Pick : Region.Picks) {
      ++PickCount;
      TopPicks += Pick.IsTop;
      ReadySum += Pick.UniqueReady;
      ReadyMax = std::max(ReadyMax, Pick.UniqueReady);
      PressureNotLatencyBest += Pick.PressureNotLatencyBest;
      ++Reasons[Pick.Reason];
      if (RVVSchedTracePicks) {
        json::Object P{
            {"node", static_cast<int64_t>(Pick.Node)},
            {"reason", Pick.Reason},
            {"is_top", Pick.IsTop},
            {"top_ready", Pick.TopReady},
            {"bottom_ready", Pick.BottomReady},
            {"unique_ready", static_cast<int64_t>(Pick.UniqueReady)},
            {"pressure_not_latency_best", Pick.PressureNotLatencyBest}};
        if (!Pick.ExcessSet.empty())
          P["reg_excess_delta"] = json::Object{{"set", Pick.ExcessSet},
                                               {"delta", Pick.ExcessDelta}};
        if (!Pick.CriticalSet.empty())
          P["reg_critical_delta"] = json::Object{{"set", Pick.CriticalSet},
                                                 {"delta", Pick.CriticalDelta}};
        if (!Pick.CurrentMaxSet.empty())
          P["reg_current_max_delta"] = json::Object{
              {"set", Pick.CurrentMaxSet}, {"delta", Pick.CurrentMaxDelta}};
        PickArray.push_back(std::move(P));
      }
    }
    for (const SchedEdgeRecord &Edge : Region.Edges)
      ++EdgeKinds[Edge.Kind];
    if (RVVSchedEmitEdges) {
      for (const SchedEdgeRecord &Edge : Region.Edges)
        EdgeArray.push_back(
            json::Object{{"from", static_cast<int64_t>(Edge.From)},
                         {"to", static_cast<int64_t>(Edge.To)},
                         {"kind", Edge.Kind},
                         {"latency", static_cast<int64_t>(Edge.Latency)},
                         {"weak", Edge.IsWeak},
                         {"cluster", Edge.IsCluster}});
    }
    if (RVVSchedEmitInstructions)
      for (const SchedNodeRecord &Node : Region.Nodes)
        NodeArray.push_back(
            json::Object{{"node", static_cast<int64_t>(Node.Node)},
                         {"opcode", Node.OpcodeName},
                         {"depth", static_cast<int64_t>(Node.Depth)},
                         {"height", static_cast<int64_t>(Node.Height)},
                         {"vector", Node.IsVector}});
  }

  Row["scheduler_executed"] = RegionCount != 0;
  Row["scheduler_region_count"] = static_cast<int64_t>(RegionCount);
  Row["scheduler_node_count"] = static_cast<int64_t>(SchedulerNodeCount);
  Row["scheduler_edge_count"] = static_cast<int64_t>(SchedulerEdgeCount);
  Row["scheduler_pick_count"] = static_cast<int64_t>(PickCount);
  Row["scheduler_ready_queue_max"] = static_cast<int64_t>(ReadyMax);
  Row["scheduler_pressure_not_latency_best_count"] =
      static_cast<int64_t>(PressureNotLatencyBest);
  if (PickCount) {
    Row["scheduler_ready_queue_avg"] =
        static_cast<double>(ReadySum) / PickCount;
    Row["scheduler_top_pick_fraction"] =
        static_cast<double>(TopPicks) / PickCount;
  } else {
    Row["scheduler_ready_queue_avg"] = nullptr;
    Row["scheduler_top_pick_fraction"] = nullptr;
  }
  Row["scheduler_critical_path"] = static_cast<int64_t>(CriticalPath);
  Row["scheduler_cyclic_critical_path"] =
      static_cast<int64_t>(CyclicCriticalPath);
  if (Directions.size() == 1)
    Row["scheduler_direction"] = *Directions.begin();
  else if (Directions.empty())
    Row["scheduler_direction"] = nullptr;
  else
    Row["scheduler_direction"] = "mixed";
  json::Object ReasonObject;
  SmallVector<StringRef, 16> ReasonNames;
  for (const auto &Entry : Reasons)
    ReasonNames.push_back(Entry.getKey());
  llvm::sort(ReasonNames);
  for (StringRef Reason : ReasonNames)
    ReasonObject[Reason.str()] = static_cast<int64_t>(Reasons[Reason]);
  Row["scheduler_reason_counts"] = std::move(ReasonObject);
  json::Object EdgeKindObject;
  SmallVector<StringRef, 8> EdgeKindNames;
  for (const auto &Entry : EdgeKinds)
    EdgeKindNames.push_back(Entry.getKey());
  llvm::sort(EdgeKindNames);
  for (StringRef Kind : EdgeKindNames)
    EdgeKindObject[Kind.str()] = static_cast<int64_t>(EdgeKinds[Kind]);
  Row["scheduler_edge_kind_counts"] = std::move(EdgeKindObject);
  if (RVVSchedTracePicks)
    Row["scheduler_picks"] = std::move(PickArray);
  if (RVVSchedEmitEdges)
    Row["scheduler_edges"] = std::move(EdgeArray);
  if (RVVSchedEmitInstructions)
    Row["scheduler_nodes"] = std::move(NodeArray);
}

static bool collectRows(MachineFunction &MF, RISCVVectorSchedStage Stage,
                        MachineLoopInfo &MLI, LiveIntervals *LIS) {
  if (!shouldCollectRISCVVectorSchedFeatures(MF))
    return false;

  SmallVector<MachineLoop *, 8> Loops;
  collectInnermostLoops(MLI, Loops);
  std::vector<SchedRegionRecord> Regions;
  if (Stage == RISCVVectorSchedStage::PostSched)
    Regions = FeatureDB->getRegions(&MF);

  unsigned Ordinal = 0;
  for (MachineLoop *Loop : Loops) {
    unsigned VectorInstCount = countRVVInstructions(*Loop);
    if (VectorInstCount < RVVSchedMinVectorInst) {
      ++Ordinal;
      continue;
    }
    auto [MinLine, MaxLine] = sourceLineRange(*Loop);
    std::string LoopUID = makeLoopUID(MF, *Loop, Ordinal, MinLine, MaxLine);
    json::Object Row =
        buildBaseRow(MF, *Loop, Stage, Ordinal, VectorInstCount, LoopUID);
    SmallVector<MachineInstr *, 64> Instructions =
        getLoopInstructions(MF, *Loop);
    OrderCapture Capture;
    if (Stage == RISCVVectorSchedStage::PreSched ||
        Stage == RISCVVectorSchedStage::PostSched) {
      SmallVector<InstructionIdentity, 64> Identities =
          getInstructionIdentities(MF, *Loop, Instructions);
      Capture = FeatureDB->captureOrder(&MF, LoopUID, Stage, Identities);
      json::Array StableIDArray;
      for (const std::string &ID : Capture.StableIDs)
        StableIDArray.push_back(ID);
      Row["stable_mi_ids"] = std::move(StableIDArray);
      Row["stable_mi_id_sequence_hash"] = hashStrings(Capture.StableIDs);
      Row["stable_mi_unmatched_count"] =
          static_cast<int64_t>(Capture.UnmatchedCount);
      if (Capture.Comparison)
        addOrderComparison(Row, "pre_to_post", *Capture.Comparison);
    }
    addInstructionMetrics(Row, MF, *Loop, Instructions, Capture.StableIDs);
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
    Row["vector_opcode_sequence_hash"] =
        instructionSequenceHash(Instructions, TII);
    Row["stage_instruction_sequence_hash"] =
        stageInstructionSequenceHash(Instructions, TII, TRI);
    if (LIS) {
      addLiveStateMetrics(Row, MF, *Loop, Instructions, *LIS);
      SmallVector<TrueEdge, 64> Edges =
          buildTrueEdges(*Loop, Instructions, *LIS);
      addDFGMetrics(Row, MF, *Loop, Instructions, Edges);
      addLookaheadAndWindowMetrics(Row, MF, Instructions, Edges);
    }
    if (Stage == RISCVVectorSchedStage::PostRVVRA ||
        Stage == RISCVVectorSchedStage::FinalSched) {
      addPhysicalMetrics(Row, MF, *Loop, Instructions);
      SmallVector<TrueEdge, 64> PhysicalEdges =
          buildPhysicalTrueEdges(*Loop, Instructions);
      addDFGMetrics(Row, MF, *Loop, Instructions, PhysicalEdges);
      addLookaheadAndWindowMetrics(Row, MF, Instructions, PhysicalEdges);
    }
    if (Stage == RISCVVectorSchedStage::PostSched)
      addSchedulerSummary(Row, *Loop, Regions);
    FeatureDB->addRow(MF.getFunction().getParent(),
                      FeatureRow{MF.getName().str(), LoopUID,
                                 stageName(Stage).str(), std::move(Row)});
    ++Ordinal;
  }
  if (Stage == RISCVVectorSchedStage::PostSched)
    FeatureDB->eraseRegions(&MF);
  if (Stage == RISCVVectorSchedStage::FinalSched)
    FeatureDB->eraseOrders(&MF);
  return false;
}

class RISCVVectorSchedPre : public MachineFunctionPass {
public:
  static char ID;
  RISCVVectorSchedPre() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override {
    return collectRows(MF, RISCVVectorSchedStage::PreSched,
                       getAnalysis<MachineLoopInfoWrapperPass>().getLI(),
                       &getAnalysis<LiveIntervalsWrapperPass>().getLIS());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addRequired<LiveIntervalsWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "RISC-V Vector Scheduling Features (pre-schedule)";
  }
};

class RISCVVectorSchedPost : public MachineFunctionPass {
public:
  static char ID;
  RISCVVectorSchedPost() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override {
    return collectRows(MF, RISCVVectorSchedStage::PostSched,
                       getAnalysis<MachineLoopInfoWrapperPass>().getLI(),
                       &getAnalysis<LiveIntervalsWrapperPass>().getLIS());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addRequired<LiveIntervalsWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "RISC-V Vector Scheduling Features (post-schedule)";
  }
};

class RISCVVectorPostRA : public MachineFunctionPass {
public:
  static char ID;
  RISCVVectorPostRA() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override {
    return collectRows(MF, RISCVVectorSchedStage::PostRVVRA,
                       getAnalysis<MachineLoopInfoWrapperPass>().getLI(),
                       nullptr);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "RISC-V Vector Scheduling Features (post-RVV-RA)";
  }
};

class RISCVVectorFinalSched : public MachineFunctionPass {
public:
  static char ID;
  RISCVVectorFinalSched() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override {
    return collectRows(MF, RISCVVectorSchedStage::FinalSched,
                       getAnalysis<MachineLoopInfoWrapperPass>().getLI(),
                       nullptr);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "RISC-V Vector Scheduling Features (final schedule)";
  }
};

class RISCVVectorSchedReport : public MachineFunctionPass {
public:
  static char ID;
  RISCVVectorSchedReport() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &) override { return false; }
  bool doFinalization(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "RISC-V Vector Scheduling Feature Report";
  }
};

} // end anonymous namespace

char RISCVVectorSchedPre::ID = 0;
char RISCVVectorSchedPost::ID = 0;
char RISCVVectorPostRA::ID = 0;
char RISCVVectorFinalSched::ID = 0;
char RISCVVectorSchedReport::ID = 0;

char &llvm::RISCVVectorSchedPreID = RISCVVectorSchedPre::ID;
char &llvm::RISCVVectorSchedPostID = RISCVVectorSchedPost::ID;
char &llvm::RISCVVectorPostRAID = RISCVVectorPostRA::ID;
char &llvm::RISCVVectorFinalSchedID = RISCVVectorFinalSched::ID;

INITIALIZE_PASS_BEGIN(RISCVVectorSchedPre, "riscv-vector-sched-pre",
                      "RISC-V Vector Scheduling Features (pre-schedule)", false,
                      true)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_END(RISCVVectorSchedPre, "riscv-vector-sched-pre",
                    "RISC-V Vector Scheduling Features (pre-schedule)", false,
                    true)

INITIALIZE_PASS_BEGIN(RISCVVectorSchedPost, "riscv-vector-sched-post",
                      "RISC-V Vector Scheduling Features (post-schedule)",
                      false, true)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_END(RISCVVectorSchedPost, "riscv-vector-sched-post",
                    "RISC-V Vector Scheduling Features (post-schedule)", false,
                    true)

INITIALIZE_PASS_BEGIN(RISCVVectorPostRA, "riscv-vector-post-ra",
                      "RISC-V Vector Scheduling Features (post-RVV-RA)", false,
                      true)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(RISCVVectorPostRA, "riscv-vector-post-ra",
                    "RISC-V Vector Scheduling Features (post-RVV-RA)", false,
                    true)

INITIALIZE_PASS_BEGIN(RISCVVectorFinalSched, "riscv-vector-final-sched",
                      "RISC-V Vector Scheduling Features (final schedule)",
                      false, true)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(RISCVVectorFinalSched, "riscv-vector-final-sched",
                    "RISC-V Vector Scheduling Features (final schedule)", false,
                    true)

INITIALIZE_PASS(RISCVVectorSchedReport, "riscv-vector-sched-report",
                "RISC-V Vector Scheduling Feature Report", false, true)

bool llvm::isRISCVVectorSchedFeatureEnabled() {
  return !RVVSchedFeatureOutput.empty();
}

bool llvm::shouldCollectRISCVVectorSchedFeatures(const MachineFunction &MF) {
  if (!isRISCVVectorSchedFeatureEnabled())
    return false;
  if (!RVVSchedOnlyFunc.empty() && MF.getName() != RVVSchedOnlyFunc)
    return false;
  return MF.getSubtarget<RISCVSubtarget>().hasVInstructions();
}

FunctionPass *llvm::createRISCVVectorSchedPrePass() {
  return new RISCVVectorSchedPre();
}
FunctionPass *llvm::createRISCVVectorSchedPostPass() {
  return new RISCVVectorSchedPost();
}
FunctionPass *llvm::createRISCVVectorPostRAPass() {
  return new RISCVVectorPostRA();
}
FunctionPass *llvm::createRISCVVectorFinalSchedPass() {
  return new RISCVVectorFinalSched();
}
FunctionPass *llvm::createRISCVVectorSchedReportPass() {
  return new RISCVVectorSchedReport();
}

bool RISCVVectorSchedReport::doFinalization(Module &M) {
  if (!isRISCVVectorSchedFeatureEnabled())
    return false;
  std::vector<FeatureRow> Rows = FeatureDB->takeRows(&M);
  llvm::sort(Rows, [](const FeatureRow &L, const FeatureRow &R) {
    return std::tuple(L.Function, L.LoopUID, stageOrder(L.Stage)) <
           std::tuple(R.Function, R.LoopUID, stageOrder(R.Stage));
  });

  std::error_code EC;
  raw_fd_ostream OS(RVVSchedFeatureOutput, EC, sys::fs::OF_Text);
  if (EC)
    report_fatal_error(Twine("unable to open RISC-V vector scheduling feature "
                             "output '") +
                       RVVSchedFeatureOutput + "': " + EC.message());
  for (FeatureRow &Row : Rows)
    OS << json::Value(std::move(Row.Object)) << '\n';
  OS.flush();
  if (OS.has_error())
    report_fatal_error(Twine("failed writing RISC-V vector scheduling feature "
                             "output '") +
                       RVVSchedFeatureOutput + "'");
  return false;
}

static StringRef edgeKindName(SDep::Kind Kind) {
  switch (Kind) {
  case SDep::Data:
    return "data";
  case SDep::Anti:
    return "anti";
  case SDep::Output:
    return "output";
  case SDep::Order:
    return "order";
  }
  llvm_unreachable("unknown scheduler edge kind");
}

uint64_t llvm::beginRISCVVectorSchedTrace(const ScheduleDAGMILive &DAG,
                                          StringRef Direction,
                                          unsigned CriticalPath,
                                          unsigned CyclicCriticalPath) {
  if (!shouldCollectRISCVVectorSchedFeatures(DAG.MF))
    return 0;
  SchedRegionRecord Region;
  Region.MF = &DAG.MF;
  Region.MBB = DAG.begin()->getParent();
  Region.Direction = Direction.str();
  Region.CriticalPath = CriticalPath;
  Region.CyclicCriticalPath = CyclicCriticalPath;
  for (const SUnit &SU : DAG.SUnits) {
    const MachineInstr *MI = SU.getInstr();
    Region.Nodes.push_back(SchedNodeRecord{
        SU.NodeNum, MI->getOpcode(), DAG.TII->getName(MI->getOpcode()).str(),
        SU.getDepth(), SU.getHeight(), isRVVInstruction(*MI)});
    for (const SDep &Dep : SU.Succs) {
      const SUnit *To = Dep.getSUnit();
      Region.Edges.push_back(SchedEdgeRecord{
          SU.NodeNum, To->NodeNum, edgeKindName(Dep.getKind()).str(),
          Dep.getLatency(), Dep.isWeak(), Dep.isCluster()});
    }
  }
  return FeatureDB->addRegion(std::move(Region));
}

void llvm::recordRISCVVectorSchedPick(uint64_t Token, const SUnit &SU,
                                      StringRef Reason, bool IsTop,
                                      unsigned TopReady, unsigned BottomReady,
                                      unsigned UniqueReady,
                                      bool PressureNotLatencyBest,
                                      const RegPressureDelta *RPDelta) {
  if (!Token)
    return;
  SchedPickRecord Pick;
  Pick.Node = SU.NodeNum;
  Pick.Reason = Reason.str();
  Pick.IsTop = IsTop;
  Pick.TopReady = TopReady;
  Pick.BottomReady = BottomReady;
  Pick.UniqueReady = UniqueReady;
  Pick.PressureNotLatencyBest = PressureNotLatencyBest;
  if (RPDelta) {
    const TargetRegisterInfo &TRI =
        *SU.getInstr()->getMF()->getSubtarget().getRegisterInfo();
    auto RecordPressure = [&](const PressureChange &Change,
                              std::string &SetName, int &Delta) {
      if (!Change.isValid())
        return;
      SetName = TRI.getRegPressureSetName(Change.getPSet());
      Delta = Change.getUnitInc();
    };
    RecordPressure(RPDelta->Excess, Pick.ExcessSet, Pick.ExcessDelta);
    RecordPressure(RPDelta->CriticalMax, Pick.CriticalSet, Pick.CriticalDelta);
    RecordPressure(RPDelta->CurrentMax, Pick.CurrentMaxSet,
                   Pick.CurrentMaxDelta);
  }
  FeatureDB->addPick(Token, std::move(Pick));
}

void llvm::endRISCVVectorSchedTrace(uint64_t Token) {
  if (Token)
    FeatureDB->finishRegion(Token);
}
