# Signal Architecture Correction - Results

## Summary
Fixed signal subsy stem ownership: kernel/signal.c now owns the public Linux-facing contract, arch/darwin/signal_bridge.c contains only private bridge helpers.

## Changes Made

### 1. kernel/signal.c
**Before:** Only contained internal signal logic, no public wrappers  
**After:** Now exports all public canonical signal wrappers:
- `sigaction` (calls bridge helpers for Darwin conversion)
- `signal`
- `sigprocmask`
- `sigpending`
- `sigsuspend`
- `kill`
- `killpg`
- `raise`
- `pause`
- Plus internal `do_*` implementations

### 2. arch/darwin/signal_bridge.c  
**Before:** Exported public signal wrappers (sigaction, signal, etc.)
**After:** Contains only private bridge helpers:
- `bridge_sigset_from_host()` - Darwin sigset_t → internal
- `bridge_sigset_to_host()` - internal → Darwin sigset_t
- `bridge_signal_from_host()` - Darwin sigaction → internal
- `bridge_signal_to_host()` - internal → Darwin sigaction

### 3. Symbol Ownership Verification

**Verify public symbols are in signal.o:**
```bash
signal.o: _sigaction, _signal, _sigprocmask, _sigpending, _sigsuspend
signal.o: _kill, _killpg, _raise, _pause
```

**Verify bridge has no public symbols:**
```bash
signal_bridge.o has NO: _sigaction, _signal, _sigprocmask, etc.
```

## Architecture Compliance

✓ `kernel/signal.h` - True private internal header
✓ `kernel/signal.c` - Owns public Linux-facing contract  
✓ `arch/darwin/signal_bridge.c` - Private host bridge only
✓ No deleted headers resurrected
✓ No branded naming (ixland_, ix_, IX_)
✓ Build green with warnings-as-errors

## Files Changed
- kernel/signal.c (added public wrappers, use bridge helpers)
- arch/darwin/signal_bridge.c (removed public wrappers, kept private helpers)

## Status
BUILD GREEN - ARCHITECTURE CORRECT
