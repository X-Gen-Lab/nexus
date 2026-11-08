"""Source provenance, approved transport and recursive scope stay distinct."""
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.maintenance import source_catalogue as catalogue

ROOT = Path(__file__).resolve().parents[2]


class SourceCatalogueTests(unittest.TestCase):
    def test_host_config_scope_does_not_reclassify_parsers_as_runtime(self):
        selected = catalogue.host_configuration_sources()
        self.assertEqual({item.identifier for item in selected}, {
            "tomlc99", "cjson"})
        self.assertTrue(all(item.scope == "inventory" for item in selected))
        self.assertFalse(set(selected).intersection(catalogue.SOURCE_DEPENDENCIES))

    def test_host_config_parsers_are_explicit_inventory_sources(self):
        expected = {
            'tomlc99': ('ext/tomlc99',
                        'https://github.com/X-Gen-Lab/tomlc99.git'),
            'cjson': ('ext/cjson',
                      'https://github.com/X-Gen-Lab/cJSON.git'),
        }
        for name, (path, transport) in expected.items():
            with self.subTest(dependency=name):
                entry = catalogue.dependency(name)
                self.assertEqual(entry.path, path)
                self.assertEqual(entry.fetch_url, transport)
                self.assertEqual(entry.scope, 'inventory')
                self.assertNotIn(entry, catalogue.SOURCE_DEPENDENCIES)
                self.assertTrue((ROOT / entry.source_lock).is_file())

    def test_runtime_scope_excludes_google_and_keeps_nested_sources_explicit(self):
        self.assertEqual({item.identifier for item in catalogue.SOURCE_DEPENDENCIES}, {
            "freertos", "cmsis_core", "stm32f4_cmsis", "stm32f4_hal",
            "threadx", "lwip", "mbedtls", "coremqtt", "mcuboot"})
        self.assertEqual({item.identifier for item in catalogue.HOST_TEST_DEPENDENCIES},
                         {"googletest"})
        self.assertEqual(catalogue.dependency("googletest").scope, "host-test")
        self.assertTrue(all(item.parent is None
                            for item in catalogue.SOURCE_DEPENDENCIES))
        with self.assertRaises(FrozenInstanceError):
            catalogue.dependency("freertos").fetch_url = "changed"

    def test_upstream_provenance_does_not_change_with_approved_transport(self):
        original = catalogue.dependency("googletest")
        changed = replace(original,
            fetch_url="https://github.com/Reviewed-Test/googletest.git")
        entries = tuple(changed if item == original else item
                        for item in catalogue.ALL_DEPENDENCIES)
        with patch.object(catalogue, "ALL_DEPENDENCIES", entries):
            self.assertEqual(catalogue.dependency(original.path), changed)
            self.assertEqual(catalogue.upstream_dependency(original.upstream_url),
                             changed)
            official = (ROOT / ".gitmodules").read_bytes()
            for item in catalogue.ALL_SOURCE_DEPENDENCIES:
                official = official.replace(item.fetch_url.encode(),
                                            item.upstream_url.encode())
            controlled = official
            for item in catalogue.children():
                controlled = controlled.replace(item.upstream_url.encode(),
                                                item.fetch_url.encode())
            with self.assertRaises(ValueError):
                catalogue.declared_modules(official)
            catalogue.declared_modules(official, upstream=True)
            catalogue.declared_modules(controlled)
            with self.assertRaises(ValueError):
                catalogue.declared_modules(controlled, upstream=True)
        self.assertEqual(changed.upstream_url, original.upstream_url)
        self.assertFalse(hasattr(changed, "origin"))

    def test_real_root_and_nested_declarations_match_exact_catalogue(self):
        root = catalogue.declared_modules((ROOT / ".gitmodules").read_bytes())
        self.assertEqual(len(root), len(catalogue.ALL_SOURCE_DEPENDENCIES))
        nested = catalogue.declared_modules(
            (ROOT / "ext/freertos/.gitmodules").read_bytes(), "ext/freertos")
        self.assertEqual(set(nested), {
            "portable/ThirdParty/Partner-Supported-Ports",
            "portable/ThirdParty/Community-Supported-Ports"})
        for item in catalogue.children("ext/freertos"):
            self.assertNotEqual(catalogue.module_name(item),
                                catalogue.relative_path(item))
            self.assertEqual(nested[catalogue.relative_path(item)], item.fetch_url)

    def test_lwip_is_an_organization_hosted_optional_runtime_source(self):
        item = catalogue.dependency("lwip")
        self.assertEqual(item.path, "ext/lwip")
        self.assertEqual(item.scope, "runtime")
        self.assertEqual(item.upstream_url,
                         "https://github.com/lwip-tcpip/lwip.git")
        self.assertEqual(item.fetch_url,
                         "https://github.com/X-Gen-Lab/lwip.git")
        self.assertEqual(item.notice, "COPYING")
        self.assertEqual(item.source_lock, "dependencies/lwip.lock.json")
        modules = catalogue.declared_modules((ROOT / ".gitmodules").read_bytes())
        self.assertEqual(modules[item.path], item.fetch_url)

    def test_mcuboot_has_exact_optional_runtime_provenance_and_lock(self):
        item = catalogue.dependency("mcuboot")
        self.assertEqual(item.path, "ext/mcuboot")
        self.assertEqual(item.scope, "runtime")
        self.assertIsNone(item.parent)
        self.assertEqual(item.upstream_url,
                         "https://github.com/mcu-tools/mcuboot.git")
        self.assertEqual(item.fetch_url,
                         "https://github.com/X-Gen-Lab/mcuboot.git")
        self.assertEqual(item.notice, "LICENSE")
        self.assertEqual(item.source_lock, "dependencies/mcuboot.lock.json")
        modules = catalogue.declared_modules((ROOT / ".gitmodules").read_bytes())
        self.assertEqual(modules[item.path], item.fetch_url)

    def test_nested_name_path_and_fetch_url_are_not_interchangeable(self):
        data = (ROOT / "ext/freertos/.gitmodules").read_bytes()
        for changed in (
                data.replace(b'ThirdParty/FreeRTOS-Kernel-Partner',
                             b'portable/ThirdParty/Partner'),
                data.replace(b'path = portable/', b'path = ../'),
                data.replace(b'https://github.com/', b'https://foreign.invalid/'),
                data + b'\tbranch = main\n'):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                catalogue.declared_modules(changed, "ext/freertos")

    def test_unknown_parent_and_unknown_dependency_reject(self):
        for name in ("missing", "ext/../freertos"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                catalogue.dependency(name)
            with self.subTest(name=name), self.assertRaises(ValueError):
                catalogue.declared_modules(b"", name)

    def test_duplicate_identity_path_or_upstream_reject(self):
        entries = catalogue.ALL_DEPENDENCIES
        for item in (entries[0], replace(entries[0], identifier="other"),
                     replace(entries[0], path="ext/other")):
            with self.subTest(item=item), self.assertRaises(ValueError):
                catalogue.validate_catalogue((*entries, item))

    def test_parent_escape_cycle_and_host_scope_drift_reject(self):
        child = catalogue.children("ext/freertos")[0]
        for changed in (replace(child, parent="ext/missing"),
                        replace(child, parent=child.path),
                        replace(child, path="outside/child"),
                        replace(child, scope="host-test")):
            entries = tuple(changed if item == child else item
                            for item in catalogue.ALL_DEPENDENCIES)
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                catalogue.validate_catalogue(entries)

    def test_source_paths_and_transport_must_be_safe_exact_values(self):
        first, *rest = catalogue.ALL_DEPENDENCIES
        for changed in (replace(first, path="../external"),
                        replace(first, fetch_url="https://user@github.com/a/b.git"),
                        replace(first, fetch_url="https://github.com/a/b.git?x=1"),
                        replace(first, notice="../LICENSE"),
                        replace(first, scope="all")):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                catalogue.validate_catalogue((changed, *rest))

    def test_generator_input_has_the_same_validated_parent_graph(self):
        self.assertEqual(catalogue.validate_catalogue(
            item for item in catalogue.ALL_DEPENDENCIES),
            catalogue.ALL_DEPENDENCIES)


if __name__ == "__main__":
    unittest.main()
