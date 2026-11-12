"""STM32F4 SDK selector and startup probe contracts, without platform claims."""
from copy import deepcopy
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
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

    def test_irq_count_and_tail_tamper_are_rejected_by_the_actual_sdk(self):
        gate = implementation()
        catalog = gate.load_catalog(ROOT)
        row = next(item for item in catalog["devices"]
                   if item["define"] == "STM32F407xx")
        row["external_irq_count"] += 1
        row["last_external_irq"]["number"] += 1
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog.json"
            path.write_text(json.dumps(catalog))
            with self.assertRaisesRegex(ValueError, "official header"):
                gate.load_catalog(ROOT, catalog_path=path)

    def test_irq_enum_slot_and_official_vector_are_independent(self):
        gate = implementation()
        catalog = gate.load_catalog(ROOT)
        profiles = gate.vendor_device_catalog(catalog)
        for chip, enum in (("405", "RNG_IRQn"), ("407", "RNG_IRQn"),
                           ("415", "HASH_RNG_IRQn"), ("417", "HASH_RNG_IRQn")):
            with self.subTest(chip=chip):
                binding = profiles[f"stm32f{chip}"].irq_bindings[enum]
                self.assertEqual(binding.number, 80)
                self.assertEqual(binding.handler, "HASH_RNG_IRQHandler")
                with self.assertRaises(TypeError):
                    profiles[f"stm32f{chip}"].irq_bindings[enum] = binding

    def test_guessed_or_missing_vector_cannot_replace_locked_startup(self):
        gate = implementation()
        for change in ("guessed", "missing", "number"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as temporary:
                catalog = gate.load_catalog(ROOT)
                row = next(item for item in catalog["devices"]
                           if item["id"] == "stm32f407")
                if change == "guessed":
                    row["irq_bindings"]["RNG_IRQn"]["handler"] = "RNG_IRQHandler"
                elif change == "missing":
                    row["irq_bindings"].pop("RNG_IRQn")
                else:
                    row["irq_bindings"]["RNG_IRQn"]["number"] = 79
                path = Path(temporary) / "catalog.json"
                path.write_text(json.dumps(catalog))
                with self.assertRaisesRegex(ValueError, "startup vector"):
                    gate.load_catalog(ROOT, catalog_path=path)

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


class STM32F4PrivateHeaderCompileTests(unittest.TestCase):
    """Private integration follows the selected SDK; no new runtime support."""

    def compile(self, name, text, device, *, preprocess=False):
        gate = implementation()
        compiler = gate.tools("arm-none-eabi-gcc")["gcc"]
        includes = (ROOT, ROOT / "soc/stm32/stm32f4/private",
                    ROOT / "soc/stm32/stm32f4/private/usb_hal",
                    ROOT / "soc/stm32/stm32f4/drivers/ip/fs_device_pio",
                    ROOT / "soc/stm32/stm32f4/drivers/ip/legacy_mac",
                    ROOT / "soc/stm32/stm32f4/drivers/ip/classic_can",
                    ROOT / "soc/stm32/stm32f4/drivers/ip/advanced_timer",
                    ROOT / "tests/contracts/multi_controller",
                    ROOT / "core/include", ROOT / "arch/include",
                    ROOT / "io/include", ROOT / "io/private",
                    ROOT / "components/usb_control/include",
                    ROOT / "vendors/st/cmsis_device_f4/Include",
                    ROOT / "vendors/st/stm32f4_hal/Inc",
                    ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include")
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / (name + ".c")
            source.write_text(text)
            obj = directory / (name + ".o")
            command = [compiler, *gate.FLAGS, "-std=c11", "-Wall", "-Wextra",
                       "-Werror", "-DNEXUS_STM32_MODEL", "-DNX_I2C_MULTI_STM32",
                       "-D" + device,
                       *["-I" + str(path) for path in includes]]
            command += (["-E", "-dM", str(source)] if preprocess else
                        ["-c", str(source), "-o", str(obj)])
            result = subprocess.run(command, capture_output=True, text=True)
            evidence = os.environ.get("NEXUS_DEVICE_HEADER_PROBE_EVIDENCE")
            if evidence:
                output = Path(evidence) / name
                output.mkdir(parents=True, exist_ok=False)
                shutil.copy2(source, output / source.name)
                (output / "stdout.log").write_text(result.stdout)
                (output / "stderr.log").write_text(result.stderr)
                (output / "command.json").write_text(json.dumps(
                    {"command": command, "returncode": result.returncode,
                     "scope": "private_header_compile_only",
                     "runtime_support": False}, indent=2) + "\n")
                if obj.exists():
                    shutil.copy2(obj, output / obj.name)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_private_includes_preserve_the_exact_selected_device(self):
        headers = ("stm32f407_clock.h", "stm32f407_system.h",
                   "stm32f407_timer_output.h", "stm32f407_timer_hardware.h",
                   "stm32f407_eth.h", "stm32f407_eth_frames.h",
                   "stm32f407_timer_pins.h", "stm32f407_provider.h",
                   "../drivers/usb_device.c", "../drivers/can.c")
        for index, header in enumerate(headers):
            with self.subTest(header=header):
                self.compile(f"dispatch-{index}", f'#include "{header}"\n'
                             '#if !defined(__STM32F405xx_H) || '
                             'defined(__STM32F407xx_H)\n'
                             '#error "private include changed the device"\n'
                             '#endif\n', "STM32F405xx", preprocess=True)

    def test_usb_cold_headers_do_not_require_an_ethernet_controller(self):
        for device in ("STM32F405xx", "STM32F407xx"):
            with self.subTest(device=device):
                text = ('#include "stm32f407_usb_cold.h"\n'
                        '#include "stm32f407_usb_hs_cold.h"\n'
                        '#include "stm32f407_usb_host_cold.h"\n'
                        '#include "stm32f4_platform_lifecycle.h"\n')
                if device == "STM32F405xx":
                    text += ('#ifdef ETH\n#error "unexpected ETH peripheral"\n'
                             '#endif\n')
                text += ('_Static_assert(sizeof(USB_OTG_GlobalTypeDef) > 0, '
                         '"USB definition required");\n')
                self.compile("usb-cold-" + device, text, device)

    def test_ethernet_cold_headers_keep_their_complete_resource_contract(self):
        self.compile("ethernet-cold-STM32F407xx",
                     '#include "stm32f407_eth_cold.h"\n'
                     '#include "stm32f407_eth_frames_cold.h"\n'
                     '#include "stm32f4_platform_lifecycle.h"\n'
                     'static nx_stm32_eth_resource_receipt_t resources;\n'
                     'int main(void) {\n'
                     '    nx_stm32_eth_resources_capture(&resources, true);\n'
                     '    return (int)nx_stm32_eth_resources_close(&resources);\n'
                     '}\n', "STM32F407xx")


if __name__ == "__main__":
    unittest.main()
