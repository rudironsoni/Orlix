#ifndef ORLIX_TIDY_ABI_SURFACE_CHECK_H
#define ORLIX_TIDY_ABI_SURFACE_CHECK_H

#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::orlix {

class OrlixAbiSurfaceCheck : public ClangTidyCheck {
public:
  OrlixAbiSurfaceCheck(llvm::StringRef Name, ClangTidyContext *Context);

  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  bool isLinuxOwnerPath(llvm::StringRef Path) const;
};

} // namespace clang::tidy::orlix

#endif
