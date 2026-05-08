#ifndef ORLIX_TIDY_TEST_VOCABULARY_CHECK_H
#define ORLIX_TIDY_TEST_VOCABULARY_CHECK_H

#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::orlix {

class OrlixTestVocabularyCheck : public ClangTidyCheck {
public:
  OrlixTestVocabularyCheck(llvm::StringRef Name, ClangTidyContext *Context);

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void onEndOfTranslationUnit() override;

private:
  bool isKernelTestPath(llvm::StringRef Path) const;
  bool isHostTestPath(llvm::StringRef Path) const;
  bool isAnyTestPath(llvm::StringRef Path) const;
  void scanMainFile();

  const SourceManager *CurrentSM = nullptr;
};

} // namespace clang::tidy::orlix

#endif
