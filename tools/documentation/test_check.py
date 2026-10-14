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

    def test_ledger_cycle_and_missing_dependency_are_rejected(self):
        path = self.write("tasks.csv", "task_id,dependencies\nDOC-01,DOC-02\nDOC-02,DOC-01\n")
        with self.assertRaisesRegex(ValueError, "cycle"):
            check.validate_ledger(path)

    def test_rst_toml_uses_the_real_resolver_and_rejects_bad_mode(self):
        from tools.configure import configure, documentation
        source = (configure.ROOT / "tools/configure/assemblies/native-gpio.toml").read_text()
        rst = ".. code-block:: toml\n\n" + "\n".join("    " + line for line in source.splitlines()) + "\n"
        converted = check.configuration_content(rst, ".rst")
        path = self.root / "docs/example.rst"
        rows = documentation.check_document(path, configure.ROOT, contents=converted)
        self.assertEqual(rows[0]["kind"], "assembly")
        with self.assertRaisesRegex(ValueError, "Unsupported mode"):
            documentation.check_document(path, configure.ROOT,
                contents=converted.replace('mode = "output"', 'mode = "unsupported"'))
        path.write_text("task_id,dependencies\nDOC-01,DOC-99\n")
        with self.assertRaisesRegex(ValueError, "dependency"):
            check.validate_ledger(path)

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


if __name__ == "__main__":
    unittest.main()
