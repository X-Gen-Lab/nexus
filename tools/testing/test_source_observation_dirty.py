"""Actual Git source observations distinguish already-dirty checkout changes.

Development observations must identify the observed tracked/untracked bytes,
executable modes and symlink text, including initialized local submodules.
Clean candidate and consumer records keep their existing five-field schema;
allowing a dirty development record does not qualify a clean candidate, an OCI
image, an offline reproduction or any produced software/hardware artifact.
"""

import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest

from tools.evidence.common import EvidenceError
from tools.evidence.identity import observe_git_source, verify_source


class DirtyGitSourceObservationTests(unittest.TestCase):
    def setUp(self):
        directory = os.environ.get("NEXUS_SOURCE_OBSERVATION_FIXTURES")
        self.temporary = tempfile.TemporaryDirectory(
            prefix="git-source-observation-", dir=directory)
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name)
        self.root = self.workspace / "checkout"
        self.repository(self.root)
        (self.root / "source.txt").write_bytes(b"committed source\n")
        (self.root / "target-a.txt").write_bytes(b"same target bytes\n")
        (self.root / "target-b.txt").write_bytes(b"same target bytes\n")
        (self.root / "source-link").symlink_to("source.txt")
        self.commit(self.root, "Initial fixture source")
        self.assertFalse(observe_git_source(self.root)["dirty"])

    def record(self, kind, **values):
        """Optional external evidence never becomes an observed untracked file."""
        destination = os.environ.get("NEXUS_SOURCE_OBSERVATION_TRANSCRIPT")
        if destination is not None:
            with Path(destination).open("a", encoding="utf-8") as stream:
                stream.write(json.dumps({"test": self.id(), "kind": kind,
                                         **values}, sort_keys=True) + "\n")

    def git(self, root, *arguments):
        command = ["git", "-C", str(root), *arguments]
        result = subprocess.run(command, capture_output=True, check=False,
                                text=True, env={
                                    key: value for key, value in os.environ.items()
                                    if not key.startswith("GIT_")})
        self.record("fixture_git_command", argv=command, cwd=str(self.workspace),
                    returncode=result.returncode, stdout=result.stdout,
                    stderr=result.stderr)
        self.assertEqual(result.returncode, 0,
                         f"Fixture Git failed: {command!r}\n{result.stderr}")
        return result.stdout.strip()

    def repository(self, root):
        root.mkdir()
        self.git(root, "init", "--quiet", "--initial-branch=main")
        # These are repository-local fixture settings, never global config.
        self.git(root, "config", "user.name", "Nexus Source Fixture")
        self.git(root, "config", "user.email", "source-fixture@example.invalid")
        self.git(root, "config", "commit.gpgsign", "false")
        self.git(root, "config", "core.filemode", "true")

    def commit(self, root, message):
        self.git(root, "add", "--all")
        # Run normal Git commit; no hook bypass, SKIP or fake commit identity.
        self.git(root, "commit", "--quiet", "-m", message)

    def observe(self, label):
        observation = observe_git_source(self.root)
        self.record("real_source_observation", label=label,
                    observation=observation)
        return observation

    def verify(self, observation, *, clean=False):
        try:
            verify_source(observation, clean=clean)
        except EvidenceError as error:
            self.record("real_source_verification", clean=clean,
                        outcome="rejected", error=str(error))
            raise
        self.record("real_source_verification", clean=clean,
                    outcome="accepted")

    def assert_dirty_change_is_observed(self, before):
        after = self.observe("after_dirty_change")
        self.assertTrue(before["dirty"])
        self.assertTrue(after["dirty"])
        self.assertEqual(before["commit"], after["commit"])
        self.assertEqual(before["tree"], after["tree"])
        # Separate subtests execute both observable contracts on the old code.
        with self.subTest(contract="dirty_snapshot_changes"):
            self.assertNotEqual(before, after,
                                "Dirty checkout changes need byte/mode/link identity")
        with self.subTest(contract="verify_rejects_stale_dirty_snapshot"):
            with self.assertRaisesRegex(EvidenceError, "stale"):
                self.verify(before)

    def test_clean_stable_record_keeps_legacy_schema_and_candidate_admission(self):
        before = self.observe("clean_before")
        self.assertEqual(set(before), {"root", "commit", "tree", "dirty",
                                       "submodules"})
        self.assertFalse(before["dirty"])
        self.assertEqual(before["submodules"], [])
        self.assertEqual(before, self.observe("clean_unchanged"))
        self.verify(before)
        self.verify(before, clean=True)

    def test_unchanged_dirty_record_is_development_only(self):
        (self.root / "source.txt").write_bytes(b"dirty tracked source\n")
        (self.root / "scratch.txt").write_bytes(b"already untracked\n")
        before = self.observe("dirty_before")
        self.assertTrue(before["dirty"])
        self.assertEqual(before, self.observe("dirty_unchanged"))
        self.verify(before)
        with self.assertRaisesRegex(EvidenceError, "dirty"):
            self.verify(before, clean=True)
        historical = {key: before[key] for key in (
            "root", "commit", "tree", "dirty", "submodules")}
        with self.assertRaisesRegex(EvidenceError, "stale"):
            self.verify(historical)

    def test_identical_dirty_bytes_rewritten_do_not_invent_a_source_change(self):
        path = self.root / "source.txt"
        path.write_bytes(b"unchanged dirty source\n")
        before = self.observe("dirty_before_same_bytes")
        path.write_bytes(path.read_bytes())
        metadata = path.stat()
        os.utime(path, ns=(metadata.st_atime_ns,
                           metadata.st_mtime_ns + 2_000_000_000))
        self.assertEqual(before, self.observe("dirty_same_bytes_new_mtime"))
        self.verify(before)

    def test_already_dirty_tracked_bytes_change_identity_and_stale_verification(self):
        path = self.root / "source.txt"
        path.write_bytes(b"dirty tracked version A\n")
        before = self.observe("tracked_dirty_A")
        status = self.git(self.root, "status", "--porcelain", "--untracked-files=all")
        path.write_bytes(b"dirty tracked version B\n")
        self.assertEqual(status, self.git(
            self.root, "status", "--porcelain", "--untracked-files=all"))
        self.assert_dirty_change_is_observed(before)

    def test_already_untracked_bytes_change_identity_and_stale_verification(self):
        path = self.root / "already-untracked.txt"
        path.write_bytes(b"untracked version A\n")
        before = self.observe("already_untracked_A")
        status = self.git(self.root, "status", "--porcelain", "--untracked-files=all")
        path.write_bytes(b"untracked version B\n")
        self.assertEqual(status, self.git(
            self.root, "status", "--porcelain", "--untracked-files=all"))
        self.assert_dirty_change_is_observed(before)

    def test_executable_mode_change_is_observed_while_tracked_bytes_stay_dirty(self):
        path = self.root / "source.txt"
        path.write_bytes(b"dirty source with stable bytes\n")
        path.chmod(0o644)
        before = self.observe("dirty_non_executable")
        content = path.read_bytes()
        self.assertFalse(path.stat().st_mode & stat.S_IXUSR)
        path.chmod(0o755)
        self.assertTrue(path.stat().st_mode & stat.S_IXUSR)
        self.assertEqual(path.read_bytes(), content)
        self.assert_dirty_change_is_observed(before)

    def test_symlink_text_change_is_observed_without_following_equal_targets(self):
        path = self.root / "source-link"
        path.unlink()
        path.symlink_to("target-a.txt")
        before = self.observe("dirty_link_target_a")
        content = path.read_bytes()
        path.unlink()
        path.symlink_to("target-b.txt")
        self.assertEqual(os.readlink(path), "target-b.txt")
        self.assertEqual(path.read_bytes(), content)
        self.assert_dirty_change_is_observed(before)

    def test_local_dirty_submodule_bytes_are_not_only_a_gitlink_or_dirty_marker(self):
        origin = self.workspace / "local-submodule-origin"
        self.repository(origin)
        (origin / "sub-source.txt").write_bytes(b"committed nested source\n")
        self.commit(origin, "Initial local submodule")
        self.git(self.root, "-c", "protocol.file.allow=always", "submodule", "add",
                 "--quiet", str(origin), "deps/sdk")
        self.commit(self.root, "Pin initialized local submodule")
        nested = self.root / "deps/sdk/sub-source.txt"
        nested.write_bytes(b"dirty nested version A\n")
        before = self.observe("submodule_dirty_A")
        self.assertEqual(len(before["submodules"]), 1)
        self.assertNotIn(before["submodules"][0][0], "-+U")
        status = self.git(self.root, "status", "--porcelain", "--untracked-files=all")
        nested.write_bytes(b"dirty nested version B\n")
        self.assertEqual(status, self.git(
            self.root, "status", "--porcelain", "--untracked-files=all"))
        self.assert_dirty_change_is_observed(before)

    def test_normal_clean_commit_change_is_already_stale(self):
        before = self.observe("clean_before_commit")
        (self.root / "source.txt").write_bytes(b"next committed source\n")
        self.commit(self.root, "Advance ordinary clean fixture commit")
        after = self.observe("clean_after_commit")
        self.assertFalse(after["dirty"])
        self.assertNotEqual(before["commit"], after["commit"])
        self.assertNotEqual(before["tree"], after["tree"])
        with self.assertRaisesRegex(EvidenceError, "stale"):
            self.verify(before, clean=True)
        self.verify(after, clean=True)


if __name__ == "__main__":
    unittest.main()
