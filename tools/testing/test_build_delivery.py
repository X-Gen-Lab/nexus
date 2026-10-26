"""Boundary contracts for content-addressed build and delivery tooling."""

from __future__ import annotations

import hashlib
import json
import os
import zipfile
from pathlib import Path
import tempfile
import shutil
import unittest
from unittest.mock import patch

from tools.dev import cache, dependencies, qualification
from tools.evidence import environment, store
from tools.evidence.common import EvidenceError


class BuildDeliveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def file(self, name, data=b"actual bytes"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def wheel(self, name="fixture"):
        path = self.root / "wheels" / (name + "-1.0-py3-none-any.whl")
        path.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr(name + "-1.0.dist-info/METADATA", "Metadata-Version: 2.1\nName: " + name + "\nVersion: 1.0\n")
        return path

    def test_candidate_plan_requires_owned_verify_terminal(self):
        plan = {"schema_version": 1, "stages": [{"name": "tools", "checker": "tool_tests", "args": []}]}
        with self.assertRaises(EvidenceError):
            qualification.validate_plan(plan, require_candidate=True)
        plan["stages"].append({"name": "candidate", "checker": "candidate_verify", "args": ["--candidate", "candidate.json"]})
        qualification.validate_plan(plan, require_candidate=True)

    def test_cache_namespace_report_cannot_replace_compiler(self):
        compiler = self.file("compiler", b"unchanged compiler")
        result = cache.main(["namespace", "--compiler", str(compiler),
                             "--output", str(compiler)])
        self.assertEqual(result, 1)
        self.assertEqual(compiler.read_bytes(), b"unchanged compiler")

    def test_store_report_cannot_replace_original_archive(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        before = archive.read_bytes()
        result = store.main(["adopt", "--source", str(archive),
                             "--store", str(self.root / "durable"),
                             "--report", str(archive)])
        self.assertEqual(result, 1)
        self.assertEqual(archive.read_bytes(), before)

    def test_durable_hardlink_survives_removed_original_name(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        adopted = store.adopt(archive, self.root / "durable")
        archive.unlink()
        self.assertEqual(store.verify_external(adopted)["status"], "verified")

    def test_hardlink_receipt_does_not_assert_durable_storage(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        adopted = store.adopt(Path(package["archive"]["path"]),
                              self.root / "local")
        self.assertEqual(adopted["kind"], "preserved_external_archive")
        self.assertEqual(adopted["persistence_status"], "not_established")
        self.assertEqual(store.verify_external(adopted)["persistence_status"],
                         "not_established")

    def test_independent_copy_survives_source_mutation_and_restores(self):
        self.file("source/input", b"executed command\n")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        copied = store.copy_archive(archive, self.root / "separate")
        destination = Path(copied["archive"]["path"])
        self.assertFalse(archive.samefile(destination))
        self.assertEqual(copied["storage"], "independent_copy")
        self.assertEqual(copied["persistence_status"], "not_established")
        archive.write_bytes(b"source damaged after preservation")
        receipt = store.restore_tree(package, self.root / "restored",
                                     archive_override=destination)
        self.assertEqual(receipt["status"], "verified")
        self.assertEqual((self.root / "restored/input").read_bytes(),
                         b"executed command\n")
        self.assertEqual(store.verify_external(copied)["status"], "verified")

    def test_copy_rejects_existing_hardlink_to_original(self):
        import os
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        destination = self.root / "copy" / archive.name
        destination.parent.mkdir()
        os.link(archive, destination)
        with self.assertRaises(EvidenceError):
            store.copy_archive(archive, destination.parent)
        self.assertTrue(archive.samefile(destination))

    def test_copy_rejects_unsafe_archive_without_publishing(self):
        import io
        import tarfile
        archive = self.root / "unsafe.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            item = tarfile.TarInfo("../outside")
            item.size = 1
            stream.addfile(item, io.BytesIO(b"x"))
        with self.assertRaises(EvidenceError):
            store.copy_archive(archive, self.root / "copy")
        self.assertEqual(list((self.root / "copy").iterdir()), [])

    def test_copy_is_idempotent_and_does_not_replace_existing_archive(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        first = store.copy_archive(archive, self.root / "copy")
        before = Path(first["archive"]["path"]).stat().st_ino
        second = store.copy_archive(archive, self.root / "copy")
        self.assertEqual(first, second)
        self.assertEqual(Path(second["archive"]["path"]).stat().st_ino,
                         before)

    def test_external_receipt_rejects_false_status_and_storage_claims(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        adopted = store.adopt(Path(package["archive"]["path"]),
                              self.root / "local")
        for field, value in (("signature_status", "signed"),
                             ("status", "passed"),
                             ("storage", "offsite_redundant"),
                             ("scope", "qualified product"),
                             ("files", True),
                             ("schema_version", True)):
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                store.verify_external({**adopted, field: value})

    def test_legacy_external_receipt_verifies_without_inheriting_durability(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        adopted = store.adopt(Path(package["archive"]["path"]),
                              self.root / "local")
        legacy = {key: value for key, value in adopted.items()
                  if key != "persistence_status"}
        legacy["kind"] = "durable_external_archive"
        result = store.verify_external(legacy)
        self.assertEqual(result["persistence_status"], "not_established")

    def test_copy_cli_protects_original_and_records_preservation(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        original = archive.read_bytes()
        result = store.main(["copy", "--source", str(archive),
                             "--store", str(self.root / "copy"),
                             "--report", str(archive)])
        self.assertEqual(result, 1)
        self.assertEqual(archive.read_bytes(), original)
        report = self.root / "copy-report.json"
        result = store.main(["copy", "--source", str(archive),
                             "--store", str(self.root / "copy"),
                             "--report", str(report)])
        self.assertEqual(result, 0)
        recorded = json.loads(report.read_text())
        self.assertEqual(recorded["kind"], "preserved_external_archive")
        self.assertEqual(recorded["persistence_status"], "not_established")

    def test_moved_archive_requires_explicit_navigation_override(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        old = Path(package["archive"]["path"])
        moved = self.root / "new-host/archive.tar.gz"
        moved.parent.mkdir()
        shutil.move(old, moved)
        with self.assertRaises(EvidenceError):
            store.verify_package(package)
        original_navigation = package["archive"]["path"]
        store.verify_package(package, archive_override=moved)
        result = store.restore_tree(package, self.root / "restored",
                                    archive_override=moved)
        self.assertEqual(result["status"], "verified")
        self.assertEqual(package["archive"]["path"], original_navigation)

    def test_environment_report_cannot_replace_transport_archive(self):
        self.file("toolchain/bin/compiler", b"compiler")
        exported = environment.export(self.root / "toolchain",
                                      self.root / "store", kind="toolchain")
        envelope = self.file("envelope.json", json.dumps(exported).encode())
        archive = Path(exported["package"]["archive"]["path"])
        original = archive.read_bytes()
        result = environment.main(["restore", "--envelope", str(envelope),
                                   "--destination", str(self.root / "restored"),
                                   "--report", str(archive)])
        self.assertEqual(result, 1)
        self.assertEqual(archive.read_bytes(), original)
        self.assertFalse((self.root / "restored").exists())

    def test_environment_report_cannot_replace_archive_override(self):
        self.file("toolchain/bin/compiler", b"compiler")
        exported = environment.export(self.root / "toolchain",
                                      self.root / "store", kind="toolchain")
        envelope = self.file("envelope.json", json.dumps(exported).encode())
        old = Path(exported["package"]["archive"]["path"])
        moved = self.root / "transported.tar.gz"
        shutil.move(old, moved)
        original = moved.read_bytes()
        result = environment.main(["restore", "--envelope", str(envelope),
                                   "--archive", str(moved),
                                   "--destination", str(self.root / "restored"),
                                   "--report", str(moved)])
        self.assertEqual(result, 1)
        self.assertEqual(moved.read_bytes(), original)
        self.assertFalse((self.root / "restored").exists())

    def test_environment_restores_after_original_store_is_removed(self):
        self.file("toolchain/bin/compiler", b"compiler")
        exported = environment.export(self.root / "toolchain",
                                      self.root / "store", kind="toolchain")
        old = Path(exported["package"]["archive"]["path"])
        moved = self.root / "transported.tar.gz"
        shutil.move(old, moved)
        result = environment.restore(exported, self.root / "restored",
                                     archive_override=moved)
        self.assertEqual(result["identity"], exported["identity"])

    def test_store_round_trip_and_relocation(self):
        source = self.root / "source"
        self.file("source/logs/raw", b"executed command\n")
        self.file("source/empty", b"")
        package = store.export_tree(source, self.root / "store")
        target = self.root / "relocated"
        receipt = store.restore_tree(package, target)
        self.assertEqual(receipt["status"], "verified")
        self.assertEqual((target / "logs/raw").read_bytes(), b"executed command\n")
        self.assertEqual((target / "empty").read_bytes(), b"")
        self.assertEqual(store.verify_package(package)["files"], 2)

    def test_store_rejects_modified_archive(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        archive = Path(package["archive"]["path"])
        archive.write_bytes(archive.read_bytes() + b"tampered")
        with self.assertRaises(EvidenceError):
            store.restore_tree(package, self.root / "target")
        self.assertFalse((self.root / "target").exists())

    def test_store_rejects_symlink_and_input_output_overlap(self):
        self.file("source/input")
        (self.root / "source/link").symlink_to("input")
        with self.assertRaises(EvidenceError):
            store.export_tree(self.root / "source", self.root / "store")
        (self.root / "source/link").unlink()
        with self.assertRaises(EvidenceError):
            store.export_tree(self.root / "source", self.root / "source/store")

    def test_store_refuses_to_overwrite_existing_destination(self):
        self.file("source/input")
        package = store.export_tree(self.root / "source", self.root / "store")
        destination = self.root / "existing"
        destination.mkdir()
        with self.assertRaises(EvidenceError):
            store.restore_tree(package, destination)

    def test_store_archive_is_deterministic_for_same_bytes(self):
        self.file("source/input")
        one = store.export_tree(self.root / "source", self.root / "one")
        two = store.export_tree(self.root / "source", self.root / "two")
        self.assertEqual(one["archive"]["sha256"], two["archive"]["sha256"])

    def test_cache_hits_are_always_revalidated(self):
        source = self.file("download")
        sha = hashlib.sha256(source.read_bytes()).hexdigest()
        entry = cache.put(source, self.root / "cache", sha)
        self.assertEqual(cache.lookup(self.root / "cache", sha), entry)
        entry.write_bytes(b"corrupt")
        with self.assertRaises(EvidenceError):
            cache.lookup(self.root / "cache", sha)

    def test_cache_wrong_digest_never_publishes(self):
        source = self.file("download")
        with self.assertRaises(EvidenceError):
            cache.put(source, self.root / "cache", "0" * 64)
        self.assertFalse((self.root / "cache" / ("0" * 64)).exists())

    def test_compiler_namespace_binds_flags_dependencies_and_compiler(self):
        compiler = self.file("cc", b"compiler-v1")
        lock = self.file("dependency", b"dependency-v1")
        one = cache.compiler_namespace(compiler, ["-mcpu=cortex-m4"], [lock])
        self.assertNotEqual(one, cache.compiler_namespace(compiler, ["-mcpu=cortex-m7"], [lock]))
        lock.write_bytes(b"dependency-v2")
        self.assertNotEqual(one, cache.compiler_namespace(compiler, ["-mcpu=cortex-m4"], [lock]))
        compiler.write_bytes(b"compiler-v2")
        self.assertNotEqual(one, cache.compiler_namespace(compiler, ["-mcpu=cortex-m4"], [lock]))

    def test_formal_reproduction_rejects_compiler_cache(self):
        with self.assertRaises(EvidenceError):
            cache.compiler_environment(self.root / "cache", "a" * 64, formal=True)

    def test_dependency_upgrade_requires_all_gates_and_unchanged_bytes(self):
        from tools.maintenance.dependency_proposals import GATES
        from tools.evidence.common import file_identity
        raw = self.file("review.raw", b"fixture independent review; no qualified source claim")
        gates = {name: {"status": "independently_reviewed",
                        "evidence": [file_identity(raw)]} for name in GATES}
        with self.assertRaises(EvidenceError):
            dependencies.verify_upgrade({})
        result = dependencies.verify_upgrade(gates)
        self.assertEqual(result["status"], "references_verified")
        self.assertFalse(result["merge_authorized"])
        self.assertFalse(result["qualification_claim"])
        raw.write_bytes(b"changed")
        with self.assertRaises(EvidenceError):
            dependencies.verify_upgrade(gates)

    def test_scope_selects_real_native_model_dependencies(self):
        selected = dependencies.required_submodules("native")
        self.assertIn("vendors/st/cmsis_device_u5", selected)
        self.assertIn("ext/freertos", selected)
        selected = dependencies.required_submodules("arm", "stm32h743", "baremetal")
        self.assertEqual(selected, ("vendors/arm/CMSIS_6", "vendors/st/cmsis_device_h7"))
        with self.assertRaises(EvidenceError):
            dependencies.required_submodules("arm", "unknown", "baremetal")

    def test_native_requires_google_source_without_exporting_it_to_firmware(self):
        self.assertIn("ext/googletest",
                      dependencies.required_submodules("native"))
        for scope in ("sdk", "candidate"):
            with self.subTest(scope=scope):
                selected = dependencies.required_submodules(scope)
                self.assertIn("vendors/st/stm32f4_hal", selected)
                self.assertNotIn("ext/googletest", selected)
        for family in dependencies.FAMILY_MODULE:
            for backend in ("baremetal", "freertos", "threadx"):
                with self.subTest(family=family, backend=backend):
                    self.assertNotIn("ext/googletest",
                        dependencies.required_submodules("arm", family, backend))

    def test_catalogue_approves_google_origin_and_rejects_origin_changes(self):
        from tools.maintenance import source_catalogue
        declaration = "".join(
            '[submodule "' + item.path + '"]\n path = ' + item.path +
            '\n url = ' + item.origin + '\n'
            for item in source_catalogue.SOURCE_DEPENDENCIES)
        google = ('[submodule "ext/googletest"]\n path = ext/googletest\n'
                  ' url = https://github.com/google/googletest.git\n')
        declared = source_catalogue.declared_modules(
            (declaration + google).encode())
        self.assertEqual(declared["ext/googletest"],
                         "https://github.com/google/googletest.git")
        with self.assertRaises(ValueError):
            source_catalogue.declared_modules(declaration.encode())
        with self.assertRaises(ValueError):
            source_catalogue.declared_modules((declaration + google.replace(
                "google/googletest.git", "unreviewed/googletest.git")).encode())

    def test_f407_scoped_prepare_includes_actual_official_ll_dependency(self):
        expected = ("vendors/arm/CMSIS_6", "vendors/st/cmsis_device_f4",
                    "vendors/st/stm32f4_hal")
        for backend in ("baremetal", "freertos", "threadx"):
            with self.subTest(backend=backend):
                selected = dependencies.required_submodules("arm", "stm32f407",
                                                            backend)
                self.assertEqual(selected, expected + (
                    () if backend == "baremetal" else ("ext/" + backend,)))

    def test_wheel_vendor_metadata_is_not_another_top_level_distribution(self):
        wheel = self.wheel()
        with zipfile.ZipFile(wheel, "a") as archive:
            archive.writestr("fixture/vendor/helper-2.0.dist-info/METADATA",
                             "Name: helper\nVersion: 2.0\n")
        lock = dependencies.wheel_lock(self.root / "wheels", "fixture==1.0\n",
                                       target="fixture")
        self.assertEqual(len(lock["wheels"]), 1)

    def test_wheel_top_level_dist_info_must_match_distribution_identity(self):
        wheel = self.root / "wheels/fixture-1.0-py3-none-any.whl"
        wheel.parent.mkdir()
        with zipfile.ZipFile(wheel, "w") as archive:
            archive.writestr("other-1.0.dist-info/METADATA",
                             "Name: fixture\nVersion: 1.0\n")
        with self.assertRaises(EvidenceError):
            dependencies.wheel_lock(self.root / "wheels", "fixture==1.0\n",
                                    target="fixture")

    def test_wheel_duplicate_top_level_metadata_is_rejected(self):
        wheel = self.wheel()
        with zipfile.ZipFile(wheel, "a") as archive:
            archive.writestr("other-1.0.dist-info/METADATA",
                             "Name: other\nVersion: 1.0\n")
        with self.assertRaises(EvidenceError):
            dependencies.wheel_lock(self.root / "wheels", "fixture==1.0\n",
                                    target="fixture")

    def test_wheel_bundle_requires_exact_full_closure(self):
        wheel = self.wheel()
        lock = dependencies.wheel_lock(self.root / "wheels", "fixture==1.0\n", target="fixture")
        dependencies.verify_wheels(self.root / "wheels", lock)
        wheel.write_bytes(b"changed")
        with self.assertRaises(EvidenceError):
            dependencies.verify_wheels(self.root / "wheels", lock)

    def test_wheel_bundle_rejects_extra_and_unpinned_requirements(self):
        self.wheel()
        with self.assertRaises(EvidenceError):
            dependencies.wheel_lock(self.root / "wheels", "fixture>=1\n", target="fixture")
        lock = dependencies.wheel_lock(self.root / "wheels", "fixture==1.0\n", target="fixture")
        self.wheel("extra")
        with self.assertRaises(EvidenceError):
            dependencies.verify_wheels(self.root / "wheels", lock)

    def test_environment_toolchain_transport_binds_expected_identity(self):
        self.file("toolchain/bin/compiler", b"compiler")
        exported = environment.export(self.root / "toolchain", self.root / "store", kind="toolchain")
        restored = environment.restore(exported, self.root / "restored")
        self.assertEqual(restored["identity"], exported["identity"])
        altered = dict(exported, identity={"sha256": "0" * 64, "files": 1, "bytes": 8})
        with self.assertRaises(EvidenceError):
            environment.restore(altered, self.root / "wrong")
        self.assertFalse((self.root / "wrong").exists())

    def test_plan_rejects_arbitrary_command_and_duplicate_stage(self):
        bad = {"schema_version": 1, "stages": [{"name": "x", "checker": "shell", "args": []}]}
        with self.assertRaises(EvidenceError):
            qualification.validate_plan(bad)
        stage = {"name": "x", "checker": "tool_tests", "args": ["--suite", "testing", "--report", "out.json"]}
        with self.assertRaises(EvidenceError):
            qualification.validate_plan({"schema_version": 1, "stages": [stage, stage]})

    def test_plan_executes_fixed_checker_and_retains_actual_failure(self):
        plan = {"schema_version": 1, "stages": [{"name": "real", "checker": "tool_tests", "args": []}]}
        source = {"commit": "fixture", "tree": "fixture", "dirty": True}
        self.file("tools/testing/run_tool_tests.py", b"# checker fixture\n")
        def run(argv, **kwargs):
            kwargs["log"].write_bytes(b"actual failed command\n")
            return type("Result", (), {"returncode": 7})()
        with patch.object(qualification, "observe_git_source", return_value=source), patch.object(qualification, "run_command", side_effect=run):
            report = qualification.execute(plan, self.root, self.root / "execution")
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["stages"][0]["exit_code"], 7)
        self.assertGreaterEqual(report["stages"][0]["duration_s"], 0)
        self.assertEqual(Path(report["stages"][0]["raw"]["path"]).read_bytes(), b"actual failed command\n")

    def test_provenance_revalidates_all_files(self):
        compiler = self.file("compiler", b"compiler")
        artifact = self.file("firmware", b"ELF")
        recorded = qualification.provenance({"commit": "fixture", "dirty": True}, [compiler], [artifact], [["cc", "main.c"]])
        self.assertEqual(recorded["kind"], "build_provenance")
        artifact.write_bytes(b"changed")
        with self.assertRaises(EvidenceError):
            qualification.verify_provenance(recorded)



class ActionlintPreparationTests(unittest.TestCase):
    """Verify the real prepared binary offline without a network fixture."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="actionlint-prepare-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.lock = self.root / "dependencies/actionlint.lock.json"
        self.lock.parent.mkdir()
        self.lock.write_bytes((dependencies.ROOT /
                               dependencies.ACTIONLINT_LOCK).read_bytes())
        self.prefix = Path(os.environ.get(
            "NEXUS_ACTIONLINT_TEST_PREFIX",
            dependencies.ROOT / ".venv/nexus-actionlint"))

    def test_four_official_hosts_share_one_lock_and_normalized_modes(self):
        hosts = (("Linux", "x86_64", "linux_amd64"),
                 ("Darwin", "x86_64", "darwin_amd64"),
                 ("Darwin", "arm64", "darwin_arm64"),
                 ("Windows", "AMD64", "windows_amd64"))
        for system, machine, expected in hosts:
            with self.subTest(host=expected), \
                    patch("platform.system", return_value=system), \
                    patch("platform.machine", return_value=machine):
                lock, asset = dependencies.actionlint_asset(self.lock)
                self.assertEqual(asset["platform"], expected)
                self.assertIn("/v" + lock["version"] + "/", asset["url"])
                self.assertEqual(asset["executable"]["mode"], 0o755)
                self.assertEqual(asset["notice"]["mode"], 0o644)

    def test_prepared_prefix_reuse_is_strictly_offline(self):
        executable = dependencies.verify_actionlint(self.prefix, self.lock)
        with patch.object(cache, "fetch", side_effect=AssertionError("network")):
            receipt = dependencies.install_actionlint(
                self.prefix, self.root / "cache", root=self.root)
        self.assertEqual(receipt["version"], "1.7.12")
        self.assertTrue(executable.is_file())

    def test_unknown_prefix_is_preserved_and_never_fetches(self):
        prefix = self.root / "unknown"
        prefix.mkdir()
        marker = prefix / "preserve"
        marker.write_bytes(b"unknown user file")
        with patch.object(cache, "fetch", side_effect=AssertionError("network")):
            with self.assertRaisesRegex(EvidenceError, "prepare"):
                dependencies.install_actionlint(
                    prefix, self.root / "cache", root=self.root)
        self.assertEqual(marker.read_bytes(), b"unknown user file")
        self.assertEqual(list(prefix.iterdir()), [marker])

    def test_changed_binary_notice_mode_and_receipt_fail_offline(self):
        prefix = self.root / "prefix"
        shutil.copytree(self.prefix, prefix)
        _, asset = dependencies.actionlint_asset(self.lock)
        for name in (asset["executable"]["member"], "LICENSE.txt", "receipt.json"):
            path = prefix / name
            before = path.read_bytes()
            path.write_bytes(before + b"corrupt")
            with self.subTest(member=name):
                with self.assertRaises(EvidenceError):
                    dependencies.verify_actionlint(prefix, self.lock)
            path.write_bytes(before)
        if os.name != "nt":
            executable = prefix / asset["executable"]["member"]
            executable.chmod(0o644)
            with self.assertRaises(EvidenceError):
                dependencies.verify_actionlint(prefix, self.lock)

    def test_lock_mismatch_and_unsupported_host_fail_closed(self):
        data = json.loads(self.lock.read_text())
        data["version"] = "0.0.0"
        self.lock.write_text(json.dumps(data))
        with self.assertRaisesRegex(EvidenceError, "lock"):
            dependencies.verify_actionlint(self.prefix, self.lock)
        with patch("platform.system", return_value="FreeBSD"):
            with self.assertRaises(EvidenceError):
                dependencies.actionlint_asset(self.lock)

    def test_digest_mismatch_archive_never_executes_or_publishes(self):
        payload = self.root / "wrong.tar.gz"
        payload.write_bytes(b"wrong official archive bytes")
        prefix = self.root / "new-prefix"
        from tools.dev import process
        with patch.object(cache, "fetch", return_value=payload), \
                patch.object(process, "run") as execute:
            with self.assertRaises(EvidenceError):
                dependencies.install_actionlint(
                    prefix, self.root / "cache", root=self.root)
        execute.assert_not_called()
        self.assertFalse(prefix.exists())

    def test_cache_and_prefix_overlap_never_creates_unknown_directory(self):
        prefix = self.root / "new-prefix"
        with self.assertRaisesRegex(EvidenceError, "overlap"):
            dependencies.install_actionlint(prefix, prefix, root=self.root)
        self.assertFalse(prefix.exists())

    def test_symlink_prefix_does_not_touch_its_target(self):
        alias = self.root / "alias"
        alias.symlink_to(self.prefix, target_is_directory=True)
        with self.assertRaisesRegex(EvidenceError, "symlink"):
            dependencies.verify_actionlint(alias, self.lock)


if __name__ == "__main__":
    unittest.main()
