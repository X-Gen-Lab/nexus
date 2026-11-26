"""Actual staged-parent and isolated-child Git boundaries for one dependency."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import dependencies
from tools.maintenance import source_catalogue
from tools.evidence.common import EvidenceError


class SelectedCheckoutTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="selected checkout ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.item = source_catalogue.dependency("lwip")
        self.source = self.root / self.item.path
        self.source.mkdir(parents=True)
        self.git(self.root, "init", "-q")
        self.git(self.source, "init", "-q")
        self.git(self.source, "remote", "add", "origin", self.item.fetch_url)
        (self.source / "COPYING").write_text("ordinary contract fixture\n")
        self.git(self.source, "add", "COPYING")
        self.git(self.source, "commit", "-qm", "fixture first revision")
        self.old = self.git(self.source, "rev-parse", "HEAD")
        (self.source / "driver.c").write_text("int fixture(void) { return 0; }\n")
        self.git(self.source, "add", "driver.c")
        self.git(self.source, "commit", "-qm", "fixture selected revision")
        self.commit = self.git(self.source, "rev-parse", "HEAD")
        self.tree = self.git(self.source, "rev-parse", "HEAD^{tree}")
        declarations = "".join(
            '[submodule "' + source_catalogue.module_name(item) + '"]\n'
            'path = ' + source_catalogue.relative_path(item) + '\n'
            'url = ' + item.fetch_url + '\n'
            for item in source_catalogue.children())
        (self.root / ".gitmodules").write_text(declarations)
        self.git(self.root, "add", ".gitmodules")
        self.git(self.root, "update-index", "--add", "--cacheinfo",
                 "160000," + self.old + "," + self.item.path)
        self.alternate = self.root / "parent-review.index"
        shutil.copy2(self.root / ".git/index", self.alternate)
        self.git(self.root, "update-index", "--cacheinfo",
                 "160000," + self.commit + "," + self.item.path,
                 index=self.alternate)

    def git(self, root, *arguments, index=None):
        env = {key: value for key, value in os.environ.items()
               if not key.startswith("GIT_")}
        if index is not None:
            env["GIT_INDEX_FILE"] = str(index)
        result = subprocess.run(
            ["git", "-c", "user.name=Nexus Fixture",
             "-c", "user.email=fixture@example.invalid", "-C", str(root),
             *arguments], env=env, capture_output=True, text=True,
            check=True, timeout=20)
        return result.stdout.strip()

    def verify(self, *, parent_index=True, source=None, development=False,
               commit=None, tree=None):
        return dependencies.verify_selected_checkout(
            self.root, source or self.source, self.item.identifier,
            commit or self.commit, tree or self.tree,
            parent_index=parent_index, development=development)

    def foreign_context(self):
        return patch.dict(os.environ, {
            "GIT_INDEX_FILE": str(self.alternate),
            "GIT_DIR": str(self.root / "foreign.git"),
            "GIT_WORK_TREE": str(self.root / "foreign-worktree")})

    def test_parent_alternate_index_binds_selected_link_child_context_is_own(self):
        with self.foreign_context():
            result = self.verify()
        self.assertEqual(result["commit"], self.commit)
        self.assertEqual(result["tree"], self.tree)
        self.assertEqual(result["path"], self.item.path)

    def test_parent_index_is_never_inherited_without_explicit_opt_in(self):
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify(parent_index=False)

    def test_child_dirty_index_is_not_hidden_by_clean_parent_review_index(self):
        (self.source / "driver.c").write_text("int fixture(void) { return 1; }\n")
        self.git(self.source, "add", "driver.c")
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify()

    def test_child_untracked_file_rejects(self):
        (self.source / "unreviewed.c").write_text("int unreviewed;\n")
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify()

    def test_wrong_tree_rejects_even_when_commit_matches(self):
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify(tree="1" * 40)

    def test_symbolic_source_selector_rejects_before_canonicalization(self):
        alias = self.root / "source alias"
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(EvidenceError):
            self.verify(source=alias, development=True)

    def test_local_origin_rewrite_rejects_without_network(self):
        self.git(self.source, "config", "url.https://example.invalid/.insteadOf",
                 "https://github.com/")
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify()

    def test_changed_module_declaration_rejects(self):
        content = (self.root / ".gitmodules").read_text()
        (self.root / ".gitmodules").write_text(
            content.replace(self.item.fetch_url, self.item.upstream_url))
        self.git(self.root, "add", ".gitmodules", index=self.alternate)
        with self.foreign_context(), self.assertRaises(EvidenceError):
            self.verify()

    def test_development_still_requires_exact_selected_commit_and_clean_origin(self):
        self.git(self.source, "remote", "set-url", "origin", self.item.upstream_url)
        with self.foreign_context():
            result = self.verify(development=True)
        self.assertEqual(result["commit"], self.commit)
        with self.assertRaises(EvidenceError):
            self.verify(development=True, commit=self.old)


if __name__ == "__main__":
    unittest.main()
