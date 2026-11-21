"""Documentation failures must be observable before publication or commit."""

from pathlib import Path
import tempfile
import unittest

from tools.documentation import check


class ExplicitPageContractTests(unittest.TestCase):
    def source(self, **changes):
        fields = {"id": "guides.page", "type": "how-to",
                  "audience": "consumer", "source_language": "en"}
        fields.update(changes)
        return "---\n" + "\n".join(
            name + ": " + value for name, value in fields.items()
            if value is not None) + "\n---\n# Page\n"

    def test_each_identity_field_is_explicit_and_nonempty(self):
        for name in ("id", "type", "audience", "source_language"):
            for value in (None, ""):
                with self.subTest(field=name, value=value):
                    with self.assertRaisesRegex(ValueError, name):
                        check.metadata(self.source(**{name: value}))

    def test_type_and_identifier_are_validated_without_path_guesses(self):
        for fields in ({"type": "guide"}, {"type": "status"},
                       {"id": "../guide"}):
            with self.subTest(fields=fields):
                with self.assertRaises(ValueError):
                    check.metadata(self.source(**fields))

    def test_metadata_is_required_even_for_an_obvious_language(self):
        with self.assertRaises(ValueError):
            check.metadata("# English heading\n中文也不决定来源语言。\n")

    def test_every_markdown_page_has_the_same_source_policy(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths = [root / "docs" / name for name in
                     ("index.md", "README.md", "api/README.md",
                      "archive/contract.md", "guides/new/README.md")]
            for path in paths:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(self.source())
            self.assertEqual(check.sources(root), sorted(paths))

    def test_rst_is_rejected_instead_of_silently_omitted_or_rendered(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "docs").mkdir()
            (root / "docs/index.md").write_text(self.source())
            (root / "docs/old.rst").write_text("Old\n===\n")
            result = check.scan(root, check_diagrams=False,
                                check_configuration=False,
                                check_navigation=False)
            self.assertIn("Markdown", " ".join(result["errors"]))
            self.assertNotIn(root / "docs/old.rst", check.sources(root))


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
        if path.suffix == ".md" and not content.startswith("---\n"):
            content = ("---\nid: " + name.replace("/", ".").removesuffix(".md")
                       + "\ntype: reference\naudience: consumer\n"
                       "source_language: en\n---\n" + content)
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
        self.write("other.md", "---\nid: guides.page\ntype: reference\n"
                   "audience: consumer\nsource_language: en\n---\n# Other\n")
        self.assertIn("duplicate page ID", " ".join(self.scan()["errors"]))

    def test_unreachable_page_is_reported(self):
        self.write("other.md", "# Other\n")
        self.assertIn("unreachable", " ".join(self.scan()["errors"]))

    def test_archive_name_does_not_hide_a_current_markdown_source(self):
        self.write("archive/old.md", "# Historical\n[old](absent.md)\n")
        self.assertIn("missing link", " ".join(self.scan()["errors"]))

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
        self.write("page.md", "---\nid: guides.page\ntype: reference\naudience: consumer\nsource_language: zz\n---\n# Page\n")
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



    def test_required_translation_rejects_missing_stale_or_unreviewed(self):
        import hashlib
        import json
        source = ("---\nid: start\ntype: reference\naudience: consumer\n"
                  "source_language: en\n---\n# Start\n")
        self.write("index.md", source)
        def validate():
            return check.validate_translations(self.root, pages=("index",))
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
    def page(self, docs, name, body):
        path = docs / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("---\nid: " + name.replace("/", ".").removesuffix(".md")
                        + "\ntype: reference\naudience: consumer\n"
                        "source_language: en\n---\n" + body)
        return path

    def test_actual_scan_reaches_dotted_and_explicit_markdown_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            self.page(docs, "index.md", "# Start\n\n```{toctree}\n"
                      "status/release-0.2.1\nMigration <status/migration.v2>\n"
                      "status/explicit-0.1.0.md\n```\n")
            for name in ("release-0.2.1.md", "migration.v2.md", "explicit-0.1.0.md"):
                self.page(docs, "status/" + name, "# Current\n")
            result = check.scan(root, check_diagrams=False, check_configuration=False)
            self.assertEqual(result["errors"], [])
            self.assertEqual(result["pages"], 4)

    def test_missing_dotted_target_never_resolves_a_truncated_decoy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "docs"
            index = self.page(docs, "index.md", "# Start\n\n```{toctree}\nrelease-0.1.0\n```\n")
            self.page(docs, "release-0.1.md", "# Decoy\n")
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
        source = (root / "docs/development/sdk-publishing.md").read_text()
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
        selection = packager.resolve_selection(root, **prepare.call_args.kwargs)
        self.assertTrue(selection.host_support,
                        "The complete plan starts with a Native consumer")
        self.assertEqual(selection.backend, "freertos")
        self.assertEqual({package.id for package in selection.packages}, {"stm32f4"})
        self.assertEqual(set(selection.components), {
            "network", "network-loaned-rx", "tls", "mqtt", "mcuboot",
            "st-usb-device", "st-usb-host", "flashdb", "littlefs", "fatfs",
            "sfud", "rtt", "shell", "cmsis-dsp", "cm-backtrace"})
        selected = set(packager.runtime_dependency_closure(root=root, selection=selection))
        self.assertTrue({"ext/lwip", "ext/mbedtls", "ext/coremqtt",
                         "ext/mcuboot", "ext/freertos", "vendors/st/usb_device",
                         "vendors/st/usb_host", "ext/flashdb", "ext/littlefs",
                         "ext/fatfs", "ext/sfud", "ext/segger-rtt",
                         "ext/letter_shell", "vendors/arm/CMSIS_DSP",
                         "ext/cm_backtrace"}.issubset(selected),
                        "The complete plan includes every selected service supplier")


if __name__ == "__main__":
    unittest.main()
