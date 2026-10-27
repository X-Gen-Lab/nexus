"""Derived status pages must preserve source ledger semantics and failures."""

import csv
from pathlib import Path
import tempfile
import unittest

from tools.documentation import status


class StatusTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs/design").mkdir(parents=True)

    def ledger(self, rows, fields=None):
        path = self.root / "docs/design/os-execution.csv"
        fields = fields or list(rows[0])
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
        return path

    def test_original_exit_status_and_evidence_are_preserved(self):
        source = self.ledger([{
            "task_id": "OS49", "task": "Actual wake",
            "software_exit": "fresh execution; no empty reports",
            "hardware_exit": "measure exact station",
            "implementation_status": "implemented_tooling",
            "software_status": "failed_attempt_retained",
            "hardware_status": "not_executed",
            "evidence_paths": "/workspace/unpublished/failed.json",
        }])
        view = status.render(self.root, source, "OS ledger", "os")
        for value in ("OS49", "fresh execution; no empty reports",
                      "measure exact station", "implemented_tooling",
                      "failed_attempt_retained", "not_executed",
                      "/workspace/unpublished/failed.json"):
            self.assertIn(value, view)
        self.assertNotIn("accepted", view)
        self.assertIn("原退出字段", view)

    def test_missing_exit_column_stays_explicit(self):
        source = self.ledger([{"task_id": "OS01", "task": "wake",
                              "qualification_authority": "fresh exact source"}])
        view = status.render(self.root, source, "OS ledger", "os")
        self.assertIn("未定义单独退出字段", view)
        self.assertIn("fresh exact source", view)
        self.assertNotIn("exit_condition |", view)

    def test_duplicate_ids_are_rejected(self):
        source = self.ledger([{"task_id": "A", "task": "x"},
                              {"task_id": "A", "task": "y"}])
        with self.assertRaisesRegex(ValueError, "duplicate"):
            status.render(self.root, source, "ledger", "os")

    def test_blank_id_and_no_id_column_are_rejected(self):
        for rows in ([{"task_id": "", "task": "x"}], [{"task": "x"}]):
            with self.subTest(rows=rows):
                source = self.ledger(rows)
                with self.assertRaises(ValueError):
                    status.render(self.root, source, "ledger", "os")

    def test_rows_with_extra_values_are_rejected(self):
        source = self.root / "docs/design/os-execution.csv"
        source.write_text("task_id,task\nOS01,wake,unexpected\n")
        with self.assertRaisesRegex(ValueError, "columns"):
            status.render(self.root, source, "ledger", "os")

    def test_input_escape_and_symlink_are_rejected(self):
        outside = self.root / "outside.csv"
        outside.write_text("task_id,task\nOS01,wake\n")
        with self.assertRaisesRegex(ValueError, "source"):
            status.render(self.root, outside, "ledger", "os")
        source = self.root / "docs/design/os-execution.csv"
        source.symlink_to(outside)
        with self.assertRaisesRegex(ValueError, "symlink"):
            status.render(self.root, source, "ledger", "os")

    def test_original_order_and_csv_digest_are_bound(self):
        source = self.ledger([{"id": "BLD-02", "requirement": "second"},
                              {"id": "BLD-01", "requirement": "first"}])
        view = status.render(self.root, source, "Build ledger", "build-system")
        self.assertLess(view.index("## BLD-02"), view.index("## BLD-01"))
        self.assertIn(status.digest(source), view)

    def test_generate_check_reports_missing_and_stale_without_writing(self):
        source = self.ledger([{"task_id": "OS01", "task": "wake"}])
        specifications = [("os-execution.csv", "os", "OS ledger")]
        report = status.generate(self.root, check=True,
                                 specifications=specifications)
        output = self.root / "docs/status/ledgers/os.md"
        self.assertFalse(output.exists())
        self.assertEqual(report["status"], "failed")
        status.generate(self.root, specifications=specifications)
        self.assertEqual(status.generate(self.root, check=True,
                                         specifications=specifications)["status"],
                         "passed")
        source.write_text(source.read_text().replace("wake", "changed"))
        report = status.generate(self.root, check=True,
                                 specifications=specifications)
        self.assertEqual(report["status"], "failed")
        self.assertIn("stale", report["errors"][0])

    def test_markdown_special_characters_remain_literal(self):
        source = self.ledger([{"task_id": "OS01", "task": "a|b",
                              "exit_condition": "bytes `x` <tag>"}])
        view = status.render(self.root, source, "ledger", "os")
        self.assertIn("a\\|b", view)
        self.assertIn("&lt;tag&gt;", view)

    def test_soc_ip_ledger_is_registered_without_changing_original_status(self):
        specification = next((item for item in status.SPECIFICATIONS
                              if item[0] == "soc-ip-layout-execution.csv"), None)
        self.assertIsNotNone(specification,
                             "completed SoC/IP ledger must enter default views")
        source = self.root / "docs/design" / specification[0]
        source.write_text(
            "task_id,software_status,physical_status,evidence_paths\n"
            "R44,segmented_execution_passed,not_executed,original/A;original/B\n"
            "R48,software_qualified,not_executed,original/candidate.json\n")
        original = source.read_bytes()
        report = status.generate(self.root, specifications=(specification,))
        self.assertEqual(report["status"], "passed")
        output = self.root / "docs/status/ledgers/soc-ip-layout.md"
        self.assertIn("segmented_execution_passed", output.read_text())
        self.assertIn("original/A;original/B", output.read_text())
        self.assertIn("not_executed", output.read_text())
        self.assertEqual(source.read_bytes(), original)
        output.write_text(output.read_text().replace("not_executed", "passed"))
        checked = status.generate(self.root, check=True,
                                  specifications=(specification,))
        self.assertEqual(checked["status"], "failed")
        self.assertEqual(source.read_bytes(), original)

    def test_current_io_ledger_is_generated_and_checked_without_promotion(self):
        fields = ("task_id", "priority", "phase", "task", "dependencies",
                  "implementation_status", "validation_status",
                  "software_status", "hardware_status", "exit_condition",
                  "evidence_paths", "owner")
        for filename, _, _ in status.SPECIFICATIONS:
            row = dict.fromkeys(fields, "")
            row.update(task_id="TASK-01", priority="P1", phase="D1",
                       task="Original task", implementation_status="implemented",
                       validation_status="passed", owner="codex")
            path = self.root / "docs/design" / filename
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                writer.writerow(row)
        source = self.root / "docs/design/io-evolution-execution.csv"
        source.write_text(
            "task_id,task,implementation_status,software_status,hardware_status,exit_condition\n"
            "IO-19,Retain failed drain,in_progress,not_qualified,not_executed,Exact source and image\n")
        original = source.read_bytes()
        status.generate(self.root)
        output = self.root / "docs/status/ledgers/io-evolution.md"
        self.assertTrue(output.is_file(),
                        "current I/O ledger is missing from default generation")
        rendered = output.read_text()
        for value in ("IO-19", "in_progress", "not_qualified", "not_executed",
                      "Exact source and image", status.digest(source)):
            self.assertIn(value, rendered)
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(status.generate(self.root, check=True)["status"],
                         "passed")
        output.write_text(rendered.replace("not_qualified", "passed"))
        report = status.generate(self.root, check=True)
        self.assertEqual(report["status"], "failed")
        self.assertIn("io-evolution.md", " ".join(report["errors"]))
        self.assertEqual(source.read_bytes(), original)
        self.assertIn("passed", output.read_text())


class AuthorityMappingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs/design").mkdir(parents=True)
        (self.root / "docs/reference").mkdir()
        (self.root / "docs/status").mkdir()
        (self.root / "docs/status/support.md").write_text("# Support\n")
        (self.root / "docs/reference/history.md").write_text("# History\n")
        (self.root / "docs/design/source.md").write_text("# Authority\n")
        self.rows = [{"source_path": "docs/status/support.md",
                      "authority_source": "docs/design/source.md",
                      "reader_task": "Select exact mode",
                      "actual_site_paths": "status/support",
                      "doc_tasks": "DOC-01"},
                     {"source_path": "docs/reference/history.md",
                      "authority_source": "docs/reference/history.md",
                      "reader_task": "Migrate old API",
                      "actual_site_paths": "reference/history",
                      "doc_tasks": "DOC-06"}]
        self.redirects = [{"old_url": "en/api/hal.html",
                           "target_url": "en/reference/history.html",
                           "semantics": "historical_contract_version_boundary"}]
        self.write()

    def write(self):
        for path, rows in (("docs/design/documentation-migration.csv", self.rows),
                           ("docs/reference/redirects.csv", self.redirects)):
            with (self.root / path).open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)

    def test_unique_authority_and_existing_site_targets_pass(self):
        self.assertEqual(status.check_mapping(self.root)["errors"], [])

    def test_duplicate_source_and_blank_reader_task_fail(self):
        self.rows.append(dict(self.rows[0]))
        self.rows[0]["reader_task"] = ""
        self.write()
        errors = status.check_mapping(self.root)["errors"]
        self.assertIn("duplicate", " ".join(errors))
        self.assertIn("reader_task", " ".join(errors))

    def test_missing_authority_and_site_target_fail(self):
        self.rows[0]["authority_source"] = "docs/missing.md"
        self.rows[0]["actual_site_paths"] = "status/missing"
        self.write()
        errors = status.check_mapping(self.root)["errors"]
        self.assertIn("authority", " ".join(errors))
        self.assertIn("site", " ".join(errors))

    def test_source_escape_and_external_authority_are_rejected(self):
        self.rows[0]["source_path"] = "../outside.md"
        self.rows[0]["authority_source"] = "https://example.com/fake"
        self.write()
        errors = status.check_mapping(self.root)["errors"]
        self.assertIn("path", " ".join(errors))

    def test_legacy_different_contract_must_have_version_boundary(self):
        self.redirects[0]["target_url"] = "en/status/support.html"
        self.write()
        errors = status.check_mapping(self.root)["errors"]
        self.assertIn("version boundary", " ".join(errors))

    def test_unsafe_reserved_and_duplicate_redirects_are_rejected(self):
        self.redirects.append(dict(self.redirects[0]))
        self.redirects.append({"old_url": "api/index.html",
                               "target_url": "../../escape.html",
                               "semantics": "current_contract_reorganized"})
        self.write()
        errors = status.check_mapping(self.root)["errors"]
        self.assertIn("duplicate", " ".join(errors))
        self.assertIn("reserved", " ".join(errors))
        self.assertIn("path", " ".join(errors))


class ImprovementsViewTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs/design").mkdir(parents=True)
        self.path = self.root / "docs/design/documentation-system-execution.csv"
        self.rows = [
            {"task_id": "DOC-01", "priority": "P1", "phase": "D1",
             "task": "authority", "dependencies": "",
             "implementation_status": "implemented",
             "validation_status": "development_passed",
             "exit_condition": "original exit; no scope expansion", "owner": "codex"},
            {"task_id": "DOC-37", "priority": "P1", "phase": "D6",
             "task": "actual deployment", "dependencies": "DOC-01",
             "implementation_status": "implemented",
             "validation_status": "pending_formal_evidence",
             "exit_condition": "actual deployment and immutable identity", "owner": "codex"},
            {"task_id": "DOC-58", "priority": "P1", "phase": "D4",
             "task": "visual review", "dependencies": "DOC-01",
             "implementation_status": "implemented",
             "validation_status": "pending_formal_evidence",
             "exit_condition": "browser/zoom/print/offline actual execution", "owner": "codex"}]
        self.write()

    def write(self):
        with self.path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(self.rows[0]))
            writer.writeheader()
            writer.writerows(self.rows)

    def test_real_status_summary_retains_pending_without_old_planned_claim(self):
        view = status.render_improvements(self.root)
        self.assertIn("development_passed", view)
        self.assertIn("pending_formal_evidence", view)
        self.assertIn("DOC-37、DOC-58", view)
        self.assertNotIn("全部任务目前为\nplanned/not_executed", view)
        self.assertNotIn("自动同步门禁仍待", view)

    def test_original_exit_rows_are_preserved_exactly_once(self):
        view = status.render_improvements(self.root)
        for row in self.rows:
            expected = "| " + " | ".join(row[key] for key in
                        ("task_id", "priority", "task", "exit_condition")) + " |"
            self.assertEqual(view.splitlines().count(expected), 1)

    def test_unknown_phase_and_duplicate_id_fail_closed(self):
        self.rows[0]["phase"] = "D999"
        self.write()
        with self.assertRaisesRegex(ValueError, "phase"):
            status.render_improvements(self.root)
        self.rows[0]["phase"] = "D1"
        self.rows.append(dict(self.rows[0]))
        self.write()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            status.render_improvements(self.root)

    def test_checklist_participates_in_same_readonly_freshness_check(self):
        spec = [("documentation-system-execution.csv", "documentation-system", "Docs")]
        status.generate(self.root, specifications=spec)
        view = self.root / "docs/design/documentation-improvements.md"
        self.assertTrue(view.is_file())
        self.assertEqual(status.generate(self.root, check=True,
                                         specifications=spec)["status"], "passed")
        view.write_text(view.read_text().replace("actual deployment", "stale task"))
        report = status.generate(self.root, check=True, specifications=spec)
        self.assertEqual(report["status"], "failed")
        self.assertIn("documentation-improvements.md", " ".join(report["errors"]))
        self.assertIn("stale task", view.read_text())

    def test_future_validation_uses_source_values_without_completion_inference(self):
        self.rows[1]["validation_status"] = "failed"
        self.write()
        view = status.render_improvements(self.root)
        self.assertIn("failed", view)
        self.assertIn("DOC-37", view)
        self.assertNotIn("全部验收完成", view)


if __name__ == "__main__":
    unittest.main()
