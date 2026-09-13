# Bazel procedure

Use the selected task's Make target for builds and tests. Use direct read-only Bazel inspection only through the Bazel inspector role.

- `bazel query` shows configured target relationships.
- `bazel cquery` shows configured targets, providers, transitions, and selected artifacts.
- `bazel aquery` shows actions, inputs, outputs, mnemonics, environment, and command lines.
- BEP records build events and artifact references. Execution logs record action execution. Profiles record phase time, critical path, and resource use.
- Compare cold, warm, source, and promoted modes with the same target, profile, destination, and revision.
- A cache miss can slow a build but cannot change its product. Mutable output bases and incremental directories remain worktree-local. Shared caches contain only immutable content-addressed or dependency-checked data with bounded retention.
- Keep action identity, incremental-directory identity, artifact identity, content identity, and proof identity separate. A proof-only change must not invalidate compilation unless it changes the product.
- Consumers select semantic artifacts, not every producer output. Promoted mode consumes verified signed buildsets. Source mode remains the independent reconstruction path.

Record the command, exit code, repository revision, target, profile, destination, artifact identity, cache mode, BEP or profile path, relevant failure, and evidence identity. Keep raw logs outside model context.
