"""Documentation failures must be observable before publication or commit."""

from pathlib import Path
import tempfile
import unittest

from tools.documentation import check


class DocumentationCheckTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs").mkdir()
        self.write("index.md", "# Start\n\n```{toctree}\npage\n```\n")
        self.write("page.md", "---\nid: guides.page\ntype: how-to\n"
                   "audience: consumer\nsource_language: en\n---\n"
                   "# Page\n\n(page-contract)=\n## Contract\n")

    def write(self, name, content):
        path = self.root / "docs" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def scan(self):
        return check.scan(self.root, check_diagrams=False,
                          check_configuration=False)

    def test_valid_cross_page_heading_and_explicit_anchor(self):
        self.write("index.md", "# Start\n\n```{toctree}\npage\n```\n"
                   "[heading](page.md#contract) [label](page.md#page-contract)\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_missing_file_is_reported(self):
        self.write("page.md", "# Page\n\n[missing](absent.md)\n")
        self.assertIn("missing", " ".join(self.scan()["errors"]))

    def test_missing_anchor_is_reported(self):
        self.write("page.md", "# Page\n\n[missing](index.md#absent)\n")
        self.assertIn("anchor", " ".join(self.scan()["errors"]))

    def test_duplicate_page_id_is_rejected(self):
        self.write("other.md", "---\nid: guides.page\n---\n# Other\n")
        self.assertIn("duplicate page ID", " ".join(self.scan()["errors"]))

    def test_unreachable_page_is_reported(self):
        self.write("other.md", "# Other\n")
        self.assertIn("unreachable", " ".join(self.scan()["errors"]))

    def test_archive_is_not_a_current_navigation_requirement(self):
        self.write("archive/old.md", "# Historical\n[old](absent.md)\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_code_fences_do_not_create_links(self):
        self.write("page.md", "# Page\n\n```c\n[not a link](absent)\n```\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_source_adjacent_readme_broken_link_is_reported(self):
        board = self.root / "boards/reference/README.md"
        board.parent.mkdir(parents=True)
        board.write_text("# Board\n[provider](../../soc/removed/README.md)\n")
        self.assertIn("boards/reference/README.md: missing link",
                      " ".join(self.scan()["errors"]))

    def test_source_adjacent_links_keep_fences_and_history_separate(self):
        board = self.root / "boards/reference/README.md"
        board.parent.mkdir(parents=True)
        board.write_text("# Board\n[contract](../../docs/page.md#contract)\n"
                         "```markdown\n[example](absent.md)\n```\n")
        history = self.root / "tests/evidence/old/README.md"
        history.parent.mkdir(parents=True)
        history.write_text("# Frozen record\n[old](absent.md)\n")
        (self.root / "CHANGELOG.md").write_text(
            "# Historical changes\n[removed](absent.md)\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_source_adjacent_markdown_anchor_is_checked(self):
        page = self.root / "components/reference/README.md"
        page.parent.mkdir(parents=True)
        page.write_text("# Component\n[contract](../../docs/page.md#absent)\n")
        self.assertIn("components/reference/README.md: missing anchor",
                      " ".join(self.scan()["errors"]))

    def test_literalinclude_missing_file_is_reported(self):
        self.write("page.md", "# Page\n\n```{literalinclude} absent.c\n```\n")
        self.assertIn("literalinclude", " ".join(self.scan()["errors"]))

    def test_reference_style_link_target_is_checked(self):
        self.write("page.md", "# Page\n\n[use][link]\n\n[link]: absent.md\n")
        self.assertIn("missing", " ".join(self.scan()["errors"]))

    def rst_page(self, contents):
        (self.root / "docs/page.md").unlink(missing_ok=True)
        self.write("page.rst", contents)

    def test_rst_missing_inline_link_is_reported(self):
        self.rst_page("Page\n====\n\n`missing <absent.md>`_\n")
        self.assertIn("missing link absent.md", " ".join(self.scan()["errors"]))

    def test_rst_missing_named_link_is_reported(self):
        self.rst_page("Page\n====\n\nUse reader_.\n\n.. _reader: absent.md\n")
        self.assertIn("missing link absent.md", " ".join(self.scan()["errors"]))

    def test_rst_missing_cross_page_anchor_is_reported(self):
        self.rst_page("Page\n====\n\n`missing <index.md#absent>`_\n")
        self.assertIn("missing anchor", " ".join(self.scan()["errors"]))

    def test_rst_heading_and_explicit_label_links_are_accepted(self):
        self.rst_page("Page\n====\n\n.. _contract-label:\n\n"
                      "Contract rules\n--------------\n")
        self.write("index.md", "# Start\n\n```{toctree}\npage\n```\n"
                   "[heading](page.rst#contract-rules) "
                   "[label](page.rst#contract-label)\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_rst_literal_code_does_not_create_links(self):
        self.rst_page("Page\n====\n\n.. code-block:: rst\n\n"
                      "    `example <absent.md>`_\n    .. image:: absent.png\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_rst_missing_image_and_include_inputs_are_reported(self):
        for directive in ("include", "image", "figure"):
            with self.subTest(directive=directive):
                self.rst_page("Page\n====\n\n.. " + directive
                              + ":: absent.txt\n")
                self.assertIn("absent.txt", " ".join(self.scan()["errors"]))

    def test_fenced_include_documentation_is_not_a_source_dependency(self):
        self.write("page.md", "# Page\n\n````markdown\n"
                   "```{literalinclude} absent.c\n```\n"
                   ":::{image} absent.png\n:::\n````\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_nested_include_cycle_is_rejected(self):
        self.write("page.md", "# Page\n\n```{include} fragment.txt\n```\n")
        self.write("fragment.txt", "```{include} page.md\n```\n")
        self.assertIn("include cycle", " ".join(self.scan()["errors"]))

    def test_colon_fenced_local_image_is_a_checked_input(self):
        self.write("page.md", "# Page\n\n:::{image} absent.png\n"
                   ":alt: example\n:::\n")
        self.assertIn("absent.png", " ".join(self.scan()["errors"]))

    def test_include_symlink_is_rejected(self):
        target = self.write("consumer.c", "int consumer(void) { return 0; }\n")
        self.write("page.md", "# Page\n\n```{literalinclude} linked.c\n```\n")
        (self.root / "docs/linked.c").symlink_to(target)
        self.assertIn("symlink", " ".join(self.scan()["errors"]))

    def test_include_filenames_are_not_uri_decoded(self):
        for name in ("consumer.c#extra", "consumer%2Ec", "<consumer.c>",
                     "consumer.c?extra", "scheme:consumer.c"):
            with self.subTest(name=name):
                actual = self.write(name, "int actual_input;\n")
                self.write("consumer.c", "int decoy_input;\n")
                self.write("page.md", "# Page\n\n```{literalinclude} "
                           + name + "\n```\n")
                self.assertEqual(check.dependencies(self.root), {actual})

    def test_image_paths_still_use_uri_decoding(self):
        image = self.write("consumer.svg", '<svg viewBox="0 0 1 1"/>')
        self.write("page.md", "# Page\n\n![image](consumer%2Esvg#part)\n")
        self.assertEqual(check.dependencies(self.root), {image})

    def test_symlink_escape_is_rejected(self):
        outside = Path(self.directory.name).parent / "outside-doc.md"
        (self.root / "docs" / "linked.md").symlink_to(outside)
        self.assertIn("symlink", " ".join(self.scan()["errors"]))

    def test_unknown_page_language_is_rejected(self):
        self.write("page.md", "---\nid: guides.page\nsource_language: zz\n---\n# Page\n")
        self.assertIn("source_language", " ".join(self.scan()["errors"]))

    def test_glob_toctree_reaches_real_markdown_pages(self):
        self.write("index.md", "# Start\n\n```{toctree}\n:glob:\n*\n```\n")
        self.assertEqual(self.scan()["errors"], [])

    def test_public_api_pages_are_included(self):
        self.write("reference/api/index.md", "# Public API\n")
        self.assertIn("unreachable", " ".join(self.scan()["errors"]))

    def test_duplicate_explicit_anchor_is_rejected(self):
        self.write("page.md", "# Page\n\n(contract)=\n## One\n\n(contract)=\n## Two\n")
        self.assertIn("duplicate anchor", " ".join(self.scan()["errors"]))

    def test_svg_with_script_or_external_resource_is_rejected(self):
        for element in ('<script/>', '<image href="https://example.com/x"/>'):
            path = self.write("assets/test.svg", '<svg xmlns="http://www.w3.org/2000/svg">'
                              + element + '</svg>')
            with self.assertRaisesRegex(ValueError, "SVG"):
                check.validate_svg(path)

    def test_svg_safe_local_structure_is_allowed(self):
        path = self.write("assets/test.svg", '<svg xmlns="http://www.w3.org/2000/svg" '
                          'viewBox="0 0 10 10"><text>中文</text></svg>')
        check.validate_svg(path)

    def test_svg_style_import_is_rejected(self):
        path = self.write("assets/test.svg", '<svg xmlns="http://www.w3.org/2000/svg" '
                          'viewBox="0 0 10 10"><style>@import "https://example.com/a.css";'
                          '</style></svg>')
        with self.assertRaisesRegex(ValueError, "SVG"):
            check.validate_svg(path)


    def test_rst_toml_uses_the_real_resolver_and_rejects_bad_mode(self):
        from tools.configure import configure, documentation
        source = (configure.ROOT / "tools/configure/assemblies/qiming-baremetal-gpio.toml").read_text()
        rst = ".. code-block:: toml\n\n" + "\n".join("    " + line for line in source.splitlines()) + "\n"
        converted = check.configuration_content(rst, ".rst")
        path = self.root / "docs/example.rst"
        rows = documentation.check_document(path, configure.ROOT, contents=converted)
        self.assertEqual(rows[0]["kind"], "assembly")
        with self.assertRaisesRegex(ValueError, "Unsupported mode"):
            documentation.check_document(path, configure.ROOT,
                contents=converted.replace('mode = "output"', 'mode = "unsupported"'))

    def test_required_translation_rejects_missing_stale_or_unreviewed(self):
        import hashlib
        import json
        source = "---\nid: start\nsource_language: en\n---\n# Start\n"
        self.write("index.md", source)
        def validate():
            return check.validate_translations(self.root, critical_pages=("index",))
        self.assertTrue(validate()["errors"])
        digest = hashlib.sha256(source.encode()).hexdigest()
        catalog = self.root / "docs/locale/zh_CN/LC_MESSAGES/index.po"
        catalog.parent.mkdir(parents=True)
        header = ('Language: zh_CN\nX-Nexus-Source-SHA256: ' + digest +
                  '\nX-Nexus-Reviewed: false\n')
        catalog.write_text('msgid ""\nmsgstr ' + json.dumps(header) +
                           '\n\nmsgid "Start"\nmsgstr "开始"\n')
        self.assertTrue(validate()["errors"])
        catalog.write_text(catalog.read_text().replace('Reviewed: false', 'Reviewed: true'))
        self.assertEqual(validate()["errors"], [])
        self.write("index.md", source + "Changed.\n")
        self.assertTrue(validate()["errors"])

    def test_external_404_and_network_failure_remain_distinct(self):
        from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
        import threading

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(404 if self.path == "/bad" else 200)
                self.end_headers()

            def log_message(self, *args):
                pass

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        url = f"http://127.0.0.1:{server.server_port}"
        rows = check.external_links([url, url + "/bad"], timeout=1, retries=0)
        self.assertEqual([row["status"] for row in rows], ["passed", "http_error"])
        server.shutdown(); server.server_close()
        failed = check.external_links([url], timeout=0.1, retries=0)
        self.assertEqual(failed[0]["status"], "network_error")


class DottedDocnameRegression(unittest.TestCase):
    def test_actual_scan_reaches_dotted_docnames_and_explicit_source_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            (docs / "status").mkdir(parents=True)
            (docs / "index.md").write_text(
                "# Start\n\n```{toctree}\nstatus/release-0.2.1\n"
                "Migration <status/migration.v2>\nstatus/explicit-0.1.0.md\n"
                "status/explicit-v2.0.rst\n```\n")
            for name, contents in (
                    ("release-0.2.1.md", "# Release\n"),
                    ("migration.v2.rst", "Migration\n=========\n"),
                    ("explicit-0.1.0.md", "# Explicit Markdown\n"),
                    ("explicit-v2.0.rst", "Explicit RST\n============\n")):
                (docs / "status" / name).write_text(contents)
            result = check.scan(root, check_diagrams=False, check_configuration=False)
            self.assertEqual(result["errors"], [])
            self.assertEqual(result["pages"], 5)

    def test_dotted_rst_toctree_resolves_full_name_before_suffix_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            docs.mkdir()
            (docs / "index.md").write_text(
                "# Start\n\n```{toctree}\nstatus\n```\n")
            (docs / "status.rst").write_text(
                "Status\n======\n\n.. toctree::\n\n    release-0.1.0\n")
            (docs / "release-0.1.0.rst").write_text("Release\n=======\n")
            result = check.scan(root, check_diagrams=False, check_configuration=False)
            self.assertEqual(result["errors"], [])

    def test_missing_dotted_target_never_resolves_a_truncated_decoy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            docs.mkdir()
            index = docs / "index.md"
            index.write_text("# Start\n\n```{toctree}\nrelease-0.1.0\n```\n")
            (docs / "release-0.1.md").write_text("# Decoy\n")
            self.assertEqual(check.navigation(index, index.read_text()),
                             [(docs / "release-0.1.0.md").resolve()])
            result = check.scan(root, check_diagrams=False, check_configuration=False)
            self.assertIn("missing toctree target", " ".join(result["errors"]))


class SourceSDKInstructionTests(unittest.TestCase):
    def test_complete_consumer_instructions_export_the_required_profile(self):
        import importlib.util
        import shlex
        from contextlib import redirect_stdout
        import io
        from unittest.mock import patch

        root = Path(__file__).resolve().parents[2]
        module_spec = importlib.util.spec_from_file_location(
            "documented_source_sdk", root / "cmake/package/package_source_sdk.py")
        packager = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(packager)
        source = (root / "docs/tutorials/source-sdk.md").read_text()
        commands = [shlex.split(line) for block in check.FENCE.finditer(source)
                    if block[2].strip() == "sh"
                    for line in block[3].replace("\\\n", " ").splitlines()
                    if line.strip()]
        command = next(argv for argv in commands
                       if len(argv) > 2 and argv[1].endswith(
                           "cmake/package/package_source_sdk.py")
                       and "--source" in argv and "--output" in argv)
        result = {"kind": "source_snapshot", "source_revision": "a" * 40,
                  "publishable": True, "files_sha256": {},
                  "snapshot_sha256": "b" * 64}
        with patch.object(packager, "prepare", return_value=result) as prepare, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(packager.main(command[2:]), 0)
        profile = prepare.call_args.kwargs["profile"]
        self.assertTrue(profile["host_support"],
                        "The complete plan starts with a Native consumer")
        selected = set(packager.runtime_dependency_closure(profile))
        self.assertTrue({"ext/lwip", "ext/mbedtls", "ext/coremqtt",
                         "ext/mcuboot"}.issubset(selected),
                        "The complete plan includes network, TLS and boot users")


if __name__ == "__main__":
    unittest.main()
