"""Package facts drive exact parts and generic build selections."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, emission
from tools.configure import integration_package, packages
from tools.configure import test_device_catalog as fixtures


class DeclarativeSelectionTests(unittest.TestCase):
    def fixture(self):
        fixture = fixtures.DeviceCatalogConfigurationTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        return fixture

    def test_new_same_device_part_needs_only_exact_fact_data(self):
        fixture = self.fixture()
        part = "STM32F407ZGT7"
        def add(soc):
            soc["variants"][part] = copy.deepcopy(
                soc["variants"]["STM32F407ZGT6"])
        fixture.update("soc/stm32/stm32f4/soc.json", add)
        def review(value):
            package = next(item for item in value["package_review"]
                           if "STM32F407ZGT6" in item["variants"])
            package["variants"].append(part)
        fixture.update("soc/stm32/stm32f4/references.json", review)
        def routes(value):
            for route in value["routes"]:
                if "STM32F407ZGT6" in route["variants"]:
                    route["variants"].append(part)
        fixture.update("soc/stm32/stm32f4/routes.json", routes)
        fixture.update("boards/stm32f407ve_sky_qingchun/board.json",
                       lambda board: board.update(soc=part))
        result = fixture.resolve()
        self.assertEqual(result.part, part)
        self.assertEqual(result.physical_part.device.macro, "STM32F407xx")
        self.assertFalse(result.physical_qualified)

    def test_bundle_exposes_one_complete_build_selection_protocol(self):
        result = self.fixture().resolve()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            emission.emit_bundle(result, output)
            source = (output / "selection.cmake").read_text()
        self.assertIn("set(NEXUS_SOC_CMAKE_ENTRY ", source)
        self.assertIn('set(NEXUS_SOC_TARGET "nexus_soc_stm32f4")', source)
        self.assertIn("set(NEXUS_SELECTION_VARIABLES ", source)
        self.assertIn("package.json", source)

    def test_alternate_uart_route_uses_authorized_soc_pins(self):
        path = configure.ROOT / (
            "tests/contracts/uart_rx_compact_assembly/f407-alternate-irq.toml")
        result = configure.resolve_ir(path)
        selected, = result.controllers
        self.assertEqual(tuple(pin["pin"] for pin in selected.pins),
                         ("PD5", "PD6"))
        self.assertEqual(tuple(pin["af"] for pin in selected.pins), (7, 7))

    def test_timebase_role_is_generated_from_binding_facts(self):
        result = self.fixture().resolve()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            emission.emit_bundle(result, output)
            header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_STM32_TIMEBASE_TIMER TIM2", header)
        self.assertIn("#define NEXUS_STM32_TIMEBASE_IRQ TIM2_IRQn", header)
        self.assertIn("#define NEXUS_STM32_DMA_SRAM_ORIGIN 0x20000000u", header)
        self.assertIn("#define NEXUS_STM32_DMA_SRAM_BYTES 131072u", header)

    def test_route_cannot_claim_an_af_outside_its_binding_contract(self):
        fixture = self.fixture()
        def invalid(value):
            route = next(item for item in value["routes"]
                         if item["id"] == "usart2-pd5-pd6")
            route["pins"][0]["af"] = 8
        fixture.update("soc/stm32/stm32f4/routes.json", invalid)
        with self.assertRaisesRegex(ValueError, "alternate.function|AF"):
            fixture.resolve()

    def test_loaded_manifest_cannot_change_before_input_identity_sealing(self):
        fixture = self.fixture()
        manifest = fixture.root / "soc/stm32/stm32f4/package.json"
        selected = packages.load(manifest)
        bind = configure.bind_input_identities

        def change(*arguments, **options):
            value = json.loads(manifest.read_text())
            value["cmake"]["target"] = "different_target"
            manifest.write_text(json.dumps(value))
            return bind(*arguments, **options)

        with patch.object(packages, "load", return_value=selected):
            with patch.object(configure, "bind_input_identities", side_effect=change):
                with self.assertRaisesRegex(ValueError, "changed.*package[.]json|[Mm]anifest.*changed"):
                    fixture.resolve()

    def test_loaded_source_fact_identity_cannot_be_rebound_to_new_bytes(self):
        fixture = self.fixture()
        selected = packages.load(fixture.root / "soc/stm32/stm32f4/package.json")
        fixture.update("soc/stm32/stm32f4/soc.json",
                       lambda soc: soc["provenance"].append("Changed after loading"))
        with patch.object(packages, "load", return_value=selected):
            with self.assertRaisesRegex(ValueError, "[Ss]ource fact.*changed"):
                fixture.resolve()

    def test_manifest_fact_paths_are_the_only_read_and_identity_authority(self):
        # Every fact path is selected from this fixture's actual manifest.
        fixture = self.fixture()
        directory = fixture.root / "soc/stm32/stm32f4"
        (directory / "facts").mkdir()
        for name in ("soc.json", "routes.json"):
            (directory / name).rename(directory / "facts" / name)
        original = configure.ROOT / "soc/stm32/stm32f4/package.json"
        value = json.loads(original.read_text())
        for name in value.get("binding_files", []):
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((original.parent / name).read_bytes())
        for key in ("soc", "routes"):
            value["facts"][key] = "facts/" + value["facts"][key]
        (directory / "bindings.json").write_bytes((original.parent /
                                                    "bindings.json").read_bytes())
        (directory / "CMakeLists.txt").write_text("# fixture package\n")
        manifest = directory / "package.json"
        manifest.write_text(json.dumps(value))
        load = packages.load
        paths = {}
        with patch.object(packages, "load",
                          side_effect=lambda path, **options: load(manifest, **options)):
            result = fixture.resolve(input_paths=paths)
        self.assertEqual(result.part, "STM32F407VET6")
        self.assertIn("soc/stm32/stm32f4/facts/soc.json", paths)
        self.assertIn("soc/stm32/stm32f4/facts/routes.json", paths)
        self.assertNotIn("soc/stm32/stm32f4/soc.json", paths)

    def test_one_resolution_cannot_replace_a_previously_read_fact_snapshot(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fact.json"
            path.write_text('{"value": 1}')
            snapshots = {}
            configure.load(path, snapshots)
            path.write_text('{"value": 2}')
            with self.assertRaisesRegex(ValueError, "[Cc]hanged"):
                configure.load(path, snapshots)

    def test_alternate_serial_routes_keep_selected_dma_requests_and_pin_banks(self):
        directory = configure.ROOT / "tests/contracts/uart_rx_compact_assembly"
        for suffix in ("dma", "spi", "i2c", "target"):
            with self.subTest(suffix=suffix):
                result = configure.resolve_ir(directory /
                                              f"f407-alternate-{suffix}.toml")
                with tempfile.TemporaryDirectory() as temporary:
                    emission.emit_bundle(result, Path(temporary))
                    source = (Path(temporary) / "bindings.c").read_text()
                if suffix == "dma":
                    self.assertIn(".stream = DMA2_Stream7", source)
                    self.assertIn(".channel = 5u", source)
                elif suffix == "spi":
                    self.assertIn(".stream = DMA2_Stream5", source)
                else:
                    self.assertIn("GPIOF", source)


if __name__ == "__main__":
    unittest.main()
