#ifndef LLVM_ANALYSIS_STATICDATAPROFILEINFO_H
#define LLVM_ANALYSIS_STATICDATAPROFILEINFO_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Constant.h"
#include "llvm/Pass.h"

namespace llvm {

/// A class that holds the constants that represent static data and their
/// profile information and provides methods to operate on them.
class StaticDataProfileInfo {
public:
  /// Accummulate the profile count of a constant that will be lowered to static
  /// data sections.
  DenseMap<const Constant *, uint64_t> ConstantProfileCounts;

  /// Keeps track of the constants that are seen at least once without profile
  /// counts.
  DenseSet<const Constant *> ConstantWithoutCounts;

public:
  StaticDataProfileInfo() = default;

  /// Add \p Count to the profile count of the constant \p C in a saturating
  /// way, and clamp the count to \p getInstrMaxCountValue if the result exceeds
  /// it.
  void addConstantProfileCount(const Constant *C,
                               std::optional<uint64_t> Count);

  /// If \p C has a count, return it. Otherwise, return std::nullopt.
  std::optional<uint64_t> getConstantProfileCount(const Constant *C) const;

  bool hasUnknownCount(const Constant *C) const {
    return ConstantWithoutCounts.count(C);
  }
};

/// This wraps the StaticDataProfileInfo object as an immutable pass, for a
/// backend pass to read or write the object.
class StaticDataProfileInfoWrapperPass : public ImmutablePass {
public:
  static char ID;
  StaticDataProfileInfoWrapperPass();
  bool doInitialization(Module &M) override;
  bool doFinalization(Module &M) override;

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
