from pathlib import Path
import unittest

import github_issue_graph


def issue(number, *, state="OPEN", labels=(), children=(), blockers=(), parent=None, body=""):
    return {
        "number": number,
        "title": "",
        "state": state,
        "body": body,
        "parent": {"number": parent} if parent else None,
        "labels": {"nodes": [{"name": label} for label in labels]},
        "subIssues": {"nodes": [{"number": child, "state": state} for child, state in children]},
        "blockedBy": {"nodes": [{"number": blocker, "state": state} for blocker, state in blockers]},
    }


class GitHubIssueGraphTests(unittest.TestCase):
    def test_agent_frontier(self):
        ready = ("ready-for-agent",)
        issues = [issue(number, labels=ready) for number in (50, 232, 233, 242, 261)]
        issues += [
            issue(48, labels=ready, children=((50, "OPEN"),)),
            issue(51, labels=ready, blockers=((50, "OPEN"),)),
            issue(52),
            issue(53, state="CLOSED", labels=ready),
        ]
        self.assertEqual(github_issue_graph.agent_frontier(issues), [50, 232, 233, 242, 261])

    def test_reports_hierarchy_mismatch(self):
        root = Path("/repo/docs")
        epic = root / "objects/epic/doing/epic.md"
        story = root / "objects/story/doing/story.md"
        objects = {
            epic: {"kind": "epic", "status": "doing", "external_id": "github:rudironsoni/Orlix#107", "number": 107, "relations": {"has_story": {story}}},
            story: {"kind": "story", "status": "doing", "external_id": "github:rudironsoni/Orlix#108", "number": 108, "relations": {"story_of": {epic}}},
        }
        problems = github_issue_graph.graph_problems(
            objects,
            [issue(107, body="## Blocks\n\n- #108"), issue(108)],
            root,
        )
        self.assertIn("missing-native-hierarchy #107 -> #108", problems)
        self.assertIn("duplicate-relationship-metadata #107", problems)
        identity_problems = github_issue_graph.wiki_link_check.external_id_problems(
            [
                (epic, "epic", "github:rudironsoni/Orlix#107"),
                (story, "story", "github:rudironsoni/Orlix#107"),
                (root / "objects/task/doing/task.md", "task", "107"),
            ],
            root,
        )
        self.assertTrue(any(problem.startswith("duplicate-external-id") for problem in identity_problems))
        self.assertTrue(any(problem.startswith("bad-work-external-id") for problem in identity_problems))


if __name__ == "__main__":
    unittest.main()
