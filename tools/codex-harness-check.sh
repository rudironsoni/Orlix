#!/usr/bin/env sh
set -eu

mode="${1:-all}"
root="$(cd "$(dirname "$0")/.." && pwd)"
failures=0

fail() {
  failures=$((failures + 1))
  printf '%s\n' "FAIL: $*" >&2
}

require_file() {
  test -f "$root/$1" || fail "missing file: $1"
}

check_hooks() {
  for hook in pre_tool_use_policy post_tool_use_review session_start_context stop_guard; do
    require_file ".codex/hooks/$hook"
    test -x "$root/.codex/hooks/$hook" || fail ".codex/hooks/$hook is not executable"
    grep -q "$hook" "$root/.codex/hooks.json" || fail ".codex/hooks.json does not configure $hook"
  done
  grep -q 'pre_tool_use_policy' "$root/.codex/hooks.json" || fail "pre_tool_use_policy hook is not wired"
}

check_subagents() {
  for agent in \
    tcti-planner \
    tcti-oracle-engineer \
    tcti-llvm-inspector \
    tcti-safety-reviewer \
    tcti-test-reducer \
    tcti-gadget-reviewer \
    tcti-release-gate-reviewer; do
    file="$root/.codex/subagents/$agent.md"
    test -f "$file" || { fail "missing subagent $agent"; continue; }
    for section in "## Purpose" "## Inputs" "## Allowed files" "## Forbidden files" "## Commands it may run" "## Required output format" "## Stop conditions"; do
      grep -q "^$section" "$file" || fail "$agent lacks section $section"
    done
    grep -q "$agent" "$root/.codex/config.toml" || fail ".codex/config.toml does not mention subagent $agent"
  done
}

check_skills() {
  for skill in orlix-tcti-next-step orlix-tcti-oracle orlix-tcti-safety orlix-tcti-debug; do
    file="$root/.agents/skills/$skill/SKILL.md"
    test -f "$file" || { fail "missing skill $skill"; continue; }
    sed -n '1,12p' "$file" | grep -q '^name: ' || fail "$skill lacks front-loaded name metadata"
    sed -n '1,12p' "$file" | grep -q '^description: .*TCTI' || fail "$skill lacks front-loaded TCTI description"
    test -f "$root/.agents/skills/$skill/agents/openai.yaml" || fail "$skill lacks agents/openai.yaml dependencies"
  done
  grep -q 'tcti-planner' "$root/.agents/skills/orlix-tcti-next-step/SKILL.md" || fail "next-step skill does not route through planner"
  grep -q 'tcti-safety-reviewer' "$root/.agents/skills/orlix-tcti-next-step/SKILL.md" || fail "next-step skill does not route through safety reviewer"
}

check_mcp() {
  for server in openaiDeveloperDocs context7 lldb orlix-tcti; do
    grep -q "\\[mcp_servers\\.$server\\]" "$root/.codex/config.toml" ||
      grep -q "\\[mcp_servers\\.\"$server\"\\]" "$root/.codex/config.toml" ||
      fail ".codex/config.toml lacks MCP server $server"
  done
  require_file "tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp"
  require_file "tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp.swift"
  require_file "tools/mcp/orlix-llvm-mcp-wrapper/lldb-mcp-wrapper.sh"
  test -x "$root/tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp" || fail "Orlix TCTI MCP launcher is not executable"
  test -x "$root/tools/mcp/orlix-llvm-mcp-wrapper/lldb-mcp-wrapper.sh" || fail "LLDB MCP wrapper is not executable"
  for tool in tcti_status tcti_next tcti_report_read tcti_reproducer_read tcti_golden_list tcti_golden_validate tcti_safety_audit tcti_plan_consistency; do
    grep -q "$tool" "$root/tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp.swift" || fail "Orlix TCTI MCP does not expose $tool"
  done
  if grep -Eiq 'generic shell|arbitrary shell|shell_exec|/bin/sh|bash -lc|zsh -lc|command.*String' "$root/tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp.swift"; then
    fail "Orlix TCTI MCP appears to expose generic shell behavior"
  fi
  swift -frontend -parse "$root/tools/mcp/orlix-tcti-mcp/orlix-tcti-mcp.swift" >/dev/null
}

check_agents_doc() {
  grep -q 'Orlix TCTI Codex harness' "$root/AGENTS.md" || fail "AGENTS.md does not mention Orlix TCTI Codex harness"
  grep -q 'orlix-tcti-next-step' "$root/AGENTS.md" || fail "AGENTS.md does not mention orlix-tcti-next-step"
  grep -q 'planner and safety reviewer' "$root/AGENTS.md" || fail "AGENTS.md does not require planner and safety reviewer"
  grep -q 'physical device' "$root/AGENTS.md" || fail "AGENTS.md does not mention physical device preflight"
  grep -q 'switch-debug oracle' "$root/AGENTS.md" || fail "AGENTS.md does not mention switch-debug oracle coverage"
}

case "$mode" in
  all|codex-harness-check)
    check_hooks
    check_subagents
    check_skills
    check_mcp
    check_agents_doc
    ;;
  hooks|codex-hooks-check)
    check_hooks
    ;;
  skills|codex-skills-check)
    check_skills
    ;;
  subagents|codex-subagents-check)
    check_subagents
    ;;
  mcp|codex-mcp-check)
    check_mcp
    ;;
  *)
    printf '%s\n' "usage: $0 [all|hooks|skills|subagents|mcp]" >&2
    exit 2
    ;;
esac

if test "$failures" -ne 0; then
  exit 1
fi

printf '%s\n' "pass: $mode"
