//===- IndirectCallPromotion.cpp - Optimizations based on value profiling -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the transformation that promotes indirect calls to
// conditional direct calls when the indirect-call value profile metadata is
// available.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/IndirectCallPromotionAnalysis.h"
#include "llvm/Analysis/IndirectCallVisitor.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TypeMetadataUtils.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ProfDataUtils.h"
#include "llvm/IR/Value.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Instrumentation/PGOInstrumentation.h"
#include "llvm/Transforms/Utils/CallPromotionUtils.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "pgo-icall-prom"

STATISTIC(NumOfPGOICallPromotion, "Number of indirect call promotions.");
STATISTIC(NumOfPGOICallsites, "Number of indirect call candidate sites.");

// Command line option to disable indirect-call promotion with the default as
// false. This is for debug purpose.
static cl::opt<bool> DisableICP("disable-icp", cl::init(false), cl::Hidden,
                                cl::desc("Disable indirect call promotion"));

namespace llvm {
extern cl::opt<bool> EnableVTableCmp;
}

static cl::opt<int> VTablePromMaxNumAdditionalALUOpForOneFunction(
    "vtable-prom-max-num-additional-op-for-one-function", cl::init(1),
    cl::Hidden,
    cl::desc("vtable prom max num additional ALU op for one function"));

static cl::opt<int> VTablePromMaxNumAdditionalALUOpForFirstOfTwoFunctions(
    "vtable-prom-max-num-additional-op-for-first-of-two-functions", cl::init(0),
    cl::Hidden,
    cl::desc("vtable prom max num additional ALU op for two functions"));

static cl::opt<int> VTablePromMaxNumAdditionalALUOpForSecondOfTwoFunctions(
    "vtable-prom-max-num-additional-op-for-second-of-two-functions",
    cl::init(0), cl::Hidden,
    cl::desc("vtable prom max num additional ALU op for two functions"));

// The max number of additional op for the last candidate.
static cl::opt<int> VTablePromMaxNumAdditionalOpForLastCandidate(
    "vtable-prom-max-num-additional-op-for-last-candidate", cl::init(0),
    cl::Hidden,
    cl::desc("vtable prom max num additional op for last candidate"));

// Used with 2 or 3 function candidates.
static cl::opt<int> VTablePromMaxNumAdditionalALUOpForSecondToLastCandidate(
    "vtable-prom-max-num-additional-op-for-second-to-last-candidate",
    cl::init(0), cl::Hidden,
    cl::desc("vtable prom max num ALU op for second to last candidate"));

// Only used for 3 function candidates.
static cl::opt<int> VTablePromMaxNumALUOpForFirstCandidate(
    "vtable-prom-max-num-additional-op-for-first-candidate", cl::init(0),
    cl::Hidden,
    cl::desc("vtable prom max num ALU op for first to last candidate"));

static cl::opt<int> VTablePromMaxNumAdditionalALUOp(
    "vtable-prom-max-num-additional-alu-op", cl::init(3), cl::Hidden,
    cl::desc("vtable prom max num additional ALU op"));

static cl::opt<bool> EnableICPVerbosePrint("enable-icp-verbose-print",
                                           cl::init(false), cl::Hidden,
                                           cl::desc("Enable verbose print"));

// Set the cutoff value for the promotion. If the value is other than 0, we
// stop the transformation once the total number of promotions equals the cutoff
// value.
// For debug use only.
static cl::opt<unsigned>
    ICPCutOff("icp-cutoff", cl::init(0), cl::Hidden,
              cl::desc("Max number of promotions for this compilation"));

// If ICPCSSkip is non zero, the first ICPCSSkip callsites will be skipped.
// For debug use only.
static cl::opt<unsigned>
    ICPCSSkip("icp-csskip", cl::init(0), cl::Hidden,
              cl::desc("Skip Callsite up to this number for this compilation"));

// Set if the pass is called in LTO optimization. The difference for LTO mode
// is the pass won't prefix the source module name to the internal linkage
// symbols.
static cl::opt<bool> ICPLTOMode("icp-lto", cl::init(false), cl::Hidden,
                                cl::desc("Run indirect-call promotion in LTO "
                                         "mode"));

// Set if the pass is called in SamplePGO mode. The difference for SamplePGO
// mode is it will add prof metadatato the created direct call.
static cl::opt<bool>
    ICPSamplePGOMode("icp-samplepgo", cl::init(false), cl::Hidden,
                     cl::desc("Run indirect-call promotion in SamplePGO mode"));

// If the option is set to true, only call instructions will be considered for
// transformation -- invoke instructions will be ignored.
static cl::opt<bool>
    ICPCallOnly("icp-call-only", cl::init(false), cl::Hidden,
                cl::desc("Run indirect-call promotion for call instructions "
                         "only"));

// If the option is set to true, only invoke instructions will be considered for
// transformation -- call instructions will be ignored.
static cl::opt<bool> ICPInvokeOnly("icp-invoke-only", cl::init(false),
                                   cl::Hidden,
                                   cl::desc("Run indirect-call promotion for "
                                            "invoke instruction only"));

// Dump the function level IR if the transformation happened in this
// function. For debug use only.
static cl::opt<bool>
    ICPDUMPAFTER("icp-dumpafter", cl::init(false), cl::Hidden,
                 cl::desc("Dump IR after transformation happens"));

namespace {

// Promote indirect calls to conditional direct calls, keeping track of
// thresholds.
class IndirectCallPromoter {
public:
  struct VirtualCallInfo {
    uint64_t Offset; // The byte offset from address point; since the address
                     // point offset hasn't been computed yet.
    Instruction *I;  // The vtable load instruction for this virtual call.
    StringRef CompatibleTypeStr;
    Instruction *TypeTestInstr; // the type.test intrinsic
  };

private:
  Function &F;

  // Symtab that maps indirect call profile values to function names and
  // defines.
  InstrProfSymtab *const Symtab;

  DenseMap<const CallBase *, VirtualCallInfo> &CB2VirtualCallInfoMap;

  const bool SamplePGO;

  OptimizationRemarkEmitter &ORE;

  // A struct that records the direct target and it's call count.
  struct PromotionCandidate {
    Function *const TargetFunction;
    const uint64_t Count;

    PromotionCandidate(Function *F, uint64_t C) : TargetFunction(F), Count(C) {}
  };

  // Promote indirect calls based on virtual tables.
  // If a global variable has multiple vtables, which one to compare with? could
  // the address be pre-computed?
  // - https://gcc.godbolt.org/z/oT4fv4qE5
  CallBase &promoteIndirectCallBasedOnVTable(
      CallBase &CB, uint64_t &TotalVTableCount, Function *TargetFunction,
      const SmallVector<VTableCandidate> &VTable2CandidateInfo,
      const std::vector<int> &VTableIndices,
      const std::unordered_map<int /* offset*/, Value *>
          &VTableOffsetToValueMap,
      SmallPtrSet<Function *, 2> &VTablePromotedSet);

  enum class VTableCompareInput {
    // do not use vtable comparison
    kUseFuncComparison = 0,
    // one function and one or two vtable offsets, compute offset-var in orig.bb
    kUseVTableComparison = 1,
    // Next good case: 3 function from 3 vtable, each with unique offset.
    // the rest of cases needs tuning
  };

  VTableCompareInput getPerFunctionVTableIndices(
      CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
      const SmallVector<VTableCandidate> &VTable2CandidateInfo,
      std::vector<std::vector<int>> &PerCandidateVTableIndices,
      SetVector<int> &Offset1, SetVector<int> &Offset2,
      SetVector<int> &Offset3);

  // Check if the indirect-call call site should be promoted. Return the number
  // of promotions. Inst is the candidate indirect call, ValueDataRef
  // contains the array of value profile data for profiled targets,
  // TotalCount is the total profiled count of call executions, and
  // NumCandidates is the number of candidate entries in ValueDataRef.
  std::vector<PromotionCandidate> getPromotionCandidatesForCallSite(
      const CallBase &CB, const ArrayRef<InstrProfValueData> &ValueDataRef,
      uint64_t TotalCount, uint32_t NumCandidates);

  // Promote a list of targets for one indirect-call callsite. Return
  // the number of promotions.
  uint32_t
  tryToPromote(CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
               uint64_t &TotalCount,
               const SmallVector<VTableCandidate> &VTable2CandidateInfo,
               uint64_t &TotalVTableCount,
               std::vector<std::vector<int>> &PerCandidateVTableIndices);

  void getVTable2CandidateInfoForIndirectCall(
      CallBase *CB, SmallVector<VTableCandidate> &VTable2CandidateInfo,
      uint64_t &TotalVTableCount);

public:
  IndirectCallPromoter(
      Function &Func, InstrProfSymtab *Symtab, bool SamplePGO,
      DenseMap<const CallBase *, VirtualCallInfo> &CB2VirtualCallInfoMap,
      OptimizationRemarkEmitter &ORE)
      : F(Func), Symtab(Symtab), CB2VirtualCallInfoMap(CB2VirtualCallInfoMap),
        SamplePGO(SamplePGO), ORE(ORE) {}
  IndirectCallPromoter(const IndirectCallPromoter &) = delete;
  IndirectCallPromoter &operator=(const IndirectCallPromoter &) = delete;

  bool processFunction(ProfileSummaryInfo *PSI);
};

} // end anonymous namespace

static std::optional<int>
getCompatibleTypeOffset(const SmallVector<MDNode *, 2> &Types,
                        StringRef CompatibleType) {
  if (Types.empty()) {
    return std::nullopt;
  }
  int Offset = -1;
  // find the offset where type string is equal to the one in llvm.type.test
  // intrinsic
  for (MDNode *Type : Types) {
    auto TypeIDMetadata = Type->getOperand(1).get();
    if (auto *TypeId = dyn_cast<MDString>(TypeIDMetadata)) {
      StringRef TypeStr = TypeId->getString();
      if (TypeStr != CompatibleType) {
        continue;
      }
      Offset = cast<ConstantInt>(
                   cast<ConstantAsMetadata>(Type->getOperand(0))->getValue())
                   ->getZExtValue();
      break;
    }
  }
  if (Offset == -1) {
    return std::nullopt;
  }
  return Offset;
}

static Function *getFunctionAtVTableOffset(GlobalVariable *GV, uint64_t Offset,
                                           Module &M) {
  Constant *Ptr = getPointerAtOffset(GV->getInitializer(), Offset, M, GV);
  if (!Ptr)
    return nullptr;

  auto C = Ptr->stripPointerCasts();
  auto Fn = dyn_cast<Function>(C);
  auto A = dyn_cast<GlobalAlias>(C);
  if (!Fn && A)
    Fn = dyn_cast<Function>(A->getAliasee());
  return Fn;
}

void IndirectCallPromoter::getVTable2CandidateInfoForIndirectCall(
    CallBase *CB, SmallVector<VTableCandidate> &VTable2CandidateInfo,
    uint64_t &TotalVTableCount) {
  VTable2CandidateInfo.clear();
  auto VirtualCallInfoIter = CB2VirtualCallInfoMap.find(CB);
  if (VirtualCallInfoIter == CB2VirtualCallInfoMap.end())
    return;

  auto &VirtualCallInfo = VirtualCallInfoIter->second;

  Instruction *VTablePtr = VirtualCallInfo.I;
  StringRef CompatibleTypeStr = VirtualCallInfo.CompatibleTypeStr;

  const int MaxNumVTableToConsider = 24;
  std::unique_ptr<InstrProfValueData[]> VTableArray =
      std::make_unique<InstrProfValueData[]>(MaxNumVTableToConsider);
  uint32_t ActualNumValueData = 0;
  // find out all vtables with callees in candidate sets, and their counts
  // if the vtable function isn't one of promotion candidates, do not do
  // anything.
  bool Res = getValueProfDataFromInst(*VTablePtr, IPVK_VTableTarget,
                                      MaxNumVTableToConsider, VTableArray.get(),
                                      ActualNumValueData, TotalVTableCount);
  if (!Res)
    return;

  if (ActualNumValueData == 0)
    return;

  SmallVector<MDNode *, 2> Types; // type metadata associated with a vtable.

  // compute the functions and counts contributed by each vtable.
  for (int j = 0; j < (int)ActualNumValueData; j++) {
    uint64_t VTableVal = VTableArray[j].Value;
    // Question: when you import the variable declarations, shall it has all
    // metadata?
    // - It should.
    GlobalVariable *VTableVariable = Symtab->getGlobalVariable(VTableVal);
    if (!VTableVariable) {
      errs() << "\tQWERTY Why not import vtable definition " << VTableVal
             << " for icp?\n";
      continue;
    }

    Types.clear();
    VTableVariable->getMetadata(LLVMContext::MD_type, Types);
    std::optional<int> MaybeOffset =
        getCompatibleTypeOffset(Types, CompatibleTypeStr);
    if (!MaybeOffset) {
      continue;
    }

    const int FuncByteOffset = (*MaybeOffset) + VirtualCallInfo.Offset;
    Function *VTableCallee = getFunctionAtVTableOffset(
        VTableVariable, FuncByteOffset, *(F.getParent()));
    if (!VTableCallee)
      continue;

    // VTablePtr = load
    // FuncAddr = GEP
    // Func = load
    // call Func

    // NOTE: require instructions are in a sequence (no other instructions in
    // the middle)

    VTable2CandidateInfo.push_back(
        {VTablePtr, VTableVariable, static_cast<uint32_t>(*MaybeOffset),
         VirtualCallInfo.Offset, VTableCallee, VTableArray[j].Count});
  }

  // sort VTable2CandidateInfo by count
  sort(VTable2CandidateInfo.begin(), VTable2CandidateInfo.end(),
       [](const VTableCandidate &LHS, const VTableCandidate &RHS) {
         return LHS.VTableValCount > RHS.VTableValCount;
       });

  return;
}

// Implement variable and constant comparison
CallBase &IndirectCallPromoter::promoteIndirectCallBasedOnVTable(
    CallBase &CB, uint64_t &TotalVTableCount, Function *TargetFunction,
    const SmallVector<VTableCandidate> &VTable2CandidateInfo,
    const std::vector<int> &VTableIndices,
    const std::unordered_map<int /*address-point-offset*/, Value *>
        &VTableOffsetToValueMap,
    SmallPtrSet<Function *, 2> &VTablePromotedSet) {
  uint64_t IfCount = 0;
  for (auto Index : VTableIndices) {
    IfCount += VTable2CandidateInfo[Index].VTableValCount;
  }
  uint64_t ElseCount = TotalVTableCount - IfCount;
  uint64_t MaxCount = (IfCount >= ElseCount ? IfCount : ElseCount);
  uint64_t Scale = calculateCountScale(MaxCount);
  MDBuilder MDB(CB.getContext());
  MDNode *BranchWeights = MDB.createBranchWeights(
      scaleBranchCount(IfCount, Scale), scaleBranchCount(ElseCount, Scale));
  uint64_t SumPromotedVTableCount = 0;
  CallBase &NewInst = promoteIndirectCallWithVTableInfo(
      CB, TargetFunction, VTable2CandidateInfo, VTableIndices,
      VTableOffsetToValueMap, SumPromotedVTableCount, BranchWeights);
  TotalVTableCount -= SumPromotedVTableCount;
  VTablePromotedSet.insert(TargetFunction);

  promoteCall(NewInst, TargetFunction, nullptr, true);
  return NewInst;
}

int getVTableAdditionalALUOps(int NumVTableCandidate) {
  if (NumVTableCandidate == 1)
    return 0;
  // each vtable candidate requires one additional icmp and one or
  if (NumVTableCandidate == 2)
    return 2;
  if (NumVTableCandidate == 3)
    return 4;

  llvm_unreachable("expect vtable candidate size to be smaller than 3");
}

// vptr = load ptr
// func-addr = gep
// funcptr = load func
// res = icmp funcptr
// br
//
// vptr = load ptr
// addr1 = sub ptr, offset
// res = icmp addr1, ptrtoint(@_vtable)
// br
int getNumAdditionalOps(int NumVTableCandidate, bool FunctionHasOffset,
                        int NumVTableOffset) {
  int NumVTableALUOps = getVTableAdditionalALUOps(NumVTableCandidate);

  int FunctionALU = FunctionHasOffset ? 1 : 0;
  // Each vtable offset requires an additional sub
  return NumVTableALUOps + NumVTableOffset - FunctionALU;
}

// Find the vtable candidates for each target function.
// IMPORTANT: all vtables should be annotated for distribution consideration.
// TUNABLE: All vtable candidates should have the same offset right now.
// Add a unit test for this function.
IndirectCallPromoter::VTableCompareInput
IndirectCallPromoter::getPerFunctionVTableIndices(
    CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
    const SmallVector<VTableCandidate> &VTable2CandidateInfo,
    std::vector<std::vector<int>> &PerFuncCandidateVTableIndices,
    SetVector<int> &Offset1, SetVector<int> &Offset2, SetVector<int> &Offset3) {
  if (!EnableVTableCmp)
    return VTableCompareInput::kUseFuncComparison;

  // No function candidate
  if (Candidates.empty())
    return VTableCompareInput::kUseFuncComparison;

  assert(PerFuncCandidateVTableIndices.empty() &&
         "Expect an empty PerCandidateVTableIndices as input");
  PerFuncCandidateVTableIndices.resize(Candidates.size());
  // build the funtion pointer set from function candidates
  SmallDenseMap<Function *, int, 4> FunctionToIndexMap;
  for (int i = 0; i < (int)Candidates.size(); i++) {
    assert(FunctionToIndexMap.find(Candidates[i].TargetFunction) ==
               FunctionToIndexMap.end() &&
           "Expect non-duplicate functions");
    FunctionToIndexMap[Candidates[i].TargetFunction] = i;
  }

  for (int i = 0; i < (int)VTable2CandidateInfo.size(); i++) {
    VTableCandidate C = VTable2CandidateInfo[i];
    // Instruction *const VPtr = C.VTableInstr;
    // FIXME: Re-visit this.
    // - If VTableLoad is not in the same basic block as CB, skip
    // if (VPtr->getParent() != CB.getParent())
    //  continue;

    // Skip this vtable candidate if the function call is not hot enough.
    auto iter = FunctionToIndexMap.find(C.TargetFunction);
    if (iter == FunctionToIndexMap.end())
      continue;

    PerFuncCandidateVTableIndices[iter->second].push_back(i);
  }

  // If any function candidate cannot find enough vtable candidates, load the
  // vfunc.
  if (Candidates.size() == 1) {
    if (PerFuncCandidateVTableIndices[0].size() > 3 ||
        PerFuncCandidateVTableIndices[0].size() < 1) {
      PerFuncCandidateVTableIndices[0].clear();
      if (EnableICPVerbosePrint) {
        errs() << "\t\t\tICP.cpp:FuncCmp [func, vtable] cnt is [1, "
               << PerFuncCandidateVTableIndices[0].size() << "]\n";
      }
      return VTableCompareInput::kUseFuncComparison;
    }

    uint64_t FunctionOffset = 0;
    for (auto Index : PerFuncCandidateVTableIndices[0]) {
      Offset1.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
    }
    // one offset, perfect
    // two offset, takes two more ALU ops
    // three offset, takes four more ALU ops
    errs() << PerFuncCandidateVTableIndices[0].size() << " " << FunctionOffset
           << " " << Offset1.size() << "\n";
    const int NumAdditionalOps =
        getNumAdditionalOps(PerFuncCandidateVTableIndices[0].size(),
                            FunctionOffset != 0, Offset1.size());
    errs() << NumAdditionalOps << "\n";
    if (NumAdditionalOps <= VTablePromMaxNumAdditionalALUOpForOneFunction) {
      return VTableCompareInput::kUseVTableComparison;
    }

    // One function with 3 vtable offsets -> tunable.
    return VTableCompareInput::kUseFuncComparison;
  } else if (Candidates.size() == 2) {
    // if one function needs a vtable load, do it for all.
    if (PerFuncCandidateVTableIndices[0].empty() ||
        PerFuncCandidateVTableIndices[1].empty()) {
      if (EnableICPVerbosePrint) {
        errs() << "\t\t\tICP.cpp:FuncCmp [func, vtable] cnt is [2, "
               << PerFuncCandidateVTableIndices[0].size() << ", "
               << PerFuncCandidateVTableIndices[1].size() << "]\n";
      }
      return VTableCompareInput::kUseFuncComparison;
    }

    SmallSet<int, 2> VTableVarAddressPointOffset;
    uint64_t FunctionOffset = 0;
    for (auto Index : PerFuncCandidateVTableIndices[0]) {
      Offset1.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
      VTableVarAddressPointOffset.insert(
          VTable2CandidateInfo[Index].AddressPointOffset);
    }
    for (auto Index : PerFuncCandidateVTableIndices[1]) {
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
      Offset2.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      VTableVarAddressPointOffset.insert(
          VTable2CandidateInfo[Index].AddressPointOffset);
    }

    int NumAdditionalOps1 = 100;
    int NumAdditionalOps2 = 100;
    if (VTableVarAddressPointOffset.size() == 1) {
      NumAdditionalOps1 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[0].size(), FunctionOffset != 0, 1);
      // Shared offset variable, no function gep
      NumAdditionalOps2 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[1].size(), false, 0);
    }

    // Two offsets. If each vtable has one unique offset, fine.
    if (VTableVarAddressPointOffset.size() == 2) {
      if (Offset1.size() == 1 && Offset2.size() == 1) {
        if (Offset1.front() == Offset2.front()) {
          llvm_unreachable(
              "ICP.cpp two vtables should have different offset\n");
        }
        NumAdditionalOps1 = getNumAdditionalOps(
            PerFuncCandidateVTableIndices[0].size(), FunctionOffset != 0, 1);

        NumAdditionalOps2 = getNumAdditionalOps(
            PerFuncCandidateVTableIndices[1].size(), false, 1);
      }
    }
    errs() << NumAdditionalOps1 << " " << NumAdditionalOps2 << "\n";
    errs() << VTablePromMaxNumAdditionalALUOpForFirstOfTwoFunctions << " "
           << VTablePromMaxNumAdditionalALUOpForSecondOfTwoFunctions << "\n";
    errs() << VTablePromMaxNumAdditionalALUOp << "\n";
    if (NumAdditionalOps1 <=
            VTablePromMaxNumAdditionalALUOpForFirstOfTwoFunctions &&
        NumAdditionalOps2 <=
            VTablePromMaxNumAdditionalALUOpForSecondOfTwoFunctions &&
        (NumAdditionalOps1 + NumAdditionalOps2 <=
         VTablePromMaxNumAdditionalALUOp)) {
      return VTableCompareInput::kUseVTableComparison;
    }
    // There are two many vtables to compare, tune this.
    return VTableCompareInput::kUseFuncComparison;
  } else if (Candidates.size() == 3) {
    // If any promoted function needs a function load, load the function
    // and do function comparison.
    if (PerFuncCandidateVTableIndices[0].empty() ||
        PerFuncCandidateVTableIndices[1].empty() ||
        PerFuncCandidateVTableIndices[2].empty()) {
      if (EnableICPVerbosePrint) {
        errs() << "\t\t\tICP.cpp:FuncCmp [func, vtable] cnt is [3, "
               << PerFuncCandidateVTableIndices[0].size() << ", "
               << PerFuncCandidateVTableIndices[1].size() << ", "
               << PerFuncCandidateVTableIndices[2].size() << "]\n";
      }
      return VTableCompareInput::kUseFuncComparison;
    }
    int NumAdditionalOps1 = 100;
    int NumAdditionalOps2 = 100;
    int NumAdditionalOps3 = 100;
    uint64_t FunctionOffset = 0;
    // one offset, each candidate has its unique vtable
    SmallSet<int, 2> VTableVarAddressPointOffset;
    for (auto Index : PerFuncCandidateVTableIndices[0]) {
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
      Offset1.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      VTableVarAddressPointOffset.insert(
          VTable2CandidateInfo[Index].AddressPointOffset);
    }
    for (auto Index : PerFuncCandidateVTableIndices[1]) {
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
      Offset2.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      VTableVarAddressPointOffset.insert(
          VTable2CandidateInfo[Index].AddressPointOffset);
    }
    for (auto Index : PerFuncCandidateVTableIndices[2]) {
      FunctionOffset = VTable2CandidateInfo[Index].FunctionOffset;
      Offset3.insert(VTable2CandidateInfo[Index].AddressPointOffset);
      VTableVarAddressPointOffset.insert(
          VTable2CandidateInfo[Index].AddressPointOffset);
    }
    // one offset variable
    if (VTableVarAddressPointOffset.size() == 1) {
      NumAdditionalOps1 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[0].size(), FunctionOffset != 0, 1);
      NumAdditionalOps2 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[1].size(), false, 0);
      NumAdditionalOps3 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[2].size(), false, 0);
    } else if (Offset1.size() == 1 && Offset2.size() == 1 &&
               Offset1.front() == Offset2.front() && Offset3.size() == 2 &&
               Offset3.contains(Offset1.front())) {
      NumAdditionalOps1 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[0].size(), FunctionOffset != 0, 1);
      NumAdditionalOps2 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[1].size(), false, 0);
      NumAdditionalOps3 = getNumAdditionalOps(
          PerFuncCandidateVTableIndices[2].size(), false, 1);
    }
    if (NumAdditionalOps1 <= VTablePromMaxNumALUOpForFirstCandidate &&
        NumAdditionalOps2 <=
            VTablePromMaxNumAdditionalALUOpForSecondToLastCandidate &&
        NumAdditionalOps3 <= VTablePromMaxNumAdditionalOpForLastCandidate &&
        (NumAdditionalOps1 + NumAdditionalOps2 + NumAdditionalOps3 <=
         VTablePromMaxNumAdditionalALUOp)) {
      return VTableCompareInput::kUseVTableComparison;
    }
  }
  return VTableCompareInput::kUseFuncComparison;
}

// Indirect-call promotion heuristic. The direct targets are sorted based on
// the count. Stop at the first target that is not promoted.
std::vector<IndirectCallPromoter::PromotionCandidate>
IndirectCallPromoter::getPromotionCandidatesForCallSite(
    const CallBase &CB, const ArrayRef<InstrProfValueData> &ValueDataRef,
    uint64_t TotalCount, uint32_t NumCandidates) {
  std::vector<PromotionCandidate> Ret;

  LLVM_DEBUG(dbgs() << " \nWork on callsite #" << NumOfPGOICallsites << CB
                    << " Num_targets: " << ValueDataRef.size()
                    << " Num_candidates: " << NumCandidates << "\n");
  NumOfPGOICallsites++;
  if (ICPCSSkip != 0 && NumOfPGOICallsites <= ICPCSSkip) {
    LLVM_DEBUG(dbgs() << " Skip: User options.\n");
    return Ret;
  }

  for (uint32_t I = 0; I < NumCandidates; I++) {
    uint64_t Count = ValueDataRef[I].Count;
    assert(Count <= TotalCount);
    (void)TotalCount;
    uint64_t Target = ValueDataRef[I].Value;
    LLVM_DEBUG(dbgs() << " Candidate " << I << " Count=" << Count
                      << "  Target_func: " << Target << "\n");

    if (ICPInvokeOnly && isa<CallInst>(CB)) {
      LLVM_DEBUG(dbgs() << " Not promote: User options.\n");
      ORE.emit([&]() {
        return OptimizationRemarkMissed(DEBUG_TYPE, "UserOptions", &CB)
               << " Not promote: User options";
      });
      break;
    }
    if (ICPCallOnly && isa<InvokeInst>(CB)) {
      LLVM_DEBUG(dbgs() << " Not promote: User option.\n");
      ORE.emit([&]() {
        return OptimizationRemarkMissed(DEBUG_TYPE, "UserOptions", &CB)
               << " Not promote: User options";
      });
      break;
    }
    if (ICPCutOff != 0 && NumOfPGOICallPromotion >= ICPCutOff) {
      LLVM_DEBUG(dbgs() << " Not promote: Cutoff reached.\n");
      ORE.emit([&]() {
        return OptimizationRemarkMissed(DEBUG_TYPE, "CutOffReached", &CB)
               << " Not promote: Cutoff reached";
      });
      break;
    }

    // Don't promote if the symbol is not defined in the module. This avoids
    // creating a reference to a symbol that doesn't exist in the module
    // This can happen when we compile with a sample profile collected from
    // one binary but used for another, which may have profiled targets that
    // aren't used in the new binary. We might have a declaration initially in
    // the case where the symbol is globally dead in the binary and removed by
    // ThinLTO.
    Function *TargetFunction = Symtab->getFunction(Target);
    if (TargetFunction == nullptr || TargetFunction->isDeclaration()) {
      LLVM_DEBUG(dbgs() << " Not promote: Cannot find the target\n");
      ORE.emit([&]() {
        return OptimizationRemarkMissed(DEBUG_TYPE, "UnableToFindTarget", &CB)
               << "Cannot promote indirect call: target with md5sum "
               << ore::NV("target md5sum", Target) << " not found";
      });
      break;
    }

    const char *Reason = nullptr;
    if (!isLegalToPromote(CB, TargetFunction, &Reason)) {
      using namespace ore;

      ORE.emit([&]() {
        return OptimizationRemarkMissed(DEBUG_TYPE, "UnableToPromote", &CB)
               << "Cannot promote indirect call to "
               << NV("TargetFunction", TargetFunction) << " with count of "
               << NV("Count", Count) << ": " << Reason;
      });
      break;
    }

    Ret.push_back(PromotionCandidate(TargetFunction, Count));
    TotalCount -= Count;
  }
  return Ret;
}

CallBase &llvm::pgo::promoteIndirectCall(CallBase &CB, Function *DirectCallee,
                                         uint64_t Count, uint64_t TotalCount,
                                         bool AttachProfToDirectCall,
                                         OptimizationRemarkEmitter *ORE) {

  uint64_t ElseCount = TotalCount - Count;
  uint64_t MaxCount = (Count >= ElseCount ? Count : ElseCount);
  uint64_t Scale = calculateCountScale(MaxCount);
  MDBuilder MDB(CB.getContext());
  MDNode *BranchWeights = MDB.createBranchWeights(
      scaleBranchCount(Count, Scale), scaleBranchCount(ElseCount, Scale));

  CallBase &NewInst =
      promoteCallWithIfThenElse(CB, DirectCallee, BranchWeights);

  if (AttachProfToDirectCall) {
    setBranchWeights(NewInst, {static_cast<uint32_t>(Count)});
  }

  using namespace ore;

  if (ORE)
    ORE->emit([&]() {
      return OptimizationRemark(DEBUG_TYPE, "Promoted", &CB)
             << "Promote indirect call to " << NV("DirectCallee", DirectCallee)
             << " with count " << NV("Count", Count) << " out of "
             << NV("TotalCount", TotalCount);
    });
  return NewInst;
}

// Promote indirect-call to conditional direct-call for one callsite.
uint32_t IndirectCallPromoter::tryToPromote(
    CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
    uint64_t &TotalCount,
    const SmallVector<VTableCandidate> &VTable2CandidateInfo,
    uint64_t &TotalVTableCount,
    std::vector<std::vector<int>> &PerCandidateVTableIndices) {
  PerCandidateVTableIndices.clear();

  SetVector<int> Offset1, Offset2, Offset3;
  auto CompareSolution = getPerFunctionVTableIndices(
      CB, Candidates, VTable2CandidateInfo, PerCandidateVTableIndices, Offset1,
      Offset2, Offset3);

  if (CompareSolution ==
      IndirectCallPromoter::VTableCompareInput::kUseFuncComparison) {
    // This is a fast path to keep the original function address based
    // promotion. Use a private function.
    uint32_t NumPromoted = 0;

    uint64_t PromotedCount = 0;
    for (const auto &C : Candidates) {
      uint64_t Count = C.Count;
      pgo::promoteIndirectCall(CB, C.TargetFunction, Count, TotalCount,
                               SamplePGO, &ORE);
      assert(TotalCount >= Count);
      TotalCount -= Count;
      PromotedCount += Count;
      NumOfPGOICallPromotion++;
      NumPromoted++;
    }

    if (NumPromoted != 0) {
      errs() << "ICP.cpp:func\t" << PromotedCount
             << "\t still doing vfunc-based comparison for ";
      CB.print(errs());
      errs() << " from function "
             << CB.getParent()->getParent()->getName().str().c_str() << "\n";
    }
    return NumPromoted;
  }

  uint64_t CachedVTableCount = TotalVTableCount;

  // Create one constant variable
  const CallBase *CBPtr = &CB;
  assert(CB2VirtualCallInfoMap.find(CBPtr) != CB2VirtualCallInfoMap.end());
  if (EnableICPVerbosePrint) {
    errs() << "The insertion point for ";
    CB.print(errs());
    errs() << " is ";
    CB2VirtualCallInfoMap[CBPtr].TypeTestInstr->print(errs());
    errs() << "\n";
  }
  IRBuilder<> Builder(CB2VirtualCallInfoMap[CBPtr]
                          .TypeTestInstr); // Change this to a different place

  // FIXME: Implementation -> there should be one VTableInstr.
  Value *CastedVTableVar = Builder.CreatePtrToInt(
      VTable2CandidateInfo[PerCandidateVTableIndices[0][0]].VTableInstr,
      Builder.getInt64Ty());
  Value *Sub = Builder.CreateNUWSub(
      CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(Offset1.front())),
      "offset_var", false);

  std::unordered_map<int, Value *> VTableOffsetToValueMap;
  VTableOffsetToValueMap[Offset1.front()] = Sub;

  SmallPtrSet<Function *, 2> VTablePromotedSet;
  // one function candidate
  if (CompareSolution == VTableCompareInput::kUseVTableComparison &&
      Candidates.size() == 1) {
    // for each offset, create an offset var
    auto iter = Offset1.begin();
    iter++; // skip the first offset

    IRBuilder<> CBBlockBuilder(&CB);
    while (iter != Offset1.end()) {
      Value *OffsetVar = CBBlockBuilder.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }
    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[0].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[0],
        VTableOffsetToValueMap, VTablePromotedSet);
  }

  // two vtable candidates
  if (CompareSolution == VTableCompareInput::kUseVTableComparison &&
      Candidates.size() == 2) {
    // create variables for offset1
    auto iter = Offset1.begin();
    iter++; // skip the first offset

    IRBuilder<> CBBlockBuilder(&CB);
    while (iter != Offset1.end()) {
      errs() << "Offset " << *iter << " is not seen\n";
      Value *OffsetVar = CBBlockBuilder.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }
    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[0].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[0],
        VTableOffsetToValueMap, VTablePromotedSet);

    IRBuilder<> CBBlockBuilder2(&CB);
    iter = Offset2.begin();
    while (iter != Offset2.end()) {
      // Variable already created
      if (VTableOffsetToValueMap.find(*iter) != VTableOffsetToValueMap.end()) {
        iter++;
        continue;
      }

      Value *OffsetVar = CBBlockBuilder2.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }

    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[1].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[1],
        VTableOffsetToValueMap, VTablePromotedSet);
  } // Two function candidates

  if (CompareSolution == VTableCompareInput::kUseVTableComparison &&
      Candidates.size() == 3) {
    // create variables for offset1
    auto iter = Offset1.begin();
    iter++; // skip the first offset

    IRBuilder<> CBBlockBuilder(&CB);
    while (iter != Offset1.end()) {
      Value *OffsetVar = CBBlockBuilder.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
    }
    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[0].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[0],
        VTableOffsetToValueMap, VTablePromotedSet);

    iter = Offset2.begin();
    IRBuilder<> CBBlockBuilder2(&CB);
    while (iter != Offset2.end()) {
      // Variable already created
      if (VTableOffsetToValueMap.find(*iter) != VTableOffsetToValueMap.end()) {
        iter++;
        continue;
      }
      Value *OffsetVar = CBBlockBuilder2.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }

    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[1].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[1],
        VTableOffsetToValueMap, VTablePromotedSet);

    IRBuilder<> CBBlockBuilder3(&CB);
    iter = Offset3.begin();
    while (iter != Offset3.begin()) {
      if (VTableOffsetToValueMap.find(*iter) != VTableOffsetToValueMap.end()) {
        iter++;
        continue;
      }
      Value *OffsetVar = CBBlockBuilder3.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }

    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[2].TargetFunction,
        VTable2CandidateInfo, PerCandidateVTableIndices[2],
        VTableOffsetToValueMap, VTablePromotedSet);
  }

  uint64_t PromotedVTableCount = CachedVTableCount - TotalVTableCount;

  if (VTablePromotedSet.size() != 0) {
    errs() << "ICP.cpp:vtable\t" << PromotedVTableCount
           << "\thas vtable prom for function "
           << CB.getParent()->getParent()->getName().str().c_str() << "\n";
  }

  // FIXME: Here assert all functions are promoted
  return VTablePromotedSet.size();
}

// Traverse all the indirect-call callsite and get the value profile
// annotation to perform indirect-call promotion.
bool IndirectCallPromoter::processFunction(ProfileSummaryInfo *PSI) {
  if (EnableICPVerbosePrint) {
    errs() << "Processing function " << F.getName().str().c_str() << "\n";
  }
  bool Changed = false;
  ICallPromotionAnalysis ICallAnalysis;
  SmallVector<VTableCandidate> VTable2CandidateInfo;

  for (auto *CB : findIndirectCalls(F)) {
    if (EnableICPVerbosePrint) {
      errs() << "CallBase is ";
      CB->print(errs());
      errs() << "\n";
    }
    uint32_t NumVals, NumCandidates;
    uint64_t TotalCount;
    auto ICallProfDataRef = ICallAnalysis.getPromotionCandidatesForInstruction(
        CB, NumVals, TotalCount, NumCandidates);
    if (!NumCandidates ||
        (PSI && PSI->hasProfileSummary() && !PSI->isHotCount(TotalCount))) {
      if (EnableICPVerbosePrint) {
        errs() << "CB is not hot enough\n";
        CB->print(errs());
        errs() << "\n";
      }
      continue;
    }
    auto PromotionCandidates = getPromotionCandidatesForCallSite(
        *CB, ICallProfDataRef, TotalCount, NumCandidates);

    uint64_t TotalVTableCount = 0;
    getVTable2CandidateInfoForIndirectCall(CB, VTable2CandidateInfo,
                                           TotalVTableCount);

    std::vector<std::vector<int>> PerCandidateVTableIndices;
    // get the vtable set for each target value.
    // for target values with only one vtable, compare vtable.
    uint32_t NumPromoted =
        tryToPromote(*CB, PromotionCandidates, TotalCount, VTable2CandidateInfo,
                     TotalVTableCount, PerCandidateVTableIndices);
    if (NumPromoted == 0)
      continue;

    Changed = true;
    // Adjust the MD.prof metadata. First delete the old one.
    CB->setMetadata(LLVMContext::MD_prof, nullptr);
    // If all promoted, we don't need the MD.prof metadata.
    if (TotalCount == 0 || NumPromoted == NumVals)
      continue;
    // Otherwise we need update with the un-promoted records back.
    annotateValueSite(*F.getParent(), *CB, ICallProfDataRef.slice(NumPromoted),
                      TotalCount, IPVK_IndirectCallTarget, NumCandidates);
    for (auto &PerCandidateVTableIndexVec : PerCandidateVTableIndices) {
      for (auto &VTableIndex : PerCandidateVTableIndexVec) {
        Instruction *VTableInstr =
            VTable2CandidateInfo[VTableIndex].VTableInstr;
        if (VTableInstr) {
          VTableInstr->setMetadata(LLVMContext::MD_prof, nullptr);
        }
      }
    }
    // annotateValueSite(*F.getParent(), *CB,
    // ICallProfDataRef.slice(NumPromoted),
    //                   TotalCount, IPVK_VTableTarget, NumCandidates);
  }
  return Changed;
}

// A wrapper function that does the actual work.
static bool promoteIndirectCalls(Module &M, ProfileSummaryInfo *PSI, bool InLTO,
                                 bool SamplePGO, ModuleAnalysisManager &MAM) {
  if (DisableICP)
    return false;

  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  auto LookupDomTree = [&FAM](Function &F) -> DominatorTree & {
    return FAM.getResult<DominatorTreeAnalysis>(F);
  };

  InstrProfSymtab Symtab;
  if (Error E = Symtab.create(M, InLTO)) {
    std::string SymtabFailure = toString(std::move(E));
    M.getContext().emitError("Failed to create symtab: " + SymtabFailure);
    return false;
  }

  StringSet<> FunctionsWithVTable;
  // Keys are indirect calls that call virtual function and is the subset of all
  // indirect calls.
  DenseMap<const CallBase *, IndirectCallPromoter::VirtualCallInfo>
      CB2VirtualCallInfoMap;
  // FIXME: What about public.type.test?
  Function *TypeTestFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_test));

  // compute <func GUID, vtable value list>
  if (TypeTestFunc && (!TypeTestFunc->use_empty())) {
    // Iterate type.test and find all indirect calls.
    for (Use &U : llvm::make_early_inc_range(TypeTestFunc->uses())) {
      auto *CI = dyn_cast<CallInst>(U.getUser());
      if (!CI)
        continue;

      // CI->print(errs());
      // CI->getParent()->getParent()->print(errs());
      auto *TypeMDVal = cast<MetadataAsValue>(CI->getArgOperand(1));
      if (!TypeMDVal)
        continue;

      // TypeMDVal->print(errs());
      auto *CompatibleTypeId = dyn_cast<MDString>(TypeMDVal->getMetadata());
      if (!CompatibleTypeId)
        continue;
      // assert(CompatibleTypeId && "Expect a compatible type str");
      StringRef CompatibleTypeStr = CompatibleTypeId->getString();

      // get the offset of vtable in global variable
      SmallVector<DevirtCallSite, 1> DevirtCalls;
      SmallVector<CallInst *, 1> Assumes;
      auto &DT = LookupDomTree(*CI->getFunction());
      findDevirtualizableCallsForTypeTest(DevirtCalls, Assumes, CI, DT);

      // type-id, offset from the address point
      // combined with type metadata to compute function offset
      for (auto &DevirtCall : DevirtCalls) {
        CallBase &CB = DevirtCall.CB;
        uint64_t Offset = DevirtCall.Offset;

        // get compatible type metadata
        // find the vtable load for this indirect call 'CB'.
        Instruction *VTableLoad =
            PGOIndirectCallVisitor::getAnnotatedVTableInstruction(&CB);

        if (!VTableLoad)
          continue;

        // Does 'CB' uniquely identify an address-point-offset?
        // - yep, since CB is unique, and it calls one function.
        // NOTE you don't know address-point-offset here yet.
        CB2VirtualCallInfoMap[&CB] = {
            Offset, VTableLoad, CompatibleTypeStr,
            dyn_cast<Instruction>(CI)}; // indirect call to type.test
        FunctionsWithVTable.insert(CB.getFunction()->getName());
      } // end for DevirtCalls

    } // end for llvm.type.test uses
  } // end for if !llvm.type.test uses empty

  if (!CB2VirtualCallInfoMap.empty() && EnableICPVerbosePrint) {
    printf("ICP.cpp: module %s\n", M.getName().str().c_str());
    for (auto &FuncName : FunctionsWithVTable) {
      printf("\tICP.cpp: function %s\n", FuncName.getKey().str().c_str());
    }
  }

  bool Changed = false;
  for (auto &F : M) {
    if (F.isDeclaration() || F.hasOptNone())
      continue;

    auto &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);

    IndirectCallPromoter CallPromoter(F, &Symtab, SamplePGO,
                                      CB2VirtualCallInfoMap, ORE);
    bool FuncChanged = CallPromoter.processFunction(PSI);
    if (ICPDUMPAFTER && FuncChanged) {
      LLVM_DEBUG(dbgs() << "\n== IR Dump After =="; F.print(dbgs()));
      LLVM_DEBUG(dbgs() << "\n");
    }
    Changed |= FuncChanged;
    if (ICPCutOff != 0 && NumOfPGOICallPromotion >= ICPCutOff) {
      LLVM_DEBUG(dbgs() << " Stop: Cutoff reached.\n");
      break;
    }
  }
  return Changed;
}

PreservedAnalyses PGOIndirectCallPromotion::run(Module &M,
                                                ModuleAnalysisManager &MAM) {
  ProfileSummaryInfo *PSI = &MAM.getResult<ProfileSummaryAnalysis>(M);

  if (!promoteIndirectCalls(M, PSI, InLTO | ICPLTOMode,
                            SamplePGO | ICPSamplePGOMode, MAM))
    return PreservedAnalyses::all();

  return PreservedAnalyses::none();
}
