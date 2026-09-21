from __future__ import annotations

import unittest

import component_actions
import promoted_execution


class ComponentActionForwardTests(unittest.TestCase):
    def test_old_name_forwards_to_origin_aware_proof(self) -> None:
        self.assertIs(component_actions.prove_execution, promoted_execution.prove_execution)
        self.assertFalse(hasattr(component_actions, "count_component_actions"))


if __name__ == "__main__":
    unittest.main()
