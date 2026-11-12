"""Complete explicit SDRAM parameter admission and unit-boundary contracts."""

from copy import deepcopy
from pathlib import Path
from dataclasses import FrozenInstanceError
from types import SimpleNamespace
import unittest


from tools.configure.stm32f4 import sdram
from tools.configure import authored, configure, ir


def selection():
    return {
        "bytes": 0x800000, "row_bits": 12, "column_bits": 8,
        "internal_banks": 4, "width_bits": 16, "cas": 3,
        "sdclk_div": 2, "read_pipe": 1, "initial_refreshes": 8,
        "max_sdclk_hz": 8000000, "refresh_window_us": 64000,
        "powerup_us": 100, "prepare_budget_us": 1000,
        "stop_budget_us": 1000,
        "timing": {
            "mrd": {"nanoseconds": 0, "cycles": 2},
            "xsr": {"nanoseconds": 70, "cycles": 1},
            "ras": {"nanoseconds": 42, "cycles": 1},
            "rc": {"nanoseconds": 63, "cycles": 1},
            "wr": {"nanoseconds": 0, "cycles": 2},
            "rp": {"nanoseconds": 18, "cycles": 1},
            "rcd": {"nanoseconds": 18, "cycles": 1},
        },
    }


class SdramAdmission(unittest.TestCase):
    def admit(self, value):
        try:
            sdram.validate(value, hclk_hz=16000000)
        except ValueError as error:
            self.fail(f"Complete explicit SDRAM input was rejected: {error}")

    def test_complete_explicit_reference_is_admitted_without_mutation(self):
        value = selection()
        before = deepcopy(value)
        self.admit(value)
        self.assertEqual(before, value)

    def test_shape_units_types_and_exact_first_geometry_never_fall_back(self):
        mutations = [
            lambda p: p.update(unknown=True),
            lambda p: p.pop("initial_refreshes"),
            lambda p: p.update(bytes=0x400000),
            lambda p: p.update(width_bits=32),
            lambda p: p.update(sdclk_div=True),
            lambda p: p.update(prepare_budget_us=0),
            lambda p: p.update(stop_budget_us=1000001),
            lambda p: p["timing"].update(extra={"cycles": 1, "nanoseconds": 0}),
            lambda p: p["timing"]["xsr"].update(cycles=True),
            lambda p: p["timing"]["mrd"].update(cycles=0),
            lambda p: p["timing"]["rc"].update(nanoseconds=0x100000000),
        ]
        for mutation in mutations:
            value = selection()
            mutation(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                sdram.validate(value, hclk_hz=16000000)

    def test_selected_sdclk_must_not_exceed_explicit_endpoint_limit(self):
        value = selection()
        value["max_sdclk_hz"] -= 1
        with self.assertRaises(ValueError):
            sdram.validate(value, hclk_hz=16000000)
        value["max_sdclk_hz"] += 1
        self.admit(value)

    def test_timing_rounds_up_and_relational_constraints_fit_actual_fields(self):
        value = selection()
        value["timing"]["ras"] = {"nanoseconds": 0, "cycles": 16}
        value["timing"]["rp"] = {"nanoseconds": 0, "cycles": 1}
        with self.assertRaises(ValueError):
            sdram.validate(value, hclk_hz=16000000)
        value = selection()
        value["timing"]["xsr"]["nanoseconds"] = 2001
        with self.assertRaises(ValueError):
            sdram.validate(value, hclk_hz=16000000)

    def test_refresh_minimum_and_forbidden_count_do_not_claim_a_legal_plan(self):
        value = selection()
        value["refresh_window_us"] = 1
        with self.assertRaises(ValueError):
            sdram.validate(value, hclk_hz=16000000)
        value["refresh_window_us"] = 31232
        for name, cycles in {"ras": 6, "rc": 12, "wr": 8,
                             "rp": 6, "rcd": 11}.items():
            value["timing"][name] = {"nanoseconds": 0, "cycles": cycles}
        with self.assertRaises(ValueError):
            sdram.validate(value, hclk_hz=16000000)
        value["refresh_window_us"] += 512
        self.admit(value)

    def test_named_endpoint_ir_keeps_original_timing_units_frozen(self):
        value = {"id": "ram", "controller": "memory",
                 "driver": "memory_window-endpoint", "binding": "ram",
                 "mode": "cpu-window", "parameters": selection()}
        owner = SimpleNamespace(id="memory", kind="external_bus",
                                mode="sdram-controller")
        try:
            endpoint = ir.EndpointIR.create(value, (owner,))
        except (ValueError, TypeError, KeyError) as error:
            self.fail(f"Validated SDRAM graph lacks named endpoint IR: {error}")
        self.assertEqual("SdramWindowOptions", type(endpoint.options).__name__)
        self.assertEqual(70, endpoint.options.parameters.timing.xsr.nanoseconds)
        self.assertEqual(1, endpoint.options.parameters.timing.xsr.cycles)
        with self.assertRaises(FrozenInstanceError):
            endpoint.options.parameters.timing.xsr.cycles = 2
        self.assertEqual(0xc0000000, endpoint.options.address)
        self.assertEqual(0x800000, endpoint.options.bytes)
        self.assertEqual(value, ir.thaw(endpoint.projection()))

    def test_authored_parameter_window_has_no_fictitious_async_bank_timings(self):
        value = {"schema": 2, "backend": "baremetal", "clock": "custom",
                 "board_package": ".", "memory": {"main_stack_bytes": 2048},
                 "memory_window": {"ram": {"controller": "memory",
                     "binding": "ram", "mode": "cpu-window",
                     "parameters": selection()}}}
        try:
            result = authored.normalize(value, Path("assembly.toml"),
                configure.ROOT, configure.fail, configure.obj,
                configure.integer, configure.text)
        except (ValueError, TypeError, KeyError) as error:
            self.fail(f"Explicit SDRAM endpoint was rejected by authoring: {error}")
        self.assertNotIn("read_timing", result.devices[0])
        self.assertNotIn("write_timing", result.devices[0])
        incomplete = deepcopy(value)
        incomplete["memory_window"]["ram"].pop("parameters")
        with self.assertRaises(ValueError):
            authored.normalize(incomplete, Path("assembly.toml"),
                configure.ROOT, configure.fail, configure.obj,
                configure.integer, configure.text)

    def test_endpoint_resolution_claims_exact_window_and_union_of_pads(self):
        resolve = getattr(sdram, "resolve_endpoint", None)
        complete = getattr(sdram, "complete_controller", None)
        self.assertTrue(callable(resolve), "SDRAM endpoint admission is absent")
        self.assertTrue(callable(complete), "SDRAM controller completion is absent")
        pads = [{"pin": pin, "af": 12} for pin in sorted(sdram.PINS)]
        route = {"id": "fmc-sdram-bank1-16", "controller": "FMC",
                 "kind": "memory_window", "modes": ["cpu-window"], "pins": pads}
        bindings = {"ram": {"route": route["id"]}}
        routes = {route["id"]: route}
        owner = {"id": "memory", "controller": "FMC", "kind": "external_bus",
                 "mode": "sdram-controller", "route": "fmc-sdram-control", "pins": []}
        device = {"id": "ram", "controller": "memory",
                  "driver": "memory_window-endpoint", "binding": "ram",
                  "mode": "cpu-window", "parameters": selection()}
        claims = []
        claim = lambda *args: claims.append(args)
        resolve(device, owner, authored_kind="memory_window", bindings=bindings,
                routes=routes, claim=claim, board_conditions={},
                clock={"core_hz": 16000000})
        complete(owner, [device], claim=claim, bindings=bindings, routes=routes)
        self.assertEqual(set(sdram.PINS), {p["pin"] for p in owner["pins"]})
        self.assertEqual(39, len(claims))
        self.assertEqual("ip:FMC/bank1/data-window", claims[0][0])
        self.assertNotIn("address", device)
        self.assertNotIn("bytes", device)
        for bad in ({**device, "mode": "startup-memory"},
                    {**device, "read_timing": {}},
                    {**device, "parameters": {**selection(), "width_bits": 32}}):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                resolve(bad, owner, authored_kind="memory_window", bindings=bindings,
                        routes=routes, claim=claim, board_conditions={},
                        clock={"core_hz": 16000000})
        self.assertEqual(39, len(claims))
        with self.assertRaises(ValueError):
            complete(owner, [device, device], claim=claim,
                     bindings=bindings, routes=routes)

    def test_constructor_emits_only_static_faces_and_explicit_c_lifecycle(self):
        def record(value):
            return (SimpleNamespace(**{key: record(item)
                                       for key, item in value.items()})
                    if isinstance(value, dict) else value)
        masks = {"C": 0x000d, "D": 0xc703, "E": 0xff83,
                 "F": 0xf83f, "G": 0x8133}
        pins = tuple({"pin": f"P{port}{pin}", "af": 12}
                     for port, bits in masks.items() for pin in range(16)
                     if bits & (1 << pin))
        child = SimpleNamespace(id="ram", controller="memory",
                                driver="memory_window-endpoint",
                                options=SimpleNamespace(
                                    mode="cpu-window",
                                    parameters=record(selection())))
        item = SimpleNamespace(id="memory", controller="FMC",
                               mode="sdram-controller", dma=(), pins=pins)
        result = SimpleNamespace(part="STM32F479NIH6", devices=(child,),
                                 clock={"core_hz": 16000000})
        try:
            construction = sdram.constructor(item, result)
        except ValueError as error:
            self.fail(f"Complete explicit static SDRAM graph was rejected: {error}")
        self.assertEqual("nx_stm32_sdram_t", construction.state_type)
        self.assertEqual("nx_stm32_sdram_admit", construction.admission[0].function)
        self.assertEqual("nx_stm32_sdram_start", construction.initialize.function)
        self.assertEqual(2, len(construction.initialize.arguments))
        self.assertEqual("nx_stm32_sdram_stop", construction.stop[0].function)
        self.assertEqual(38, len(construction.pin_snapshots))
        source = "\n".join(construction.definitions)
        self.assertIn("nx_stm32_sdram_window_ops", source)
        self.assertIn(".system = &g_nx_stm32_system", source)
        self.assertIn(".prepare_budget_us = 1000u", source)
        for forbidden in ("nx_deadline", "clock_read", "WRITE_REG", "while ("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
