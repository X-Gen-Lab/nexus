"""Behavioral tests for lexical comment detection and sealed legacy debt."""

import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO

from scripts.ci import comment_style as gate


def header(name="sample.c"):
    fields = ("file", "brief", "author")
    if Path(name).suffix in gate.SOURCE_SUFFIXES:
        fields += ("version", "date", "copyright")
    return "/**\n" + "".join(
        f" * \\{field:<16} {name if field == 'file' else 'Example'}\n"
        for field in fields
    ) + " */\n"


class CommentLexerTests(unittest.TestCase):
    def inspect(self, body, name="sample.c"):
        return gate.inspect_source(name, (header(name) + body).encode())

    def test_real_comments_are_distinct_from_urls_and_character_literals(self):
        body = r'''
const char* url = "https://example.test/@brief/*===*/";
const char* escaped = "quote: \" and slash: \\ //";
char quote = '\'';
char slash = '/';
/* URL https://example.test/path is plain prose. */
// this is a prohibited comment
'''
        facts = self.inspect(body)
        self.assertEqual(facts["rules"], {"line_comment": 1})
        self.assertEqual(facts["diagnostics"][0]["line"], 15)

    def test_all_raw_literal_prefixes_hide_comment_like_contents(self):
        body = "\n".join(
            prefix + r'"tag(https://example.test/" // @brief /*===*/ )tag";'
            for prefix in ("R", "u8R", "uR", "UR", "LR")
        )
        self.assertEqual(self.inspect(body, "sample.cpp")["rules"], {})

    def test_raw_string_splicing_does_not_invent_an_early_terminator(self):
        body = 'const char* text = R"x(fake )x\\\n" // @brief\n)x";\n'
        self.assertEqual(self.inspect(body, "sample.cpp")["rules"], {})

    def test_line_splicing_forms_comments_and_keeps_physical_locations(self):
        body = "/\\\n/ continued \\\n @brief\nint value;\n"
        facts = self.inspect(body)
        self.assertEqual(facts["rules"], {"at_doxygen_tag": 1, "line_comment": 1})
        positions = {item["rule"]: item["line"] for item in facts["diagnostics"]}
        self.assertEqual(positions, {"line_comment": 9, "at_doxygen_tag": 11})

    def test_spliced_string_hides_url_on_next_physical_line(self):
        body = 'const char* text = "https:\\\n//example.test/@brief";\n'
        self.assertEqual(self.inspect(body)["rules"], {})

    def test_digit_separators_do_not_hide_following_comments(self):
        body = "unsigned value = 1'000 + 0xAB'CD;\n// genuine comment\n"
        self.assertEqual(self.inspect(body, "sample.cpp")["rules"], {"line_comment": 1})

    def test_email_is_not_an_at_style_command(self):
        self.assertEqual(self.inspect("/* Contact team@example.test */\n")["rules"], {})

    def test_comment_rules_and_source_documentation(self):
        body = "/** @brief @param x wrong\n * === separator ===\n */\n"
        body += "/** \\param[in] x: duplication\n * \\return value\n * \\retval OK\n */\n"
        facts = self.inspect(body)
        self.assertEqual(facts["rules"], {
            "at_doxygen_tag": 2,
            "equals_separator": 1,
            "source_api_documentation": 3,
        })

    def test_header_api_documentation_and_hyphen_sections_are_allowed(self):
        body = "/** \\brief Example\n * \\param[out] output: Value\n * \\return OK\n */\n"
        body += "/*---------------------------------------------------------------------------*/\n"
        self.assertEqual(self.inspect(body, "sample.hpp")["rules"], {})

    def test_both_block_section_spellings_are_rejected(self):
        body = "/*=== Ordinary ===*/\n/**=== Doxygen ===*/\n"
        self.assertEqual(self.inspect(body)["rules"], {"equals_separator": 2})

    def test_source_brief_details_note_are_allowed(self):
        body = "/** \\brief Example\n * \\details Implementation\n * \\note Constraint\n */\n"
        self.assertEqual(self.inspect(body)["rules"], {})

    def test_missing_header_is_not_hidden_by_comments_after_code(self):
        facts = gate.inspect_source("sample.c", b"int value;\n" + header().encode())
        self.assertEqual(facts["rules"], {"file_header_missing": 1})

    def test_source_and_header_have_different_required_fields(self):
        partial = header("sample.h").encode()
        self.assertEqual(gate.inspect_source("sample.h", partial)["rules"], {})
        self.assertEqual(gate.inspect_source("sample.c", partial)["rules"],
                         {"file_header_field_missing": 3})

    def test_private_fragment_requires_basic_header_without_tu_fields(self):
        contents = header("sample.h") + "/** \\param x: private fragment */\n"
        facts = gate.inspect_source("sample.inc", contents.encode())
        self.assertEqual(facts["rules"], {})
        self.assertEqual(gate.inspect_source("sample.inc", b"int field;")["rules"],
                         {"file_header_missing": 1})

    def test_public_in_memory_api_preserves_issue_fields(self):
        issues = gate.analyze_source(header() + "// violation\n", "sample.c")
        self.assertEqual(len(issues), 1)
        self.assertEqual((issues[0].rule, issues[0].line, issues[0].column),
                         ("line_comment", 9, 1))
        self.assertIn("/*", issues[0].message)

    def test_license_before_doxygen_header_is_allowed(self):
        contents = "/* License retained. */\n" + header() + "int value;\n"
        self.assertEqual(gate.inspect_source("sample.c", contents.encode())["rules"], {})
        contents = "/** License retained. */\n" + header() + "int value;\n"
        self.assertEqual(gate.inspect_source("sample.c", contents.encode())["rules"], {})

    def test_malformed_literals_and_encoding_fail_closed(self):
        for body in ('"unterminated\n', "/* unterminated", 'R"x(unclosed'):
            with self.subTest(body=body):
                self.assertIn("lexical_error", self.inspect(body)["rules"])
        self.assertEqual(gate.inspect_source("sample.c", b"\xff")["rules"],
                         {"lexical_error": 1})

    def test_bom_and_crlf_do_not_break_detection(self):
        text = (header() + "// violation\n").replace("\n", "\r\n")
        facts = gate.inspect_source("sample.c", b"\xef\xbb\xbf" + text.encode())
        self.assertEqual(facts["rules"], {"line_comment": 1})


class FrozenDebtTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / "sample.c"
        self.path.write_text(header() + "// old debt\n")
        facts = gate.inspect_source("sample.c", self.path.read_bytes())
        self.baseline = {
            "schema_version": 1,
            "ruleset_version": 1,
            "files": {"sample.c": {
                "source_sha256": facts["source_sha256"], "rules": facts["rules"],
            }},
        }

    def check(self, baseline=None):
        return gate.check_files(self.root, [self.path], baseline)

    def test_unlisted_and_new_files_are_strict(self):
        self.assertEqual(self.check()["status"], "failed")
        baseline = copy.deepcopy(self.baseline)
        baseline["files"] = {}
        self.assertEqual(self.check(baseline)["status"], "failed")

    def test_wholly_unchanged_file_can_retain_frozen_debt(self):
        report = self.check(self.baseline)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["files"]["sample.c"]["baseline_status"],
                         "unchanged_frozen_debt")
        self.assertEqual(report["files"]["sample.c"]["rules"], {"line_comment": 1})
        self.assertEqual(report["files"]["sample.c"]["failures"], [])

    def test_same_count_modified_violation_cannot_replace_old_debt(self):
        self.path.write_text(header() + "// a replacement with the same rule count\n")
        report = self.check(self.baseline)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["files"]["sample.c"]["rules"], {"line_comment": 1})
        self.assertEqual(report["files"]["sample.c"]["baseline_status"], "changed_strict")

    def test_even_unrelated_modification_requires_all_debt_fixed(self):
        self.path.write_text(self.path.read_text() + "int new_value;\n")
        self.assertEqual(self.check(self.baseline)["status"], "failed")

    def test_partial_reduction_of_modified_file_is_still_strict(self):
        self.path.write_text(header() + "// one\n// two\n")
        facts = gate.inspect_source("sample.c", self.path.read_bytes())
        baseline = copy.deepcopy(self.baseline)
        baseline["files"]["sample.c"] = {
            "source_sha256": facts["source_sha256"], "rules": facts["rules"],
        }
        self.path.write_text(header() + "// one\n")
        self.assertEqual(self.check(baseline)["status"], "failed")

    def test_all_debt_fixed_and_removed_baseline_entry_pass(self):
        self.path.write_text(header() + "int value;\n")
        self.assertEqual(self.check(self.baseline)["status"], "passed")
        baseline = copy.deepcopy(self.baseline)
        del baseline["files"]["sample.c"]
        self.assertEqual(self.check(baseline)["status"], "passed")

    def test_baseline_may_not_allow_unrecorded_rule_even_at_same_hash(self):
        baseline = copy.deepcopy(self.baseline)
        baseline["files"]["sample.c"]["rules"] = {}
        self.assertEqual(self.check(baseline)["status"], "failed")

    def test_baseline_cannot_hide_missing_file_or_symlink(self):
        self.path.unlink()
        with self.assertRaises(gate.StyleError):
            self.check(self.baseline)
        other = self.root / "other.c"
        other.write_text(header())
        self.path.symlink_to(other)
        with self.assertRaises(gate.StyleError):
            self.check(self.baseline)

    def test_empty_scope_and_outside_paths_are_rejected(self):
        with self.assertRaises(gate.StyleError):
            gate.check_files(self.root, [], self.baseline)
        with self.assertRaises(gate.StyleError):
            gate.check_files(self.root, [self.root.parent / "outside.c"], self.baseline)

    def test_invalid_versions_hashes_counts_and_paths_are_rejected(self):
        cases = [
            ("schema_version", True), ("schema_version", 1.0),
            ("ruleset_version", 2),
        ]
        for key, value in cases:
            baseline = copy.deepcopy(self.baseline)
            baseline[key] = value
            with self.assertRaises(gate.StyleError):
                gate.validate_baseline(baseline)
        for name in ("../bad.c", "/bad.c", "./bad.c", "bad\\name.c"):
            baseline = copy.deepcopy(self.baseline)
            baseline["files"][name] = baseline["files"].pop("sample.c")
            with self.assertRaises(gate.StyleError):
                gate.validate_baseline(baseline)
        for count in (0, -1, True, "1"):
            baseline = copy.deepcopy(self.baseline)
            baseline["files"]["sample.c"]["rules"]["line_comment"] = count
            with self.assertRaises(gate.StyleError):
                gate.validate_baseline(baseline)
        baseline = copy.deepcopy(self.baseline)
        baseline["files"]["sample.c"]["source_sha256"] = "not-a-digest"
        with self.assertRaises(gate.StyleError):
            gate.validate_baseline(baseline)

    def test_duplicate_json_keys_are_rejected(self):
        path = self.root / "baseline.json"
        path.write_text('{"schema_version":1,"schema_version":1,'
                        '"ruleset_version":1,"files":{}}')
        with self.assertRaisesRegex(gate.StyleError, "Duplicate"):
            gate.load_baseline(path)

    def test_baseline_is_read_only_and_source_hash_is_exact(self):
        path = self.root / "baseline.json"
        path.write_text(json.dumps(self.baseline))
        before = path.read_bytes()
        report = self.check(gate.load_baseline(path))
        self.assertEqual(path.read_bytes(), before)
        self.assertEqual(report["files"]["sample.c"]["source_sha256"],
                         hashlib.sha256(self.path.read_bytes()).hexdigest())


class CommandLineTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / ".clang-format-dirs").write_text(
            "owned\n!owned/vendor\n[extensions]\n.c\n.h\n.cpp\n.inc\n"
        )
        (self.root / "owned").mkdir()
        self.source = self.root / "owned/sample.c"
        self.source.write_text(header())

    def run_cli(self, *arguments):
        output = StringIO()
        errors = StringIO()
        with redirect_stdout(output), redirect_stderr(errors):
            result = gate.main(["--root", str(self.root), *arguments])
        return result, output.getvalue(), errors.getvalue()

    def test_explicit_scope_uses_shared_manifest_without_clang_dependency(self):
        code, output, _ = self.run_cli("--files", "owned/sample.c", "--json")
        self.assertEqual(code, 0)
        report = json.loads(output)
        self.assertEqual(report["checked_files"], 1)
        self.assertIn("owned/sample.c", report["files"])

    def test_cli_violation_and_baseline_leave_sources_unchanged(self):
        self.source.write_text(header() + "// debt\n")
        before = self.source.read_bytes()
        code, output, _ = self.run_cli("--files", str(self.source), "--json")
        self.assertEqual(code, 1)
        facts = json.loads(output)["files"]["owned/sample.c"]
        baseline = {
            "schema_version": 1, "ruleset_version": 1,
            "files": {"owned/sample.c": {
                "source_sha256": facts["source_sha256"], "rules": facts["rules"],
            }},
        }
        (self.root / "baseline.json").write_text(json.dumps(baseline))
        code, output, _ = self.run_cli(
            "--files", "owned/sample.c", "--baseline", "baseline.json"
        )
        self.assertEqual(code, 0)
        self.assertIn("1 unchanged files with frozen debt", output)
        self.assertEqual(self.source.read_bytes(), before)

    def test_excluded_and_unowned_explicit_files_fail(self):
        vendor = self.root / "owned/vendor"
        vendor.mkdir()
        (vendor / "sample.c").write_text(header())
        (self.root / "outside.c").write_text(header())
        for name in ("owned/vendor/sample.c", "outside.c"):
            with self.subTest(name=name):
                with redirect_stderr(StringIO()):
                    with self.assertRaises(SystemExit) as raised:
                        gate.main(["--root", str(self.root), "--files", name])
                self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
