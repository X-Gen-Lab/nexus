"""Named pin states remain a selected, typed, exact-pad software contract."""
from copy import deepcopy
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure.tests.assembly_fixture import render_toml
from tools.configure import matrix, bindings, configure, dependencies, factory


CANONICAL = "tools/configure/assemblies/g474-pin-states.toml"
BOARD = "cmake/package/verification/io_evolution/boards/g474-pin-states/board.json"


class PinStateConfigurationTests(unittest.TestCase):
    def resolve(self, change=None):
        if change is None:
            return configure.resolve_ir(configure.ROOT / CANONICAL)
        data = configure.load_assembly(configure.ROOT / CANONICAL).to_dict()
        board = json.loads((configure.ROOT / BOARD).read_text())
        change(data, board)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "board.json").write_text(json.dumps(board))
            data["board_package"] = str(root)
            path = root / "assembly.toml"
            path.write_text(render_toml(data))
            return configure.resolve_ir(path)

    def test_exact_canonical_options_are_immutable_register_values(self):
        result = self.resolve()
        self.assertEqual((result.soc_family, result.part, result.backend,
                          result.clock_profile),
                         ("stm32g474", "STM32G474RET6", "baremetal",
                          "hse24-pll170"))
        item, = result.controllers
        self.assertEqual((item.kind, item.mode, item.controller),
                         ("gpio", "named-states", "GPIOA"))
        self.assertEqual(type(item.options).__name__, "PinStateOptions")
        self.assertEqual(item.options.mask, 32)
        self.assertEqual((item.options.default.mode,
                          item.options.default.output_type,
                          item.options.default.speed,
                          item.options.default.pull,
                          item.options.default.levels), (1024, 0, 0, 0, 0))
        self.assertEqual(item.options.sleep.mode, 3072)
        with self.assertRaises(FrozenInstanceError):
            item.options.mask = 1
        self.assertFalse(result.physical_qualified)

    def test_face_is_selected_only_and_does_not_compete_with_gpio(self):
        selected = self.resolve()
        header, source = factory.emit(selected)
        self.assertNotIn("pin_state", factory.KINDS)
        self.assertIn('#include "nexus/io/pin_state.h"', header)
        self.assertIn("NX_PIN_STATE_ID_STATUS = 0", header)
        self.assertIn("NX_GPIO_ID_COUNT = 0", header)
        self.assertNotIn("NX_GPIO_ID_STATUS", header)
        self.assertIn("nx_factory_pin_state", source)
        for name in ("tests/contracts/g474_assembly/empty.toml",
                     "tests/contracts/g474_assembly/io.toml"):
            ordinary = configure.resolve_ir(configure.ROOT / name)
            old_header, old_source = factory.emit(ordinary)
            self.assertNotIn("pin_state", old_header + old_source)

    def test_static_plan_and_failed_init_cleanup_use_existing_lifecycle(self):
        source = bindings.emit(self.resolve())[1]
        self.assertIn('#include "stm32g474_pin_state.h"', source)
        self.assertLess(source.index('#include "stm32g474_pin_state.h"'),
                        source.index("static nx_stm32g474_pin_state_t"))
        self.assertIn("static const nx_pin_state_plan_t", source)
        self.assertIn("nx_stm32g474_pin_state_prepare", source)
        self.assertIn("nx_stm32g474_pin_state_stop", source)
        self.assertNotIn("nx_stm32g474_gpio_initialize", source)

    def test_generator_identity_binds_the_explicit_state_validator(self):
        paths, _ = dependencies.snapshot(mode="platform", family="stm32g474")
        self.assertIn("tools/configure/pin_states.py", paths)

    def test_named_states_are_not_optional_or_partially_defaulted(self):
        for missing in ("default", "sleep", "pin_states"):
            with self.subTest(missing=missing):
                def change(data, board):
                    binding = board["bindings"][0]
                    (binding if missing == "pin_states" else
                     binding["pin_states"]).pop(missing)
                with self.assertRaisesRegex(ValueError, "pin.states|named.states"):
                    self.resolve(change)

    def test_unknown_state_and_electrical_fields_are_rejected(self):
        for field in ("wake", "af", "retention", "surprise"):
            with self.subTest(field=field):
                def change(data, board):
                    board["bindings"][0]["pin_states"]["sleep"][field] = 1
                with self.assertRaisesRegex(ValueError, "pin.state|Unknown"):
                    self.resolve(change)
        def extra(data, board):
            board["bindings"][0]["pin_states"]["idle"] = {"mode": "input"}
        with self.assertRaisesRegex(ValueError, "pin.state|Unknown"):
            self.resolve(extra)

    def test_non_output_states_do_not_invent_latches_or_drive_settings(self):
        for key, value in (("level", 0), ("output_type", "push-pull"),
                           ("speed", "low")):
            with self.subTest(key=key):
                def change(data, board):
                    board["bindings"][0]["pin_states"]["sleep"][key] = value
                with self.assertRaisesRegex(ValueError, "pin.state|Unknown"):
                    self.resolve(change)

    def test_output_values_are_explicit_and_bounded_to_owned_pads(self):
        for key, value in (("level", True), ("level", 1),
                           ("output_type", "unknown"), ("speed", "unknown"),
                           ("mode", "alternate"), ("pull", "unknown")):
            with self.subTest(key=key, value=value):
                def change(data, board):
                    board["bindings"][0]["pin_states"]["default"][key] = value
                with self.assertRaises(ValueError):
                    self.resolve(change)
        for key in ("level", "output_type", "speed", "pull"):
            with self.subTest(missing=key):
                def change(data, board):
                    board["bindings"][0]["pin_states"]["default"].pop(key)
                with self.assertRaises(ValueError):
                    self.resolve(change)

    def test_default_output_must_equal_explicit_board_initial(self):
        for value in (32, None):
            with self.subTest(initial=value):
                def change(data, board):
                    binding = board["bindings"][0]
                    if value is None:
                        binding.pop("initial")
                    else:
                        binding["initial"] = value
                with self.assertRaisesRegex(ValueError, "pin.state|initial"):
                    self.resolve(change)

    def test_duplicate_pad_cannot_gain_a_second_face(self):
        def change(data, board):
            second = deepcopy(data["controllers"][0])
            second["id"] = "duplicate"
            data["controllers"].append(second)
        with self.assertRaisesRegex(ValueError, "Resource conflict"):
            self.resolve(change)

    def test_disjoint_pad_can_share_the_existing_bank_gate(self):
        def change(data, board):
            board["bindings"].append({"id": "other", "controller": "GPIOA",
                                      "route": "gpio-pa0", "initial": 0})
            data["controllers"].append({"id": "other", "binding": "other",
                                         "mode": "output",
                                         "authored_kind": "gpio"})
        result = self.resolve(change)
        self.assertEqual({item.id for item in result.controllers},
                         {"status", "other"})
        self.assertEqual({item.controller for item in result.controllers},
                         {"GPIOA"})

    def test_first_release_rejects_unreviewed_backend(self):
        def change(data, board):
            data["backend"] = "freertos"
        with self.assertRaisesRegex(ValueError, "named.states.*baremetal|pin.states.*baremetal"):
            self.resolve(change)

    def test_physical_catalogue_contains_exactly_one_new_mode(self):
        from tools.configure import selected_contracts
        rows = [row for row in matrix.support_catalogue(configure.ROOT)
                if row.mode == "named-states"]
        self.assertEqual([(row.family, row.part, row.controller,
                           row.kind, row.implementation, row.reviewed_routes)
                          for row in rows],
                         [("stm32g474", "STM32G474RET6", "GPIOA", "gpio",
                           "pin_state", ("gpio-pa5",))])


    def test_instance_identity_and_selection_enum_remain_distinct(self):
        header, _ = factory.emit(self.resolve())
        self.assertIn("typedef uint16_t nx_pin_state_id_t;", header)
        self.assertNotIn("nx_pin_state_instance_id_t", header)
        public = (configure.ROOT / "io/include/nexus/io/pin_state.h").read_text()
        self.assertIn("} nx_pin_state_selection_t;", public)
        self.assertIn("nx_pin_state_selection_t state", public)

    def test_malformed_container_types_are_configuration_errors(self):
        from tools.configure.providers.common import ConfigurationError
        for key, value in (("mode", []), ("mode", {}), ("pull", []),
                           ("pull", {}), ("output_type", []),
                           ("output_type", {}), ("speed", []),
                           ("speed", {}), ("level", [])):
            with self.subTest(key=key, value=value):
                def change(data, board):
                    board["bindings"][0]["pin_states"]["default"][key] = value
                with self.assertRaises(ConfigurationError):
                    self.resolve(change)


if __name__ == "__main__":
    unittest.main()
