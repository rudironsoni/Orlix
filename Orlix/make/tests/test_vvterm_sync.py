import importlib.util
from pathlib import Path
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "vvterm_sync.py"
SPEC = importlib.util.spec_from_file_location("vvterm_sync", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
vvterm_sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vvterm_sync)


class VVTermSyncTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.policy = {
            "path_replacements": [["VVTerm", "Orlix"], ["vvterm", "orlix"]],
            "text_replacements": [["VVTerm", "Orlix"], ["vvterm", "orlix"]],
            "preserve_text_paths": ["LICENSE"],
            "allowed_text_paths": ["LICENSE"],
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_normalize_tree_renames_paths_and_text_but_preserves_legal_text(self) -> None:
        source = self.root / "source"
        destination = self.root / "destination"
        (source / "VVTerm").mkdir(parents=True)
        (source / "VVTerm" / "vvterm.swift").write_text("struct VVTerm {}\n")
        (source / "LICENSE").write_text("VVTerm copyright\n")

        vvterm_sync.normalize_tree(source, destination, self.policy)

        self.assertEqual((destination / "Orlix" / "orlix.swift").read_text(), "struct Orlix {}\n")
        self.assertEqual((destination / "LICENSE").read_text(), "VVTerm copyright\n")

    def test_normalize_tree_preserves_binary_data(self) -> None:
        source = self.root / "source"
        destination = self.root / "destination"
        source.mkdir()
        payload = b"\x00VVTerm\xff"
        (source / "image.bin").write_bytes(payload)

        vvterm_sync.normalize_tree(source, destination, self.policy)

        self.assertEqual((destination / "image.bin").read_bytes(), payload)

    def test_normalize_tree_rejects_path_collisions(self) -> None:
        source = self.root / "source"
        destination = self.root / "destination"
        source.mkdir()
        (source / "VVTerm").write_text("one")
        (source / "Orlix").write_text("two")

        with self.assertRaises(vvterm_sync.SyncError):
            vvterm_sync.normalize_tree(source, destination, self.policy)

    def test_merge_trees_preserves_ours_and_imports_non_conflicting_upstream_change(self) -> None:
        base = self.root / "base"
        ours = self.root / "ours"
        theirs = self.root / "theirs"
        merge = self.root / "merge"
        for tree in (base, ours, theirs):
            tree.mkdir()
        (base / "shared.txt").write_text("first\nsecond\nthird\n")
        (ours / "shared.txt").write_text("ours\nsecond\nthird\n")
        (ours / "orlix.txt").write_text("custom\n")
        (theirs / "shared.txt").write_text("first\nsecond\ntheirs\n")

        conflicts = vvterm_sync.merge_trees(base, ours, theirs, merge)

        self.assertEqual(conflicts, [])
        self.assertEqual((merge / "shared.txt").read_text(), "ours\nsecond\ntheirs\n")
        self.assertEqual((merge / "orlix.txt").read_text(), "custom\n")

    def test_merge_trees_reports_conflict(self) -> None:
        base = self.root / "base"
        ours = self.root / "ours"
        theirs = self.root / "theirs"
        merge = self.root / "merge"
        for tree in (base, ours, theirs):
            tree.mkdir()
        (base / "shared.txt").write_text("base\n")
        (ours / "shared.txt").write_text("ours\n")
        (theirs / "shared.txt").write_text("theirs\n")

        conflicts = vvterm_sync.merge_trees(base, ours, theirs, merge)

        self.assertEqual(conflicts, ["shared.txt"])
        self.assertIn("<<<<<<<", (merge / "shared.txt").read_text())

    def test_merge_trees_imports_upstream_rename_deletion_and_binary_change(self) -> None:
        base = self.root / "base"
        ours = self.root / "ours"
        theirs = self.root / "theirs"
        merge = self.root / "merge"
        for tree in (base, ours, theirs):
            tree.mkdir()
        for tree in (base, ours):
            (tree / "renamed.txt").write_text("rename payload\n")
            (tree / "deleted.txt").write_text("remove me\n")
            (tree / "image.bin").write_bytes(b"\x00old")
        (theirs / "new-name.txt").write_text("rename payload\n")
        (theirs / "image.bin").write_bytes(b"\x00new")

        conflicts = vvterm_sync.merge_trees(base, ours, theirs, merge)

        self.assertEqual(conflicts, [])
        self.assertFalse((merge / "renamed.txt").exists())
        self.assertEqual((merge / "new-name.txt").read_text(), "rename payload\n")
        self.assertFalse((merge / "deleted.txt").exists())
        self.assertEqual((merge / "image.bin").read_bytes(), b"\x00new")

    def test_branding_check_allows_only_declared_legal_path(self) -> None:
        source = self.root / "source"
        source.mkdir()
        (source / "LICENSE").write_text("VVTerm copyright\n")
        (source / "source.swift").write_text("let value = \"vvterm\"\n")

        failures = vvterm_sync.check_branding(source, self.policy)

        self.assertEqual(failures, ["unbranded text: source.swift"])


if __name__ == "__main__":
    unittest.main()
