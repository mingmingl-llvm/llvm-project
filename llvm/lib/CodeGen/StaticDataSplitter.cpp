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

class StaticDataSplitter : public MachineFunctionPass {
  StaticDataSplitterOptions Options;

public:
  static char ID;

  StaticDataSplitter(StaticDataSplitterOptions Options)
      : MachineFunctionPass(ID), Options(Options) {
    initializeStaticDataSplitterPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Static Data Splitter"; }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

char StaticDataSplitter::ID = 0;

INITIALIZE_PASS(StaticDataSplitter, DEBUG_TYPE, "Split static data", false,
                false)

MachineFunctionPass *
llvm::createStaticDataSplitterPass(const bool SplitJumpTable,
                                   const bool SplitConstantPool) {
  StaticDataSplitterOptions Options;
  Options.SplitJumpTables = SplitJumpTable;
  Options.SplitConstantPool = SplitConstantPool;
  return new StaticDataSplitter(Options);
}
