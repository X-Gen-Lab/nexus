"""Two actual ARM images consume H747 AMP under an external-boot link context."""
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from contextlib import ExitStack

from tools.measurement.elf import Elf32


ROOT = Path(__file__).resolve().parents[2]


class H747AMPPairTests(unittest.TestCase):
    def test_independent_core_images_share_only_the_explicit_noload_window(self):
        environment = dict(os.environ)
        environment["PATH"] = (str(ROOT / ".venv/bin") + os.pathsep +
                               environment.get("PATH", ""))
        cmake = shutil.which("cmake", path=environment["PATH"])
        self.assertIsNotNone(cmake, "Prepared CMake is required")
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc",
                                         path=environment["PATH"]),
                             "Prepared ARM toolchain is required")
        images = {}
        report = {"kind": "external_paired_boot_link_only", "cores": {},
                  "physical_execution": "not_executed"}
        retained = environment.get("NEXUS_H747_PAIR_OUTPUT")
        with ExitStack() as stack:
            folder = (str(Path(retained).resolve()) if retained else
                      stack.enter_context(tempfile.TemporaryDirectory(
                          prefix="nexus H747 pair ")))
            Path(folder).mkdir(parents=True, exist_ok=True)
            for core in ("CM7", "CM4"):
                build = Path(folder) / core
                commands = [
                    [cmake, "-S", str(ROOT / "tests/contracts/h747_amp_pair"),
                     "-B", str(build), "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release",
                     "-DNEXUS_SOURCE_ROOT=" + str(ROOT),
                     "-DNEXUS_REFERENCE_CORE=" + core,
                     "-DCMAKE_TOOLCHAIN_FILE=" + str(
                         ROOT / "cmake/toolchains/arm-gcc.cmake")],
                    [cmake, "--build", str(build), "--target", "reference",
                     "--parallel", "1"],
                ]
                observations = []
                for index, command in enumerate(commands):
                    process = subprocess.run(command, env=environment,
                                             capture_output=True, text=True,
                                             timeout=180)
                    Path(folder, f"{core}-{index}.log").write_text(
                        process.stdout + process.stderr)
                    observations.append({"command": command,
                                         "exit_code": process.returncode})
                    Path(folder, f"{core}-commands.json").write_text(
                        json.dumps(observations, indent=2) + "\n")
                    self.assertEqual(process.returncode, 0,
                                     process.stdout + process.stderr)
                elf = Elf32((build / "reference.elf").read_bytes())
                images[core] = elf
                for symbol in ("nx_h747_amp_attach", "nx_h747_amp_construct",
                               "nx_h747_amp_hw_take", "nx_h747_amp_hw_ready",
                               "nx_amp_ipc_construct", "Reset_Handler"):
                    self.assertIn(symbol, elf.symbols)
                shared = next(section for section in elf.sections
                              if section["name"] == ".shared")
                self.assertEqual((shared["kind"], shared["address"],
                                  shared["size"]), (8, 0x38000000, 4096))
                self.assertTrue(elf.symbol("_ebss") < 0x38000000)
                self.assertFalse(any(load["physical"] <= 0x38000000 <
                                     load["physical"] + load["files"]
                                     for load in elf.loads))
                entries = json.loads(
                    (build / "compile_commands.json").read_text())
                for entry in entries:
                    command = entry["command"]
                    self.assertIn("CORE_" + core, command)
                    self.assertNotIn("CORE_" + ("CM4" if core == "CM7"
                                               else "CM7"), command)
                    self.assertNotIn("stm32h743/", entry["file"])
                self.assertIn("--no-gc-sections",
                              (build / "build.ninja").read_text())
                binary = build / "reference.bin"
                objcopy = shutil.which("arm-none-eabi-objcopy",
                                       path=environment["PATH"])
                self.assertIsNotNone(objcopy)
                copied = subprocess.run(
                    [objcopy, "-O", "binary", str(build / "reference.elf"),
                     str(binary)], env=environment, capture_output=True,
                    text=True, timeout=30)
                self.assertEqual(copied.returncode, 0,
                                 copied.stdout + copied.stderr)
                observations.append({"command": [objcopy, "-O", "binary",
                                                  str(build / "reference.elf"),
                                                  str(binary)],
                                     "exit_code": copied.returncode})
                Path(folder, f"{core}-commands.json").write_text(
                    json.dumps(observations, indent=2) + "\n")
                artifacts = []
                for path in [build / name for name in (
                        "reference.elf", "reference.bin", "reference.map",
                        "compile_commands.json", "memory.ld", "build.ninja")]:
                    artifacts.append({"path": str(path),
                                      "sha256": hashlib.sha256(
                                          path.read_bytes()).hexdigest(),
                                      "bytes": path.stat().st_size})
                sources = sorted({Path(entry["file"]).resolve()
                                  for entry in entries})
                report["cores"][core] = {
                    "commands": observations, "artifacts": artifacts,
                    "source_inputs": [{"path": str(path),
                                       "sha256": hashlib.sha256(
                                           path.read_bytes()).hexdigest()}
                                      for path in sources],
                    "execution": "not_executed_link_only"}
            self.assertTrue(0x08000000 <= images["CM7"].entry < 0x08020000)
            self.assertTrue(0x08100000 <= images["CM4"].entry < 0x08120000)
            for core, origin in (("CM7", 0x24000000), ("CM4", 0x30000000)):
                self.assertEqual(images[core].symbol("_estack"), origin + 32768)
            if retained:
                Path(folder, "report.json").write_text(
                    json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
