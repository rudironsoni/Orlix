#ifndef ORLIX_TIDY_SOURCE_POLICY_CHECK_H
#define ORLIX_TIDY_SOURCE_POLICY_CHECK_H

#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::orlix {

class OrlixSourcePolicyCheck : public ClangTidyCheck {
public:
  OrlixSourcePolicyCheck(llvm::StringRef Name, ClangTidyContext *Context);

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void onEndOfTranslationUnit() override;

private:
  bool isLinuxOwnerPath(llvm::StringRef Path) const;
  void scanMainFile();
  const SourceManager *CurrentSM = nullptr;
};

} // namespace clang::tidy::orlix

#endif
