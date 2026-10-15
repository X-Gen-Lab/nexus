"""Derived work packages never promote source navigation to qualification."""

import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
from types import ModuleType
import unittest
from unittest.mock import patch

from tools.maintenance import work_packages

ROOT = Path(__file__).resolve().parents[2]


class WorkPackageDerivationTests(unittest.TestCase):
    def setUp(self):
        self.source = "soc/example/drivers/uart.c"
        self.rows = [{"family": "example", "controller": "UART1",
                      "kind": "uart", "mode": "irq-byte-event",
                      "routes": ["uart1-pa2-pa3"], "boards": ["reference"],
                      "fixtures": ["tests/contracts/example_assembly/uart.toml"],
                      "exports": [self.source]}]
        self.properties = [{
            "id": "example.uart.completion", "family": "example",
            "peripheral": "uart", "mode": "irq-byte-event",
            "target": "nexus_example_uart_test", "case": "Uart.Completes",
            "test_source": "tests/google/example_uart_test.cpp",
            "production_sources": [self.source, "core/src/request.c"],
            "model_sources": [], "assertions": ["EXPECT_TRUE(done)",
                                                   "EXPECT_EQ(count, 1U)"],
            "calls": ["run()"], "invariant": "Final access precedes release.",
            "limitations": "Bounded host model; no physical timing proof.",
        }]
        self.providers = {"example": {
            "configuration_modules": ["tools/configure/providers/example/__init__.py"],
            "fact_files": ["soc/example/soc.json"],
            "git_dependencies": [], "import_lock": None,
        }}

    def derive(self):
        return work_packages.derive(
            self.rows, self.properties, self.providers, {self.source})

    def test_exact_scope_and_source_link_reviewed_contract_without_execution(self):
        report = self.derive()
        package = report["work_packages"][0]
        self.assertEqual(package["identity"], "example.UART1.irq-byte-event")
        self.assertEqual(package["implementation_status"], "source_selected")
        self.assertEqual(package["software_status"], "not_executed")
        self.assertEqual(package["hardware_status"], "not_executed")
        self.assertEqual(package["reviewed_properties"], self.properties)
        self.assertEqual(package["contract_status"], "reviewed_bounded")
        self.assertEqual(package["sdk_required_sources"], [self.source])
        self.assertFalse(report["software_qualification"])
        self.assertFalse(report["physical_qualification"])
        self.assertEqual(report["summary"], {
            "work_packages": 1, "source_selected": 1,
            "with_reviewed_contract": 1, "unmapped_properties": 0,
            "software_executed": 0, "hardware_executed": 0,
        })

    def test_nearby_mode_family_or_source_cannot_inherit_property(self):
        for field, value in (("mode", "dma-tx"), ("family", "other"),
                             ("peripheral", "spi"),
                             ("production_sources", ["soc/example/drivers/other.c"])):
            with self.subTest(field=field):
                properties = copy.deepcopy(self.properties)
                properties[0][field] = value
                report = work_packages.derive(
                    self.rows, properties, self.providers, {self.source})
                package = report["work_packages"][0]
                self.assertEqual(package["contract_status"], "not_catalogued")
                self.assertEqual(package["reviewed_properties"], [])
                self.assertEqual(report["unmapped_properties"], properties)

    def test_declared_mode_without_fixture_is_an_open_work_package(self):
        self.rows[0].update(fixtures=[], exports=[])
        package = self.derive()["work_packages"][0]
        self.assertEqual(package["implementation_status"], "not_selected_by_fixture")
        self.assertEqual(package["contract_status"], "not_catalogued")
        self.assertEqual(package["software_status"], "not_executed")
        self.assertEqual(package["hardware_status"], "not_executed")

    def test_selected_source_missing_from_sdk_required_contract_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "SDK required"):
            work_packages.derive(self.rows, self.properties, self.providers, set())

    def test_duplicate_work_package_cannot_hide_another_controller(self):
        self.rows.append(copy.deepcopy(self.rows[0]))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.derive()

    def test_source_without_resolver_fixture_is_rejected(self):
        self.rows[0]["fixtures"] = []
        with self.assertRaisesRegex(ValueError, "fixture"):
            self.derive()

    def test_missing_provider_is_not_guessed_from_family_name(self):
        self.providers.clear()
        with self.assertRaisesRegex(ValueError, "provider"):
            self.derive()

    def test_derivation_is_deterministic_and_does_not_mutate_authorities(self):
        second = copy.deepcopy(self.rows[0])
        second["controller"] = "UART2"
        self.rows.append(second)
        baseline = copy.deepcopy((self.rows, self.properties, self.providers))
        first = self.derive()
        self.rows.reverse()
        self.assertEqual(first, self.derive())
        self.rows.reverse()
        self.assertEqual((self.rows, self.properties, self.providers), baseline)


class WorkPackageCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.root = Path(self.storage.name)
        test = self.root / "tests/google/example_test.cpp"
        test.parent.mkdir(parents=True)
        test.write_text("TEST(Uart, Completes) { run(); EXPECT_TRUE(done); "
                        "EXPECT_EQ(count, 1U); }\n", encoding="utf-8")
        production = self.root / "soc/example/drivers/uart.c"
        production.parent.mkdir(parents=True)
        production.write_text("void run(void) {}\n", encoding="utf-8")
        fixtures = WorkPackageDerivationTests()
        fixtures.setUp()
        property_row = fixtures.properties[0]
        property_row.update(test_source="tests/google/example_test.cpp",
                            production_sources=["soc/example/drivers/uart.c"])
        self.catalogue = self.root / "catalogue.json"
        self.catalogue.write_text(json.dumps({
            "schema": 1, "scope": "Synthetic catalogue; no qualification.",
            "properties": [property_row],
            "unproven": [{"scope": "physical", "reason": "No equipment."}],
        }), encoding="utf-8")

    def test_uses_existing_reviewed_case_and_source_validator(self):
        data, sources = work_packages.reviewed_catalogue(self.root, self.catalogue)
        self.assertEqual(len(data["properties"]), 1)
        self.assertIn("tests/google/example_test.cpp", sources)
        self.assertIn("soc/example/drivers/uart.c", sources)

    def test_missing_assertions_or_production_source_fail_before_reporting(self):
        for relative in ("tests/google/example_test.cpp", "soc/example/drivers/uart.c"):
            path = self.root / relative
            original = path.read_bytes()
            with self.subTest(relative=relative):
                path.write_text("/* A declaration is not a reviewed test. */\n")
                if relative.endswith(".cpp"):
                    with self.assertRaises(ValueError):
                        work_packages.reviewed_catalogue(self.root, self.catalogue)
                else:
                    path.unlink()
                    with self.assertRaises((OSError, ValueError)):
                        work_packages.reviewed_catalogue(self.root, self.catalogue)
                path.write_bytes(original)

    def test_duplicate_json_keys_do_not_replace_an_authority(self):
        contents = self.catalogue.read_text()
        self.catalogue.write_text(contents.replace('"schema": 1',
                                                  '"schema": 0, "schema": 1'))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            work_packages.reviewed_catalogue(self.root, self.catalogue)

    def test_invalid_shape_and_pass_markers_cannot_become_a_plan(self):
        original = json.loads(self.catalogue.read_text())
        invalid = [[], {**original, "schema": True},
                   {**original, "software_status": "passed"},
                   {**original, "properties": "properties"},
                   {**original, "properties": [None]},
                   {**original, "unproven": []}]
        for data in invalid:
            with self.subTest(data=data):
                self.catalogue.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    work_packages.reviewed_catalogue(self.root, self.catalogue)

    def test_noncanonical_or_duplicate_source_list_rejected(self):
        original = json.loads(self.catalogue.read_text())
        for sources in (["soc/example/drivers/../drivers/uart.c"],
                        [str(self.root / "soc/example/drivers/uart.c")],
                        ["soc/example/drivers/uart.c"] * 2):
            data = copy.deepcopy(original)
            data["properties"][0]["production_sources"] = sources
            with self.subTest(sources=sources):
                self.catalogue.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    work_packages.reviewed_catalogue(self.root, self.catalogue)

    def test_oversized_catalogue_rejected_before_json_parsing(self):
        self.catalogue.write_bytes(b" " * (2 * 1024 * 1024 + 1))
        with self.assertRaisesRegex(ValueError, "size"):
            work_packages.reviewed_catalogue(self.root, self.catalogue)

    def test_symlink_catalogue_or_production_source_is_not_owned(self):
        for path in (self.catalogue, self.root / "soc/example/drivers/uart.c"):
            original = path.read_bytes()
            target = self.root / (path.name + ".original")
            target.write_bytes(original)
            path.unlink()
            path.symlink_to(target)
            with self.subTest(path=str(path)), self.assertRaises(ValueError):
                work_packages.reviewed_catalogue(self.root, self.catalogue)
            path.unlink()
            path.write_bytes(original)


class WorkPackageCommandTests(unittest.TestCase):
    def test_report_cannot_overwrite_source_or_existing_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            root.mkdir()
            source = root / "authority.json"
            source.write_text("original\n")
            with self.assertRaisesRegex(ValueError, "source"):
                work_packages.write_report(root, source, {})
            self.assertEqual(source.read_text(), "original\n")
            output = Path(directory) / "report.json"
            work_packages.write_report(root, output, {"hardware_status": "not_executed"})
            with self.assertRaises((ValueError, FileExistsError)):
                work_packages.write_report(root, output, {"hardware_status": "passed"})
            self.assertEqual(json.loads(output.read_text()),
                             {"hardware_status": "not_executed"})

    def test_failed_collection_retains_no_success_report(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            with patch.object(work_packages, "collect", side_effect=ValueError("bad source")), \
                    redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
                code = work_packages.main(["--output", str(output)])
            self.assertEqual(code, 1)
            self.assertFalse(output.exists())

    def test_output_symlink_parent_and_traversal_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root, actual = base / "source", base / "actual"
            root.mkdir()
            actual.mkdir()
            alias = base / "alias"
            alias.symlink_to(actual, target_is_directory=True)
            for output in (alias / "report.json", actual / ".." / "report.json"):
                with self.subTest(output=str(output)), self.assertRaises(ValueError):
                    work_packages.write_report(root, output, {})
            self.assertEqual(list(actual.iterdir()), [])


class CurrentWorkPackageTests(unittest.TestCase):
    def test_installed_cmake_python_package_cannot_shadow_sdk_authority(self):
        installed = ModuleType("cmake")
        installed.__path__ = []
        with patch.dict(sys.modules):
            for name in list(sys.modules):
                if name == "cmake" or name.startswith("cmake."):
                    del sys.modules[name]
            sys.modules["cmake"] = installed
            report = work_packages.collect(ROOT)
        self.assertTrue(report["work_packages"])
        self.assertFalse(report["software_qualification"])

    def test_actual_authorities_derive_exact_scopes_and_keep_qualification_separate(self):
        report = work_packages.collect(ROOT)
        self.assertGreater(len(report["work_packages"]), 0)
        selected = next(row for row in report["work_packages"]
                        if row["identity"] == "stm32f407.USART1.dma-tx")
        self.assertEqual(selected["provider_implementation"], "uart_dma")
        self.assertIn("soc/stm32f407/drivers/uart_dma.c", selected["sdk_required_sources"])
        self.assertTrue(selected["reviewed_properties"])
        self.assertTrue(report["authority_inputs"])
        inputs = {row["path"] for row in report["authority_inputs"]}
        self.assertIn("dependencies/platform-compatibility.json", inputs)
        self.assertIn("CMakeLists.txt", inputs)
        self.assertTrue(report["fixture_configurations"])
        for fixture in report["fixture_configurations"]:
            self.assertEqual(len(fixture["configuration_sha256"]), 64)
            self.assertTrue(fixture["inputs"])
            self.assertTrue({item["path"] for item in fixture["inputs"]} <= inputs)
        for row in report["work_packages"]:
            self.assertEqual(row["software_status"], "not_executed")
            self.assertEqual(row["hardware_status"], "not_executed")
        self.assertFalse(report["software_qualification"])
        self.assertFalse(report["physical_qualification"])


if __name__ == "__main__":
    unittest.main()
