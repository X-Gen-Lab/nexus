"""Selected RMII MAC and paired MDIO faces preserve exact cold ownership."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.generation import bindings


F107 = gate.ROOT / "tests/integration/assemblies/stm32f1_eth_mii"


class F107EthernetConfigurationTests(unittest.TestCase):
    def test_raw_modes_use_exact_pins_and_distinct_media_select(self):
        cases = (("f107vc.toml", "raw-mii", 17, 0, "true"),
                 ("f107vc-rmii.toml", "raw-rmii", 9, 1, "false"))
        for filename, mode, count, interface, mii in cases:
            with self.subTest(mode=mode):
                ir = gate.resolve_ir(F107 / filename)
                controller, = ir.controllers
                self.assertEqual((controller.controller, controller.mode),
                                 ("ETH_MAC", mode))
                self.assertEqual(len(controller.pins), count)
                if mode == "raw-rmii":
                    self.assertEqual({(pin["pin"], pin["function"])
                                      for pin in controller.pins},
                                     {("PC1", "mdc"), ("PA2", "mdio"),
                                      ("PA1", "ref-clk"), ("PD8", "crs-dv"),
                                      ("PD9", "rxd0"), ("PD10", "rxd1"),
                                      ("PB11", "tx-en"), ("PB12", "txd0"),
                                      ("PB13", "txd1")})
                claims = {claim.key: dict(claim.attributes)
                          for claim in ir.claims}
                self.assertEqual(claims["register:AFIO_MAPR/ETH_REMAP"]["value"], 1)
                self.assertEqual(claims["register:AFIO_MAPR/MII_RMII_SEL"]["value"],
                                 interface)
                self.assertIn("clock:DMA2", claims)
                self.assertIn("irq:ETH", claims)
                self.assertIn(".mii = " + mii, bindings.emit(ir)[1])
                self.assertFalse(ir.physical_qualified)

    def test_rmii_requires_board_route_explicit_clock_wiring_and_exclusive_pads(self):
        source = (F107 / "f107vc-rmii.toml").read_text().replace(
            'board_package = "board-rmii"',
            'board_package = ' + json.dumps(str(F107 / "board-rmii")))
        cases = (('"mb784-rmii-u2"', '"unknown"', "requires"),
                 ('mode = "raw-rmii"', 'mode = "raw-mii"', "route|mode"),
                 ('clock = "hse25-crystal-pll72"', 'clock = "hsi8"', "clock"))
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            for original, replacement, message in cases:
                with self.subTest(message=message):
                    path.write_text(source.replace(original, replacement))
                    with self.assertRaisesRegex(ValueError, message):
                        gate.resolve_ir(path)
            board = gate.load(F107 / "board-rmii/board.json")
            board["reserved_resources"].append("PD8")
            (path.parent / "board.json").write_text(json.dumps(board))
            path.write_text(source.replace(json.dumps(str(F107 / "board-rmii")),
                                           '"."'))
            with self.assertRaisesRegex(ValueError, "PD8"):
                gate.resolve_ir(path)


class EthernetConfigurationTests(unittest.TestCase):
    def test_mii_exact_ve_zg_route_is_complete_without_fitted_phy_claim(self):
        for part in ('STM32F407VET6', 'STM32F407ZGT6'):
            with self.subTest(part=part):
                board = gate.load(gate.ROOT /
                    'tests/integration/assemblies/ethernet_assembly/stm32-zg-board/board.json')
                board['soc'] = part
                board['bindings'][0]['route'] = 'mii-a-b-c'
                (self.root / 'board.json').write_text(json.dumps(board))
                path = self.assembly(change=lambda text: text.replace(
                    'mode="raw-rmii"', 'mode="raw-mii"'))
                lines = path.read_text().splitlines()
                lines = ['board_package="."' if line.startswith('board_package=')
                         else line for line in lines]
                path.write_text('\n'.join(lines) + '\n')
                result = gate.resolve_ir(path)
                item, = result.controllers
                self.assertEqual(item.mode, 'raw-mii')
                self.assertEqual(len(item.pins), 17)
                wires = {pin['pin'] for pin in item.pins}
                self.assertIn('PB8', wires)
                self.assertIn('PC3', wires)
                self.assertFalse(any(pin.startswith('PG') for pin in wires))
                source = bindings.emit(result)[1]
                self.assertIn('.mii = true', source)
                self.assertFalse(result.physical_qualified)

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def assembly(self, label="stm32-zg", change=None):
        base = gate.ROOT / "tests/integration/assemblies/ethernet_assembly"
        content = (base / (label + ".toml")).read_text().replace(
            f'board_package="{label}-board"',
            'board_package=' + json.dumps(str(base / (label + "-board"))))
        path = self.root / "assembly.toml"
        path.write_text(change(content) if change else content)
        return path

    def test_two_exact_zg_parts_have_one_mac_and_no_stream_dma_claim(self):
        for label in ('stm32-zg',):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 1)
                self.assertEqual(result.controllers[0].options.address,
                                 (2, 0, 0, 0, 0, 1))
                with self.assertRaises(FrozenInstanceError):
                    result.controllers[0].options.rx_count = 3
                claims = {item.key for item in result.claims}
                self.assertIn("irq:ETH", claims)
                self.assertIn("pin:PG14", claims)
                self.assertIn("clock:ETHMAC", claims)
                self.assertIn("controller:SYSCFG_RMII", claims)
                self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_exact_chosen_storage_and_paired_readonly_faces_share_one_context(self):
        output = self.root / "generated"
        gate.configure(self.assembly(), output, test_support=native_fixture(self.assembly()))
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_eth(", header)
        self.assertIn("nx_factory_mdio(", header)
        self.assertIn('#include "nexus/io/mdio.h"', (output / "nexus_bindings.h").read_text())
        self.assertIn("static nx_eth_descriptor_t s_nx_eth_tx_mac[2]", source)
        self.assertIn("static nx_eth_descriptor_t s_nx_eth_rx_mac[2]", source)
        self.assertIn("uint8_t s_nx_eth_storage_mac[3072]", source)
        self.assertIn(".rx_storage_bytes = 1536u", source)
        self.assertIn("&nx_eth_mac_ops, &s_nx_port_mac", source)
        self.assertIn(".ops = &nx_eth_mdio_ops, .context = &s_nx_port_mac", source)
        self.assertEqual(source.count("static nx_eth_mac_t s_nx_port_mac;"), 1)
        self.assertNotIn("malloc", source)
        self.assertNotIn("dp83848", source.lower())

    def test_mac_address_requires_explicit_nonzero_unicast_six_bytes(self):
        for value in ("[1,0,0,0,0,1]", "[0,0,0,0,0,0]", "[2,0,0,0,0]",
                      "[2,0,0,0,0,256]", "[true,0,0,0,0,1]"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "Ethernet"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "[2,0,0,0,0,1]", value)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    "[2,0,0,0,0,1]", value))))

    def test_count_buffer_alignment_and_finite_budgets_reject_before_emission(self):
        for before, after in (("tx_count=2", "tx_count=0"),
                              ("rx_count=2", "rx_count=true"),
                              ("rx_count=2", "rx_count=9999999"),
                              ("rx_storage_bytes=1536", "rx_storage_bytes=1517"),
                              ("rx_storage_bytes=1536", "rx_storage_bytes=1537"),
                              ("rx_storage_bytes=1536", "rx_storage_bytes=8192"),
                              ("init_timeout_us=5000", "init_timeout_us=0"),
                              ("drain_timeout_us=1000", "drain_timeout_us=0")):
            with self.subTest(after=after), self.assertRaisesRegex(ValueError, "Ethernet"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(before, after)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(before, after))))

    def test_failed_mac_cold_retains_typed_prepare_and_close_responsibility(self):
        output = self.root / "generated"
        gate.configure(self.assembly(), output, test_support=native_fixture(self.assembly()))
        source = (output / "bindings.c").read_text()
        self.assertIn("(nx_lifecycle_acquisition_t){ "
                      "nx_stm32_eth_cold_prepare(&s_nx_port_mac, "
                      "&s_nx_eth_config_mac, &s_nx_eth_receipt_mac), true }",
                      source)
        self.assertIn("nx_stm32_eth_cold_close(", source)
        self.assertIn(".ethernet_close = nx_stm32_eth_resources_close,", source)
        self.assertIn("static const nx_stm32_eth_cold_config_t", source)
        self.assertNotIn("nx_stm32_eth_initialize(", source)
        self.assertNotIn("nx_stm32_pin_restore(", source)

    def test_unreviewed_ve_package_cannot_use_zg_port_g_route(self):
        package = self.root / "board"
        package.mkdir()
        board = gate.load(gate.ROOT / "tests/integration/assemblies/ethernet_assembly/stm32-zg-board/board.json")
        board["soc"] = "STM32F407VET6"
        (package / "board.json").write_text(json.dumps(board))
        path = self.assembly()
        content = path.read_text()
        start = content.index("board_package=")
        end = content.index("\n", start)
        path.write_text(content[:start] + 'board_package="board"' + content[end:])
        with self.assertRaisesRegex(ValueError, "reviewed exact-part route"):
            gate.resolve_ir(path, test_support=native_fixture(path))

    def test_default_platform_has_no_mac_ring_phy_or_mdio_context(self):
        output = self.root / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output, test_support=native_fixture(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml"))
        source = (output / "bindings.c").read_text()
        self.assertNotIn("s_nx_eth_", source)
        self.assertNotIn("nx_eth_mac_ops", source)
        self.assertNotIn("nx_eth_mdio_ops", source)


if __name__ == "__main__":
    unittest.main()
