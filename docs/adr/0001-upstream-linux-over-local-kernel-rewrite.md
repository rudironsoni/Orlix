# ADR 0001: Build Upstream Linux Instead Of Rewriting Linux Locally

## Status

Accepted

## Context

The repository historically contained an active local kernel prototype under `OrlixKernel/fs`, `OrlixKernel/kernel`, and `OrlixKernel/runtime`, later quarantined under `LegacyOrlix/`. That direction required Orlix to reimplement Linux core subsystems locally, so the prototype has been retired from the tracked source tree.

The product goal is real Linux userspace compatibility inside an iOS app boundary.

## Decision

Orlix will compile upstream Linux and adapt it through Linux-native extension points. Upstream Linux owns core Linux subsystems. The local kernel prototype is migration reference only and is not a target implementation path.

When upstream Linux already provides the surface or implementation approach for a problem, Orlix follows that Linux approach rather than inventing a parallel Orlix-specific surface.

## Consequences

New Linux subsystem behavior must not be added to a local prototype tree.

Useful behavior from the prototype may be migrated by ownership into upstream Linux-native paths under `OrlixKernel/Sources/ports/orlix/overlay`, `OrlixKernel/Sources/boot`, Linux drivers, or `OrlixHostAdapter/Sources` seams.

The active product tree has no `LegacyOrlix/`, `OrlixKernel/fs`, `OrlixKernel/kernel`, or `OrlixKernel/runtime` directories.
