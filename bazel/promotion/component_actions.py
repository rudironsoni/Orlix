#!/usr/bin/env python3
"""Promoted execution proof. The promotion registry is not a denylist.

Callers must use origin-aware proof in promoted_execution.py. This module
forwards so old Make unittest names keep importing a real checker.
"""

from __future__ import annotations

from promoted_execution import PromotedExecutionError, main, prove_execution

ComponentActionError = PromotedExecutionError


if __name__ == "__main__":
    raise SystemExit(main())
