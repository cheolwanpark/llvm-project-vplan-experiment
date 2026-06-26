//===-- RISCVDLMULAnalysis.cpp - RISC-V DLMUL Analysis --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVDLMULAnalysis.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/bit.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/RISCVTargetParser.h"
#include <algorithm>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "riscv-dlmul-analysis"

static cl::opt<std::string>
    DLMULAnalysisOutput("dlmul-analysis",
                        cl::desc("Enable RISC-V DLMUL analysis and write a "
                                 "Markdown report to the given file"),
                        cl::value_desc("filename"), cl::init(""), cl::Hidden);

static cl::opt<unsigned> DLMULAnalysisMaxIslandSize(
    "riscv-dlmul-analysis-max-island-size",
    cl::desc("Maximum number of instructions to include in one RISC-V DLMUL "
             "analysis island"),
    cl::init(64), cl::Hidden);

namespace {

enum DLMULBlocker : uint64_t {
  UnknownOpcode = 1ULL << 0,
  ElementsDependOnVL = 1ULL << 1,
  ReadsPastVL = 1ULL << 2,
  CrossLaneOperation = 1ULL << 3,
  Reduction = 1ULL << 4,
  SlideOrPermute = 1ULL << 5,
  GatherScatter = 1ULL << 6,
  SegmentOperation = 1ULL << 7,
  FaultFirstLoad = 1ULL << 8,
  VolatileMemory = 1ULL << 9,
  MayRaiseFPException = 1ULL << 10,
  HasTiedPassthru = 1ULL << 11,
  TailOrMaskUndisturbed = 1ULL << 12,
  MaskSplitRequired = 1ULL << 13,
  UsesV0Mask = 1ULL << 14,
  EscapesLoop = 1ULL << 15,
  EscapesIsland = 1ULL << 16,
  LoopCarriedPHI = 1ULL << 17,
  UnknownVL = 1ULL << 18,
  IncompatibleVL = 1ULL << 19,
  IncompatibleSEW = 1ULL << 20,
  BoundaryPackRequired = 1ULL << 21,
  BoundaryUnpackRequired = 1ULL << 22,
  AddressRewriteRequired = 1ULL << 23,
  TooLarge = 1ULL << 24,
  LaterHUse = 1ULL << 25,
  HFrontierTooWide = 1ULL << 26,
  NotStoreClosed = 1ULL << 27,
  MaskInsideLIsland = 1ULL << 28,
  StateCarryOrBarrier = 1ULL << 29,
  StaticLMULRegion = 1ULL << 30,
  SpillReloadRepairCandidate = 1ULL << 31,
};

enum class DLMULDirection { GenericSplit, HighToLow };
enum class DLMULKind {
  Unknown,
  ForcedHPrefixToLSuffix,
  HFanoutToLStores,
  SpillDrivenRepair,
  MemoryAnchored,
  StaticLMULRegion,
};
enum class DLMULGrade { Observed, Green, Yellow, Red, Info };

struct DLMULInstrInfo {
  unsigned Opcode = 0;
  unsigned BaseOpcode = 0;
  std::string OpcodeName;
  int LMUL = 0;
  unsigned SEW = 0;
  std::string VLKind = "Unknown";
  uint64_t HardBlockers = 0;
  uint64_t RepairBlockers = 0;
  bool IsRVV = false;
  bool IsFreeModeForHL = false;
  bool HasMask = false;
  bool MayLoad = false;
  bool MayStore = false;
};

struct DLMULCandidate {
  unsigned CandidateId = 0;
  std::string Module;
  std::string Function;
  std::string Stage;
  unsigned MBBNumber = 0;
  unsigned InstrOrdinal = 0;
  unsigned Opcode = 0;
  unsigned BaseOpcode = 0;
  std::string OpcodeName;
  std::string DebugLoc;
  int LMUL = 0;
  unsigned SEW = 0;
  std::string VLKind = "Unknown";
  std::string LoopHeader;
  unsigned LoopDepth = 0;
  unsigned Level = 0;
  DLMULDirection Direction = DLMULDirection::GenericSplit;
  DLMULKind Kind = DLMULKind::Unknown;
  DLMULGrade Grade = DLMULGrade::Observed;
  int Score = 0;
  uint64_t HardBlockers = 0;
  uint64_t RepairBlockers = 0;
  unsigned NumInstrs = 0;
  unsigned NumVectorDefs = 0;
  unsigned NumExits = 0;
  unsigned NumExternalUses = 0;
  unsigned HAnchorCount = 0;
  unsigned HInputFrontier = 0;
  bool HasLaterHUse = false;
  bool StoreClosed = false;
  bool PartitionFeasible = false;
  bool IsDownsplitCandidate = false;
  unsigned BankedRequired = 0;
  unsigned BankedCapacity = 0;
  int TargetLMUL = 0;
  unsigned SplitFactor = 1;
  int OriginalPeakLiveUnits = 0;
  int SplitPeakLiveUnits = 0;
  int LiveUnitSaving = 0;
  unsigned VectorUnitCapacity = 32;
  bool SpillRiskBefore = false;
  bool SpillRiskAfter = false;
  bool SpillRiskReduced = false;
};

struct DLMULSummary {
  unsigned FunctionsScanned = 0;
  unsigned RVVSeeds = 0;
  unsigned VRM8Seeds = 0;
  unsigned LMUL1Seeds = 0;
  unsigned LMUL2Seeds = 0;
  unsigned LMUL4Seeds = 0;
  unsigned LMUL8Seeds = 0;
  unsigned Level1 = 0;
  unsigned Level2 = 0;
  unsigned Level3 = 0;
  unsigned Level4 = 0;
  unsigned HLGreen = 0;
  unsigned HLYellow = 0;
  unsigned HLRed = 0;
  unsigned StaticLMUL = 0;
};

struct DLMULModuleData {
  DLMULSummary Summary;
  std::vector<DLMULCandidate> Candidates;
  unsigned NextCandidateId = 0;
};

class DLMULCandidateDB {
  sys::SmartMutex<true> Mutex;
  StringMap<DLMULModuleData> Modules;

public:
  void merge(StringRef Module, DLMULSummary Summary,
             std::vector<DLMULCandidate> Candidates) {
    sys::SmartScopedLock<true> Lock(Mutex);
    DLMULModuleData &Data = Modules[Module];
    Data.Summary.FunctionsScanned += Summary.FunctionsScanned;
    Data.Summary.RVVSeeds += Summary.RVVSeeds;
    Data.Summary.VRM8Seeds += Summary.VRM8Seeds;
    Data.Summary.LMUL1Seeds += Summary.LMUL1Seeds;
    Data.Summary.LMUL2Seeds += Summary.LMUL2Seeds;
    Data.Summary.LMUL4Seeds += Summary.LMUL4Seeds;
    Data.Summary.LMUL8Seeds += Summary.LMUL8Seeds;
    Data.Summary.Level1 += Summary.Level1;
    Data.Summary.Level2 += Summary.Level2;
    Data.Summary.Level3 += Summary.Level3;
    Data.Summary.Level4 += Summary.Level4;
    Data.Summary.HLGreen += Summary.HLGreen;
    Data.Summary.HLYellow += Summary.HLYellow;
    Data.Summary.HLRed += Summary.HLRed;
    Data.Summary.StaticLMUL += Summary.StaticLMUL;
    for (DLMULCandidate &Candidate : Candidates) {
      Candidate.CandidateId = Data.NextCandidateId++;
      Data.Candidates.push_back(std::move(Candidate));
    }
  }

  DLMULModuleData take(StringRef Module) {
    sys::SmartScopedLock<true> Lock(Mutex);
    auto It = Modules.find(Module);
    if (It == Modules.end())
      return {};
    DLMULModuleData Data = std::move(It->second);
    Modules.erase(It);
    return Data;
  }
};

ManagedStatic<DLMULCandidateDB> DLMULDB;

class DLMULFunctionScanner {
  MachineFunction &MF;
  RISCVDLMULStage Stage;
  MachineRegisterInfo &MRI;
  const TargetInstrInfo &TII;
  MachineLoopInfo *MLI;
  DenseMap<const MachineInstr *, unsigned> Ordinals;
  DenseMap<const MachineInstr *, unsigned> FunctionOrdinals;
  DenseMap<const MachineInstr *, DLMULInstrInfo> InstrInfoCache;
  DenseSet<Register> SeenSeeds;
  DLMULSummary Summary;
  std::vector<DLMULCandidate> Candidates;

public:
  DLMULFunctionScanner(MachineFunction &MF, RISCVDLMULStage Stage,
                       MachineLoopInfo *MLI)
      : MF(MF), Stage(Stage), MRI(MF.getRegInfo()),
        TII(*MF.getSubtarget().getInstrInfo()), MLI(MLI) {}

  std::pair<DLMULSummary, std::vector<DLMULCandidate>> scan();

private:
  void collectOrdinals();
  void scanSeed(Register SeedReg, MachineInstr &SeedMI);
  DLMULInstrInfo classifyInstr(const MachineInstr &MI);
  DLMULCandidate buildCandidate(Register SeedReg, MachineInstr &SeedMI,
                                ArrayRef<MachineInstr *> Island,
                                uint64_t ExtraHardBlockers,
                                uint64_t ExtraRepairBlockers);
  void computeLivePressure(DLMULCandidate &C, ArrayRef<MachineInstr *> Island,
                           const DenseSet<Register> &IslandDefs,
                           bool HasMaskInIsland);
  bool isRVVReg(Register Reg) const;
  bool isVRM8Reg(Register Reg) const;
  bool isDLMULSeedReg(Register Reg) const;
  bool isSourceLMULReg(Register Reg, int SourceLMUL) const;
  int getRegLMUL(Register Reg) const;
  std::string getRegLMULString(Register Reg) const;
  void recordSeedLMUL(int LMUL);
};

class RISCVDLMULCollector : public MachineFunctionPass {
public:
  static char ID;

  RISCVDLMULCollector(RISCVDLMULStage Stage)
      : MachineFunctionPass(ID), Stage(Stage) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addUsedIfAvailable<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setIsSSA();
  }

  StringRef getPassName() const override {
    return "RISC-V DLMUL Analysis Collector";
  }

private:
  RISCVDLMULStage Stage;
};

class RISCVDLMULReport : public MachineFunctionPass {
public:
  static char ID;

  RISCVDLMULReport() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override { return false; }
  bool doFinalization(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return "RISC-V DLMUL Analysis"; }
};

} // end anonymous namespace

char RISCVDLMULCollector::ID = 0;
char RISCVDLMULReport::ID = 0;

INITIALIZE_PASS(RISCVDLMULCollector, DEBUG_TYPE,
                "RISC-V DLMUL Analysis Collector", false, true)
INITIALIZE_PASS(RISCVDLMULReport, "riscv-dlmul-analysis-report",
                "RISC-V DLMUL Analysis Report", false, true)

bool llvm::isRISCVDLMULAnalysisEnabled() {
  return !DLMULAnalysisOutput.empty();
}

FunctionPass *llvm::createRISCVDLMULCollectorPass(RISCVDLMULStage Stage) {
  return new RISCVDLMULCollector(Stage);
}

FunctionPass *llvm::createRISCVDLMULReportPass() {
  return new RISCVDLMULReport();
}

static StringRef stageName(RISCVDLMULStage Stage) {
  switch (Stage) {
  case RISCVDLMULStage::PostISelRaw:
    return "post-isel-raw";
  case RISCVDLMULStage::PostRVVOpt:
    return "post-rvv-opt";
  case RISCVDLMULStage::PreRAInput:
    return "pre-ra-input";
  }
  llvm_unreachable("unknown DLMUL analysis stage");
}

static StringRef directionName(DLMULDirection Direction) {
  switch (Direction) {
  case DLMULDirection::GenericSplit:
    return "GenericSplit";
  case DLMULDirection::HighToLow:
    return "HighToLow";
  }
  llvm_unreachable("unknown DLMUL direction");
}

static StringRef kindName(DLMULKind Kind) {
  switch (Kind) {
  case DLMULKind::Unknown:
    return "Unknown";
  case DLMULKind::ForcedHPrefixToLSuffix:
    return "ForcedHPrefixToLSuffix";
  case DLMULKind::HFanoutToLStores:
    return "HFanoutToLStores";
  case DLMULKind::SpillDrivenRepair:
    return "SpillDrivenRepair";
  case DLMULKind::MemoryAnchored:
    return "MemoryAnchored";
  case DLMULKind::StaticLMULRegion:
    return "StaticLMULRegion";
  }
  llvm_unreachable("unknown DLMUL kind");
}

static StringRef gradeName(DLMULGrade Grade) {
  switch (Grade) {
  case DLMULGrade::Observed:
    return "Observed";
  case DLMULGrade::Green:
    return "Green";
  case DLMULGrade::Yellow:
    return "Yellow";
  case DLMULGrade::Red:
    return "Red";
  case DLMULGrade::Info:
    return "Info";
  }
  llvm_unreachable("unknown DLMUL grade");
}

static StringRef levelName(unsigned Level) {
  switch (Level) {
  case 0:
    return "Observed";
  case 1:
    return "ElementwiseCandidate";
  case 2:
    return "LocalSplitCandidate";
  case 3:
    return "ChainSplitCandidate";
  case 4:
    return "ClosedMemoryToMemoryCandidate";
  }
  return "Unknown";
}

static std::optional<std::pair<unsigned, bool>>
decodeLMUL(RISCVVType::VLMUL LMUL) {
  return RISCVVType::decodeVLMUL(LMUL);
}

static int lmulToSignedInt(RISCVVType::VLMUL LMUL) {
  std::optional<std::pair<unsigned, bool>> Decoded = decodeLMUL(LMUL);
  if (!Decoded)
    return 0;
  return Decoded->second ? -static_cast<int>(Decoded->first)
                         : static_cast<int>(Decoded->first);
}

static std::string lmulName(int LMUL) {
  if (LMUL == 0)
    return "unknown";
  if (LMUL < 0)
    return ("mf" + Twine(-LMUL)).str();
  return ("m" + Twine(LMUL)).str();
}

static bool isAnalyzableIntegerLMUL(int LMUL) {
  return LMUL == 1 || LMUL == 2 || LMUL == 4 || LMUL == 8;
}

static int targetLMULForSplit(int LMUL) {
  if (LMUL > 1)
    return LMUL / 2;
  if (LMUL == 1)
    return 1;
  return 0;
}

static unsigned splitFactorForLMULs(int FromLMUL, int ToLMUL) {
  if (FromLMUL > ToLMUL && ToLMUL > 0)
    return FromLMUL / ToLMUL;
  return 1;
}

static std::string opcodeName(const TargetInstrInfo &TII, unsigned Opcode) {
  if (!Opcode)
    return "unknown";
  return TII.getName(Opcode).str();
}

static std::string debugLocString(const MachineInstr &MI) {
  std::string Result;
  raw_string_ostream OS(Result);
  if (MI.getDebugLoc())
    MI.getDebugLoc().print(OS);
  return Result;
}

static bool hasVolatileMemory(const MachineInstr &MI) {
  for (MachineMemOperand *MMO : MI.memoperands())
    if (MMO->isVolatile())
      return true;
  return false;
}

static bool nameContains(StringRef Name, StringRef Needle) {
  return Name.contains_insensitive(Needle);
}

static void printJSONString(raw_ostream &OS, StringRef S) {
  OS << '"';
  for (char C : S) {
    switch (C) {
    case '\\':
      OS << "\\\\";
      break;
    case '"':
      OS << "\\\"";
      break;
    case '\n':
      OS << "\\n";
      break;
    case '\r':
      OS << "\\r";
      break;
    case '\t':
      OS << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(C) < 0x20)
        OS << formatv("\\u{0:X4}", static_cast<unsigned>(C));
      else
        OS << C;
      break;
    }
  }
  OS << '"';
}

struct BlockerNameEntry {
  uint64_t Bit;
  const char *Name;
};

static constexpr BlockerNameEntry BlockerNames[] = {
    {UnknownOpcode, "UnknownOpcode"},
    {ElementsDependOnVL, "ElementsDependOnVL"},
    {ReadsPastVL, "ReadsPastVL"},
    {CrossLaneOperation, "CrossLaneOperation"},
    {Reduction, "Reduction"},
    {SlideOrPermute, "SlideOrPermute"},
    {GatherScatter, "GatherScatter"},
    {SegmentOperation, "SegmentOperation"},
    {FaultFirstLoad, "FaultFirstLoad"},
    {VolatileMemory, "VolatileMemory"},
    {MayRaiseFPException, "MayRaiseFPException"},
    {HasTiedPassthru, "HasTiedPassthru"},
    {TailOrMaskUndisturbed, "TailOrMaskUndisturbed"},
    {MaskSplitRequired, "MaskSplitRequired"},
    {UsesV0Mask, "UsesV0Mask"},
    {EscapesLoop, "EscapesLoop"},
    {EscapesIsland, "EscapesIsland"},
    {LoopCarriedPHI, "LoopCarriedPHI"},
    {UnknownVL, "UnknownVL"},
    {IncompatibleVL, "IncompatibleVL"},
    {IncompatibleSEW, "IncompatibleSEW"},
    {BoundaryPackRequired, "BoundaryPackRequired"},
    {BoundaryUnpackRequired, "BoundaryUnpackRequired"},
    {AddressRewriteRequired, "AddressRewriteRequired"},
    {TooLarge, "TooLarge"},
    {LaterHUse, "LaterHUse"},
    {HFrontierTooWide, "HFrontierTooWide"},
    {NotStoreClosed, "NotStoreClosed"},
    {MaskInsideLIsland, "MaskInsideLIsland"},
    {StateCarryOrBarrier, "StateCarryOrBarrier"},
    {StaticLMULRegion, "StaticLMULRegion"},
    {SpillReloadRepairCandidate, "SpillReloadRepairCandidate"},
};

static StringRef blockerName(uint64_t Bit) {
  for (const BlockerNameEntry &E : BlockerNames)
    if (E.Bit == Bit)
      return E.Name;
  return "Unknown";
}

static void printBlockerArray(raw_ostream &OS, uint64_t Blockers) {
  OS << '[';
  bool First = true;
  for (const BlockerNameEntry &E : BlockerNames) {
    if (!(Blockers & E.Bit))
      continue;
    if (!First)
      OS << ", ";
    printJSONString(OS, E.Name);
    First = false;
  }
  OS << ']';
}

static unsigned countBlockers(uint64_t Blockers) {
  return llvm::popcount(Blockers);
}

static std::string vlKind(const MachineInstr &MI) {
  const MCInstrDesc &Desc = MI.getDesc();
  if (!RISCVII::hasVLOp(Desc.TSFlags))
    return "Unknown";

  const MachineOperand &VL = MI.getOperand(RISCVII::getVLOpNum(Desc));
  if (VL.isImm())
    return VL.getImm() == RISCV::VLMaxSentinel ? "VLMAXSentinel" : "Immediate";
  if (VL.isReg()) {
    if (VL.getReg() == RISCV::X0)
      return "X0";
    return VL.getReg().isVirtual() ? "VReg" : "PhysReg";
  }
  return "Unknown";
}

static bool hasV0Use(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.getReg() == RISCV::V0)
      return true;
  return false;
}

void DLMULFunctionScanner::collectOrdinals() {
  unsigned FunctionOrdinal = 0;
  for (MachineBasicBlock &MBB : MF) {
    unsigned Ordinal = 0;
    for (MachineInstr &MI : MBB) {
      Ordinals[&MI] = Ordinal++;
      FunctionOrdinals[&MI] = FunctionOrdinal++;
    }
  }
}

bool DLMULFunctionScanner::isRVVReg(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg);
  return RC && RISCVRegisterInfo::isRVVRegClass(RC);
}

int DLMULFunctionScanner::getRegLMUL(Register Reg) const {
  if (!Reg.isVirtual())
    return 0;
  const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg);
  if (!RC || !RISCVRegisterInfo::isRVVRegClass(RC))
    return 0;
  return lmulToSignedInt(RISCVRI::getLMul(RC->TSFlags));
}

std::string DLMULFunctionScanner::getRegLMULString(Register Reg) const {
  return lmulName(getRegLMUL(Reg));
}

bool DLMULFunctionScanner::isVRM8Reg(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg);
  return RC && RISCVRegisterInfo::isVRRegClass(RC) &&
         RISCVRI::getLMul(RC->TSFlags) == RISCVVType::LMUL_8;
}

bool DLMULFunctionScanner::isDLMULSeedReg(Register Reg) const {
  return isAnalyzableIntegerLMUL(getRegLMUL(Reg));
}

bool DLMULFunctionScanner::isSourceLMULReg(Register Reg, int SourceLMUL) const {
  return getRegLMUL(Reg) == SourceLMUL;
}

void DLMULFunctionScanner::recordSeedLMUL(int LMUL) {
  ++Summary.RVVSeeds;
  switch (LMUL) {
  case 1:
    ++Summary.LMUL1Seeds;
    break;
  case 2:
    ++Summary.LMUL2Seeds;
    break;
  case 4:
    ++Summary.LMUL4Seeds;
    break;
  case 8:
    ++Summary.LMUL8Seeds;
    break;
  default:
    break;
  }
}

namespace {
struct DLMULLiveInterval {
  unsigned First = ~0U;
  unsigned Last = 0;
  bool Seen = false;

  void note(unsigned Index) {
    if (!Seen) {
      First = Index;
      Last = Index;
      Seen = true;
      return;
    }
    First = std::min(First, Index);
    Last = std::max(Last, Index);
  }
};
} // end anonymous namespace

void DLMULFunctionScanner::computeLivePressure(
    DLMULCandidate &C, ArrayRef<MachineInstr *> Island,
    const DenseSet<Register> &IslandDefs, bool HasMaskInIsland) {
  SmallVector<MachineInstr *, 32> OrderedIsland;
  DenseSet<const MachineInstr *> IslandSet;
  for (MachineInstr *MI : Island) {
    if (!MI || !IslandSet.insert(MI).second)
      continue;
    OrderedIsland.push_back(MI);
  }

  std::sort(OrderedIsland.begin(), OrderedIsland.end(),
            [this](const MachineInstr *L, const MachineInstr *R) {
              return FunctionOrdinals.lookup(L) < FunctionOrdinals.lookup(R);
            });

  C.VectorUnitCapacity = HasMaskInIsland ? 31 : 32;
  if (OrderedIsland.empty())
    return;

  DenseMap<Register, DLMULLiveInterval> Intervals;
  DenseSet<Register> IslandRegs;

  auto NoteReg = [&](Register Reg, unsigned Index) {
    if (!isRVVReg(Reg))
      return;
    IslandRegs.insert(Reg);
    Intervals[Reg].note(Index);
  };

  for (unsigned Index = 0; Index != OrderedIsland.size(); ++Index) {
    MachineInstr *MI = OrderedIsland[Index];
    for (const MachineOperand &MO : MI->uses()) {
      if (!MO.isReg() || !MO.getReg().isVirtual())
        continue;
      NoteReg(MO.getReg(), Index);
    }
    for (const MachineOperand &MO : MI->defs()) {
      if (!MO.isReg() || !MO.getReg().isVirtual())
        continue;
      NoteReg(MO.getReg(), Index);
    }
  }

  unsigned LastIndex = OrderedIsland.size() - 1;
  for (Register Reg : IslandRegs) {
    DLMULLiveInterval &Interval = Intervals[Reg];
    if (!Interval.Seen)
      continue;

    if (!IslandDefs.contains(Reg))
      Interval.First = 0;

    if (!IslandDefs.contains(Reg))
      continue;

    for (MachineOperand &Use : MRI.use_nodbg_operands(Reg)) {
      MachineInstr *User = Use.getParent();
      if (!User || IslandSet.contains(User))
        continue;
      Interval.Last = LastIndex;
      break;
    }
  }

  int OriginalPeak = 0;
  int SplitPeak = 0;
  for (unsigned Index = 0; Index != OrderedIsland.size(); ++Index) {
    int OriginalUnits = 0;
    int SplitUnits = 0;
    for (Register Reg : IslandRegs) {
      const DLMULLiveInterval &Interval = Intervals[Reg];
      if (!Interval.Seen || Index < Interval.First || Index > Interval.Last)
        continue;

      int RegLMUL = std::max(getRegLMUL(Reg), 1);
      OriginalUnits += RegLMUL;
      if (C.IsDownsplitCandidate && getRegLMUL(Reg) == C.LMUL)
        SplitUnits += std::max(C.TargetLMUL, 1);
      else
        SplitUnits += RegLMUL;
    }
    OriginalPeak = std::max(OriginalPeak, OriginalUnits);
    SplitPeak = std::max(SplitPeak, SplitUnits);
  }

  C.OriginalPeakLiveUnits = OriginalPeak;
  C.SplitPeakLiveUnits = SplitPeak;
  C.LiveUnitSaving = std::max(0, OriginalPeak - SplitPeak);
  C.SpillRiskBefore = OriginalPeak > static_cast<int>(C.VectorUnitCapacity);
  C.SpillRiskAfter = SplitPeak > static_cast<int>(C.VectorUnitCapacity);
  C.SpillRiskReduced = C.SpillRiskBefore && !C.SpillRiskAfter;
}

DLMULInstrInfo DLMULFunctionScanner::classifyInstr(const MachineInstr &MI) {
  auto Found = InstrInfoCache.find(&MI);
  if (Found != InstrInfoCache.end())
    return Found->second;

  DLMULInstrInfo Info;
  Info.Opcode = MI.getOpcode();
  Info.BaseOpcode = RISCV::getRVVMCOpcode(MI.getOpcode());
  Info.OpcodeName = opcodeName(TII, MI.getOpcode());
  Info.IsRVV = Info.BaseOpcode != 0;
  Info.MayLoad = MI.mayLoad();
  Info.MayStore = MI.mayStore();

  const MCInstrDesc &Desc = MI.getDesc();
  uint64_t TSFlags = Desc.TSFlags;

  if (Info.IsRVV) {
    Info.LMUL = lmulToSignedInt(RISCVII::getLMul(TSFlags));
    if (RISCVII::hasSEWOp(TSFlags)) {
      const MachineOperand &SEWOp = MI.getOperand(RISCVII::getSEWOpNum(Desc));
      if (SEWOp.isImm() && SEWOp.getImm() < 32)
        Info.SEW = 1U << SEWOp.getImm();
    }
    Info.VLKind = vlKind(MI);

    unsigned RVVTSFlags = TII.get(Info.BaseOpcode).TSFlags;
    if (RISCVII::elementsDependOnVL(RVVTSFlags))
      Info.HardBlockers |= ElementsDependOnVL;
    if (RISCVII::readsPastVL(RVVTSFlags))
      Info.HardBlockers |= ReadsPastVL;
    if (RISCVII::isRVVWideningReduction(TSFlags))
      Info.HardBlockers |= Reduction;
    if (RISCVII::isTiedPseudo(TSFlags))
      Info.RepairBlockers |= HasTiedPassthru;
    if (RISCVII::elementsDependOnMask(RVVTSFlags)) {
      Info.HasMask = true;
      Info.RepairBlockers |= MaskSplitRequired;
    }
    if (RISCVII::hasVecPolicyOp(TSFlags)) {
      const MachineOperand &Policy =
          MI.getOperand(RISCVII::getVecPolicyOpNum(Desc));
      if (Policy.isImm() && (Policy.getImm() & RISCVVType::TAIL_AGNOSTIC) == 0)
        Info.RepairBlockers |= TailOrMaskUndisturbed;
    }
  } else if (!MI.isCopy() && !MI.isPHI())
    Info.HardBlockers |= UnknownOpcode;

  if (MI.isCall() || MI.isInlineAsm() || MI.hasUnmodeledSideEffects())
    Info.HardBlockers |= StateCarryOrBarrier;
  if (MI.mayRaiseFPException())
    Info.RepairBlockers |= MayRaiseFPException;
  if (hasVolatileMemory(MI))
    Info.HardBlockers |= VolatileMemory;
  if (hasV0Use(MI)) {
    Info.HasMask = true;
    Info.RepairBlockers |= UsesV0Mask | MaskSplitRequired;
  }

  StringRef Name = Info.OpcodeName;
  if (nameContains(Name, "RED"))
    Info.HardBlockers |= Reduction;
  if (nameContains(Name, "SLIDE") || nameContains(Name, "RGATHER") ||
      nameContains(Name, "COMPRESS") || nameContains(Name, "PERMUTE"))
    Info.HardBlockers |= SlideOrPermute | CrossLaneOperation;
  if (nameContains(Name, "GATHER") || nameContains(Name, "SCATTER") ||
      nameContains(Name, "VLX") || nameContains(Name, "VSX") ||
      nameContains(Name, "VLOX") || nameContains(Name, "VSOX"))
    Info.HardBlockers |= GatherScatter;
  if (nameContains(Name, "SEG"))
    Info.HardBlockers |= SegmentOperation;
  if (nameContains(Name, "FF"))
    Info.HardBlockers |= FaultFirstLoad;

  Info.IsFreeModeForHL = Info.IsRVV && !Info.HasMask && !Info.HardBlockers &&
                         !Info.RepairBlockers &&
                         (MI.mayLoadOrStore() || Info.LMUL > 1);

  DLMULInstrInfo Inserted = Info;
  InstrInfoCache.insert({&MI, Inserted});
  return Info;
}

std::pair<DLMULSummary, std::vector<DLMULCandidate>>
DLMULFunctionScanner::scan() {
  ++Summary.FunctionsScanned;
  collectOrdinals();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      for (MachineOperand &MO : MI.defs()) {
        if (!MO.isReg() || !isDLMULSeedReg(MO.getReg()))
          continue;
        if (!SeenSeeds.insert(MO.getReg()).second)
          continue;
        recordSeedLMUL(getRegLMUL(MO.getReg()));
        if (isVRM8Reg(MO.getReg()))
          ++Summary.VRM8Seeds;
        scanSeed(MO.getReg(), MI);
      }
    }
  }

  return {Summary, std::move(Candidates)};
}

void DLMULFunctionScanner::scanSeed(Register SeedReg, MachineInstr &SeedMI) {
  SmallVector<Register, 16> WorklistRegs;
  DenseSet<Register> SeenRegs;
  DenseSet<MachineInstr *> SeenInstrs;
  SmallVector<MachineInstr *, 32> Island;
  uint64_t ExtraHardBlockers = 0;
  uint64_t ExtraRepairBlockers = 0;

  WorklistRegs.push_back(SeedReg);
  SeenRegs.insert(SeedReg);

  const MachineLoop *SeedLoop =
      MLI && SeedMI.getParent() ? MLI->getLoopFor(SeedMI.getParent()) : nullptr;

  while (!WorklistRegs.empty()) {
    Register Reg = WorklistRegs.pop_back_val();
    MachineInstr *Def = MRI.getVRegDef(Reg);
    if (Def && SeenInstrs.insert(Def).second)
      Island.push_back(Def);

    for (MachineOperand &Use : MRI.use_nodbg_operands(Reg)) {
      MachineInstr *User = Use.getParent();
      if (!User)
        continue;
      if (SeedLoop && User->getParent() &&
          MLI->getLoopFor(User->getParent()) != SeedLoop) {
        ExtraRepairBlockers |= EscapesLoop;
        continue;
      }

      if (SeenInstrs.insert(User).second)
        Island.push_back(User);

      if (Island.size() > DLMULAnalysisMaxIslandSize) {
        ExtraHardBlockers |= TooLarge;
        break;
      }

      if (User->isCopy() || User->isPHI()) {
        for (MachineOperand &DefMO : User->defs()) {
          if (!DefMO.isReg() || !DefMO.getReg().isVirtual() ||
              !isRVVReg(DefMO.getReg()))
            continue;
          if (SeenRegs.insert(DefMO.getReg()).second)
            WorklistRegs.push_back(DefMO.getReg());
        }
      } else if (User->mayStore()) {
        ExtraRepairBlockers |= AddressRewriteRequired;
      } else {
        for (MachineOperand &DefMO : User->defs()) {
          if (!DefMO.isReg() || !DefMO.getReg().isVirtual() ||
              !isRVVReg(DefMO.getReg()))
            continue;
          if (SeenRegs.insert(DefMO.getReg()).second)
            WorklistRegs.push_back(DefMO.getReg());
        }
      }
    }

    if (ExtraHardBlockers & TooLarge)
      break;
  }

  if (Island.empty())
    Island.push_back(&SeedMI);

  Candidates.push_back(buildCandidate(SeedReg, SeedMI, Island,
                                      ExtraHardBlockers, ExtraRepairBlockers));
}

DLMULCandidate DLMULFunctionScanner::buildCandidate(
    Register SeedReg, MachineInstr &SeedMI, ArrayRef<MachineInstr *> Island,
    uint64_t ExtraHardBlockers, uint64_t ExtraRepairBlockers) {
  DLMULCandidate C;
  Module *M = MF.getFunction().getParent();
  C.Module = M ? M->getModuleIdentifier() : "";
  C.Function = MF.getName().str();
  C.Stage = stageName(Stage).str();
  C.MBBNumber = SeedMI.getParent() ? SeedMI.getParent()->getNumber() : 0;
  C.InstrOrdinal = Ordinals.lookup(&SeedMI);
  C.DebugLoc = debugLocString(SeedMI);
  C.LMUL = getRegLMUL(SeedReg);
  C.TargetLMUL = targetLMULForSplit(C.LMUL);
  C.SplitFactor = splitFactorForLMULs(C.LMUL, C.TargetLMUL);
  C.IsDownsplitCandidate = C.TargetLMUL > 0 && C.TargetLMUL < C.LMUL;

  DLMULInstrInfo SeedInfo = classifyInstr(SeedMI);
  C.Opcode = SeedInfo.Opcode;
  C.BaseOpcode = SeedInfo.BaseOpcode;
  C.OpcodeName = SeedInfo.OpcodeName;
  C.SEW = SeedInfo.SEW;
  C.VLKind = SeedInfo.VLKind;

  const MachineLoop *Loop =
      MLI && SeedMI.getParent() ? MLI->getLoopFor(SeedMI.getParent()) : nullptr;
  if (Loop) {
    C.LoopDepth = Loop->getLoopDepth();
    if (MachineBasicBlock *Header = Loop->getHeader())
      C.LoopHeader = ("bb." + Twine(Header->getNumber())).str();
  }

  DenseSet<const MachineInstr *> IslandSet;
  for (MachineInstr *MI : Island)
    IslandSet.insert(MI);
  DenseSet<Register> IslandDefs;
  DenseSet<Register> HInputs;
  unsigned RVVNodes = 0;
  unsigned FreeNodes = 0;
  unsigned Loads = 0;
  unsigned Stores = 0;
  bool HasMaskInIsland = false;

  C.HardBlockers = ExtraHardBlockers;
  C.RepairBlockers = ExtraRepairBlockers;

  for (MachineInstr *MI : Island) {
    if (!MI)
      continue;
    DLMULInstrInfo Info = classifyInstr(*MI);
    C.HardBlockers |= Info.HardBlockers;
    C.RepairBlockers |= Info.RepairBlockers;
    if (Info.IsRVV)
      ++RVVNodes;
    if (Info.IsFreeModeForHL)
      ++FreeNodes;
    if (Info.MayLoad)
      ++Loads;
    if (Info.MayStore) {
      ++Stores;
      ++C.NumExits;
    }

    if (Info.HasMask) {
      HasMaskInIsland = true;
      C.HardBlockers |= MaskInsideLIsland;
    }

    for (const MachineOperand &MO : MI->defs()) {
      if (MO.isReg() && MO.getReg().isVirtual() && isRVVReg(MO.getReg())) {
        IslandDefs.insert(MO.getReg());
        ++C.NumVectorDefs;
      }
    }
  }

  for (MachineInstr *MI : Island) {
    if (!MI)
      continue;
    for (const MachineOperand &MO : MI->uses()) {
      if (!MO.isReg() || !MO.getReg().isVirtual() || !isRVVReg(MO.getReg()))
        continue;
      if (!IslandDefs.contains(MO.getReg()) &&
          isSourceLMULReg(MO.getReg(), C.LMUL))
        HInputs.insert(MO.getReg());
    }
  }

  for (Register DefReg : IslandDefs) {
    for (MachineOperand &Use : MRI.use_nodbg_operands(DefReg)) {
      MachineInstr *User = Use.getParent();
      if (!User || IslandSet.contains(User))
        continue;
      ++C.NumExternalUses;
      C.RepairBlockers |= EscapesIsland;
      if (isSourceLMULReg(DefReg, C.LMUL)) {
        C.HasLaterHUse = true;
        C.HardBlockers |= LaterHUse;
      }
    }
  }

  C.NumInstrs = Island.size();
  C.HInputFrontier = HInputs.size();
  C.HAnchorCount = HInputs.empty() ? 0 : HInputs.size();
  C.StoreClosed = Stores > 0 && C.NumExternalUses == 0;
  computeLivePressure(C, Island, IslandDefs, HasMaskInIsland);
  C.BankedCapacity = 8;
  C.BankedRequired =
      C.HInputFrontier * C.SplitFactor + std::max(1U, C.NumVectorDefs);
  C.PartitionFeasible = C.BankedRequired <= C.BankedCapacity;

  if (C.HInputFrontier >= 4) {
    C.HardBlockers |= HFrontierTooWide;
    C.HasLaterHUse = true;
  }
  if (!C.StoreClosed && Stores == 0)
    C.RepairBlockers |= NotStoreClosed;

  bool HasHardBlockers = C.HardBlockers != 0;
  bool HasRepairBlockers = C.RepairBlockers != 0;
  bool Elementwise = RVVNodes != 0 && !HasHardBlockers;
  C.Level = Elementwise ? 1 : 0;
  if (Elementwise && !HasRepairBlockers)
    C.Level = 2;
  if (C.Level >= 2 && C.NumVectorDefs > 1 && C.LiveUnitSaving > 0)
    C.Level = 3;
  if (C.Level >= 3 && Loads > 0 && Stores > 0 && C.StoreClosed)
    C.Level = 4;

  bool LooksHighToLow = C.IsDownsplitCandidate && C.StoreClosed &&
                        FreeNodes > 0 && C.PartitionFeasible;
  if (LooksHighToLow) {
    C.Direction = DLMULDirection::HighToLow;
    C.Kind = C.HInputFrontier > 1 ? DLMULKind::HFanoutToLStores
                                  : DLMULKind::ForcedHPrefixToLSuffix;
    if (!HasHardBlockers && C.HInputFrontier < 4)
      C.Grade = HasRepairBlockers ? DLMULGrade::Yellow : DLMULGrade::Green;
    else
      C.Grade = DLMULGrade::Red;
  } else if (!C.IsDownsplitCandidate && RVVNodes != 0) {
    C.Direction = DLMULDirection::GenericSplit;
    C.Kind = DLMULKind::StaticLMULRegion;
    C.Grade = DLMULGrade::Info;
    C.RepairBlockers |= StaticLMULRegion;
  } else if (!C.StoreClosed && FreeNodes == RVVNodes && RVVNodes != 0) {
    C.Direction = DLMULDirection::GenericSplit;
    C.Kind = DLMULKind::StaticLMULRegion;
    C.Grade = DLMULGrade::Info;
    C.RepairBlockers |= StaticLMULRegion;
  } else if (HasHardBlockers) {
    C.Grade = DLMULGrade::Red;
  } else if (HasRepairBlockers) {
    C.Grade = DLMULGrade::Yellow;
  }

  C.Score = 10 * static_cast<int>(C.LoopDepth) +
            5 * static_cast<int>(C.NumInstrs) + 5 * C.LiveUnitSaving -
            20 * static_cast<int>(countBlockers(C.RepairBlockers)) -
            80 * static_cast<int>(countBlockers(C.HardBlockers));
  if (C.Level >= 3)
    C.Score += 100;
  if (C.Level >= 4)
    C.Score += 40;
  if (C.Direction == DLMULDirection::HighToLow)
    C.Score += C.Grade == DLMULGrade::Green ? 120 : 45;
  if (C.Kind == DLMULKind::StaticLMULRegion)
    C.Score += 10;

  if (C.Level >= 1)
    ++Summary.Level1;
  if (C.Level >= 2)
    ++Summary.Level2;
  if (C.Level >= 3)
    ++Summary.Level3;
  if (C.Level >= 4)
    ++Summary.Level4;
  if (C.Direction == DLMULDirection::HighToLow && C.Grade == DLMULGrade::Green)
    ++Summary.HLGreen;
  if (C.Direction == DLMULDirection::HighToLow && C.Grade == DLMULGrade::Yellow)
    ++Summary.HLYellow;
  if (C.Direction == DLMULDirection::HighToLow && C.Grade == DLMULGrade::Red)
    ++Summary.HLRed;
  if (C.Kind == DLMULKind::StaticLMULRegion)
    ++Summary.StaticLMUL;

  return C;
}

bool RISCVDLMULCollector::runOnMachineFunction(MachineFunction &MF) {
  if (!isRISCVDLMULAnalysisEnabled())
    return false;

  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.hasVInstructions())
    return false;

  MachineLoopInfo *MLI = nullptr;
  if (auto *Wrapper = getAnalysisIfAvailable<MachineLoopInfoWrapperPass>())
    MLI = &Wrapper->getLI();

  DLMULFunctionScanner Scanner(MF, Stage, MLI);
  auto [Summary, Candidates] = Scanner.scan();
  Module *M = MF.getFunction().getParent();
  DLMULDB->merge(M ? M->getModuleIdentifier() : "", Summary,
                 std::move(Candidates));
  return false;
}

static void printCandidateJSON(raw_ostream &OS, const DLMULCandidate &C) {
  OS << "{\n";
  OS << "  \"module\": ";
  printJSONString(OS, C.Module);
  OS << ",\n  \"function\": ";
  printJSONString(OS, C.Function);
  OS << ",\n  \"stage\": ";
  printJSONString(OS, C.Stage);
  OS << ",\n  \"candidate_id\": " << C.CandidateId;
  OS << ",\n  \"level\": ";
  printJSONString(OS, levelName(C.Level));
  OS << ",\n  \"direction\": ";
  printJSONString(OS, directionName(C.Direction));
  OS << ",\n  \"kind\": ";
  printJSONString(OS, kindName(C.Kind));
  OS << ",\n  \"grade\": ";
  printJSONString(OS, gradeName(C.Grade));
  OS << ",\n  \"score\": " << C.Score;
  OS << ",\n  \"loop\": { \"header\": ";
  printJSONString(OS, C.LoopHeader);
  OS << ", \"depth\": " << C.LoopDepth << " }";
  OS << ",\n  \"seed\": { \"mbb\": " << C.MBBNumber
     << ", \"ordinal\": " << C.InstrOrdinal << ", \"opcode\": ";
  printJSONString(OS, C.OpcodeName);
  OS << ", \"debug_loc\": ";
  printJSONString(OS, C.DebugLoc);
  OS << " }";
  OS << ",\n  \"shape\": { \"from_lmul\": ";
  printJSONString(OS, lmulName(C.LMUL));
  OS << ", \"to_lmul\": ";
  printJSONString(OS, lmulName(C.TargetLMUL));
  OS << ", \"split_factor\": " << C.SplitFactor
     << ", \"is_downsplit_candidate\": "
     << (C.IsDownsplitCandidate ? "true" : "false") << ", \"sew\": " << C.SEW
     << ", \"vl_kind\": ";
  printJSONString(OS, C.VLKind);
  OS << " }";
  OS << ",\n  \"high_to_low\": { \"h_anchor_count\": " << C.HAnchorCount
     << ", \"h_input_frontier\": " << C.HInputFrontier
     << ", \"has_later_h_use\": " << (C.HasLaterHUse ? "true" : "false")
     << ", \"store_closed\": " << (C.StoreClosed ? "true" : "false")
     << ", \"partition_feasible\": " << (C.PartitionFeasible ? "true" : "false")
     << ", \"banked_required\": " << C.BankedRequired
     << ", \"banked_capacity\": " << C.BankedCapacity << " }";
  OS << ",\n  \"island\": { \"num_instrs\": " << C.NumInstrs
     << ", \"num_vector_defs\": " << C.NumVectorDefs
     << ", \"num_exits\": " << C.NumExits
     << ", \"num_external_uses\": " << C.NumExternalUses << " }";
  OS << ",\n  \"pressure\": { \"original_peak_live_units\": "
     << C.OriginalPeakLiveUnits
     << ", \"split_peak_live_units\": " << C.SplitPeakLiveUnits
     << ", \"live_unit_saving\": " << C.LiveUnitSaving
     << ", \"vector_unit_capacity\": " << C.VectorUnitCapacity
     << ", \"spill_risk_before\": " << (C.SpillRiskBefore ? "true" : "false")
     << ", \"spill_risk_after\": " << (C.SpillRiskAfter ? "true" : "false")
     << ", \"spill_risk_reduced\": " << (C.SpillRiskReduced ? "true" : "false")
     << " }";
  OS << ",\n  \"blockers\": { \"hard\": ";
  printBlockerArray(OS, C.HardBlockers);
  OS << ", \"repair\": ";
  printBlockerArray(OS, C.RepairBlockers);
  OS << " }\n";
  OS << "}\n";
}

static void printReport(raw_ostream &OS, StringRef Module,
                        DLMULModuleData &Data) {
  std::stable_sort(Data.Candidates.begin(), Data.Candidates.end(),
                   [](const DLMULCandidate &L, const DLMULCandidate &R) {
                     if (L.Score != R.Score)
                       return L.Score > R.Score;
                     if (L.Function != R.Function)
                       return L.Function < R.Function;
                     if (L.Stage != R.Stage)
                       return L.Stage < R.Stage;
                     if (L.MBBNumber != R.MBBNumber)
                       return L.MBBNumber < R.MBBNumber;
                     return L.InstrOrdinal < R.InstrOrdinal;
                   });

  MapVector<uint64_t, unsigned> BlockerCounts;
  for (const DLMULCandidate &C : Data.Candidates) {
    uint64_t Blockers = C.HardBlockers | C.RepairBlockers;
    for (unsigned I = 0; I != 64; ++I) {
      uint64_t Bit = 1ULL << I;
      if (Blockers & Bit)
        ++BlockerCounts[Bit];
    }
  }
  SmallVector<std::pair<uint64_t, unsigned>, 16> SortedBlockers;
  for (const auto &Entry : BlockerCounts)
    SortedBlockers.push_back(Entry);
  std::sort(SortedBlockers.begin(), SortedBlockers.end(),
            [](const auto &L, const auto &R) {
              if (L.second != R.second)
                return L.second > R.second;
              return L.first < R.first;
            });

  OS << "# DLMUL analysis summary\n\n";
  OS << "- module: `" << Module << "`\n";
  OS << "- functions scanned: " << Data.Summary.FunctionsScanned << "\n";
  OS << "- RVV seeds: " << Data.Summary.RVVSeeds << "\n";
  OS << "- VRM8 seeds: " << Data.Summary.VRM8Seeds << "\n";
  OS << "- LMUL m1 seeds: " << Data.Summary.LMUL1Seeds << "\n";
  OS << "- LMUL m2 seeds: " << Data.Summary.LMUL2Seeds << "\n";
  OS << "- LMUL m4 seeds: " << Data.Summary.LMUL4Seeds << "\n";
  OS << "- LMUL m8 seeds: " << Data.Summary.LMUL8Seeds << "\n";
  OS << "- Level 1 elementwise candidates: " << Data.Summary.Level1 << "\n";
  OS << "- Level 2 local split candidates: " << Data.Summary.Level2 << "\n";
  OS << "- Level 3 chain split candidates: " << Data.Summary.Level3 << "\n";
  OS << "- Level 4 closed memory-to-memory candidates: " << Data.Summary.Level4
     << "\n";
  OS << "- High-to-Low green candidates: " << Data.Summary.HLGreen << "\n";
  OS << "- High-to-Low yellow candidates: " << Data.Summary.HLYellow << "\n";
  OS << "- High-to-Low red candidates: " << Data.Summary.HLRed << "\n";
  OS << "- Static LMUL info regions: " << Data.Summary.StaticLMUL << "\n\n";

  OS << "## Top blockers\n\n";
  if (SortedBlockers.empty()) {
    OS << "- none\n\n";
  } else {
    unsigned Printed = 0;
    for (const auto &[Bit, Count] : SortedBlockers) {
      OS << "- " << blockerName(Bit) << ": " << Count << "\n";
      if (++Printed == 10)
        break;
    }
    OS << "\n";
  }

  OS << "## Top candidates\n\n";
  OS << "| id | score | function | stage | level | direction | kind | grade | "
        "blockers |\n";
  OS << "| --- | ---: | --- | --- | --- | --- | --- | --- | ---: |\n";
  for (const DLMULCandidate &C : Data.Candidates) {
    OS << "| " << C.CandidateId << " | " << C.Score << " | `" << C.Function
       << "` | " << C.Stage << " | " << levelName(C.Level) << " | "
       << directionName(C.Direction) << " | " << kindName(C.Kind) << " | "
       << gradeName(C.Grade) << " | "
       << countBlockers(C.HardBlockers | C.RepairBlockers) << " |\n";
  }

  OS << "\n## Candidate records\n\n";
  for (const DLMULCandidate &C : Data.Candidates) {
    OS << "```json\n";
    printCandidateJSON(OS, C);
    OS << "```\n\n";
  }
}

bool RISCVDLMULReport::doFinalization(Module &M) {
  if (!isRISCVDLMULAnalysisEnabled())
    return false;

  DLMULModuleData Data = DLMULDB->take(M.getModuleIdentifier());
  if (DLMULAnalysisOutput == "-") {
    printReport(outs(), M.getModuleIdentifier(), Data);
    return false;
  }

  std::error_code EC;
  raw_fd_ostream OS(DLMULAnalysisOutput, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "error: unable to open DLMUL analysis output file '"
           << DLMULAnalysisOutput << "': " << EC.message() << "\n";
    return false;
  }

  printReport(OS, M.getModuleIdentifier(), Data);
  return false;
}
