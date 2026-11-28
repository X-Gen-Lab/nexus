"""One global support scan checks facts, exports and derived documentation."""

from pathlib import Path
import contextlib
import io
import subprocess
import sys
import tempfile
import tomllib
import unittest
from unittest.mock import patch

from scripts.ci import provider_assemblies
from tools.maintenance import capabilities

ROOT = Path(__file__).resolve().parents[3]


class CapabilityUnitTests(unittest.TestCase):
    def test_selected_query_does_not_import_checkout_only_tools(self):
        script = '''
import importlib.abc
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
class InstalledScope(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        if fullname in {'tools.configure.documentation', 'tools.configure.matrix'}:
            raise ImportError('checkout-only module requested: ' + fullname)
sys.meta_path.insert(0, InstalledScope())
from tools.maintenance import capabilities
root = Path(sys.argv[1])
selected = capabilities.selection(
    root / 'tools/configure/assemblies/qiming-baremetal-gpio.toml', root)
assert selected['selection']['soc_family'] == 'stm32f4'
assert selected['validation']['configuration'] == 'validated'
'''
        result = subprocess.run([sys.executable, '-I', '-B', '-c', script, str(ROOT)],
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_global_maintenance_rejects_installed_or_incomplete_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for installed in (False, True):
                if installed:
                    (root / '.nexus-source-sdk.json').write_text('{}\n')
                with self.subTest(installed=installed):
                    errors = io.StringIO()
                    with contextlib.redirect_stdout(io.StringIO()), \
                            contextlib.redirect_stderr(errors):
                        status = capabilities.main(['--check', '--root', str(root)])
                    self.assertEqual(status, 1)
                    self.assertIn('complete source checkout', errors.getvalue())

    def test_generated_block_requires_exact_content_and_single_markers(self):
        expected = "| UART | dma-tx |\n"
        document = ("intro\n" + capabilities.START + "\n" + expected
                    + capabilities.END + "\n")
        capabilities.check_block(document, expected)
        for stale in (document.replace("dma-tx", "future"),
                      document + capabilities.START,
                      document.replace(capabilities.END, "")):
            with self.subTest(stale=stale), self.assertRaises(ValueError):
                capabilities.check_block(stale, expected)

    def test_ops_evidence_ignores_comments_and_strings(self):
        source = '''
/* const nx_uart_ops_t nx_sample_uart_ops = {0}; */
const char* prose = "const nx_uart_ops_t nx_sample_uart_ops = {0};";
const nx_uart_ops_t nx_sample_uart_ops_extra = {0};
const nx_uart_ops_t nx_sample_uart_ops = {0};
'''
        self.assertEqual(capabilities.ops_definitions(source),
                         {"nx_sample_uart_ops_extra", "nx_sample_uart_ops"})


class CapabilityDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with patch('tools.configure.generation.bindings.emit',
                   side_effect=AssertionError('capability queries must not emit C')):
            cls.rows = capabilities.collect(ROOT)

    def test_current_matrix_uses_actual_resolver_and_preserves_scope(self):
        rows = self.rows
        self.assertTrue(rows)
        uart_dma = [row for row in rows if row["family"] == "stm32f4"
                    and row["controller"] == "USART1"
                    and row["mode"] == "dma-tx"]
        self.assertEqual(len(uart_dma), 1)
        self.assertEqual(uart_dma[0]["exports"],
                         ["soc/stm32/stm32f4/drivers/uart/uart_hal.c"])
        self.assertTrue(uart_dma[0]["fixtures"])
        spi = next(row for row in rows if row["family"] == "stm32f4"
                   and row["controller"] == "SPI1" and row["mode"] == "dma")
        self.assertEqual(set(spi["boards"]), {
            "stm32f407zg-qiming-core", "stm32f407zg-qiming-v41"})
        nested = (
            ("DCMI", "capture", "jpeg-blocks",
             "tests/integration/assemblies/f407_capture_audio/jpeg_blocks/ve-jpeg-blocks.toml",
             "soc/stm32/stm32f4/drivers/capture/capture.c"),
            ("SPI1", "spi", "short-poll",
             "tests/integration/resources/io_storage_cost/spi_endpoints/endpoints-1.toml",
             "soc/stm32/stm32f4/drivers/spi/spi.c"))
        for controller, kind, mode, fixture, source in nested:
            with self.subTest(fixture=fixture):
                selected = next(row for row in rows
                                if (row["family"], row["controller"],
                                    row["kind"], row["mode"]) ==
                                ("stm32f4", controller, kind, mode))
                self.assertIn(fixture, selected["fixtures"])
                self.assertEqual(selected["exports"], [source])

    def test_selected_shared_ip_exports_keep_the_actual_driver_path(self):
        rows = self.rows
        selected = [row for row in rows if row["family"] == "stm32f4"
                    and row["kind"] == "i2c" and row["exports"]
                    and row["mode"] in {"irq-messages", "dma-messages"}]
        self.assertEqual({row["mode"] for row in selected},
                         {"irq-messages", "dma-messages"})
        for row in selected:
            with self.subTest(mode=row["mode"]):
                self.assertEqual(row["exports"],
                                 ["soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"])

    def test_can_exports_use_the_shared_production_controller(self):
        selected = [row for row in self.rows if row["kind"] == "can"]
        self.assertEqual(len(selected), 2)
        for row in selected:
            with self.subTest(controller=row["controller"]):
                self.assertEqual(row["exports"], [
                    "soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.c"])
                self.assertTrue(row["fixtures"])

    def test_selected_timer_faces_preserve_exact_modes_and_source_paths(self):
        selected = [row for row in self.rows
                    if row["kind"] in {"timer_capture", "advanced_pwm"}]
        expected = {
            ("stm32f4", f"TIM{number}", "timer_capture", "edge-capture")
            for number in (1, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14)
        } | {
            ("stm32f4", "TIM1", "advanced_pwm", "complementary-pwm"),
            ("stm32f4", "TIM8", "advanced_pwm", "complementary-pwm"),
        }
        self.assertEqual({(row["family"], row["controller"], row["kind"],
                           row["mode"]) for row in selected}, expected)
        for row in selected:
            with self.subTest(controller=row["controller"], kind=row["kind"]):
                self.assertTrue(row["fixtures"])
                if row["kind"] == "timer_capture" and row["controller"] != "TIM4":
                    name = "timer_capture_" + row["controller"].lower()
                elif row["kind"] == "advanced_pwm" and row["controller"] == "TIM8":
                    name = "advanced_pwm_tim8"
                else:
                    name = row["kind"]
                self.assertEqual(row["exports"], [
                    f"soc/stm32/stm32f4/drivers/timer/{name}.c"])
                self.assertFalse(row["boards"])

    def test_declared_fixtures_share_the_resolved_capability_index(self):
        directory = ROOT / "tests/integration/assemblies/declared_io_assembly"
        fixtures = sorted(directory.glob("*.toml"))
        self.assertTrue(fixtures)
        indexed = {(row["family"], row["controller"], row["mode"]): row
                   for row in self.rows}
        for row in provider_assemblies.validate_fixtures(ROOT, fixtures):
            relative = Path(row["assembly"]).relative_to(ROOT).as_posix()
            for item in row["resolved"]["controllers"]:
                key = (row["resolved"]["soc_family"], item["controller"],
                       item["mode"])
                with self.subTest(selection=key, fixture=relative):
                    self.assertIn(relative, indexed[key]["fixtures"])
                    self.assertTrue(indexed[key]["exports"])

    def test_native_models_require_explicit_scope_outside_production_matrix(self):
        rows = self.rows
        self.assertEqual({row["family"] for row in rows}, {"stm32f1", "stm32f4"})
        fixture = ROOT / "cmake/package/verification/io_evolution/native-time-pair.toml"
        with self.assertRaisesRegex(ValueError, "explicit test support"):
            capabilities.selection(fixture, ROOT)
        selected = capabilities.selection(fixture, ROOT, test_support=True)
        self.assertIs(selected["physical_qualified"], False)
        self.assertEqual({(item["controller"], item["kind"], item["mode"])
                          for item in selected["controllers"]}, {
            ("RTC0", "rtc", "calendar-alarm"),
            ("RTC1", "rtc", "calendar-alarm"),
            ("LPTIM0", "lptim", "counter16-oneshot"),
            ("LPTIM1", "lptim", "counter16-oneshot")})
        for item in selected["controllers"]:
            self.assertTrue(item["contract"]["model"])
            self.assertEqual(item["contract"]["provider"], "native")
            self.assertIn("MODEL", item["contract"]["features"])
        empty = ROOT / "cmake/package/verification/io_evolution/native-time-empty.toml"
        self.assertEqual(capabilities.selection(
            empty, ROOT, test_support=True)["controllers"], [])
        rendered = capabilities.render(rows)
        self.assertNotIn("| native |", rendered)

    def test_checked_document_matches_facts_and_selected_implementation(self):
        expected = capabilities.render(self.rows)
        document = (ROOT / capabilities.DOCUMENT).read_text()
        capabilities.check_block(document, expected)

    def test_redistributed_references_preserve_selected_source_provenance(self):
        directory = ROOT / "cmake/package/verification/io_evolution"
        fixtures = sorted(directory.glob("*.toml"))
        self.assertTrue(fixtures)
        rows = self.rows
        for fixture in fixtures:
            host = tomllib.loads(fixture.read_text())["backend"] == "native"
            resolved = capabilities.configure.resolve_ir(
                fixture, ROOT, test_support=host)
            relative = fixture.relative_to(ROOT).as_posix()
            if host:
                self.assertFalse(any(relative in row["fixtures"] for row in rows))
                continue
            for item in resolved.controllers:
                with self.subTest(fixture=fixture.name, device=item.id):
                    selected = [row for row in rows if
                                (row["family"], row["controller"], row["kind"],
                                 row["mode"]) ==
                                (resolved.soc_family, item.controller,
                                 item.kind, item.mode)]
                    self.assertEqual(len(selected), 1)
                    self.assertIn(relative, selected[0]["fixtures"])
                    self.assertTrue(selected[0]["exports"])



if __name__ == "__main__":
    unittest.main()
