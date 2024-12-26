

#ifndef LLVM_TRANSFORMS_IPO_TYPEREFINE_H
#define LLVM_TRANSFORMS_IPO_TYPEREFINE_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

/// Optimize globals that never have their address taken.
class TypeRefinePass : public PassInfoMixin<TypeRefinePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  // Iterate all users of the given type intrinsic, and build a value map.
  bool visitTypeIntrinsicUser(Module &M, Function *IntrinsicFunc);

  void printValueMap() const;

  // Key is the vtable-load instruction, value is a set of type names.
  DenseMap<Value *, DenseSet<StringRef>> ValueMap;
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_IPO_TYPEREFINE_H
