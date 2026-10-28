"""Delivery CLI results are explicit files independent of human command logs."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.documentation import cli, delivery
from tools.documentation import test_delivery as fixture


class DeliveryReportTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixture.DeliveryTests(
            "test_package_has_no_implicit_historical_qualification")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.report = self.fixture.base / "reports/delivery.json"
        self.errors = io.StringIO()

    def invoke(self, arguments):
        with redirect_stdout(io.StringIO()), redirect_stderr(self.errors):
            try:
                return cli.main(arguments, root=self.fixture.source)
            except SystemExit as error:
                return error.code

    def read_report(self, action, status):
        record = json.loads(self.report.read_text())
        self.assertEqual(record["schema_version"], 1)
        self.assertEqual(record["kind"],
                         "nexus_documentation_delivery_result")
        self.assertEqual(record["action"], action)
        self.assertEqual(record["status"], status)
        self.assertIs(record["qualification"], False)
        return record

    def test_package_verify_and_restore_report_actual_independent_results(self):
        arguments = ["package", "--build", str(self.fixture.build),
                     "--output", str(self.fixture.base / "output"),
                     "--release-id", "1.0.0-explicit-report",
                     "--report", str(self.report)]
        self.assertEqual(self.invoke(arguments), 0, self.errors.getvalue())
        packaged = self.read_report("package", "passed")
        result = packaged["result"]
        self.assertEqual(packaged["errors"], [])
        archive = result["archive"]
        expected = result["manifest_sha256"]
        verified = delivery.verify(archive, expected_manifest_sha256=expected)
        self.assertEqual(self.invoke(["verify", archive,
            "--expected-manifest-sha256", expected,
            "--report", str(self.report)]), 0, self.errors.getvalue())
        self.assertEqual(self.read_report("verify", "passed")["result"],
                         verified)
        restored = self.fixture.base / "independent-root"
        self.assertEqual(self.invoke(["restore", archive,
            "--output", str(restored),
            "--expected-manifest-sha256", expected,
            "--report", str(self.report)]), 0, self.errors.getvalue())
        self.assertEqual(self.read_report("restore", "passed")["result"],
                         delivery.verify(restored))

    def test_failed_verification_replaces_old_success_with_failure(self):
        result = self.fixture.package()
        self.report.parent.mkdir(parents=True)
        self.report.write_text('{"status":"passed","result":"stale"}')
        self.assertNotEqual(self.invoke(["verify", result["archive"],
            "--expected-manifest-sha256", "0" * 64,
            "--report", str(self.report)]), 0)
        report = self.read_report("verify", "failed")
        self.assertIsNone(report["result"])
        self.assertTrue(report["errors"])

    def test_failed_package_is_reported_without_creating_archive(self):
        (self.fixture.build / "index.html").unlink()
        output = self.fixture.base / "output"
        self.assertNotEqual(self.invoke(["package", "--build",
            str(self.fixture.build), "--output", str(output), "--report",
            str(self.report)]), 0)
        self.assertIsNone(self.read_report("package", "failed")["result"])
        self.assertFalse(output.exists())

    def test_failed_restore_preserves_existing_destination(self):
        result = self.fixture.package()
        restored = self.fixture.base / "existing"
        restored.mkdir()
        valuable = restored / "valuable.txt"
        valuable.write_text("keep")
        self.assertNotEqual(self.invoke(["restore", result["archive"],
            "--output", str(restored), "--report", str(self.report)]), 0)
        self.assertEqual(valuable.read_text(), "keep")
        self.assertEqual(self.read_report("restore", "failed")["action"],
                         "restore")

    def test_report_cannot_overwrite_verified_payload_or_archive(self):
        result = self.fixture.package()
        archive = self.fixture.base / "archive.json"
        archive.write_bytes(Path(result["archive"]).read_bytes())
        inputs = [(result["directory"], Path(result["directory"])
                   / delivery.MANIFEST), (str(archive), archive)]
        for source, target in inputs:
            before = target.read_bytes()
            with self.subTest(target=target), redirect_stderr(io.StringIO()):
                self.assertNotEqual(self.invoke(["verify", source,
                    "--report", str(target)]), 0)
                self.assertEqual(target.read_bytes(), before)

    def test_report_preserves_build_and_qualification_reference(self):
        metadata = self.fixture.build / "documentation-build.json"
        original = metadata.read_bytes()
        arguments = ["package", "--build", str(self.fixture.build),
                     "--output", str(self.fixture.base / "output")]
        self.assertNotEqual(self.invoke(arguments + ["--report",
                                                    str(metadata)]), 0)
        self.assertEqual(metadata.read_bytes(), original)
        candidate = self.fixture.base / "candidate.json"
        candidate.write_text('{"valuable":"candidate"}')
        self.assertNotEqual(self.invoke(arguments + [
            "--qualification-reference", str(candidate),
            "--report", str(candidate)]), 0)
        self.assertEqual(candidate.read_text(), '{"valuable":"candidate"}')

    def test_report_cannot_pollute_restore_destination(self):
        result = self.fixture.package()
        restored = self.fixture.base / "restored"
        self.assertNotEqual(self.invoke(["restore", result["archive"],
            "--output", str(restored), "--report",
            str(restored / "report.json")]), 0)
        self.assertFalse(restored.exists())

    def test_report_cannot_be_written_into_package_output(self):
        output = self.fixture.base / "output"
        self.assertNotEqual(self.invoke(["package", "--build",
            str(self.fixture.build), "--output", str(output),
            "--report", str(output / "report.json")]), 0)
        self.assertFalse(output.exists())

    def test_missing_archive_removes_stale_success(self):
        self.report.parent.mkdir(parents=True)
        self.report.write_text('{"status":"passed"}')
        missing = self.fixture.base / "missing.tar.gz"
        self.assertNotEqual(self.invoke(["verify", str(missing),
            "--report", str(self.report)]), 0)
        self.assertIsNone(self.read_report("verify", "failed")["result"])

    def test_symlink_report_is_rejected_without_modifying_target(self):
        result = self.fixture.package()
        target = self.fixture.base / "valuable.json"
        target.write_text("keep")
        self.report.parent.mkdir(parents=True)
        self.report.symlink_to(target)
        self.assertNotEqual(self.invoke(["verify", result["archive"],
            "--report", str(self.report)]), 0)
        self.assertEqual(target.read_text(), "keep")

    def test_failed_report_publication_preserves_verified_package(self):
        output = self.fixture.base / "output"
        replace = Path.replace
        failed = False

        def fail_once(source, destination):
            nonlocal failed
            if destination == self.report and not failed:
                failed = True
                raise OSError("report publication interrupted")
            return replace(source, destination)

        with patch.object(Path, "replace", fail_once):
            self.assertNotEqual(self.invoke(["package", "--build",
                str(self.fixture.build), "--output", str(output),
                "--release-id", "recoverable-package",
                "--report", str(self.report)]), 0)
        self.assertTrue(failed)
        self.assertIsNone(self.read_report("package", "failed")["result"])
        archive = output / "recoverable-package.tar.gz"
        self.assertEqual(delivery.verify(archive)["release_id"],
                         "recoverable-package")
        self.assertEqual(self.invoke(["verify", str(archive),
            "--report", str(self.report)]), 0)
        self.assertEqual(self.read_report("verify", "passed")["result"]
                         ["release_id"], "recoverable-package")

    def test_interrupted_report_write_preserves_previous_bytes(self):
        self.report.parent.mkdir(parents=True)
        self.report.write_text("previous")
        with patch.object(Path, "replace", side_effect=OSError("interrupted")):
            with self.assertRaises(OSError):
                cli._record(self.report, {"status": "failed"})
        self.assertEqual(self.report.read_text(), "previous")
        self.assertEqual(list(self.report.parent.iterdir()), [self.report])


if __name__ == "__main__":
    unittest.main()
