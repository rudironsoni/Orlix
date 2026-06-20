import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "tools/orlix-linux-oracle/orlix-linux-oracle.swift"
CASE = ROOT / "tools/orlix-linux-oracle/cases/path-errno.json"
SAMPLES = ROOT / "tools/orlix-linux-oracle/samples"


class OrlixLinuxOracleToolTests(unittest.TestCase):
    maxDiff = None

    def run_oracle(self, *args, check=True):
        result = subprocess.run(
            ["swift", str(TOOL), *map(str, args)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if check and result.returncode != 0:
            self.fail(
                "oracle command failed\n"
                f"args: {args}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            )
        return result

    def test_path_errno_case_validates(self):
        result = self.run_oracle("validate-case", CASE)

        self.assertIn("case path-errno is valid", result.stdout)

    def test_path_errno_matching_samples_compare(self):
        result = self.run_oracle(
            "compare",
            "--case",
            CASE,
            "--linux-result",
            SAMPLES / "path-errno.linux.json",
            "--orlix-result",
            SAMPLES / "path-errno.orlix.json",
        )

        self.assertIn("case path-errno matches", result.stdout)

    def test_path_errno_drift_sample_fails_compare(self):
        result = self.run_oracle(
            "compare",
            "--case",
            CASE,
            "--linux-result",
            SAMPLES / "path-errno.linux.json",
            "--orlix-result",
            SAMPLES / "path-errno.orlix-drift.json",
            check=False,
        )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("oracle comparison failed", result.stderr)
        self.assertIn("errnoEvents differ", result.stderr)

    def test_path_errno_orlix_log_conversion_matches_sample(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "path-errno.orlix.generated.json"

            self.run_oracle(
                "orlix-result-from-log",
                "--case",
                CASE,
                "--log",
                SAMPLES / "path-errno.orlix-kselftest.log",
                "--output",
                output,
            )

            generated = json.loads(output.read_text())
            expected = json.loads((SAMPLES / "path-errno.orlix.json").read_text())
            self.assertEqual(generated.get("signal"), expected.get("signal"))
            expected_without_nil_signal = dict(expected)
            expected_without_nil_signal.pop("signal", None)
            self.assertEqual(generated, expected_without_nil_signal)


if __name__ == "__main__":
    unittest.main()
