"""Behavioral rejection, identity and atomic-generation tests for the resolver."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest

import configure as gate


class ResolverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("soc/native/soc.json", "soc/native/routes.json",
                     "boards/native_reference/board.json"):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, destination)
        self.assembly = self.root / "assembly.json"
        self.data = {"schema_version": 1, "board_package": "boards/native_reference",
                     "backend": "native", "clock_profile": "model",
                     "controllers": [], "devices": [],
                     "memory_budgets": {"main_stack_bytes": 2048}, "layout": None}
        self.write(self.assembly, self.data)
        self.board_path = self.root / "boards/native_reference/board.json"
        self.soc_path = self.root / "soc/native/soc.json"
        self.routes_path = self.root / "soc/native/routes.json"

    def write(self, path, data):
        path.write_text(json.dumps(data), encoding="utf-8")

    def resolve(self):
        self.write(self.assembly, self.data)
        return gate.resolve(self.assembly, self.root)

    def mutate(self, path, callback):
        data = json.loads(path.read_text())
        callback(data)
        self.write(path, data)

    def selected(self, **extra):
        return {"id": "led", "binding": "led0", "mode": "output", **extra}

    def rejected(self, match):
        with self.assertRaisesRegex(gate.ConfigurationError, match):
            self.resolve()

    def test_minimal_has_exact_identity_and_no_tasks(self):
        result = self.resolve()
        self.assertEqual(result["part"], "NATIVE_MODEL")
        self.assertFalse(result["physical_qualified"])
        self.assertEqual(result["controllers"], [])
        self.assertEqual(len(result["inputs"]), 4)
        self.assertNotIn("tasks", result)

    def test_duplicate_json_keys_rejected(self):
        self.assembly.write_text('{"schema_version":1,"schema_version":1}')
        with self.assertRaisesRegex(gate.ConfigurationError, "Duplicate"):
            gate.resolve(self.assembly, self.root)

    def test_unknown_assembly_key_rejected(self):
        self.data["business_worker"] = True
        self.rejected("unknown")

    def test_unknown_board_key_rejected(self):
        self.mutate(self.board_path, lambda item: item.update({"flash_size": 999}))
        self.rejected("unknown")

    def test_schema_boolean_rejected(self):
        self.data["schema_version"] = True
        self.rejected("integer required")

    def test_budget_boolean_rejected(self):
        self.data["memory_budgets"]["main_stack_bytes"] = True
        self.rejected("integer required")

    def test_budget_overflow_rejected(self):
        self.data["memory_budgets"]["main_stack_bytes"] = 1 << 32
        self.rejected("integer required")

    def test_stack_alignment_rejected(self):
        self.data["memory_budgets"]["main_stack_bytes"] = 2049
        self.rejected("8-byte")

    def test_stack_exhaustion_rejected(self):
        self.data["memory_budgets"]["main_stack_bytes"] = 1048576
        self.rejected("exhaust")

    def test_unknown_clock_rejected(self):
        self.data["clock_profile"] = "made-up"
        self.rejected("Unknown clock")

    def test_oscillator_contradiction_rejected(self):
        self.mutate(self.board_path, lambda item: item["clocks"].update(hse_hz=8000000))
        self.rejected("oscillator")

    def test_abi_contradiction_rejected(self):
        self.data["abi"] = {"arch": "cortex-m4", "fpu": "none", "float_abi": "soft"}
        self.rejected("ABI contradicts")

    def test_unknown_exact_variant_rejected(self):
        self.mutate(self.board_path, lambda item: item.update(soc="OTHER_MODEL"))
        self.rejected("exact part")

    def test_wrong_host_backend_rejected(self):
        self.data["backend"] = "freertos"
        self.rejected("host model")

    def test_missing_binding_rejected(self):
        self.data["controllers"] = [self.selected(binding="made-up")]
        self.rejected("Unknown Board binding")

    def test_unsupported_mode_rejected(self):
        self.data["controllers"] = [self.selected(mode="dma")]
        self.rejected("Unsupported mode")

    def test_controller_and_pin_conflict_rejected(self):
        self.data["controllers"] = [self.selected(), self.selected(id="other")]
        self.rejected("Resource conflict pin:PA0")

    def test_reserved_pin_conflict_rejected(self):
        self.mutate(self.board_path, lambda item: item.update(reserved_resources=["PA0"]))
        self.data["controllers"] = [self.selected()]
        self.rejected("Resource conflict pin:PA0")

    def test_gpio_mask_mismatch_rejected(self):
        self.mutate(self.board_path, lambda item: item["bindings"][0].update(mask=2))
        self.rejected("mask differs")

    def test_gpio_initial_unauthorized_rejected(self):
        self.mutate(self.board_path, lambda item: item["bindings"][0].update(initial=2))
        self.rejected("unauthorized")

    def test_unknown_nested_pin_key_rejected(self):
        self.mutate(self.routes_path, lambda item: item["routes"][0]["pins"][0].update(auto=True))
        self.rejected("unknown")

    def test_invalid_route_af_rejected(self):
        self.mutate(self.routes_path, lambda item: item["routes"][0]["pins"][0].update(af=16))
        self.rejected("pin AF")

    def test_unreviewed_route_pin_rejected(self):
        self.mutate(self.routes_path, lambda item: item["routes"][0]["pins"][0].update(pin="PK15"))
        self.rejected("not permitted")

    def test_flash_geometry_rejected(self):
        self.mutate(self.soc_path, lambda item: item["variants"]["NATIVE_MODEL"]["flash"].update(erase_blocks=[1]))
        self.rejected("geometry")

    def test_memory_overlap_rejected(self):
        self.mutate(self.soc_path, lambda item: item["variants"]["NATIVE_MODEL"]["memory"].append({"id":"other","origin":536870912,"size":1,"dma":False,"linker":False}))
        self.rejected("Overlapping")

    def test_multiple_linker_domains_rejected(self):
        self.mutate(self.soc_path, lambda item: item["variants"]["NATIVE_MODEL"]["memory"].append({"id":"other","origin":268435456,"size":1024,"dma":False,"linker":True}))
        self.rejected("Exactly one")

    def test_declared_input_escape_rejected(self):
        self.mutate(self.board_path, lambda item: item.update(inputs=["../secret"]))
        self.rejected("escapes containment")

    def test_missing_input_rejected(self):
        self.soc_path.unlink()
        self.rejected("regular file")

    def test_successful_bundle_is_atomic_and_failure_invalidates(self):
        output = self.root / "bundle"
        result = gate.configure(self.assembly, output, self.root)
        self.assertEqual(json.loads((output / "resolved.json").read_text()), result)
        self.assertTrue((output / "memory.ld").is_file())
        self.data["clock_profile"] = "bad"
        self.write(self.assembly, self.data)
        with self.assertRaises(gate.ConfigurationError):
            gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_failed_generation_leaves_no_partial_bundle(self):
        from unittest.mock import patch
        with patch.object(gate, "generate", side_effect=OSError("disk-full")):
            with self.assertRaises(OSError):
                gate.configure(self.assembly, self.root / "bundle", self.root)
        self.assertFalse((self.root / "bundle").exists())
        self.assertFalse(list(self.root.glob(".nexus-config-*")))

    def test_unknown_existing_directory_not_removed(self):
        output = self.root / "bundle"
        output.mkdir()
        (output / "user.txt").write_text("keep")
        with self.assertRaisesRegex(gate.ConfigurationError, "not an owned"):
            gate.configure(self.assembly, output, self.root)
        self.assertEqual((output / "user.txt").read_text(), "keep")

    def test_input_hash_changes_effective_identity(self):
        before = self.resolve()
        self.mutate(self.board_path, lambda item: item["unknowns"].append("another limit"))
        after = self.resolve()
        self.assertNotEqual(before["configuration_sha256"], after["configuration_sha256"])

    def test_relocated_identical_input_has_identical_config(self):
        before = self.resolve()
        destination = self.root.parent / (self.root.name + "-relocated")
        shutil.copytree(self.root, destination)
        self.addCleanup(shutil.rmtree, destination)
        after = gate.resolve(destination / "assembly.json", destination)
        self.assertEqual(before["configuration_sha256"], after["configuration_sha256"])

    def test_unsupported_image_offset_rejected(self):
        self.data["layout"] = "layout.json"
        self.write(self.root / "layout.json", {"schema_version":1,"regions":[{"id":"image","origin":134217729,"size":1048575,"kind":"image"}]})
        self.rejected("erase boundaries")

    def test_generated_text_has_no_control_characters(self):
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        for path in output.iterdir():
            text = path.read_text()
            self.assertFalse(any(ord(char) < 32 and char not in "\n\t"
                                 for char in text), path.name)
        # Inspect the MCU-specific runtime hooks as well as the Native bundle.
        import bindings
        result = self.resolve()
        result["soc_family"] = "stm32f407"
        result["part"] = "STM32F407VET6"
        _, source = bindings.emit(result)
        self.assertIn(r"\brief C runtime", source)
        self.assertNotIn("\x08", source)

    def test_budget_schema_generated_without_double_stack(self):
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        budget = json.loads((output / "resource-budget.json").read_text())
        self.assertEqual(budget["msp_max"], 2048)
        linker = (output / "memory.ld").read_text()
        self.assertEqual(linker.count(". += _Min_Stack_Size"), 1)
        self.assertIn("__nx_resolved_sha256_7", linker)

    def test_irrelevant_controller_settings_rejected(self):
        self.data["controllers"] = [self.selected(baud=115200)]
        self.rejected("Unsupported gpio configuration fields")

    def test_reserved_c_identifier_rejected(self):
        self.data["controllers"] = [self.selected(id="for")]
        self.rejected("Reserved C identifier")

    def test_distinct_ids_with_same_c_symbol_rejected(self):
        self.data["controllers"] = [self.selected(id="a-b"), self.selected(id="a_b")]
        # Use an unconnected reviewed SPI model for the second binding.
        self.data["controllers"][1] = {"id":"a_b", "binding":"spi0", "mode":"short-poll"}
        self.rejected("generated C identifiers")

    def test_unknown_component_rejected(self):
        self.data["components"] = ["imaginary-service"]
        self.rejected("Unknown maintained component")

    def test_polling_does_not_claim_unused_irq(self):
        self.data["controllers"] = [{"id":"spi", "binding":"spi0", "mode":"short-poll"}]
        self.mutate(self.soc_path, lambda item: item["controllers"]["SPI0"].update(irq=["SPI0_IRQn"]))
        result = self.resolve()
        self.assertFalse(any(item["key"].startswith("irq:") for item in result["claims"]))

    def test_uart_storage_capacity_bounded(self):
        self.data["controllers"] = [{"id":"uart", "binding":"uart0", "mode":"irq-byte-event",
            "baud":115200, "irq_priority":5, "rx_profile":"events", "rx_capacity":4097}]
        self.rejected("UART RX capacity")

    def test_navigation_sidecar_binds_declared_bytes(self):
        output = self.root / "bundle"
        result = gate.configure(self.assembly, output, self.root)
        navigation = json.loads((output / "input_paths.json").read_text())
        self.assertEqual(set(navigation), {item["path"] for item in result["inputs"]})
        for item in result["inputs"]:
            self.assertEqual(gate.file_digest(Path(navigation[item["path"]])), item["sha256"])


class AdvancedResolverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.assembly = Path(self.temporary.name) / "assembly.json"

    def fixture(self, name):
        directory = gate.ROOT / "tests/contracts/stm32_assembly"
        result = json.loads((directory / ("ve-" + name + ".json")).read_text())
        result["board_package"] = str(directory / result["board_package"])
        return result

    def reject(self, data, message):
        self.assembly.write_text(json.dumps(data))
        with self.assertRaisesRegex(gate.ConfigurationError, message):
            gate.resolve(self.assembly)

    def test_shared_exti_priority_contradiction_rejected(self):
        data = self.fixture("exti")
        data["controllers"][1]["irq_priority"] = 6
        self.reject(data, "identical priorities")

    def test_exti_boolean_capacity_rejected(self):
        data = self.fixture("exti")
        data["controllers"][0]["event_capacity"] = True
        self.reject(data, "EXTI event capacity")

    def test_adc_unreviewed_sequence_rejected(self):
        data = self.fixture("adc-scan")
        data["controllers"][0]["channels"] = [1, 0]
        self.reject(data, "differs from reviewed")

    def test_adc_boolean_sampling_rejected(self):
        data = self.fixture("adc")
        data["controllers"][0]["sample_times"] = [True]
        self.reject(data, "ADC sample encoding")

    def test_pwm_fractional_divider_rejected(self):
        data = self.fixture("pwm")
        data["controllers"][0]["tick_hz"] = 333333
        self.reject(data, "exact maintained timer divider")

    def test_spi_without_child_rejected(self):
        data = self.fixture("spi")
        data["devices"] = []
        self.reject(data, "reviewed CS endpoint")

    def test_unknown_device_driver_rejected(self):
        data = self.fixture("spi")
        data["devices"][0]["driver"] = "generic-magic"
        self.reject(data, "Unknown maintained device driver")

    def test_bmp280_requires_real_component(self):
        data = self.fixture("spi")
        data["devices"][0]["driver"] = "bmp280"
        self.reject(data, "requires the maintained bmp280-spi")


if __name__ == "__main__":
    unittest.main()
