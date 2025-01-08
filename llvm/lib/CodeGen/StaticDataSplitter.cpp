//===- StaticDataSplitter.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass uses profile information to split out cold static data into a
// cold-suffixed section (e.g., `rodata.cold`, `.data.rel.ro.cold`).

#include "llvm/CodeGen/StaticDataSplitter.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/CodeGen/MBFIWrapper.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "static-data-splitter"

STATISTIC(NumHotJumpTables, "Number of hot jump tables seen");
STATISTIC(NumColdJumpTables, "Number of cold jump tables seen");
STATISTIC(NumUnknownJumpTablse, "Number of jump tables with unknown hotness");

class StaticDataSplitter : public MachineFunctionPass {

  const MachineBranchProbabilityInfo *MBPI = nullptr;
  const MachineBlockFrequencyInfo *MBFI = nullptr;
  const ProfileSummaryInfo *PSI = nullptr;

  void splitJumpTables(MachineFunction &MF);

  void splitJumpTablesWithProfiles(MachineFunction &MF,
                                   MachineJumpTableInfo &MJTI);

public:
  static char ID;

  StaticDataSplitter() : MachineFunctionPass(ID) {
    initializeStaticDataSplitterPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Static Data Splitter"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineBranchProbabilityInfoWrapperPass>();
    AU.addRequired<MachineBlockFrequencyInfoWrapperPass>();
    AU.addRequired<ProfileSummaryInfoWrapperPass>();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

// TODO: The return value
bool StaticDataSplitter::runOnMachineFunction(MachineFunction &MF) {

  MBPI = &getAnalysis<MachineBranchProbabilityInfoWrapperPass>().getMBPI();
  MBFI = &getAnalysis<MachineBlockFrequencyInfoWrapperPass>().getMBFI();
  PSI = &getAnalysis<ProfileSummaryInfoWrapperPass>().getPSI();

  splitJumpTables(MF);

  return false;
}

void StaticDataSplitter::splitJumpTablesWithProfiles(
    MachineFunction &MF, MachineJumpTableInfo &MJTI) {
  DataHotness Hotness = DataHotness::Cold;
  for (const auto &MBB : MF) {
    const int JTI = MBB.getJumpTableIndex();
    if (JTI == -1)
      continue;

    // If the source or any of the destination basic blocks are not not cold,
    // mark the jump table as hot.
    if (!PSI->isColdBlock(&MBB, MBFI))
      Hotness = DataHotness::Hot;

    for (const MachineBasicBlock *MBB : MJTI.getJumpTables()[JTI].MBBs)
      if (!PSI->isColdBlock(MBB, MBFI))
        Hotness = DataHotness::Hot;

    if (Hotness == DataHotness::Hot)
      ++NumHotJumpTables;
    else
      ++NumColdJumpTables;

    MF.getJumpTableInfo()->updateJumpTableHotness(JTI, Hotness);
  }
}

void StaticDataSplitter::splitJumpTables(MachineFunction &MF) {
  MachineJumpTableInfo *MJTI = MF.getJumpTableInfo();
  if (!MJTI)
    return;

  if (PSI && PSI->hasProfileSummary() && MBFI) {
    splitJumpTablesWithProfiles(MF, *MJTI);
    return;
  }

  // If this pass is enabled and a function doesn't have profile information,
  // conservatively mark all jump tables as hot.
  for (size_t JTI = 0; JTI < MJTI->getJumpTables().size(); JTI++)
    MF.getJumpTableInfo()->updateJumpTableHotness(JTI, DataHotness::Hot);

  NumUnknownJumpTablse += MJTI->getJumpTables().size();
}

char StaticDataSplitter::ID = 0;

INITIALIZE_PASS_BEGIN(StaticDataSplitter, DEBUG_TYPE, "Split static data",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineBranchProbabilityInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineBlockFrequencyInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ProfileSummaryInfoWrapperPass)
INITIALIZE_PASS_END(StaticDataSplitter, DEBUG_TYPE, "Split static data", false,
                    false)

MachineFunctionPass *llvm::createStaticDataSplitterPass() {
  return new StaticDataSplitter();
}
