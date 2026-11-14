"""SDK exports follow package ownership rather than per-driver source lists."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def packager():
    spec = importlib.util.spec_from_file_location(
        "sdk_export_ownership", ROOT / "cmake/package/package_source_sdk.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class SDKExportOwnershipTests(unittest.TestCase):
    def setUp(self):
        self.sdk = packager()
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK package owner ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package = self.root / "soc/acme/reviewed"
        self.package.mkdir(parents=True)
        for name in ("soc.json", "routes.json", "CMakeLists.txt"):
            (self.package / name).write_text("{}\n")
        (self.package / "package.json").write_text(json.dumps({
            "schema_version": 1, "interface_version": 1,
            "id": "reviewed", "family": "reviewed", "vendor": "acme",
            "source_directory": ".",
            "facts": {"soc": "soc.json", "routes": "routes.json"},
            "dependencies": {},
            "cmake": {"entry": "CMakeLists.txt", "target": "reviewed_soc"},
            "exports": {"roots": ["."], "exclude": ["testing"]}}))

    def default_scope(self):
        path = self.root / "dependencies/platform-compatibility.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        value = json.loads((ROOT / "dependencies/platform-compatibility.json").read_text())
        value["source_sdk"]["default_package_manifests"] = [
            "soc/acme/reviewed/package.json"]
        path.write_text(json.dumps(value))

    def second_package(self):
        second = self.root / "soc/acme/unselected"
        second.mkdir(parents=True)
        value = json.loads((self.package / "package.json").read_text())
        value.update(id="unselected", family="unselected")
        value["dependencies"] = {"hal": "stm32f4_hal"}
        for name in ("soc.json", "routes.json", "CMakeLists.txt"):
            (second / name).write_text("{}\n")
        (second / "package.json").write_text(json.dumps(value))
        return second

    def test_default_profile_is_owned_by_input_root_and_does_not_expand(self):
        self.default_scope()
        self.second_package()
        profile = self.sdk.source_profile(root=self.root)
        self.assertEqual(profile["packages"], ["reviewed"])
        closure = self.sdk.runtime_dependency_closure(profile, root=self.root)
        self.assertEqual(closure, {})
        entries = {"soc/acme/unselected/drivers/unselected.c":
                   ("100644", "blob", "a" * 40)}
        self.assertEqual(self.sdk.owned_source_files(
            self.root, entries, profile=profile), ())

    def test_explicit_package_selection_does_not_pull_default_suppliers(self):
        self.default_scope()
        self.second_package()
        profile = self.sdk.source_profile(root=self.root, packages=("unselected",))
        self.assertEqual(profile["packages"], ["unselected"])
        closure = self.sdk.runtime_dependency_closure(profile, root=self.root)
        self.assertEqual(set(closure), {"vendors/st/stm32f4_hal"})
        for packages in (("unknown",), ("reviewed", "reviewed"), (), "reviewed"):
            with self.subTest(packages=packages), self.assertRaises(self.sdk.SDKError):
                self.sdk.source_profile(root=self.root, packages=packages)

    def test_selected_f4_retains_its_authoritative_configuration_code_closure(self):
        from tools.configure.dependencies import source_names
        expected = set(source_names("platform", "stm32f4", sdk=True))
        selection = self.sdk.resolve_selection(ROOT)
        entries = {name: ("100644", "blob", "a" * 40) for name in expected}
        members = set(self.sdk.owned_source_files(ROOT, entries,
                                                  selection=selection))
        self.assertEqual(expected, members,
            "The F4 SDK must retain the adapter needed by its own resolver")

    def test_data_only_sdk_keeps_common_tools_without_loading_legacy_families(self):
        from tools.configure.dependencies import source_names
        from tools.configure.providers import descriptor
        common = set(source_names("platform", None, sdk=True))
        legacy = set(source_names("platform", "stm32f4", sdk=True)) - common
        self.default_scope()
        entries = {name: ("100644", "blob", "a" * 40)
                   for name in common | legacy}
        with patch.object(descriptor, "maintained", side_effect=AssertionError(
                "An unselected legacy provider was loaded")):
            with patch.object(descriptor, "families", side_effect=AssertionError(
                    "Unselected provider families were enumerated")):
                selection = self.sdk.resolve_selection(self.root)
                members = set(self.sdk.owned_source_files(self.root, entries,
                                                          selection=selection))
        self.assertEqual(members, common)

    def committed_package(self):
        driver = self.package / "drivers/new_ip.c"
        driver.parent.mkdir()
        driver.write_text("int reviewed_driver(void) { return 0; }\n")
        for arguments in (("init", "--quiet"), ("add", "."),
                          ("-c", "user.name=Nexus Test", "-c",
                           "user.email=nexus-test@example.invalid", "commit",
                           "--quiet", "-m", "fixture: reviewed package")):
            subprocess.run(("git", "-C", str(self.root), *arguments),
                           check=True, capture_output=True)
        return driver.relative_to(self.root).as_posix()

    def exclude_committed_driver(self):
        manifest = self.package / "package.json"
        value = json.loads(manifest.read_text())
        value["exports"]["exclude"].append("drivers")
        manifest.write_text(json.dumps(value))

    def test_publishable_member_selection_uses_committed_package_policy(self):
        driver = self.committed_package()
        entries = self.sdk.tree_entries(self.root)
        self.exclude_committed_driver()
        try:
            members = self.sdk.snapshot_files(self.root, entries,
                profile={"backend": "baremetal", "components": [],
                         "host_support": False, "packages": ["reviewed"]})
        except self.sdk.SDKError:
            return  # A changed author policy may reject, never omit the driver.
        self.assertIn(driver, members,
            "A live export edit changed the published committed-tree scope")

    def test_publishable_snapshot_rejects_export_policy_changed_after_admission(self):
        self.committed_package()
        revision = self.sdk.git(self.root, "rev-parse", "HEAD").decode().strip()
        self.exclude_committed_driver()
        with self.assertRaises(self.sdk.SDKError):
            self.sdk.verify_preparation_snapshot(self.root, revision, None)

    def test_captured_package_selection_survives_no_live_rediscovery(self):
        self.default_scope()
        driver = self.committed_package()
        entries = self.sdk.tree_entries(self.root)
        selection = self.sdk.resolve_selection(self.root, entries=entries)
        revision = self.sdk.git(self.root, "rev-parse", "HEAD").decode().strip()
        manifest = self.package / "package.json"
        original = manifest.read_bytes()
        self.exclude_committed_driver()
        members = self.sdk.snapshot_files(self.root, entries, selection=selection)
        self.assertIn(driver, members)
        manifest.write_bytes(original)
        with self.assertRaises(self.sdk.SDKError):
            self.sdk.verify_preparation_snapshot(self.root, revision, None,
                                                  selection=selection)

    def test_installed_scope_rejects_unselected_package_source(self):
        self.default_scope()
        self.second_package()
        selection = self.sdk.resolve_selection(self.root)
        unexpected = "soc/acme/unselected/drivers/other.c"
        for members in ((unexpected,), ("core/request.c",)):
            with self.subTest(members=members), self.assertRaises(self.sdk.SDKError):
                self.sdk.check_selected_source_scope(
                    selection, members, {unexpected: "a" * 64,
                                         "core/request.c": "b" * 64})

    def test_tracked_import_scope_comes_only_from_selected_package_facts(self):
        self.default_scope()
        (self.package / "soc.json").write_text(json.dumps({
            "source_lock": "vendors/acme/reviewed/source.lock.json"}))
        profile = self.sdk.source_profile(root=self.root)
        self.assertEqual(self.sdk.reviewed_import_directories(
            root=self.root, profile=profile), ("vendors/acme/reviewed",))
        entries = {"vendors/acme/reviewed/driver.c": ("100644", "blob", "a" * 40),
                   "vendors/acme/inventory/driver.c": ("100644", "blob", "b" * 40)}
        self.assertEqual(self.sdk.owned_source_files(self.root, entries, profile=profile),
                         ("vendors/acme/reviewed/driver.c",))

    def test_publishable_import_fact_matches_the_same_source_tree(self):
        self.default_scope()
        self.committed_package()
        entries = self.sdk.tree_entries(self.root)
        (self.package / "soc.json").write_text(json.dumps({
            "source_lock": "vendors/acme/unreviewed/source.lock.json"}))
        with self.assertRaises(self.sdk.SDKError):
            self.sdk.resolve_selection(self.root, entries=entries)

    def test_new_package_driver_is_exported_without_common_tool_edits(self):
        entries = {
            "soc/acme/reviewed/drivers/new_ip.c": ("100644", "blob", "a" * 40),
            "soc/acme/reviewed/private/new_ip.h": ("100644", "blob", "b" * 40),
            "soc/acme/reviewed/testing/model.c": ("100644", "blob", "c" * 40),
            "soc/acme/unselected/drivers/other.c": ("100644", "blob", "d" * 40),
        }
        members = self.sdk.owned_source_files(self.root, entries,
            profile={"backend": "baremetal", "components": [],
                     "host_support": False, "packages": ["reviewed"]})
        self.assertEqual(set(members), {
            "soc/acme/reviewed/drivers/new_ip.c",
            "soc/acme/reviewed/private/new_ip.h"})

    def test_export_declaration_rejects_rehashed_file_map_omission(self):
        declaration = {"members": ["core/src/request.c"], "packages": []}
        with self.assertRaisesRegex(self.sdk.SDKError, "missing required sources"):
            self.sdk.check_source_exports(declaration, {})
        self.assertEqual(self.sdk.check_source_exports(
            declaration, {"core/src/request.c": "a" * 64}),
            ("core/src/request.c",))

    def test_export_declaration_rejects_unsafe_duplicate_or_unlisted_package(self):
        for declaration in (
                {"members": ["../escape.c"], "packages": []},
                {"members": ["core/a.c", "core/a.c"], "packages": []},
                {"members": ["core/a.c"], "packages": [
                    {"id": "reviewed", "manifest": "soc/acme/reviewed/package.json"}]},
                {"members": ["core/a.c"], "packages": [], "unknown": 1}):
            with self.subTest(declaration=declaration):
                with self.assertRaises(self.sdk.SDKError):
                    self.sdk.check_source_exports(
                        declaration, {"core/a.c": "a" * 64})


if __name__ == "__main__":
    unittest.main()
