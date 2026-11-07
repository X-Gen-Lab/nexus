"""Explicit GPIO electrical intent reaches the selected F407 cold contract."""
from tools.configure.tests.native_fixture import native_fixture
import dataclasses
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, experience
from tools.configure.ir import freeze
from tools.configure.stm32f4.bindings import shared_irq_definitions


class F407GpioElectricalTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, *, kind="gpio", mode="input", options="", native=False):
        board_name = "native_reference" if native else "stm32f407_qiming_v31"
        board_path = (gate.ROOT / "tests/support/native/board/board.json"
                      if native else gate.ROOT / "boards" / board_name / "board.json")
        board = gate.load(board_path)
        board["id"] = "electrical-contract-software-fixture"
        board["source_revision"] = "silicon-contract-only"
        board["provenance"] = []
        board["unknowns"] = ["Software fixture; no new PCB qualification"]
        if native:
            binding = next(item for item in board["bindings"]
                           if item["id"] == "led0")
        elif kind == "exti":
            binding = {"id": "selected", "controller": "EXTI5",
                       "route": "exti5-pa5"}
        else:
            binding = {"id": "selected", "controller": "GPIOE",
                       "route": "gpio-pe3", "mask": 8, "initial": 8}
        binding = {**binding, "id": "selected"}
        board["bindings"] = [binding]
        (self.directory / "board.json").write_text(json.dumps(board))
        clock = "model" if native else "hse8-pll168"
        backend = "native" if native else "baremetal"
        irq = ('edge = "falling"\nevent_capacity = 4\n'
               'irq.priority = 5\n') if kind == "exti" else ""
        path = self.directory / "assembly.toml"
        path.write_text(
            'schema = 2\nboard_package = "."\n'
            f'backend = "{backend}"\nclock = "{clock}"\n'
            '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
            'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n'
            f'[{kind}.selected]\nbinding = "selected"\nmode = "{mode}"\n'
            + irq + options)
        return path

    def test_input_pull_up_and_down_are_immutable_and_reach_c(self):
        for pull, value in (("up", 1), ("down", 2)):
            with self.subTest(pull=pull):
                result = gate.resolve_ir(self.assembly(options=f'pull="{pull}"\n'), test_support=native_fixture(self.assembly(options=f'pull="{pull}"\n')))
                item, = result.controllers
                self.assertEqual(item.options.pull, pull)
                with self.assertRaises(dataclasses.FrozenInstanceError):
                    item.options.pull = "none"
                source = bindings.emit(result)[1]
                self.assertIn(f".pull = {value}u", source)
                self.assertIn(".output = false", source)

    def test_open_drain_output_reaches_c_without_runtime_storage(self):
        result = gate.resolve_ir(self.assembly(
            mode="output", options='pull="up"\noutput_type="open-drain"\n'), test_support=native_fixture(self.assembly(
            mode="output", options='pull="up"\noutput_type="open-drain"\n')))
        item, = result.controllers
        self.assertEqual(item.options.output_type, "open-drain")
        source = bindings.emit(result)[1]
        self.assertIn(".pull = 1u", source)
        self.assertIn(".output_type = 1u", source)
        self.assertIn("nx_stm32_gpio_prepare", source)
        self.assertNotIn("LL_GPIO_", source)

    def test_ordinary_existing_gpio_keeps_explicit_old_electrical_defaults(self):
        result = gate.resolve_ir(self.assembly(mode="output"), test_support=native_fixture(self.assembly(mode="output")))
        item, = result.controllers
        self.assertEqual((item.options.pull, item.options.output_type),
                         ("none", "push-pull"))
        source = bindings.emit(result)[1]
        self.assertIn(".pull = 0u", source)
        self.assertIn(".output_type = 0u", source)

    def test_invalid_electrical_intent_is_rejected(self):
        for options in ('pull="floating"\n', 'pull=true\n', 'pull=[]\n',
                        'output_type="invalid"\n',
                        'output_type="open-drain"\n'):
            with self.subTest(options=options):
                with self.assertRaises(gate.ConfigurationError):
                    gate.resolve_ir(self.assembly(options=options), test_support=native_fixture(self.assembly(options=options)))

    def test_unmaintained_provider_cannot_silently_ignore_pull(self):
        gate.resolve_ir(self.assembly(native=True, mode="output"), test_support=native_fixture(self.assembly(native=True, mode="output")))
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "does not maintain these electrical"):
            gate.resolve_ir(self.assembly(native=True, mode="output",
                                          options='pull="up"\n'), test_support=native_fixture(self.assembly(native=True, mode="output",
                                          options='pull="up"\n')))

    def test_exti_pull_is_passed_to_its_own_cold_pin_contract(self):
        result = gate.resolve_ir(self.assembly(
            kind="exti", mode="edge-event", options='pull="up"\n'), test_support=native_fixture(self.assembly(
            kind="exti", mode="edge-event", options='pull="up"\n')))
        item, = result.controllers
        self.assertEqual(item.options.pull, "up")
        self.assertIn(".pull = 1u", bindings.emit(result)[1])

    def test_each_exti_vector_group_has_one_exact_dispatcher(self):
        result = gate.resolve_ir(self.assembly(kind="exti", mode="edge-event"), test_support=native_fixture(self.assembly(kind="exti", mode="edge-event")))
        original, = result.controllers
        selected = []
        for line in (0, 3, 6, 9, 11, 15):
            selected.append(dataclasses.replace(
                original, id=f"edge{line}", controller=f"EXTI{line}",
                pins=(freeze({"pin": f"PF{line}", "function": "input", "af": 0}),)))
        source = "\n".join(shared_irq_definitions(selected))
        for vector, mask in (("EXTI0", 1), ("EXTI3", 8),
                             ("EXTI9_5", (1 << 6) | (1 << 9)),
                             ("EXTI15_10", (1 << 11) | (1 << 15))):
            self.assertEqual(source.count(f"void {vector}_IRQHandler(void) {{"), 1)
            self.assertIn(f"u, {mask}u);", source)

    def constrained(self, electrical, **arguments):
        path = self.assembly(**arguments)
        board_path = self.directory / "board.json"
        board = json.loads(board_path.read_text())
        board["bindings"][0]["electrical"] = electrical
        board_path.write_text(json.dumps(board))
        return path

    def test_matching_board_input_pull_constraint_is_immutable(self):
        result = gate.resolve_ir(self.constrained(
            {"mode": "input", "pull": "up"}, options='pull="up"\n'), test_support=native_fixture(self.constrained(
            {"mode": "input", "pull": "up"}, options='pull="up"\n')))
        constraint = result.board_bindings["selected"]["electrical"]
        self.assertEqual(constraint.mode, "input")
        self.assertEqual(constraint.pull, "up")
        with self.assertRaises(dataclasses.FrozenInstanceError):
            constraint.pull = "none"
        self.assertEqual(result.controllers[0].options.pull, "up")

    def test_board_required_pull_cannot_be_omitted_or_changed(self):
        for options in ("", 'pull="none"\n', 'pull="down"\n'):
            with self.subTest(options=options):
                with self.assertRaises(gate.ConfigurationError):
                    gate.resolve_ir(self.constrained(
                        {"mode": "input", "pull": "up"}, options=options), test_support=native_fixture(self.constrained(
                        {"mode": "input", "pull": "up"}, options=options)))

    def test_matching_board_output_release_and_initial_constraints(self):
        result = gate.resolve_ir(self.constrained(
            {"mode": "output", "pull": "up",
             "output_type": "open-drain", "initial": 8},
            mode="output", options='pull="up"\noutput_type="open-drain"\n'), test_support=native_fixture(self.constrained(
            {"mode": "output", "pull": "up",
             "output_type": "open-drain", "initial": 8},
            mode="output", options='pull="up"\noutput_type="open-drain"\n')))
        self.assertEqual(result.controllers[0].options.initial, 8)
        self.assertEqual(result.controllers[0].options.output_type, "open-drain")

    def test_board_output_type_cannot_be_silently_replaced(self):
        with self.assertRaises(gate.ConfigurationError):
            gate.resolve_ir(self.constrained(
                {"mode": "output", "output_type": "open-drain"},
                mode="output"), test_support=native_fixture(self.constrained(
                {"mode": "output", "output_type": "open-drain"},
                mode="output")))

    def test_board_constraint_rejects_nonapplicable_or_conflicting_facts(self):
        for electrical in ({"mode": "input", "output_type": "push-pull"},
                           {"mode": "input", "initial": 0},
                           {"mode": "input", "pull": "bad"},
                           {"mode": "output", "initial": 0},
                           {"mode": "output", "initial": True},
                           {"mode": "output", "initial": 16},
                           {"pull": "up"}, {"mode": "input", "typo": 1}):
            with self.subTest(electrical=electrical):
                with self.assertRaises(gate.ConfigurationError):
                    gate.resolve_ir(self.constrained(electrical), test_support=native_fixture(self.constrained(electrical)))

    def test_exti_board_pull_constraint_reaches_selected_cold_config(self):
        result = gate.resolve_ir(self.constrained(
            {"mode": "edge-event", "pull": "up"}, kind="exti",
            mode="edge-event", options='pull="up"\n'), test_support=native_fixture(self.constrained(
            {"mode": "edge-event", "pull": "up"}, kind="exti",
            mode="edge-event", options='pull="up"\n')))
        self.assertIn(".pull = 1u", bindings.emit(result)[1])
        with self.assertRaises(gate.ConfigurationError):
            gate.resolve_ir(self.constrained(
                {"mode": "edge-event", "pull": "up"}, kind="exti",
                mode="edge-event"), test_support=native_fixture(self.constrained(
                {"mode": "edge-event", "pull": "up"}, kind="exti",
                mode="edge-event")))

    def test_board_first_query_projects_required_electrical_facts_as_json(self):
        self.constrained({"mode": "input", "pull": "up"},
                         options='pull="up"\n')
        catalogue = experience.board_catalogue(
            gate, gate.ROOT, board_package=self.directory)
        view = experience.bindings_view(catalogue)
        projected = json.loads(json.dumps(view))
        self.assertEqual(projected["bindings"][0]["electrical"],
                         {"mode": "input", "pull": "up"})
        self.assertIn("Required electrical:", experience.render_bindings(view))


if __name__ == "__main__":
    unittest.main()
