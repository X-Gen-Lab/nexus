"""Review transport-only FreeRTOS changes using real local Git object graphs."""
from __future__ import annotations
from dataclasses import replace
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import freertos_transport as transport
from tools.maintenance import source_catalogue as catalogue


class FreeRtosTransportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="freertos transport ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "ext/freertos"
        self.source.mkdir(parents=True)
        self.original = tuple(replace(item, fetch_url=item.upstream_url)
                              for item in catalogue.ALL_DEPENDENCIES)
        policy = patch.object(catalogue, "ALL_DEPENDENCIES", self.original)
        policy.start()
        self.addCleanup(policy.stop)
        self.git(self.root, "init", "--quiet")
        self.git(self.source, "init", "--quiet")
        self.git(self.source, "remote", "add", "origin",
                 catalogue.dependency("freertos").upstream_url)
        self.upstream = self.modules("ext/freertos", upstream=True)
        (self.source / ".gitmodules").write_bytes(self.upstream)
        (self.source / "tasks.c").write_text("/* Official fixture. */\n")
        (self.source / "LICENSE.md").write_text("Official fixture notice\n")
        for item in catalogue.children("ext/freertos"):
            child = self.root / item.path
            child.mkdir(parents=True)
            self.git(child, "init", "--quiet")
            self.git(child, "remote", "add", "origin", item.upstream_url)
            (child / "LICENSE").write_text("Port fixture notice\n")
            self.git(child, "add", ".")
            self.git(child, "commit", "--quiet", "-m", "official port")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "--quiet", "-m", "official kernel")
        self.base = self.git(self.source, "rev-parse", "HEAD")
        self.selected = self.base
        (self.root / ".gitmodules").write_bytes(self.modules(None))
        self.git(self.root, "add", ".gitmodules", "ext/freertos")
        self.git(self.root, "commit", "--quiet", "-m", "owning platform")

    def git(self, directory, *arguments, raw=False):
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith("GIT_")}
        environment.update({"GIT_AUTHOR_NAME": "Transport fixture",
            "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
            "GIT_COMMITTER_NAME": "Transport fixture",
            "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})
        result = subprocess.run(["git", "-C", str(directory), *arguments],
            capture_output=True, env=environment, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return result.stdout if raw else result.stdout.decode().strip()

    def modules(self, parent, *, upstream=False):
        return "".join('[submodule "' + catalogue.module_name(item) + '"]\n'
            '\tpath = ' + catalogue.relative_path(item) + '\n\turl = ' +
            (item.upstream_url if upstream else item.fetch_url) + '\n'
            for item in catalogue.children(parent)).encode()

    def controlled(self):
        records = tuple(replace(item, fetch_url=(
            "https://github.com/X-Gen-Lab/" +
            item.upstream_url.rsplit("/", 1)[1]))
            if item.identifier == "freertos" or item.parent == "ext/freertos"
            else item for item in self.original)
        patched = patch.object(catalogue, "ALL_DEPENDENCIES", records)
        patched.start()
        self.addCleanup(patched.stop)
        for item in records:
            if item.identifier == "freertos" or item.parent == "ext/freertos":
                self.git(self.root / item.path, "remote", "set-url", "origin",
                         item.fetch_url)
        (self.root / ".gitmodules").write_bytes(self.modules(None))
        (self.source / ".gitmodules").write_bytes(self.modules("ext/freertos"))
        self.git(self.source, "add", ".gitmodules")
        self.git(self.source, "commit", "--quiet", "-m", "transport only")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()

    def write_review(self):
        selected_modules = self.git(self.source, "show",
            self.selected + ":.gitmodules", raw=True)
        change = self.git(self.source, "diff", "--full-index",
            "--no-ext-diff", "--no-textconv",
            "--no-renames", "--diff-algorithm=myers", "--no-indent-heuristic",
            "--unified=3", self.base, self.selected, "--", ".gitmodules",
            raw=True)
        self.lock = {"schema": 1, "kind": "freertos-transport",
            "dependency": "freertos", "upstream_base": self.base,
            "selected_commit": self.selected, "changed_paths": [".gitmodules"],
            "baseline_modules_sha256": self.sha(self.upstream),
            "selected_modules_sha256": self.sha(selected_modules),
            "patch_sha256": self.sha(change),
            "policy": "only-approved-nested-fetch-urls"}
        for name, data in zip(transport.REVIEW_FILES,
                (json.dumps(self.lock).encode(), self.upstream, change)):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        self.git(self.root, "add", ".gitmodules", "dependencies",
                 "ext/freertos")

    @staticmethod
    def sha(data):
        return hashlib.sha256(data).hexdigest()

    def save_lock(self):
        target = self.root / transport.REVIEW_FILES[0]
        target.write_text(json.dumps(self.lock))

    def verify(self, **options):
        return transport.verify(self.root, **options)

    def test_original_upstream_remains_valid_without_review_lock(self):
        result = self.verify()
        self.assertEqual(result["mode"], "upstream")
        self.assertEqual(result["upstream_base"], self.base)
        self.assertEqual(result["selected_commit"], self.base)

    def test_exact_two_url_change_is_admitted(self):
        self.controlled()
        result = self.verify()
        self.assertEqual(result["mode"], "metadata-only")
        self.assertEqual(result["upstream_base"], self.base)
        self.assertEqual(result["selected_commit"], self.selected)

    def test_reviewed_migration_can_start_at_original_clean_checkout(self):
        self.controlled()
        self.git(self.source, "checkout", "--quiet", self.base)
        revisions = transport.migration_revisions(self.root, self.selected)
        self.assertEqual(revisions, frozenset({self.base, self.selected}))
        with self.assertRaises(ValueError):
            self.verify()

    def test_provisional_migration_never_claims_git_object_proof(self):
        self.controlled()
        with patch.object(subprocess, "run", side_effect=AssertionError("Git")):
            revisions = transport.provisional_migration_revisions(
                self.root, self.selected)
        self.assertEqual(revisions, frozenset({self.base, self.selected}))
        with self.assertRaises(ValueError):
            transport.provisional_migration_revisions(self.root, "a" * 40)
        baseline = self.root / transport.REVIEW_FILES[1]
        baseline.write_bytes(baseline.read_bytes() + b"# corrupt\n")
        with self.assertRaises(ValueError):
            transport.provisional_migration_revisions(self.root, self.selected)

    def test_original_migration_has_no_unapproved_alternative(self):
        self.assertEqual(transport.migration_revisions(self.root, self.base),
                         frozenset({self.base}))

    def test_changed_code_cannot_be_hidden_by_declared_changed_paths(self):
        self.controlled()
        (self.source / "tasks.c").write_text("/* Unreviewed behavior. */\n")
        self.git(self.source, "add", "tasks.c")
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        with self.assertRaisesRegex(ValueError, "tree"):
            self.verify()

    def test_changed_mode_rejects(self):
        self.controlled()
        self.git(self.source, "update-index", "--chmod=+x", ".gitmodules")
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        with self.assertRaisesRegex(ValueError, "mode|tree|patch"):
            self.verify()

    def test_changed_nested_gitlink_rejects(self):
        self.controlled()
        item = catalogue.children("ext/freertos")[0]
        self.git(self.source, "update-index", "--cacheinfo", "160000",
                 "a" * 40, catalogue.relative_path(item))
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        with self.assertRaisesRegex(ValueError, "tree"):
            self.verify()

    def test_extra_module_field_rejects_even_with_recomputed_hashes(self):
        self.controlled()
        with (self.source / ".gitmodules").open("a") as stream:
            stream.write("\tbranch = unreviewed\n")
        self.git(self.source, "add", ".gitmodules")
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        with self.assertRaises(ValueError):
            self.verify()

    def test_extra_comment_change_is_not_a_url_only_change(self):
        self.controlled()
        with (self.source / ".gitmodules").open("a") as stream:
            stream.write("# Additional metadata edit\n")
        self.git(self.source, "add", ".gitmodules")
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        with self.assertRaisesRegex(ValueError, "URL"):
            self.verify()

    def test_replacement_refs_cannot_hide_changed_kernel_code(self):
        self.controlled()
        approved = self.selected
        (self.source / "tasks.c").write_text("/* Changed kernel. */\n")
        self.git(self.source, "add", "tasks.c")
        self.git(self.source, "commit", "--amend", "--quiet", "--no-edit")
        self.selected = self.git(self.source, "rev-parse", "HEAD")
        self.write_review()
        self.git(self.source, "replace", self.selected, approved)
        with self.assertRaisesRegex(ValueError, "tree"):
            transport.migration_revisions(self.root, self.selected)

    def test_wrong_base_rejects(self):
        self.controlled()
        self.lock["upstream_base"] = "b" * 40
        self.save_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_multiple_parents_reject(self):
        self.controlled()
        tree = self.git(self.source, "rev-parse", "HEAD^{tree}")
        merge = self.git(self.source, "commit-tree", tree, "-p", self.base,
                         "-p", self.selected, "-m", "unreviewed merge")
        self.git(self.source, "checkout", "--quiet", merge)
        self.selected = merge
        self.write_review()
        with self.assertRaisesRegex(ValueError, "parent"):
            self.verify()

    def test_legacy_grafts_cannot_hide_an_actual_second_parent(self):
        self.controlled()
        tree = self.git(self.source, "rev-parse", "HEAD^{tree}")
        merge = self.git(self.source, "commit-tree", tree, "-p", self.base,
                         "-p", self.selected, "-m", "unreviewed merge")
        self.git(self.source, "checkout", "--quiet", merge)
        self.selected = merge
        self.write_review()
        grafts = self.source / ".git/info/grafts"
        grafts.write_text(merge + " " + self.base + "\n")
        with self.assertRaisesRegex(ValueError, "parent"):
            self.verify()

    def test_wrong_origin_and_dirty_source_reject(self):
        self.controlled()
        (self.source / "untracked.txt").write_text("unreviewed\n")
        with self.assertRaisesRegex(ValueError, "dirty"):
            self.verify()
        (self.source / "untracked.txt").unlink()
        self.git(self.source, "remote", "set-url", "origin",
                 "https://github.com/unknown/FreeRTOS-Kernel.git")
        with self.assertRaisesRegex(ValueError, "origin"):
            self.verify()

    def test_uninitialized_source_cannot_inherit_parent(self):
        shutil.rmtree(self.source / ".git")
        with self.assertRaisesRegex(ValueError, "initialized"):
            self.verify()

    def test_parent_active_index_is_honored_only_for_parent(self):
        self.controlled()
        alternate = self.root / ".git/review-index"
        shutil.copy2(self.root / ".git/index", alternate)
        self.git(self.root, "update-index", "--cacheinfo", "160000",
                 "c" * 40, "ext/freertos")
        with patch.dict(os.environ, {"GIT_INDEX_FILE": str(alternate)}):
            self.assertEqual(self.verify()["selected_commit"], self.selected)
        with self.assertRaisesRegex(ValueError, "gitlink|selected"):
            self.verify()

    def test_preparation_can_ignore_an_external_parent_index(self):
        self.controlled()
        index = self.root / ".git/index"
        original = index.read_bytes()
        self.git(self.root, "update-index", "--cacheinfo", "160000",
                 "c" * 40, "ext/freertos")
        foreign = self.root / ".git/foreign-index"
        shutil.copy2(index, foreign)
        index.write_bytes(original)
        with patch.dict(os.environ, {"GIT_INDEX_FILE": str(foreign)}):
            result = self.verify(parent_index=False)
            self.assertEqual(result["selected_commit"], self.selected)
            with self.assertRaisesRegex(ValueError, "gitlink|selected"):
                self.verify()

    def test_unknown_lock_fields_and_duplicate_json_reject(self):
        self.controlled()
        self.lock["fallback"] = True
        self.save_lock()
        with self.assertRaisesRegex(ValueError, "fields"):
            self.verify()
        self.lock.pop("fallback")
        self.save_lock()
        path = self.root / transport.REVIEW_FILES[0]
        path.write_text(path.read_text().replace('"schema": 1',
                       '"schema": 1, "schema": 1'))
        with self.assertRaisesRegex(ValueError, "Duplicate JSON"):
            self.verify()

    def test_orphan_review_material_is_rejected(self):
        target = self.root / transport.REVIEW_FILES[1]
        target.parent.mkdir(parents=True)
        target.write_bytes(self.upstream)
        with self.assertRaisesRegex(ValueError, "review"):
            self.verify()

    def test_no_git_export_validates_material_without_executing_git(self):
        self.controlled()
        shutil.rmtree(self.root / ".git")
        shutil.rmtree(self.source / ".git")
        with patch.object(subprocess, "run", side_effect=AssertionError("Git")):
            result = self.verify(selected_commit=self.selected,
                                 require_git=False)
        self.assertEqual(result["mode"], "metadata-only")
        self.assertEqual(result["verification"], "exported-materials")

    def test_review_creation_during_export_verification_rejects(self):
        snapshot = transport.snapshot
        reads = 0

        def changed(path):
            nonlocal reads
            result = snapshot(path)
            if path == self.source / ".gitmodules":
                reads += 1
                if reads == 2:
                    target = self.root / transport.REVIEW_FILES[0]
                    target.parent.mkdir(parents=True)
                    target.write_text("{}")
            return result

        with patch.object(transport, "snapshot", side_effect=changed):
            with self.assertRaisesRegex(ValueError, "changed"):
                self.verify(selected_commit=self.base, require_git=False)

    def test_no_git_requires_known_selected_commit(self):
        with self.assertRaisesRegex(ValueError, "selected"):
            self.verify(require_git=False)

    def test_controlled_transport_cannot_drop_its_review_lock(self):
        self.controlled()
        for name in transport.REVIEW_FILES:
            (self.root / name).unlink()
        with self.assertRaisesRegex(ValueError, "review"):
            self.verify(selected_commit=self.selected, require_git=False)

    def test_no_git_changed_exported_modules_reject(self):
        self.controlled()
        (self.source / ".gitmodules").write_bytes(self.upstream)
        with self.assertRaises(ValueError):
            self.verify(selected_commit=self.selected, require_git=False)

    def test_no_git_wrong_selected_commit_reject(self):
        self.controlled()
        with self.assertRaisesRegex(ValueError, "selected"):
            self.verify(selected_commit="a" * 40, require_git=False)

    def test_missing_or_modified_review_material_rejects(self):
        self.controlled()
        for name in transport.REVIEW_FILES[1:]:
            target = self.root / name
            original = target.read_bytes()
            target.write_bytes(original + b"unreviewed\n")
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    self.verify()
            target.write_bytes(original)
        (self.root / transport.REVIEW_FILES[2]).unlink()
        with self.assertRaises(ValueError):
            self.verify()


if __name__ == "__main__":
    unittest.main()
