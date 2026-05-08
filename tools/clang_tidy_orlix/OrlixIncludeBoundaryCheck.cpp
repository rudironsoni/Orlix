#include "OrlixIncludeBoundaryCheck.h"

#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

#include <string>
#include <vector>

namespace clang::tidy::orlix {

namespace {

bool pathHasComponent(llvm::StringRef Path, llvm::StringRef Needle) {
  return Path.contains(Needle);
}

class IncludeBoundaryPPCallbacks : public PPCallbacks {
public:
  IncludeBoundaryPPCallbacks(OrlixIncludeBoundaryCheck &Check,
                             llvm::StringRef MainFilePath)
      : Check(Check), MainFilePath(MainFilePath.str()) {}

  void InclusionDirective(SourceLocation HashLoc, const Token &, StringRef FileName,
                          bool IsAngled, CharSourceRange, OptionalFileEntryRef,
                          StringRef, StringRef, const Module *, bool,
                          SrcMgr::CharacteristicKind) override {
    std::string IncludeText =
        std::string(IsAngled ? "<" : "\"") + FileName.str() +
        std::string(IsAngled ? ">" : "\"");

    if (FileName.starts_with("Foundation/") || FileName.starts_with("UIKit/") ||
        FileName.starts_with("CoreFoundation/") ||
        FileName.starts_with("CoreServices/") ||
        FileName.starts_with("CoreGraphics/") ||
        FileName.starts_with("TargetConditionals") ||
        FileName.starts_with("dispatch") || FileName.starts_with("os/")) {
      Check.diag(HashLoc,
                 "host framework imports are forbidden in Linux-owner code");
    }

    static const std::vector<std::string> ForbiddenHeaders = {
        "pthread.h",            "sys/sysctl.h", "mach/",
        "objc/",                "dispatch/",    "os/log.h",
        "TargetConditionals.h", "Foundation/",  "UIKit/",
        "CoreFoundation/"};

    for (const auto &Header : ForbiddenHeaders) {
      if (FileName == Header || FileName.starts_with(Header)) {
        Check.diag(HashLoc,
                   "forbidden host header is included from Linux-owner code");
      }
    }

    if (FileName.starts_with("OrlixHostAdapter/") ||
        IncludeText.find("OrlixHostAdapter/") != std::string::npos) {
      Check.diag(HashLoc,
                 "Linux-owner code must not include OrlixHostAdapter headers");
    }

    if (FileName.starts_with("OrlixMLibC/") ||
        FileName.starts_with("orlixmlibc/") ||
        IncludeText.find("OrlixMLibC/") != std::string::npos) {
      Check.diag(HashLoc,
                 "Linux-owner code must not include OrlixMLibC headers");
    }

    if (FileName.contains("internal/ios")) {
      if (Check.isKernelPublicHeaderPath(MainFilePath)) {
        Check.diag(HashLoc,
                   "public headers in OrlixKernel/include must not depend on internal/ios");
      } else {
        Check.diag(HashLoc,
                   "Linux-owner code must not include internal/ios mediation headers");
      }
    }
  }

private:
  OrlixIncludeBoundaryCheck &Check;
  std::string MainFilePath;
};

} // namespace

OrlixIncludeBoundaryCheck::OrlixIncludeBoundaryCheck(llvm::StringRef Name,
                                                       ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

bool OrlixIncludeBoundaryCheck::isLinuxOwnerPath(llvm::StringRef Path) const {
  return pathHasComponent(Path, "OrlixKernel/fs/") ||
         pathHasComponent(Path, "OrlixKernel/kernel/") ||
         pathHasComponent(Path, "OrlixKernel/runtime/") ||
         pathHasComponent(Path, "OrlixKernel/include/");
}

bool OrlixIncludeBoundaryCheck::isKernelPublicHeaderPath(
    llvm::StringRef Path) const {
  return pathHasComponent(Path, "OrlixKernel/include/");
}

void OrlixIncludeBoundaryCheck::registerPPCallbacks(const SourceManager &SM,
                                                     Preprocessor *PP,
                                                     Preprocessor *) {
  StringRef Path = getCurrentMainFile();
  if (!isLinuxOwnerPath(Path))
    return;
  PP->addPPCallbacks(
      std::make_unique<IncludeBoundaryPPCallbacks>(*this, Path));
}

} // namespace clang::tidy::orlix
