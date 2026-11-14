"""An explicit reference family uses package data without Python entry code."""
from pathlib import Path
import hashlib
import json
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, integration_package
from tools.configure import test_external_package as legacy_fixtures


class DataPackageTests(unittest.TestCase):
    def fixture(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        source = configure.ROOT / "tests/contracts/declarative_gd32f470"
        for name in ("package.json", "soc.json", "routes.json", "bindings.json",
                     "references.json", "assembly.toml", "board/board.json"):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, path)
        # This suite exercises resolution/static generation only. The actual
        # reference target independently compiles the real SPL-based C sources.
        (root / "CMakeLists.txt").write_text("# static configuration fixture\n")
        return root

    def test_reference_package_resolves_and_generates_without_a_python_module(self):
        root = self.fixture()
        result = configure.resolve_ir(root / "assembly.toml", test_support=True)
        self.assertEqual(result.part, "GD32F470ZGT6")
        self.assertFalse(result.physical_qualified)
        self.assertEqual({item.kind for item in result.controllers}, {"gpio", "uart"})
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "generated"
            configure.configure(root / "assembly.toml", output, test_support=True)
            source = (output / "bindings.c").read_text()
            identity = configure.load(output / "generator-identity.json")
        self.assertIn("nx_gd32_gpio_prepare", source)
        self.assertIn("nx_gd32_uart_prepare", source)
        self.assertIn("nx_gd32_uart_validate", source)
        self.assertIn("USART0_IRQHandler", source)
        self.assertFalse(list(root.rglob("*.py")))
        self.assertFalse(result.provider_context.descriptor.configuration_modules)
        self.assertFalse(any(item["path"].startswith("tools/configure/stm32f4/")
                             for item in identity["files"]))

    def test_reference_scope_is_not_an_implicit_production_platform(self):
        root = self.fixture()
        with self.assertRaisesRegex(ValueError, "[Rr]eference.*test support"):
            configure.resolve_ir(root / "assembly.toml")

    def test_fragments_share_the_selected_input_and_domain_policy_owner(self):
        root = self.fixture()
        document = json.loads((root / "bindings.json").read_bytes())
        uart = document["software"]["bindings"].pop()
        (root / "bindings.json").write_text(json.dumps(document))
        fragment = root / "serial.json"
        fragment.write_text(json.dumps({"schema_version": 1, "bindings": [uart],
            "domains": {"schema_version": 1,
                        "limits": {"uart_baud": [1200, 1000000]}}}))
        manifest = json.loads((root / "package.json").read_bytes())
        manifest["binding_files"] = ["serial.json"]
        (root / "package.json").write_text(json.dumps(manifest))
        result = configure.resolve_ir(root / "assembly.toml", test_support=True)
        self.assertEqual({item.kind for item in result.controllers}, {"gpio", "uart"})
        self.assertIn(fragment, result.provider_context.inputs)
        self.assertEqual(result.provider_context.domain_policies.LIMITS["uart_baud"],
                         (1200, 1000000))
        invalid = json.loads(fragment.read_bytes())
        invalid["domains"]["schema_version"] = 2
        fragment.write_text(json.dumps(invalid))
        with self.assertRaisesRegex(ValueError, "[Ss]chema"):
            configure.resolve_ir(root / "assembly.toml", test_support=True)

    def test_selected_input_digests_identify_the_bytes_the_provider_decoded(self):
        from tools.configure.data_provider import DataProvider
        root = self.fixture()
        path = root / "bindings.json"
        expected = hashlib.sha256(path.read_bytes()).hexdigest()
        construct = DataProvider.from_package
        def change(*arguments, **options):
            value = construct(*arguments, **options)
            path.write_bytes(path.read_bytes() + b"\n")
            return value
        with patch.object(DataProvider, "from_package", side_effect=change):
            context = integration_package.select(".", family="gd32f470_reference",
                root=configure.ROOT, assembly_directory=root, test_support=True,
                read=configure._SnapshotReader({}))
        self.assertEqual(dict(context.input_digests)[path], expected)

    def test_new_local_family_is_found_from_its_manifest_without_tool_registration(self):
        package = self.fixture()
        source = package / "platform"
        target = source / "soc/vendor/gd32f470_reference"
        target.parent.mkdir(parents=True)
        target.mkdir()
        for path in tuple(package.iterdir()):
            if path.name != "platform":
                shutil.move(str(path), target / path.name)
        assembly = target / "assembly.toml"
        assembly.write_text(assembly.read_text().replace('soc_package="."\n', ''))
        soc = json.loads((target / "soc.json").read_text())
        soc.pop("source_lock")
        (target / "soc.json").write_text(json.dumps(soc))
        manifest = json.loads((target / "package.json").read_text())
        manifest["dependencies"] = {}
        (target / "package.json").write_text(json.dumps(manifest))
        (source / "dependencies").mkdir()
        shutil.copyfile(configure.ROOT / "dependencies/platform-compatibility.json",
                        source / "dependencies/platform-compatibility.json")
        result = configure.resolve_ir(assembly, source, test_support=True)
        self.assertEqual(result.soc_family, "gd32f470_reference")
        self.assertFalse(result.provider_context.external)
        self.assertEqual(result.provider_context.directory, source)
        self.assertEqual(result.provider_context.source_directory, target)
        self.assertEqual(result.provider_context.descriptor.source_directory,
                         "soc/vendor/gd32f470_reference")
        self.assertFalse(result.provider_context.descriptor.configuration_modules)

    def test_python_provider_fixture_is_never_a_production_package(self):
        fixture = legacy_fixtures.ExternalPackageTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        package, _ = fixture.package()
        with patch.object(integration_package, "_load_provider",
                          side_effect=AssertionError("Executed Python provider")):
            with self.assertRaisesRegex(ValueError, "[Pp]roduction.*package.json"):
                integration_package.select(str(package), family="partner_model",
                    root=configure.ROOT, assembly_directory=package.parent)


if __name__ == "__main__":
    unittest.main()
