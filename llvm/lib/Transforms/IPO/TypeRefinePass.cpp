#include "llvm/Transforms/IPO/TypeRefinePass.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

using namespace llvm;

#define DEBUG_TYPE "typrefine"

namespace llvm {

// TODO: Register this pass to opt.

bool TypeRefinePass::visitTypeIntrinsicUser(Module &M,
                                            Function *IntrinsicFunc) {
  if (!IntrinsicFunc)
    return false;

  for (User *U : IntrinsicFunc->users()) {
    if (auto *CI = dyn_cast<CallInst>(U)) {
      Value *V = CI->getArgOperand(0);

      auto *TypeMDVal = cast<MetadataAsValue>(CI->getArgOperand(1));
      assert(TypeMDVal != nullptr && "Type metadata should not be null");

      auto *TypeMD = cast<MDString>(TypeMDVal->getMetadata());
      assert(TypeMD != nullptr && "Type metadata should not be null");

      StringRef TypeName = TypeMD->getString();
      if (auto *I = dyn_cast<LoadInst>(V)) {
        Value *Ptr = I->getPointerOperand();
        errs() << "Found a vtable load: " << *I << "\n";
        errs() << Ptr->getName() << "\n";

        ValueMap[Ptr].insert(TypeName);
      }
    }
  }
  return true;
}

void TypeRefinePass::printValueMap() const {
  for (auto &Pair : ValueMap) {
    Value *V = Pair.first;
    errs() << "Vtable load: " << *V << "\n";
    for (auto &TypeName : Pair.second) {
      errs() << "  Type: " << TypeName << "\n";
    }
  }
}

PreservedAnalyses TypeRefinePass::run(Module &M, ModuleAnalysisManager &AM) {

  bool Changed = visitTypeIntrinsicUser(
      M, Intrinsic::getDeclarationIfExists(&M, Intrinsic::type_test));
  Changed |= visitTypeIntrinsicUser(
      M, Intrinsic::getDeclarationIfExists(&M, Intrinsic::public_type_test));
  // TODO; It's a hack to find out virtual calls by looking at 'this' pointer.
  if (Changed) {
    printValueMap();
  }
  return PreservedAnalyses::all();
}
} // namespace llvm
