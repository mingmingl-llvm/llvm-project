#include "llvm/Transforms/IPO/TypeRefinePass.h"

using namespace llvm;

#define DEBUG_TYPE "typrefine"

namespace llvm {

// TODO: Register this pass to opt.

PreservedAnalyses TypeRefinePass::run(Module &M, ModuleAnalysisManager &AM) {

  // TODO; It's a hack to find out virtual calls by looking at 'this' pointer.
  return PreservedAnalyses::all();
}
} // namespace llvm
