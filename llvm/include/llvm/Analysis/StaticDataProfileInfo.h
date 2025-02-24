#ifndef LLVM_ANALYSIS_STATICDATAPROFILEINFO_H
#define LLVM_ANALYSIS_STATICDATAPROFILEINFO_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Constant.h"
#include "llvm/Pass.h"

namespace llvm {

class StaticDataProfileInfo {
public:
  // Accummulate the profile count of a constant.
  // The constant can be a global variable or the constant pool value.
  DenseMap<const Constant *, uint64_t> ConstantProfileCounts;

public:
  StaticDataProfileInfo() = default;

  void addConstantProfileCount(const Constant *C,
                               std::optional<uint64_t> Count);

  std::optional<uint64_t> getConstantProfileCount(const Constant *C) const;
};

// This wraps the StaticDataProfileInfo object as an immutable pass.
class StaticDataProfileInfoWrapperPass : public ImmutablePass {
public:
  static char ID;
  StaticDataProfileInfoWrapperPass();
  bool doInitialization(Module &M) override;
  bool doFinalization(Module &M) override;
  void print(raw_ostream &OS, const Module *M = nullptr) const override;

  StaticDataProfileInfo &getStaticDataProfileInfo() { return *Info; }
  const StaticDataProfileInfo &getStaticDataProfileInfo() const {
    return *Info;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

private:
  std::unique_ptr<StaticDataProfileInfo> Info;
};

} // namespace llvm

#endif // LLVM_ANALYSIS_STATICDATAPROFILEINFO_H
