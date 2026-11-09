"""Consumers bind actual ARM ABI to canonical IR, never autodetect ELF flags."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   load_json, verify_file_identity)
from tools.evidence.identity import canonical_digest
from tools.measurement import compare, interfaces
from tools.measurement.compare import run_logged
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data
from tests.contracts.evidence_abi_support import build_layout, link_abi_fixture
from tests.contracts.test_interface_cost import image


class ConsumerAbiTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)












    def interface_cli(self, elf, resolved=None, output=None):
        argv = ["--elf", str(elf), "--output", str(output or self.root / "interface.json")]
        if resolved is not None:
            argv += ["--resolved", str(resolved)]
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                code = interfaces.main(argv)
            except SystemExit as error:
                code = error.code
        return code, stderr.getvalue()

    def test_interface_cli_binds_all_three_abis_to_canonical_cpu(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                elf, resolved, _ = link_abi_fixture(self.root, abi)
                code, error = self.interface_cli(elf, resolved)
                self.assertEqual(code, 0, error)

    def test_interface_cli_rejects_both_abi_mismatches_and_malformed_cpu(self):
        for declared, linked, cpu in (("soft", "hard", {"float_abi": "soft"}),
                                     ("hard", "soft", {"float_abi": "hard"}),
                                     ("softfp", "hard", {"float_abi": "softfp"}),
                                     ("hard", "softfp", {"float_abi": "hard"}),
                                     ("hard", "hard", None)):
            with self.subTest(declared=declared, linked=linked):
                elf, resolved, _ = link_abi_fixture(self.root, declared, linked=linked, cpu=cpu)
                code, error = self.interface_cli(elf, resolved)
                self.assertEqual(code, 1, error)
                self.assertRegex(error, "ABI required|resolved CPU")
                self.assertFalse((self.root / "interface.json").exists())

    def test_interface_cli_legacy_without_resolved_is_hard_only(self):
        for abi, expected in (("hard", 0), ("soft", 1), ("softfp", 1)):
            with self.subTest(abi=abi):
                elf, _, _ = link_abi_fixture(self.root, abi)
                code, error = self.interface_cli(elf)
                self.assertEqual(code, expected, error)

    def test_interface_output_cannot_overwrite_input_or_declared_source(self):
        elf, resolved, _ = link_abi_fixture(self.root, "soft")
        for path in (elf, resolved, self.root / "input_paths.json", self.root / "image.c"):
            with self.subTest(path=path.name):
                before = path.read_bytes()
                code, error = self.interface_cli(elf, resolved, path)
                self.assertEqual(code, 1, error)
                self.assertEqual(path.read_bytes(), before)

    def test_family_operations_tables_are_measured_and_writable_tables_rejected(self):
        for family in ("stm32f103", "stm32g474", "stm32h743", "stm32u575", "gd32f303"):
            with self.subTest(family=family):
                objects = [("nx_" + family + "_uart_ops", 32)]
                result = interfaces.inspect_interfaces(image(objects))
                self.assertEqual(result["operations_flash_bytes"], 32)
                with self.assertRaisesRegex(EvidenceError, "read-only"):
                    interfaces.inspect_interfaces(image(objects, True))

    def matrix_fixture(self, declared, *, extra_source="", **kwargs):
        source = self.root / ("source-" + declared)
        source.mkdir(exist_ok=True)
        subprocess.run(["git", "init", "-q", str(source)], check=True)
        (source / "tracked.txt").write_text("ABI consumer fixture\n")
        subprocess.run(["git", "-C", str(source), "add", "."], check=True)
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture",
            "-c", "user.email=fixture@example.invalid", "commit", "-qm", "fixture"], check=True)
        assembly = self.root / "fixture.toml"
        contents = ('schema = 2\nboard_package = "board"\n'
                    'backend = "baremetal"\nclock = "fixed"\n'
                    '[memory]\nmain_stack_bytes = 2048\n')
        assembly.write_text(contents)
        (self.root / "fixture-empty.toml").write_text(contents)
        def build_boundary(argv, log):
            if "-B" in argv:
                build_layout(Path(argv[argv.index("-B") + 1]), declared,
                             extra_source=extra_source, **kwargs)
            # Replace only external build orchestration; ELF, canonical IR,
            # resource and interface gates all consume actual ARM bytes.
            return run_logged([sys.executable, "-c", "print('ARM fixture linked')"], log)
        with patch.object(compare, "run_logged", side_effect=build_boundary):
            return compare.compare(source, assembly, self.root / ("matrix-" + declared),
                workloads=("empty",), optimizations=("Os",))

    def test_compare_reparses_actual_elf_with_resolved_abi(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                report = self.matrix_fixture(abi)
                self.assertEqual(report["status"], "passed", report.get("failure"))
                self.assertGreater(report["images"][0]["resources"]["size"], 0)

    def test_empty_image_rejects_a_retained_family_named_provider(self):
        report = self.matrix_fixture("soft", extra_source=
            "\nvoid nx_stm32f103_gpio_probe(void) {}\n")
        self.assertEqual(report["status"], "failed")
        proof = report["images"][0]["unreferenced_device_gc"]
        self.assertEqual(proof["active_device_provider_symbols"], ["nx_stm32f103_gpio_probe"])

    def test_compare_retains_failed_report_for_malformed_present_cpu(self):
        report = self.matrix_fixture("hard", cpu=None)
        self.assertEqual(report["status"], "failed")
        self.assertIn("resolved CPU", report["images"][0]["failure"])
        self.assertEqual(load_json(self.root / "matrix-hard/matrix.json"), report)

    def test_interface_rejects_stale_inputs_and_unbound_canonical_config(self):
        elf, resolved_path, _ = link_abi_fixture(self.root, "soft")
        original = load_json(resolved_path)
        original_source = (self.root / "image.c").read_text()
        (self.root / "image.c").write_text("changed input after link")
        code, error = self.interface_cli(elf, resolved_path)
        self.assertEqual(code, 1)
        self.assertIn("input changed", error)
        (self.root / "image.c").write_text(original_source)
        original["board"] = "changed-canonical-board"
        original.pop("configuration_sha256")
        original["configuration_sha256"] = canonical_digest(original)
        atomic_json(resolved_path, original)
        code, error = self.interface_cli(elf, resolved_path)
        self.assertEqual(code, 1)
        self.assertIn("ELF does not bind", error)



if __name__ == "__main__":
    unittest.main()
