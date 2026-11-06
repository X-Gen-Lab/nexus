"""Read package metadata and refresh only authored input identities."""
from contextlib import redirect_stderr, redirect_stdout
from dataclasses import FrozenInstanceError
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import dependencies, integration_package
from tools.configure import test_external_package as external_tests


class ProviderManifestTests(unittest.TestCase):
    """Static authoring work must never import selected trusted provider code."""

    reseal = staticmethod(external_tests.ExternalPackageTests.reseal)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package, self.assembly = external_tests.ExternalPackageTests.package(self)

    def load(self):
        self.assertTrue(callable(getattr(integration_package, "load_manifest", None)),
                        "A static manifest loader must exist independently of select")
        return integration_package.load_manifest(self.package)

    def command(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        self.assertTrue(callable(getattr(integration_package, "main", None)),
                        "Manifest authoring needs an explicit check/refresh CLI")
        with redirect_stdout(output), redirect_stderr(errors):
            status = integration_package.main([*arguments, "--package", str(self.package)])
        return status, output.getvalue(), errors.getvalue()

    def test_static_manifest_record_does_not_execute_entry_and_is_frozen(self):
        entry = self.package / "configure/__init__.py"
        entry.write_text('raise RuntimeError("metadata query executed provider")\n')
        self.reseal(self.package)
        record = self.load()
        self.assertEqual(record.family, "partner_model")
        self.assertEqual(record.descriptor.family, record.family)
        self.assertEqual(record.source_directory, self.package / "soc")
        self.assertIn(self.package / "nexus-soc.json", record.inputs)
        self.assertEqual(dict(record.input_digests)[entry],
                         hashlib.sha256(entry.read_bytes()).hexdigest())
        with self.assertRaises(FrozenInstanceError):
            record.family = "other"
        with self.assertRaises(TypeError):
            record.identity["id"] = "other"

    def test_static_loader_does_not_import_unselected_builtin_descriptors(self):
        process = subprocess.run([
            sys.executable, "-c",
            "import sys; from tools.configure.integration_package import load_manifest; "
            "load_manifest(sys.argv[1]); "
            "assert not [name for name in sys.modules if name.startswith('soc.')], "
            "'Static external metadata imported unselected builtin source'",
            str(self.package)], cwd=external_tests.ROOT,
            capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)

    def test_check_is_readonly_and_refresh_only_updates_declared_hashes(self):
        manifest = self.package / "nexus-soc.json"
        original = json.loads(manifest.read_text())
        entry = self.package / "configure/__init__.py"
        entry.write_text('raise RuntimeError("authoring executed provider")\n')
        undeclared = self.package / "configure/undeclared.py"
        undeclared.write_text('raise RuntimeError("directory discovery")\n')
        before = manifest.read_bytes()
        status, _, errors = self.command("check")
        self.assertEqual(status, 1)
        self.assertIn("SHA256", errors)
        self.assertEqual(manifest.read_bytes(), before)
        status, output, errors = self.command("refresh-inputs")
        self.assertEqual(status, 0, errors)
        refreshed = json.loads(manifest.read_text())
        self.assertEqual(set(refreshed["inputs"]), set(original["inputs"]))
        self.assertNotIn("configure/undeclared.py", refreshed["inputs"])
        for key in original.keys() - {"inputs"}:
            self.assertEqual(refreshed[key], original[key])
        self.assertIn("Refreshed", output)
        status, output, errors = self.command("check", "--json")
        self.assertEqual(status, 0, errors)
        self.assertEqual(json.loads(output)["family"], "partner_model")
        self.assertNotIn("sandbox", output.lower())

    def test_refresh_rejects_bad_metadata_without_mutation(self):
        manifest = self.package / "nexus-soc.json"
        original = json.loads(manifest.read_text())
        for changes in ({"interface_version": 2}, {"unknown": True},
                        {"inputs": {**original["inputs"], "../escape": "0" * 64}},
                        {"inputs": {**original["inputs"], "missing": "0" * 64}}):
            with self.subTest(changes=changes):
                manifest.write_text(json.dumps({**original, **changes}))
                before = manifest.read_bytes()
                status, _, _ = self.command("refresh-inputs")
                self.assertEqual(status, 1)
                self.assertEqual(manifest.read_bytes(), before)

    def test_refresh_withdraws_temporary_output_when_input_changes_or_disappears(self):
        manifest = self.package / "nexus-soc.json"
        entry = self.package / "configure/__init__.py"
        original_manifest = manifest.read_bytes()
        original_entry = entry.read_bytes()
        fsync = os.fsync
        for missing in (False, True):
            with self.subTest(missing=missing):
                entry.write_bytes(original_entry)

                def interrupt_input(descriptor):
                    fsync(descriptor)
                    if missing:
                        entry.unlink()
                    else:
                        entry.write_bytes(original_entry + b"\n# changed during refresh\n")

                with patch.object(integration_package.os, "fsync", interrupt_input):
                    status, output, errors = self.command("refresh-inputs", "--json")
                self.assertEqual(status, 1)
                self.assertFalse(output.strip(), "Failed refresh must not issue a verified report")
                self.assertIn("SoC package rejected", errors)
                self.assertEqual(manifest.read_bytes(), original_manifest)
                self.assertEqual(list(self.package.glob(".nexus-soc-*")), [])
                status, output, _ = self.command("check", "--json")
                self.assertEqual(status, 1)
                self.assertFalse(output.strip())

    def test_static_loader_rejects_duplicate_json_and_bool_version(self):
        manifest = self.package / "nexus-soc.json"
        original = manifest.read_text()
        for text in (original.replace('"schema_version": 1', '"schema_version": true'),
                     original.replace('"schema_version": 1',
                                      '"schema_version": 1, "schema_version": 1')):
            with self.subTest(text=text):
                manifest.write_text(text)
                with self.assertRaisesRegex(ValueError, "schema|Duplicate"):
                    self.load()

    def test_editing_schema_is_derived_from_the_manifest_contract(self):
        self.assertTrue(callable(getattr(integration_package, "manifest_schema", None)),
                        "Editing schema must be a projection of the runtime contract")
        path = external_tests.ROOT / "tools/configure/soc-package.schema.json"
        self.assertEqual(json.loads(path.read_text()), integration_package.manifest_schema())
        schema = integration_package.manifest_schema()
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(set(schema["required"]), set(schema["properties"]))

    def test_external_final_identity_excludes_unselected_builtin_behavior(self):
        paths, identity = dependencies.snapshot(mode="platform")
        result = external_tests.configure.resolve_ir(self.assembly)
        selected_paths, selected_identity = dependencies.selected(
            paths, identity, family=result.soc_family, context=result.provider_context)
        builtins = {name for family in external_tests.configure.descriptor.families()
                    for name in family.configuration_modules}
        self.assertFalse(set(selected_paths) & builtins,
                         "External final identity must exclude unselected builtin behavior")
        self.assertIn("tools/configure/integration_package.py", selected_paths)
        self.assertIn("soc-package/configure/__init__.py", selected_paths)
        self.assertEqual({row["path"] for row in selected_identity["files"]},
                         set(selected_paths))
        dependencies.verify(selected_paths, selected_identity)
        self.assertTrue(set(paths) & builtins,
                        "The initial pre-resolution snapshot remains complete")

    def test_external_identity_tracks_only_selected_code_and_keeps_full_snapshot(self):
        generator = self.root / "generator"
        for name in dependencies.source_names("platform", None, sdk=True):
            target = generator / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(external_tests.ROOT / name, target)
        result = external_tests.configure.resolve_ir(self.assembly)
        with patch.object(dependencies, "ROOT", generator):
            initial_paths, initial = dependencies.snapshot(mode="platform", sdk=True)
            selected_paths, selected = dependencies.selected(
                initial_paths, initial, family=result.soc_family,
                context=result.provider_context)
            self.assertTrue(set(dependencies.SDK).issubset(selected_paths))
            leaf = generator / "soc/stm32/stm32h743/configure/contract.py"
            leaf.write_bytes(leaf.read_bytes() + b"\n# unselected leaf changed\n")
            with self.assertRaisesRegex(ValueError, "changed during generation"):
                dependencies.verify(initial_paths, initial)
            paths, identity = dependencies.snapshot(mode="platform", sdk=True)
            _, after_leaf = dependencies.selected(
                paths, identity, family=result.soc_family,
                context=result.provider_context)
            self.assertEqual(after_leaf["sha256"], selected["sha256"])
            for name in ("tools/configure/providers/descriptor.py",
                         "tools/configure/contracts.py"):
                with self.subTest(selected_source=name):
                    source = generator / name
                    original = source.read_bytes()
                    source.write_bytes(original + b"\n# selected common input changed\n")
                    with self.assertRaisesRegex(ValueError, "changed during generation"):
                        dependencies.verify(selected_paths, selected)
                    paths, identity = dependencies.snapshot(mode="platform", sdk=True)
                    _, changed = dependencies.selected(
                        paths, identity, family=result.soc_family,
                        context=result.provider_context)
                    self.assertNotEqual(changed["sha256"], selected["sha256"])
                    source.write_bytes(original)
            entry = self.package / "configure/__init__.py"
            entry.write_bytes(entry.read_bytes() + b"\n# selected provider changed\n")
            with self.assertRaisesRegex(ValueError, "changed during generation"):
                dependencies.verify(selected_paths, selected)
            self.reseal(self.package)
            changed_result = external_tests.configure.resolve_ir(self.assembly)
            paths, identity = dependencies.snapshot(mode="platform", sdk=True)
            _, changed = dependencies.selected(
                paths, identity, family=changed_result.soc_family,
                context=changed_result.provider_context)
            self.assertNotEqual(changed["sha256"], selected["sha256"])

    def test_external_final_identity_keeps_verified_input_snapshot(self):
        paths, identity = dependencies.snapshot(mode="platform")
        result = external_tests.configure.resolve_ir(self.assembly)
        entry = self.package / "configure/__init__.py"
        entry.write_text(entry.read_text() + "\n# changed after selected context\n")
        selected_paths, selected_identity = dependencies.selected(
            paths, identity, family=result.soc_family, context=result.provider_context)
        with self.assertRaisesRegex(ValueError, "changed during generation"):
            dependencies.verify(selected_paths, selected_identity)


if __name__ == "__main__":
    unittest.main()
