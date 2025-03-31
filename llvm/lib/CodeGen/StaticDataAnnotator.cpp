//===- StaticDataAnnotator - Annotate static data's section prefix --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// To reason about module-wide data hotness in a module granularity, this file
// implements a module pass StaticDataAnnotator to work coordinately with the
// StaticDataSplitter pass.
//
// The StaticDataSplitter pass is a machine function pass. It analyzes data
// hotness based on code and adds counters in StaticDataProfileInfo via its
// wrapper pass StaticDataProfileInfoWrapper.
// The StaticDataProfileInfoWrapper sits in the middle between the
// StaticDataSplitter and StaticDataAnnotator passes.
// The StaticDataAnnotator pass is a module pass. It iterates global variables
// in the module, looks up counters from StaticDataProfileInfo and sets the
// section prefix based on profiles.
//
// The three-pass structure is implemented for practical reasons, to work around
// the limitation that a module pass based on legacy pass manager cannot make
// use of MachineBlockFrequencyInfo analysis. In the future, we can consider
// porting the StaticDataSplitter pass to a module-pass using the new pass
// manager framework. That way, analysis are lazily computed as opposed to
// eagerly scheduled, and a module pass can use MachineBlockFrequencyInfo.
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/StaticDataProfileInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "static-data-annotator"

using namespace llvm;

static cl::opt<std::string>
    HotSymbolsFile("hot-symbols-file", cl::init(""),
                   cl::desc("Hot symbols file name for jump table hotness"));

/// A module pass which iterates global variables in the module and annotates
/// their section prefixes based on profile-driven analysis.
class StaticDataAnnotator : public ModulePass {
public:
  static char ID;

  StaticDataProfileInfo *SDPI = nullptr;
  const ProfileSummaryInfo *PSI = nullptr;

  StaticDataAnnotator() : ModulePass(ID) {
    initializeStaticDataAnnotatorPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<StaticDataProfileInfoWrapperPass>();
    AU.addRequired<ProfileSummaryInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return "Static Data Annotator"; }

  bool runOnModule(Module &M) override;

  void initHotSymbolSet();

  std::unique_ptr<MemoryBuffer> HotSymbolsBuffer;

  DenseSet<StringRef> HotSymbolsSet;
};

void StaticDataAnnotator::initHotSymbolSet() {
  // read file
  auto MemoryBufferOrErr = MemoryBuffer::getFile(HotSymbolsFile);
  if (!MemoryBufferOrErr) {
    errs() << "Failed to open hot symbols file: " << HotSymbolsFile << " "
           << MemoryBufferOrErr.getError().message() << "\n";
    return;
  }

  HotSymbolsBuffer = std::move(*MemoryBufferOrErr);

  // parse file
  SmallVector<StringRef, 0> Symbols;
  HotSymbolsBuffer->getBuffer().split(Symbols, "\n");
  for (auto &Symbol : Symbols) {
    if (!Symbol.empty())
      HotSymbolsSet.insert(Symbol);
  }
  errs() << "Hot symbols set size: " << HotSymbolsSet.size() << "\n";
}

bool StaticDataAnnotator::runOnModule(Module &M) {
  initHotSymbolSet();
  SDPI = &getAnalysis<StaticDataProfileInfoWrapperPass>()
              .getStaticDataProfileInfo();
  PSI = &getAnalysis<ProfileSummaryInfoWrapperPass>().getPSI();

  if (!PSI->hasProfileSummary())
    return false;

  bool Changed = false;
  for (auto &GV : M.globals()) {
    if (GV.isDeclarationForLinker())
      continue;

    // Get the canonical name of the global variable.
    StringRef Name = GV.getName();
    auto LLVMSuffix = Name.rfind(".llvm.");
    if (LLVMSuffix != StringRef::npos) {
      Name = Name.substr(0, LLVMSuffix);
      errs() << "SDA.cpp:127" << GV.getName() << "\t" << Name << "\n";
    }

    if (!Name.starts_with(".str") && !GV.hasPrivateLinkage()) {
      if (HotSymbolsSet.contains(Name)) {
        errs() << "SDA.cpp:126" << GV.getName() << "\t" << Name << "\n";
        GV.setSectionPrefix("hot");
      } else {
        GV.setSectionPrefix("unlikely");
      }
      Changed = true;
      continue;
    } else {
      errs() << "SDA.cpp:130" << GV.getName() << "\t" << Name << "\n";
    }

    // The implementation below assumes prior passes don't set section prefixes,
    // and specifically do 'assign' rather than 'update'. So report error if a
    // section prefix is already set.
    if (auto maybeSectionPrefix = GV.getSectionPrefix();
        maybeSectionPrefix && !maybeSectionPrefix->empty())
      llvm::report_fatal_error("Global variable " + GV.getName() +
                               " already has a section prefix " +
                               *maybeSectionPrefix);

    StringRef SectionPrefix = SDPI->getConstantSectionPrefix(&GV, PSI);
    if (SectionPrefix.empty())
      continue;

    GV.setSectionPrefix(SectionPrefix);
    Changed = true;
  }

  return Changed;
}

char StaticDataAnnotator::ID = 0;

INITIALIZE_PASS(StaticDataAnnotator, DEBUG_TYPE, "Static Data Annotator", false,
                false)

ModulePass *llvm::createStaticDataAnnotatorPass() {
  return new StaticDataAnnotator();
}
