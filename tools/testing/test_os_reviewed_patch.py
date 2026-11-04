"""Reviewed kernel patches require exact positions, context and line counts."""

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "nexus_reviewed_patch", ROOT / "os/reviewed_patch.py")
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


class ReviewedPatchTests(unittest.TestCase):
    """Reject partial or fuzzy edits before any output is published."""

    def apply(self, source, patch):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "reviewed.patch"
            path.write_text(patch, encoding="utf-8")
            return PATCH.apply_reviewed_patch(source, path)

    def test_exact_context_and_disjoint_hunks(self):
        patch = ("--- upstream.c\n+++ owned.c\n"
                 "@@ -1,2 +1,2 @@\n a\n-b\n+B\n"
                 "@@ -4 +4,2 @@\n-d\n+D\n+e\n")
        self.assertEqual(self.apply("a\nb\nc\nd\n", patch), "a\nB\nc\nD\ne\n")

    def test_insert_and_delete_use_exact_unified_diff_coordinates(self):
        patch = ("--- old\n+++ new\n@@ -0,0 +1 @@\n+first\n"
                 "@@ -2 +2,0 @@\n-b\n")
        self.assertEqual(self.apply("a\nb\nc\n", patch), "first\na\nc\n")

    def test_drift_never_finds_another_matching_location(self):
        patch = "--- old\n+++ new\n@@ -1 +1 @@\n-a\n+A\n"
        for source in ("changed\na\n", "x\na\n", ""):
            with self.subTest(source=source), self.assertRaises(ValueError):
                self.apply(source, patch)

    def test_incomplete_or_inconsistent_patch_is_rejected(self):
        for hunk in ("@@ -1,2 +1,2 @@\n-a\n+A\n",
                     "@@ -1 +2 @@\n-a\n+A\n",
                     "@@ -1 +1 @@\n-a\n+A\n+extra\n",
                     "@@ -2 +1 @@\n-b\n+B\n",
                     "@@ -1 +1 @@\n?a\n"):
            with self.subTest(hunk=hunk), self.assertRaises(ValueError):
                self.apply("a\nb\n", "--- old\n+++ new\n" + hunk)

    def test_empty_multiple_file_and_reordered_patches_are_rejected(self):
        for patch in ("", "--- old\n+++ new\n",
                      "--- old\n+++ new\n@@ -1 +1 @@\n-a\n+A\n"
                      "--- other\n+++ new\n",
                      "--- old\n+++ new\n@@ -2 +2 @@\n-b\n+B\n"
                      "@@ -1 +1 @@\n-a\n+A\n"):
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                self.apply("a\nb\n", patch)


if __name__ == "__main__":
    unittest.main()
