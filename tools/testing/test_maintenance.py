"""Maintained support text and review assignments reject stale declarations."""

import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest

from tools.maintenance import capabilities, ownership

ROOT = Path(__file__).resolve().parents[2]


class CapabilityDocumentationTests(unittest.TestCase):
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

    def test_current_matrix_uses_actual_resolver_and_preserves_scope(self):
        rows = capabilities.collect(ROOT)
        self.assertTrue(rows)
        uart_dma = [row for row in rows if row["family"] == "stm32f407"
                    and row["controller"] == "USART1"
                    and row["mode"] == "dma-tx"]
        self.assertEqual(len(uart_dma), 1)
        self.assertEqual(uart_dma[0]["exports"],
                         ["soc/stm32f407/drivers/uart_dma.c"])
        self.assertTrue(uart_dma[0]["fixtures"])
        spi = next(row for row in rows if row["family"] == "stm32f407"
                   and row["controller"] == "SPI1" and row["mode"] == "dma")
        self.assertEqual(spi["boards"], [])

    def test_checked_document_matches_facts_and_selected_implementation(self):
        expected = capabilities.render(capabilities.collect(ROOT))
        document = (ROOT / capabilities.DOCUMENT).read_text()
        capabilities.check_block(document, expected)

    def test_redistributed_references_preserve_selected_source_provenance(self):
        directory = ROOT / "cmake/package/verification/io_evolution"
        fixtures = sorted(directory.glob("*.toml"))
        self.assertTrue(fixtures)
        rows = capabilities.collect(ROOT)
        for fixture in fixtures:
            resolved = capabilities.configure.resolve(fixture, ROOT)
            relative = fixture.relative_to(ROOT).as_posix()
            for item in resolved["controllers"]:
                with self.subTest(fixture=fixture.name, device=item["id"]):
                    selected = [row for row in rows if
                                (row["family"], row["controller"], row["kind"],
                                 row["mode"]) ==
                                (resolved["soc_family"], item["controller"],
                                 item["kind"], item["mode"])]
                    self.assertEqual(len(selected), 1)
                    self.assertIn(relative, selected[0]["fixtures"])
                    self.assertTrue(selected[0]["exports"])

    def test_alternate_interface_uses_its_exact_route_and_source_on_one_timer(self):
        rows = capabilities.collect(ROOT)
        timer = [row for row in rows if row["family"] == "stm32g474" and
                 row["controller"] == "TIM3"]
        self.assertEqual({(row["kind"], row["mode"]) for row in timer},
                         {("pwm", "fixed-pwm"),
                          ("pwm", "shared-fixed-pwm"),
                          ("encoder", "quadrature")})
        encoder = next(row for row in timer if row["kind"] == "encoder")
        pwm = next(row for row in timer if row["mode"] == "fixed-pwm")
        shared = next(row for row in timer
                      if row["mode"] == "shared-fixed-pwm")
        self.assertEqual(encoder["routes"], ["tim3-encoder-pa6-pa7"])
        self.assertEqual(pwm["routes"], ["tim3-ch1-pa6"])
        self.assertEqual(shared["routes"], ["tim3-ch1-pa6", "tim3-ch2-pa7"])
        self.assertEqual(shared["exports"],
                         ["soc/stm32g474/drivers/pwm_shared.c"])
        self.assertIn("cmake/package/verification/io_evolution/"
                      "g474-pwm-shared.toml",
                      shared["fixtures"])
        self.assertEqual(encoder["exports"],
                         ["soc/stm32g474/drivers/encoder.c"])
        self.assertIn("tests/contracts/g474_control_assembly/encoder.toml",
                      encoder["fixtures"])
        self.assertEqual(encoder["boards"], [])


class AgentMaintenanceTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads(
            (ROOT / ".github/maintainer-roles.json").read_text())
        self.document["assignment_status"] = "agent_managed"
        self.document["maintenance"] = {
            "mode": "agent_managed",
            "owner": "codex",
            "review_policy": "agent_review_and_automated_gates",
            "required_checks": ["format", "comments", "tdd",
                                "host-contracts", "arm-builds",
                                "resource-budgets", "sdk-consumers",
                                "evidence-identity"],
        }
        for role in self.document["roles"]:
            role["members"] = []

    def test_agent_responsibilities_do_not_require_named_people(self):
        report = ownership.validate(self.document, require_assigned=True)
        self.assertEqual(report["status"], "agent_managed")
        self.assertEqual(report["responsible_agent"], "codex")
        self.assertEqual(report["unassigned_roles"], [])
        self.assertFalse(report["named_members_required"])
        self.assertFalse(report["independent_human_review_verified"])
        self.assertFalse(report["branch_protection_verified"])

    def test_agent_policy_cannot_fabricate_codeowners(self):
        with self.assertRaisesRegex(ValueError, "GitHub accounts"):
            ownership.render(self.document)

    def test_agent_policy_requires_real_execution_gate_names(self):
        for mutation in ("missing-check", "duplicate-check", "unknown-owner",
                         "human-account", "unknown-field"):
            document = copy.deepcopy(self.document)
            if mutation == "missing-check":
                document["maintenance"]["required_checks"].remove("tdd")
            elif mutation == "duplicate-check":
                document["maintenance"]["required_checks"].append("tdd")
            elif mutation == "unknown-owner":
                document["maintenance"]["owner"] = "@fake-person"
            elif mutation == "human-account":
                document["roles"][0]["members"] = ["@fake-person"]
            else:
                document["maintenance"]["branch_protection_verified"] = True
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                ownership.validate(document, require_assigned=True)


class ReviewOwnershipTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / ".github/maintainer-roles.json"
        self.document = json.loads(path.read_text())
        self.document.pop("maintenance", None)
        self.document["assignment_status"] = (
            "roles_defined_named_team_assignment_pending")
        for role in self.document["roles"]:
            role["members"] = []

    def test_template_is_valid_but_not_an_assigned_team(self):
        report = ownership.validate(self.document)
        self.assertEqual(report["seats"], 10)
        self.assertEqual(report["status"], "pending")
        with self.assertRaisesRegex(ValueError, "unassigned"):
            ownership.validate(self.document, require_assigned=True)

    def assigned(self):
        document = copy.deepcopy(self.document)
        document["assignment_status"] = "assigned"
        for role in document["roles"]:
            role["members"] = [f"@test-{role['id']}-{index}"
                               for index in range(role["seats"])]
        return document

    def test_assigned_team_generates_complete_review_output(self):
        document = self.assigned()
        report = ownership.validate(document, require_assigned=True)
        self.assertEqual(report["status"], "assigned")
        text = ownership.render(document)
        self.assertIn("/core/ @test-core-interfaces-0", text)
        self.assertIn("/soc/ @test-bsp-ports-0", text)
        self.assertIn("/tests/ @test-quality-hil-0", text)
        self.assertLess(text.index("/** "), text.index("/core/ "))
        self.assertLess(text.index("/components/ "),
                        text.index("/components/storage/ "))
        self.assertTrue(text.endswith("\n"))

    def test_invalid_assignments_are_rejected(self):
        for mutation in ("duplicate-role", "unknown-backup", "malformed-member",
                         "wrong-seats", "unknown-key", "duplicate-member"):
            document = self.assigned()
            if mutation == "duplicate-role":
                document["roles"][1]["id"] = document["roles"][0]["id"]
            elif mutation == "unknown-backup":
                document["roles"][0]["backup_role"] = "missing"
            elif mutation == "malformed-member":
                document["roles"][0]["members"] = ["@bad account"]
            elif mutation == "wrong-seats":
                document["roles"][0]["seats"] = 2
            elif mutation == "unknown-key":
                document["branch_protection_enabled"] = True
            else:
                account = document["roles"][0]["members"][0]
                document["roles"][1]["members"][0] = account
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                ownership.validate(document, require_assigned=True)

    def test_pending_team_cannot_publish_an_ownership_file(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "CODEOWNERS"
            with self.assertRaisesRegex(ValueError, "unassigned"):
                ownership.write(self.document, output)
            self.assertFalse(output.exists())

    def test_malformed_lists_and_conflicting_paths_are_rejected(self):
        for mutation in ("nested-member", "duplicate-path", "boolean-schema",
                         "invalid-backup", "missing-note"):
            document = self.assigned()
            if mutation == "nested-member":
                document["roles"][0]["members"] = [{}]
            elif mutation == "duplicate-path":
                document["roles"][1]["review_paths"] = \
                    document["roles"][0]["review_paths"]
            elif mutation == "boolean-schema":
                document["schema_version"] = True
            elif mutation == "invalid-backup":
                document["roles"][0]["backup_role"] = []
            else:
                document["note"] = ""
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                ownership.validate(document, require_assigned=True)

    def test_cli_rejects_duplicate_json_keys_without_publication(self):
        document = json.dumps(self.assigned()).replace(
            '"schema_version": 1',
            '"schema_version": 2, "schema_version": 1', 1)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "team.json"
            output = Path(directory) / "CODEOWNERS"
            source.write_text(document)
            with redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
                result = ownership.main(["--input", str(source),
                                         "--output", str(output)])
            self.assertEqual(result, 1)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
