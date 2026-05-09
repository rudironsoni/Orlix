#include "OrlixLoggingPolicyCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/StringRef.h"

namespace clang::tidy::orlix {

using namespace clang::ast_matchers;

namespace {

constexpr llvm::StringLiteral BannedLoggingFunctions[] = {
    "printf",  "fprintf", "vprintf", "vfprintf", "puts",
    "fputs",   "putc",    "putchar", "perror",   "NSLog",
};

bool pathHasComponent(llvm::StringRef Path, llvm::StringRef Needle) {
  return Path.contains(Needle);
}

} // namespace

OrlixLoggingPolicyCheck::OrlixLoggingPolicyCheck(llvm::StringRef Name,
                                                   ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

bool OrlixLoggingPolicyCheck::isLinuxOwnerPath(llvm::StringRef Path) const {
  return pathHasComponent(Path, "OrlixKernel/fs/") ||
         pathHasComponent(Path, "OrlixKernel/kernel/") ||
         pathHasComponent(Path, "OrlixKernel/runtime/") ||
         pathHasComponent(Path, "OrlixKernel/include/") ||
         pathHasComponent(Path, "OrlixKernel/internal/");
}

void OrlixLoggingPolicyCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(
          callee(functionDecl(hasAnyName("printf", "fprintf", "vprintf",
                                         "vfprintf", "puts", "fputs", "putc",
                                         "putchar", "perror", "NSLog"))
                     .bind("callee")))
          .bind("call"),
      this);
}

void OrlixLoggingPolicyCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
  const auto *Callee = Result.Nodes.getNodeAs<FunctionDecl>("callee");
  if (!Call || !Callee || !Result.SourceManager)
    return;

  SourceLocation Loc = Call->getExprLoc();
  if (Loc.isInvalid() || Result.SourceManager->isInSystemHeader(Loc))
    return;

  auto Entry = Result.SourceManager->getFileEntryRefForID(
      Result.SourceManager->getFileID(Loc));
  if (!Entry)
    return;

  llvm::StringRef Path = Entry->getName();
  if (!isLinuxOwnerPath(Path))
    return;

  diag(Loc,
       "forbidden logging/debug call '%0' found in Linux-owner product code; "
       "route observability through owned kernel instrumentation instead")
      << Callee->getName();
}

} // namespace clang::tidy::orlix
