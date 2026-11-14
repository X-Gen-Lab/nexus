"""Exact part references consume one SDK device catalog without opening SDKs."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate
from tools.configure.tests.f4_fixture import copy_package, load_f4_fixture


CATALOG = "soc/stm32/stm32f4/devices.json"
SOC = "soc/stm32/stm32f4/soc.json"


class DeviceCatalogConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        copy_package(self.root)
        for name in ("dependencies/platform-compatibility.json",
                     "boards/stm32f407ve_sky_qingchun/board.json"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, path)
        self.fixture = load_f4_fixture("STM32F407VET6", self.root)
        self.assembly = self.root / "assembly.toml"
        self.assembly.write_text('schema=2\nboard="stm32f407ve_sky_qingchun"\n'
                                 'backend="baremetal"\nclock="hse8-pll168"\n'
                                 '[memory]\nmain_stack_bytes=2048\n')

    def update(self, relative, mutate):
        path = self.root / relative
        value = json.loads(path.read_text())
        mutate(value)
        path.write_text(json.dumps(value))

    def resolve(self, **options):
        # Source-lock identity is a separate external boundary. This fixture
        # deliberately has no vendor checkout and makes no cold-build claim.
        def dependency(_root, relative, expected=None):
            return {"path": relative, "commit": expected}
        with patch.object(gate, "dependency_identity", side_effect=dependency):
            return gate.resolve_ir(self.assembly, self.root, **options)

    def test_catalog_is_a_sealed_fact_input_without_vendor_checkout(self):
        self.assertFalse((self.root / "vendors").exists())
        paths = {}
        result = self.resolve(input_paths=paths)
        self.assertEqual(result.physical_part.device.macro, "STM32F407xx")
        self.assertEqual(result.physical_part.device.startup, "startup_stm32f407xx.s")
        self.assertEqual(result.cpu_profile.irq.external_count, 82)
        self.assertIn(CATALOG, paths)
        records = {item["path"]: item["sha256"] for item in result.inputs}
        self.assertEqual(records[CATALOG], hashlib.sha256(
            (self.root / CATALOG).read_bytes()).hexdigest())
        self.assertIn(self.root / CATALOG, self.fixture.package.inputs)

    def test_unknown_catalog_device_id_is_rejected(self):
        self.update(SOC, lambda soc: soc["variants"]["STM32F407VET6"].update(
            device="stm32f499"))
        with self.assertRaisesRegex(gate.ConfigurationError, "[Uu]nknown.*device"):
            self.resolve()

    def test_repeated_legacy_macro_startup_object_is_rejected(self):
        self.update(SOC, lambda soc: soc["variants"]["STM32F407VET6"].update(
            device={"macro": "STM32F407xx", "startup": "startup_stm32f407xx.s"}))
        with self.assertRaisesRegex(gate.ConfigurationError, "device.*ID|device.*identifier"):
            self.resolve()

    def test_catalog_irq_change_is_projected_and_bound_without_vendor_checkout(self):
        before = self.resolve()
        def mutate(catalog):
            selected = next(item for item in catalog["devices"]
                            if item["define"] == "STM32F407xx")
            selected["external_irq_count"] += 1
            selected["last_external_irq"]["number"] += 1
        self.update(CATALOG, mutate)
        after = self.resolve()
        self.assertFalse((self.root / "vendors").exists())
        self.assertEqual(after.cpu_profile.irq.external_count, 83)
        before_inputs = {item["path"]: item["sha256"] for item in before.inputs}
        after_inputs = {item["path"]: item["sha256"] for item in after.inputs}
        self.assertNotEqual(before_inputs[CATALOG], after_inputs[CATALOG])
        self.assertEqual(after_inputs[CATALOG], hashlib.sha256(
            (self.root / CATALOG).read_bytes()).hexdigest())

    def test_normalized_irq_requires_the_same_selected_catalog_record(self):
        raw = gate.load(self.root / SOC)
        catalog = gate.load(self.root / CATALOG)
        selected = gate.catalog_soc_facts(raw, "STM32F407VET6", catalog)
        self.assertEqual(selected.device.external_irq_count, 82)
        provider = self.fixture.provider.for_soc(selected.soc)
        with self.assertRaisesRegex(gate.ConfigurationError, "catalog device"):
            gate.validate_soc(selected.soc, "STM32F407VET6", provider)
        selected.soc["irq"]["external_count"] += 1
        with self.assertRaisesRegex(gate.ConfigurationError, "IRQ count"):
            gate.validate_soc(selected.soc, "STM32F407VET6", provider,
                              catalog_device=selected.device)

    def test_duplicate_device_id_and_header_startup_mismatch_are_rejected(self):
        for case in ("duplicate", "startup"):
            with self.subTest(case=case):
                original = (gate.ROOT / CATALOG).read_bytes()
                (self.root / CATALOG).write_bytes(original)
                def mutate(catalog):
                    if case == "duplicate":
                        catalog["devices"].append(dict(catalog["devices"][0]))
                    else:
                        catalog["devices"][0]["startup"] = "startup_stm32f407xx.s"
                self.update(CATALOG, mutate)
                with self.assertRaisesRegex(gate.ConfigurationError, "duplicate|startup"):
                    self.resolve()

    def test_catalog_change_updates_configuration_input_identity(self):
        first = self.resolve()
        value = json.loads((self.root / CATALOG).read_text())
        (self.root / CATALOG).write_text(json.dumps(value, indent=4))
        second = self.resolve()
        self.assertEqual(first.physical_part.device, second.physical_part.device)
        before = {item["path"]: item["sha256"] for item in first.inputs}
        after = {item["path"]: item["sha256"] for item in second.inputs}
        self.assertNotEqual(before[CATALOG], after[CATALOG])


if __name__ == "__main__":
    unittest.main()
