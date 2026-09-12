---
type: task
tags: [task, bazel, github]
updated: 2026-09-10
status: doing
summary: "Implement Make-owned CI entry points, protected workflows and environments, tag rulesets, cache policy, and signing trust policy."
task_of:
  - "[Operate Bazel through protected automation](../../story/doing/operate-bazel-through-protected-automation.md)"
blocks:
  - "[Implement shared Bazel cache and buildset reuse](../doing/implement-shared-bazel-cache-and-buildset-reuse.md)"
---

# Implement Protected Bazel Automation

Add stable pull-request, protected-main, nightly, promotion, TestFlight, App Store, release, benchmark, and garbage-collection workflows. Pin actions, restrict permissions and release tags, protect locks and rules, keep private signing keys in protected environments, and measure private repository cost.

Promotion publishes a signed component proposal as a workflow artifact. Lock proposals consume successful promotion runs from the same main commit and verify the component signatures before assembling the buildset. Nightly reconstruction compares fresh source-built UAPI, mlibc, and rootfs trees with the verified signed artifacts. Missing keys fail these gates.

The Bazel matrix check runs on every pull request and every push to `main`, including changes to application, kernel, libc, and userspace sources. Workflow path filters must not exclude those changes or leave the required check absent.

The component benchmark workflow calls `make __bazel-cache-equivalence` on `main`. It retains Bazel build events, timing profiles, execution logs, and full-tree comparison evidence for the seed, cached, and uncached builds. The workflow has read-only repository access, a three-hour limit, and fourteen-day evidence retention. It does not measure app edits, branch switches, indexing, or release performance; those remain cutover requirements.
