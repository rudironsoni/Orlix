#include "OrlixAbstractionLeakageCheck.h"
#include "OrlixAbiSurfaceCheck.h"
#include "OrlixHostVocabularyCheck.h"
#include "OrlixIncludeBoundaryCheck.h"
#include "OrlixLoggingPolicyCheck.h"
#include "OrlixSourcePolicyCheck.h"
#include "OrlixTestPolicyCheck.h"
#include "OrlixTestVocabularyCheck.h"
#include "OrlixTypeOwnershipCheck.h"

#include "clang-tidy/ClangTidyModule.h"

namespace clang::tidy::orlix {

class OrlixTidyModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &Factories) override {
    Factories.registerCheck<OrlixAbstractionLeakageCheck>(
        "orlix-abstraction-leakage");
    Factories.registerCheck<OrlixAbiSurfaceCheck>("orlix-abi-surface");
    Factories.registerCheck<OrlixHostVocabularyCheck>(
        "orlix-host-vocabulary");
    Factories.registerCheck<OrlixIncludeBoundaryCheck>(
        "orlix-include-boundary");
    Factories.registerCheck<OrlixLoggingPolicyCheck>("orlix-logging-policy");
    Factories.registerCheck<OrlixSourcePolicyCheck>("orlix-source-policy");
    Factories.registerCheck<OrlixTestPolicyCheck>("orlix-test-policy");
    Factories.registerCheck<OrlixTestVocabularyCheck>(
        "orlix-test-vocabulary");
    Factories.registerCheck<OrlixTypeOwnershipCheck>(
        "orlix-type-ownership");
  }
};

static ClangTidyModuleRegistry::Add<OrlixTidyModule>
    X("orlix-module", "Orlix custom source policy checks.");

volatile int OrlixTidyModuleAnchorSource = 0;

} // namespace clang::tidy::orlix
