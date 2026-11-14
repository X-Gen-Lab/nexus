"""External memory recipes retain selected storage, faces and fixed C calls."""
from dataclasses import dataclass, replace
from pathlib import Path
from types import MappingProxyType
import json
import tomllib
import unittest

from tools.configure.data_provider import Binding
from tools.configure.ir import (DmaMemoryBindingIR, EndpointIR, InterruptBindingIR,
                                PadBindingIR, mode_options)
from tools.configure.records import freeze
from tools.configure.calls import Symbol
from tools.configure.static_ir import Flags

ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class Integration:
    pads: tuple
    clock_inputs: object
    constants: object
    execution_profile: str
    irq_profile: object = None
    kernel_policy: str = "NX_IRQ_KERNEL_NONE"
    vector_bindings: tuple = ()


@dataclass(frozen=True)
class Controller:
    id: str
    kind: str
    controller: str
    mode: str
    integration: Integration
    options: object = None
    pins: tuple = ()


@dataclass(frozen=True)
class Configuration:
    controllers: tuple


def projection(name, mode, endpoint_mode):
    mapped = endpoint_mode == "nor-mapped-cpu"
    if name == "sdram":
        source = ROOT / "tests/contracts/stm32f4_sdram/f479ni-sdram.toml"
        kind, driver, route_name = "memory_window", "memory_window-endpoint", "fmc-sdram-bank1-16"
        controller_name, identifier = "FMC", "memory"
    else:
        source = ROOT / ("tests/contracts/stm32f4_qspi/f479ni-qspi-map.toml" if mapped
                         else "tests/contracts/stm32f4_qspi/f479ni-qspi-dma.toml")
        kind = "mapped_storage" if mapped else "flash_async"
        driver = kind + "-endpoint"
        route_name = "quadspi-bank1-mapped114" if mapped else "quadspi-bank1-sdr111"
        controller_name, identifier = "QUADSPI", "serial"
    document = tomllib.loads(source.read_text())
    child_id, child_values = next(iter(document[kind].items()))
    child_values.update(id=child_id, driver=driver, mode=endpoint_mode)
    if name == "sdram":
        child_values["address"] = 0xc0000000
    route = next(row for row in json.loads((ROOT / "soc/stm32/stm32f4/routes.json").read_text())["routes"]
                 if row["id"] == route_name)
    facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings.json").read_text())["pads"]
    pads = tuple(PadBindingIR(pin["pin"], facts[pin["pin"]]["registers"],
        facts[pin["pin"]]["port_index"], facts[pin["pin"]]["number"], pin["af"], pin["function"])
        for pin in route["pins"])
    memory = (DmaMemoryBindingIR(0x20000000, 0x20000,
              Flags((Symbol("NX_DMA_MEMORY_READ"), Symbol("NX_DMA_MEMORY_WRITE")))),)
    profile = endpoint_mode if name == "sdram" else "cpu-map" if mapped else "command"
    integration = Integration(pads, freeze({"core_hz": 16000000}),
        freeze({"external_bus": {"readable_regions": memory}}), profile)
    controller = Controller(identifier, "external_bus", controller_name, mode, integration)
    child = EndpointIR.create(child_values, (controller,))
    rows = json.loads((ROOT / "soc/stm32/stm32f4/bindings" / (name + ".json")).read_text())["bindings"]
    row = next(row for row in rows if mode == row.get("mode") or mode in row.get("modes", ()))
    selected = Binding.create(row).recipe
    built = selected.construct(controller, Configuration((controller,)), MappingProxyType({}),
                               MappingProxyType({}), (child,))
    return built


def graphics_projection(display, *, grant=False, single_format=False):
    from tools.configure.test_graphics_configuration import GraphicsConfigurationTests
    @dataclass(frozen=True)
    class IRQ:
        external_count: int = 93
        priority_bits: int = 4
        syscall_priority: int | None = None
    kind = "display" if display else "image"
    document = tomllib.loads(GraphicsConfigurationTests.graphics(display=display, image=not display, aux=display))
    identity, values = next(iter(document[kind].items()))
    irq = values.pop("irq")
    values.update(kind=kind, irq_priority=irq["priority"])
    if display and single_format:
        values["layers"][0]["formats"] = ["rgb565"]
    if grant:
        values["memory_window"] = "ram"
    options = mode_options(values)
    route_name = "ltdc-rgb565-reference" if display else "dma2d-memory"
    route = next(row for row in json.loads((ROOT / "soc/stm32/stm32f4/routes.json").read_text())["routes"] if row["id"] == route_name)
    facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings.json").read_text())["pads"]
    pads = tuple(PadBindingIR(pin["pin"], facts[pin["pin"]]["registers"], facts[pin["pin"]]["port_index"],
        facts[pin["pin"]]["number"], pin["af"], pin["function"]) for pin in route["pins"])
    regions = (DmaMemoryBindingIR(0x20000000, 0x20000,
               Flags((Symbol("NX_DMA_MEMORY_READ"), Symbol("NX_DMA_MEMORY_WRITE")))),)
    if grant:
        regions += (DmaMemoryBindingIR(0xc0000000, 0x800000, Symbol("NX_DMA_MEMORY_READ")),)
    vectors = ((InterruptBindingIR("LTDC_IRQn", "LTDC_IRQHandler", 88),
                InterruptBindingIR("LTDC_ER_IRQn", "LTDC_ER_IRQHandler", 89)) if display else
               (InterruptBindingIR("DMA2D_IRQn", "DMA2D_IRQHandler", 90),))
    integration = Integration(pads, freeze({"core_hz": 16000000}),
        freeze({"external_bus": {"regions": regions, "graphics_owner": "memory" if grant else None}}),
        "default", IRQ(), "NX_IRQ_KERNEL_NONE", vectors)
    item = Controller(identity, kind, "LTDC" if display else "DMA2D", values["mode"], integration, options)
    memory_owner = Controller("memory", "external_bus", "FMC", "sdram-controller", integration)
    configuration = Configuration((memory_owner, item) if grant else (item,))
    rows = json.loads((ROOT / "soc/stm32/stm32f4/bindings/graphics.json").read_text())["bindings"]
    row = next(row for row in rows if row["kind"] == kind)
    return Binding.create(row).recipe.construct(item, configuration, MappingProxyType({}), MappingProxyType({}), ())


def fsmc_projection(path, *, retained=()):
    from tools.configure.tests.external_memory_fixture import policy
    @dataclass(frozen=True)
    class Package:
        package: str
    @dataclass(frozen=True)
    class PadPolicy:
        pads: tuple
        def pad_bindings(self, pins):
            return self.pads
    @dataclass(frozen=True)
    class Startup:
        def excluded_pins(self, configuration):
            return retained
    @dataclass(frozen=True)
    class Domains:
        EXTERNAL_MEMORY: object
    @dataclass(frozen=True)
    class Context:
        binding_policy: PadPolicy
        domain_policies: Domains
    @dataclass(frozen=True)
    class FullConfiguration:
        controllers: tuple
        devices: tuple
        physical_part: Package
        provider_context: Context
    document = tomllib.loads(path.read_text())
    board = json.loads((path.parent / document["board_package"] / "board.json").read_text())
    bindings = {row["id"]: row for row in board["bindings"]}
    routes = {row["id"]: row for row in json.loads((ROOT / "soc/stm32/stm32f4/routes.json").read_text())["routes"]}
    identity, selected = next(iter(document["external_bus"].items()))
    owner_values = {**selected, "id": identity, "kind": "external_bus", "controller": "FSMC",
                    "route": bindings[selected["binding"]]["route"]}
    selected_policy = policy()
    children = []
    for kind in ("memory_window", "parallel16", "flash_async", "nand"):
        for child_id, child_values in document.get(kind, {}).items():
            values = {**child_values, "id": child_id, "driver": kind + "-endpoint"}
            selected_policy.resolve_endpoint(values, owner_values, authored_kind=kind,
                bindings=bindings, routes=routes, claim=lambda *row: None,
                board_conditions={}, clock={"core_hz": 168000000})
            children.append(values)
    selected_policy.complete_controller(owner_values, children, bindings=bindings,
                                       routes=routes, claim=lambda *row: None)
    facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings.json").read_text())["pads"]
    pads = tuple(PadBindingIR(pin["pin"], facts[pin["pin"]]["registers"], facts[pin["pin"]]["port_index"],
        facts[pin["pin"]]["number"], pin["af"], pin["function"]) for pin in owner_values["pins"])
    integrated = Integration(pads, freeze({"core_hz": 168000000}), freeze({}), "default")
    owner = Controller(identity, "external_bus", "FSMC", selected["mode"], integrated,
                       pins=freeze(owner_values["pins"]))
    children = tuple(EndpointIR.create(values, (owner,)) for values in children)
    package = "LQFP100" if board["soc"] == "STM32F407VET6" else "LQFP144"
    configuration = FullConfiguration((owner,), children, Package(package),
        Context(PadPolicy(pads), Domains(Startup() if retained else None)))
    constants = selected_policy.binding_values(owner, configuration)
    integrated = replace(integrated, constants=freeze(constants),
                         execution_profile=selected_policy.endpoint_profile(owner, children))
    owner = replace(owner, integration=integrated)
    configuration = replace(configuration, controllers=(owner,))
    rows = json.loads((ROOT / "soc/stm32/stm32f4/bindings/fsmc.json").read_text())["bindings"]
    row = next(row for row in rows if row.get("mode") == owner.mode)
    return Binding.create(row).recipe.construct(owner, configuration, MappingProxyType({}), MappingProxyType({}), children)


class ExternalMemoryRecipeTests(unittest.TestCase):
    def source(self, built):
        return "\n".join(line for item in built.static_declarations for line in item.render())

    def test_sdram_two_modes_keep_one_bank_owner_and_only_selected_graphics_adapter(self):
        for mode in ("cpu-window", "graphics-owned"):
            with self.subTest(mode=mode):
                built = projection("sdram", "sdram-controller", mode)
                source = self.source(built)
                self.assertEqual("nx_stm32_sdram_t", built.state_type)
                self.assertEqual("nx_stm32_sdram_start", built.initialize.function)
                self.assertEqual("nx_stm32_sdram_stop", built.stop[0].function)
                self.assertIn("nx_device_ram", source)
                self.assertEqual(mode == "graphics-owned", "s_nx_sdram_graphics_memory" in source)
                self.assertIn(".graphics_only = " + ("true" if mode == "graphics-owned" else "false"), source)
                self.assertEqual(38, len(built.pin_snapshots))

    def test_qspi_dma_is_optional_and_command_endpoints_retain_readonly_face(self):
        for mode in ("qspi-indirect-sdr", "qspi-indirect-dma"):
            with self.subTest(mode=mode):
                built = projection("qspi", mode, "nor-indirect")
                source = self.source(built)
                self.assertEqual("nx_stm32_qspi_t", built.state_type)
                self.assertIn("s_nx_flash_readonly_storage", source)
                self.assertIn("nx_device_storage", source)
                self.assertEqual(mode.endswith("dma"), "nx_stm32_qspi_dma_t" in source)
                self.assertEqual("nx_stm32_qspi_admit", built.admission[0].function)
                self.assertEqual("nx_stm32_qspi_stop", built.stop[0].function)
                self.assertEqual(4, len(built.pin_snapshots))

    def test_command_profile_does_not_import_the_unselected_mapping_component(self):
        rows = json.loads((ROOT / "soc/stm32/stm32f4/bindings/qspi.json").read_text())["bindings"]
        selected = Binding.create(next(row for row in rows if row["mode"] == "qspi-indirect-sdr"))
        command = selected.headers_for(Controller("serial", "external_bus", "QUADSPI",
            "qspi-indirect-sdr", Integration((), freeze({}), freeze({}), "command")))
        mapped = selected.headers_for(Controller("serial", "external_bus", "QUADSPI",
            "qspi-indirect-sdr", Integration((), freeze({}), freeze({}), "cpu-map")))
        self.assertNotIn("stm32f4_qspi_mapped.h", command)
        self.assertIn("stm32f4_qspi_mapped.h", mapped)

    def test_cpu_map_uses_composite_controller_and_exports_no_flash_bypass(self):
        built = projection("qspi", "qspi-indirect-sdr", "nor-mapped-cpu")
        source = self.source(built)
        self.assertEqual("nx_stm32_qspi_mapped_t", built.state_type)
        self.assertIn("nx_mapped_storage_slot_t", source)
        self.assertIn("nx_stm32_qspi_mapped_ops", source)
        self.assertNotIn("flash_readonly", source)
        self.assertNotIn("nx_stm32_qspi_dma_t", source)
        self.assertEqual("&s_nx_port_serial.controller", built.initialize.arguments[0].render())
        self.assertEqual(6, len(built.pin_snapshots))


class GraphicsRecipeTests(unittest.TestCase):
    def test_scanout_static_encoding_handles_single_or_two_formats_without_host_arithmetic(self):
        for single_format in (False, True):
            built = graphics_projection(True, single_format=single_format)
            source = "\n".join(line for item in built.static_declarations for line in item.render())
            self.assertIn("NX_STM32_LTDC_TIMING(", source)
            self.assertIn("NX_STM32_LTDC_FORMATS(NX_IMAGE_RGB565", source)
            self.assertEqual(not single_format, "NX_IMAGE_ARGB8888" in source)
            self.assertEqual(20, len(built.pin_snapshots))
            self.assertEqual(["LTDC_IRQHandler", "LTDC_ER_IRQHandler"], [irq.name for irq in built.irq_connections])

    def test_graphics_grants_reference_existing_bank_owner_without_new_mutable_storage(self):
        for display in (False, True):
            built = graphics_projection(display, grant=True)
            source = "\n".join(line for item in built.static_declarations for line in item.render())
            self.assertIn(".graphics_memory = &s_nx_sdram_graphics_memory", source)
            self.assertNotIn("nx_stm32_sdram_graphics_t", source)
            plain = graphics_projection(display)
            plain_source = "\n".join(line for item in plain.static_declarations for line in item.render())
            self.assertIn(".graphics_memory = NULL", plain_source)
            self.assertEqual("nx_stm32_ltdc_state_t" if display else "nx_stm32_image_state_t", built.state_type)


class FsmcRecipeTests(unittest.TestCase):
    def test_parameterized_banks_keep_driver_state_separate_and_parent_last_to_close(self):
        for filename in ("ve-nand.toml", "ve-nor.toml", "ve-psram.toml", "zg-all.toml"):
            with self.subTest(filename=filename):
                built = fsmc_projection(ROOT / "tests/contracts/fsmc_assembly" / filename)
                source = "\n".join(line for item in built.static_declarations for line in item.render())
                self.assertEqual("nx_stm32_fsmc_t", built.state_type)
                self.assertIn("nx_stm32_fsmc_parameters_t", source)
                self.assertEqual("nx_stm32_fsmc_stop", built.stop[-1].function)
                self.assertNotIn("g_nx_stm32_sram_boot_plan", source)
                if "nor" in filename or filename == "zg-all.toml":
                    self.assertIn("nx_stm32_fsmc_nor_async_ops", source)
                    self.assertIn("NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE", source)
                    self.assertIn("s_nx_flash_readonly_", source)
                if "nand" in filename or filename == "zg-all.toml":
                    self.assertIn("nx_stm32_fsmc_nand_ops", source)

    def test_reset_backed_sram_retains_global_bank_and_allocates_no_snapshot_for_permanent_pads(self):
        import tempfile
        from tools.configure.test_f407_fsmc_configuration import F407FSMCConfigurationTests
        with tempfile.TemporaryDirectory() as temporary:
            fixture = F407FSMCConfigurationTests(methodName="runTest")
            path = fixture.fixture(Path(temporary), sram=True, lcd=False, memory_mode="startup-memory")
            routes = json.loads((ROOT / "soc/stm32/stm32f4/routes.json").read_text())["routes"]
            retained = tuple(pin["pin"] for row in routes if row["id"] == "fsmc-ne3-is62wv51216" for pin in row["pins"])
            built = fsmc_projection(path, retained=retained)
            source = "\n".join(line for item in built.static_declarations for line in item.render())
            self.assertTrue(built.pre_c_effects)
            self.assertIn("g_nx_fsmc_bank_config_ext0", source)
            self.assertIn("g_nx_stm32_sram_boot_plan", source)
            self.assertNotIn("uint16_t s_nx_fsmc_pin_state_", source)
            self.assertEqual(0, len(built.pin_snapshots))
            self.assertEqual("NX_STM32_FSMC_BOOT_OWNER", built.initial_state.name)


if __name__ == '__main__':
    unittest.main()
