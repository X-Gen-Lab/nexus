"""Compile exact GD470 SDK offsets as C11 and reject real macro drift.

These compiler contracts do not establish peripheral behavior or physical HIL.
The isolated production units must emit no firmware code or data. Negatives
override one actual SDK register macro after including its original header;
the vendor checkout and its locked identity remain untouched.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOC = ROOT / "soc/gd32/gd32f470"
SDK = ROOT / "vendors/gigadevice/gd32f4xx/Firmware"
PERIPHERAL = SDK / "GD32F4xx_standard_peripheral/Include"
REGISTERS = {
    "can": (
        "CAN_CTL", "CAN_STAT", "CAN_TSTAT", "CAN_RFIFO0", "CAN_RFIFO1",
        "CAN_INTEN", "CAN_ERR", "CAN_BT", "CAN_TMI", "CAN_TMP",
        "CAN_TMDATA1", "CAN_RFIFOMI", "CAN_RFIFOMDATA1", "CAN_FCTL",
        "CAN_FMCFG", "CAN_FSCFG", "CAN_FAFIFO", "CAN_FW", "CAN_FDATA0",
        "CAN_FDATA1"),
    "ethernet": (
        "ENET_MAC_CFG", "ENET_MAC_FRMF", "ENET_MAC_PHY_CTL",
        "ENET_MAC_PHY_DATA", "ENET_MAC_DBG", "ENET_MAC_INTMSK",
        "ENET_MAC_ADDR0H", "ENET_MAC_ADDR0L", "ENET_DMA_BCTL",
        "ENET_DMA_TPEN", "ENET_DMA_RPEN", "ENET_DMA_RDTADDR",
        "ENET_DMA_TDTADDR", "ENET_DMA_STAT", "ENET_DMA_CTL",
        "ENET_DMA_INTEN", "ENET_MAC_FCTL", "ENET_MAC_VLT",
        "ENET_PTP_TSCTL", "ENET_PTP_SSINC", "ENET_PTP_TSH",
        "ENET_PTP_TSL", "ENET_PTP_TSUH", "ENET_PTP_TSUL",
        "ENET_PTP_TSADDEND"),
}


class GD470RegisterLayoutTests(unittest.TestCase):
    """Real host/ARM compilers check the SDK, not a mirror register model."""

    def setUp(self):
        self.host = shutil.which("cc")
        self.arm = (os.environ.get("NEXUS_ARM_GCC") or
                    shutil.which("arm-none-eabi-gcc"))
        self.assertTrue(self.host, "actual host compiler required")
        self.assertTrue(self.arm, "actual pinned ARM compiler required")
        temporary = tempfile.TemporaryDirectory(prefix="gd470-layout-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def compile(self, kind, *, compiler=None, forced_header=None):
        source = SOC / "drivers" / (kind + "_layout.c")
        self.assertTrue(source.is_file(), "missing C11 contract: " + str(source))
        output = self.directory / (kind + ".o")
        command = [compiler or self.host, "-std=c11", "-pedantic-errors",
                   "-Wall", "-Wextra", "-Werror", "-DGD32F470",
                   "-DHXTAL_VALUE=25000000", "-I" + str(SOC / "private")]
        if compiler == self.arm:
            command += ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
                        "-mfloat-abi=hard", "-isystem",
                        str(ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include")]
        else:
            command += ["-I" + str(ROOT / "tests/contracts/gd32_model")]
        for directory in (SDK / "CMSIS/GD/GD32F4xx/Include", PERIPHERAL):
            command += ["-isystem", str(directory)]
        if forced_header:
            command += ["-include", str(forced_header)]
        command += ["-c", str(source), "-o", str(output)]
        result = subprocess.run(command, capture_output=True, text=True,
                                check=False, timeout=30)
        return result, output

    def test_standard_c11_contracts_emit_no_arm_code_or_data(self):
        size = str(Path(self.arm).with_name("arm-none-eabi-size"))
        for kind in REGISTERS:
            for compiler in (self.host, self.arm):
                with self.subTest(kind=kind, compiler=compiler):
                    result, output = self.compile(kind, compiler=compiler)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    if compiler == self.arm:
                        measured = subprocess.run(
                            [size, "--format=berkeley", str(output)],
                            capture_output=True, text=True, check=False,
                            timeout=30)
                        self.assertEqual(measured.returncode, 0,
                                         measured.stderr)
                        self.assertEqual(measured.stdout.splitlines()[1]
                                         .split()[:3], ["0", "0", "0"])

    def reject_sdk_drift(self, kind, header):
        original = (PERIPHERAL / header).read_text()
        for register in REGISTERS[kind]:
            with self.subTest(register=register):
                match = re.search(r"^#define\s+(" + register +
                                  r"(?:\([^\n]*?\))?)\s+(.+?)\s*/\*",
                                  original, flags=re.MULTILINE)
                self.assertIsNotNone(match, "SDK macro missing: " + register)
                forced = self.directory / "drift.h"
                forced.write_text(
                    '#include "' + header + '"\n#undef ' + register +
                    "\n#define " + match[1] + " (" + match[2] + " + 4U)\n")
                result, _ = self.compile(kind, forced_header=forced)
                self.assertNotEqual(result.returncode, 0,
                                    "accepted actual SDK drift: " + register)
                self.assertIn("static assertion failed", result.stderr)
                self.assertIn(register, result.stderr)

    def test_every_can_register_offset_rejects_sdk_drift(self):
        self.reject_sdk_drift("can", "gd32f4xx_can.h")

    def test_every_ethernet_register_offset_rejects_sdk_drift(self):
        self.reject_sdk_drift("ethernet", "gd32f4xx_enet.h")


if __name__ == "__main__":
    unittest.main()
