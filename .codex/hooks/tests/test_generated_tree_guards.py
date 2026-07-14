#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


HOOK_DIR = Path(__file__).resolve().parents[1]
PRE_TOOL_GUARD = HOOK_DIR / "pre_tool_use_guard.py"
PERMISSION_GUARD = HOOK_DIR / "permission_request_guard.py"


def run_hook(script, payload):
    return subprocess.run(
        [sys.executable, str(script)],
        input=json.dumps(payload),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def run_hook_in_repo(script, payload, policy):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        subprocess.run(["git", "init"], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        subprocess.run(
            ["git", "-c", "user.name=Hook Test", "-c", "user.email=hook@example.invalid", "commit", "--allow-empty", "-m", "fixture"],
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        policy = dict(policy)
        policy.setdefault(
            "git_sha",
            subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        )
        envelope = root / "Build" / "AgentHarness" / "orlix-tcti" / "next-task.json"
        envelope.parent.mkdir(parents=True)
        envelope.write_text(json.dumps(policy))
        return subprocess.run(
            [sys.executable, str(script)],
            input=json.dumps(payload),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=root,
            check=False,
        )


def bash_payload(command):
    return {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": command},
    }


def exec_command_payload(command, tool_name="exec_command"):
    return {
        "hook_event_name": "PreToolUse",
        "tool_name": tool_name,
        "tool_input": {"cmd": command},
    }


class GeneratedTreeGuardTests(unittest.TestCase):
    @staticmethod
    def tcti_runtime_path():
        return "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/" + "hosted_exec/tcti/exec.c"

    def test_pre_tool_guard_allows_user_directed_tcti_runtime_write_without_autonomous_authorization(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            {
                "hook_event_name": "PreToolUse",
                "tool_name": "apply_patch",
                "tool_input": {
                    "patch": "*** Update File: " + self.tcti_runtime_path()
                },
            },
            {"runtime_patch_allowed": False},
        )

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pre_tool_guard_allows_tcti_runtime_write_with_autonomous_authorization(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            {
                "hook_event_name": "PreToolUse",
                "tool_name": "apply_patch",
                "tool_input": {
                    "patch": "*** Update File: " + self.tcti_runtime_path()
                },
            },
            {
                "runtime_patch_allowed": True,
                "allowed_scope": ["OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/" + "hosted_exec/tcti/**"],
            },
        )

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pre_tool_guard_blocks_rtk_phone_command_before_promotion(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            exec_command_payload("rtk proxy make runtime-validation DESTINATION=iphoneos GATE=tcti-userland-marker"),
            {"physical_device_allowed": False},
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("physical_device_allowed=false", result.stderr)

    def test_pre_tool_guard_allows_rtk_phone_command_only_with_device_authorization(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            exec_command_payload("rtk proxy make runtime-validation DESTINATION=iphoneos GATE=tcti-userland-marker"),
            {"physical_device_allowed": True},
        )

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pre_tool_guard_blocks_optional_l4_target_without_device_authorization(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-release-l4 ORLIX_PHYSICAL_VALIDATION_OPT_IN=YES"),
            {"physical_device_allowed": False},
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("physical_device_allowed=false", result.stderr)

    def test_pre_tool_guard_blocks_beta_upload_before_release_promotion(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-upload"),
            {
                "simulator_gates_complete": False,
                "simulator_readiness_gate_ids": ["simulator-l3"],
                "simulator_readiness_missing_gate_ids": ["simulator-l3"],
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_blocks_top_level_beta_release_before_simulator_completion(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-release"),
            {
                "simulator_gates_complete": False,
                "simulator_readiness_gate_ids": ["simulator-l3"],
                "simulator_readiness_missing_gate_ids": ["simulator-l3"],
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_blocks_release_when_policy_is_missing(self):
        command = "make " + "be" + "ta-upload"
        result = run_hook(PRE_TOOL_GUARD, bash_payload(command))

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_allows_beta_release_with_current_complete_simulator_evidence_without_l4(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-upload"),
            {
                "simulator_gates_complete": True,
                "simulator_readiness_gate_ids": ["simulator-l3"],
                "simulator_readiness_missing_gate_ids": [],
                "physical_device_allowed": False,
            },
        )

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pre_tool_guard_blocks_stale_complete_simulator_evidence(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-archive"),
            {
                "simulator_gates_complete": False,
                "simulator_readiness_gate_ids": ["simulator-l3"],
                "simulator_readiness_missing_gate_ids": [],
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_blocks_partial_simulator_evidence_even_with_optional_l4(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-export-archive"),
            {
                "simulator_gates_complete": True,
                "simulator_readiness_gate_ids": ["simulator-tcti-full-linux-runtime-readiness"],
                "simulator_readiness_missing_gate_ids": ["simulator-tcti-full-linux-runtime-readiness"],
                "physical_device_allowed": True,
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_allows_release_name_in_read_only_search(self):
        command = "rtk grep -n '" + "be" + "ta-archive' Makefile"
        result = run_hook(PRE_TOOL_GUARD, bash_payload(command))

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pre_tool_guard_blocks_direct_agent_envelope_write(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload("rtk proxy touch Build/AgentHarness/orlix-tcti/next-task.json"),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_allows_user_directed_mixed_hook_and_runtime_patch(self):
        patch = "\n".join(
            [
                "*** Begin Patch",
                "*** Update File: .codex/hooks/example.py",
                "*** Update File: " + self.tcti_runtime_path(),
                "*** End Patch",
            ]
        )
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            {"hook_event_name": "PreToolUse", "tool_name": "apply_patch", "tool_input": {"patch": patch}},
            {"runtime_patch_allowed": False},
        )

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_permission_guard_cannot_override_release_promotion(self):
        command = "make " + "be" + "ta-archive"
        result = run_hook_in_repo(
            PERMISSION_GUARD,
            bash_payload(command),
            {
                "simulator_gates_complete": False,
                "simulator_readiness_gate_ids": ["simulator-l3"],
                "simulator_readiness_missing_gate_ids": ["simulator-l3"],
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_legacy_release_gate_boolean_does_not_authorize_beta(self):
        result = run_hook_in_repo(
            PRE_TOOL_GUARD,
            bash_payload("rtk proxy make beta-upload"),
            {"release_gate_eligible": True},
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("current, complete mandatory simulator L0-L3 evidence", result.stderr)

    def test_pre_tool_guard_blocks_direct_generated_report_write(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload("rtk proxy touch Build/Reports/runtime/fake.json"),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_external_ssd_bypass(self):
        commands = (
            "/usr/bin/xcrun simctl list",
            "rtk proxy /Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild -version",
            "xcodebuild -derivedDataPath /tmp/DerivedData test",
            "xcodebuild SYMROOT=/tmp/build test",
        )
        for command in commands:
            with self.subTest(command=command):
                result = run_hook(PRE_TOOL_GUARD, exec_command_payload(command))
                self.assertEqual(result.returncode, 2)
                self.assertIn("external-SSD", result.stderr)

    def test_pre_tool_guard_allows_read_only_bash_generated_tree_inspection(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            bash_payload(
                "rtk rg -n 'SYSCALL_DEFINE' "
                "Build/OrlixKernel/src/linux-6.12-port/kernel/fork.c"
            ),
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_allows_read_only_exec_command_generated_tree_inspection(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload(
                "rtk sed -n '1,80p' "
                "Build/OrlixMLibC/upstream/mlibc-1.0.git/options/posix/generic/unistd.cpp"
            ),
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_patch_edits_to_generated_trees(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            {
                "hook_event_name": "PreToolUse",
                "tool_name": "apply_patch",
                "tool_input": {
                    "patch": "\n".join(
                        [
                            "*** Begin Patch",
                            "*** Update File: Build/OrlixMLibC/src/mlibc-1.0/sysdeps/orlix/foo.cpp",
                            "@@",
                            "-old",
                            "+new",
                            "*** End Patch",
                        ]
                    )
                },
            },
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_bash_mutation_to_generated_trees(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            bash_payload(
                "rtk sed -i '' 's/old/new/' "
                "Build/OrlixOS/upstream/coreutils-9.5.git/src/cat.c"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_exec_command_mutation_to_linux_clone(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload(
                "rtk sed -i '' 's/old/new/' "
                "Build/OrlixKernel/upstream/linux-6.12.git/kernel/fork.c"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_exec_command_mutation_to_mlibc_clone(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload(
                "rtk touch Build/OrlixMLibC/upstream/mlibc-1.0.git/agent-edit"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_exec_command_mutation_to_coreutils_clone(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload(
                "rtk git -C Build/OrlixOS/upstream/coreutils-9.5.git apply /tmp/fix.patch"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_namespaced_exec_command_after_cd_to_generated_tree(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            exec_command_payload(
                "cd Build/OrlixOS/upstream/coreutils-9.5.git && rtk git apply /tmp/fix.patch",
                tool_name="functions.exec_command",
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_wrapped_builds_in_generated_trees(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            bash_payload(
                "rtk timeout 1200 make -C "
                "Build/OrlixKernel/src/linux-6.12-port oldconfig"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_script_writes_to_generated_trees(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            bash_payload(
                "rtk python3 - <<'PY'\n"
                "from pathlib import Path\n"
                "Path('Build/OrlixMLibC/src/mlibc-1.0/foo.cpp').write_text('x')\n"
                "PY"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_pre_tool_guard_blocks_writes_to_documented_generated_roots(self):
        result = run_hook(
            PRE_TOOL_GUARD,
            bash_payload(
                "rtk python3 - <<'PY'\n"
                "from pathlib import Path\n"
                "Path('Build/OrlixMLibC/kernel-headers/release/include/linux/foo.h').write_text('x')\n"
                "PY"
            ),
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("ORLIX-HARNESS-BLOCK", result.stderr)

    def test_permission_guard_allows_read_only_generated_tree_bash_request(self):
        result = run_hook(
            PERMISSION_GUARD,
            {
                "hook_event_name": "PermissionRequest",
                "tool_name": "Bash",
                "tool_input": {
                    "command": "rtk sed -n '1,80p' "
                    "Build/OrlixMLibC/upstream/mlibc-1.0.git/options/posix/generic/unistd.cpp"
                },
            },
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("ORLIX-HARNESS-BLOCK", result.stderr)


if __name__ == "__main__":
    unittest.main()
