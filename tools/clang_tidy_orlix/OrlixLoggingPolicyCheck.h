#ifndef ORLIX_TIDY_LOGGING_POLICY_CHECK_H
#define ORLIX_TIDY_LOGGING_POLICY_CHECK_H

#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::orlix {

class OrlixLoggingPolicyCheck : public ClangTidyCheck {
public:
  OrlixLoggingPolicyCheck(llvm::StringRef Name, ClangTidyContext *Context);

  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  bool isLinuxOwnerPath(llvm::StringRef Path) const;
};

} // namespace clang::tidy::orlix

#endif
