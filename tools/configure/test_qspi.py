"""One complete exact endpoint contract; no generated hardware algorithms."""
from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory
from tools.configure.stm32f4 import qspi


def parameters():
    return {"part": "W25Q128JV", "prescaler": 1, "cs_high_cycles": 2,
            "read_budget_us": 1000, "prepare_budget_us": 1000,
            "stop_budget_us": 1000}


class QspiAdmission(unittest.TestCase):
    def resolve(self, directory, *, extra="", part="STM32F479NIH6"):
        base = configure.ROOT / "tests/contracts/stm32f4_family"
        board = json.loads((base / "f479ni-board/board.json").read_text())
        board.update(id="qspi-software-reference", soc=part, bindings=[
            dict(id="control", controller="QUADSPI", route="quadspi-indirect-control"),
            dict(id="flash1", controller="QUADSPI", route="quadspi-bank1-sdr111")])
        (directory / "board").mkdir(exist_ok=True)
        (directory / "board/board.json").write_text(json.dumps(board))
        text = (base / "f479ni.toml").read_text().split("[crc.signature]")[0]
        text = text.replace('board_package = "f479ni-board"', 'board_package = "board"')
        text += ('[external_bus.serial]\nbinding="control"\nmode="qspi-indirect-sdr"\n'
                 '[flash_async.storage]\ncontroller="serial"\nbinding="flash1"\n'
                 'mode="nor-indirect"\nparameters={part="W25Q128JV",prescaler=1,'
                 'cs_high_cycles=2,read_budget_us=1000,prepare_budget_us=1000,'
                 'stop_budget_us=1000}\n')
        path = directory / "assembly.toml"
        path.write_text(text + extra)
        return configure.resolve_ir(path)

    def test_full_resolver_connects_typed_serial_endpoint_and_three_static_faces(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary))
            controller, = result.controllers
            endpoint, = result.devices
            self.assertEqual("QspiFlashOptions", type(endpoint.options).__name__)
            self.assertEqual("W25Q128JV", endpoint.options.parameters.part)
            self.assertEqual({"PB6", "PF8", "PF9", "PF10"},
                             {pin["pin"] for pin in controller.pins})
            self.assertEqual((), controller.dma)
            self.assertFalse(result.physical_qualified)
            source = bindings.emit(result)[1]
            for value in ("nx_stm32_qspi_admit", "nx_stm32_qspi_start",
                          "nx_stm32_qspi_stop", "nx_stm32_qspi_bus_ops",
                          "nx_stm32_qspi_async_ops", "nx_stm32_qspi_flash_ops"):
                self.assertIn(value, source)
            self.assertNotIn("stm32f407_fsmc", source)
            self.assertNotIn("HAL_QSPI_", source)
            self.assertIn("nx_factory_flash_async_flash_ref", factory.emit_strong(result))
            self.assert_static_construction(qspi.constructor(controller, result))

    def test_serial_controller_rejects_a_second_endpoint_and_unreviewed_part(self):
        duplicate = ('[flash_async.peer]\ncontroller="serial"\nbinding="flash1"\n'
                     'mode="nor-indirect"\nparameters={part="W25Q128JV",prescaler=1,'
                     'cs_high_cycles=2,read_budget_us=1000,prepare_budget_us=1000,'
                     'stop_budget_us=1000}\n')
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                self.resolve(Path(temporary), extra=duplicate)
            with self.assertRaises(ValueError):
                self.resolve(Path(temporary), part="STM32F407ZGT6")

    def admit(self, value, hz=16000000):
        try:
            qspi.validate(value, hclk_hz=hz)
        except ValueError as error:
            self.fail(f"Complete exact serial endpoint was rejected: {error}")

    def test_explicit_device_and_bounds_are_admitted_without_mutation(self):
        value = parameters()
        before = deepcopy(value)
        self.admit(value)
        self.assertEqual(before, value)

    def test_unknown_fields_devices_and_nonfinite_budgets_never_fall_back(self):
        mutations = [
            lambda p: p.update(part="W25Q64JV"),
            lambda p: p.update(unknown=True),
            lambda p: p.pop("prescaler"),
            lambda p: p.update(prescaler=True),
            lambda p: p.update(prescaler=256),
            lambda p: p.update(cs_high_cycles=0),
            lambda p: p.update(read_budget_us=0),
            lambda p: p.update(prepare_budget_us=0x100000000),
            lambda p: p.update(stop_budget_us=1000001),
        ]
        for mutation in mutations:
            value = parameters()
            mutation(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                qspi.validate(value, hclk_hz=16000000)

    def test_serial_clock_and_ncs_high_time_preserve_exact_rational_limits(self):
        value = parameters()
        value.update(prescaler=3, cs_high_cycles=2)
        with self.assertRaises(ValueError):
            qspi.validate(value, hclk_hz=180000000)
        value["cs_high_cycles"] = 3
        self.admit(value, 180000000)
        value["prescaler"] = 2
        with self.assertRaises(ValueError):
            qspi.validate(value, hclk_hz=180000000)
        self.admit(value, 150000000)

    def assert_static_construction(self, made):
        self.assertEqual("nx_stm32_qspi_t", made.state_type)
        self.assertEqual("nx_stm32_qspi_admit", made.admission[0].function)
        self.assertEqual("nx_stm32_qspi_start", made.initialize.function)
        self.assertEqual(2, len(made.initialize.arguments))
        self.assertEqual("nx_stm32_qspi_stop", made.stop[0].function)
        self.assertEqual(4, len(made.pin_snapshots))
        source = "\n".join(made.definitions)
        self.assertIn("nx_stm32_qspi_async_ops, &s_nx_port_serial", source)
        self.assertIn("nx_stm32_qspi_flash_ops, &s_nx_port_serial", source)
        self.assertIn(".system = &g_nx_stm32_system", source)
        for forbidden in ("clock_read", "nx_deadline", "while (", "WRITE_REG",
                          "IRQHandler"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
