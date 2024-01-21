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

static cl::opt<int> ICPVTableCmpInstThreshold(
    "icp-vtable-cmp-inst-threshold", cl::init(1), cl::Hidden,
    cl::desc(
        "The maximum number of additional instructions for each candidate."));

static cl::opt<int> ICPVTableCmpLastCandidateInstThreshold(
    "icp-vtable-cmp-inst-last-candidate-threshold", cl::init(2), cl::Hidden,
    cl::desc("The number of additional instructions allowed for the last "
             "candidate"));

static cl::opt<int> VTableCmpTotalInstThreshold(
    "icp-vtable-cmp-total-inst-threshold", cl::init(3), cl::Hidden,
    cl::desc("The total number of additional instructions allowed across all "
             "function candidates of an indirect call site"));

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

  DenseMap<const CallBase *, VirtualCallInfo> &VirtualCallToTypeInfo;

  const bool SamplePGO;

  OptimizationRemarkEmitter &ORE;

  // A struct that records the direct target and it's call count.
  struct PromotionCandidate {
    Function *const TargetFunction;
    const uint64_t Count;

    PromotionCandidate(Function *F, uint64_t C) : TargetFunction(F), Count(C) {}
  };

  // Promote indirect calls based on virtual tables.
  CallBase &promoteIndirectCallBasedOnVTable(
      CallBase &CB, uint64_t &TotalVTableCount, Function *TargetFunction,
      Instruction *VPtr,
      const SmallVector<VTableCandidateInfo> &VTable2CandidateInfo,
      const std::vector<int> &VTableIndices,
      const std::unordered_map<int /* offset*/, Value *>
          &VTableOffsetToValueMap,
      SmallPtrSet<Function *, 2> &VTablePromotedSet,
      OptimizationRemarkEmitter *ORE);

  enum class CompareOption {
    // compare functions
    kFunction = 0,
    // compare vtables
    kVTable = 1,
  };

  // Associates a function candidate with profiled vtables.
  struct PerFuncVTableInfo {
    // The function offset (e.g. byte size computed by gep instruction)
    uint64_t FunctionOffset;
    // The indices to access VTableCandidateInfo.
    std::vector<int> Indices;
    // The vtable address point offsets that are FIRST seen because of this
    // function candidate. Specifically, if an address point is seen by prior
    // function candidate of the same indirect callsite, later function
    // candidates won't record the offset.
    SetVector<int> Offsets;
  };

  // Analyze function profiles and vtable profiles, and returns the comparison
  // option. If vtable comparison is chosen, `PerFuncVCInfo` is initialized
  // to do vtable-based transformations.
  CompareOption
  analyzeProfiles(CallBase &CB,
                  const std::vector<PromotionCandidate> &FunctionCandidates,
                  const SmallVector<VTableCandidateInfo> &VCInfo,
                  std::vector<PerFuncVTableInfo> &PerFuncVCInfo);

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
  uint32_t tryToPromote(CallBase &CB,
                        const std::vector<PromotionCandidate> &Candidates,
                        uint64_t &TotalCount);

  // Do indirect call promotion using function-based comparison.
  uint32_t tryToPromoteAndCompareFunctions(
      CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
      uint64_t &TotalCount);

  // Compute the vtable information and sum of all vtable counts for an indirect
  // call.
  uint64_t computeVTableCandidateInfo(
      const CallBase &CB,
      SmallVector<VTableCandidateInfo> &VTable2CandidateInfo);

public:
  IndirectCallPromoter(
      Function &Func, InstrProfSymtab *Symtab, bool SamplePGO,
      DenseMap<const CallBase *, VirtualCallInfo> &VirtualCallToTypeInfo,
      OptimizationRemarkEmitter &ORE)
      : F(Func), Symtab(Symtab), VirtualCallToTypeInfo(VirtualCallToTypeInfo),
        SamplePGO(SamplePGO), ORE(ORE) {}
  IndirectCallPromoter(const IndirectCallPromoter &) = delete;
  IndirectCallPromoter &operator=(const IndirectCallPromoter &) = delete;

  bool processFunction(ProfileSummaryInfo *PSI);
};

} // end anonymous namespace

using VirtualCallToTypeInfoMapTy =
    DenseMap<const CallBase *, IndirectCallPromoter::VirtualCallInfo>;

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

uint64_t IndirectCallPromoter::computeVTableCandidateInfo(
    const CallBase &CB, SmallVector<VTableCandidateInfo> &VTableInfo) {
  VTableInfo.clear();
  uint64_t TotalVTableCount = 0U;
  // Return early if type information is not found. Most likely the callee is
  // not a virtual function.
  auto VirtualCallInfoIter = VirtualCallToTypeInfo.find(&CB);
  if (VirtualCallInfoIter == VirtualCallToTypeInfo.end())
    return TotalVTableCount;

  auto &VirtualCallInfo = VirtualCallInfoIter->second;

  Instruction *VTablePtr = VirtualCallInfo.I;
  StringRef CompatibleTypeStr = VirtualCallInfo.CompatibleTypeStr;

  const int MaxNumVTableToConsider = 24;

  uint32_t ActualNumValueData = 0;
  // find out all vtables with callees in candidate sets, and their counts
  // if the vtable function isn't one of promotion candidates, do not do
  // anything.
  auto VTableValueDataArray = getValueProfDataFromInst(
      *VTablePtr, IPVK_VTableTarget, MaxNumVTableToConsider, ActualNumValueData,
      TotalVTableCount);

  if (VTableValueDataArray.get() == nullptr)
    return TotalVTableCount;

  LLVM_DEBUG(dbgs() << "Callsite #" << NumOfPGOICallsites << CB
                    << " has type profiles\n");

  SmallVector<MDNode *, 2> Types; // type metadata associated with a vtable.

  // compute the functions and counts contributed by each vtable.
  for (int j = 0; j < (int)ActualNumValueData; j++) {
    uint64_t VTableVal = VTableValueDataArray[j].Value;
    // Question: when you import the variable declarations, shall it has all
    // metadata?
    // - It should.
    GlobalVariable *VTableVariable = Symtab->getGlobalVariable(VTableVal);
    if (!VTableVariable) {
      LLVM_DEBUG(dbgs() << "\tCannot find vtable definition for " << VTableVal
                        << "\n");
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
    Function *Callee = getFunctionAtVTableOffset(VTableVariable, FuncByteOffset,
                                                 *(F.getParent()));
    if (!Callee)
      continue;

    VTableInfo.push_back({VTableVariable, static_cast<uint32_t>(*MaybeOffset),
                          VirtualCallInfo.Offset, Callee,
                          VTableValueDataArray[j].Count});
  }

  // Sort vtable information by count.
  sort(VTableInfo.begin(), VTableInfo.end(),
       [](const VTableCandidateInfo &LHS, const VTableCandidateInfo &RHS) {
         return LHS.VTableValCount > RHS.VTableValCount;
       });

  return TotalVTableCount;
}

CallBase &IndirectCallPromoter::promoteIndirectCallBasedOnVTable(
    CallBase &CB, uint64_t &TotalVTableCount, Function *TargetFunction,
    Instruction *VPtr,
    const SmallVector<VTableCandidateInfo> &VTable2CandidateInfo,
    const std::vector<int> &VTableIndices,
    const std::unordered_map<int /*address-point-offset*/, Value *>
        &VTableOffsetToValueMap,
    SmallPtrSet<Function *, 2> &VTablePromotedSet,
    OptimizationRemarkEmitter *ORE) {
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
      CB, TargetFunction, VPtr, VTable2CandidateInfo, VTableIndices,
      VTableOffsetToValueMap, SumPromotedVTableCount, BranchWeights);

  using namespace ore;
  if (ORE)
    ORE->emit([&]() {
      return OptimizationRemark(DEBUG_TYPE, "Promoted", &CB)
             << "Promote indirect call to "
             << NV("DirectCallee", TargetFunction) << " with count "
             << NV("Count", SumPromotedVTableCount) << " out of "
             << NV("TotalCount", TotalVTableCount);
    });
  TotalVTableCount -= SumPromotedVTableCount;
  VTablePromotedSet.insert(TargetFunction);

  promoteCall(NewInst, TargetFunction, nullptr, true);

  return NewInst;
}

// Computes the number of additional instructions if vtable comparison is used
// rather than function comparison.
// Assuming that `ptrtoint ptr vptr to i64` is no-op after instruction lowering.
int getNumAdditionalInsts(int NumVTableCandidate, bool FunctionHasOffset,
                          int NumVTableOffset) {
  // One icmp instruction for each vtable candidate.
  int NumVTableCmpInsts = NumVTableCandidate;
  // The number of OR instructions to or icmp results together.
  int NumVTableOrInsts = NumVTableCmpInsts - 1;

  // If function offset is not zero, GEP is used to calculated function address.
  int NumFunctionGEPInst = FunctionHasOffset ? 1 : 0;

  return NumVTableCmpInsts + NumVTableOrInsts + NumVTableOffset -
         NumFunctionGEPInst - 1 /* NumFunctionICmp */;
}

// Analyze function profiles and vtable profiles and returns whether to compare
// functions or vtables for indirect call promotion.
IndirectCallPromoter::CompareOption IndirectCallPromoter::analyzeProfiles(
    CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
    const SmallVector<VTableCandidateInfo> &VTable2CandidateInfo,
    std::vector<PerFuncVTableInfo> &PerFuncVCInfo) {
  // Return early if vtable comparison is not enabled or if there are no
  // function candidates.
  if (!EnableVTableCmp || Candidates.empty())
    return CompareOption::kFunction;

  PerFuncVCInfo.resize(Candidates.size());

  // Key is target function, and value is the index.
  SmallDenseMap<Function *, int, 4> FunctionToIndexMap;
  for (int i = 0; i < (int)Candidates.size(); i++) {
    assert(FunctionToIndexMap.find(Candidates[i].TargetFunction) ==
               FunctionToIndexMap.end() &&
           "Expect non-duplicate functions");
    FunctionToIndexMap[Candidates[i].TargetFunction] = i;
  }

  for (int i = 0; i < (int)VTable2CandidateInfo.size(); i++) {
    VTableCandidateInfo VC = VTable2CandidateInfo[i];

    // FIXME: skip a vtable candidate if its function is not hot enough.
    auto FuncIndexIter = FunctionToIndexMap.find(VC.TargetFunction);
    if (FuncIndexIter == FunctionToIndexMap.end())
      continue;
    int FuncIndex = FuncIndexIter->second;

    // Note, function offset for the same function candidate remains the same
    // across all vtable candidates.
    PerFuncVCInfo[FuncIndex].FunctionOffset = VC.FunctionOffset;
    PerFuncVCInfo[FuncIndex].Indices.push_back(i);
    PerFuncVCInfo[FuncIndex].Offsets.insert(VC.AddressPointOffset);
  }

  int VTableCmpSumInst = 0;
  // Records the offsets seen in prior vtable candidates.
  SetVector<int> Offsets;

  for (size_t i = 0; i < PerFuncVCInfo.size(); i++) {
    auto &PerFuncVI = PerFuncVCInfo[i];
    // Fall back to function comparison if vtable cannot be found for any
    // function candidate.
    if (PerFuncVI.Indices.empty())
      return CompareOption::kFunction;

    int VTableCmpInstThreshold = (i == PerFuncVCInfo.size() - 1)
                                     ? ICPVTableCmpLastCandidateInstThreshold
                                     : ICPVTableCmpInstThreshold;

    PerFuncVI.Offsets.set_subtract(Offsets);
    const int NumAdditionalInsts = getNumAdditionalInsts(
        PerFuncVI.Indices.size(), PerFuncVI.FunctionOffset != 0,
        PerFuncVI.Offsets.size());

    // If the cost of comparing vtables is higher than that of comparing
    // functions, fall back to function comparison.
    if (NumAdditionalInsts > VTableCmpInstThreshold) {
      return CompareOption::kFunction;
    }

    VTableCmpSumInst += NumAdditionalInsts;

    for (auto &Offset : PerFuncVI.Offsets) {
      Offsets.insert(Offset);
    }
  }

  return VTableCmpSumInst > VTableCmpTotalInstThreshold
             ? CompareOption::kFunction
             : CompareOption::kVTable;
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

uint32_t IndirectCallPromoter::tryToPromoteAndCompareFunctions(
    CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
    uint64_t &TotalCount) {

  uint32_t NumPromoted = 0;

  uint64_t PromotedCount = 0;
  for (const auto &C : Candidates) {
    uint64_t Count = C.Count;
    pgo::promoteIndirectCall(CB, C.TargetFunction, Count, TotalCount, SamplePGO,
                             &ORE);
    assert(TotalCount >= Count);
    TotalCount -= Count;
    PromotedCount += Count;
    NumOfPGOICallPromotion++;
    NumPromoted++;
  }

  return NumPromoted;
}

// Promote indirect-call to conditional direct-call for one callsite.
uint32_t IndirectCallPromoter::tryToPromote(
    CallBase &CB, const std::vector<PromotionCandidate> &Candidates,
    uint64_t &TotalCount) {
  if (!EnableVTableCmp)
    return tryToPromoteAndCompareFunctions(CB, Candidates, TotalCount);

  SmallVector<VTableCandidateInfo> VTable2CandidateInfo;
  uint64_t TotalVTableCount =
      computeVTableCandidateInfo(CB, VTable2CandidateInfo);

  std::vector<PerFuncVTableInfo> PerFuncVCInfo;
  auto CompareSolution =
      analyzeProfiles(CB, Candidates, VTable2CandidateInfo, PerFuncVCInfo);

  if (CompareSolution == IndirectCallPromoter::CompareOption::kFunction)
    return tryToPromoteAndCompareFunctions(CB, Candidates, TotalCount);

  Instruction *VPtr = PGOIndirectCallVisitor::tryGetVTableInstruction(&CB);

  // Create one constant variable
  const CallBase *CBPtr = &CB;
  assert(VirtualCallToTypeInfo.find(CBPtr) != VirtualCallToTypeInfo.end());

  auto TypeInfo = VirtualCallToTypeInfo.at(CBPtr);
  IRBuilder<> Builder(TypeInfo.TypeTestInstr);

  Value *CastedVTableVar = Builder.CreatePtrToInt(VPtr, Builder.getInt64Ty());

  auto FirstOffset = PerFuncVCInfo[0].Offsets.front();
  Value *Sub = Builder.CreateNUWSub(
      CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(FirstOffset)),
      "offset_var", false /* FoldConstant */);

  std::unordered_map<int, Value *> VTableOffsetToValueMap;
  VTableOffsetToValueMap[FirstOffset] = Sub;

  SmallPtrSet<Function *, 2> VTablePromotedSet;

  // for each offset, create an offset var
  // FIXME:
  // As opposed to creating offset var (to represent an address in the middle of
  // vtable array) for comparison, create alias of the address to be compared
  // directly. The aliases are only created for frequently accessed vtables.
  auto iter = PerFuncVCInfo[0].Offsets.begin();
  iter++; // skip the first offset

  IRBuilder<> CBBlockBuilder(&CB);
  while (iter != PerFuncVCInfo[0].Offsets.end()) {
    Value *OffsetVar = CBBlockBuilder.CreateNUWSub(
        CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
        "offset_var", false /* FoldConstant*/);
    VTableOffsetToValueMap[*iter] = OffsetVar;
    iter++;
  }
  promoteIndirectCallBasedOnVTable(
      CB, TotalVTableCount, Candidates[0].TargetFunction, VPtr,
      VTable2CandidateInfo, PerFuncVCInfo[0].Indices, VTableOffsetToValueMap,
      VTablePromotedSet, &ORE);

  for (size_t i = 1; i < Candidates.size(); i++) {
    auto &Offset = PerFuncVCInfo[i].Offsets;
    IRBuilder<> CBBlockBuilder(&CB);
    iter = Offset.begin();
    while (iter != Offset.end()) {
      // Variable already created
      if (VTableOffsetToValueMap.find(*iter) != VTableOffsetToValueMap.end()) {
        iter++;
        continue;
      }

      Value *OffsetVar = CBBlockBuilder.CreateNUWSub(
          CastedVTableVar, Builder.getInt64(static_cast<uint64_t>(*iter)),
          "offset_var", false /* FoldConstant*/);
      VTableOffsetToValueMap[*iter] = OffsetVar;
      iter++;
    }

    promoteIndirectCallBasedOnVTable(
        CB, TotalVTableCount, Candidates[i].TargetFunction, VPtr,
        VTable2CandidateInfo, PerFuncVCInfo[i].Indices, VTableOffsetToValueMap,
        VTablePromotedSet, &ORE);
  }

  return VTablePromotedSet.size();
}

// Traverse all the indirect-call callsite and get the value profile
// annotation to perform indirect-call promotion.
bool IndirectCallPromoter::processFunction(ProfileSummaryInfo *PSI) {
  bool Changed = false;
  ICallPromotionAnalysis ICallAnalysis;

  for (auto *CB : findIndirectCalls(F)) {
    uint32_t NumVals, NumCandidates;
    uint64_t TotalCount;
    auto ICallProfDataRef = ICallAnalysis.getPromotionCandidatesForInstruction(
        CB, NumVals, TotalCount, NumCandidates);
    if (!NumCandidates ||
        (PSI && PSI->hasProfileSummary() && !PSI->isHotCount(TotalCount))) {
      continue;
    }
    auto PromotionCandidates = getPromotionCandidatesForCallSite(
        *CB, ICallProfDataRef, TotalCount, NumCandidates);

    // get the vtable set for each target value.
    // for target values with only one vtable, compare vtable.
    uint32_t NumPromoted = tryToPromote(*CB, PromotionCandidates, TotalCount);
    if (NumPromoted == 0)
      continue;

    Changed = true;
    // Adjust the MD.prof metadata. First delete the old one.
    CB->setMetadata(LLVMContext::MD_prof, nullptr);

    // Nullify the vtable profiles.
    // FIXME: This assumes ICP happens once per indirect-call.
    // A more accurate profile update is to preserve vtable counters that are
    // not promoted.
    Instruction *VPtr = PGOIndirectCallVisitor::tryGetVTableInstruction(CB);
    if (VPtr && mayHaveValueProfileOfKind(*VPtr, IPVK_VTableTarget)) {
      VPtr->setMetadata(LLVMContext::MD_prof, nullptr);
    }
    // If all promoted, we don't need the MD.prof metadata.
    if (TotalCount == 0 || NumPromoted == NumVals)
      continue;
    // Otherwise we need update with the un-promoted records back.
    annotateValueSite(*F.getParent(), *CB, ICallProfDataRef.slice(NumPromoted),
                      TotalCount, IPVK_IndirectCallTarget, NumCandidates);
  }
  return Changed;
}

static void computeVirtualCallToTypeInfo(
    Module &M, ModuleAnalysisManager &MAM,
    VirtualCallToTypeInfoMapTy &VirtualCallToTypeInfo) {
  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  auto LookupDomTree = [&FAM](Function &F) -> DominatorTree & {
    return FAM.getResult<DominatorTreeAnalysis>(F);
  };
  // Look at users of llvm.type.test only.
  // This assumes thinlto and whole-program-devirtualization is enabled and
  // `llvm.public.type.test` is refined into `llvm.type.test` in prior passes in
  // the postlink optimizer pipeline
  // FIXME: Implement for type_checked_load and type_checked_load_relative.
  Function *TypeTestFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_test));

  if (!TypeTestFunc || TypeTestFunc->use_empty())
    return;

  // Iterate all type.test calls and find all indirect calls.
  for (Use &U : llvm::make_early_inc_range(TypeTestFunc->uses())) {
    auto *CI = dyn_cast<CallInst>(U.getUser());
    if (!CI)
      continue;

    auto *TypeMDVal = cast<MetadataAsValue>(CI->getArgOperand(1));
    if (!TypeMDVal)
      continue;

    auto *CompatibleTypeId = dyn_cast<MDString>(TypeMDVal->getMetadata());
    if (!CompatibleTypeId)
      continue;

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
          PGOIndirectCallVisitor::tryGetVTableInstruction(&CB);

      if (!VTableLoad)
        continue;

      // Does 'CB' uniquely identify an address-point-offset?
      // - yep, since CB is unique, and it calls one function.
      VirtualCallToTypeInfo[&CB] = {Offset, VTableLoad, CompatibleTypeStr,
                                    dyn_cast<Instruction>(CI)};
    }
  }
}

// A wrapper function that does the actual work.
static bool promoteIndirectCalls(Module &M, ProfileSummaryInfo *PSI, bool InLTO,
                                 bool SamplePGO, ModuleAnalysisManager &MAM) {
  if (DisableICP)
    return false;

  InstrProfSymtab Symtab;
  if (Error E = Symtab.create(M, InLTO)) {
    std::string SymtabFailure = toString(std::move(E));
    M.getContext().emitError("Failed to create symtab: " + SymtabFailure);
    return false;
  }

  // Keys are indirect calls that call virtual functions, and values are type
  // information.
  VirtualCallToTypeInfoMapTy VirtualCallToTypeInfo;

  computeVirtualCallToTypeInfo(M, MAM, VirtualCallToTypeInfo);

  bool Changed = false;
  for (auto &F : M) {
    if (F.isDeclaration() || F.hasOptNone())
      continue;

    auto &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);

    IndirectCallPromoter CallPromoter(F, &Symtab, SamplePGO,
                                      VirtualCallToTypeInfo, ORE);
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
