"""Explicit external integration admission and static assembly regressions."""
import dataclasses
import hashlib
import json
import os
from pathlib import Path
import py_compile
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from tools.configure import configure, experience, onboarding, selected_contracts


ROOT = configure.ROOT
LIFECYCLE_REFERENCE = (ROOT /
    "tests/contracts/external_provider_lifecycle")


class ExternalPackageTests(unittest.TestCase):
    """Use a software-only independent family, never invented silicon facts."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def package(self, name="partner", marker=1):
        package = self.root / name
        source = package / "soc"
        configuration = package / "configure"
        source.mkdir(parents=True)
        configuration.mkdir()
        family = "partner_model"
        soc = configure.load(ROOT / "soc/native/soc.json")
        soc["id"] = family
        (source / "soc.json").write_text(json.dumps(soc), encoding="utf-8")
        shutil.copyfile(ROOT / "soc/native/routes.json", source / "routes.json")
        (source / "CMakeLists.txt").write_text(
            "add_library(nexus_soc_partner_model INTERFACE)\n"
            "target_link_libraries(nexus_soc_partner_model INTERFACE Nexus::IO)\n",
            encoding="utf-8")
        for name in ("__init__.py", "contract.py", "bindings.py", "emission.py"):
            text = (ROOT / "soc/native/configure" / name).read_text(
                encoding="utf-8")
            text = text.replace('"native"', '"partner_model"')
            if name == "__init__.py":
                text += f"\nPACKAGE_TEST_MARKER = {marker}\n"
            (configuration / name).write_text(text, encoding="utf-8")
        manifest = dict(schema_version=1, interface_version=1,
                        id="partner-model", version="1.0.0", family=family,
                        vendor="partner", source_directory="soc",
                        configuration_directory="configure", inputs={})
        self.reseal(package, manifest)
        board = self.root / (package.name + "-board")
        board.mkdir()
        facts = configure.load(ROOT / "boards/native_reference/board.json")
        facts["id"] = "package-" + hashlib.sha256(
            package.name.encode()).hexdigest()[:12]
        facts["soc_family"] = family
        (board / "board.json").write_text(json.dumps(facts), encoding="utf-8")
        assembly = self.root / (package.name + ".toml")
        assembly.write_text(
            f'schema = 2\nboard_package = {json.dumps(board.name)}\n'
            f'soc_package = {json.dumps(package.name)}\nbackend = "native"\n'
            'clock = "model"\n[memory]\nmain_stack_bytes = 1024\n'
            '[gpio.indicator]\nbinding = "led0"\nmode = "output"\n',
            encoding="utf-8")
        return package, assembly

    @staticmethod
    def reseal(package, manifest=None):
        path = package / "nexus-soc.json"
        if manifest is None:
            manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["inputs"] = {
            value.relative_to(package).as_posix():
            hashlib.sha256(value.read_bytes()).hexdigest()
            for directory in ("soc", "configure")
            for value in sorted((package / directory).rglob("*"))
            if value.is_file()}
        path.write_text(json.dumps(manifest, sort_keys=True), encoding="utf-8")

    def manifest(self, package, **changes):
        path = package / "nexus-soc.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest.update(changes)
        path.write_text(json.dumps(manifest), encoding="utf-8")

    def test_independent_family_resolves_and_emits_static_factory(self):
        package, assembly = self.package()
        result = configure.resolve_ir(assembly)
        self.assertEqual(result.soc_family, "partner_model")
        self.assertEqual(result.provider_context.provider.PACKAGE_TEST_MARKER, 1)
        self.assertEqual(result.provider_context.directory, package)
        self.assertEqual(result.provider_context.descriptor.vendor, "partner")
        self.assertFalse(result.physical_qualified)
        contract = selected_contracts.describe(result.controllers[0])
        self.assertTrue(contract.model)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            result.provider_context.directory = ROOT
        output = self.root / "generated"
        configure.configure(assembly, output)
        self.assertIn("nx_factory_gpio", (output / "bindings.c").read_text())
        self.assertIn(str(package / "soc"),
                      (output / "selection.cmake").read_text())
        paths = json.loads((output / "input_paths.json").read_text())
        self.assertIn(str(package / "nexus-soc.json"), paths.values())
        code = json.loads((output / "configuration_code_paths.json").read_text())
        self.assertIn(str(package / "configure/__init__.py"), code.values())

    def test_same_family_in_two_packages_never_reuses_other_provider(self):
        first_package, first = self.package("one", 1)
        second_package, second = self.package("two", 2)
        left, right = configure.resolve_ir(first), configure.resolve_ir(second)
        self.assertEqual(left.provider_context.provider.PACKAGE_TEST_MARKER, 1)
        self.assertEqual(right.provider_context.provider.PACKAGE_TEST_MARKER, 2)
        self.assertIsNot(left.provider_context.provider,
                         right.provider_context.provider)
        self.assertNotEqual(left.configuration_sha256, right.configuration_sha256)
        self.assertEqual(left.provider_context.directory, first_package)
        self.assertEqual(right.provider_context.directory, second_package)

    def test_manifest_unknown_field_and_interface_version_reject(self):
        for change, message in (({"unknown": True}, "unknown"),
                                ({"interface_version": 2}, "interface")):
            with self.subTest(change=change):
                package, assembly = self.package("case" + str(len(list(self.root.iterdir()))))
                self.manifest(package, **change)
                with self.assertRaisesRegex(ValueError, message):
                    configure.resolve_ir(assembly)

    def test_builtin_family_cannot_be_shadowed(self):
        package, assembly = self.package()
        self.manifest(package, family="native")
        with self.assertRaisesRegex(ValueError, "shadow|built-in"):
            configure.resolve_ir(assembly)

    def test_changed_input_is_rejected_before_loading_provider(self):
        package, assembly = self.package()
        entry = package / "configure/__init__.py"
        entry.write_text('raise RuntimeError("unverified code executed")\n')
        with self.assertRaisesRegex(ValueError, "digest|SHA256|identity"):
            configure.resolve_ir(assembly)

    def test_missing_required_input_is_rejected(self):
        package, assembly = self.package()
        path = package / "nexus-soc.json"
        value = json.loads(path.read_text())
        del value["inputs"]["soc/CMakeLists.txt"]
        self.manifest(package, inputs=value["inputs"])
        with self.assertRaisesRegex(ValueError, "required|CMakeLists"):
            configure.resolve_ir(assembly)

    def test_declared_input_cannot_escape_package(self):
        package, assembly = self.package()
        path = package / "nexus-soc.json"
        value = json.loads(path.read_text())
        value["inputs"]["../outside.py"] = "0" * 64
        self.manifest(package, inputs=value["inputs"])
        with self.assertRaisesRegex(ValueError, "path|escape|relative"):
            configure.resolve_ir(assembly)

    @unittest.skipUnless(hasattr(os, "symlink"), "host has no symlink support")
    def test_selected_package_and_input_symlinks_reject(self):
        package, assembly = self.package()
        link = self.root / "package-link"
        link.symlink_to(package, target_is_directory=True)
        original = assembly.read_text()
        assembly.write_text(original.replace('soc_package = "partner"',
                                             'soc_package = "package-link"'))
        with self.assertRaisesRegex(ValueError, "symlink"):
            configure.resolve_ir(assembly)
        assembly.write_text(original)
        source = package / "soc/routes.json"
        relocated = self.root / "routes-copy.json"
        source.rename(relocated)
        source.symlink_to(relocated)
        with self.assertRaisesRegex(ValueError, "symlink"):
            configure.resolve_ir(assembly)

    def test_generation_cannot_withdraw_selected_package(self):
        package, assembly = self.package()
        original = (package / "soc/soc.json").read_bytes()
        with self.assertRaisesRegex(ValueError, "overlap|input|package|protected"):
            configure.configure(assembly, package)
        self.assertEqual((package / "soc/soc.json").read_bytes(), original)

    def test_binding_views_and_provider_check_use_selected_package(self):
        _, assembly = self.package()
        result = configure.resolve_ir(assembly)
        facts = experience.binding_facts(configure, result, ROOT)
        view = experience.bindings_view(facts, kind="gpio")
        self.assertEqual(view["selection"]["family"], "partner_model")
        self.assertEqual(onboarding.check(result)["family"], "partner_model")
        self.assertIn("Physical verification: not_applicable",
                      experience.describe(result))

    def test_resealed_helper_does_not_reuse_same_timestamp_bytecode(self):
        package, assembly = self.package()
        entry = package / "configure/__init__.py"
        entry.write_text(entry.read_text() +
                         "\nfrom .helper import PACKAGE_TEST_MARKER\n")
        helper = package / "configure/helper.py"
        helper.write_text("PACKAGE_TEST_MARKER = 1\n")
        py_compile.compile(str(helper), doraise=True)
        before = helper.stat()
        self.reseal(package)
        self.assertEqual(configure.resolve_ir(assembly).provider_context.provider.
                         PACKAGE_TEST_MARKER, 1)
        helper.write_text("PACKAGE_TEST_MARKER = 2\n")
        os.utime(helper, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.reseal(package)
        self.assertEqual(configure.resolve_ir(assembly).provider_context.provider.
                         PACKAGE_TEST_MARKER, 2)

    def test_undeclared_helper_rejects_before_its_code_executes(self):
        package, assembly = self.package()
        entry = package / "configure/__init__.py"
        entry.write_text(entry.read_text() + "\nfrom . import helper\n")
        self.reseal(package)
        (package / "configure/helper.py").write_text(
            'raise RuntimeError("undeclared code executed")\n')
        with self.assertRaisesRegex(ValueError, "undeclared"):
            configure.resolve_ir(assembly)

    def test_external_source_target_configures_links_and_runs(self):
        """Execute a package-owned C adapter, not a generated-source snapshot."""
        consumer = self.root / "consumer"
        shutil.copytree(LIFECYCLE_REFERENCE, consumer)
        build = self.root / "build"
        report = build / "external-lifecycle-google.xml"
        commands = (
            ["cmake", "-S", str(consumer), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Debug",
             "-DNEXUS_REFERENCE_PLATFORM_SOURCE=" + str(ROOT),
             "-DEXTERNAL_LIFECYCLE_HOST_TESTS=ON",
             "-DNEXUS_REFERENCE_GOOGLETEST_SOURCE=" + str(ROOT / "ext/googletest")],
            ["cmake", "--build", str(build), "--target",
             "external_lifecycle_consumer", "external_lifecycle_contract_test",
             "--parallel", "1"],
            [str(build / "external_lifecycle_contract_test"),
             "--gtest_output=xml:" + str(report)],
            [str(build / "bin/external_lifecycle_consumer")])
        environment = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
        for name in ("GTEST_FILTER", "GTEST_TOTAL_SHARDS", "GTEST_SHARD_INDEX",
                     "GTEST_SHARD_STATUS_FILE", "GTEST_ALSO_RUN_DISABLED_TESTS"):
            environment.pop(name, None)
        for command in commands:
            execution = subprocess.run(command, capture_output=True, text=True,
                                       timeout=180, env=environment)
            self.assertEqual(execution.returncode, 0,
                             execution.stdout + execution.stderr)
        results = ET.parse(report).getroot()
        self.assertEqual(int(results.attrib["tests"]), 4)
        self.assertEqual(int(results.attrib["failures"]), 0)
        self.assertEqual(int(results.attrib["errors"]), 0)
        self.assertEqual(int(results.attrib["disabled"]), 0)
        self.assertFalse(results.findall(".//skipped"))
        entries = json.loads((build / "compile_commands.json").read_text())
        adapter = consumer / "provider/soc/lifecycle_adapter.c"
        self.assertEqual(sum(Path(row["file"]).resolve() == adapter
                             for row in entries), 1)
        manifest = json.loads((consumer / "provider/nexus-soc.json").read_text())
        self.assertEqual(manifest["inputs"]["soc/lifecycle_adapter.c"],
                         hashlib.sha256(adapter.read_bytes()).hexdigest())

    def test_package_parent_with_cmake_metacharacters_rejects(self):
        for character in ('$', ';', '"'):
            with self.subTest(character=character):
                package, assembly = self.package("parent" + character)
                with self.assertRaisesRegex(ValueError, "CMake path"):
                    configure.resolve_ir(assembly)

    def test_package_path_with_spaces_is_supported(self):
        package, assembly = self.package("package with spaces")
        board = self.root / "package with spaces-board/board.json"
        content = json.loads(board.read_text())
        content["id"] = "space-reference"
        board.write_text(json.dumps(content))
        result = configure.resolve_ir(assembly)
        self.assertEqual(result.provider_context.directory, package)

    def test_deferred_declared_helper_uses_verified_source(self):
        package, assembly = self.package()
        helper = package / "configure/helper.py"
        helper.write_text("PACKAGE_TEST_MARKER = 42\n")
        entry = package / "configure/__init__.py"
        entry.write_text(entry.read_text() +
                         "\ndef deferred_marker():\n"
                         "    from .helper import PACKAGE_TEST_MARKER\n"
                         "    return PACKAGE_TEST_MARKER\n")
        self.reseal(package)
        result = configure.resolve_ir(assembly)
        self.assertEqual(result.provider_context.provider.deferred_marker(), 42)

    def test_deferred_undeclared_helper_cannot_use_filesystem_fallback(self):
        package, assembly = self.package()
        entry = package / "configure/__init__.py"
        entry.write_text(entry.read_text() +
                         "\ndef deferred_marker():\n"
                         "    from .helper import PACKAGE_TEST_MARKER\n"
                         "    return PACKAGE_TEST_MARKER\n")
        self.reseal(package)
        (package / "configure/helper.py").write_text("PACKAGE_TEST_MARKER = 42\n")
        result = configure.resolve_ir(assembly)
        with self.assertRaises((ImportError, ValueError)):
            result.provider_context.provider.deferred_marker()

    def test_enhanced_spi_owner_uses_external_selected_context(self):
        _, assembly = self.package()
        assembly.write_text(assembly.read_text().replace(
            'backend = "native"',
            'backend = "native"\ncomponents = ["spi-dma-owner"]') +
            '\n[spi.bus]\nbinding = "spi0"\nmode = "async-model"\n'
            'max_hz = 1000000\n[spi_device.device]\ncontroller = "bus"\n'
            'driver = "spi-dma-transaction"\n')
        output = self.root / "enhanced-generated"
        configure.configure(assembly, output)
        self.assertIn("nx_factory_spi_dma_owner_binding",
                      (output / "bindings.c").read_text())

    def test_external_sources_do_not_reinterpret_nexus_internal_source_lock(self):
        package, assembly = self.package()
        path = package / "soc/soc.json"
        facts = json.loads(path.read_text())
        facts["source_lock"] = "vendor/source.lock.json"
        path.write_text(json.dumps(facts))
        self.reseal(package)
        with self.assertRaisesRegex(ValueError, "External.*inputs"):
            configure.resolve_ir(assembly)

    def test_relocation_across_platform_path_order_preserves_identity(self):
        parent = self.root
        platform = parent / "m-platform"
        (platform / "dependencies").mkdir(parents=True)
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json",
                        platform / "dependencies/platform-compatibility.json")
        self.root = parent / "a-product"
        self.root.mkdir()
        _, assembly = self.package()
        original = configure.resolve_ir(assembly, root=platform)
        relocated = parent / "z-product"
        shutil.copytree(self.root, relocated)
        moved = configure.resolve_ir(relocated / assembly.name, root=platform)
        self.assertNotEqual(original.provider_context.directory,
                            moved.provider_context.directory)
        self.assertEqual(sorted(original.to_dict()["inputs"],
                                key=lambda record: record["path"]),
                         sorted(moved.to_dict()["inputs"],
                                key=lambda record: record["path"]))
        self.assertEqual(original.to_dict(), moved.to_dict())


if __name__ == "__main__":
    unittest.main()
