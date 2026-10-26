"""SDK device components retain exactly their reviewed upstream closure."""
from tools.configure.providers.descriptor import source_directory
from pathlib import Path
import hashlib
import json
import re
import sys
import shutil
import subprocess
import tempfile
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as sdk
import verify_consumers as consumers
from tools.maintenance import cmsis_device_lock
from tools.configure.providers import descriptor


class DeviceSDKPayloadTests(unittest.TestCase):
    def test_shared_driver_sources_are_transportable_and_mandatory(self):
        required = {
            "drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c",
            "drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.h",
            "drivers/i2c/i2c_v1_async/i2c_v1_async.c",
            "drivers/i2c/i2c_v1_async/i2c_v1_async.h",
            "drivers/i2c/i2c_v2/i2c_v2.c",
            "drivers/i2c/i2c_v2/i2c_v2.h",
            "drivers/adc/legacy_adc/legacy_adc.c",
            "drivers/adc/legacy_adc/legacy_adc.h",
            "drivers/flash/legacy_flash/legacy_flash.c",
            "drivers/flash/legacy_flash/legacy_flash.h",
            "drivers/timer/legacy_pwm/legacy_pwm.c",
            "drivers/timer/legacy_pwm/legacy_pwm.h",
        }
        for name in sorted(required):
            with self.subTest(source=name):
                self.assertTrue(sdk.selected(name), name)
                self.assertTrue(name in sdk.REQUIRED_FILES, name)

    def test_runtime_clock_helper_and_actual_consumer_are_mandatory(self):
        names = {"tools/configure/runtime_power.py",
                 "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml",
                 "cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c"}
        self.assertFalse(names - set(sdk.REQUIRED_FILES))
        owned = set(descriptor.maintained("stm32h743").owned_payload)
        self.assertFalse((names - {"tools/configure/runtime_power.py"}) - owned)
        for name in names:
            self.assertTrue(sdk.selected(name), name)
        self.assertIn("cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c",
                      sdk.IO_EVOLUTION_VERIFICATION_FILES)

    def test_h743_spi_canonical_inputs_and_shared_smoke_are_required(self):
        names = ["tools/configure/assemblies/h743-" + variant + ".toml"
                 for variant in ("spi-dma", "spi-dma-uart-tx",
                                 "spi-dma-uart-rx")]
        smoke = ("cmake/package/verification/io_evolution/consumers/"
                 "h743-spi-dma.c")
        for name in [*names, smoke]:
            with self.subTest(path=name):
                self.assertIn(name, descriptor.maintained("stm32h743").owned_payload)
                self.assertIn(name, sdk.REQUIRED_FILES)
                self.assertTrue(sdk.selected(name))
        self.assertIn(smoke, sdk.IO_EVOLUTION_VERIFICATION_FILES)
        for name in names:
            self.assertNotIn(name, sdk.IO_EVOLUTION_VERIFICATION_FILES)

    def test_latest_selected_inputs_survive_the_owned_sdk_payload(self):
        rows = (
            ("stm32h743", "h743 spi dma cache on",
             "h743-spi-dma-cache-on", "h743-spi-dma"),
            ("stm32h743", "h743 uart continuous",
             "h743-uart-continuous", "h743-uart-continuous"),
            ("stm32h743", "h743 uart continuous cache on",
             "h743-uart-continuous-cache-on", "h743-uart-continuous"),
            ("stm32u575", "u575 uart chain",
             "u575-uart-chain", "u575-uart-gpdma"),
            ("stm32u575", "u575 lptim run", "u575-lptim-run", "u575-lptim"),
            ("stm32u575", "u575 rtc run", "u575-rtc-run", "u575-rtc"),
        )
        for family, label, stem, smoke in rows:
            with self.subTest(consumer=label), \
                    tempfile.TemporaryDirectory(prefix="SDK ") as value:
                canonical = "tools/configure/assemblies/" + stem + ".toml"
                template = ("cmake/package/verification/io_evolution/consumers/"
                            + smoke + ".c")
                self.assertIn((label, canonical, smoke),
                              consumers.SELECTED_CONSUMERS)
                self.assertIn(canonical,
                              descriptor.maintained(family).owned_payload)
                self.assertTrue(template in sdk.IO_EVOLUTION_VERIFICATION_FILES,
                                template)
                directory = Path(value)
                prefix = directory / "prefix with spaces"
                installed = prefix / "share/nexus/src"
                for name in (canonical, template):
                    self.assertTrue((ROOT / name).is_file(), name)
                    self.assertIn(name, sdk.REQUIRED_FILES)
                    self.assertTrue(sdk.selected(name), name)
                for name in (canonical, template,
                             "dependencies/platform-compatibility.json"):
                    destination = installed / name
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / name, destination)
                source, _, command = consumers.prepare_selected_consumer(
                    prefix, directory, label, canonical, smoke,
                    lambda argv: None, expected_revision="b" * 40)
                self.assertEqual((source / "main.c").read_bytes(),
                                 (installed / template).read_bytes())
                self.assertIn("-DNEXUS_ASSEMBLY_FILE=" +
                              str(installed / canonical), command)

    def test_named_pin_state_sdk_payload_and_canonical_plan_are_explicit(self):
        canonical = "tools/configure/assemblies/g474-pin-states.toml"
        directory = "cmake/package/verification/io_evolution/"
        required = {
            "io/include/nexus/io/pin_state.h", "io/src/api/pin_state.c",
            "io/private/pin_state_impl.h", "tools/configure/pin_states.py",
            "soc/stm32/stm32g474/drivers/pin_state.c",
            "soc/stm32/stm32g474/private/stm32g474_pin_state.h",
            canonical, directory + "boards/g474-pin-states/board.json",
            directory + "consumers/pin-states.c",
        }
        self.assertFalse(required - set(sdk.REQUIRED_FILES))
        for name in required:
            with self.subTest(path=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(sdk.selected(name))
        self.assertIn(("g474 pin states", canonical, "pin-states"),
                      consumers.SELECTED_CONSUMERS)
        from tools.release import transport
        self.assertIs(consumers.SELECTED_CONSUMERS, transport.SELECTED_CONSUMERS)
        self.assertNotIn(canonical, sdk.IO_EVOLUTION_VERIFICATION_FILES)
        for relative in ("boards/g474-pin-states/board.json", "consumers/pin-states.c"):
            self.assertIn(directory + relative, sdk.IO_EVOLUTION_VERIFICATION_FILES)

    def test_named_pin_state_installed_template_uses_public_canonical_consumer(self):
        canonical = Path("tools/configure/assemblies/g474-pin-states.toml")
        smoke = Path("cmake/package/verification/io_evolution/consumers/pin-states.c")
        with tempfile.TemporaryDirectory(prefix="Pin state SDK ") as value:
            root = Path(value)
            prefix = root / "prefix with spaces"
            installed = prefix / "share/nexus/src"
            for relative in (Path("dependencies/platform-compatibility.json"), canonical, smoke):
                target = installed / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, target)
            source, _, command = consumers.prepare_selected_consumer(
                prefix, root, "g474 pin states", canonical.as_posix(), "pin-states",
                lambda argv: None, expected_revision="b" * 40)
            self.assertEqual((source / "main.c").read_bytes(), (ROOT / smoke).read_bytes())
            main = (source / "main.c").read_text()
            for operation in ("nx_factory_pin_state", "NX_PIN_STATE_SLEEP",
                              "NX_PIN_STATE_DEFAULT", "nx_platform_stop"):
                self.assertIn(operation, main)
            self.assertNotIn("stm32g474", main)
            private = (source / "private_headers.c").read_text()
            self.assertIn('__has_include("stm32g474_pin_state.h")', private)
            self.assertIn('__has_include("pin_state_impl.h")', private)
            self.assertIn("-DNEXUS_ASSEMBLY_FILE=" + str(installed / canonical), command)
            self.assertTrue(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))

    def test_native_time_payload_and_selected_consumers_are_explicit(self):
        payload = {
            "io/include/nexus/io/rtc.h", "io/include/nexus/io/lptim.h",
            "io/include/nexus/io/native/rtc_lptim_model.h",
            "soc/native/drivers/rtc_lptim_provider.h", "soc/native/drivers/rtc_lptim.c",
            "io/private/rtc_lptim_internal.h", "io/src/api/rtc.c", "io/src/api/lptim.c",
            "boards/native_reference/board.json",
        }
        base = "cmake/package/verification/io_evolution/"
        for profile in ("pair", "empty"):
            payload.update((base + "native-time-" + profile + ".toml",
                            base + "consumers/native-time-" + profile + ".c"))
            self.assertIn(("native time " + profile, base + "native-time-" + profile + ".toml",
                           "native-time-" + profile), consumers.SELECTED_CONSUMERS)
        self.assertFalse(payload - set(descriptor.maintained("native").owned_payload))
        self.assertFalse(payload - set(sdk.REQUIRED_FILES))
        for name in payload:
            self.assertTrue(sdk.selected(name), name)

    def test_native_selected_template_does_not_cross_compile_host_model(self):
        with tempfile.TemporaryDirectory(prefix="Native SDK template ") as value:
            root = Path(value)
            prefix = root / "prefix with spaces"
            installed = prefix / "share/nexus/src"
            policy = installed / "dependencies/platform-compatibility.json"
            policy.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
            canonical = Path("cmake/package/verification/io_evolution")
            for profile in ("pair", "empty"):
                for relative in (canonical / ("native-time-" + profile + ".toml"),
                                 canonical / "consumers" / ("native-time-" + profile + ".c")):
                    target = installed / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / relative, target)
                source, _, command = consumers.prepare_selected_consumer(
                    prefix, root, "native time " + profile,
                    (canonical / ("native-time-" + profile + ".toml")).as_posix(),
                    "native-time-" + profile,
                    lambda argv: None, expected_revision="a" * 40)
                self.assertFalse(any("CMAKE_TOOLCHAIN_FILE" in arg for arg in command))
                self.assertIn('rtc_lptim_provider.h',
                              (source / "private_headers.c").read_text())
                self.assertEqual((source / "main.c").read_bytes(),
                                 (installed / canonical / "consumers" /
                                  ("native-time-" + profile + ".c")).read_bytes())
                self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "a" * 40, command)

    def native_image_fixture(self, root, profile):
        """Format inputs plus external process oracle, never a built ELF claim."""
        installed = root / "prefix/share/nexus/src"
        source, build, config = root / "source", root / "build", root / "config"
        for directory in (installed, source, build / "bin", config):
            directory.mkdir(parents=True)
        canonical = Path("cmake/package/verification/io_evolution")
        for item in (canonical / ("native-time-" + profile + ".toml"),
                     canonical / "consumers" / ("native-time-" + profile + ".c")):
            target = installed / item
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / item, target)
        (source / "main.c").write_bytes((installed / canonical / "consumers" /
                                        ("native-time-" + profile + ".c")).read_bytes())
        (source / "private_headers.c").write_text("_Static_assert(1, \"public\");\n")
        (build / "resolved-sdk.txt").write_text(str(installed))
        (build / "resolved-config.txt").write_text(str(config))
        rows = [{"file": str(source / name), "directory": str(build),
                 "command": "cc -c " + str(source / name)}
                for name in ("main.c", "private_headers.c")]
        rows.append({"file": str(config / "bindings.c"), "directory": str(build),
                     "command": "cc -c " + str(config / "bindings.c")})
        (build / "compile_commands.json").write_text(json.dumps(rows))
        controllers = []
        if profile == "pair":
            for kind, names, mode in (("rtc", ("calendar_a", "calendar_b"),
                                       "calendar-alarm"),
                                      ("lptim", ("counter_a", "counter_b"),
                                       "counter16-oneshot")):
                for index, name in enumerate(names):
                    entry = {"kind": kind, "id": name, "mode": mode}
                    if kind == "lptim":
                        entry["tick_hz"] = (32768, 1000)[index]
                    controllers.append(entry)
        (config / "resolved.json").write_text(json.dumps({
            "backend": "native", "soc_family": "native",
            "cpu": {"arch": "native"}, "controllers": controllers}))
        (config / "bindings.c").write_text("/* explicit unit format input */\n")
        (installed / ".nexus-source-sdk.json").write_text(json.dumps({
            "snapshot_sha256": "a" * 64}))
        header = bytearray(64)
        header[:6] = b"\x7fELF\x02\x01"
        header[18:20] = (62).to_bytes(2, "little")
        (build / "bin/installed_selected").write_bytes(header)
        symbols = ""
        if profile == "pair":
            symbols = ("nx_native_rtc_ops D 2000 30\n"
                       "nx_native_lptim_ops D 2030 28\n"
                       "nx_native_time_clock_read t 1000 5\n"
                       "s_nx_native_time_clock d 2060 10\n" +
                       "".join("s_nx_port_" + entry["id"] + " b 3000 40\n"
                               for entry in controllers))
        calls = []
        def external(argv):
            calls.append([str(arg) for arg in argv])
            return subprocess.CompletedProcess(argv, 0,
                symbols if str(argv[0]) == "nm" else "", "")
        return installed, source, build, config, symbols, calls, external

    def test_native_selected_checker_runs_host_and_observes_only_selected_symbols(self):
        for profile, count in (("pair", 4), ("empty", 0)):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as value:
                installed, source, build, _, _, calls, external = \
                    self.native_image_fixture(Path(value), profile)
                report = consumers.check_selected_image(installed, source, build, external)
                self.assertEqual(report["kind"], "native-time-host-observation")
                self.assertEqual(report["selected_state_count"], count)
                self.assertEqual(report["physical_status"], "not_executed")
                self.assertIn([str(build / "bin/installed_selected")], calls)
                self.assertTrue(any(row[0] == "nm" for row in calls))
                self.assertFalse(any("retention.py" in " ".join(row) for row in calls))
                self.assertFalse((build / "bin/installed_selected.elf").exists())

    def test_native_selected_checker_rejects_wrong_source_domain_and_retention(self):
        for defect in ("source", "domain", "missing-state", "empty-leak"):
            with self.subTest(defect=defect), tempfile.TemporaryDirectory() as value:
                profile = "empty" if defect == "empty-leak" else "pair"
                installed, source, build, config, symbols, _, external = \
                    self.native_image_fixture(Path(value), profile)
                if defect == "source":
                    (source / "main.c").write_text("int main(void) { return 0; }\n")
                elif defect == "domain":
                    p = config / "resolved.json"
                    data = json.loads(p.read_text())
                    data["controllers"][0]["mode"] = "hardware-irq"
                    p.write_text(json.dumps(data))
                else:
                    if defect == "missing-state":
                        symbols = "\n".join(line for line in symbols.splitlines()
                                            if not line.startswith("s_nx_port_counter_b "))
                    else:
                        symbols = "nx_native_rtc_ops D 2000 30\n"
                    def external(argv):
                        return subprocess.CompletedProcess(argv, 0,
                            symbols if str(argv[0]) == "nm" else "", "")
                with self.assertRaises(ValueError):
                    consumers.check_selected_image(installed, source, build, external)

    def test_rtscts_canonical_inputs_and_boards_are_owned_sdk_requirements(self):
        for family, stem in (("stm32h743", "h743"),
                             ("stm32u575", "u575")):
            board = f"boards/{family}zi_rtscts_reference/board.json"
            assembly = ("cmake/package/verification/io_evolution/" +
                        stem + "-uart-rtscts.toml")
            with self.subTest(family=family):
                self.assertIn(board, descriptor.maintained(family).owned_payload)
                for name in (assembly, board):
                    self.assertTrue((ROOT / name).is_file())
                    self.assertIn(name, sdk.REQUIRED_FILES)
                    self.assertTrue(sdk.selected(name))
                self.assertIn((stem + " rtscts", assembly,
                               "uart-fifo"), consumers.SELECTED_CONSUMERS)

    def test_shared_pwm_reference_is_an_explicit_public_sdk_consumer(self):
        directory = "cmake/package/verification/io_evolution/"
        for relative in ("g474-pwm-shared.toml",
                         "boards/g474-pwm-shared/board.json",
                         "consumers/g474-pwm-shared.c"):
            name = directory + relative
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertIn(name, sdk.REQUIRED_FILES)
                self.assertTrue(sdk.selected(name))
        self.assertIn(("g474 shared pwm", directory + "g474-pwm-shared.toml",
                       "g474-pwm-shared"), consumers.SELECTED_CONSUMERS)
        board = json.loads((ROOT / directory /
                            "boards/g474-pwm-shared/board.json").read_text())
        self.assertIsNone(board["physical_pcb_revision"])

    def test_selected_adc_and_rtscts_sources_are_owned_sdk_requirements(self):
        additions = {
            "stm32g474": ("drivers/adc_group.c",
                          "private/stm32g474_adc_group.h"),
            "stm32h743": ("drivers/uart_rtscts.c",
                          "private/stm32h743_uart_rtscts.h"),
            "stm32u575": ("drivers/uart_rtscts.c",
                          "private/stm32u575_uart_rtscts.h"),
        }
        for family, relative in additions.items():
            payload = set(descriptor.maintained(family).owned_payload)
            for name in relative:
                source = f"{source_directory(family)}/{name}"
                with self.subTest(source=source):
                    self.assertTrue((ROOT / source).is_file())
                    self.assertIn(source, payload)
                    self.assertIn(source, sdk.REQUIRED_FILES)
                    self.assertTrue(sdk.selected(source))

    def test_expanded_soc_ownership_is_complete_and_required_by_sdk(self):
        required = set(sdk.REQUIRED_FILES)
        for family in ("stm32f103", "gd32f303", "stm32g474",
                       "stm32h743", "stm32u575"):
            directory = ROOT / source_directory(family)
            # This independent boundary inventories owned firmware files; it
            # does not discover or select production translation units.
            expected = {path.relative_to(ROOT).as_posix()
                        for path in directory.rglob("*")
                        if path.is_file() and path.suffix in {".c", ".h", ".S"}}
            expected.add(f"{source_directory(family)}/CMakeLists.txt")
            payload = set(descriptor.maintained(family).owned_payload)
            with self.subTest(family=family):
                self.assertFalse(expected - payload,
                                 "Incomplete owned payload: " +
                                 repr(sorted(expected - payload)))
                self.assertFalse(expected - required,
                                 "SDK accepts truncated firmware: " +
                                 repr(sorted(expected - required)))

    def test_rehashed_omission_cannot_remove_new_provider_inputs(self):
        names = {
            "drivers/adc/legacy_adc/legacy_adc.c", "drivers/timer/legacy_pwm/legacy_pwm.c",
            "drivers/flash/legacy_flash/legacy_flash.c", "drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c",
            "drivers/i2c/i2c_v1_async/i2c_v1_async.h", "drivers/i2c/i2c_v2/i2c_v2.c",
            "soc/stm32/stm32g474/drivers/adc_stream.c",
            "soc/stm32/stm32g474/private/stm32g474_acquisition.h",
            "soc/stm32/stm32h743/memory_init.c",
            "soc/stm32/stm32h743/private/memory_init.h",
            "soc/stm32/stm32h743/drivers/adc.c", "soc/stm32/stm32h743/drivers/flash.c",
            "soc/stm32/stm32u575/secure_batch.c",
            "soc/stm32/stm32u575/private/stm32u575_secure_batch.h",
            "soc/stm32/stm32g474/drivers/adc_group.c",
            "soc/stm32/stm32g474/private/stm32g474_adc_group.h",
            "soc/stm32/stm32h743/drivers/uart_rtscts.c",
            "soc/stm32/stm32h743/private/stm32h743_uart_rtscts.h",
            "soc/stm32/stm32u575/drivers/uart_rtscts.c",
            "soc/stm32/stm32u575/private/stm32u575_uart_rtscts.h",
            "boards/stm32h743zi_rtscts_reference/board.json",
            "boards/stm32u575zi_rtscts_reference/board.json",
            "cmake/package/verification/io_evolution/h743-uart-rtscts.toml",
            "cmake/package/verification/io_evolution/u575-uart-rtscts.toml",
            "cmake/package/verification/io_evolution/g474-pwm-shared.toml",
            "cmake/package/verification/io_evolution/boards/g474-pwm-shared/board.json",
            "cmake/package/verification/io_evolution/consumers/g474-pwm-shared.c",
        }
        names.update(descriptor.maintained("native").owned_payload)
        names.update({"tools/configure/runtime_power.py",
                      "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml",
                      "cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c"})
        names.update({
            "tools/configure/assemblies/h743-spi-dma.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-tx.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-rx.toml",
            "cmake/package/verification/io_evolution/consumers/h743-spi-dma.c",
        })
        names.update(
            "vendors/gigadevice/gd32f30x/Firmware/"
            "GD32F30x_standard_peripheral/Include/gd32f30x_" + kind + ".h"
            for kind in ("adc", "dma", "exti", "i2c"))
        self.assertFalse(names - set(sdk.REQUIRED_FILES),
                         "Missing immutable omission guard: " +
                         repr(sorted(names - set(sdk.REQUIRED_FILES))))
        # Exercise the real early completeness verifier with a byte-consistent
        # rehashed map. Dependency contents are intentionally outside this
        # early metadata boundary; no downstream provenance check is mocked.
        with tempfile.TemporaryDirectory(prefix="required SDK omission ") as value:
            root = Path(value) / "prefix/share/nexus/src"
            files = {}
            for name in sdk.REQUIRED_FILES:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"owned input\n")
                files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
            identity = {
                "schema": 1, "kind": "nexus-installed-source-sdk",
                "source_revision": "a" * 40, "source_tree": "b" * 40,
                "publishable": False, "source_dirty": True,
                "files_sha256": files, "dependencies": [],
                "reviewed_imports": [],
                "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"],
                "snapshot_sha256": "", "package_files_sha256": {},
            }
            for name in sorted(names):
                with self.subTest(omitted=name):
                    target = root / name
                    original = target.read_bytes()
                    expected = files.pop(name)
                    target.unlink()
                    identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
                        files, sort_keys=True,
                        separators=(",", ":")).encode()).hexdigest()
                    (root / sdk.MANIFEST).write_text(json.dumps(identity) + "\n")
                    try:
                        with self.assertRaisesRegex(
                                sdk.SDKError, "missing required sources/licenses"):
                            sdk.verify(root)
                    finally:
                        target.write_bytes(original)
                        files[name] = expected

    def test_modern_consumer_inputs_are_explicit_and_use_only_relocated_sdk(self):
        with tempfile.TemporaryDirectory(prefix="modern consumer inputs ") as value:
            directory = Path(value)
            prefix = directory / "prefix"
            policy = prefix / "share/nexus/src/dependencies/platform-compatibility.json"
            policy.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
            for family, clock in (("h743", "hsi64-fixed-cache-off"),
                                  ("u575", "msis4"),
                                  ("f103", "hsi8-fixed"),
                                  ("gd303", "irc8"),
                                  ("g474", "hse24-pll170")):
                for backend in ("baremetal", "freertos"):
                    with self.subTest(family=family, backend=backend):
                        source, _, command = consumers.prepare_modern_consumer(
                            prefix, directory, family, backend, lambda argv: None,
                            expected_revision="a" * 40)
                        text = (source / "assembly.toml").read_text()
                        self.assertIn(f'backend = "{backend}"', text)
                        self.assertIn(f'clock = "{clock}"', text)
                        self.assertIn(str(prefix / "share/nexus/src/boards"), text)
                        self.assertNotIn(str(ROOT) + "/", text)
                        code = (source / "main.c").read_text()
                        self.assertIn("nx_factory_uart", code)
                        self.assertIn("nx_gpio_port_write", code)
                        authored = tomllib.loads(text)
                        board_name = Path(authored["board_package"]).name
                        board = json.loads((ROOT / "boards" / board_name /
                                            "board.json").read_text())
                        binding_id = authored["gpio"]["led"]["binding"]
                        binding = next(item for item in board["bindings"]
                                       if item["id"] == binding_id)
                        self.assertIn(authored["uart"]["console"]["binding"],
                                      {item["id"] for item in board["bindings"]})
                        match = re.search(r"nx_gpio_port_write\(led, 1U << (\d+)U", code)
                        self.assertIsNotNone(match)
                        self.assertEqual((1 << int(match[1])) & ~binding["mask"], 0,
                                         "Consumer write exceeds its Board pin lease")
                        self.assertEqual("vTaskStartScheduler" in code,
                                         backend == "freertos")
                        self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "a" * 40,
                                      command)
                        self.assertNotIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
            for family, backend in (("cortex-m7", "baremetal"),
                                    ("h743", "threadx")):
                with self.subTest(family=family, backend=backend):
                    with self.assertRaises(ValueError):
                        consumers.prepare_modern_consumer(
                            prefix, directory, family, backend, lambda argv: None)

    def test_new_soc_payload_requires_its_actual_owned_firmware_sources(self):
        required = set(sdk.REQUIRED_FILES)
        for family, board in (("stm32h743", "stm32h743_nucleo_zi2"),
                              ("stm32u575", "stm32u575_nucleo_zi_q"),
                              ("stm32f103", "stm32f103_reference"),
                              ("gd32f303", "gd32f303_reference"),
                              ("stm32g474", "stm32g474re_hse24_reference")):
            names = (f"{source_directory(family)}/CMakeLists.txt", f"{source_directory(family)}/system.c",
                     f"{source_directory(family)}/drivers/gpio.c", f"{source_directory(family)}/drivers/uart.c",
                     f"{source_directory(family)}/private/{family}_system.h",
                     f"{source_directory(family)}/private/{family}_provider.h",
                     f"boards/{board}/board.json")
            for name in names:
                with self.subTest(name=name):
                    self.assertTrue(name in required,
                                    "SDK accepted missing owned firmware source: " + name)

    def test_h743_pre_ram_power_boot_assembly_and_header_are_required(self):
        required = set(sdk.REQUIRED_FILES)
        for name in ("soc/stm32/stm32h743/power_boot.S",
                     "soc/stm32/stm32h743/private/boot_registers.h"):
            with self.subTest(name=name):
                self.assertTrue(name in required,
                                "SDK omitted the actual H743 pre-RAM startup input")

    def test_upstream_checkout_exports_only_the_exact_reviewed_component(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                entries = sdk.dependency_entries(ROOT, review["directory"])
                self.assertEqual(set(entries), review["files"])
                self.assertTrue(all(mode in {"100644", "100755"}
                                    and kind == "blob"
                                    for mode, kind, _ in entries.values()))

    def test_original_notice_lock_and_verifier_cannot_be_omitted_from_payload(self):
        required = set(sdk.REQUIRED_FILES)
        self.assertTrue("tools/maintenance/cmsis_device_lock.py" in required,
                        "Missing generic device source verifier payload")
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                self.assertTrue(review["lock_name"] in required,
                                "Missing exact device source lock payload")
                for name in review["files"]:
                    self.assertTrue(review["directory"] + "/" + name in required,
                                    "Missing original reviewed device source: " + name)


if __name__ == "__main__":
    unittest.main()
