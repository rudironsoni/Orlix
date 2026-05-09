#include "OrlixTypeOwnershipCheck.h"

#include "clang/Lex/Preprocessor.h"

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

SourceLocation translateLocation(const SourceManager &SM, FileID FID,
                                 unsigned Line, unsigned Column) {
  return SM.translateLineCol(FID, Line, Column);
}

void scanLines(ClangTidyCheck &Check, const SourceManager &SM, StringRef Buffer,
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
}

const std::vector<RegexRule> TypeOwnershipRules = {
    {R"(^\s*typedef\s+__INT(8|16|32|64)_TYPE__\s+(u?int(8|16|32|64)_t)\s*;)",
     "repo-local Linux-owner type packs are forbidden"},
    {R"(^\s*typedef\s+__UINT(8|16|32|64)_TYPE__\s+(u?int(8|16|32|64)_t)\s*;)",
     "repo-local Linux-owner type packs are forbidden"},
    {R"(^\s*typedef\s+__SIZE_TYPE__\s+size_t\s*;)",
     "repo-local Linux-owner type packs are forbidden"},
    {R"(\blinux_bool_t\b)",
     "synthetic linux_* scalar aliases are forbidden"},
    {R"(\blinux_atomic_int\b)",
     "synthetic linux_* scalar aliases are forbidden"},
    {R"(\b(?:struct|typedef)\s+linux_(timespec|timeval|timezone|itimerval|itimerspec|sockaddr|socklen|fd_set|termios|winsize|stat|statfs|iovec|msghdr|mmsghdr|ucred)\b)",
     "repo-local linux_* stand-ins for Linux concepts are forbidden; use the real Linux contract name instead"},
    {R"(\b(?:struct|typedef)\s+kernel_(timespec|timeval|timezone|itimerval|itimerspec|sockaddr|socklen|fd_set|termios|winsize|stat|statfs|iovec|msghdr|mmsghdr|ucred)\b)",
     "repo-local kernel_* stand-ins for Linux concepts are forbidden; use the real Linux contract name instead"},
    {R"(^\s*typedef\b.*\b(pid_t|uid_t|gid_t|mode_t|dev_t|ino_t|nlink_t|socklen_t|sa_family_t|suseconds_t)\b)",
     "repo-local libc-owned typedef declarations are forbidden in Linux-owner code"},
    {R"(^\s*(?:typedef\s+)?struct\s+(sigval|sigevent|statvfs|termios|winsize|iovec|msghdr|cmsghdr|mmsghdr)\s*(?:[;{]|$))",
     "repo-local libc-owned struct declarations are forbidden in Linux-owner code"},
};

} // namespace

OrlixTypeOwnershipCheck::OrlixTypeOwnershipCheck(llvm::StringRef Name,
                                                   ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

bool OrlixTypeOwnershipCheck::isLinuxOwnerPath(llvm::StringRef Path) const {
  return pathHasComponent(Path, "OrlixKernel/fs/") ||
         pathHasComponent(Path, "OrlixKernel/kernel/") ||
         pathHasComponent(Path, "OrlixKernel/runtime/") ||
         pathHasComponent(Path, "OrlixKernel/include/") ||
         pathHasComponent(Path, "OrlixKernel/internal/");
}

void OrlixTypeOwnershipCheck::registerPPCallbacks(const SourceManager &SM,
                                                   Preprocessor *,
                                                   Preprocessor *) {
  StringRef Path = getCurrentMainFile();
  if (!isLinuxOwnerPath(Path))
    return;
  CurrentSM = &SM;
}

void OrlixTypeOwnershipCheck::onEndOfTranslationUnit() { scanMainFile(); }

void OrlixTypeOwnershipCheck::scanMainFile() {
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

  StringRef Buffer = SM.getBufferData(FID);
  scanLines(*this, SM, Buffer, TypeOwnershipRules);
}

} // namespace clang::tidy::orlix
