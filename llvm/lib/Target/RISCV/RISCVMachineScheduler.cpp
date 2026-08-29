//===- RISCVMachineScheduler.cpp - MI Scheduler for RISC-V ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVMachineScheduler.h"
#include "RISCVVectorSchedFeatures.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/ScheduleDAG.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-prera-sched-strategy"

static StringRef reasonName(GenericSchedulerBase::CandReason Reason) {
  switch (Reason) {
  case GenericSchedulerBase::NoCand:
    return "no-candidate";
  case GenericSchedulerBase::Only1:
    return "only-one";
  case GenericSchedulerBase::PhysReg:
    return "phys-reg";
  case GenericSchedulerBase::RegExcess:
    return "reg-excess";
  case GenericSchedulerBase::RegCritical:
    return "reg-critical";
  case GenericSchedulerBase::Stall:
    return "stall";
  case GenericSchedulerBase::Cluster:
    return "cluster-or-vsetvli-compat";
  case GenericSchedulerBase::Weak:
    return "weak";
  case GenericSchedulerBase::RegMax:
    return "reg-max";
  case GenericSchedulerBase::ResourceReduce:
    return "resource-reduce";
  case GenericSchedulerBase::ResourceDemand:
    return "resource-demand";
  case GenericSchedulerBase::BotHeightReduce:
    return "bottom-height-reduce";
  case GenericSchedulerBase::BotPathReduce:
    return "bottom-path-reduce";
  case GenericSchedulerBase::TopDepthReduce:
    return "top-depth-reduce";
  case GenericSchedulerBase::TopPathReduce:
    return "top-path-reduce";
  case GenericSchedulerBase::NodeOrder:
    return "node-order";
  case GenericSchedulerBase::FirstValid:
    return "first-valid";
  }
  llvm_unreachable("unknown scheduler candidate reason");
}

void RISCVPreRAMachineSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  endRISCVVectorSchedTrace(VectorSchedTraceToken);
  VectorSchedTraceToken = 0;
  GenericScheduler::initialize(DAG);
  StringRef Direction = RegionPolicy.OnlyTopDown    ? "topdown"
                        : RegionPolicy.OnlyBottomUp ? "bottomup"
                                                    : "bidirectional";
  VectorSchedTraceToken = beginRISCVVectorSchedTrace(
      *this->DAG, Direction, Rem.CriticalPath, Rem.CyclicCritPath);
}

SUnit *RISCVPreRAMachineSchedStrategy::pickNode(bool &IsTopNode) {
  SUnit *SU = GenericScheduler::pickNode(IsTopNode);
  if (!SU || !VectorSchedTraceToken)
    return SU;

  const SchedCandidate *Selected = nullptr;
  if (IsTopNode && TopCand.SU == SU && TopCand.Reason != NoCand)
    Selected = &TopCand;
  else if (!IsTopNode && BotCand.SU == SU && BotCand.Reason != NoCand)
    Selected = &BotCand;

  SmallPtrSet<SUnit *, 16> UniqueReady;
  for (SUnit *Ready : Top.Available.elements())
    UniqueReady.insert(Ready);
  for (SUnit *Ready : Bot.Available.elements())
    UniqueReady.insert(Ready);
  UniqueReady.insert(SU);

  bool IsPressureReason = Selected && (Selected->Reason == RegExcess ||
                                       Selected->Reason == RegCritical ||
                                       Selected->Reason == RegMax);
  unsigned SelectedPath = IsTopNode ? SU->getHeight() : SU->getDepth();
  unsigned BestReadyPath = SelectedPath;
  for (SUnit *Ready :
       IsTopNode ? Top.Available.elements() : Bot.Available.elements())
    BestReadyPath = std::max(BestReadyPath, IsTopNode ? Ready->getHeight()
                                                      : Ready->getDepth());

  recordRISCVVectorSchedPick(
      VectorSchedTraceToken, *SU,
      reasonName(Selected ? Selected->Reason : Only1), IsTopNode,
      Top.Available.size() + IsTopNode, Bot.Available.size() + !IsTopNode,
      UniqueReady.size(), IsPressureReason && SelectedPath < BestReadyPath,
      Selected ? &Selected->RPDelta : nullptr);
  return SU;
}

RISCV::VSETVLIInfo
RISCVPreRAMachineSchedStrategy::getVSETVLIInfo(const MachineInstr *MI) const {
  unsigned TSFlags = MI->getDesc().TSFlags;
  if (!RISCVII::hasSEWOp(TSFlags))
    return RISCV::VSETVLIInfo();
  return VIA.computeInfoForInstr(*MI);
}

bool RISCVPreRAMachineSchedStrategy::tryVSETVLIInfo(
    const RISCV::VSETVLIInfo &TryInfo, const RISCV::VSETVLIInfo &CandInfo,
    SchedCandidate &TryCand, SchedCandidate &Cand, CandReason Reason) const {
  // Do not compare the vsetvli info changes between top and bottom
  // boundary.
  if (Cand.AtTop != TryCand.AtTop)
    return false;

  auto IsCompatible = [&](const RISCV::VSETVLIInfo &FirstInfo,
                          const RISCV::VSETVLIInfo &SecondInfo) {
    return FirstInfo.isValid() && SecondInfo.isValid() &&
           FirstInfo.isCompatible(RISCV::DemandedFields::all(), SecondInfo,
                                  Context->LIS);
  };

  // Try Cand first.
  // We prefer the top node as it is straightforward from the perspective of
  // vsetvli dataflow.
  if (Cand.AtTop && IsCompatible(CandInfo, TopInfo))
    return true;

  if (!Cand.AtTop && IsCompatible(CandInfo, BottomInfo))
    return true;

  // Then try TryCand.
  if (TryCand.AtTop && IsCompatible(TryInfo, TopInfo)) {
    TryCand.Reason = Reason;
    return true;
  }

  if (!TryCand.AtTop && IsCompatible(TryInfo, BottomInfo)) {
    TryCand.Reason = Reason;
    return true;
  }

  return false;
}

bool RISCVPreRAMachineSchedStrategy::tryCandidate(SchedCandidate &Cand,
                                                  SchedCandidate &TryCand,
                                                  SchedBoundary *Zone) const {
  //-------------------------------------------------------------------------//
  // Below is copied from `GenericScheduler::tryCandidate`.
  // FIXME: Is there a way to not replicate this?
  //-------------------------------------------------------------------------//
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }

  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg))
    return TryCand.Reason != NoCand;

  // Avoid exceeding the target's limit.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.Excess, Cand.RPDelta.Excess, TryCand, Cand,
                  RegExcess, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // We only compare a subset of features when comparing nodes between
  // Top and Bottom boundary. Some properties are simply incomparable, in many
  // other instances we should only override the other boundary if something
  // is a clear good pick on one boundary. Skip heuristics that are more
  // "tie-breaking" in nature.
  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
        tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(Zone->getLatencyStallCycles(TryCand.SU),
                Zone->getLatencyStallCycles(Cand.SU), TryCand, Cand, Stall))
      return TryCand.Reason != NoCand;
  }

  // Keep clustered nodes together to encourage downstream peephole
  // optimizations which may reduce resource requirements.
  //
  // This is a best effort to set things up for a post-RA pass. Optimizations
  // like generating loads of multiple registers should ideally be done within
  // the scheduler pass by combining the loads during DAG postprocessing.
  unsigned CandZoneCluster = Cand.AtTop ? TopClusterID : BotClusterID;
  unsigned TryCandZoneCluster = TryCand.AtTop ? TopClusterID : BotClusterID;
  bool CandIsClusterSucc =
      isTheSameCluster(CandZoneCluster, Cand.SU->ParentClusterIdx);
  bool TryCandIsClusterSucc =
      isTheSameCluster(TryCandZoneCluster, TryCand.SU->ParentClusterIdx);

  if (tryGreater(TryCandIsClusterSucc, CandIsClusterSucc, TryCand, Cand,
                 Cluster))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak))
      return TryCand.Reason != NoCand;
  }

  // Avoid increasing the max pressure of the entire region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
                  Cand, RegMax, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Avoid critical resource consumption and balance the schedule.
    TryCand.initResourceDelta(DAG, SchedModel);
    if (tryLess(TryCand.ResDelta.CritResources, Cand.ResDelta.CritResources,
                TryCand, Cand, ResourceReduce))
      return TryCand.Reason != NoCand;
    if (tryGreater(TryCand.ResDelta.DemandedResources,
                   Cand.ResDelta.DemandedResources, TryCand, Cand,
                   ResourceDemand))
      return TryCand.Reason != NoCand;

    // Avoid serializing long latency dependence chains.
    // For acyclic path limited loops, latency was already checked above.
    if (!RegionPolicy.DisableLatencyHeuristic && TryCand.Policy.ReduceLatency &&
        !Rem.IsAcyclicLatencyLimited && tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum))
      TryCand.Reason = NodeOrder;
  }

  //-------------------------------------------------------------------------//
  // Below is RISC-V specific scheduling heuristics.
  //-------------------------------------------------------------------------//

  // Add RISC-V specific heuristic only when TryCand isn't selected or
  // selected as node order.
  if (TryCand.Reason != NodeOrder && TryCand.Reason != NoCand)
    return true;

  // TODO: We should not use `CandReason::Cluster` here, but is there a
  // mechanism to extend this enum?
  if (ST->enableVsetvliSchedHeuristic() &&
      tryVSETVLIInfo(getVSETVLIInfo(TryCand.SU->getInstr()),
                     getVSETVLIInfo(Cand.SU->getInstr()), TryCand, Cand,
                     Cluster))
    return TryCand.Reason != NoCand;

  return TryCand.Reason != NoCand;
}

void RISCVPreRAMachineSchedStrategy::enterMBB(MachineBasicBlock *MBB) {
  endRISCVVectorSchedTrace(VectorSchedTraceToken);
  VectorSchedTraceToken = 0;
  TopInfo = RISCV::VSETVLIInfo();
  BottomInfo = RISCV::VSETVLIInfo();
}

void RISCVPreRAMachineSchedStrategy::leaveMBB() {
  endRISCVVectorSchedTrace(VectorSchedTraceToken);
  VectorSchedTraceToken = 0;
  TopInfo = RISCV::VSETVLIInfo();
  BottomInfo = RISCV::VSETVLIInfo();
}

void RISCVPreRAMachineSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  GenericScheduler::schedNode(SU, IsTopNode);
  if (ST->enableVsetvliSchedHeuristic()) {
    MachineInstr *MI = SU->getInstr();
    const RISCV::VSETVLIInfo &Info = getVSETVLIInfo(MI);
    if (Info.isValid()) {
      if (IsTopNode)
        TopInfo = Info;
      else
        BottomInfo = Info;
      LLVM_DEBUG({
        dbgs() << "Previous scheduled Unit: \n";
        dbgs() << "  IsTop: " << IsTopNode << "\n";
        dbgs() << "  SU(" << SU->NodeNum << ") - ";
        MI->dump();
        dbgs() << "  \n";
        Info.dump();
        dbgs() << "  \n";
      });
    }
  }
}
