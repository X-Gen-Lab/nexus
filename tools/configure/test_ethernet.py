"""Selected RMII MAC and paired MDIO faces preserve exact cold ownership."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class EthernetConfigurationTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def assembly(self, label="stm32-zg", change=None):
        base = gate.ROOT / "tests/contracts/ethernet_assembly"
        content = (base / (label + ".toml")).read_text().replace(
            f'board_package="{label}-board"',
            'board_package=' + json.dumps(str(base / (label + "-board"))))
        path = self.root / "assembly.toml"
        path.write_text(change(content) if change else content)
        return path

    def test_two_exact_zg_parts_have_one_mac_and_no_stream_dma_claim(self):
        for label in ("stm32-zg", "gd32"):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 1)
                self.assertEqual(result.controllers[0].options.address,
                                 (2, 0, 0, 0, 0, 1))
                with self.assertRaises(FrozenInstanceError):
                    result.controllers[0].options.rx_count = 3
                claims = {item.key for item in result.claims}
                self.assertIn("irq:ENET" if label == "gd32" else "irq:ETH", claims)
                self.assertIn("pin:PG14", claims)
                self.assertIn("clock:ETHMAC", claims)
                self.assertIn("controller:SYSCFG_RMII", claims)
                self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_exact_chosen_storage_and_paired_readonly_faces_share_one_context(self):
        output = self.root / "generated"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_eth(", header)
        self.assertIn("nx_factory_mdio(", header)
        self.assertIn('#include "nexus/io/mdio.h"', (output / "nexus_bindings.h").read_text())
        self.assertIn("static nx_eth_descriptor_t s_nx_eth_tx_mac[2]", source)
        self.assertIn("static nx_eth_descriptor_t s_nx_eth_rx_mac[2]", source)
        self.assertIn("uint8_t s_nx_eth_storage_mac[2][1536]", source)
        self.assertIn("&nx_eth_mac_ops, &s_nx_port_mac", source)
        self.assertIn("&nx_eth_mdio_ops, &s_nx_port_mac", source)
        self.assertEqual(source.count("static nx_eth_mac_t s_nx_port_mac;"), 1)
        self.assertNotIn("malloc", source)
        self.assertNotIn("dp83848", source.lower())

    def test_mac_address_requires_explicit_nonzero_unicast_six_bytes(self):
        for value in ("[1,0,0,0,0,1]", "[0,0,0,0,0,0]", "[2,0,0,0,0]",
                      "[2,0,0,0,0,256]", "[true,0,0,0,0,1]"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "Ethernet"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "[2,0,0,0,0,1]", value)))

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
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(before, after)))

    def test_failed_mac_cold_is_counted_before_prepare_and_pins_restore_after_stop(self):
        output = self.root / "generated"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        start = source.index("nx_platform_start_result_t nx_platform_start")
        prepare = source.index("result.primary = prepare_mac();", start)
        self.assertLess(source.index("s_nx_platform_initialized = 1u;", start), prepare)
        stop = source.index("static nx_result_t stop_mac(void)")
        self.assertLess(source.index("nx_eth_port_stop", stop),
                        source.index("nx_stm32_pin_restore", stop))
        self.assertIn("nx_stm32_pin_capture", source[stop:])

    def test_unreviewed_ve_package_cannot_use_zg_port_g_route(self):
        package = self.root / "board"
        package.mkdir()
        board = gate.load(gate.ROOT / "tests/contracts/ethernet_assembly/stm32-zg-board/board.json")
        board["soc"] = "STM32F407VET6"
        (package / "board.json").write_text(json.dumps(board))
        path = self.assembly()
        content = path.read_text()
        start = content.index("board_package=")
        end = content.index("\n", start)
        path.write_text(content[:start] + 'board_package="board"' + content[end:])
        with self.assertRaisesRegex(ValueError, "reviewed exact-part route"):
            gate.resolve_ir(path)

    def test_default_platform_has_no_mac_ring_phy_or_mdio_context(self):
        output = self.root / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output)
        source = (output / "bindings.c").read_text()
        self.assertNotIn("s_nx_eth_", source)
        self.assertNotIn("nx_eth_mac_ops", source)
        self.assertNotIn("nx_eth_mdio_ops", source)


if __name__ == "__main__":
    unittest.main()
