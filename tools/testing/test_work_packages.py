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


    def test_native_model_metadata_is_preserved_without_execution(self):
        row = copy.deepcopy(self.rows[0])
        row.update(family="native", controller="RTC0", kind="rtc",
                   mode="calendar-alarm", routes=[], boards=[],
                   implementation="MODEL", physical_qualified=False)
        providers = {"native": copy.deepcopy(self.providers["example"])}
        before = copy.deepcopy((row, providers))
        report = work_packages.derive([row], [], providers, {self.source})
        package = report["work_packages"][0]
        self.assertEqual(package["implementation"], "MODEL")
        self.assertIs(package["physical_qualified"], False)
        self.assertEqual(package["reviewed_routes"], [])
        self.assertEqual(package["declared_boards"], [])
        self.assertEqual(package["software_status"], "not_executed")
        self.assertEqual(package["hardware_status"], "not_executed")
        self.assertFalse(report["software_qualification"])
        self.assertFalse(report["physical_qualification"])
        self.assertEqual((row, providers), before)

    def test_invalid_native_model_or_physical_markers_fail_closed(self):
        row = copy.deepcopy(self.rows[0])
        row.update(family="native", controller="RTC0", kind="rtc",
                   mode="calendar-alarm", routes=[], boards=[],
                   implementation="MODEL", physical_qualified=False)
        providers = {"native": copy.deepcopy(self.providers["example"])}
        mutations = [("physical_qualified", True),
                     ("physical_qualified", 0),
                     ("physical_qualified", None),
                     ("physical_qualified", "false"),
                     ("implementation", "hardware"),
                     ("implementation", None)]
        for field, value in mutations:
            invalid = copy.deepcopy(row)
            invalid[field] = value
            before = copy.deepcopy((invalid, providers))
            with self.subTest(field=field, value=value):
                with self.assertRaisesRegex(ValueError, "MODEL"):
                    work_packages.derive([invalid], [], providers, {self.source})
                self.assertEqual((invalid, providers), before)
        for field in ("implementation", "physical_qualified"):
            invalid = copy.deepcopy(row)
            del invalid[field]
            with self.subTest(missing=field), self.assertRaisesRegex(ValueError, "MODEL"):
                work_packages.derive([invalid], [], providers, {self.source})


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


    def test_native_descriptor_uses_current_fact_and_dependency_contract(self):
        from tools.configure.providers import descriptor
        family = descriptor.maintained("native")
        contracts = work_packages.provider_contracts()
        self.assertIn("native", contracts)
        self.assertEqual(contracts["native"], {
            "configuration_modules": list(family.configuration_modules),
            "fact_files": list(family.fact_files),
            "git_dependencies": [{"role": role, "path": path}
                                 for role, path in family.git_dependencies],
            "import_lock": family.import_lock,
            "dependency_identity_status": "declared_unverified",
        })
        self.assertEqual(contracts["native"]["git_dependencies"], [])
        self.assertIsNone(contracts["native"]["import_lock"])

    def test_actual_native_models_keep_sources_sdk_fixtures_and_mcu_shape(self):
        report = work_packages.collect(ROOT)
        models = [row for row in report["work_packages"] if row["family"] == "native"]
        physical = [row for row in report["work_packages"] if row["family"] != "native"]
        self.assertEqual(len(models), 19)
        self.assertEqual(len(physical), 191)
        expected = {
            "identity", "family", "controller", "kind", "mode", "provider",
            "reviewed_routes", "declared_boards", "resolver_fixtures",
            "selected_ops_sources", "sdk_required_sources", "implementation_status",
            "contract_status", "reviewed_properties", "software_status",
            "hardware_status", "provider_implementation",
        }
        for row in physical:
            with self.subTest(MCU=row["identity"]):
                self.assertEqual(set(row), expected)
                self.assertEqual(row["software_status"], "not_executed")
                self.assertEqual(row["hardware_status"], "not_executed")
        for row in models:
            with self.subTest(model=row["identity"]):
                self.assertEqual(set(row), expected | {"implementation", "physical_qualified"})
                self.assertEqual(row["implementation"], "MODEL")
                self.assertIs(row["physical_qualified"], False)
                self.assertEqual(row["reviewed_routes"], [])
                self.assertEqual(row["declared_boards"], [])
                self.assertTrue(row["selected_ops_sources"])
                self.assertEqual(row["sdk_required_sources"], row["selected_ops_sources"])
                self.assertTrue(row["resolver_fixtures"])
                self.assertEqual(row["implementation_status"], "source_selected")
                self.assertEqual(row["software_status"], "not_executed")
                self.assertEqual(row["hardware_status"], "not_executed")
                self.assertEqual(row["provider"], work_packages.provider_contracts()["native"])
        fixture = "cmake/package/verification/io_evolution/native-time-pair.toml"
        for controller, kind, mode in (("RTC0", "rtc", "calendar-alarm"),
                                       ("RTC1", "rtc", "calendar-alarm"),
                                       ("LPTIM0", "lptim", "counter16-oneshot"),
                                       ("LPTIM1", "lptim", "counter16-oneshot")):
            with self.subTest(controller=controller):
                values = [row for row in models if
                          (row["controller"], row["kind"], row["mode"]) ==
                          (controller, kind, mode)]
                self.assertEqual(len(values), 1)
                self.assertEqual(values[0]["selected_ops_sources"], ["io/native/rtc_lptim.c"])
                self.assertIn(fixture, values[0]["resolver_fixtures"])
                self.assertIn(values[0]["provider_implementation"], {"rtc", "lptim"})
        inputs = {row["path"] for row in report["authority_inputs"]}
        self.assertTrue({"soc/native/soc.json", "soc/native/routes.json",
                         "tools/configure/providers/native/__init__.py",
                         "tools/configure/providers/native/bindings.py",
                         "tools/configure/providers/native/emission.py",
                         "io/native/rtc_lptim.c", fixture} <= inputs)
        empty = "cmake/package/verification/io_evolution/native-time-empty.toml"
        self.assertFalse(any(empty in row["resolver_fixtures"] for row in models))
        self.assertFalse(report["software_qualification"])
        self.assertFalse(report["physical_qualification"])
        self.assertEqual(report["summary"]["software_executed"], 0)
        self.assertEqual(report["summary"]["hardware_executed"], 0)


if __name__ == "__main__":
    unittest.main()
