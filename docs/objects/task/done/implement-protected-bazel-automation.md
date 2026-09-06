---
type: task
tags: [task, bazel, github]
updated: 2026-09-06
status: done
summary: "Implement Make-owned CI entry points, protected workflows and environments, tag rulesets, cache policy, and signing trust policy."
task_of:
  - "[Operate Bazel through protected automation](../../story/done/operate-bazel-through-protected-automation.md)"
blocks:
  - "[Implement shared Bazel cache and buildset reuse](implement-shared-bazel-cache-and-buildset-reuse.md)"
---

# Implement Protected Bazel Automation

Add stable pull-request, protected-main, nightly, promotion, TestFlight, App Store, release, benchmark, and garbage-collection workflows. Pin actions, restrict permissions and release tags, protect locks and rules, keep private signing keys in protected environments, and measure private repository cost.
