# The project and direct component entry points share this exact gate because
# instruction-artifact-contributors.mk uses GNU Make 4 parse-time semantics.
override ORLIX_GNU_MAKE_CONTRACT_MAKE_VERSION_ORIGIN := $(origin MAKE_VERSION)
override ORLIX_GNU_MAKE_CONTRACT_MAJOR_ORIGIN := $(origin ORLIX_GNU_MAKE_CONTRACT_MAJOR)

ifneq ($(filter command line environment environment override,$(ORLIX_GNU_MAKE_CONTRACT_MAKE_VERSION_ORIGIN)),)
$(error Orlix root Make refuses caller override of MAKE_VERSION; select the version from the executing Make)
endif

ifneq ($(filter command line environment environment override,$(ORLIX_GNU_MAKE_CONTRACT_MAJOR_ORIGIN)),)
$(error Orlix root Make refuses caller override of ORLIX_GNU_MAKE_CONTRACT_MAJOR; select the version from the executing Make)
endif

override ORLIX_GNU_MAKE_CONTRACT_MAJOR := $(firstword $(subst ., ,$(MAKE_VERSION)))

ifeq ($(filter 0 1 2 3,$(ORLIX_GNU_MAKE_CONTRACT_MAJOR)),)

unexport ORLIX_GMAKE_CONTRACT_REEXEC
unexport ORLIX_ROOT_GMAKE_REEXEC
ORLIX_GNU_MAKE_CONTRACT_READY := 1

else

ORLIX_GMAKE ?= /opt/homebrew/bin/gmake

ifneq ($(filter 1,$(ORLIX_GMAKE_CONTRACT_REEXEC) $(ORLIX_ROOT_GMAKE_REEXEC)),)
$(error Orlix GNU Make re-exec guard tripped; require GNU Make >= 4.0)
endif

.DEFAULT_GOAL := __orlix-gnu-make-contract-reexec
.PHONY: __orlix-gnu-make-contract-reexec

$(MAKEFILE_LIST): ;

%: __orlix-gnu-make-contract-reexec ; @:

__orlix-gnu-make-contract-reexec:
	+@set -eu; \
	gmake='$(ORLIX_GMAKE)'; \
	if [ ! -x "$$gmake" ]; then \
		printf 'Orlix root Make missing required GNU Make >= 4.0 tool: %s; install Homebrew gmake with: brew bundle --file Brewfile\n' "$$gmake" >&2; \
		exit 2; \
	fi; \
	version="$$("$$gmake" --version 2>/dev/null | awk 'NR == 1 && $$1 == "GNU" && $$2 == "Make" { print $$3; exit }')"; \
	case "$$version" in [4-9].*|[1-9][0-9].*) ;; *) \
		printf 'Orlix root Make requires GNU Make >= 4.0 at %s; found %s\n' "$$gmake" "$${version:-unknown}" >&2; \
		exit 2;; \
	esac; \
	ORLIX_GMAKE_CONTRACT_REEXEC=1 ORLIX_ROOT_GMAKE_REEXEC=1 exec "$$gmake" -f "$(ORLIX_GNU_MAKE_CONTRACT_ENTRYPOINT)" $(MAKECMDGOALS)

endif
