"""Task-driven CAN selection and exclusive packet SRAM ownership."""
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
import unittest

from tools.configure import can, configure as gate, ir, selected_contracts
from tools.configure.providers.common import mode_traits
from tools.configure.resource_graph import resource_claims


class CanPollContracts(unittest.TestCase):
    def test_fixed_polling_uses_no_irq_or_authored_filter_domain(self):
        traits = mode_traits("can", "classic-poll")
        self.assertFalse(traits.irq)
        self.assertFalse(traits.dma)
        provider = SimpleNamespace(CAN_INTEGRATION=None)
        can.validate_soc({"controllers": {"BUS_A": {
            "kind": "can", "modes": ["classic-poll"], "irq": []}}}, provider)
        selected = {"id": "test", "kind": "can", "mode": "classic-poll"}
        can.validate_selection(selected, provider, 4096)
        self.assertEqual(ir.mode_options(selected), ir.NoOptions())
        hardware = SimpleNamespace(MODEL=False, CLASSIC_CAN_MAILBOXES=1, PUBLIC_FORMATS={"can"})
        traits = selected_contracts._format(
            SimpleNamespace(kind="can", mode="classic-poll", options=ir.NoOptions()),
            hardware, traits)
        self.assertEqual(traits["timestamp_source"], "service_observation")
        self.assertEqual(traits["fields"]["hardware_tx_mailboxes"], 1)
        self.assertIsNone(can.resolve_domain(
            None, [selected], {}, provider, None, {}))
        with self.assertRaisesRegex(ValueError, "selected CAN controller"):
            can.resolve_domain({"split": 14, "filters": []},
                               [selected], {}, provider, None, {})

    def test_polling_cannot_author_options_which_the_c_owner_does_not_consume(self):
        self.assertIsNone(can.validate_selection(
            {"mode": "classic-poll"}, None, 4096))
        for option, value in (("bitrate", 500000), ("operation", "silent-loopback"),
                              ("event_capacity", 1), ("irq_priority", 5),
                              ("calls_os", False), ("fifo_order", False)):
            with self.subTest(option=option), self.assertRaisesRegex(ValueError, option):
                can.validate_selection({"mode": "classic-poll", option: value},
                                       None, 4096)

    def test_fixed_polling_fact_cannot_publish_irq_bindings(self):
        soc = {"controllers": {"BUS_A": {
            "kind": "can", "modes": ["classic-poll"], "irq": ["CAN_A"]}}}
        with self.assertRaisesRegex(ValueError, "polling.*IRQ"):
            can.validate_soc(soc, SimpleNamespace(CAN_INTEGRATION=None))

    def test_packet_memory_owners_conflict_without_irq_or_pin_overlap(self):
        resource = "memory:usb-can-pma"
        logical = (("can", "classic-poll", "can1-silent-loopback"),
                   ("usb", "device", "usb-fs-pa11-pa12"))
        for owners in (logical, tuple(reversed(logical))):
            with self.subTest(order=owners):
                claims, _, claim = resource_claims(SimpleNamespace())
                claim(resource, *owners[0])
                self.assertEqual(claims[resource]["owner"], owners[0][0])
                with self.assertRaisesRegex(ValueError,
                        f"Resource conflict {resource}: {owners[0][0]} .*"
                        f"versus {owners[1][0]}") as rejected:
                    claim(resource, *owners[1])
                details = rejected.exception.details
                self.assertEqual(details["code"], "resource_conflict")
                self.assertEqual(details["resource"], resource)
                self.assertEqual([row["owner"] for row in details["claims"]],
                                 [owner[0] for owner in owners])


class CanExternalAssembly(unittest.TestCase):
    def test_only_explicit_external_topology_and_clock_can_enable_normal_mode(self):
        fixtures = gate.ROOT / "tests/integration/assemblies"
        normal = fixtures / "stm32f1_can_normal"
        source = (normal / "rb.toml").read_text(encoding="utf-8")
        expected = [
            (source.replace('sn65hvd230dr-3v3-rs-pc2', 'none'),
             "Board hardware requirement for can_transceiver"),
            (source.replace('hse8-mco-pll72', 'hsi8'),
             "requires clock profile hse8-mco-pll72"),
        ]
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            assembly = work / "assembly.toml"
            shutil.copytree(normal / "board", work / "board")
            for contents, diagnostic in expected:
                assembly.write_text(contents, encoding="utf-8")
                with self.subTest(diagnostic=diagnostic):
                    with self.assertRaisesRegex(gate.ConfigurationError, diagnostic):
                        gate.resolve_ir(assembly)
            assembly.write_text(source, encoding="utf-8")
            compiled = gate.resolve_ir(assembly).to_dict()
            resources = {item["key"] for item in compiled["resource_plan"]["claims"]}
            self.assertLessEqual(
                {"pin:PA11", "pin:PA12", "pin:PC2", "memory:usb-can-pma"}, resources)

            wrong_board = fixtures / "stm32f1_can/board/board.json"
            shutil.copyfile(wrong_board, work / "board/board.json")
            assembly.write_text(
                source.replace('[hardware]\ncan_transceiver = "sn65hvd230dr-3v3-rs-pc2"\n',
                               '').replace('hse8-mco-pll72', 'hsi8'),
                encoding="utf-8")
            with self.assertRaisesRegex(gate.ConfigurationError, "Unknown Board binding"):
                gate.resolve_ir(assembly)
