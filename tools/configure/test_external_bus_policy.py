"""External memory policy reuses an IP contract with package-owned identities."""
from copy import deepcopy
import unittest


def facts():
    value = {
        "schema_version": 1,
        "controllers": {
            "MEMORY_A": {
                "ip": "async-banked-v1", "modes": ["async16", "external-memory"],
                "alternate": 6, "max_hclk_hz": 120000000,
                "max_burst_hz": 40000000, "signal_prefix": "BUS_",
                "chip_selects": {"1": "CS1", "2": "CS2", "3": "CS3", "4": "CS4"},
                "nand_chip_selects": {"2": "CS1", "3": "CS2"},
                "nand_latches": ["A16", "A17"],
                "window_origin": 0x50000000, "bank_stride": 0x1000000,
                "nand_origin": 0x70000000, "nand_stride": 0x10000000,
                "nand_window_bytes": 0x40000,
            },
            "MEMORY_B": {"ip": "sdram-v1", "modes": ["sdram-controller"],
                         "alternate": 7, "max_hclk_hz": 120000000},
            "SERIAL_A": {"ip": "serial-nor-v1", "modes": ["qspi-indirect-sdr"],
                         "max_hclk_hz": 120000000},
        },
        "endpoints": {
            "bank-route": {"controller": "MEMORY_A", "kind": "memory_window",
                           "modes": ["cpu-window", "startup-memory"],
                           "protocol": "fixed-window", "bank": 2,
                           "address": 0x51000000, "bytes": 65536},
            "sdram-route": {"controller": "MEMORY_B", "kind": "memory_window",
                            "modes": ["cpu-window", "graphics-owned"],
                            "protocol": "sdram", "address": 0x90000000},
            "serial-route": {"controller": "SERIAL_A", "kind": "flash_async",
                             "modes": ["nor-indirect"], "protocol": "serial-nor-read",
                             "part": "W25Q128JV", "max_read_hz": 50000000,
                             "cs_high_ns": 50, "functions": ["select", "clock", "io0", "io1"]},
        },
        "graphics": {"display_controller": "SCANOUT_A", "image_controller": "BLIT_A",
                     "image_route": "blit-route", "display_routes": {"panel-route": [5, 6, 5]},
                     "alternate": 8, "max_pixel_hz": 50000000,
                     "clock_path": ["pixel", "hz"],
                     "memory_producers": [{"controller": "MEMORY_B",
                                            "mode": "sdram-controller",
                                            "endpoint_mode": "graphics-owned"}]},
    }

    for row in value["controllers"].values():
        row["contexts"] = {mode: {"backends": ["baremetal"],
                                  "clock_profiles": ["explicit-run"]}
                           for mode in row["modes"]}
    return value



class ExternalBusPolicyTests(unittest.TestCase):
    def policy(self, value=None):
        from tools.configure.policies.external_bus import ExternalBusPolicy
        return ExternalBusPolicy.from_validated(facts() if value is None else value)

    @staticmethod
    def endpoint():
        return {"id": "ram", "driver": "memory_window-endpoint", "binding": "ram",
                "controller": "bus", "mode": "cpu-window",
                "read_timing": {"address_setup": 0, "address_hold": 1,
                                "data_setup": 2, "bus_turn": 0},
                "write_timing": {"address_setup": 0, "address_hold": 1,
                                 "data_setup": 2, "bus_turn": 0}}

    def test_second_package_controller_bank_and_window_are_data_only(self):
        policy = self.policy()
        child = self.endpoint()
        controller = {"id": "bus", "controller": "MEMORY_A", "mode": "async16",
                      "route": "control-route"}
        routes = {"bank-route": {"id": "bank-route", "controller": "MEMORY_A",
                  "kind": "memory_window", "modes": ["cpu-window", "startup-memory"],
                  "pins": [{"pin": "PX1", "function": "BUS_D0", "af": 6}]}}
        bindings = {"ram": {"route": "bank-route"}}
        claims = []
        policy.resolve_endpoint(child, controller, authored_kind="memory_window",
            bindings=bindings, routes=routes, claim=lambda *row: claims.append(row),
            board_conditions=(), clock={"core_hz": 100000000})
        self.assertEqual((0x51000000, 65536), (child["address"], child["bytes"]))
        self.assertEqual("ip:MEMORY_A/bank2", claims[0][0])
        policy.complete_controller(controller, [child], bindings=bindings,
                                   routes=routes, claim=lambda *row: claims.append(row))
        self.assertEqual("PX1", controller["pins"][0]["pin"])
        self.assertEqual("pin:PX1", claims[-1][0])

    def test_clock_and_protocol_profile_come_from_selected_package(self):
        policy = self.policy()
        parameters = {"part": "W25Q128JV", "prescaler": 1,
                      "cs_high_cycles": 3, "read_budget_us": 100,
                      "prepare_budget_us": 200, "stop_budget_us": 300}
        policy.validate_serial(parameters, hclk_hz=100000000,
                               controller="SERIAL_A", route="serial-route", mode="nor-indirect")
        with self.assertRaises(ValueError):
            policy.validate_serial(parameters, hclk_hz=120000001,
                                   controller="SERIAL_A", route="serial-route", mode="nor-indirect")
        changed = deepcopy(parameters)
        changed["part"] = "W25Q128JVSIQ"
        with self.assertRaises(ValueError):
            policy.validate_serial(changed, hclk_hz=100000000,
                                   controller="SERIAL_A", route="serial-route", mode="nor-indirect")

    def test_graphics_lease_orders_package_memory_owner_before_consumers(self):
        controllers = [{"id": "display", "kind": "display", "memory_window": "ram"},
                       {"id": "bus", "kind": "external_bus", "controller": "MEMORY_B",
                        "mode": "sdram-controller"}]
        endpoint = {"id": "ram", "controller": "bus", "driver": "memory_window-endpoint",
                    "mode": "graphics-owned"}
        self.policy().complete_plan(controllers, [endpoint])
        self.assertEqual(["bus", "display"], [item["id"] for item in controllers])
        with self.assertRaises(ValueError):
            self.policy().complete_plan(controllers[0:1], [endpoint])

    def test_absent_or_unknown_ip_never_inherits_f4_defaults(self):
        for mutate in (lambda p: p["controllers"]["MEMORY_A"].update(ip="unknown"),
                       lambda p: p["endpoints"]["bank-route"].update(controller="FSMC"),
                       lambda p: p.update(unreviewed=True)):
            value = facts()
            mutate(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.policy(value)

    def test_protocol_kind_modes_and_window_cannot_contradict_the_ip(self):
        for mutate in (
            lambda p: p["endpoints"]["bank-route"].update(kind="parallel16"),
            lambda p: p["endpoints"]["bank-route"].update(modes=["unimplemented"]),
            lambda p: p["endpoints"]["bank-route"].update(address=0x52000000),
            lambda p: p["controllers"]["MEMORY_A"].update(modes=["unimplemented"]),
        ):
            value = facts()
            mutate(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.policy(value)

    def test_selected_execution_context_never_borrows_another_profile(self):
        policy = self.policy()
        route = {"kind": "external_bus", "controller": "MEMORY_A"}
        selection = {"mode": "async16"}
        policy.validate_context(selection, route, backend="baremetal", clock_profile="explicit-run")
        for backend, clock in (("freertos", "explicit-run"), ("baremetal", "other")):
            with self.subTest(backend=backend, clock=clock), self.assertRaises(ValueError):
                policy.validate_context(selection, route, backend=backend, clock_profile=clock)

    def test_every_existing_external_protocol_retains_its_complete_endpoint_plan(self):
        import json
        from pathlib import Path
        import tomllib
        from tools.configure.tests.external_memory_fixture import policy
        root = Path(__file__).resolve().parents[2]
        routes = {row["id"]: row for row in json.loads(
            (root / "soc/stm32/stm32f4/routes.json").read_text())["routes"]}
        paths = ("fsmc_assembly/ve-nand.toml", "fsmc_assembly/ve-nor.toml",
                 "fsmc_assembly/ve-psram.toml", "fsmc_assembly/zg-all.toml",
                 "stm32f4_sdram/f479ni-sdram.toml",
                 "stm32f4_qspi/f479ni-qspi-dma.toml",
                 "stm32f4_qspi/f479ni-qspi-map.toml")
        for name in paths:
            with self.subTest(name=name):
                source = root / "tests/contracts" / name
                authored = tomllib.loads(source.read_text())
                board = json.loads((source.parent / authored["board_package"] / "board.json").read_text())
                bindings = {row["id"]: row for row in board["bindings"]}
                owners = []
                for identity, selected in authored["external_bus"].items():
                    route = routes[bindings[selected["binding"]]["route"]]
                    owners.append({**selected, "id": identity, "kind": "external_bus",
                                   "controller": route["controller"], "route": route["id"]})
                children, claims = [], []
                selected_policy = policy()
                for kind in ("memory_window", "parallel16", "flash_async", "nand", "mapped_storage"):
                    for identity, value in authored.get(kind, {}).items():
                        child = {**deepcopy(value), "id": identity, "driver": kind + "-endpoint"}
                        owner = next(row for row in owners if row["id"] == child["controller"])
                        if owner["mode"] == "external-memory" and "parameters" in child:
                            with self.assertRaises(ValueError):
                                selected_policy.resolve_endpoint(deepcopy(child), {**owner, "mode": "async16"},
                                    authored_kind=kind, bindings=bindings, routes=routes,
                                    claim=lambda *row: None, board_conditions={}, clock={"core_hz": 168000000})
                        selected_policy.resolve_endpoint(child, owner, authored_kind=kind,
                            bindings=bindings, routes=routes, claim=lambda *row: claims.append(row),
                            board_conditions={}, clock={"core_hz": 168000000 if authored["clock"] == "hse8-pll168" else 16000000})
                        children.append(child)
                for owner in owners:
                    selected_policy.complete_controller(owner,
                        [row for row in children if row["controller"] == owner["id"]],
                        bindings=bindings, routes=routes, claim=lambda *row: claims.append(row))
                    self.assertTrue(owner["pins"])
                    self.assertEqual(len(owner["pins"]), len({row["pin"] for row in owner["pins"]}))
                selected_policy.complete_plan(owners, children)
                self.assertTrue(claims)
                for child in children:
                    if child["driver"] == "memory_window-endpoint":
                        self.assertIn("address", child)

    def test_one_execution_key_selects_complete_endpoint_layout(self):
        from types import SimpleNamespace as Value
        policy = self.policy()
        for controller, mode, child_mode, expected in (
            ("MEMORY_A", "async16", "cpu-window", "runtime"),
            ("MEMORY_A", "async16", "startup-memory", "startup-memory"),
            ("MEMORY_A", "external-memory", "psram-window", "parameterized"),
            ("MEMORY_B", "sdram-controller", "graphics-owned", "graphics-owned"),
            ("SERIAL_A", "qspi-indirect-sdr", "nor-indirect", "command"),
        ):
            item = Value(kind="external_bus", controller=controller, mode=mode)
            child = Value(options=Value(mode=child_mode))
            self.assertEqual(expected, policy.endpoint_profile(item, (child,)))
        self.assertEqual("default", policy.endpoint_profile(Value(kind="gpio"), ()))
        with self.assertRaises(ValueError):
            policy.endpoint_profile(Value(kind="external_bus", controller="MEMORY_B",
                                          mode="sdram-controller"), ())

    def test_private_projection_references_exact_graphics_owner_without_global_dma_grant(self):
        from types import SimpleNamespace as Value
        value = facts()
        value["packages"] = {"TEST_PACKAGE": 80}
        selected = self.policy(value)
        endpoint = Value(id="ram", controller="bus", driver="memory_window-endpoint",
                         options=Value(mode="graphics-owned", address=0x90000000, bytes=1024))
        owner = Value(id="bus", controller="MEMORY_B", mode="sdram-controller")
        memory = ({"origin": 0x20000000, "size": 4096, "dma": True},)
        configuration = Value(physical_part=Value(package="TEST_PACKAGE"),
                              devices=(endpoint,), controllers=(owner,), memory=memory)
        display = Value(kind="display", options=Value(memory_window="ram"))
        values = selected.binding_values(display, configuration)["external_bus"]
        self.assertEqual("bus", values["graphics_owner"])
        self.assertIs(endpoint, values["graphics_endpoint"])
        self.assertEqual((0x20000000, 0x90000000), tuple(row.begin for row in values["regions"]))
        self.assertEqual(memory, configuration.memory)
        other = Value(kind="display", options=Value(memory_window=None))
        plain = selected.binding_values(other, configuration)["external_bus"]
        self.assertEqual((0x20000000,), tuple(row.begin for row in plain["regions"]))
        bus = Value(id="bus", kind="external_bus", controller="MEMORY_A", mode="async16", pins=())
        configuration.provider_context = Value(binding_policy=Value(pad_bindings=lambda pins: ()),
            domain_policies=Value(EXTERNAL_MEMORY=None))
        self.assertEqual(80, selected.binding_values(bus, configuration)["external_bus"]["package_pins"])

    def test_permanent_boot_pads_have_no_mutable_snapshot_allocation(self):
        from types import SimpleNamespace as Value
        from tools.configure.ir import PadBindingIR
        value = facts()
        value["packages"] = {"TEST_PACKAGE": 80}
        selected = self.policy(value)
        pads = (PadBindingIR("PX1", "GPIOX", 23, 1, 6, "BUS_D0"),
                PadBindingIR("PX2", "GPIOX", 23, 2, 6, "BUS_D1"))
        configuration = Value(physical_part=Value(package="TEST_PACKAGE"),
            devices=(), provider_context=Value(binding_policy=Value(pad_bindings=lambda pins: pads),
                domain_policies=Value(EXTERNAL_MEMORY=Value(excluded_pins=lambda cfg: ("PX1",)))))
        item = Value(id="bus", kind="external_bus", controller="MEMORY_A", mode="async16", pins=())
        projected = selected.binding_values(item, configuration)["external_bus"]
        self.assertEqual((pads[1],), projected["cold_pads"])
        self.assertIs(pads[0], projected["pins"][0].pad)
        self.assertIsNone(projected["pins"][0].snapshot_index)
        self.assertEqual(0, projected["pins"][1].snapshot_index)
        self.assertTrue(projected["has_cold_pads"])

    def test_public_endpoint_capability_references_existing_parent_modes_only(self):
        selected = self.policy()
        self.assertEqual(("async16", "external-memory"),
                         selected.interface_parent_modes("MEMORY_A", "memory_window", "cpu-window"))
        self.assertEqual(("async16",),
                         selected.interface_parent_modes("MEMORY_A", "memory_window", "startup-memory"))
        self.assertEqual(("sdram-controller",),
                         selected.interface_parent_modes("MEMORY_B", "memory_window", "graphics-owned"))
        self.assertEqual(("qspi-indirect-sdr",),
                         selected.interface_parent_modes("SERIAL_A", "flash_async", "nor-indirect"))
        for controller, kind, mode in (
            ("UNKNOWN", "memory_window", "cpu-window"),
            ("SERIAL_A", "memory_window", "cpu-window"),
            ("MEMORY_A", "memory_window", "graphics-owned"),
            ("MEMORY_A", "spi", "polling"),
            (None, "memory_window", "cpu-window"),
        ):
            with self.subTest(controller=controller, kind=kind, mode=mode):
                self.assertEqual((), selected.interface_parent_modes(controller, kind, mode))

    def test_cpu_map_capability_never_authorizes_the_dma_parent_mode(self):
        from tools.configure.tests.external_memory_fixture import policy
        self.assertEqual(("qspi-indirect-sdr",), policy().interface_parent_modes(
            "QUADSPI", "mapped_storage", "nor-mapped-cpu"))

    def test_all_authored_endpoint_profiles_derive_capability_without_a_second_table(self):
        from tools.configure.tests.external_memory_fixture import policy
        selected = policy()
        for profile in selected.endpoints.values():
            for mode in profile["modes"]:
                parents = selected.interface_parent_modes(profile["controller"], profile["kind"], mode)
                with self.subTest(profile=profile, mode=mode):
                    self.assertTrue(parents)
                    self.assertTrue(set(parents).issubset(selected.controllers[profile["controller"]]["modes"]))

    def test_policy_fact_snapshot_is_immutable_and_has_no_family_import(self):
        value = facts()
        policy = self.policy(value)
        value["endpoints"]["bank-route"]["address"] = 0
        self.assertEqual(0x51000000, policy.endpoints["bank-route"]["address"])
        with self.assertRaises(TypeError):
            policy.endpoints["bank-route"]["address"] = 0
        import inspect
        source = inspect.getsource(type(policy))
        self.assertNotIn("tools.configure.stm32f4", source)


if __name__ == "__main__":
    unittest.main()
