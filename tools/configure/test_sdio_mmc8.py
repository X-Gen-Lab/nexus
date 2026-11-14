"""MMC8 owns ten exact silicon pads without widening SD or SDIO functions."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure
from tools.configure.tests.f4_fixture import provider as f4_provider
from tools.configure.tests.native_fixture import native_fixture


class MmcEightBitConfigurationTests(unittest.TestCase):
    def fixture(self, directory, *, mode="sdio-poll", protocol="mmc", route=None):
        board = configure.load(configure.ROOT /
                               "boards/stm32f407zg_qiming_v41/board.json")
        board["id"] = "f407-mmc8-software-reference"
        board["provenance"] = []
        board["unknowns"] = ["Ten silicon pads only; no physical MMC device"]
        board["bindings"] = [{"id": "mmc", "controller": "SDIO",
                              "route": route or "sdio-mmc8-pc8-pd2"}]
        (directory / "board.json").write_text(json.dumps(board))
        source = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                  'clock="hse8-pll168"\noptimization="Os"\n'
                  '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                  'static_ram_limit_bytes=32768\nflash_load_limit_bytes=65536\n'
                  f'[block.sd0]\nbinding="mmc"\nmode="{mode}"\n'
                  f'protocol="{protocol}"\nbus_width=8\nmax_blocks=8\n'
                  'poll_budget=50000\nstop_timeout_us=25000\n')
        if mode != "sdio-poll":
            source += '[block.sd0.irq]\npriority=6\n'
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return assembly

    def resolve(self, directory, **options):
        path = self.fixture(directory, **options)
        return configure.resolve_ir(path, test_support=native_fixture(path))

    def test_mmc_width_eight_is_a_valid_explicit_protocol_selection(self):
        provider = f4_provider("STM32F407ZGT6")
        routes = configure.load(provider.package.fact("routes"))["routes"]
        route = next(row for row in routes if row["id"] == "sdio-mmc8-pc8-pd2")
        provider.validate_selection({"mode": "sdio-poll", "protocol": "mmc", "bus_width": 8,
            "max_blocks": 8, "poll_budget": 1000, "stop_timeout_us": 10000}, route)

    def test_all_engines_use_precise_ten_pin_static_storage(self):
        expected = ["PC8", "PC9", "PC10", "PC11", "PC12", "PD2",
                    "PB8", "PB9", "PC6", "PC7"]
        for mode in ("sdio-poll", "sdio-irq", "sdio-dma"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), mode=mode)
                item, = result.controllers
                self.assertEqual(8, item.options.bus_width)
                self.assertEqual(expected, [pin["pin"] for pin in item.pins])
                self.assertTrue({"pin:" + pin for pin in expected}.issubset(
                    {claim.key for claim in item.integration.resources}))
                source = bindings.emit(result)[1]
                self.assertIn("s_nx_sdio_pin_state_sd0[10]", source)
                self.assertIn(".bus_width = 8u", source)
                self.assertIn("NX_STM32_SDIO_MMC", source)
                self.assertNotIn("nx_block_port_activate(", source)

    def test_sd_and_sdio_never_inherit_mmc_width(self):
        for protocol in ("sd-memory", "sdio"):
            with self.subTest(protocol=protocol), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaisesRegex(ValueError, "(?i)(mmc|width)"):
                    self.resolve(Path(tmp), protocol=protocol)

    def test_legacy_six_pin_binding_cannot_select_eight_bit_mmc(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "(?i)(pad|pin|route|width)"):
                self.resolve(Path(tmp), route="sdio-pc8-pd2")


if __name__ == "__main__":
    unittest.main()
