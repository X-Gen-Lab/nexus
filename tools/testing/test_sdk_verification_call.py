"""Every source-consuming CMake entry rechecks actual SDK bytes."""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import json
import os
import signal
import shlex
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import tomllib
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.dev import process as maintained_process

ACTUAL_PROCESS_RUN = maintained_process.run


class SDKVerificationCallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "prefix"
        self.source = self.prefix / "share/nexus/src"
        self.package = self.prefix / "lib/cmake/Nexus"
        self.package.mkdir(parents=True)
        (self.source / "cmake/package").mkdir(parents=True)
        (self.source / "cmake/platform").mkdir(parents=True)
        (self.source / ".nexus-source-sdk.json").write_text(json.dumps(
            {"source_revision": "fixture", "publishable": True}) + "\n")
        (self.source / "payload").write_text("approved")
        (self.source / "cmake/package/package_source_sdk.py").write_text(
            "import json,sys\nfrom pathlib import Path\n"
            "root=Path(sys.argv[2])\n"
            "count=root.parent/'count'\n"
            "count.write_text(str(int(count.read_text())+1) if count.exists() else '1')\n"
            "if (root/'payload').read_text() != 'approved':\n"
            "    sys.exit('actual payload changed')\n"
            "print(json.dumps({'source_revision':'fixture','publishable':True}))\n")
        shutil.copyfile(ROOT / "cmake/platform/SDK.cmake",
                        self.source / "cmake/platform/SDK.cmake")
        (self.source / "CMakeLists.txt").write_text(
            'set(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/SDK.cmake")\n'
            'add_library(nexus_config INTERFACE)\n'
            'add_library(Nexus::Config ALIAS nexus_config)\n'
            'set_target_properties(nexus_config PROPERTIES NEXUS_SOURCE_DIR '
            '"${CMAKE_CURRENT_SOURCE_DIR}")\n')
        template = (ROOT / "cmake/package/NexusConfig.cmake.in").read_text()
        (self.package / "NexusConfig.cmake").write_text(
            template.replace("@NEXUS_SDK_VERSION@", "1.0.0"))

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, extra=""):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(verify_call NONE)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'set(Nexus_DIR "{self.package}")\n'
            'find_package(Nexus CONFIG REQUIRED)\n' + extra)
        return subprocess.run([shutil.which("cmake"), "-S", str(self.root),
                               "-B", str(self.root / "build"), "-G", "Ninja"],
                              capture_output=True, text=True, timeout=30,
                              check=False)

    def test_package_and_provider_entries_each_verify(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "2")

    def test_repeated_find_and_later_sdk_include_each_verify_again(self):
        result = self.configure(
            'find_package(Nexus CONFIG REQUIRED)\n'
            f'set(NEXUS_SOURCE_DIR "{self.source}")\n'
            f'include("{self.source}/cmake/platform/SDK.cmake")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "4")

    def test_modified_payload_rejected_by_next_find_even_same_manifest(self):
        result = self.configure(
            f'file(WRITE "{self.source}/payload" "changed")\n'
            'find_package(Nexus CONFIG REQUIRED)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("actual payload changed", result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "3")


class SDKConsumerOutputAdmissionTests(unittest.TestCase):
    """Reject invalid roots before deleting SDK bytes or publishing reports."""

    def setUp(self):
        specification = importlib.util.spec_from_file_location(
            "nexus_consumer_output_admission",
            ROOT / "cmake/package/verify_consumers.py")
        self.consumer = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.consumer)

    def snapshot(self, root):
        return {path.relative_to(root).as_posix():
                ("symlink", str(path.readlink())) if path.is_symlink() else
                ("file", path.read_bytes())
                for path in root.rglob("*")
                if path.is_symlink() or path.is_file()}

    def fixture(self, root):
        prefix = root / "SDK prefix"
        sdk = prefix / "share/nexus/src"
        sdk.mkdir(parents=True)
        (sdk / "payload").write_bytes(b"preserve exact SDK bytes\x00\xff")
        (sdk / ".nexus-source-sdk.json").write_text(
            '{"source_revision":"fixture","publishable":true}\n')
        return prefix

    def reject_unchanged(self, root, prefix, output):
        before = self.snapshot(root)
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            result = self.consumer.main([
                "--prefix", str(prefix), "--output", str(output)])
        self.assertEqual(result, 1)
        self.assertEqual(self.snapshot(root), before,
                         "invalid output admission changed retained SDK bytes")
        self.assertIn("canonical and disjoint", errors.getvalue())

    def test_owned_ancestor_equal_and_nested_output_preserve_sdk(self):
        for relation in ("ancestor", "equal", "nested"):
            with self.subTest(relation=relation), tempfile.TemporaryDirectory(
                    prefix="SDK output containment ") as temporary:
                root = Path(temporary)
                prefix = self.fixture(root)
                output = {"ancestor": root, "equal": prefix,
                          "nested": prefix / "results"}[relation]
                output.mkdir(exist_ok=True)
                (output / ".nexus-sdk-consumer-check").write_text(
                    "owned SDK consumer verification\n")
                self.reject_unchanged(root, prefix, output)

    def test_output_alias_cannot_write_report_into_owned_ancestor(self):
        with tempfile.TemporaryDirectory(prefix="SDK output alias ") as temporary:
            root = Path(temporary)
            prefix = self.fixture(root)
            (root / ".nexus-sdk-consumer-check").write_text(
                "owned SDK consumer verification\n")
            output = root / "aliased output"
            output.symlink_to(root, target_is_directory=True)
            self.reject_unchanged(root, prefix, output)

    def test_prefix_alias_cannot_replace_a_preexisting_output_report(self):
        with tempfile.TemporaryDirectory(prefix="SDK prefix alias ") as temporary:
            root = Path(temporary)
            prefix = self.fixture(root)
            alias = root / "aliased SDK"
            alias.symlink_to(prefix, target_is_directory=True)
            output = root / "consumer results"
            output.mkdir()
            (output / ".nexus-sdk-consumer-check").write_text(
                "owned SDK consumer verification\n")
            (output / "verification.json").write_bytes(b"retained prior failure\n")
            self.reject_unchanged(root, alias, output)


class SDKConsumerParallelTests(unittest.TestCase):
    """Real CLI and templates; external CMake/SDK processes are test doubles."""

    SELECTED_INPUTS = (
        ("h743 high", "h743-high-peripherals", "h743-high"),
        ("u575 gpdma", "u575-uart-gpdma", "u575-uart-gpdma"),
        ("g474 fd normal", "g474-can-fd-normal", "g474-can-fd"),
        ("g474 fd loopback", "g474-can-fd-loopback", "g474-can-fd"),
        ("g474 pwm adc", "g474-pwm-adc", "g474-pwm-adc"),
        ("g474 adc regular software", "g474-adc-group-regular-software", "g474-adc-group"),
        ("g474 adc regular timer", "g474-adc-group-regular-timer", "g474-adc-group"),
        ("g474 adc injected software", "g474-adc-group-injected-software", "g474-adc-group"),
        ("g474 adc injected timer", "g474-adc-group-injected-timer", "g474-adc-group"),
        ("h743 fifo", "h743-uart-fifo", "uart-fifo"),
        ("u575 fifo", "u575-uart-fifo", "uart-fifo"),
        ("f407 frames", "f407zg-eth-frames", "ethernet-frames"),
        ("gd470 frames", "gd470-eth-frames", "ethernet-frames"),
        ("h743 rtscts", "h743-uart-rtscts", "uart-fifo"),
        ("u575 rtscts", "u575-uart-rtscts", "uart-fifo"),
        ("g474 shared pwm", "g474-pwm-shared", "g474-pwm-shared"),
        ("native time pair", "native-time-pair", "native-time-pair"),
        ("native time empty", "native-time-empty", "native-time-empty"),
        ("h743 runtime clock", "h743-spi-dma-runtime-clock", "h743-runtime-clock"),
        ("h743 spi dma", "h743-spi-dma", "h743-spi-dma"),
        ("h743 spi dma cache on", "h743-spi-dma-cache-on", "h743-spi-dma"),
        ("u575 uart chain", "u575-uart-chain", "u575-uart-gpdma"),
        ("h743 spi dma uart tx", "h743-spi-dma-uart-tx", "h743-spi-dma"),
        ("h743 spi dma uart rx", "h743-spi-dma-uart-rx", "h743-spi-dma"),
        ("g474 pin states", "g474-pin-states", "pin-states"),
        ("u575 lptim run", "u575-lptim-run", "u575-lptim"),
        ("u575 rtc run", "u575-rtc-run", "u575-rtc"),
        ("h743 uart continuous", "h743-uart-continuous", "h743-uart-continuous"),
        ("h743 uart continuous cache on", "h743-uart-continuous-cache-on", "h743-uart-continuous"),
        ("h743 uart duplex continuous", "h743-uart-duplex-continuous", "h743-uart-duplex-continuous"),
        ("h743 uart duplex continuous cache on", "h743-uart-duplex-continuous-cache-on", "h743-uart-duplex-continuous"),
        ("u575 retained time", "nucleo-u575-baremetal-empty", "u575-retained-time"),
        ("u575 stop2", "nucleo-u575-baremetal-empty", "u575-stop2"),
        ("u575 lpdma copy", "u575-lpdma-copy-stop2", "u575-lpdma-copy"),
        ("u575 lptim1 lpbam", "u575-lptim1-lpbam-stop2", "u575-lptim1-lpbam"),
        ("u575 mpu gpio", "u575-mpu-gpio-write", "u575-mpu-gpio"),
        ("f407 i2s2 slave tx", "sky-i2s2-slave-tx", "f407-i2s2-slave-tx"),
    )
    CANONICAL_ASSEMBLIES = frozenset({
        "h743-spi-dma-runtime-clock", "h743-spi-dma", "h743-spi-dma-cache-on",
        "u575-uart-chain", "h743-spi-dma-uart-tx", "h743-spi-dma-uart-rx",
        "g474-pin-states", "u575-lptim-run", "u575-rtc-run", "h743-uart-continuous",
        "h743-uart-continuous-cache-on", "h743-uart-duplex-continuous",
        "h743-uart-duplex-continuous-cache-on", "nucleo-u575-baremetal-empty",
        "u575-lpdma-copy-stop2", "u575-lptim1-lpbam-stop2",
        "u575-mpu-gpio-write", "sky-i2s2-slave-tx"})

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK CLI budgets ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.prefix = self.directory / "prefix with spaces"
        self.output = self.directory / "consumer results with spaces"
        self.sdk = self.prefix / "share/nexus/src"
        policy = self.sdk / "dependencies/platform-compatibility.json"
        policy.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
        (self.sdk / ".nexus-source-sdk.json").write_text(json.dumps({
            "publishable": True, "source_dirty": False,
            "source_revision": "a" * 40, "source_tree": "b" * 40}))
        verification = self.sdk / "cmake/package/verification/io_evolution"
        for name in ("h743-high-peripherals", "u575-uart-gpdma",
                     "g474-can-fd-normal", "g474-can-fd-loopback",
                     "g474-pwm-adc", "g474-adc-group-regular-software",
                     "g474-adc-group-regular-timer",
                     "g474-adc-group-injected-software",
                     "g474-adc-group-injected-timer", "h743-uart-fifo",
                     "u575-uart-fifo", "f407zg-eth-frames",
                     "gd470-eth-frames", "h743-uart-rtscts",
                     "u575-uart-rtscts", "g474-pwm-shared"):
            file = verification / (name + ".toml")
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('schema = 2\nbackend = "baremetal"\n# canonical installed test input: ' + name)
        for name in ("h743-high", "u575-uart-gpdma", "g474-can-fd",
                     "g474-pwm-adc", "g474-adc-group", "uart-fifo",
                     "ethernet-frames", "g474-pwm-shared"):
            file = verification / "consumers" / (name + ".c")
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("/* installed public smoke: " + name + " */\n")
        for stem in self.CANONICAL_ASSEMBLIES:
            relative = Path("tools/configure/assemblies") / (stem + ".toml")
            target = self.sdk / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        shutil.copyfile(ROOT / "cmake/package/verification/io_evolution/consumers/h743-spi-dma.c",
                        verification / "consumers/h743-spi-dma.c")
        shutil.copyfile(ROOT / "cmake/package/verification/io_evolution/consumers/pin-states.c",
                        verification / "consumers/pin-states.c")
        for stem in ("u575-lptim", "u575-rtc", "u575-retained-time",
                     "u575-stop2", "u575-lpdma-copy", "u575-lptim1-lpbam",
                     "u575-mpu-gpio", "f407-i2s2-slave-tx",
                     "h743-uart-continuous",
                     "h743-uart-duplex-continuous"):
            relative = Path("cmake/package/verification/io_evolution/consumers") / (stem + ".c")
            shutil.copyfile(ROOT / relative, self.sdk / relative)
        runtime_smoke = ROOT / "cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c"
        if runtime_smoke.is_file():
            shutil.copyfile(runtime_smoke, verification / "consumers/h743-runtime-clock.c")
        # These two real canonical files are SUT inputs. Their actual C
        # execution/proc mapping checks are qualified by the separate installed
        # integration; this suite models only external process ports.
        for profile in ("pair", "empty"):
            stem = "native-time-" + profile
            for relative in (stem + ".toml", "consumers/" + stem + ".c"):
                shutil.copyfile(ROOT / "cmake/package/verification/io_evolution" /
                                relative, verification / relative)
        specification = importlib.util.spec_from_file_location(
            "nexus_consumer_parallel_contract",
            ROOT / "cmake/package/verify_consumers.py")
        self.consumer = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.consumer)
        self.commands = []
        self.failed_build = False
        self.failed_sdk = False
        self.selected_fault = None

    def external_process(self, command, **options):
        command = [str(argument) for argument in command]
        self.commands.append(command)
        code = 0
        stdout = "external fixture\n"
        if command[0] == "cmake" and "-S" in command:
            source = Path(command[command.index("-S") + 1])
            build = Path(command[command.index("-B") + 1])
            build.mkdir(parents=True)
            (build / "resolved-sdk.txt").write_text(str(self.sdk))
            products = [path for path in (source / "main.c", source / "main.cpp",
                                         source / "private_headers.c")
                        if path.exists()]
            rows = [{"file": str(path), "directory": str(build),
                     "arguments": ["cc", "-c", str(path)],
                     "command": "cc -c " + shlex.quote(str(path))}
                    for path in products]
            (build / "compile_commands.json").write_text(json.dumps(rows))
            config = build / "configuration"
            config.mkdir()
            (config / "bindings.c").write_text("/* external CMake fixture */")
            (config / "resolved.json").write_text("{}")
            (build / "resolved-config.txt").write_text(str(config))
            binary = build / "bin"
            binary.mkdir()
            for name in ("installed_c", "installed_cpp", "installed_modern",
                         "installed_selected"):
                (binary / (name + ".resources.json")).write_text(
                    '{"status":"passed"}')
                (binary / (name + ".elf")).write_bytes(b"test double")
            assembly_arg = next((arg.split("=", 1)[1] for arg in command
                                 if arg.startswith("-DNEXUS_ASSEMBLY_FILE=")), None)
            native = (source.name.startswith("selected ") and assembly_arg is not None
                      and tomllib.loads(Path(assembly_arg).read_text())["backend"] == "native")
            if native:
                authored = tomllib.loads(Path(assembly_arg).read_text())
                controllers = []
                for kind in ("rtc", "lptim"):
                    for name, item in authored.get(kind, {}).items():
                        controller = {"kind": kind, "id": name, "mode": item["mode"]}
                        if kind == "lptim":
                            controller["tick_hz"] = item["tick_hz"]
                        controllers.append(controller)
                (config / "resolved.json").write_text(json.dumps({
                    "backend": "native", "soc_family": "native",
                    "cpu": {"arch": "native"}, "controllers": controllers}))
                # Minimal ELF64 format input; the external invocation below is
                # a process test double, never a claim to execute these bytes.
                header = bytearray(64)
                header[:6] = b"\x7fELF\x02\x01"
                header[18:20] = (62).to_bytes(2, "little")
                (binary / "installed_selected").write_bytes(header)
                rows.append({"file": str(config / "bindings.c"),
                             "directory": str(build),
                             "arguments": ["cc", "-c", str(config / "bindings.c")],
                             "command": "cc -c " + shlex.quote(str(config / "bindings.c"))})
                if self.selected_fault == "native_private_include":
                    rows[0]["arguments"].insert(1, "-I" + str(self.sdk / "io/native"))
                    rows[0]["command"] = shlex.join(rows[0]["arguments"])
                (build / "compile_commands.json").write_text(json.dumps(rows))
                # Native owns no ARM byproducts. Their absence is asserted by
                # the full-matrix oracle after the real verifier returns.
                for name in ("installed_selected.elf", "installed_selected.resources.json"):
                    (binary / name).unlink()
            elif source.name.startswith("selected "):
                import hashlib
                def identity(path):
                    return {"path": str(path), "size": path.stat().st_size,
                            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                image = binary / "installed_selected.elf"
                for suffix in ("bin", "map"):
                    (binary / ("installed_selected." + suffix)).write_text(
                        "external build artifact " + suffix)
                resources = {"status": "passed", "elf": identity(image),
                             "resolved": identity(config / "resolved.json")}
                if self.selected_fault == "resource_identity":
                    resources["elf"]["sha256"] = "f" * 64
                (binary / "installed_selected.resources.json").write_text(
                    json.dumps(resources))
                if self.selected_fault == "missing_bin":
                    (binary / "installed_selected.bin").unlink()
                if self.selected_fault == "private_include":
                    (build / "compile_commands.json").write_text(json.dumps([
                        {"file": str(source / "main.c"),
                         "command": "cc -I" + str(self.sdk / "soc/stm32u575/private")
                                    + " -c main.c"},
                        {"file": str(source / "private_headers.c"),
                         "command": "cc -c private_headers.c"}]))
        elif command[0] == "cmake" and "--build" in command:
            code = 42 if self.failed_build else 0
            if (Path(command[2]).name.startswith("selected ") and
                    self.selected_fault == "build"):
                code = 43
            if (Path(command[2]).name.startswith("selected g474 adc ") and
                    self.selected_fault == "adc_build"):
                code = 44
            if (" fifo " in Path(command[2]).name and
                    self.selected_fault == "fifo_build"):
                code = 45
            if (" frames " in Path(command[2]).name and
                    self.selected_fault == "frames_build"):
                code = 46
            for family in ("h743", "u575"):
                if (Path(command[2]).name == "selected " + family + " rtscts build"
                        and self.selected_fault == family + "_rtscts_build"):
                    code = 47
            if (Path(command[2]).name == "selected g474 shared pwm build"
                    and self.selected_fault == "shared_pwm_build"):
                code = 48
        elif command[0] == "nm":
            build = Path(command[-1]).parents[1]
            profile = "empty" if "native time empty" in build.name else "pair"
            if profile == "pair":
                stdout = ("nx_native_rtc_ops D 2000 30\n"
                          "nx_native_lptim_ops D 2030 28\n"
                          "nx_native_time_clock_read t 1000 5\n"
                          "s_nx_native_time_clock d 2060 10\n"
                          "s_nx_port_calendar_a b 3000 40\n"
                          "s_nx_port_calendar_b b 3040 40\n"
                          "s_nx_port_counter_a b 3080 38\n"
                          "s_nx_port_counter_b b 30b8 38\n")
                if self.selected_fault == "native_missing_state":
                    stdout = "\n".join(line for line in stdout.splitlines()
                                         if not line.startswith("s_nx_port_counter_b "))
            else:
                stdout = ""
                extras = {
                    "empty_state": "s_nx_port_calendar_a b 3000 40\n",
                    "empty_ops": "nx_native_rtc_ops D 2000 30\n",
                    "empty_clock_face": "s_nx_native_time_clock d 2060 10\n",
                    "empty_clock_read": "nx_native_time_clock_read t 1000 5\n",
                }
                stdout = extras.get(self.selected_fault, stdout)
            if self.selected_fault == "native_nm":
                code = 50
        elif len(command) == 1 and Path(command[0]).name == "installed_selected":
            if self.selected_fault == "native_runtime":
                code = 49
        elif any(argument.endswith("retention.py") for argument in command):
            failed = ("selected " in command[command.index("--elf") + 1]
                      and self.selected_fault == "retention")
            failed = failed or (
                "selected g474 adc " in command[command.index("--elf") + 1]
                and self.selected_fault == "adc_retention")
            failed = failed or (
                " fifo " in command[command.index("--elf") + 1]
                and self.selected_fault == "fifo_retention")
            failed = failed or (
                " frames " in command[command.index("--elf") + 1]
                and self.selected_fault == "frames_retention")
            for family in ("h743", "u575"):
                failed = failed or (
                    Path(command[command.index("--elf") + 1]).parents[1].name ==
                    "selected " + family + " rtscts build" and
                    self.selected_fault == family + "_rtscts_retention")
            failed = failed or (
                Path(command[command.index("--elf") + 1]).parents[1].name ==
                "selected g474 shared pwm build" and
                self.selected_fault == "shared_pwm_retention")
            report = {"status": "failed" if failed else "passed"}
            if "selected " in command[command.index("--elf") + 1]:
                import hashlib
                path = Path(command[command.index("--elf") + 1])
                report["elf"] = {"path": str(path), "size": path.stat().st_size,
                                 "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            Path(command[command.index("--output") + 1]).write_text(json.dumps(report))
        elif any(argument.endswith("package_source_sdk.py")
                 for argument in command):
            code = 9 if self.failed_sdk else 0
        if self.selected_fault and self.selected_fault.startswith("selected:"):
            _, operation, label = self.selected_fault.split(":", 2)
            if operation == "build" and command[0] == "cmake" and "--build" in command:
                if Path(command[command.index("--build") + 1]).name == "selected " + label + " build":
                    code = 51
            if operation == "retention" and any(x.endswith("retention.py") for x in command):
                image = Path(command[command.index("--elf") + 1])
                if image.parents[1].name == "selected " + label + " build":
                    code = 52
        return subprocess.CompletedProcess(command, code, stdout, "")

    def execute(self, extra=()):
        with mock.patch.object(subprocess, "run",
                               side_effect=self.external_process), \
                mock.patch.object(maintained_process, "run",
                                  side_effect=self.external_run):
            status = self.consumer.main([
                "--prefix", str(self.prefix), "--output", str(self.output),
                *extra])
        report = json.loads((self.output / "verification.json").read_text())
        return status, report

    def external_run(self, command, **options):
        """Model only the external port, including its retained receipt shape."""
        started = time.time_ns()
        result = self.external_process(command)
        log = Path(options["log"])
        attempt = log.parent / (log.stem + ".attempts") / "external-fixture"
        attempt.mkdir(parents=True)
        def identity(name, text):
            path = attempt / name
            path.write_text(text)
            return {"path": str(path), "bytes": path.stat().st_size,
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        receipt = {"schema": 1, "command": [str(item) for item in command],
            "cwd": str(options["cwd"]), "stage": options.get("stage", "fixture"),
            "started_ns": started, "finished_ns": time.time_ns(),
            "duration_seconds": (time.time_ns() - started) / 1e9,
            "exit_code": result.returncode, "child_returncode": result.returncode,
            "outcome": "completed", "withdrawal_complete": True,
            "withdrawal_stable": True, "withdrawn_processes": [],
            "stdout": identity("stdout.txt", result.stdout),
            "stderr": identity("stderr.txt", result.stderr)}
        result.receipt_path = attempt / "receipt.json"
        result.receipt_path.write_text(json.dumps(receipt))
        log.write_text(result.stdout + result.stderr)
        return result

    def builds(self):
        return [command for command in self.commands
                if command[0] == "cmake" and "--build" in command]

    def assert_complete_matrix(self):
        builds = self.builds()
        expected = {"native", "stm32", "gd32"}
        expected.update(family + " " + backend
                        for family in ("h743", "u575", "f103", "gd303", "g474")
                        for backend in ("baremetal", "freertos"))
        expected.update("selected " + name
                        for name, _, _ in self.SELECTED_INPUTS)
        self.assertEqual({Path(command[2]).name.removesuffix(" build")
                          for command in builds}, expected)
        self.assertEqual(len(builds), 13 + len(self.SELECTED_INPUTS))
        self.assertEqual(sum(len(command) - command.index("--target") - 1
                             for command in builds), 16 + len(self.SELECTED_INPUTS))
        configurations = [command for command in self.commands
                          if command[0] == "cmake" and "-S" in command]
        self.assertEqual(len(configurations), 13 + len(self.SELECTED_INPUTS))
        for command in configurations:
            self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "a" * 40,
                          command)
        retention = [command for command in self.commands
                     if any(arg.endswith("retention.py") for arg in command)]
        self.assertEqual(len(retention), 8 + len(self.SELECTED_INPUTS))
        native = [command for command in self.commands if len(command) == 1]
        self.assertEqual({Path(command[0]).name for command in native},
                         {"installed_c", "installed_cpp", "installed_selected"})
        self.assertEqual(len(native), 4)
        self.assertEqual({Path(command[0]).parents[1].name for command in native
                          if Path(command[0]).name == "installed_selected"},
                         {"selected native time pair build", "selected native time empty build"})
        nm = [command for command in self.commands if command[0] == "nm"]
        self.assertEqual(len(nm), 2)
        self.assertEqual({Path(command[-1]).parents[1].name for command in nm},
                         {"selected native time pair build", "selected native time empty build"})
        self.assertIn("--verify", self.commands[0])

    def test_producer_uses_the_standalone_transport_plan_without_a_copy(self):
        from tools.release import transport
        for name in ("BASE_CONSUMERS", "MODERN_CONSUMERS", "SELECTED_CONSUMERS"):
            self.assertIs(getattr(self.consumer, name), getattr(transport, name))
        expected = {
            (name, ("tools/configure/assemblies/" if stem in
                    self.CANONICAL_ASSEMBLIES else
                    "cmake/package/verification/io_evolution/") + stem +
             ".toml", smoke)
            for name, stem, smoke in self.SELECTED_INPUTS}
        self.assertEqual(set(transport.SELECTED_CONSUMERS), expected)
        self.assertEqual(len(transport.SELECTED_CONSUMERS), len(expected))
        status, _ = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        self.assert_complete_matrix()
        sources = {path.name for path in self.output.glob("* source")}
        self.assertEqual(sources, {source for source, _, _ in
                                   transport.consumer_layout()})

    def test_spi_inputs_are_explicit_canonical_paths_with_one_smoke(self):
        for label, variant in (("h743 spi dma", "spi-dma"),
                               ("h743 spi dma cache on", "spi-dma-cache-on"),
                               ("h743 spi dma uart tx", "spi-dma-uart-tx"),
                               ("h743 spi dma uart rx", "spi-dma-uart-rx")):
            with self.subTest(variant=variant):
                self.assertIn((label, "tools/configure/assemblies/h743-" +
                               variant + ".toml", "h743-spi-dma"),
                              self.consumer.SELECTED_CONSUMERS)
        self.assertTrue(all(Path(path).suffix == ".toml" and
                            not Path(path).is_absolute() and
                            ".." not in Path(path).parts
                            for _, path, _ in self.consumer.SELECTED_CONSUMERS))

    def test_selected_owner_smokes_link_explicit_public_components(self):
        for label, stem, smoke, library in (
                ("u575 retained time", "nucleo-u575-baremetal-empty",
                 "u575-retained-time", "Nexus::U575RetainedTime"),
                ("u575 stop2", "nucleo-u575-baremetal-empty", "u575-stop2",
                 "Nexus::U575Stop2"),
                ("u575 lpdma copy", "u575-lpdma-copy-stop2", "u575-lpdma-copy",
                 "Nexus::U575Stop2Copy"),
                ("u575 lptim1 lpbam", "u575-lptim1-lpbam-stop2",
                 "u575-lptim1-lpbam", "Nexus::U575Stop2Lptim1"),
                ("u575 mpu gpio", "u575-mpu-gpio-write",
                 "u575-mpu-gpio", "Nexus::U575MpuGpio"),
                ("f407 i2s2 slave tx", "sky-i2s2-slave-tx",
                 "f407-i2s2-slave-tx", "Nexus::F407I2S2SlaveTx")):
            assembly = "tools/configure/assemblies/" + stem + ".toml"
            with self.subTest(smoke=smoke):
                directory = self.directory / smoke
                directory.mkdir()
                source, _, command = self.consumer.prepare_selected_consumer(
                    self.prefix, directory, label, assembly, smoke,
                    lambda argv: None, expected_revision="a" * 40)
                canonical = (self.sdk / "cmake/package/verification/io_evolution/consumers" /
                             (smoke + ".c"))
                self.assertEqual((source / "main.c").read_bytes(),
                                 canonical.read_bytes())
                project = (source / "CMakeLists.txt").read_text()
                self.assertIn("LIBRARIES " + library + ")", project)
                if smoke == "f407-i2s2-slave-tx":
                    self.assertIn("stm32f407_i2s2_slave_tx.h",
                                  (source / "private_headers.c").read_text())
                self.assertIn("set(CMAKE_C_STANDARD 11)", project)
                self.assertIn("set(CMAKE_C_STANDARD_REQUIRED ON)", project)
                self.assertIn("set(CMAKE_C_EXTENSIONS OFF)", project)
                self.assertIn("-DNEXUS_ASSEMBLY_FILE=" +
                              str(self.sdk / assembly), command)
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg
                                    for arg in command))

    def test_runtime_clock_uses_canonical_input_and_public_lifecycle_smoke(self):
        self.assertIn(("h743 runtime clock",
                       "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml",
                       "h743-runtime-clock"), self.consumer.SELECTED_CONSUMERS)
        status, _ = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        self.assert_complete_matrix()
        smoke = self.output / "selected h743 runtime clock source/main.c"
        canonical = self.sdk / "cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c"
        self.assertEqual(smoke.read_bytes(), canonical.read_bytes())
        for name in ("nx_platform_runtime_domain", "nx_platform_runtime_pause",
                     "nx_platform_runtime_resume", "nx_clock_domain_quiescent"):
            self.assertIn(name + "(", smoke.read_text())

    def test_missing_each_canonical_input_is_terminal_without_directory_fallback(self):
        for stem in sorted(self.CANONICAL_ASSEMBLIES - {"g474-pin-states"}):
            with self.subTest(stem=stem):
                path = self.sdk / "tools/configure/assemblies" / (stem + ".toml")
                original = path.read_bytes()
                path.unlink()
                # A same-stem legacy-directory decoy must not replace it.
                decoy = self.sdk / "cmake/package/verification/io_evolution" / path.name
                decoy.write_bytes(original)
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                path.write_bytes(original)
                decoy.unlink()

    def test_missing_named_pin_state_canonical_input_cannot_use_a_legacy_decoy(self):
        relative = "tools/configure/assemblies/g474-pin-states.toml"
        source = self.sdk / relative
        original = source.read_bytes()
        source.unlink()
        decoy = self.sdk / "cmake/package/verification/io_evolution/g474-pin-states.toml"
        decoy.write_bytes(original)
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertNotIn("g474 pin states", {row["name"] for row in report["selected_images"]})
        self.assertIn("g474-pin-states.toml", report["error"])

    def test_sdk_relative_inputs_reject_alias_schema_and_symlink_before_output(self):
        relative = "tools/configure/assemblies/h743-spi-dma.toml"
        path = self.sdk / relative
        original = path.read_bytes()
        directory = self.directory / "invalid selected inputs"
        directory.mkdir()
        alias = path.with_name("alias.toml")
        alias.symlink_to(path)
        cases = [str(path), "./" + relative,
                 "tools/configure/assemblies/../assemblies/h743-spi-dma.toml",
                 "tools/configure/assemblies/alias.toml"]
        for source in cases:
            with self.subTest(source=source), self.assertRaises(ValueError):
                self.consumer.prepare_selected_consumer(
                    self.prefix, directory, "invalid", source, "h743-spi-dma",
                    lambda argv: None, expected_revision="a" * 40)
            self.assertFalse((directory / "selected invalid source").exists())
        for schema in ("1", "true", "2.0"):
            with self.subTest(schema=schema):
                path.write_bytes(original.replace(b"schema = 2",
                                                  ("schema = " + schema).encode()))
                with self.assertRaisesRegex(ValueError, "schema 2"):
                    self.consumer.prepare_selected_consumer(
                        self.prefix, directory, "invalid", relative,
                        "h743-spi-dma", lambda argv: None,
                        expected_revision="a" * 40)
                self.assertFalse((directory / "selected invalid source").exists())
        path.write_bytes(original)

    def test_each_selected_mode_build_and_retention_failure_terminates_the_matrix(self):
        for label in ("h743 spi dma", "h743 spi dma cache on",
                      "h743 spi dma uart tx", "h743 spi dma uart rx",
                      "u575 uart chain", "u575 lptim run", "u575 rtc run",
                      "h743 uart continuous", "h743 uart continuous cache on",
                      "h743 uart duplex continuous",
                      "h743 uart duplex continuous cache on",
                      "u575 retained time", "u575 stop2", "u575 lpdma copy",
                      "u575 lptim1 lpbam", "u575 mpu gpio", "f407 i2s2 slave tx"):
            for operation, exit_code in (("build", 51), ("retention", 52)):
                with self.subTest(label=label, operation=operation):
                    self.selected_fault = "selected:" + operation + ":" + label
                    with contextlib.redirect_stderr(io.StringIO()):
                        status, report = self.execute(("--parallel", "1"))
                    self.assertEqual(status, 1)
                    self.assertEqual(report["status"], "failed")
                    self.assertNotIn(label, {row["name"] for row in report["selected_images"]})
                    self.assertEqual(report["commands"][-1]["exit_code"], exit_code)

    def test_explicit_one_bounds_every_build_without_reducing_the_matrix(self):
        with mock.patch.dict("os.environ", {"CMAKE_BUILD_PARALLEL_LEVEL": "99"}):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["physical_status"], "not_executed")
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()],
                         ["1"] * (13 + len(self.SELECTED_INPUTS)))

    def test_default_uses_two_consumers_with_one_job_per_build(self):
        status, report = self.execute()
        self.assertEqual(status, 0)
        self.assertEqual(report["source_revision"], "a" * 40)
        self.assertEqual(report["source_tree"], "b" * 40)
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()],
                         ["1"] * (13 + len(self.SELECTED_INPUTS)))
        self.assertEqual(report["parallel_jobs"], 2)
        self.assertEqual(report["consumer_workers"], 2)

    def test_positive_budget_bounds_consumers_without_nested_build_jobs(self):
        status, report = self.execute(("--parallel", "3"))
        self.assertEqual(status, 0)
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()],
                         ["1"] * (13 + len(self.SELECTED_INPUTS)))
        self.assertEqual(report["parallel_jobs"], 3)
        self.assertEqual(report["consumer_workers"], 3)

    def test_selected_inputs_and_public_smokes_come_only_from_installed_sdk(self):
        status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        configurations = [command for command in self.commands
                          if command[0] == "cmake" and "-S" in command
                          and Path(command[command.index("-S") + 1]).name.startswith(
                              "selected ")]
        self.assertEqual(len(configurations), len(self.SELECTED_INPUTS))
        inputs = set()
        for command in configurations:
            assembly = Path(next(argument.split("=", 1)[1] for argument in command
                                 if argument.startswith("-DNEXUS_ASSEMBLY_FILE=")))
            expected_parent = (self.sdk / "tools/configure/assemblies"
                               if assembly.stem in self.CANONICAL_ASSEMBLIES else
                               self.sdk / "cmake/package/verification/io_evolution")
            self.assertEqual(assembly.parent, expected_parent)
            inputs.add(assembly.name)
            self.assertIn("-DNEXUS_BUILD_TESTS=OFF", command)
            self.assertIn("-DNEXUS_BUILD_WORKLOAD=OFF", command)
            self.assertFalse(any("ALLOW_SOURCE_SDK_FIXTURE" in arg for arg in command))
            source = Path(command[command.index("-S") + 1])
            smoke = (source / "main.c").read_text()
            if assembly.stem.startswith("native-time-"):
                canonical = assembly.parent / "consumers" / (assembly.stem + ".c")
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertFalse(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
                if assembly.stem.endswith("pair"):
                    self.assertIn('/proc/self/maps', smoke)
                    self.assertIn('readonly_mapping(a->ops, sizeof(*a->ops))', smoke)
            elif assembly.name.startswith("h743-spi-dma"):
                stem = ("h743-runtime-clock" if assembly.stem ==
                        "h743-spi-dma-runtime-clock" else "h743-spi-dma")
                canonical = self.sdk / "cmake/package/verification/io_evolution/consumers" / (stem + ".c")
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            elif assembly.stem == "sky-i2s2-slave-tx":
                canonical = (self.sdk /
                    "cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c")
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertIn("nx_f407_i2s2_slave_tx_initialize(", smoke)
                self.assertIn("nx_frame_stream_acquire(", smoke)
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            elif assembly.stem == "nucleo-u575-baremetal-empty":
                stem = ("u575-retained-time" if source.name ==
                        "selected u575 retained time source" else "u575-stop2")
                canonical = self.sdk / "cmake/package/verification/io_evolution/consumers" / (stem + ".c")
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            elif assembly.stem in {"u575-lpdma-copy-stop2",
                                   "u575-lptim1-lpbam-stop2", "u575-mpu-gpio-write"}:
                stem = ("u575-lpdma-copy" if assembly.stem ==
                        "u575-lpdma-copy-stop2" else "u575-lptim1-lpbam" if
                        assembly.stem == "u575-lptim1-lpbam-stop2" else "u575-mpu-gpio")
                canonical = (self.sdk /
                    "cmake/package/verification/io_evolution/consumers" /
                    (stem + ".c"))
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            elif assembly.name == "g474-pin-states.toml":
                canonical = self.sdk / "cmake/package/verification/io_evolution/consumers/pin-states.c"
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertIn("nx_factory_pin_state", smoke)
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            elif assembly.stem in {"u575-lptim-run", "u575-rtc-run", "h743-uart-continuous",
                                   "h743-uart-continuous-cache-on",
                                   "h743-uart-duplex-continuous",
                                   "h743-uart-duplex-continuous-cache-on"}:
                stem = ("u575-rtc" if assembly.stem == "u575-rtc-run"
                        else "u575-lptim" if assembly.stem == "u575-lptim-run"
                        else "h743-uart-duplex-continuous" if
                        assembly.stem.startswith("h743-uart-duplex-")
                        else "h743-uart-continuous")
                canonical = self.sdk / "cmake/package/verification/io_evolution/consumers" / (stem + ".c")
                self.assertEqual((source / "main.c").read_bytes(), canonical.read_bytes())
                self.assertIn("nx_factory_", smoke)
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            else:
                self.assertTrue(smoke.startswith("/* installed public smoke: "))
                self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
            private_guard = (source / "private_headers.c").read_text()
            for name in ("stm32h7xx.h", "stm32u5xx.h", "stm32g4xx.h",
                         "stm32f4xx.h", "gd32f4xx.h",
                         "stm32f407_eth_frames.h", "gd32f470_eth_frames.h",
                         "stm32g474_pwm_shared.h", "rtc_lptim_provider.h",
                         "stm32h743_spi_dma.h", "stm32g474_pin_state.h",
                         "pin_state_impl.h", "stm32u575_lptim.h", "stm32u575_rtc.h",
                         "retained_time.h", "stm32u575_rtc_calendar.h",
                         "stm32u575_stop2_hw.h",
                         "stm32u575_lpdma_copy_hw.h",
                         "stm32u575_lpdma_copy_internal.h",
                         "stm32u575_lptim1_profile.h",
                         "stm32u575_lpdma_lptim1_internal.h",
                         "stm32u575_uart_gpdma_chain.h",
                         "stm32u575_uart_gpdma_impl.h",
                         "stm32h743_uart_continuous.h",
                         "stm32h743_uart_duplex.h",
                         "stm32h743_uart_continuous_impl.h",
                         "stm32h743_uart_dma_parts.h"):
                self.assertIn('__has_include("' + name + '")', private_guard)
            self.assertNotIn(str(ROOT), (source / "CMakeLists.txt").read_text())
        self.assertEqual(inputs, {stem + ".toml" for _, stem, _
                                  in self.SELECTED_INPUTS})
        self.assertEqual(len(report["selected_images"]), len(self.SELECTED_INPUTS))
        arm_images = [image for image in report["selected_images"]
                      if not image["name"].startswith("native time ")]
        self.assertEqual(len(arm_images), len(self.SELECTED_INPUTS) - 2)
        for image in arm_images:
            self.assertEqual(set(image["artifacts"]), {"elf", "bin", "map"})
        native_images = [image for image in report["selected_images"]
                         if image["name"].startswith("native time ")]
        self.assertEqual([image["selected_state_count"] for image in native_images], [4, 0])
        self.assertEqual([image["selected_state_bytes"] for image in native_images], [240, 0])
        for image in native_images:
            self.assertEqual(image["kind"], "native-time-host-observation")
            self.assertEqual(image["status"], "passed")
            self.assertNotIn("artifacts", image)
            self.assertNotIn("resources", image)
            binary = Path(image["elf"]["path"])
            self.assertEqual(binary.name, "installed_selected")
            for suffix in ("elf", "bin", "map", "resources.json"):
                self.assertFalse(binary.with_name(binary.name + "." + suffix).exists())

    def test_a_new_consumer_build_failure_is_terminal(self):
        self.selected_fault = "build"
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(self.builds()), 14)
        self.assertEqual(report["commands"][-1]["exit_code"], 43)

    def test_selected_retention_failure_is_terminal(self):
        self.selected_fault = "retention"
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(self.builds()), 14)
        self.assertIn("retention", report["error"])

    def test_adc_group_failure_is_terminal_after_all_eighteen_prior_roots(self):
        for fault in ("adc_build", "adc_retention"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 19)
                if fault == "adc_build":
                    self.assertEqual(report["commands"][-1]["exit_code"], 44)
                else:
                    self.assertIn("retention", report["error"])

    def test_selected_private_inputs_and_stale_resources_are_rejected(self):
        for fault in ("private_include", "resource_identity", "missing_bin"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")

    def test_fifo_failure_is_terminal_after_all_twenty_two_prior_roots(self):
        for fault in ("fifo_build", "fifo_retention"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 23)
                if fault == "fifo_build":
                    self.assertEqual(report["commands"][-1]["exit_code"], 45)
                else:
                    self.assertIn("retention", report["error"])

    def test_missing_installed_input_or_smoke_never_falls_back_to_checkout(self):
        directory = self.sdk / "cmake/package/verification/io_evolution"
        for relative in ("h743-high-peripherals.toml", "consumers/h743-high.c",
                         "g474-adc-group-regular-software.toml",
                         "consumers/g474-adc-group.c", "h743-uart-fifo.toml",
                         "u575-uart-fifo.toml", "consumers/uart-fifo.c",
                         "f407zg-eth-frames.toml", "gd470-eth-frames.toml",
                         "consumers/ethernet-frames.c", "h743-uart-rtscts.toml",
                         "u575-uart-rtscts.toml", "g474-pwm-shared.toml",
                         "consumers/g474-pwm-shared.c", "consumers/u575-retained-time.c",
                         "consumers/u575-stop2.c",
                         "consumers/u575-lptim1-lpbam.c", "native-time-pair.toml",
                         "native-time-empty.toml", "consumers/native-time-pair.c",
                         "consumers/native-time-empty.c"):
            with self.subTest(relative=relative):
                file = directory / relative
                original = file.read_bytes()
                file.unlink()
                try:
                    with contextlib.redirect_stderr(io.StringIO()):
                        status, report = self.execute(("--parallel", "1"))
                    self.assertEqual(status, 1)
                    self.assertEqual(report["status"], "failed")
                finally:
                    file.write_bytes(original)

    def test_frames_failure_is_terminal_after_all_twenty_four_prior_roots(self):
        for fault in ("frames_build", "frames_retention"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 25)
                if fault == "frames_build":
                    self.assertEqual(report["commands"][-1]["exit_code"], 46)
                else:
                    self.assertIn("retention", report["error"])

    def test_rtscts_reuses_the_same_public_byte_smoke_in_both_roots(self):
        status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        expected = (self.sdk / "cmake/package/verification/io_evolution/"
                    "consumers/uart-fifo.c").read_bytes()
        for family in ("h743", "u575"):
            source = self.output / ("selected " + family + " rtscts source")
            self.assertTrue((source / "main.c").is_file())
            self.assertEqual((source / "main.c").read_bytes(), expected)
            self.assertEqual(sum(image["name"] == family + " rtscts"
                                 for image in report["selected_images"]), 1)

    def test_rtscts_product_probe_rejects_both_real_private_provider_headers(self):
        directory = self.directory / "private isolation inputs"
        directory.mkdir()
        source, _, _ = self.consumer.prepare_selected_consumer(
            self.prefix, directory, "h743 rtscts",
            "cmake/package/verification/io_evolution/h743-uart-rtscts.toml",
            "uart-fifo", lambda command: None, expected_revision="a" * 40)
        probe = (source / "private_headers.c").read_text()
        for family in ("stm32h743", "stm32u575"):
            self.assertIn('__has_include("' + family + '_uart_rtscts.h")',
                          probe)

    def test_each_rtscts_failure_is_terminal_after_the_preserved_matrix(self):
        for family, builds in (("h743", 27), ("u575", 28)):
            for operation in ("build", "retention"):
                with self.subTest(family=family, operation=operation):
                    self.selected_fault = family + "_rtscts_" + operation
                    before = len(self.builds())
                    with contextlib.redirect_stderr(io.StringIO()):
                        status, report = self.execute(("--parallel", "1"))
                    self.assertEqual(status, 1)
                    self.assertEqual(report["status"], "failed")
                    self.assertEqual(len(self.builds()) - before, builds)
                    self.assertEqual(Path(self.builds()[-1][2]).name,
                                     "selected " + family + " rtscts build")
                    self.assertFalse(any(image["name"] == family + " rtscts"
                                         for image in report["selected_images"]))
                    if operation == "build":
                        self.assertEqual(report["commands"][-1]["exit_code"], 47)
                    else:
                        self.assertIn("retention", report["error"])

    def test_shared_pwm_failure_is_terminal_after_all_preserved_roots(self):
        for operation in ("build", "retention"):
            with self.subTest(operation=operation):
                self.selected_fault = "shared_pwm_" + operation
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 29)
                self.assertEqual(Path(self.builds()[-1][2]).name,
                                 "selected g474 shared pwm build")
                self.assertEqual(len(report["selected_images"]), 15)
                if operation == "build":
                    self.assertEqual(report["commands"][-1]["exit_code"], 48)
                else:
                    self.assertIn("retention", report["error"])

    def test_native_runtime_nm_private_state_and_missing_owner_fail_closed(self):
        for fault in ("native_runtime", "native_nm", "native_private_include",
                      "native_missing_state"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 30)
                self.assertEqual(len(report["selected_images"]), 16)
                self.assertEqual(Path(self.builds()[-1][2]).name,
                                 "selected native time pair build")
                if fault in ("native_runtime", "native_nm"):
                    self.assertEqual(report["commands"][-1]["exit_code"],
                                     49 if fault == "native_runtime" else 50)
                else:
                    self.assertIn("Native", report["error"])

    def test_empty_native_state_ops_and_both_clock_objects_are_absent(self):
        for fault in ("empty_state", "empty_ops", "empty_clock_face", "empty_clock_read"):
            with self.subTest(fault=fault):
                self.selected_fault = fault
                before = len(self.builds())
                with contextlib.redirect_stderr(io.StringIO()):
                    status, report = self.execute(("--parallel", "1"))
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(len(self.builds()) - before, 31)
                self.assertEqual(len(report["selected_images"]), 17)
                self.assertEqual(Path(self.builds()[-1][2]).name,
                                 "selected native time empty build")
                self.assertIn("retention", report["error"])

    def test_shutdown_signals_join_and_publish_a_failed_report(self):
        from concurrent.futures import ThreadPoolExecutor
        actual_shutdown = ThreadPoolExecutor.shutdown
        for selected in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signal=selected):
                previous = {item: signal.getsignal(item)
                            for item in (signal.SIGINT, signal.SIGTERM)}
                delivered = []
                escaped = None
                def shutdown(executor, *arguments, **options):
                    delivered.append(selected)
                    try:
                        os.kill(os.getpid(), selected)
                    finally:
                        actual_shutdown(executor, *arguments, **options)
                try:
                    with mock.patch.object(ThreadPoolExecutor, "shutdown",
                                           shutdown):
                        try:
                            status, report = self.execute(("--parallel", "2"))
                        except KeyboardInterrupt as error:
                            escaped = error
                finally:
                    for signum, handler in previous.items():
                        signal.signal(signum, handler)
                self.assertEqual(delivered, [selected])
                self.assertIsNone(escaped,
                    "signal escaped while joining owned consumer workers")
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "failed")
                self.assertEqual(report["first_failure"]["exit_code"], 130)
                self.assertTrue(all(row["withdrawal_complete"]
                                    for row in report["commands"]))

    def test_invalid_budget_rejects_before_any_output_or_external_command(self):
        for value in ("0", "-1", "01", "+1", "1.5", "True", "", " 1"):
            with self.subTest(value=value), mock.patch.object(
                    subprocess, "run") as command, \
                    mock.patch.object(maintained_process, "run") as maintained:
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors), self.assertRaises(
                        SystemExit) as failure:
                    self.consumer.main([
                        "--prefix", str(self.prefix), "--output", str(self.output),
                        "--parallel", value])
                self.assertEqual(failure.exception.code, 2)
                self.assertIn("positive integer", errors.getvalue())
                self.assertFalse(self.output.exists())
                command.assert_not_called()
                maintained.assert_not_called()

    def test_explicit_budget_preserves_a_real_subprocess_failure(self):
        self.failed_build = True
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(self.builds()), 1)
        self.assertEqual(report["commands"][-1]["exit_code"], 42)

    def test_explicit_budget_cannot_skip_sdk_verification(self):
        self.failed_sdk = True
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(self.builds(), [])
        self.assertEqual(report["commands"][-1]["exit_code"], 9)


class SDKConsumerWorkerPoolTests(unittest.TestCase):
    """Run real owned subprocesses behind the external CMake/SDK port."""

    SHIM = r'''
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

payload = json.loads(sys.argv[1])
command = payload['command']
workspace = Path(payload['workspace'])
root = Path(payload['root'])
sys.path.insert(0, str(root))

def event(phase):
    row = {'phase': phase, 'time_ns': time.monotonic_ns(), 'pid': os.getpid(),
           'argv': command, 'nexus_jobs': os.environ.get('NEXUS_BUILD_JOBS'),
           'cmake_jobs': os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL')}
    descriptor = os.open(workspace / 'events.jsonl',
                         os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.write(descriptor, (json.dumps(row) + '\n').encode())
    finally:
        os.close(descriptor)

def wait_for(predicate, timeout=3):
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() >= deadline:
            print('independent consumer did not reach rendezvous', flush=True)
            raise SystemExit(91)
        time.sleep(0.01)

event('start')
try:
    configure = command[0] == 'cmake' and '-S' in command
    label = Path(command[command.index('-S') + 1]).name if configure else ''
    mode = payload['mode']
    if mode == 'overlap' and configure:
        first = ('native source', 'stm32 source', 'gd32 source')[:payload['jobs']]
        if label in first:
            (workspace / ('ready-' + label)).write_text(str(os.getpid()))
            wait_for(lambda: all((workspace / ('ready-' + name)).exists()
                                 for name in first))
    if mode == 'fail-peer' and configure:
        if label == 'native source':
            child = subprocess.Popen([sys.executable, '-c',
                'import os,sys,time;from pathlib import Path;'
                'Path(sys.argv[1]).write_text(str(os.getpid()));time.sleep(30)',
                str(workspace / 'grandchild.pid')])
            (workspace / 'blocked.pid').write_text(str(os.getpid()))
            wait_for(lambda: (workspace / 'grandchild.pid').exists())
            print('blocked consumer parent and grandchild', flush=True)
            time.sleep(30)
        elif label == 'stm32 source':
            wait_for(lambda: (workspace / 'grandchild.pid').exists())
            print('actual peer failure 42', flush=True)
            raise SystemExit(42)
    if (mode == 'two-failures' and configure and
            label in ('native source', 'stm32 source')):
        raise SystemExit(42 if label == 'native source' else 43)
    if mode in ('timeout', 'interrupt') and configure and label == 'native source':
        (workspace / 'blocked.pid').write_text(str(os.getpid()))
        print('actual blocked command before ' + mode, flush=True)
        time.sleep(30)
    if (mode == 'validation' and
            any(item.endswith('retention.py') for item in command)):
        image = Path(command[command.index('--elf') + 1])
        if image.parents[1].name == 'h743 baremetal build':
            (workspace / 'validation-ready').write_text(str(os.getpid()))
            wait_for(lambda: (workspace / 'validation-release').exists(), 5)
    specification = importlib.util.spec_from_file_location(
        'external_sdk_fixture', root / 'tools/testing/test_sdk_verification_call.py')
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    fixture = module.SDKConsumerParallelTests()
    fixture.sdk = Path(payload['sdk'])
    fixture.commands = []
    fixture.failed_build = False
    fixture.failed_sdk = False
    fixture.selected_fault = payload.get('fault')
    result = fixture.external_process(command)
    if mode == 'validation' and (workspace / 'validation-ready').exists():
        if any(item.endswith('retention.py') for item in command):
            image = Path(command[command.index('--elf') + 1])
            if image.parents[1].name == 'h743 baremetal build':
                Path(command[command.index('--output') + 1]).write_text(
                    json.dumps({'status': 'failed'}))
    print(result.stdout, end='', flush=True)
    print(result.stderr, end='', file=sys.stderr, flush=True)
    raise SystemExit(result.returncode)
finally:
    event('end')
'''

    def setUp(self):
        self.fixture = SDKConsumerParallelTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.directory = self.fixture.directory
        self.output = self.fixture.output
        self.shim = self.directory / 'external_sdk_process.py'
        self.shim.write_text(self.SHIM)
        self.lock = threading.Lock()

    def real_execute(self, *, mode='overlap', jobs=2, fault=None):
        completed_failures = threading.Barrier(2)
        def external(command, **options):
            command = [str(argument) for argument in command]
            with self.lock:
                index = len(self.fixture.commands)
                self.fixture.commands.append(command)
            log = options.get('log', self.directory / f'old-port-{index}.log')
            payload = {'command': command, 'root': str(ROOT),
                'sdk': str(self.fixture.sdk), 'workspace': str(self.directory),
                'mode': mode, 'jobs': jobs, 'fault': fault}
            actual = [sys.executable, str(self.shim), json.dumps(payload)]
            selected = (command[0] == 'cmake' and '-S' in command and
                Path(command[command.index('-S') + 1]).name == 'native source')
            if mode == 'launch' and selected:
                actual = [str(self.directory / 'missing-external-tool')]
            timeout = 0.2 if mode == 'timeout' and selected else 8
            result = ACTUAL_PROCESS_RUN(actual,
                cwd=options.get('cwd', ROOT),
                env=options.get('env', os.environ.copy()), log=log,
                timeout=timeout, stage=options.get('stage', 'external-sdk-fixture'),
                cancel_event=options.get('cancel_event'))
            if mode == 'two-failures' and result.returncode in (42, 43):
                completed_failures.wait(timeout=5)
            return result
        with mock.patch.object(subprocess, 'run',
                               side_effect=external), \
                mock.patch.object(maintained_process, 'run',
                                  side_effect=external), \
                mock.patch.dict(os.environ, {'NEXUS_BUILD_JOBS': '99',
                    'CMAKE_BUILD_PARALLEL_LEVEL': '99'}), \
                contextlib.redirect_stderr(io.StringIO()):
            status = self.fixture.consumer.main([
                '--prefix', str(self.fixture.prefix), '--output', str(self.output),
                '--parallel', str(jobs)])
        return status, json.loads((self.output / 'verification.json').read_text())

    def events(self):
        return [json.loads(line) for line in
                (self.directory / 'events.jsonl').read_text().splitlines()]

    def assert_complete_execution(self, jobs):
        status, report = self.real_execute(jobs=jobs)
        self.assertEqual(status, 0, report)
        self.fixture.assert_complete_matrix()
        active, peak = set(), 0
        for event in sorted(self.events(), key=lambda row: row['time_ns']):
            self.assertEqual(event['nexus_jobs'], '1')
            self.assertEqual(event['cmake_jobs'], '1')
            if event['phase'] == 'start':
                active.add(event['pid'])
                peak = max(peak, len(active))
            else:
                active.remove(event['pid'])
            self.assertLessEqual(len(active), jobs)
        self.assertEqual(peak, jobs)
        self.assertEqual(active, set())
        self.assertEqual(report['status'], 'passed')
        self.assertEqual(report['parallel_jobs'], jobs)
        self.assertEqual(report['consumer_workers'], jobs)
        self.assertEqual(len(report['consumers']),
                         13 + len(self.fixture.SELECTED_INPUTS))
        self.assertTrue(all(row['status'] == 'passed'
                            for row in report['consumers']))
        self.assertEqual([row['name'] for row in report['selected_images']],
                         [name for name, _, _ in
                          self.fixture.consumer.SELECTED_CONSUMERS])
        keys = [(row['consumer_index'], row['phase_index'])
                for row in report['commands']]
        self.assertEqual(keys, sorted(keys))
        self.assertEqual(len(keys), len(set(keys)))
        logs = [row['log'] for row in report['commands']]
        self.assertEqual(len(logs), len(set(logs)))
        for row in report['commands']:
            self.assertEqual(row['allocated_jobs'], 1)
            self.assertEqual(row['log'], 'command-{:02d}-{:02d}.log'.format(
                row['consumer_index'], row['phase_index']))
            receipt = json.loads((self.output / row['receipt']).read_text())
            self.assertEqual(receipt['exit_code'], row['exit_code'])
            self.assertEqual(receipt['outcome'], 'completed')
            self.assertTrue(receipt['withdrawal_complete'])
            for stream in ('stdout', 'stderr'):
                identity = receipt[stream]
                path = Path(identity['path'])
                self.assertEqual(path.stat().st_size, identity['bytes'])
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                                 identity['sha256'])

    def test_two_independent_chains_overlap_with_one_global_job_budget(self):
        self.assert_complete_execution(2)

    def test_three_independent_chains_overlap_without_nested_job_expansion(self):
        self.assert_complete_execution(3)

    def assert_withdrawn(self, pid):
        path = Path('/proc') / str(pid) / 'stat'
        if path.exists():
            state = path.read_text().rsplit(')', 1)[1].split()[0]
            self.assertIn(state, ('Z', 'X'), 'owned process remains executable')

    def test_first_failure_cancels_and_joins_peer_parent_and_grandchild(self):
        started = time.monotonic()
        status, report = self.real_execute(mode='fail-peer')
        self.assertEqual(status, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertLess(time.monotonic() - started, 6)
        self.assertEqual(report['first_failure']['exit_code'], 42)
        self.assertEqual({row['exit_code'] for row in report['commands']
                          if row['exit_code']}, {42, 130})
        self.assertFalse(any('gd32 source' in ' '.join(event['argv'])
                             for event in self.events()))
        self.assertEqual(sum(row['status'] != 'not_started'
                             for row in report['consumers']), 2)
        for name in ('blocked.pid', 'grandchild.pid'):
            self.assert_withdrawn(int((self.directory / name).read_text()))
        peer = next(row for row in report['commands'] if row['exit_code'] == 130)
        receipt = json.loads((self.output / peer['receipt']).read_text())
        self.assertEqual(receipt['outcome'], 'cancelled')
        self.assertTrue(receipt['withdrawal_complete'])
        self.assertGreaterEqual(len(receipt['withdrawn_processes']), 2)
        self.assertIn('blocked consumer parent and grandchild',
                      (self.output / peer['log']).read_text())

    def test_actual_timeout_and_launch_failure_retain_truthful_results(self):
        for mode, code, outcome in (('timeout', 124, 'timed_out'),
                                     ('launch', 127, 'launch_failed')):
            with self.subTest(mode=mode):
                status, report = self.real_execute(mode=mode, jobs=1)
                self.assertEqual(status, 1)
                self.assertEqual(report['status'], 'failed')
                row = report['commands'][-1]
                self.assertEqual(row['exit_code'], code)
                self.assertEqual(report['first_failure']['exit_code'], code)
                receipt = json.loads((self.output / row['receipt']).read_text())
                self.assertEqual(receipt['outcome'], outcome)
                self.assertTrue(receipt['withdrawal_complete'])
                if mode == 'timeout':
                    self.assert_withdrawn(int(
                        (self.directory / 'blocked.pid').read_text()))

    def test_two_completed_failures_are_both_retained_before_cancellation(self):
        status, report = self.real_execute(mode='two-failures')
        self.assertEqual(status, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual({row['exit_code'] for row in report['failures']},
                         {42, 43})
        self.assertIn(report['first_failure']['exit_code'], (42, 43))
        self.assertFalse(any('gd32 source' in ' '.join(event['argv'])
                             for event in self.events()))
        failed = [row for row in report['commands'] if row['exit_code']]
        self.assertEqual({row['exit_code'] for row in failed}, {42, 43})
        for row in failed:
            receipt = json.loads((self.output / row['receipt']).read_text())
            self.assertEqual(receipt['child_returncode'], row['exit_code'])
            self.assertEqual(receipt['outcome'], 'completed')
            self.assertTrue(receipt['withdrawal_complete'])

    def test_sigint_cancels_real_workers_before_final_failed_report(self):
        delivered = threading.Event()
        def interrupt():
            deadline = time.monotonic() + 5
            while not (self.directory / 'blocked.pid').exists():
                if time.monotonic() >= deadline:
                    return
                time.sleep(0.01)
            delivered.set()
            os.kill(os.getpid(), signal.SIGINT)
        notifier = threading.Thread(target=interrupt)
        notifier.start()
        try:
            status, report = self.real_execute(mode='interrupt')
        finally:
            notifier.join(timeout=6)
        self.assertTrue(delivered.is_set())
        self.assertFalse(notifier.is_alive())
        self.assertEqual(status, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['first_failure']['exit_code'], 130)
        self.assertTrue(all(row['withdrawal_complete']
                            for row in report['commands']))
        self.assert_withdrawn(int((self.directory / 'blocked.pid').read_text()))

    def test_pass_is_withheld_until_blocked_validation_rejects_and_joins(self):
        result = []
        errors = []
        def execute():
            try:
                result.append(self.real_execute(mode='validation'))
            except BaseException as error:
                errors.append(error)
        worker = threading.Thread(target=execute)
        worker.start()
        try:
            deadline = time.monotonic() + 5
            while not (self.directory / 'validation-ready').exists():
                if not worker.is_alive() or time.monotonic() >= deadline:
                    self.fail('retention validation did not reach its barrier')
                time.sleep(0.01)
            self.assertTrue(worker.is_alive())
            self.assertFalse((self.output / 'verification.json').exists())
        finally:
            (self.directory / 'validation-release').write_text('release')
            worker.join(timeout=8)
        self.assertFalse(worker.is_alive())
        self.assertEqual(errors, [])
        status, report = result[0]
        self.assertEqual(status, 1)
        self.assertEqual(report['status'], 'failed')
        failed = [row for row in report['consumers'] if row['status'] == 'failed']
        self.assertTrue(any(row['name'] == 'h743 baremetal' for row in failed))


if __name__ == "__main__":
    unittest.main()
