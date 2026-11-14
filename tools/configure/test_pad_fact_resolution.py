"""Declared pad coordinates and fact paths drive the same developer views."""
from copy import deepcopy
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

from tools.configure import configure, experience, physical_facts
from tools.configure import test_data_package as data_fixture


class PadFactResolutionTests(unittest.TestCase):
    def test_named_pad_routes_and_gpio_masks_use_explicit_coordinates(self):
        pads = {"IO_ENABLE": {"registers": "GPIO_A", "port_index": 3, "number": 11}}
        provider = SimpleNamespace(PAD_FACTS=pads)
        soc = {"variants": {"TEST_PART": {"pins": ["IO_ENABLE"]}},
               "controllers": {"PORT_A": {"kind": "gpio", "modes": ["output"],
                                           "pins": ["IO_ENABLE"]}}}
        route = {"id": "enable", "kind": "gpio", "controller": "PORT_A",
                 "variants": ["TEST_PART"], "modes": ["output"],
                 "pins": [{"pin": "IO_ENABLE", "function": "gpio", "af": 0}],
                 "source": "isolated declaration contract"}
        routes = configure.resolve_routes({"schema_version": 1, "routes": [route]},
                                          soc, "TEST_PART", provider)
        board = {"bindings": [{"id": "enable", "controller": "PORT_A", "route": "enable",
                                "initial": 1 << 11}]}
        resolved = configure.resolve_bindings(board, routes, "TEST_PART", pad_facts=pads)
        self.assertEqual(resolved["enable"]["mask"], 1 << 11)
        with self.assertRaises(ValueError):
            configure.resolve_bindings(board, routes, "TEST_PART", pad_facts={})
        with self.assertRaises(ValueError):
            configure.resolve_routes({"schema_version": 1, "routes": [route]},
                                     soc, "TEST_PART", SimpleNamespace(PAD_FACTS={}))

    def test_conditional_pad_explicitly_refers_to_a_declared_gpio(self):
        pads = {"IO_2": {"registers": "GPIO_A", "port_index": 0, "number": 2},
                "OTHER": {"registers": "GPIO_B", "port_index": 1, "number": 3}}
        condition = {"gpio": "IO_2", "condition": "reviewed analog switch",
                     "source": "isolated declaration contract", "status": "not_exposed"}
        physical_facts.package_pins(["OTHER", "ANALOG_2"],
                                    {"ANALOG_2": condition}, {"ANALOG_2": "IO_2"}, pad_facts=pads)
        with self.assertRaises(ValueError):
            physical_facts.package_pins(["OTHER", "ANALOG_2"],
                {"ANALOG_2": {**condition, "gpio": "undeclared"}},
                {"ANALOG_2": "IO_2"}, pad_facts=pads)

    def test_preview_and_binding_query_share_manifest_fact_paths_and_pad_facts(self):
        root = data_fixture.DataPackageTests.fixture(self)
        manifest = json.loads((root / "package.json").read_text())
        bindings = json.loads((root / "bindings.json").read_text())
        soc = json.loads((root / "soc.json").read_text())
        bindings["pads"] = {pin: {"registers": "GPIO" + pin[1],
                                    "port_index": ord(pin[1]) - ord("A"),
                                    "number": int(pin[2:])}
                            for pin in next(iter(soc["variants"].values()))["pins"]}
        (root / "bindings.json").write_text(json.dumps(bindings))
        names = {"PA4": "IO_ENABLE", "PA9": "IO_TX", "PA10": "IO_RX"}

        def rename(value):
            if isinstance(value, str):
                return names.get(value, "pin:" + names[value[4:]]
                                 if value.startswith("pin:") and value[4:] in names else value)
            if isinstance(value, list):
                return [rename(item) for item in value]
            if isinstance(value, dict):
                return {names.get(key, key): rename(item) for key, item in value.items()}
            return value

        for name in ("soc.json", "routes.json", "bindings.json", "board/board.json"):
            path = root / name
            path.write_text(json.dumps(rename(json.loads(path.read_text()))))
        (root / "facts").mkdir()
        for key, name in list(manifest["facts"].items()):
            destination = root / "facts" / Path(name).name
            (root / name).rename(destination)
            manifest["facts"][key] = "facts/" + destination.name
        fact = root / manifest["facts"]["soc"]
        soc = json.loads(fact.read_text())
        if "references" in soc:
            soc["references"] = manifest["facts"]["references"]
        fact.write_text(json.dumps(soc))
        (root / "package.json").write_text(json.dumps(manifest))
        result = configure.resolve_ir(root / "assembly.toml", test_support=True)
        preview = experience.board_catalogue(configure, configure.ROOT,
            board_package=root / "board", soc_package=str(root), test_support=True)
        self.assertEqual(preview["board_bindings"]["status"]["mask"], 1 << 4)
        self.assertIn("IO_ENABLE", preview["routes"]["gpio-pa4"]["pins"][0]["pin"])
        queried = experience.binding_facts(configure, result, configure.ROOT)
        self.assertEqual(queried["part"], result.part)
        self.assertIs(result.provider_context.binding_policy.pad_facts,
                      result.provider_context.provider.PAD_FACTS)


if __name__ == "__main__":
    unittest.main()
