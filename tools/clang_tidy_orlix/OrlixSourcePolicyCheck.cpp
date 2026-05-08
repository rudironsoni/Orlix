#include "OrlixSourcePolicyCheck.h"

#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/StringRef.h"

#include <regex>
#include <string>
#include <vector>

namespace clang::tidy::orlix {

namespace {

struct RegexRule {
  const char *Pattern;
  const char *Message;
};

bool pathHasComponent(llvm::StringRef Path, llvm::StringRef Needle) {
  return Path.contains(Needle);
}

bool hasExtension(llvm::StringRef Path, llvm::StringRef Ext) {
  return Path.ends_with(Ext);
}

SourceLocation translateLocation(const SourceManager &SM, FileID FID,
                                 unsigned Line, unsigned Column) {
  return SM.translateLineCol(FID, Line, Column);
}

void scanLines(ClangTidyCheck &Check, const SourceManager &SM, StringRef Buffer,
               StringRef Path,
               const std::vector<RegexRule> &Rules) {
  FileID FID = SM.getMainFileID();
  size_t Start = 0;
  unsigned LineNo = 1;
  while (Start <= Buffer.size()) {
    size_t End = Buffer.find('\n', Start);
    if (End == StringRef::npos)
      End = Buffer.size();
    std::string Line(Buffer.slice(Start, End).str());
    for (const auto &Rule : Rules) {
      std::smatch Match;
      if (std::regex_search(Line, Match, std::regex(Rule.Pattern))) {
        unsigned Column = static_cast<unsigned>(Match.position() + 1);
        Check.diag(translateLocation(SM, FID, LineNo, Column), Rule.Message);
      }
    }
    if (End == Buffer.size())
      break;
    Start = End + 1;
    ++LineNo;
  }

  (void)Path;
}

const std::vector<RegexRule> SourceRules = {
    {R"(\b(fdopendir|readdir|closedir|seekdir|telldir)\s*\()",
     "host directory iteration APIs are forbidden in Linux-owner code"},
    {R"(^\s*#\s*include\s*<dirent\.h>)",
     "host dirent headers are forbidden in Linux-owner code"},
    {R"(\b(dlsym|RTLD_NEXT|RTLD_DEFAULT|dlopen|pthread_[a-z_]+|objc_[a-z_]+|mach_[a-z_]+|os_log)\b)",
     "forbidden host APIs/tokens found in Linux-owner code"},
    {R"(\b(__APPLE__|__MACH__|TARGET_OS_[A-Z0-9_]+)\b)",
     "Darwin/iOS platform tokens are forbidden in Linux-owner code"},
};

} // namespace

OrlixSourcePolicyCheck::OrlixSourcePolicyCheck(llvm::StringRef Name,
                                                 ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

bool OrlixSourcePolicyCheck::isLinuxOwnerPath(llvm::StringRef Path) const {
  return pathHasComponent(Path, "OrlixKernel/fs/") ||
         pathHasComponent(Path, "OrlixKernel/kernel/") ||
         pathHasComponent(Path, "OrlixKernel/runtime/") ||
         pathHasComponent(Path, "OrlixKernel/include/");
}

void OrlixSourcePolicyCheck::registerPPCallbacks(
    const SourceManager &SM, Preprocessor *, Preprocessor *) {
  StringRef Path = getCurrentMainFile();
  if (!isLinuxOwnerPath(Path))
    return;
  CurrentSM = &SM;
}

void OrlixSourcePolicyCheck::onEndOfTranslationUnit() { scanMainFile(); }

void OrlixSourcePolicyCheck::scanMainFile() {
  if (!CurrentSM)
    return;
  const SourceManager &SM = *CurrentSM;
  FileID FID = SM.getMainFileID();
  auto Entry = SM.getFileEntryRefForID(FID);
  if (!Entry)
    return;
  StringRef Path = Entry->getName();
  if (!isLinuxOwnerPath(Path))
    return;

  if (hasExtension(Path, ".m") || hasExtension(Path, ".mm")) {
    diag(SM.getLocForStartOfFile(FID),
         "Objective-C files are forbidden in Linux-owner paths");
  }

  StringRef Buffer = SM.getBufferData(FID);
  scanLines(*this, SM, Buffer, Path, SourceRules);
}

} // namespace clang::tidy::orlix
