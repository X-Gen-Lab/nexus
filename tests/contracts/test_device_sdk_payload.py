"""SDK device components retain exactly their reviewed upstream closure."""
from pathlib import Path
import hashlib
import json
import sys
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as sdk
import verify_consumers as consumers
from tools.maintenance import cmsis_device_lock
from tools.configure.providers import descriptor


class DeviceSDKPayloadTests(unittest.TestCase):
    def test_shared_driver_sources_are_transportable_and_mandatory(self):
        root = "soc/stm32/stm32f4/drivers/ip/"
        required = {root + name for name in (
            "i2c_v1_async/i2c_v1_async.c", "i2c_v1_async/i2c_v1_async.h",
            "classic_can/can_controller.c", "classic_can/can_controller.h",
            "fs_device_pio/usb_device_controller.c", "fs_device_pio/usb_device_controller.h",
            "legacy_mac/eth_mac.c", "legacy_mac/eth_frames_mac.c",
            "advanced_timer/timer_capture_impl.h", "advanced_timer/advanced_pwm_impl.h")}
        self.assertLessEqual(required, set(sdk.REQUIRED_FILES))
        self.assertTrue(all(sdk.selected(name) for name in required))






    def test_native_time_payload_and_selected_consumers_are_explicit(self):
        payload = {
            "io/include/nexus/io/rtc.h", "io/include/nexus/io/lptim.h",
            "io/include/nexus/io/native/rtc_lptim_model.h",
            "tests/support/native/drivers/rtc_lptim_provider.h", "tests/support/native/drivers/rtc_lptim.c",
            "io/private/rtc_lptim_internal.h", "io/src/api/rtc.c", "io/src/api/lptim.c",
            "tests/support/native/board/board.json",
        }
        base = "cmake/package/verification/io_evolution/"
        for profile in ("pair", "empty"):
            payload.update((base + "native-time-" + profile + ".toml",
                            base + "consumers/native-time-" + profile + ".c"))
            self.assertIn(("native time " + profile, base + "native-time-" + profile + ".toml",
                           "native-time-" + profile), consumers.SELECTED_CONSUMERS)
        self.assertFalse(payload - set(descriptor.maintained("native", test_support=True).owned_payload))
        self.assertFalse(payload - set(sdk.required_files(ROOT, sdk.source_profile(host_support=True))))
        for name in payload:
            self.assertTrue(sdk.selected(name, profile=sdk.source_profile(host_support=True)), name)

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
                self.assertIn("-DNEXUS_TEST_SUPPORT=ON", command)
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






    def test_rehashed_omission_cannot_remove_new_provider_inputs(self):
        names = {"soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c",
                 "soc/stm32/stm32f4/private/stm32f407_system.h",
                 "tests/support/native/descriptor.py",
                 "tests/support/native/drivers/rtc_lptim.c"}
        required = (*sdk.REQUIRED_FILES, *sdk.HOST_SUPPORT_FILES)
        self.assertFalse(names - set(required),
                         "Missing immutable omission guard: " +
                         repr(sorted(names - set(required))))
        # Exercise the real early completeness verifier with a byte-consistent
        # rehashed map. Dependency contents are intentionally outside this
        # early metadata boundary; no downstream provenance check is mocked.
        with tempfile.TemporaryDirectory(prefix="required SDK omission ") as value:
            root = Path(value) / "prefix/share/nexus/src"
            files = {}
            for name in required:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"owned input\n")
                files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
            identity = {
                "schema": 2, "profile": sdk.source_profile(host_support=True), "kind": "nexus-installed-source-sdk",
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







if __name__ == "__main__":
    unittest.main()
