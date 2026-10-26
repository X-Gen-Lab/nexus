"""Real Device/Core compilation and include-origin admission contracts."""

import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev.process import run
from tools.maintenance import cmsis_core_probe as probe


class CmsisCoreProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="nexus-cmsis6-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.output = Path(cls.temporary.name)
        cls.compiler = os.environ.get("NEXUS_ARM_GCC", "arm-none-eabi-gcc")
        cls.cxx = os.environ.get("NEXUS_ARM_GXX", "arm-none-eabi-g++")
        cls.result = probe.execute(ROOT, cls.output / "matrix", cls.compiler,
                                   cls.cxx)

    def test_real_device_core_matrix_closes_c_and_cpp_and_security_contexts(
            self):
        self.assertEqual(self.result["status"], "passed")
        self.assertEqual(self.result["core_commit"], probe.CORE_COMMIT)
        self.assertEqual(len(self.result["probes"]), 16)
        self.assertFalse(self.result["software_qualified"])
        self.assertEqual(self.result["physical_status"], "not_executed")
        self.assertEqual({item["language"] for item in self.result["probes"]},
                         {"c11", "c++17"})
        self.assertEqual({item["family"] for item in self.result["probes"]},
                         {"stm32f103", "stm32f407", "stm32g474", "stm32h743",
                          "stm32u575", "gd32f303", "gd32f470"})
        secure = [item for item in self.result["probes"]
                  if item["family"] == "stm32u575"]
        self.assertEqual({item["security"] for item in secure},
                         {"secure", "nonsecure"})
        for item in self.result["probes"]:
            with self.subTest(profile=item["profile"],
                              language=item["language"]):
                self.assertGreater(item["object"]["bytes"], 0)
                names = {Path(record["path"]).name
                         for record in item["dependencies"]}
                self.assertIn("cmsis_version.h", names)
                self.assertIn("cmsis_gcc.h", names)
                self.assertIn("stdint.h", names)

    def test_make_dependencies_keep_system_headers_and_escaped_paths(self):
        text = ("cmsis_probe: /work/a\\ b.c \\\n"
                " /sdk/cmsis\\#version.h /tool/price$$.h\n")
        self.assertEqual(probe.dependencies(text),
                         [Path("/work/a b.c"), Path("/sdk/cmsis#version.h"),
                          Path("/tool/price$.h")])
        for malformed in ("other: /a.h\n", "cmsis_probe:\n",
                          "cmsis_probe: /a.h\nother: /b.h\n",
                          "cmsis_probe: relative.h\n"):
            with (self.subTest(malformed=malformed),
                  self.assertRaises(ValueError)):
                probe.dependencies(malformed)

    def test_system_marked_legacy_vendor_core_is_rejected_by_real_compiler(
            self):
        profile = next(item for item in probe.PROFILES
                       if item.family == "gd32f470")
        output = self.output / "legacy"
        output.mkdir()
        source = output / "probe.c"
        source.write_text(probe.source(profile), encoding="utf-8")
        argv = probe.compile_command(ROOT, profile, "c11", self.compiler,
                                     source, output)
        central = str(ROOT / probe.CORE_INCLUDE)
        legacy = ROOT / "vendors/gigadevice/gd32f4xx/Firmware/CMSIS"
        argv[argv.index(central)] = str(legacy)
        result = run(argv, cwd=ROOT, env=os.environ.copy(),
                     log=output / "legacy.raw", timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("CMSIS Core 6.4 required", result.stderr)

    def test_same_version_foreign_header_still_fails_actual_include_closure(
            self):
        profile = probe.PROFILES[0]
        output = self.output / "foreign"
        output.mkdir()
        source = output / "probe.c"
        source.write_text(probe.source(profile), encoding="utf-8")
        foreign = output / "cmsis_version.h"
        shutil.copyfile(ROOT / probe.CORE_INCLUDE / foreign.name, foreign)
        argv = probe.compile_command(ROOT, profile, "c11", self.compiler,
                                     source, output)
        argv.extend(["-include", str(foreign)])
        result = run(argv, cwd=ROOT, env=os.environ.copy(),
                     log=output / "foreign.raw", timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = probe.dependencies((output / "probe.d").read_text())
        self.assertIn(foreign, actual)
        with self.assertRaisesRegex(ValueError, "foreign CMSIS Core"):
            probe.check_dependencies(ROOT, profile, actual)

    def test_missing_required_core_header_or_output_reuse_cannot_pass(self):
        item = self.result["probes"][0]
        profile = probe.PROFILES[0]
        actual = [Path(record["path"]) for record in item["dependencies"]]
        actual = [path for path in actual if path.name != "cmsis_gcc.h"]
        with self.assertRaisesRegex(ValueError, "incomplete Device/Core"):
            probe.check_dependencies(ROOT, profile, actual)
        before = (self.output / "matrix/report.json").read_bytes()
        with self.assertRaises(ValueError):
            probe.execute(ROOT, self.output / "matrix", self.compiler, self.cxx)
        self.assertEqual((self.output / "matrix/report.json").read_bytes(),
                         before)
        self.assertEqual(json.loads(before)["status"], "passed")


if __name__ == "__main__":
    unittest.main()
