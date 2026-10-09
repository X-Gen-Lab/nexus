"""Verify the contribution guides' commit-header contract."""

import unittest

from scripts.ci.check_commit_message import valid_message


class CommitMessageTests(unittest.TestCase):
    def test_conventional_headers_and_breaking_change(self):
        for message in (
            "fix(hal): retain borrowed buffers",
            "refactor(io)!: use fixed ports\n\nBREAKING CHANGE: old API removed",
            "docs: clarify comment rules",
            "ci(style): check the same snapshot",
        ):
            with self.subTest(message=message):
                self.assertTrue(valid_message(message))

    def test_git_generated_merge_and_revert(self):
        self.assertTrue(valid_message("Merge branch 'main' into feature"))
        self.assertTrue(valid_message('Revert "fix(hal): retain borrowed buffers"'))

    def test_invalid_or_empty_headers(self):
        for message in ("", "\nfix: later", "update", "fix: ", "unknown: text", "fix text"):
            with self.subTest(message=message):
                self.assertFalse(valid_message(message))


if __name__ == "__main__":
    unittest.main()
