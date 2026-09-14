---
root: true
targets: ['*']
globs: ['**/*']
---
# Orlix

Read `docs/index.md` and the active task envelope under `Build/AgentHarness/agent/`. Read only the canonical pages and paths selected by that envelope. Use `docs/ontology.md` and `docs/AGENTS.md` for durable knowledge changes.

Orlix is the first-party app. OrlixKit is its public Linux runtime SDK. OrlixEngine hosts one OrlixOS with one upstream Linux kernel. OrlixOS hosts OrlixInstances, OrlixProcesses, and OrlixContainers. ADR 0040 owns this vocabulary.

Upstream Linux owns Linux behavior. OrlixTCTI owns guest instruction execution. OrlixHostAdapter owns private Darwin mechanics only. Herdr owns terminal topology. Guest artifacts never become Apple-native link dependencies. Guest ELF text stays host data and product paths must not create executable code at runtime.

Make is the repository interface. Bazel owns the product graph after cutover. Upstream build systems retain internal ownership. The ontology owns durable work meaning. `.rulesync/` owns portable agent configuration. RuleSync owns client serialization. GitHub CI owns generated-output enforcement. `Tools/AgentHarness/` owns Orlix task and lifecycle semantics. `Build/AgentHarness/` owns current state and evidence. Do not edit generated agent destinations.

Canonical RuleSync source changes through normal pull requests. After merge, the trusted main-triggered RuleSync generation workflow generates client output on a dedicated automation branch, opens a generated-output pull request, and invokes its exact verifier. The source revision must be an ancestor of protected `main`, and the verifier rejects the generated pull request if canonical RuleSync inputs changed later on `main`. The generation workflow publishes the required status only after independent regeneration matches exactly. The normal pull-request guard accepts generated output only when that exact status identifies the trusted generator run for the same current `main` source revision. GitHub auto-merge merges the generated pull request after all required checks pass. No actor bypasses the main-branch ruleset. Generated client output is never a canonical project input.

Use the skills named by the task envelope. Product proof follows ADR 0017: kernel dependency, kselftest, mlibc, OrlixMLibC syscall and UAPI, POSIX shell, then jq, curl, and zsh. One tier does not prove a later tier. Physical TCTI work requires the three `AREA=orlix-tcti` Make gates, current simulator-ladder evidence, and `physical_device_allowed=true`.

Start at most four subagents in one session. Use high reasoning for planning, review, TCTI inspection, architecture, and hard proof. Use normal reasoning for implementation and mechanical inspection.
