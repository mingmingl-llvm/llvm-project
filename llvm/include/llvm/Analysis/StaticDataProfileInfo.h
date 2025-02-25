#ifndef LLVM_ANALYSIS_STATICDATAPROFILEINFO_H
#define LLVM_ANALYSIS_STATICDATAPROFILEINFO_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Constant.h"
#include "llvm/Pass.h"

namespace llvm {

/// A class to hold the constants that represent static data and their profile
/// information, and provide methods to operate on them.
class StaticDataProfileInfo {
public:
  /// Accummulate the profile count of a constant.
  /// The constant can be a global variable or the constant pool value.
  DenseMap<const Constant *, uint64_t> ConstantProfileCounts;

public:
  StaticDataProfileInfo() = default;

  /// Add \p Count to the profile count of the constant \p C in a saturating
  /// way, and clamp the count to \p getInstrMaxCountValue if the result exceeds
  /// it.
  void addConstantProfileCount(const Constant *C,
                               std::optional<uint64_t> Count);

  /// If \p C has a count, return it. Otherwise, return std::nullopt.
  std::optional<uint64_t> getConstantProfileCount(const Constant *C) const;
};

/// This wraps the StaticDataProfileInfo object as an immutable pass, for a
/// backend pass to read or write the object.
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

  /// This pass provides StaticDataProfileInfo for reads/writes but does not
  /// modify \p M or other analysis. All analysis are preserved.
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

private:
  std::unique_ptr<StaticDataProfileInfo> Info;
};

} // namespace llvm

#endif // LLVM_ANALYSIS_STATICDATAPROFILEINFO_H
