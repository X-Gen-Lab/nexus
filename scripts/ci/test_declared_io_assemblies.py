"""Keep reviewed exact-part mode selections in the actual ARM fixture graph."""

import json
from pathlib import Path
import tempfile
import unittest

from scripts.ci import provider_assemblies
from tools.configure import configure
from tools.maintenance import capabilities

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests/contracts/declared_io_assembly"
GROUPS = {'f407', 've-duplex', 'f407-input', 've-duplex-continuous', 've-word-adc-can', 'f407-output', 've-rx-idle', 've-rx-continuous'}
HISTORICAL_MODES = {'STM32F407ZGT6': {('GPIOE', 'input'), ('GPIOB', 'input'), ('GPIOG', 'output'), ('EXTI0', 'edge-event'), ('GPIOA', 'input'), ('GPIOA', 'output'), ('GPIOG', 'input')}}
VE_MODES = {
    ("ADC1", "trigger-dma-continuous"), ("CAN1", "classic-irq"),
    ("CAN2", "classic-irq"), ("EXTI0", "edge-event"), ("GPIOA", "input"),
    ("GPIOA", "output"), ("GPIOB", "input"), ("GPIOE", "input"),
    ("GPIOE", "output"), ("USART1", "dma-duplex"),
    ("USART1", "dma-duplex-continuous"), ("USART1", "irq-words"),
    ("USART1", "rx-dma-continuous"), ("USART1", "rx-dma-idle"),
}


class DeclaredModeIngress(unittest.TestCase):
    def fixtures(self):
        paths = sorted(FIXTURES.glob("*.toml"))
        self.assertEqual({path.stem for path in paths}, GROUPS)
        return paths

    def resolved(self):
        return provider_assemblies.validate_fixtures(ROOT, self.fixtures())

    def test_all_reviewed_groups_are_owned_resolvable_arm_projects(self):
        for row in self.resolved():
            with self.subTest(assembly=row["assembly"]):
                self.assertEqual(Path(row["project"]), FIXTURES)
                self.assertEqual(row["resolved"]["backend"], "baremetal")
                self.assertFalse(row["resolved"]["physical_qualified"])
                self.assertIsNone(row["resolved"]["physical_pcb_revision"])
                self.assertTrue(row["inputs"])

    def test_exact_part_selections_close_reviewed_positive_gaps(self):
        actual = {(row["resolved"]["part"], item["controller"], item["mode"])
                  for row in self.resolved()
                  for item in row["resolved"]["controllers"]}
        expected = {(part, controller, mode)
                    for part, modes in HISTORICAL_MODES.items()
                    for controller, mode in modes}
        self.assertEqual(len(expected), 7)
        expected |= {("STM32F407VET6", controller, mode)
                     for controller, mode in VE_MODES}
        expected.add(("STM32F407ZGT6", "GPIOB", "output"))
        self.assertEqual(actual, expected)

    def test_every_group_is_an_explicit_actual_provider_stage_input_once(self):
        plan = json.loads((ROOT / "tools/dev/plans/software.json").read_text())
        selected = {"{root}/" + path.relative_to(ROOT).as_posix()
                    for path in self.fixtures()}
        observed = []
        for stage in plan["stages"]:
            arguments = stage["args"]
            for index, value in enumerate(arguments):
                if value == "--fixture" and arguments[index + 1] in selected:
                    self.assertEqual(stage["checker"], "provider_assemblies")
                    self.assertEqual(stage["name"],
                                     "providers-multi_controller_assembly")
                    observed.append(arguments[index + 1])
        self.assertEqual(set(observed), selected)
        self.assertEqual(len(observed), len(selected))
        stage, = [stage for stage in plan["stages"] if
                  stage["name"] == "providers-multi_controller_assembly"]
        arguments = stage["args"]
        self.assertNotIn("--project", arguments)
        actual = [arguments[index + 1] for index, value in enumerate(arguments)
                  if value == "--fixture"]
        original = {"{root}/" + path.relative_to(ROOT).as_posix()
                    for path in (ROOT / "tests/contracts" /
                                 "multi_controller_assembly").glob("*.toml")}
        self.assertEqual(set(actual), selected | original)
        self.assertEqual(len(actual), len(selected | original))

    def test_capability_discovery_uses_the_same_authored_selections(self):
        indexed = {(row["family"], row["controller"], row["mode"]): row
                   for row in capabilities.collect(ROOT)}
        for row in self.resolved():
            relative = Path(row["assembly"]).relative_to(ROOT).as_posix()
            for item in row["resolved"]["controllers"]:
                key = (row["resolved"]["soc_family"], item["controller"],
                       item["mode"])
                with self.subTest(selection=key, fixture=relative):
                    self.assertIn(relative, indexed[key]["fixtures"])
                    self.assertTrue(indexed[key]["exports"])

    def test_configuration_queries_never_promote_link_or_physical_acceptance(self):
        for path in self.fixtures():
            selection = capabilities.selection(path, ROOT)
            with self.subTest(assembly=path.name):
                self.assertFalse(selection["physical_qualified"])
                self.assertEqual(selection["validation"]["configuration"],
                                 "validated")
                for name in ("host_models", "arm_image", "installed_sdk",
                             "software_candidate", "physical_hil"):
                    self.assertEqual(selection["validation"][name], "not_assessed")

    def test_reserved_and_package_absent_routes_stay_unselectable(self):
        cases = [
            ("stm32f4", "STM32F407VET6", "GPIOG", "gpio", mode)
            for mode in ("input", "output")
        ] + [
            ("stm32f4", "STM32F407VET6", "ETH", kind, mode)
            for kind, mode in (("eth", "raw-rmii"),
                               ("eth_frames", "rmii-vlan-sg-ptp"))
        ]
        for family, part, controller, kind, mode in cases:
            path = ROOT / "soc/stm32" / family
            facts = configure.load(path / "soc.json")
            routes = configure.resolve_routes(configure.load(path / "routes.json"),
                                              facts, part,
                                              configure.maintained(family))
            candidates = [route for route in routes.values()
                          if route["controller"] == controller
                          and route["kind"] == kind and mode in route["modes"]]
            with self.subTest(part=part, controller=controller, mode=mode):
                self.assertFalse(any(part in route["variants"]
                                     for route in candidates))
                route = candidates[0]["id"] if candidates else "unreviewed-pad"
                board = {"bindings": [{"id": "blocked", "controller": controller,
                                       "route": route}]}
                with self.assertRaisesRegex(ValueError, "no reviewed exact-part route"):
                    configure.resolve_bindings(board, routes, part)


if __name__ == "__main__":
    unittest.main()
