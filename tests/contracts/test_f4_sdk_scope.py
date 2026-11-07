"""F4 source SDK exports one explicit supplier scope and optional host support."""
import importlib.util
import json
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def load_sdk():
    spec = importlib.util.spec_from_file_location(
        "f4_source_sdk_scope", ROOT / "cmake/package/package_source_sdk.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class F4SourceSDKScopeTests(unittest.TestCase):
    def test_default_sdk_has_no_retired_or_optional_supplier(self):
        sdk = load_sdk()
        closure = sdk.runtime_dependency_closure()
        self.assertEqual(set(closure), {
            "vendors/arm/CMSIS_6", "vendors/st/cmsis_device_f4",
            "vendors/st/stm32f4_hal"})

    def test_selected_sdk_keeps_os_and_component_provenance_explicit(self):
        sdk = load_sdk()
        profile = sdk.source_profile(backend="freertos", components=("network",))
        closure = sdk.runtime_dependency_closure(profile)
        self.assertIn("ext/freertos", closure)
        self.assertIn("ext/lwip", closure)
        self.assertNotIn("vendors/st/stm32h7_hal", closure)
        self.assertNotIn("ext/mbedtls", closure)

    def test_host_support_is_an_explicit_payload_choice(self):
        sdk = load_sdk()
        path = "tests/support/native/board/board.json"
        self.assertFalse(sdk.selected(path))
        self.assertTrue(sdk.selected(path, profile=sdk.source_profile(
            host_support=True)))
        self.assertFalse(sdk.selected("tests/google/CMakeLists.txt",
                                     profile=sdk.source_profile(host_support=True)))

    def test_unselected_host_c_trial_is_not_exported_as_an_sdk_tool(self):
        sdk = load_sdk()
        for name in ("tools/nxconfig/CMakeLists.txt",
                     "tools/nxconfig/src/main.c"):
            with self.subTest(path=name):
                self.assertFalse(sdk.selected(name))
                self.assertFalse(sdk.selected(name, profile=sdk.source_profile(
                    host_support=True)))

    def test_prepared_sdk_contains_only_its_selected_supplier_payload(self):
        sdk = load_sdk()
        with tempfile.TemporaryDirectory(prefix="f4 sdk actual payload ",
                                         dir="/dev/shm") as name:
            prefix = Path(name) / "prefix"
            sdk.prepare(ROOT, prefix, development_fixture=True)
            root = prefix / "share/nexus/src"
            identity = sdk.verify(root)
            self.assertEqual({item["path"] for item in identity["dependencies"]}, {
                "vendors/arm/CMSIS_6", "vendors/st/cmsis_device_f4",
                "vendors/st/stm32f4_hal"})
            self.assertFalse(any(path.startswith(("tools/nxconfig/",
                "ext/tomlc99/", "ext/cjson/", "tests/"))
                for path in identity["files_sha256"]))


    def test_rehashed_unselected_vendor_payload_is_rejected(self):
        sdk = load_sdk()
        with tempfile.TemporaryDirectory(prefix="f4 sdk supplier scope ",
                                         dir="/dev/shm") as name:
            prefix = Path(name) / "prefix"
            sdk.prepare(ROOT, prefix, development_fixture=True)
            root = prefix / "share/nexus/src"
            identity = sdk.verify(root)
            foreign = "vendors/st/cmsis_device_h7/Include/unselected.h"
            target = root / foreign
            target.parent.mkdir(parents=True)
            target.write_bytes(b"unselected supplier payload\n")
            identity["files_sha256"][foreign] = sdk.digest(target.read_bytes())
            identity["snapshot_sha256"] = sdk.digest(json.dumps(
                identity["files_sha256"], sort_keys=True,
                separators=(",", ":")).encode())
            (root / sdk.MANIFEST).write_text(json.dumps(identity) + "\n")
            with self.assertRaisesRegex(sdk.SDKError, "Unselected supplier"):
                sdk.verify(root)

    def test_manifest_schema_requires_an_integer(self):
        sdk = load_sdk()
        with tempfile.TemporaryDirectory(prefix="f4 sdk manifest schema ",
                                         dir="/dev/shm") as name:
            prefix = Path(name) / "prefix"
            sdk.prepare(ROOT, prefix, development_fixture=True)
            root = prefix / "share/nexus/src"
            identity = sdk.verify(root)
            identity["schema"] = 2.0
            (root / sdk.MANIFEST).write_text(json.dumps(identity) + "\n")
            with self.assertRaisesRegex(sdk.SDKError, "Unsupported"):
                sdk.verify(root)


if __name__ == "__main__":
    unittest.main()
