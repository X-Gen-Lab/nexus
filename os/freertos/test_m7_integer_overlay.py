"""Execute the owned integer-port overlay against locked kernel sources."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "nexus_m7_overlay", Path(__file__).with_name("prepare_m7_integer.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class IntegerPortOverlayTest(unittest.TestCase):
    """The immutable vendor tree is an input, never an output."""

    def test_pendsv_has_preserved_primask_errata_guard_without_vfp(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            MODULE.prepare(ROOT / "ext/freertos", output)
            source = (output / "port.c").read_text(encoding="utf-8")
            function = source.split(
                "void xPortPendSVHandler( void )\n{", 1
            )[1].split("\n}", 1)[0]
            self.assertIn("mrs r1, primask", function)
            self.assertIn("cpsid i", function)
            self.assertIn("msr primask, r1", function)
            self.assertLess(
                function.index("cpsid i"), function.index("msr basepri")
            )
            self.assertLess(function.index("msr basepri"), function.index("dsb"))
            self.assertLess(function.index("isb"), function.index("msr primask"))
            self.assertNotIn("vstm", function)
            self.assertNotIn("vldm", function)
            self.assertNotIn("cpsie i", function)

    def test_every_inline_basepri_raise_preserves_incoming_primask(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            MODULE.prepare(ROOT / "ext/freertos", output)
            header = (output / "portmacro.h").read_text(encoding="utf-8")
            for name in (
                "vPortRaiseBASEPRI", "ulPortRaiseBASEPRI", "vPortSetBASEPRI"
            ):
                match = re.search(
                    r"static (?:void|uint32_t) " + name + r"\(", header
                )
                self.assertIsNotNone(match)
                start = match.start()
                function = header[start : header.index("\n}", start)]
                self.assertIn("primask", function)
                self.assertIn("cpsid i", function)
                self.assertNotIn("cpsie i", function)

    def test_unknown_vendor_bytes_reject_before_writing_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "portable/GCC/ARM_CM3"
            source.mkdir(parents=True)
            for name in ("port.c", "portmacro.h"):
                original = ROOT / "ext/freertos/portable/GCC/ARM_CM3" / name
                (source / name).write_bytes(original.read_bytes())
            with (source / "port.c").open("ab") as stream:
                stream.write(b"\n/* Unreviewed vendor alteration. */\n")
            output = root / "derived"
            with self.assertRaisesRegex(ValueError, "source identity"):
                MODULE.prepare(root, output)
            self.assertFalse(output.exists())

    def test_rerun_is_deterministic_and_never_rewrites_vendor_inputs(self):
        original = {
            name: (ROOT / "ext/freertos/portable/GCC/ARM_CM3" / name).read_bytes()
            for name in ("port.c", "portmacro.h")
        }
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            MODULE.prepare(ROOT / "ext/freertos", output)
            first = {name: (output / name).read_bytes() for name in original}
            MODULE.prepare(ROOT / "ext/freertos", output)
            self.assertEqual(
                first, {name: (output / name).read_bytes() for name in original}
            )
        self.assertEqual(
            original,
            {
                name: (ROOT / "ext/freertos/portable/GCC/ARM_CM3" / name).read_bytes()
                for name in original
            },
        )


if __name__ == "__main__":
    unittest.main()
