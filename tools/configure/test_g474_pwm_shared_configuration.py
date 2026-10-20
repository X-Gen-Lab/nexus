"""Two existing PWM faces require one explicit immutable physical assembly.

These tests exercise the normal resolver and generated lifecycle contract.
Whole generated C/provider programs and ARM/image costs require their separate
actual execution evidence; emitted text is not that runtime proof.
"""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure import authored, bindings, configure, factory, ir
from tools.configure import selected_contracts
from tools.configure.providers import maintained


MODE = "shared-fixed-pwm"
CANONICAL = "cmake/package/verification/io_evolution/g474-pwm-shared.toml"
SPI = ('\n[spi.bus]\nbinding="spi"\nmode="short-poll"\nmax_hz=8000000\n'
       '\n[spi_device.device]\ncontroller="bus"\ncs_binding="cs"\n')
SPI_BINDINGS = (
    {"id": "spi", "controller": "SPI1", "route": "spi1-pa5-pa6-pa7"},
    {"id": "cs", "controller": "GPIOA", "route": "gpio-pa4",
     "mask": 16, "initial": 16},
)
ENCODER = ('\n[encoder.position]\nbinding="position"\nmode="quadrature"\n'
           'counting="x4-ti12"\nfilter_a=0\nfilter_b=0\ninvert_a=false\n'
           'invert_b=false\nexclusive=true\n')


class G474SharedPwmConfiguration(unittest.TestCase):
    def resolve(self, *, order=("left", "right"), updates=None,
                additions=None, board_change=None, extra="", prepend=False,
                extra_bindings=(), capabilities=True):
        fixture = configure.ROOT / CANONICAL
        contents = fixture.read_text()
        package = (fixture.parent / tomllib.loads(contents)["board_package"]).resolve()
        board = json.loads((package / "board.json").read_text())
        header, rest = contents.split("[pwm.left]", 1)
        first, second = rest.split("[pwm.right]", 1)
        blocks = {"left": "[pwm.left]" + first,
                  "right": "[pwm.right]" + second}
        for name, fields in (updates or {}).items():
            for key, value in fields.items():
                pattern = r"(?m)^" + re.escape(key) + r" = .*?$"
                blocks[name], count = re.subn(pattern, key + " = " +
                                              json.dumps(value), blocks[name])
                self.assertEqual(count, 1)
        for name, fields in (additions or {}).items():
            blocks[name] = blocks[name].rstrip() + "\n" + "\n".join(
                key + " = " + json.dumps(value) for key, value in fields.items()) + "\n"
        if not order:
            board["bindings"] = []
        board["bindings"].extend(dict(item) for item in extra_bindings)
        if board_change is not None:
            board_change(board)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            package = directory / "board"
            package.mkdir()
            (package / "board.json").write_text(json.dumps(board))
            previous = tomllib.loads(contents)["board_package"]
            header = header.replace('board_package = "' + previous + '"',
                                    'board_package = "' + str(package) + '"')
            if not capabilities:
                header = header.replace("capabilities = true", "capabilities = false")
            sections = "\n".join(blocks[name].strip() for name in order)
            path = directory / "assembly.toml"
            path.write_text(header + (extra + "\n" + sections if prepend
                                      else sections + "\n" + extra))
            return configure.resolve_ir(path)

    def normal(self, **kwargs):
        try:
            return self.resolve(**kwargs)
        except ValueError as error:
            self.fail("Exact shared TIM3 pair was rejected: " + str(error))

    def owners(self, result):
        self.assertTrue(hasattr(result, "pwm_shared_owners"),
                        "The validated pair needs an explicit typed physical owner")
        self.assertIsInstance(result.pwm_shared_owners, tuple)
        return result.pwm_shared_owners

    def test_exact_pair_derives_one_immutable_owner_and_two_public_decisions(self):
        result = self.normal()
        self.assertEqual([(item.id, item.kind, item.controller, item.mode)
                          for item in result.controllers],
                         [("left", "pwm", "TIM3", MODE),
                          ("right", "pwm", "TIM3", MODE)])
        self.assertTrue(all(isinstance(item.options, ir.PwmOptions)
                            for item in result.controllers))
        owner, = self.owners(result)
        self.assertIsInstance(owner, ir.PwmSharedOwnerIR)
        self.assertEqual((owner.provider, owner.controller, owner.mode,
                          owner.kernel_hz, owner.tick_hz, owner.period_ticks),
                         ("stm32g474", "TIM3", MODE, 170000000, 1000000, 1000))
        self.assertEqual([(member.id, member.channel, member.pin,
                           member.duty_ticks, member.inactive)
                          for member in owner.members],
                         [("left", 1, "PA6", 250, 0),
                          ("right", 2, "PA7", 750, 128)])
        with self.assertRaises(FrozenInstanceError):
            owner.period_ticks = 2000
        with self.assertRaises(FrozenInstanceError):
            owner.members[1].channel = 1
        self.assertFalse(result["physical_qualified"])

    def test_declaration_order_does_not_select_a_different_physical_owner(self):
        forward = self.normal()
        reverse = self.normal(order=("right", "left"))
        self.assertEqual(self.owners(forward), self.owners(reverse))
        self.assertEqual(tuple(item.id for item in reverse.controllers),
                         ("right", "left"))

    def test_period_mismatch_cannot_be_repaired_by_either_member(self):
        self.normal()
        for order in (("left", "right"), ("right", "left")):
            with self.subTest(order=order), self.assertRaisesRegex(
                    ValueError, "base|period|shared|compatible"):
                self.resolve(order=order, updates={"right": {"period_ticks": 2000}})

    def test_tick_mismatch_and_nonintegral_dividers_are_rejected(self):
        self.normal()
        for value in (2000000, 999999, 0, True):
            with self.subTest(tick=value), self.assertRaises(ValueError):
                self.resolve(updates={"right": {"tick_hz": value}})

    def test_duplicate_real_channel_cannot_hide_behind_a_binding_alias(self):
        self.normal()
        def duplicate(board):
            right, = [item for item in board["bindings"]
                      if item["id"] == "right-output"]
            right.update(route="tim3-ch1-pa6", mask=64, initial=0)
        with self.assertRaisesRegex(ValueError, "conflict|channel|pair|TIM3|PA6"):
            self.resolve(board_change=duplicate)

    def test_one_missing_member_does_not_take_an_unauthored_second_pad(self):
        self.normal()
        for name in ("left", "right"):
            with self.subTest(member=name), self.assertRaisesRegex(
                    ValueError, "pair|two|member|channel"):
                self.resolve(order=(name,))

    def test_exclusive_timer_owner_conflicts_in_both_input_orders(self):
        self.normal()
        exclusive = ('\n[pwm.exclusive]\nbinding="left-output"\n'
                     'mode="fixed-pwm"\ntick_hz=1000000\n'
                     'period_ticks=1000\nduty_ticks=500\n')
        for before in (False, True):
            with self.subTest(before=before), self.assertRaisesRegex(
                    ValueError, "conflict|TIM3|exclusive|shared"):
                self.resolve(extra=exclusive, prepend=before)

    def test_whole_encoder_personality_conflicts_in_both_input_orders(self):
        control = self.normal(order=(), extra=ENCODER, extra_bindings=(
            {"id": "position", "controller": "TIM3", "route": "tim3-encoder-pa6-pa7"},))
        self.assertEqual(control.controllers[0].kind, "encoder")
        self.normal()
        for before in (False, True):
            with self.subTest(before=before), self.assertRaisesRegex(
                    ValueError, "conflict|TIM3|personality|PA6|PA7"):
                self.resolve(extra=ENCODER, prepend=before, extra_bindings=(
                    {"id": "position", "controller": "TIM3", "route": "tim3-encoder-pa6-pa7"},))

    def test_spi_collision_uses_a_complete_valid_bus_endpoint_and_cs(self):
        control = self.normal(order=(), extra=SPI, extra_bindings=SPI_BINDINGS)
        self.assertEqual(control.controllers[0].kind, "spi")
        self.assertEqual(len(control.devices), 1)
        self.normal()
        with self.assertRaisesRegex(ValueError, "conflict.*PA[67]|PA[67].*conflict"):
            self.resolve(extra=SPI, extra_bindings=SPI_BINDINGS)

    def test_schema_keeps_the_existing_pwm_fields_and_rejects_share_flags(self):
        fields = {"binding", "mode", "period_ticks", "duty_ticks", "tick_hz"}
        self.assertEqual(authored.INSTANCE_FIELDS["pwm"], fields)
        schema = authored.editor_schema()
        pwm = schema["properties"]["pwm"]["additionalProperties"]
        self.assertEqual(set(pwm["properties"]), fields)
        self.assertFalse(pwm["additionalProperties"])
        for key in ("shared", "share_owner", "pool", "owner"):
            with self.subTest(key=key), self.assertRaisesRegex(
                    ValueError, "pwm.left|unknown|fields|keys"):
                self.resolve(additions={"left": {key: True}})

    def test_new_mode_cannot_escape_its_exact_provider_or_channel_routes(self):
        result = self.normal()
        item = result.controllers[0]
        provider = maintained("stm32g474")
        selection = dict(item.values)
        route = {key: item[key] for key in ("kind", "controller", "pins")}
        provider.validate_selection(selection, route)
        for family in ("stm32f103", "stm32f407", "stm32h743", "gd32f470"):
            with self.subTest(family=family), self.assertRaises(ValueError):
                maintained(family).mode_implementation("pwm", MODE)
        invalid = [dict(route) for _ in range(3)]
        invalid[0]["controller"] = "TIM4"
        invalid[1]["pins"] = ({"pin": "PA8", "function": "ch3", "af": 2},)
        invalid[2]["pins"] = ({"pin": "PA6", "function": "ch1", "af": 3},)
        for wrong in invalid:
            with self.subTest(route=wrong), self.assertRaises(ValueError):
                provider.validate_selection(selection, wrong)

    def test_board_inactive_levels_and_masks_are_explicit_exact_pad_facts(self):
        self.normal()
        for key in ("initial", "mask"):
            def remove(board, key=key):
                board["bindings"][1].pop(key)
            with self.subTest(missing=key), self.assertRaises(ValueError):
                self.resolve(board_change=remove)
        for key, value in (("mask", 64), ("initial", 64), ("initial", 256)):
            def wrong(board, key=key, value=value):
                board["bindings"][1][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.resolve(board_change=wrong)

    def test_emission_constructs_once_but_keeps_two_original_cold_factory_faces(self):
        for order in (("left", "right"), ("right", "left")):
            for enabled in (False, True):
                with self.subTest(order=order, capabilities=enabled):
                    result = self.normal(order=order, capabilities=enabled)
                    header, getters = factory.emit(result)
                    self.assertIn("const nx_pwm_port_t* nx_factory_pwm(", header)
                    self.assertIn("NX_PWM_ID_LEFT", header)
                    self.assertIn("NX_PWM_ID_RIGHT", header)
                    self.assertIn("return nx_binding_left;", getters)
                    self.assertIn("return nx_binding_right;", getters)
                    self.assertNotIn("initialize(", getters)
                    self.assertNotIn("NX_PWM_SHARED_ID", header)
                    source = bindings.emit(result)[1]
                    owners = re.findall(r"static nx_stm32g474_pwm_shared_state_t (\w+);", source)
                    self.assertEqual(len(owners), 1)
                    refs = re.findall(r"static const nx_stm32g474_pwm_shared_channel_t (\w+)", source)
                    self.assertEqual(len(refs), 2)
                    for name, channel in (("left", 1), ("right", 2)):
                        self.assertRegex(source, re.escape(
                            "static const nx_stm32g474_pwm_shared_channel_t s_nx_channel_" + name) +
                            r"\s*=\s*\{\s*\.owner\s*=\s*&" + owners[0] +
                            r",\s*\.channel\s*=\s*" + str(channel) + r"u\s*\};")
                        self.assertRegex(source, re.escape(
                            "static const nx_pwm_port_t s_nx_face_" + name) +
                            r"\s*=\s*\{\s*&nx_stm32g474_pwm_shared_ops,\s*" +
                            r"\(void\*\)&s_nx_channel_" + name + r"\s*\};")
                    self.assertEqual(source.count("nx_stm32g474_pwm_shared_initialize("), 1)
                    self.assertEqual(source.count("nx_stm32g474_pwm_shared_close("), 1)
                    self.assertEqual(source.count("&nx_stm32g474_pwm_shared_ops"), 2)
                    self.assertNotIn("nx_stm32g474_pwm_state_t s_nx_port_left", source)
                    self.assertNotIn("nx_stm32g474_pwm_state_t s_nx_port_right", source)
                    self.assertNotIn("malloc(", source)

    def test_physical_private_symbols_do_not_collide_with_a_legal_public_id(self):
        ordinary = ('\n[pwm.pwm_shared_tim3]\nbinding="aux-output"\n'
                    'mode="fixed-pwm"\ntick_hz=1000000\n'
                    'period_ticks=1000\nduty_ticks=500\n')
        auxiliary = ({"id": "aux-output", "controller": "TIM4",
                      "route": "tim4-ch1-pb6", "mask": 64, "initial": 0},)
        for before in (False, True):
            for enabled in (False, True):
                with self.subTest(before=before, capabilities=enabled):
                    result = self.normal(extra=ordinary, prepend=before,
                                         extra_bindings=auxiliary,
                                         capabilities=enabled)
                    self.assertEqual(len(result.controllers), 3)
                    self.assertEqual(len(self.owners(result)), 1)
                    header, getters = factory.emit(result)
                    for identifier in ("LEFT", "RIGHT", "PWM_SHARED_TIM3"):
                        self.assertIn("NX_PWM_ID_" + identifier, header)
                    self.assertIn("return nx_binding_pwm_shared_tim3;", getters)
                    source = bindings.emit(result)[1]
                    states = re.findall(
                        r"static nx_stm32g474_pwm(?:_shared)?_state_t (\w+);",
                        source)
                    self.assertEqual(len(states), 2)
                    self.assertEqual(len(states), len(set(states)))
                    helpers = re.findall(
                        r"static nx_result_t (\w+)\(void\) \{", source)
                    self.assertEqual(len(helpers), len(set(helpers)))
                    self.assertEqual(source.count("nx_stm32g474_pwm_shared_initialize("), 1)
                    self.assertEqual(source.count("nx_stm32g474_pwm_shared_close("), 1)
                    self.assertEqual(source.count("nx_stm32g474_pwm_initialize("), 1)
                    self.assertEqual(source.count("&nx_stm32g474_pwm_shared_ops"), 2)
                    self.assertIn("s_nx_port_pwm_shared_tim3.registers = TIM4;", source)
                    if not before:
                        self.assertIn("NX_PLATFORM_PHASE_CONTROLLER, 2u);", source)

    def test_resource_plan_and_selected_contracts_describe_the_real_shared_base(self):
        result = self.normal()
        claims = result.resource_plan.claims
        bases = [claim for claim in claims if claim.key == "controller:TIM3/base"]
        self.assertEqual({claim.owner for claim in bases}, {"left", "right"})
        self.assertTrue(all(claim.policy == "compatible" for claim in bases))
        self.assertTrue(all(dict(claim.attributes) == {"tick_hz": 1000000,
                                                     "period_ticks": 1000}
                            for claim in bases))
        for channel, owner in ((1, "left"), (2, "right")):
            claim, = [claim for claim in claims
                      if claim.key == "controller:TIM3/channel/" + str(channel)]
            self.assertEqual((claim.owner, claim.policy), (owner, "exclusive"))
        self.assertFalse(any(claim.key == "controller:TIM3" for claim in claims))
        self.assertEqual(sum(claim.key == "irq:TIM3" for claim in claims), 1)
        self.assertEqual(sum(claim.key == "clock:TIM3" for claim in claims), 1)
        self.assertEqual(sum(claim.key == "reset:TIM3" for claim in claims), 1)
        self.assertEqual(result.dma_topology.lanes, ())
        contracts = selected_contracts.for_configuration(result)
        self.assertEqual(len(contracts), 2)
        self.assertTrue(all(item.public_class == "pwm" and item.mode == MODE
                            for item in contracts))
        source = bindings.emit(result)[1]
        self.assertIn('#include "stm32g474_pwm_shared.h"', source)
        self.assertNotIn("void TIM3_IRQHandler", source)
        self.assertNotIn("DMA1_Channel", source)

    def test_existing_exclusive_selection_emits_no_optional_owner_or_refs(self):
        result = configure.resolve_ir(configure.ROOT / "tests/contracts/g474_assembly/pwm.toml")
        self.assertEqual(self.owners(result), ())
        self.assertNotIn("pwm_shared_owners", result.to_dict())
        source = bindings.emit(result)[1]
        self.assertNotIn("pwm_shared", source)
        self.assertIn("nx_stm32g474_pwm_initialize(", source)
        self.assertIn("&nx_stm32g474_pwm_ops", source)


if __name__ == "__main__":
    unittest.main()
