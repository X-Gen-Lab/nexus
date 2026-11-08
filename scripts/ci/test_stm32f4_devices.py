"""STM32F4 SDK selector and startup probe contracts, without platform claims."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "scripts/ci/stm32f4_devices.py"
CATALOG = ROOT / "soc/stm32/stm32f4/devices.json"


def implementation():
    if not GATE.is_file():
        raise AssertionError("STM32F4 SDK device compiler gate is not implemented")
    specification = importlib.util.spec_from_file_location("f4_devices_gate", GATE)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


class STM32F4DeviceProbeTests(unittest.TestCase):
    def test_catalog_covers_the_official_selector_without_ordering_part_claims(self):
        self.assertTrue(CATALOG.is_file(), "F4 SDK device catalog is missing")
        catalog = json.loads(CATALOG.read_text())
        selector = (ROOT / "vendors/st/cmsis_device_f4/Include/stm32f4xx.h").read_text()
        selected = dict(re.findall(
            r"#(?:if|elif) defined\((STM32F\w+)\)\s*\n#include \"([^\"]+)\"",
            selector))
        self.assertEqual(len(selected), 23)
        self.assertEqual(catalog["scope"], "sdk_device_compile")
        self.assertEqual({item["define"] for item in catalog["devices"]}, set(selected))
        for item in catalog["devices"]:
            self.assertEqual(item["header"], "Include/" + selected[item["define"]])
            self.assertEqual(item["startup"], "Source/Templates/gcc/startup_" +
                             Path(selected[item["define"]]).stem + ".s")
            self.assertNotIn("flash", item)
            self.assertNotIn("package", item)

    def test_irq_span_and_timebase_presence_are_device_specific(self):
        gate = implementation()
        profiles = gate.load_catalog(ROOT)["devices"]
        by_define = {item["define"]: item for item in profiles}
        self.assertEqual(by_define["STM32F407xx"]["external_irq_count"], 82)
        self.assertEqual(by_define["STM32F410Rx"]["external_irq_count"], 98)
        self.assertEqual(by_define["STM32F413xx"]["external_irq_count"], 102)
        self.assertTrue(by_define["STM32F407xx"]["timebase_presence"]["TIM2"])
        self.assertFalse(by_define["STM32F410Rx"]["timebase_presence"]["TIM2"])
        self.assertTrue(by_define["STM32F410Rx"]["timebase_presence"]["TIM5"])

    def test_selection_is_exact_and_unknown_or_duplicate_macros_are_rejected(self):
        gate = implementation()
        catalog = gate.load_catalog(ROOT)
        self.assertEqual(len(gate.select_profiles(catalog, ())), 23)
        selected = gate.select_profiles(catalog, ("STM32F407xx", "STM32F410Rx"))
        self.assertEqual(tuple(item["define"] for item in selected),
                         ("STM32F407xx", "STM32F410Rx"))
        for choices in (("STM32F407xx -DOTHER",), ("STM32F999xx",),
                        ("STM32F407xx", "STM32F407xx")):
            with self.subTest(choices=choices):
                with self.assertRaisesRegex(ValueError, "unknown|duplicate"):
                    gate.select_profiles(catalog, choices)

    def test_redirected_startup_or_corrupted_catalog_identity_is_rejected(self):
        gate = implementation()
        catalog = gate.load_catalog(ROOT)
        mutations = []
        wrong_startup = deepcopy(catalog)
        wrong_startup["devices"][0]["startup"] = "Source/Templates/gcc/startup_stm32f407xx.s"
        mutations.append(wrong_startup)
        duplicate = deepcopy(catalog)
        duplicate["devices"].append(deepcopy(duplicate["devices"][0]))
        mutations.append(duplicate)
        wrong_identity = deepcopy(catalog)
        wrong_identity["inputs"]["Include/stm32f4xx.h"]["sha256"] = "0" * 64
        mutations.append(wrong_identity)
        for value in mutations:
            with self.subTest(mutation=value["devices"][0]["define"]):
                with tempfile.TemporaryDirectory() as temporary:
                    path = Path(temporary) / "catalog.json"
                    path.write_text(json.dumps(value))
                    with self.assertRaisesRegex(ValueError, "startup|duplicate|identity"):
                        gate.load_catalog(ROOT, catalog_path=path)

    def test_missing_official_sdk_input_is_a_failure(self):
        gate = implementation()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = root / "soc/stm32/stm32f4/devices.json"
            catalog.parent.mkdir(parents=True)
            catalog.write_bytes(CATALOG.read_bytes())
            with self.assertRaisesRegex(ValueError, "missing|regular"):
                gate.load_catalog(root)

    def test_vector_size_and_reset_entry_cannot_belong_to_another_profile(self):
        gate = implementation()
        profile = next(item for item in gate.load_catalog(ROOT)["devices"]
                       if item["define"] == "STM32F407xx")
        symbols = ("08000000 00000188 R g_pfnVectors\n"
                   "08000188 00000050 W Reset_Handler\n")
        header = "  Machine: ARM\n  Entry point address: 0x8000189\n"
        gate.check_linked_image(profile, symbols, header)
        for bad_symbols, bad_header in (
                (symbols.replace("00000188 R", "00000184 R"), header),
                (symbols, header.replace("8000189", "8000201")),
                (symbols, header.replace("Machine: ARM", "Machine: X86-64"))):
            with self.assertRaisesRegex(ValueError, "vector|entry|ARM"):
                gate.check_linked_image(profile, bad_symbols, bad_header)

    def test_unknown_device_fails_before_executing_any_tool(self):
        gate = implementation()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            with patch.object(gate.Recorder, "execute") as execute:
                result = gate.run(ROOT, "missing-arm-none-eabi-gcc", output,
                                  devices=("STM32F999xx",))
            self.assertEqual(result, 1)
            execute.assert_not_called()
            report = json.loads((output / "results.json").read_text())
            self.assertEqual(report["status"], "failed")
            self.assertEqual(report["scope"], "sdk_device_compile")
            self.assertEqual(report["physical_status"], "not_executed")
            self.assertEqual(report["full_platform_status"], "not_qualified")

    def test_timebase_presence_requires_booleans(self):
        gate = implementation()
        catalog = gate.load_catalog(ROOT)
        catalog["devices"][0]["timebase_presence"]["TIM2"] = 1
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog.json"
            path.write_text(json.dumps(catalog))
            with self.assertRaisesRegex(ValueError, "boolean"):
                gate.load_catalog(ROOT, catalog_path=path)

    def test_failure_report_binds_the_executed_gate_and_catalog_inputs(self):
        gate = implementation()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            self.assertEqual(gate.run(ROOT, "missing-arm-none-eabi-gcc", output,
                                      devices=("STM32F999xx",)), 1)
            report = json.loads((output / "results.json").read_text())
            inputs = {Path(item["path"]): item for item in report["control_inputs"]}
            self.assertIn(GATE, inputs)
            self.assertIn(CATALOG, inputs)
            self.assertTrue(all(item["bytes"] > 0 for item in inputs.values()))

    def test_wrong_compiler_release_is_not_admitted(self):
        gate = implementation()
        gate.check_compiler_version(
            "arm-none-eabi-gcc (Arm GNU Toolchain 14.3.Rel1) 14.3.1", "14.3.rel1")
        for text in ("arm-none-eabi-gcc 13.2.1", "gcc 14.3.1"):
            with self.assertRaisesRegex(ValueError, "compiler|toolchain"):
                gate.check_compiler_version(text, "14.3.rel1")


if __name__ == "__main__":
    unittest.main()
