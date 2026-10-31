"""Dependency preparation rejects unsafe checkouts before any Git mutation."""

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

from tools.dev import dependencies
from tools.evidence.common import EvidenceError
from tools.maintenance import source_catalogue


class DependencyPreparationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.area = Path(temporary.name)
        # This local transport model starts at official URLs independently of
        # the current production fetch policy, then exercises its migration.
        baseline = tuple(replace(item, fetch_url=item.upstream_url)
                         for item in source_catalogue.ALL_DEPENDENCIES)
        policy = patch.object(source_catalogue, "ALL_DEPENDENCIES", baseline)
        policy.start()
        self.addCleanup(policy.stop)
        self.root = self.area / "platform"
        self.module = "ext/googletest"
        self.origin = source_catalogue.dependency("googletest").fetch_url
        self.seed = self.area / "seed"
        self.seed.mkdir()
        self.git(self.seed, "init", "-q")
        (self.seed / "source.txt").write_text("fixture source\n")
        self.git(self.seed, "add", ".")
        self.commit(self.seed)
        self.selected = self.git(self.seed, "rev-parse", "HEAD")
        self.root.mkdir()
        self.git(self.root, "init", "-q")
        (self.root / ".gitmodules").write_text(self.modules())
        (self.root / "owned.txt").write_text("platform source\n")
        self.git(self.root, "add", ".")
        for item in source_catalogue.ALL_SOURCE_DEPENDENCIES:
            self.git(self.root, "update-index", "--add", "--cacheinfo",
                     "160000", self.selected, item.path)
        self.commit(self.root)
        self.checkout = self.root / self.module
        self.git(self.root, "clone", "-q", str(self.seed), self.module)
        self.git(self.checkout, "remote", "set-url", "origin", self.origin)
        self.git(self.root, "config", "submodule." + self.module + ".url",
                 self.origin)
        selection = patch.object(dependencies, "required_submodules",
                                 return_value=(self.module,))
        selection.start()
        self.addCleanup(selection.stop)

    def git(self, root, *arguments):
        return subprocess.check_output(
            ["git", "-C", str(root), *arguments], text=True,
            stderr=subprocess.STDOUT, timeout=15,
            env={key: value for key, value in os.environ.items()
                 if not key.startswith("GIT_")}).strip()

    def modules(self, parent=None):
        return "".join('[submodule "' + source_catalogue.module_name(item) +
            '"]\n\tpath = ' + source_catalogue.relative_path(item) +
            '\n\turl = ' + item.fetch_url + '\n'
            for item in source_catalogue.children(parent))

    def commit(self, root):
        self.git(root, "-c", "user.name=Fixture", "-c",
                 "user.email=fixture@example.invalid", "commit", "-qm",
                 "fixture dependency preparation")

    def prepare(self, network=False):
        return dependencies.prepare(self.root, "native",
                                    allow_network=network)

    def reject_before_mutation(self):
        run = subprocess.run
        mutations = []

        def observe(argv, **kwargs):
            if "submodule" in argv and any(
                    action in argv for action in ("sync", "update")):
                mutations.append(argv)
                return subprocess.CompletedProcess(argv, 0, "", "")
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            with self.assertRaises(EvidenceError):
                self.prepare(network=True)
        self.assertEqual(mutations, [])

    def test_clean_offline_checkout_records_exact_selected_commit(self):
        receipt = self.prepare()
        self.assertEqual(receipt["status"], "verified")
        self.assertFalse(receipt["network_authorized"])
        self.assertEqual(receipt["recursive_submodules"],
                         [{"path": self.module, "commit": self.selected}])

    def test_inherited_git_routing_cannot_select_another_repository(self):
        environment = {"GIT_DIR": str(self.seed / ".git"),
            "GIT_WORK_TREE": str(self.seed),
            "GIT_INDEX_FILE": str(self.seed / ".git/index")}
        with patch.dict(os.environ, environment):
            self.assertEqual(self.prepare()["status"], "verified")

    def test_unknown_origin_rejected_even_with_identical_bytes(self):
        self.git(self.checkout, "remote", "set-url", "origin",
                 "https://github.com/unreviewed/googletest.git")
        self.reject_before_mutation()

    def test_unknown_cached_module_url_is_not_silently_replaced(self):
        self.git(self.root, "config", "submodule." + self.module + ".url",
                 "https://github.com/private/googletest.git")
        self.reject_before_mutation()

    def test_fetch_url_rewrite_is_not_an_approved_transport(self):
        self.git(self.root, "config", "url." + str(self.seed) + ".insteadOf",
                 self.origin)
        self.reject_before_mutation()

    def test_module_local_fetch_rewrite_is_rejected_before_sync(self):
        self.git(self.checkout, "config", "url." + str(self.seed) + ".insteadOf",
                 self.origin)
        self.reject_before_mutation()

    def test_network_preparation_updates_parent_before_recursive_sync(self):
        run = subprocess.run
        operations = []

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                return subprocess.CompletedProcess(
                    argv, 0, self.selected + "\trefs/heads/main\n", "")
            if "submodule" in argv and any(
                    action in argv for action in ("sync", "update")):
                operations.append(argv[argv.index("submodule") + 1:])
                return subprocess.CompletedProcess(argv, 0, "", "")
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            self.assertEqual(self.prepare(network=True)["status"], "verified")
        self.assertEqual([operation[0] for operation in operations],
                         ["sync", "update", "sync", "update"])
        self.assertNotIn("--recursive", operations[1])
        self.assertIn("--recursive", operations[2])
        self.assertIn("--recursive", operations[3])
        for operation in (operations[1], operations[3]):
            self.assertIn("--checkout", operation)
            self.assertIn("--jobs", operation)
            self.assertNotIn("--force", operation)
            self.assertNotIn("--remote", operation)

    def test_unavailable_fetch_source_does_not_trigger_sync(self):
        run = subprocess.run
        operations = []

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                return subprocess.CompletedProcess(argv, 2, "", "unavailable")
            if "submodule" in argv:
                operations.append(argv)
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            with self.assertRaises(EvidenceError):
                self.prepare(network=True)
        self.assertEqual(operations, [])

    def controlled_transport(self):
        selected = source_catalogue.dependency("googletest")
        controlled = replace(selected,
            fetch_url="https://github.com/X-Gen-Lab/googletest.git")
        registry = tuple(controlled if item == selected else item
                         for item in source_catalogue.ALL_DEPENDENCIES)
        active = patch.object(source_catalogue, "ALL_DEPENDENCIES", registry)
        active.start()
        self.addCleanup(active.stop)
        declaration = self.root / ".gitmodules"
        declaration.write_text(declaration.read_text().replace(
            selected.fetch_url, controlled.fetch_url))
        self.git(self.root, "add", ".gitmodules")
        self.commit(self.root)
        return controlled

    def test_explicit_migration_syncs_only_approved_origin(self):
        controlled = self.controlled_transport()
        with self.assertRaises(EvidenceError):
            self.prepare()
        run = subprocess.run
        queried = []

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                queried.append(argv)
                return subprocess.CompletedProcess(argv, 0, self.selected +
                    "\trefs/tags/nexus/pins/" + self.selected + "\n", "")
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            result = self.prepare(network=True)
        self.assertEqual(result["status"], "verified")
        self.assertTrue(queried)
        self.assertTrue(all(controlled.fetch_url in argv for argv in queried))
        self.assertTrue(all(self.origin not in argv for argv in queried))
        self.assertEqual(self.git(self.checkout, "remote", "get-url", "origin"),
                         controlled.fetch_url)
        self.assertEqual(self.git(self.checkout, "rev-parse", "HEAD"),
                         self.selected)

    def test_missing_controlled_pin_never_falls_back_or_changes_origin(self):
        controlled = self.controlled_transport()
        run = subprocess.run
        queried = []

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                queried.append(argv)
                return subprocess.CompletedProcess(argv, 0,
                    self.selected + "\trefs/heads/main\n", "")
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            with self.assertRaises(EvidenceError):
                self.prepare(network=True)
        self.assertEqual(len(queried), 1)
        self.assertIn(controlled.fetch_url, queried[0])
        self.assertEqual(self.git(self.checkout, "remote", "get-url", "origin"),
                         self.origin)

    def test_controlled_policy_does_not_take_over_another_user_fork(self):
        self.controlled_transport()
        self.git(self.checkout, "remote", "set-url", "origin",
                 "https://github.com/personal/googletest.git")
        self.reject_before_mutation()

    def freertos_transition(self, *, changed_kernel=False):
        from tools.maintenance import freertos_transport
        kernel = self.area / "kernel-remote"
        kernel.mkdir()
        self.git(kernel, "init", "-q")
        baseline = self.modules("ext/freertos").encode()
        (kernel / ".gitmodules").write_bytes(baseline)
        (kernel / "tasks.c").write_text("/* Fixture kernel. */\n")
        self.git(kernel, "add", ".")
        for child in source_catalogue.children("ext/freertos"):
            self.git(kernel, "update-index", "--add", "--cacheinfo", "160000",
                     self.selected, source_catalogue.relative_path(child))
        self.commit(kernel)
        base = self.git(kernel, "rev-parse", "HEAD")
        checkout = self.root / "ext/freertos"
        self.git(self.root, "clone", "-q", "--no-hardlinks", str(kernel),
                 "ext/freertos")
        official = source_catalogue.dependency("freertos").upstream_url
        self.git(checkout, "remote", "set-url", "origin", official)
        self.git(self.root, "config", "submodule.ext/freertos.url", official)
        for child in source_catalogue.children("ext/freertos"):
            self.git(checkout, "clone", "-q", str(self.seed),
                     source_catalogue.relative_path(child))
            self.git(self.root / child.path, "remote", "set-url", "origin",
                     child.upstream_url)
            self.git(checkout, "config", "submodule." +
                     source_catalogue.module_name(child) + ".url",
                     child.upstream_url)
        registry = tuple(replace(item, fetch_url=(
            "https://github.com/X-Gen-Lab/" + item.upstream_url.rsplit("/", 1)[1]))
            if item.identifier == "freertos" or item.parent == "ext/freertos"
            else item for item in source_catalogue.ALL_DEPENDENCIES)
        policy = patch.object(source_catalogue, "ALL_DEPENDENCIES", registry)
        policy.start()
        self.addCleanup(policy.stop)
        updated = self.modules("ext/freertos").encode()
        (kernel / ".gitmodules").write_bytes(updated)
        if changed_kernel:
            (kernel / "tasks.c").write_text("/* Unreviewed kernel edit. */\n")
            self.git(kernel, "add", "tasks.c")
        self.git(kernel, "add", ".gitmodules")
        self.commit(kernel)
        target = self.git(kernel, "rev-parse", "HEAD")
        change = subprocess.check_output(["git", "-C", str(kernel), "diff",
            "--full-index", "--no-ext-diff", "--no-textconv", "--no-renames",
            "--diff-algorithm=myers", "--no-indent-heuristic", "--unified=3",
            base, target, "--", ".gitmodules"],
            env={key: value for key, value in os.environ.items()
                 if not key.startswith("GIT_")})
        lock = {"schema": 1, "kind": "freertos-transport",
            "dependency": "freertos", "upstream_base": base,
            "selected_commit": target, "changed_paths": [".gitmodules"],
            "baseline_modules_sha256": hashlib.sha256(baseline).hexdigest(),
            "selected_modules_sha256": hashlib.sha256(updated).hexdigest(),
            "patch_sha256": hashlib.sha256(change).hexdigest(),
            "policy": "only-approved-nested-fetch-urls"}
        for name, data in zip(freertos_transport.REVIEW_FILES,
                (json.dumps(lock).encode(), baseline, change)):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (self.root / ".gitmodules").write_text(self.modules())
        self.git(self.root, "add", ".gitmodules", "dependencies")
        self.git(self.root, "update-index", "--cacheinfo", "160000", target,
                 "ext/freertos")
        self.commit(self.root)
        selection = patch.object(dependencies, "required_submodules",
                                 return_value=("ext/freertos",))
        selection.start()
        self.addCleanup(selection.stop)
        return kernel, checkout, base, target

    def freertos_network(self, kernel, target, calls, *, before_sync=None):
        run = subprocess.run

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                revision = target if "FreeRTOS-Kernel.git" in " ".join(argv) \
                    else self.selected
                return subprocess.CompletedProcess(argv, 0, revision +
                    "\trefs/tags/nexus/pins/" + revision + "\n", "")
            if "fetch" in argv:
                calls.append(argv)
                argv = list(argv)
                argv[-2] = str(kernel)
            if (before_sync is not None and "submodule" in argv and
                    "sync" in argv and "--recursive" not in argv):
                before_sync()
            return run(argv, **kwargs)

        return patch.object(dependencies.subprocess, "run", side_effect=observe)

    def freertos_state(self, checkout):
        return {"head": self.git(checkout, "rev-parse", "HEAD"),
                "index": self.git(checkout, "write-tree"),
                "origin": self.git(checkout, "remote", "get-url", "origin"),
                "parent_index": self.git(self.root, "write-tree")}

    def forbid_remote_transport(self, checkout):
        # Only explicit fixture fetches are redirected to the local supplier.
        # An accidental fetch inside submodule update must fail without network.
        for owner in (self.root, checkout):
            self.git(owner, "config", "protocol.https.allow", "never")

    def test_old_freertos_checkout_fetches_reviewed_object_before_switch(self):
        kernel, checkout, base, target = self.freertos_transition()
        self.forbid_remote_transport(checkout)
        missing = subprocess.run(["git", "-C", str(checkout), "cat-file", "-e",
                                  target + "^{commit}"], capture_output=True)
        self.assertNotEqual(missing.returncode, 0)
        self.assertEqual(self.git(checkout, "rev-parse", "HEAD"), base)
        initial = self.freertos_state(checkout)
        admitted = []

        def before_sync():
            admitted.append((self.freertos_state(checkout),
                self.git(checkout, "for-each-ref", "--format=%(objectname)",
                         "refs/nexus/reviewed/" + target)))

        calls = []
        with self.freertos_network(kernel, target, calls,
                                   before_sync=before_sync):
            receipt = self.prepare(network=True)
        self.assertEqual(receipt["status"], "verified")
        self.assertEqual(admitted, [(initial, target)])
        self.assertEqual(self.git(checkout, "rev-parse", "HEAD"), target)
        self.assertEqual(len(calls), 1)
        self.assertIn("--no-write-fetch-head", calls[0])
        self.assertEqual(len(receipt["recursive_submodules"]), 3)

    def test_present_dangling_freertos_target_needs_no_remote_fetch(self):
        kernel, checkout, _, target = self.freertos_transition()
        self.forbid_remote_transport(checkout)
        self.git(checkout, "fetch", "--no-tags", "--no-write-fetch-head",
                 "--recurse-submodules=no", str(kernel), target)
        self.assertEqual(self.git(checkout, "rev-list", target,
                                  "--not", "--all"), target)
        initial = self.freertos_state(checkout)
        admitted = []
        calls = []
        with self.freertos_network(kernel, target, calls,
                before_sync=lambda: admitted.append(
                    self.freertos_state(checkout))):
            receipt = self.prepare(network=True)
        self.assertEqual(receipt["status"], "verified")
        self.assertEqual(calls, [])
        self.assertEqual(admitted, [initial])
        self.assertEqual(self.git(checkout, "rev-parse", "HEAD"), target)
        self.assertEqual(self.git(checkout, "rev-list", target,
                                  "--not", "--all"), "")

    def test_unreviewed_freertos_tree_never_gets_a_retained_reference(self):
        kernel, checkout, _, target = self.freertos_transition(
            changed_kernel=True)
        self.forbid_remote_transport(checkout)
        initial = self.freertos_state(checkout)
        references = self.git(checkout, "for-each-ref",
                              "--format=%(refname) %(objectname)")
        calls = []
        with self.freertos_network(kernel, target, calls):
            with self.assertRaisesRegex(EvidenceError,
                                        "migration proof failed"):
                self.prepare(network=True)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.freertos_state(checkout), initial)
        self.assertEqual(self.git(checkout, "for-each-ref",
            "--format=%(refname) %(objectname)"), references)
        self.assertEqual(self.git(checkout, "for-each-ref",
            "--format=%(refname)", "refs/nexus/reviewed"), "")

    def test_owned_freertos_pin_cannot_move_a_symbolic_reference_target(self):
        kernel, checkout, base, target = self.freertos_transition()
        self.forbid_remote_transport(checkout)
        self.git(checkout, "fetch", "--no-tags", "--no-write-fetch-head",
                 "--recurse-submodules=no", str(kernel), target)
        guard = "refs/heads/guard"
        reference = "refs/nexus/reviewed/" + target
        self.git(checkout, "update-ref", guard, base)
        self.git(checkout, "symbolic-ref", reference, guard)
        calls = []
        with self.freertos_network(kernel, target, calls):
            self.assertEqual(self.prepare(network=True)["status"], "verified")
        self.assertEqual(calls, [])
        self.assertEqual(self.git(checkout, "rev-parse", guard), base)
        self.assertEqual(self.git(checkout, "rev-parse", reference), target)
        symbolic = subprocess.run(["git", "-C", str(checkout),
            "symbolic-ref", "-q", reference], capture_output=True)
        self.assertNotEqual(symbolic.returncode, 0)

    def test_unknown_freertos_head_rejected_before_object_fetch(self):
        kernel, checkout, _, target = self.freertos_transition()
        (checkout / "tasks.c").write_text("/* Private work. */\n")
        self.git(checkout, "add", "tasks.c")
        self.commit(checkout)
        calls = []
        with self.freertos_network(kernel, target, calls):
            with self.assertRaises(EvidenceError):
                self.prepare(network=True)
        self.assertEqual(calls, [])

    def test_freertos_preparation_uses_the_inspected_repository_index(self):
        kernel, checkout, _, target = self.freertos_transition()
        self.forbid_remote_transport(checkout)
        environment = {"GIT_INDEX_FILE": str(self.seed / ".git/index")}
        with self.freertos_network(kernel, target, []):
            with patch.dict(os.environ, environment):
                self.assertEqual(self.prepare(network=True)["status"], "verified")
        self.assertEqual(self.git(checkout, "rev-parse", "HEAD"), target)

    def test_checkout_local_rewrite_cannot_redirect_review_object_fetch(self):
        kernel, checkout, _, target = self.freertos_transition()
        self.git(checkout, "config", "url." + str(self.seed) + ".insteadOf",
                 source_catalogue.dependency("freertos").fetch_url)
        calls = []
        with self.freertos_network(kernel, target, calls):
            with self.assertRaises(EvidenceError):
                self.prepare(network=True)
        self.assertEqual(calls, [])

    def test_uncommitted_platform_edit_rejected_before_sync(self):
        (self.root / "owned.txt").write_text("uncommitted platform work\n")
        self.reject_before_mutation()

    def test_staged_platform_edit_rejected_before_sync(self):
        (self.root / "owned.txt").write_text("staged platform work\n")
        self.git(self.root, "add", "owned.txt")
        self.reject_before_mutation()

    def test_untracked_platform_file_rejected_before_sync(self):
        (self.root / "untracked.txt").write_text("user work\n")
        self.reject_before_mutation()

    def test_dirty_dependency_rejected_before_sync(self):
        (self.checkout / "source.txt").write_text("user dependency work\n")
        self.reject_before_mutation()

    def test_untracked_dependency_file_rejected_before_sync(self):
        (self.checkout / "untracked.txt").write_text("user dependency work\n")
        self.reject_before_mutation()

    def test_unknown_dependency_revision_rejected_before_sync(self):
        (self.checkout / "source.txt").write_text("private local revision\n")
        self.git(self.checkout, "add", "source.txt")
        self.commit(self.checkout)
        self.reject_before_mutation()

    def test_changed_module_url_rejected_before_sync(self):
        path = self.root / ".gitmodules"
        path.write_text(path.read_text().replace(self.origin,
                       "https://github.com/unreviewed/googletest.git"))
        self.git(self.root, "add", ".gitmodules")
        self.commit(self.root)
        self.reject_before_mutation()

    def test_missing_offline_dependency_fails_without_fetch(self):
        shutil.rmtree(self.checkout)
        with self.assertRaises(EvidenceError):
            self.prepare()

    def test_uninitialized_nonempty_directory_is_not_overwritten(self):
        shutil.rmtree(self.checkout)
        self.checkout.mkdir()
        (self.checkout / "user.txt").write_text("untracked directory\n")
        self.reject_before_mutation()
        self.assertEqual((self.checkout / "user.txt").read_text(),
                         "untracked directory\n")

    def test_symlinked_module_directory_is_rejected(self):
        moved = self.area / "moved"
        self.checkout.rename(moved)
        self.checkout.symlink_to(moved, target_is_directory=True)
        self.reject_before_mutation()

    def test_wrong_nested_git_root_is_rejected(self):
        shutil.rmtree(self.checkout / ".git")
        self.reject_before_mutation()

    def test_symlinked_git_marker_is_rejected_before_git_mutation(self):
        directory = self.area / "moved-git-directory"
        (self.checkout / ".git").rename(directory)
        (self.checkout / ".git").symlink_to(directory, target_is_directory=True)
        self.reject_before_mutation()


if __name__ == "__main__":
    unittest.main()
