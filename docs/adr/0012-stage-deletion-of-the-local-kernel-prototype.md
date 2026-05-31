# ADR 0012: Stage Deletion Of The Local Kernel Prototype

## Status

Accepted

## Context

The repository historically contained local kernel prototype directories. Deleting them immediately would have discarded behavior notes before target equivalents existed. Keeping them active would have preserved the wrong architecture.

## Decision

Deletion was staged. New work there was forbidden immediately. Useful behavior could be migrated by ownership. Remaining prototype directories are deleted after migration or intentional retirement.

## Consequences

Legacy prototype material has been retired from the tracked source tree. `LegacyOrlix/`, `OrlixKernel/fs`, `OrlixKernel/kernel`, and `OrlixKernel/runtime` should stay absent from the active product tree; active OrlixKernel work belongs under `OrlixKernel/Sources`.

XCTest coverage that targeted those local-kernel directories is also retired as proof. Linux subsystem assertions should move to KUnit or kselftest under `OrlixKernel/Sources/ports/orlix/overlay`, while retained XCTest should cover iOS-hosted Orlix launch, Linux test-output collection, packaging, or narrow host mechanics under the owning project's `Tests` tree.

Documentation and agent rules must stop presenting those paths as Linux owners.

Final cleanup has removed the directories completely.
