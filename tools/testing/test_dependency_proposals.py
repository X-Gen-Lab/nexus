"""Weekly maintenance cannot silently promote a proposed dependency."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import subprocess
from tools.maintenance import dependency_proposals as proposals
from tools.maintenance import (cmsis_device_lock, lwip_lock, mbedtls_lock, coremqtt_lock,
                               mcuboot_lock,
                               source_catalogue, stm32f1_hal_lock, stm32f4_hal_lock,
                               stm32h5_device_lock, stm32h5_hal_lock)
from tools.maintenance.stm32_ll_lock import PROFILES
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

    def test_actual_inventory_observes_pins_not_latest_claims(self):
        expected = {item.path for item in source_catalogue.ALL_SOURCE_DEPENDENCIES}
        declared = proposals.git(ROOT, "show", "HEAD:.gitmodules")
        if any('path = ' + path not in declared and 'path=' + path not in declared
               for path in expected):
            # New dependency declarations and Gitlinks must be committed
            # together. An unfinished source-graph change remains rejected.
            with self.assertRaisesRegex(ValueError, "catalogue review|Missing committed source Gitlink"):
                proposals.inventory(ROOT)
            return
        missing = [path for path in expected
                   if not proposals.git(ROOT, "ls-tree", "HEAD", "--", path)]
        if missing:
            # A working .gitmodules addition is not a committed dependency pin.
            # Exercise the actual rejection until root commits its Gitlink.
            with self.assertRaisesRegex(ValueError, "Missing committed source Gitlink"):
                proposals.inventory(ROOT)
            return
        changed = [path for path in expected if
                   proposals.git(ROOT, "ls-tree", "HEAD", "--", path).split()[2]
                   != proposals.git(ROOT / path, "rev-parse", "HEAD")]
        if changed:
            # A staged migration is not the committed weekly inventory. The
            # ordinary hook validates that boundary instead of inventing pins.
            with self.assertRaisesRegex(ValueError, "locked clean Gitlink"):
                proposals.inventory(ROOT)
            return
        result = proposals.inventory(ROOT)
        self.assertEqual(result["owner"], "codex")
        self.assertEqual(result["mode"], "weekly_read_only")
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["upgrade_approved"])
        self.assertEqual(result["network_reads"], 0)
        self.assertGreaterEqual(len(result["dependencies"]), 8)
        for record in result["dependencies"]:
            self.assertTrue(record["revision"])
            self.assertTrue(record["identities"])

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

class DependencyInventoryGitTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dependency gitlinks ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.environment = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
        self.git(self.root, "init", "--quiet")
        self.threadx_lock = json.loads((ROOT / "dependencies/threadx.lock.json").read_text())
        self.g4_lock = json.loads((ROOT / "dependencies/stm32g4.lock.json").read_text())
        dependencies = list(source_catalogue.ALL_DEPENDENCIES)
        self.write(self.root / ".gitmodules", "".join(
            f'[submodule "{item.path}"]\npath = {item.path}\nurl = {item.fetch_url}\n'
            for item in source_catalogue.ALL_SOURCE_DEPENDENCIES).encode())
        for item in dependencies:
            checkout = self.root / item.path
            review = next((review for review in
                           cmsis_device_lock.REVIEWED_DEVICES.values()
                           if review["directory"] == item.path), None)
            for official in (stm32f1_hal_lock, stm32f4_hal_lock):
                if item.path in {official.DIRECTORY, official.CMSIS_DIRECTORY}:
                    review = {"commit": (official.COMMIT
                                          if item.path == official.DIRECTORY
                                          else official.CMSIS_COMMIT)}
            for official in (*PROFILES.values(), stm32h5_hal_lock.PROFILE):
                if item.path in {official.directory, official.cmsis_directory}:
                    review = {"commit": (official.commit
                        if item.path == official.directory else official.cmsis_commit)}
            if item.identifier in ("googletest", "mcuboot", "tomlc99", "cjson"):
                review = {"commit": json.loads(
                    (ROOT / item.source_lock).read_text())["commit"]}
            if item.identifier == "lwip":
                review = {"commit": lwip_lock.COMMIT}
            if item.identifier == "coremqtt":
                review = {"commit": coremqtt_lock.COMMIT}
            if item.source_lock == mbedtls_lock.LOCK_NAME:
                review = {"commit": mbedtls_lock.record(
                    mbedtls_lock.load_lock(), item.path)["commit"]}
            if item.identifier == "freertos" or item.parent is not None:
                review = {"commit": self.git(ROOT / item.path,
                                              "rev-parse", "HEAD")}
            if review is not None:
                # Fixed device pins use real local official Git objects. A
                # fabricated fixture commit must never relax the source lock.
                checkout.parent.mkdir(parents=True, exist_ok=True)
                self.git(self.root, "clone", "--quiet", "--shared",
                         str(ROOT / item.path), str(checkout))
                self.git(checkout, "remote", "set-url", "origin", item.fetch_url)
                self.git(checkout, "checkout", "--quiet", "--detach",
                         review["commit"])
                if item.source_lock is not None:
                    self.write(self.root / item.source_lock,
                               (ROOT / item.source_lock).read_bytes())
                for device in cmsis_device_lock.REVIEWED_DEVICES.values():
                    if device["directory"] == item.path:
                        self.write(self.root / device["lock_name"],
                                   (ROOT / device["lock_name"]).read_bytes())
                continue
            checkout.mkdir(parents=True)
            self.git(checkout, "init", "--quiet")
            self.git(checkout, "remote", "add", "origin", item.fetch_url)
            paths = [item.notice]
            if item.identifier == "threadx":
                paths.extend(self.threadx_lock["files"])
            elif item.identifier == "stm32g4_cmsis":
                paths.extend(self.g4_lock["files"])
            for name in paths:
                self.write(checkout / name, (ROOT / item.path / name).read_bytes())
            self.git(checkout, "add", ".")
            self.commit(checkout, "actual fixture dependency")
            if item.identifier == "threadx":
                self.threadx_lock["commit"] = self.git(checkout, "rev-parse", "HEAD")
                self.git(checkout, "tag", self.threadx_lock["tag"])
            elif item.identifier == "stm32g4_cmsis":
                self.g4_lock["commit"] = self.git(checkout, "rev-parse", "HEAD")
        self.write(self.root / "dependencies/threadx.lock.json",
                   (json.dumps(self.threadx_lock) + "\n").encode())
        self.write(self.root / "dependencies/stm32g4.lock.json",
                   (json.dumps(self.g4_lock) + "\n").encode())
        for name in ("dependencies/googletest.lock.json",
                     "dependencies/toolchains.lock.json", "dependencies/environment.lock.json",
                     "dependencies/development-tools.txt", "dependencies/environment-tools.txt",
                     "dependencies/actions.lock.json", "vendors/gigadevice/gd32f4xx/source.lock.json"):
            self.write(self.root / name, (ROOT / name).read_bytes())
        from tools.maintenance.freertos_transport import REVIEW_FILES
        for name in REVIEW_FILES:
            if (ROOT / name).is_file():
                self.write(self.root / name, (ROOT / name).read_bytes())
        for notice in (ROOT / "vendors/gigadevice/gd32f4xx/LICENSES").iterdir():
            self.write(self.root / "vendors/gigadevice/gd32f4xx/LICENSES" / notice.name,
                       notice.read_bytes())
        self.git(self.root, "add", ".")
        self.commit(self.root, "actual parent committed gitlinks")

    def write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def git(self, path, *arguments):
        result = subprocess.run(["git", "-C", str(path), *arguments], capture_output=True,
                                text=True, env=self.environment, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def commit(self, path, message):
        self.git(path, "-c", "user.name=Dependency test", "-c",
                 "user.email=dependency-test@local.invalid", "commit", "--quiet", "-m", message)

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
                self.assertTrue({self.root / parser.path / name
                                 for name in lock["files"]} <= observed)
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

    def test_five_committed_gitlinks_observe_actual_g4_origin_license_and_sources(self):
        result = proposals.inventory(self.root)
        record = next(x for x in result["dependencies"] if x["id"] == "stm32g4_cmsis")
        self.assertEqual(record["revision"], self.g4_lock["commit"])
        self.assertEqual(record["upstream_url"], self.g4_lock["repository"])
        observed = {Path(x["path"]).relative_to(self.root).as_posix()
                    for x in record["identities"]}
        required = {".gitmodules", "dependencies/stm32g4.lock.json"}
        required.update("vendors/st/cmsis_device_g4/" + name
                        for name in self.g4_lock["files"])
        self.assertEqual(observed, required)
        self.assertFalse(result["software_qualified"])

    def test_reviewed_device_gitlinks_keep_exact_official_source_identities(self):
        result = proposals.inventory(self.root)
        self.assertEqual(len([record for record in result["dependencies"]
                              if record.get("path")]),
                         len(source_catalogue.ALL_DEPENDENCIES))
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                record = next(record for record in result["dependencies"]
                              if record.get("path") == review["directory"])
                self.assertEqual(record["revision"], review["commit"])
                self.assertEqual(record["upstream_url"], review["origin"])
                observed = {Path(item["path"]).relative_to(self.root).as_posix()
                            for item in record["identities"]}
                expected = {".gitmodules"}
                for selected in cmsis_device_lock.REVIEWED_DEVICES.values():
                    if selected["directory"] == review["directory"]:
                        expected.add(selected["lock_name"])
                        expected.update(selected["directory"] + "/" + name
                                        for name in selected["files"])
                self.assertEqual(observed, expected)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["software_qualified"])

    def test_h5_inventory_observes_both_exact_official_source_closures(self):
        result = proposals.inventory(self.root)
        for source, names in ((stm32h5_device_lock, stm32h5_device_lock.FILES),
                              (stm32h5_hal_lock,
                               stm32h5_hal_lock.PROFILE.selected_files)):
            with self.subTest(directory=source.DIRECTORY):
                record = next(item for item in result["dependencies"]
                              if item.get("path") == source.DIRECTORY)
                expected = {".gitmodules", source.LOCK_NAME}
                expected.update(source.DIRECTORY + "/" + name
                                for name in names)
                self.assertEqual({Path(item["path"]).relative_to(
                    self.root).as_posix() for item in record["identities"]},
                    expected)
        self.assertFalse(result["source_dirty"])
        self.assertFalse(result["software_qualified"])

    def test_missing_secondary_h747_review_cannot_disappear_from_inventory(self):
        self.assertFalse(proposals.inventory(self.root)["software_qualified"])
        lock = self.root / "dependencies/stm32h747.lock.json"
        lock.unlink()
        with self.assertRaises((OSError, ValueError)):
            proposals.inventory(self.root)

    def test_secondary_h747_review_commit_must_match_actual_h7_gitlink(self):
        self.assertFalse(proposals.inventory(self.root)["software_qualified"])
        lock = self.root / "dependencies/stm32h747.lock.json"
        value = json.loads(lock.read_bytes())
        value["commit"] = "0" * 40
        lock.write_text(json.dumps(value) + "\n")
        with self.assertRaisesRegex(ValueError, "reviewed|Reviewed|Gitlink"):
            proposals.inventory(self.root)

    def test_secondary_h747_identity_cannot_substitute_primary_h743_review(self):
        self.assertFalse(proposals.inventory(self.root)["software_qualified"])
        lock = self.root / "dependencies/stm32h747.lock.json"
        lock.write_bytes((self.root /
                          "dependencies/stm32h7.lock.json").read_bytes())
        with self.assertRaisesRegex(ValueError, "selector|identity"):
            proposals.inventory(self.root)

    def test_reviewed_device_inventory_rejects_rehashed_omission_and_foreign_origin(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                path = self.root / review["lock_name"]
                original = path.read_bytes()
                changed = json.loads(original)
                del changed["files"][next(name for name in review["files"]
                                          if name.endswith("xx.h"))]
                path.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    proposals.inventory(self.root)
                path.write_bytes(original)
                checkout = self.root / review["directory"]
                self.git(checkout, "remote", "set-url", "origin",
                         "https://example.com/foreign-device.git")
                with self.assertRaises(ValueError):
                    proposals.inventory(self.root)
                self.git(checkout, "remote", "set-url", "origin",
                         source_catalogue.dependency(review["directory"]).fetch_url)
                self.assertFalse(proposals.inventory(self.root)["source_dirty"])

    def test_reviewed_device_inventory_requires_committed_gitlinks(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                self.git(self.root, "rm", "--cached", review["directory"])
                self.commit(self.root, "actual parent without device Gitlink")
                with self.assertRaisesRegex(ValueError, "Missing committed source Gitlink"):
                    proposals.inventory(self.root)
                self.git(self.root, "add", review["directory"])
                self.commit(self.root, "restore exact official device Gitlink")
                self.assertFalse(proposals.inventory(self.root)["source_dirty"])

    def test_g4_uncommitted_gitlink_foreign_origin_and_rehashed_omission_reject(self):
        path = self.root / "dependencies/stm32g4.lock.json"
        changed = copy.deepcopy(self.g4_lock)
        del changed["files"]["Include/stm32g474xx.h"]
        path.write_text(json.dumps(changed))
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)
        path.write_text(json.dumps(self.g4_lock) + "\n")
        self.git(self.root / "vendors/st/cmsis_device_g4", "remote", "set-url", "origin",
                 "https://example.com/g4.git")
        with self.assertRaises(ValueError):
            proposals.inventory(self.root)
        self.git(self.root / "vendors/st/cmsis_device_g4", "remote", "set-url", "origin",
                 source_catalogue.dependency("stm32g4_cmsis").fetch_url)
        self.git(self.root, "rm", "--cached", "vendors/st/cmsis_device_g4")
        self.commit(self.root, "actual parent without G4 gitlink")
        with self.assertRaisesRegex(ValueError, "Missing committed source Gitlink"):
            proposals.inventory(self.root)

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


if __name__ == "__main__":
    unittest.main()
