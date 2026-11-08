"""Weekly maintenance cannot silently promote a proposed dependency."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch as mock_patch
import subprocess
from tools.maintenance import dependency_proposals as proposals
from tools.maintenance import (cmsis_device_lock, lwip_lock, mbedtls_lock, coremqtt_lock,
                               mcuboot_lock,
                               source_catalogue, stm32f4_hal_lock)
from tools.dev.dependencies import required_submodules
from tools.testing.inventory_fixtures import write_files
import os

ROOT = Path(__file__).resolve().parents[2]

class DependencyProposalTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.archive = self.folder / "candidate.tar"; self.archive.write_bytes(b"candidate source archive")
        self.notice = self.folder / "LICENSE.txt"; self.notice.write_bytes(b"Candidate license notice")
        self.plan = {"schema_version": 1, "dependency": "freertos", "proposed_revision": "b"*40,
                     "source_url": "https://example.com/source.tar", "archive": self.file(self.archive),
                     "license": {"expression": "MIT", "notices": [self.file(self.notice)]}, "evidence": {}}
        self.current = {"dependencies": [{"id": "freertos", "revision": "a"*40, "kind": "git_source"}]}

    def file(self, path):
        data = path.read_bytes()
        return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}


    def test_archive_and_license_identity_produce_only_pending_proposal(self):
        result = proposals.propose(self.plan, self.current)
        self.assertEqual(result["status"], "requires_review_and_execution")
        self.assertFalse(result["upgrade_approved"])
        self.assertFalse(result["locks_changed"])
        self.assertEqual(set(result["gate_matrix"]), set(proposals.GATES))
        self.assertTrue(all(v["status"] == "not_executed" for v in result["gate_matrix"].values()))

    def test_missing_or_unknown_plan_field_rejects(self):
        for key in self.plan:
            plan = copy.deepcopy(self.plan); del plan[key]
            with self.subTest(key=key), self.assertRaises(ValueError): proposals.propose(plan, self.current)
        plan = copy.deepcopy(self.plan); plan["auto_merge"] = True
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_unknown_dependency_and_same_revision_reject(self):
        for dependency, revision in (("unknown", "b"*40), ("freertos", "a"*40)):
            plan = copy.deepcopy(self.plan); plan.update(dependency=dependency, proposed_revision=revision)
            with self.subTest(dependency=dependency), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_unpinned_git_revision_rejects(self):
        for revision in ("main", "latest", "b"*39, True):
            plan = copy.deepcopy(self.plan); plan["proposed_revision"] = revision
            with self.subTest(revision=revision), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_candidate_archive_changed_or_empty_rejects(self):
        for content in (b"changed", b""):
            self.archive.write_bytes(content)
            with self.subTest(content=content), self.assertRaises(ValueError): proposals.propose(self.plan, self.current)

    def test_candidate_archive_symlink_rejects(self):
        alias = self.folder / "alias.tar"; alias.symlink_to(self.archive)
        plan = copy.deepcopy(self.plan); plan["archive"]["path"] = str(alias)
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_missing_or_changed_license_notice_rejects(self):
        self.notice.write_bytes(b"different terms")
        with self.assertRaises(ValueError): proposals.propose(self.plan, self.current)
        self.notice.unlink()
        with self.assertRaises(ValueError): proposals.propose(self.plan, self.current)

    def test_license_is_reviewed_not_guessed(self):
        for expression in ("", "unknown", True):
            plan = copy.deepcopy(self.plan); plan["license"]["expression"] = expression
            with self.subTest(expression=expression), self.assertRaises(ValueError): proposals.propose(plan, self.current)
        plan = copy.deepcopy(self.plan); plan["license"]["notices"] = []
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_bare_passed_marker_never_approves_upgrade(self):
        report = self.folder / "fake.json"; report.write_text('{"status":"passed"}')
        plan = copy.deepcopy(self.plan); plan["evidence"] = {gate: [self.file(report)] for gate in proposals.GATES}
        result = proposals.propose(plan, self.current)
        self.assertFalse(result["upgrade_approved"])
        self.assertTrue(all(v["status"] == "referenced_unreviewed" for v in result["gate_matrix"].values()))
        self.assertFalse(result["software_qualified"])

    def test_unknown_evidence_gate_and_empty_reference_rejects(self):
        for evidence in ({"fake_gate": [self.file(self.archive)]}, {"board_arm": []}):
            plan = copy.deepcopy(self.plan); plan["evidence"] = evidence
            with self.subTest(evidence=evidence), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_stale_evidence_reference_rejects(self):
        plan = copy.deepcopy(self.plan); plan["evidence"] = {"configuration": [self.file(self.notice)]}
        self.notice.write_bytes(b"new report")
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_sources_with_credentials_or_non_https_reject(self):
        for url in ("http://example.com/src", "https://user:secret@example.com/src", "file:///tmp/source", "https://example.com/src?token=private"):
            plan = copy.deepcopy(self.plan); plan["source_url"] = url
            with self.subTest(url=url), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_proposal_does_not_modify_inputs_or_locks(self):
        before = {p: p.read_bytes() for p in (self.archive, self.notice, ROOT/'dependencies/toolchains.lock.json', ROOT/'dependencies/googletest.lock.json')}
        proposals.propose(self.plan, self.current)
        self.assertEqual(before, {p: p.read_bytes() for p in before})

    def test_boolean_schema_is_rejected(self):
        plan = copy.deepcopy(self.plan); plan["schema_version"] = True
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)


    def upstream_current(self):
        return {"dependencies": [{"id": "freertos", "revision": "a"*40,
                                  "kind": "git_source",
                                  "upstream_url": "https://github.com/FreeRTOS/FreeRTOS-Kernel.git"}]}

    def capture(self, step, substitutions, output):
        response = output / (step["name"] + ".stdout.bin")
        error = output / (step["name"] + ".stderr.bin")
        response.write_bytes(b"b"*40 + b"\trefs/tags/V11.3.0\n")
        error.write_bytes(b"")
        return {"status": "captured", "process_started": True,
                "exit_code": 0, "argv": step["arguments"],
                "stdout": self.file(response),
                "stderr": {"path": str(error), "sha256": hashlib.sha256(b"").hexdigest(), "size": 0}}

    def test_explicit_upstream_records_raw_refs_without_upgrading(self):
        output = self.folder / "upstream"
        result = proposals.observe_upstream(self.upstream_current(), output, runner=self.capture)
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["upgrade_approved"])
        self.assertEqual(result["observations"][0]["status"], "observed")
        self.assertEqual(result["observations"][0]["refs"][0]["revision"], "b"*40)
        self.assertTrue((output / "observations.json").is_file())

    def test_upstream_observation_compares_official_base_not_transport_commit(self):
        current = self.upstream_current()
        item = current["dependencies"][0]
        item.update(revision="c" * 40, upstream_revision="a" * 40,
                    fetch_url="https://github.com/X-Gen-Lab/FreeRTOS-Kernel.git")
        result = proposals.observe_upstream(current, self.folder / "split",
                                             runner=self.capture)
        observed = result["observations"][0]
        self.assertEqual(observed["current_revision"], "a" * 40)
        self.assertEqual(observed["selected_revision"], "c" * 40)
        self.assertEqual(observed["source_url"], item["upstream_url"])

    def test_official_base_is_not_a_new_version_of_transport_commit(self):
        self.current["dependencies"][0].update(
            revision="c" * 40, upstream_revision="a" * 40)
        self.plan["proposed_revision"] = "a" * 40
        with self.assertRaisesRegex(ValueError, "Distinct"):
            proposals.propose(self.plan, self.current)

    def test_upstream_upgrade_leaves_required_transport_revision_pending(self):
        self.current["dependencies"][0].update(
            revision="c" * 40, upstream_revision="a" * 40)
        result = proposals.propose(self.plan, self.current)
        self.assertEqual(result["current_upstream_revision"], "a" * 40)
        self.assertEqual(result["selected_revision"], "c" * 40)
        self.assertEqual(result["proposed_revision"], "b" * 40)
        self.assertIsNone(result["proposed_selected_revision"])
        self.assertEqual(result["transport_review"], "required")

    def test_official_nested_url_without_git_suffix_is_observed(self):
        current = self.upstream_current()
        current["dependencies"][0]["upstream_url"] = (
            "https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports")
        result = proposals.observe_upstream(current, self.folder / "nested",
                                             runner=self.capture)
        self.assertEqual(result["observations"][0]["status"], "observed")

    def test_failed_network_stays_not_observed_with_actual_exit(self):
        def failure(step, substitutions, output):
            result = self.capture(step, substitutions, output)
            result.update(status="failed", exit_code=128)
            return result
        result = proposals.observe_upstream(self.upstream_current(), self.folder / "failed", runner=failure)
        self.assertEqual(result["observations"][0]["status"], "not_observed")
        self.assertEqual(result["observations"][0]["request"]["exit_code"], 128)

    def test_upstream_origin_without_public_allowed_host_rejects_before_request(self):
        current = self.upstream_current(); current["dependencies"][0]["upstream_url"] = "https://user:password@evil.example/source.git"
        with self.assertRaises(ValueError):
            proposals.observe_upstream(current, self.folder / "bad", runner=lambda *_: self.fail("unexpected request"))

    def test_existing_or_symlink_output_cannot_replace_evidence(self):
        output = self.folder / "retained"; output.mkdir(); (output / "raw.bin").write_bytes(b"retained")
        with self.assertRaises(ValueError): proposals.observe_upstream(self.upstream_current(), output, runner=self.capture)
        alias = self.folder / "alias"; alias.symlink_to(output, target_is_directory=True)
        with self.assertRaises(ValueError): proposals.observe_upstream(self.upstream_current(), alias / "new", runner=self.capture)
        self.assertEqual((output / "raw.bin").read_bytes(), b"retained")

    def test_malformed_remote_refs_do_not_become_observed(self):
        def malformed(step, substitutions, output):
            result = self.capture(step, substitutions, output)
            Path(result["stdout"]["path"]).write_bytes(b"not a ref response\n")
            result["stdout"] = self.file(Path(result["stdout"]["path"]))
            return result
        result = proposals.observe_upstream(self.upstream_current(), self.folder / "malformed", runner=malformed)
        self.assertEqual(result["observations"][0]["status"], "not_observed")

    def test_upstream_invocation_is_no_shell_noninteractive_and_bounded(self):
        def checked(step, substitutions, output):
            self.assertLessEqual(step["timeout_s"], 10)
            self.assertLessEqual(step["maximum_output_bytes"], 1024*1024)
            arguments = step["arguments"]
            self.assertIn("GIT_CONFIG_GLOBAL=/dev/null", arguments)
            self.assertIn("GIT_TERMINAL_PROMPT=0", arguments)
            self.assertIn("ls-remote", arguments)
            self.assertIn("--refs", arguments)
            return self.capture(step, substitutions, output)
        proposals.observe_upstream(self.upstream_current(), self.folder / "bounded", runner=checked)


    def test_upstream_git_does_not_inherit_capture_repository_local_configuration(self):
        def checked(step, substitutions, output):
            arguments = step["arguments"]
            self.assertIn("-C", arguments)
            directory = Path(arguments[arguments.index("-C") + 1])
            self.assertTrue(directory.is_dir())
            self.assertFalse((directory / ".git").exists())
            self.assertIn("GIT_CEILING_DIRECTORIES=" + str(directory.parent), arguments)
            return self.capture(step, substitutions, output)
        proposals.observe_upstream(self.upstream_current(), self.folder / "isolated", runner=checked)


    def test_parent_traversal_cannot_write_from_build_into_source(self):
        path = ROOT / "build" / ".." / "dependencies" / "proposal-output-must-not-exist"
        with self.assertRaises(ValueError):
            proposals.observe_upstream(self.upstream_current(), path, runner=lambda *_: self.fail("unexpected request"))
        self.assertFalse((ROOT / "dependencies/proposal-output-must-not-exist").exists())

class _InventoryGitFixture(unittest.TestCase):
    """Real Git and validators, with explicitly owned external catalogue data."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dependency gitlinks ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.environment = {k: v for k, v in os.environ.items()
                            if not k.startswith("GIT_")}
        self.git(self.root, "init", "--quiet")
        for name in ("dependencies/googletest.lock.json",
                     "dependencies/toolchains.lock.json",
                     "dependencies/environment.lock.json",
                     "dependencies/development-tools.txt",
                     "dependencies/environment-tools.txt",
                     "dependencies/actions.lock.json",
                     "vendors/gigadevice/gd32f4xx/source.lock.json"):
            self.write(self.root / name, (ROOT / name).read_bytes())
        from tools.maintenance.freertos_transport import REVIEW_FILES
        for name in REVIEW_FILES:
            if (ROOT / name).is_file():
                self.write(self.root / name, (ROOT / name).read_bytes())
        directory = Path("vendors/gigadevice/gd32f4xx/LICENSES")
        for notice in (ROOT / directory).iterdir():
            self.write(self.root / directory / notice.name, notice.read_bytes())

    def catalogue(self, entries):
        source_catalogue.validate_catalogue(entries)
        roots = tuple(item for item in entries if item.parent is None)
        for module, name, value in (
                (source_catalogue, "ALL_DEPENDENCIES", entries),
                (source_catalogue, "ALL_SOURCE_DEPENDENCIES", roots),
                (proposals, "ALL_DEPENDENCIES", entries)):
            patch = mock_patch.object(module, name, value)
            patch.start()
            self.addCleanup(patch.stop)
        self.write(self.root / ".gitmodules", "".join(
            f'[submodule "{source_catalogue.module_name(item)}"]\n'
            f'path = {source_catalogue.relative_path(item)}\n'
            f'url = {item.fetch_url}\n' for item in roots).encode())

    def write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def git(self, path, *arguments):
        result = subprocess.run(["git", "-C", str(path), *arguments],
            capture_output=True, text=True, env=self.environment, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def commit(self, path, message):
        self.git(path, "-c", "user.name=Dependency test", "-c",
                 "user.email=dependency-test@local.invalid", "commit",
                 "--quiet", "-m", message)


class DependencyInventoryGitTests(_InventoryGitFixture):
    """An independent catalogue fixture using the selected F4/service sources.

    This is neither the complete production inventory nor Native's closure.
    FreeRTOS's full child graph has separate transport contracts; unselected
    device verifiers use synthetic inputs in their own maintained suites.
    """

    def setUp(self):
        super().setUp()
        selected = set(required_submodules("native")) - {"ext/freertos"}
        entries = tuple(item for item in source_catalogue.ALL_DEPENDENCIES
                        if any(item.path == name or
                               item.path.startswith(name + "/")
                               for name in selected))
        self.catalogue(entries)
        self.entries = entries
        self.threadx_lock = json.loads(
            (ROOT / "dependencies/threadx.lock.json").read_text())
        for item in entries:
            checkout = self.root / item.path
            review = None
            official = stm32f4_hal_lock
            if item.path in {official.DIRECTORY, official.CMSIS_DIRECTORY}:
                review = (official.COMMIT if item.path == official.DIRECTORY
                          else official.CMSIS_COMMIT)
            if item.identifier in {"googletest", "mcuboot", "tomlc99", "cjson"}:
                review = json.loads((ROOT / item.source_lock).read_text())["commit"]
            if item.identifier == "lwip":
                review = lwip_lock.COMMIT
            if item.identifier == "coremqtt":
                review = coremqtt_lock.COMMIT
            if item.source_lock == mbedtls_lock.LOCK_NAME:
                review = mbedtls_lock.record(
                    mbedtls_lock.load_lock(), item.path)["commit"]
            if item.parent is not None:
                review = self.git(ROOT / item.path, "rev-parse", "HEAD")
            if review is not None:
                checkout.parent.mkdir(parents=True, exist_ok=True)
                self.git(self.root, "clone", "--quiet", "--shared",
                         str(ROOT / item.path), str(checkout))
                self.git(checkout, "remote", "set-url", "origin", item.fetch_url)
                self.git(checkout, "checkout", "--quiet", "--detach", review)
                if item.source_lock is not None:
                    self.write(self.root / item.source_lock,
                               (ROOT / item.source_lock).read_bytes())
                continue
            checkout.mkdir(parents=True)
            self.git(checkout, "init", "--quiet")
            self.git(checkout, "remote", "add", "origin", item.fetch_url)
            paths = {item.notice}
            if item.identifier == "threadx":
                paths.update(self.threadx_lock["files"])
            for name in paths:
                self.write(checkout / name, (ROOT / item.path / name).read_bytes())
            self.git(checkout, "add", ".")
            self.commit(checkout, "actual fixture dependency")
            if item.identifier == "threadx":
                self.threadx_lock["commit"] = self.git(checkout, "rev-parse", "HEAD")
                self.git(checkout, "tag", self.threadx_lock["tag"])
        self.write(self.root / "dependencies/threadx.lock.json",
                   (json.dumps(self.threadx_lock) + "\n").encode())
        self.git(self.root, "add", ".")
        self.commit(self.root, "actual parent committed gitlinks")

    def test_actual_fixture_inventory_observes_pins_without_mutation(self):
        before = self.git(self.root, "rev-parse", "HEAD")
        result = proposals.inventory(self.root)
        self.assertEqual(result["owner"], "codex")
        self.assertEqual(result["mode"], "weekly_read_only")
        self.assertEqual(result["source_revision"], before)
        self.assertEqual(self.git(self.root, "rev-parse", "HEAD"), before)
        self.assertEqual(self.git(self.root, "status", "--porcelain"), "")
        for key in ("source_dirty", "locks_changed", "upgrade_approved",
                    "software_qualified"):
            self.assertFalse(result[key], key)
        self.assertEqual(result["network_reads"], 0)
        observed = {item["path"] for item in result["dependencies"]
                    if "path" in item}
        self.assertEqual(observed, {item.path for item in self.entries})

    def test_host_parser_inventory_records_its_locked_host_source_closure(self):
        inventory = proposals.inventory(self.root)
        for parser in source_catalogue.host_configuration_sources():
            with self.subTest(parser=parser.identifier):
                record = next(item for item in inventory["dependencies"]
                              if item["id"] == parser.identifier)
                lock = json.loads((self.root / parser.source_lock).read_text())
                self.assertEqual(record["revision"], lock["commit"])
                self.assertEqual(record["upstream_url"], parser.upstream_url)
                observed = {Path(item["path"]) for item in record["identities"]}
                self.assertLessEqual({self.root / parser.path / name
                                      for name in lock["files"]}, observed)
                self.assertIn(self.root / parser.source_lock, observed)

    def test_host_parser_rehashed_omission_and_hidden_source_change_reject(self):
        parser = source_catalogue.dependency("tomlc99")
        lock_path = self.root / parser.source_lock
        original_lock = lock_path.read_bytes()
        lock = json.loads(original_lock)
        lock["files"].pop("toml.c")
        lock_path.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError, "Host parser"):
            proposals.inventory(self.root)
        lock_path.write_bytes(original_lock)
        checkout = self.root / parser.path
        source = checkout / "toml.c"
        original_source = source.read_bytes()
        self.git(checkout, "update-index", "--skip-worktree", "toml.c")
        source.write_bytes(original_source + b"\n/* hidden changed bytes */\n")
        self.assertEqual(self.git(checkout, "status", "--porcelain"), "")
        with self.assertRaisesRegex(ValueError, "Host parser"):
            proposals.inventory(self.root)
        lock = json.loads(original_lock)
        lock["files"]["toml.c"] = {
            "bytes": source.stat().st_size,
            "sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
        lock_path.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError, "Host parser"):
            proposals.inventory(self.root)

    def test_googletest_inventory_records_gitlink_and_upstream_license(self):
        result = proposals.inventory(self.root)
        record = next(item for item in result["dependencies"]
                      if item["id"] == "googletest")
        self.assertEqual(record.get("path"), "ext/googletest")
        self.assertEqual(record["revision"],
                         self.git(self.root / "ext/googletest",
                                  "rev-parse", "HEAD"))
        self.assertEqual(record["upstream_revision"],
                         "6910c9d9165801d8827d628cb72eb7ea9dd538c5")
        self.assertNotEqual(record["revision"], record["upstream_revision"])
        self.assertEqual(record["upstream_url"],
                         "https://github.com/google/googletest.git")
        self.assertEqual({Path(item["path"]).relative_to(self.root).as_posix()
                          for item in record["identities"]},
                         {".gitmodules", "dependencies/googletest.lock.json",
                          "ext/googletest/LICENSE"})

    def test_committed_gitlinks_observe_real_threadx_identity_and_all_sources(self):
        result = proposals.inventory(self.root)
        record = next(x for x in result["dependencies"] if x["id"] == "threadx")
        self.assertEqual(record["revision"], self.threadx_lock["commit"])
        self.assertEqual(record["upstream_url"], "https://github.com/eclipse-threadx/threadx.git")
        self.assertGreaterEqual(len(record["identities"]), 217)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["software_qualified"])

    def test_lwip_inventory_records_real_pin_closure_and_license(self):
        result = proposals.inventory(self.root)
        record = next(item for item in result["dependencies"]
                      if item["id"] == "lwip")
        self.assertEqual(record["revision"], lwip_lock.COMMIT)
        self.assertEqual(record["upstream_revision"], lwip_lock.COMMIT)
        self.assertEqual(record["upstream_url"], lwip_lock.UPSTREAM)
        self.assertEqual(record["fetch_url"], lwip_lock.FETCH)
        lock = json.loads((self.root / "dependencies/lwip.lock.json").read_text())
        self.assertEqual(len(lock["files"]), 311)
        observed = {Path(item["path"]).relative_to(self.root).as_posix()
                    for item in record["identities"]}
        expected = {".gitmodules", "dependencies/lwip.lock.json"}
        expected.update("ext/lwip/" + name for name in lock["files"])
        self.assertEqual(observed, expected)
        self.assertIn("ext/lwip/COPYING", observed)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["software_qualified"])

    def test_mcuboot_inventory_records_exact_pin_closure_and_upstream(self):
        result = proposals.inventory(self.root)
        record = next(item for item in result["dependencies"]
                      if item["id"] == "mcuboot")
        self.assertEqual(record["revision"], mcuboot_lock.COMMIT)
        self.assertEqual(record["upstream_revision"],
                         "6d3b3d2c38ab20c242e5b9abb04d050086383eb2")
        self.assertEqual(record["upstream_url"],
                         "https://github.com/mcu-tools/mcuboot.git")
        self.assertEqual(record["fetch_url"],
                         "https://github.com/X-Gen-Lab/mcuboot.git")
        lock = json.loads((self.root / "dependencies/mcuboot.lock.json").read_text())
        self.assertEqual(len(lock["files"]), 80)
        observed = {Path(item["path"]).relative_to(self.root).as_posix()
                    for item in record["identities"]}
        expected = {".gitmodules", "dependencies/mcuboot.lock.json"}
        expected.update("ext/mcuboot/" + name for name in lock["files"])
        self.assertEqual(observed, expected)
        self.assertIn("ext/mcuboot/NEXUS_CURATED_SOURCE.md", observed)
        self.assertIn("ext/mcuboot/LICENSE", observed)
        self.assertIn("ext/mcuboot/NOTICE", observed)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["software_qualified"])

    def test_mcuboot_inventory_rejects_rehashed_omission_and_foreign_origin(self):
        lock = self.root / "dependencies/mcuboot.lock.json"
        original = lock.read_bytes()
        changed = json.loads(original)
        del changed["files"]["boot/bootutil/src/bootutil_public.c"]
        lock.write_text(json.dumps(changed))
        with self.assertRaisesRegex(ValueError,
                                    "MCUboot reviewed source/notice closure"):
            proposals.inventory(self.root)
        lock.write_bytes(original)
        checkout = self.root / "ext/mcuboot"
        self.git(checkout, "remote", "set-url", "origin",
                 "https://github.com/mcu-tools/mcuboot.git")
        with self.assertRaisesRegex(ValueError,
                                    "Source checkout differs.*ext/mcuboot"):
            proposals.inventory(self.root)
        self.git(checkout, "remote", "set-url", "origin",
                 "https://github.com/X-Gen-Lab/mcuboot.git")
        self.assertFalse(proposals.inventory(self.root)["source_dirty"])

    def test_mcuboot_inventory_rechecks_bytes_hidden_from_git_status(self):
        checkout = self.root / "ext/mcuboot"
        relative = "boot/bootutil/src/bootutil_public.c"
        source = checkout / relative
        original = source.read_bytes()
        self.git(checkout, "update-index", "--assume-unchanged", relative)
        source.write_bytes(original + b"\n/* changed inventory observation */\n")
        self.assertEqual(self.git(checkout, "status", "--porcelain"), "")
        with self.assertRaisesRegex(ValueError,
                                    "MCUboot selected source bytes differ"):
            proposals.inventory(self.root)
        source.write_bytes(original)
        self.git(checkout, "update-index", "--no-assume-unchanged", relative)
        self.assertFalse(proposals.inventory(self.root)["source_dirty"])

    def test_lwip_inventory_rejects_rehashed_omission_and_foreign_transport(self):
        lock = self.root / "dependencies/lwip.lock.json"
        original = lock.read_bytes()
        changed = json.loads(original)
        del changed["files"]["src/core/tcp.c"]
        lock.write_text(json.dumps(changed))
        with self.assertRaisesRegex(ValueError, "lwIP reviewed source closure"):
            proposals.inventory(self.root)
        lock.write_bytes(original)
        checkout = self.root / "ext/lwip"
        self.git(checkout, "remote", "set-url", "origin", lwip_lock.UPSTREAM)
        with self.assertRaisesRegex(ValueError, "Source checkout differs.*ext/lwip"):
            proposals.inventory(self.root)
        self.git(checkout, "remote", "set-url", "origin", lwip_lock.FETCH)
        self.assertFalse(proposals.inventory(self.root)["source_dirty"])

    def test_lwip_inventory_rechecks_source_hidden_from_git_status(self):
        checkout = self.root / "ext/lwip"
        relative = "src/core/tcp.c"
        source = checkout / relative
        original = source.read_bytes()
        self.git(checkout, "update-index", "--assume-unchanged", relative)
        source.write_bytes(original + b"\n/* changed source observation */\n")
        self.assertEqual(self.git(checkout, "status", "--porcelain"), "")
        with self.assertRaisesRegex(ValueError, "lwIP source bytes differ"):
            proposals.inventory(self.root)
        source.write_bytes(original)
        self.git(checkout, "update-index", "--no-assume-unchanged", relative)
        self.assertFalse(proposals.inventory(self.root)["source_dirty"])










    def test_checkout_change_and_foreign_origin_reject_without_repair(self):
        file = self.root / "ext/threadx" / self.threadx_lock["common"][0]
        old = file.read_bytes()
        file.write_bytes(b"corrupt source")
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)
        file.write_bytes(old)
        self.git(self.root / "ext/threadx", "remote", "set-url", "origin",
                 "https://example.com/threadx.git")
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)

    def test_uncommitted_gitlink_and_unreviewed_module_declaration_reject(self):
        self.git(self.root, "rm", "--cached", "ext/threadx")
        self.commit(self.root, "parent does not own threadx gitlink")
        with self.assertRaisesRegex(ValueError, "Missing committed source Gitlink"):
            proposals.inventory(self.root)
        with (self.root / ".gitmodules").open("a") as stream:
            stream.write('\n[submodule "ext/unknown"]\npath=ext/unknown\nurl=https://example.com/unknown.git\n')
        with self.assertRaisesRegex(ValueError, "catalogue review"):
            proposals.inventory(self.root)

    def test_rehashed_lock_omission_and_wrong_locked_commit_reject(self):
        path = self.root / "dependencies/threadx.lock.json"
        altered = copy.deepcopy(self.threadx_lock)
        name = altered["common"].pop()
        del altered["files"][name]
        path.write_text(json.dumps(altered))
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)
        altered = copy.deepcopy(self.threadx_lock)
        altered["commit"] = "a" * 40
        path.write_text(json.dumps(altered))
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)


class DependencyInventorySelectorsTests(_InventoryGitFixture):
    """One real Gitlink must close every independently reviewed selector."""

    def setUp(self):
        super().setUp()
        self.dependency = source_catalogue.SourceDependency(
            "fixture_device", "vendors/fixture/device",
            "https://github.com/example/fixture-device.git",
            "https://github.com/example/fixture-device-mirror.git",
            "inventory", "LICENSE.md", "dependencies/primary.lock.json")
        self.catalogue((self.dependency,))
        self.checkout = self.root / self.dependency.path
        files = {"files": dict.fromkeys(
            ("LICENSE.md", "Include/primaryxx.h", "Include/secondaryxx.h"))}
        write_files(self.checkout, files)
        self.git(self.checkout, "init", "--quiet")
        self.git(self.checkout, "remote", "add", "origin",
                 self.dependency.fetch_url)
        self.git(self.checkout, "add", ".")
        self.commit(self.checkout, "synthetic reviewed selector sources")
        pin = self.git(self.checkout, "rev-parse", "HEAD")
        reviews = {}
        for name in ("primary", "secondary"):
            part = "FIXTURE_" + name.upper()
            names = frozenset({"LICENSE.md", "Include/" + name + "xx.h"})
            review = {"origin": self.dependency.upstream_url,
                      "commit": pin, "directory": self.dependency.path,
                      "lock_name": "dependencies/" + name + ".lock.json",
                      "files": names}
            reviews[part] = review
            lock = {"schema_version": 1, "kind": "nexus-reviewed-device-source",
                    "repository": review["origin"], "commit": pin,
                    "selected_part": part,
                    "files": {key: files["files"][key] for key in sorted(names)},
                    "license": {"path": "LICENSE.md", "expression": "Apache-2.0"}}
            self.write(self.root / review["lock_name"],
                       (json.dumps(lock) + "\n").encode())
        patch = mock_patch.object(cmsis_device_lock, "REVIEWED_DEVICES", reviews)
        patch.start()
        self.addCleanup(patch.stop)
        self.git(self.root, "add", ".")
        self.commit(self.root, "parent owns one reviewed selector Gitlink")
        self.primary = self.root / "dependencies/primary.lock.json"
        self.secondary = self.root / "dependencies/secondary.lock.json"

    def verified(self):
        result = proposals.inventory(self.root)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["software_qualified"])
        return next(item for item in result["dependencies"]
                    if item.get("id") == self.dependency.identifier)

    def test_both_selector_locks_and_source_closures_are_observed(self):
        record = self.verified()
        observed = {Path(item["path"]).relative_to(self.root).as_posix()
                    for item in record["identities"]}
        self.assertEqual(observed, {
            ".gitmodules", "dependencies/primary.lock.json",
            "dependencies/secondary.lock.json",
            "vendors/fixture/device/LICENSE.md",
            "vendors/fixture/device/Include/primaryxx.h",
            "vendors/fixture/device/Include/secondaryxx.h"})
        self.assertEqual(record["revision"],
                         self.git(self.checkout, "rev-parse", "HEAD"))

    def test_missing_secondary_lock_cannot_disappear(self):
        self.verified()
        self.secondary.unlink()
        with self.assertRaisesRegex(ValueError, r"secondary\.lock\.json"):
            proposals.inventory(self.root)

    def test_secondary_commit_must_match_review_and_gitlink(self):
        self.verified()
        lock = json.loads(self.secondary.read_text())
        lock["commit"] = "0" * 40
        self.secondary.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError,
                                    "Reviewed official origin, part and commit"):
            proposals.inventory(self.root)

    def test_primary_selector_cannot_substitute_secondary(self):
        self.verified()
        self.secondary.write_bytes(self.primary.read_bytes())
        with self.assertRaisesRegex(ValueError, "selector identity changed"):
            proposals.inventory(self.root)

    def test_secondary_bytes_hidden_from_git_status_are_rechecked(self):
        self.verified()
        name = "Include/secondaryxx.h"
        self.git(self.checkout, "update-index", "--assume-unchanged", name)
        source = self.checkout / name
        source.write_bytes(source.read_bytes() + b"changed secondary bytes\n")
        self.assertEqual(self.git(self.checkout, "status", "--porcelain"), "")
        with self.assertRaisesRegex(ValueError, "Device source/license identity"):
            proposals.inventory(self.root)

    def test_reviewed_source_origin_cannot_change(self):
        self.verified()
        self.git(self.checkout, "remote", "set-url", "origin",
                 "https://github.com/example/foreign-device.git")
        with self.assertRaisesRegex(ValueError,
                                    "Source checkout differs.*locked clean"):
            proposals.inventory(self.root)

    def test_reviewed_source_requires_a_committed_parent_gitlink(self):
        self.verified()
        self.git(self.root, "rm", "--cached", self.dependency.path)
        self.commit(self.root, "parent no longer owns reviewed source")
        with self.assertRaisesRegex(ValueError, "Missing committed source Gitlink"):
            proposals.inventory(self.root)


if __name__ == "__main__":
    unittest.main()
