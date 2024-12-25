

#ifndef LLVM_TRANSFORMS_IPO_TYPEREFINE_H
#define LLVM_TRANSFORMS_IPO_TYPEREFINE_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

/// Optimize globals that never have their address taken.
class TypeRefinePass : public PassInfoMixin<TypeRefinePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_IPO_TYPEREFINE_H
