"""Contract tests for safe, deterministic diagram generation and verification."""

import csv
import os
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

from tools.documentation import diagrams


SVG = ('<svg xmlns="http://www.w3.org/2000/svg" '
       'xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 20 20">'
       '<defs><path id="glyph-1" d="M 0 0 L 10 10"/></defs>'
       '<use xlink:href="#glyph-1"/></svg>')


class DiagramContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.folder = self.root / "docs/assets/diagrams"
        self.folder.mkdir(parents=True)
        self.source = self.folder / "sample.dot"
        self.source.write_text('digraph Sample { graph [label="当前图"]; '
                               'a [label="独立状态"]; '
                               'a -> b [label="直接调用"]; }\n')
        self.tool = self.root / "dot"
        self.environment = dict(os.environ)
        self.environment["dot"] = str(self.tool)
        self.write_tool(SVG)
        self.registry()

    def write_tool(self, output, status=0):
        self.tool.write_text('#!' + sys.executable + '\nimport sys\n'
                             'source = sys.stdin.read()\n'
                             'assert sys.argv[1:] == ["-Tsvg:cairo"]\n'
                             'assert "digraph" in source\n'
                             'sys.stdout.write(' + repr(output) + ')\n'
                             'sys.exit(' + str(status) + ')\n')
        self.tool.chmod(0o755)

    def registry(self, source="sample.dot", output="sample.svg"):
        with (self.folder / "diagram-index.csv").open("w", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(("diagram_id", "source_dot", "rendered_svg"))
            writer.writerow(("FIG-01", source, output))

    def render(self):
        return diagrams.render(self.source, self.environment)

    def test_render_adds_accessible_identity_and_all_labels(self):
        tree = ET.fromstring(self.render())
        values = {node.tag.split("}")[-1]: node.text for node in tree
                  if node.tag.split("}")[-1] in {"title", "desc"}}
        self.assertEqual(values["title"], "当前图")
        self.assertIn("独立状态", values["desc"])
        self.assertIn("直接调用", values["desc"])
        self.assertEqual(tree.get("role"), "img")
        self.assertIn("sample-title", tree.get("aria-labelledby"))

    def test_render_is_deterministic_and_keeps_local_glyph_references(self):
        first = self.render()
        self.assertEqual(first, self.render())
        self.assertIn(b'href="#glyph-1"', first)
        self.assertNotIn(b"DOCTYPE", first)

    def test_nonzero_renderer_fails_instead_of_publishing_svg(self):
        self.write_tool(SVG, status=3)
        with self.assertRaisesRegex(ValueError, "renderer"):
            self.render()
        self.assertFalse(self.source.with_suffix(".svg").exists())

    def test_malformed_renderer_svg_is_rejected(self):
        self.write_tool("<svg>")
        with self.assertRaisesRegex(ValueError, "SVG"):
            self.render()

    def test_unsafe_renderer_output_is_rejected(self):
        for content in ('<script/>', '<foreignObject/>',
                        '<image href="https://example.com/x"/>',
                        '<g style="fill:url(https://example.com/x)"/>',
                        '<g onload="alert(1)"/>'):
            self.write_tool('<svg xmlns="http://www.w3.org/2000/svg" '
                            'viewBox="0 0 20 20">' + content + '</svg>')
            with self.assertRaisesRegex(ValueError, "SVG"):
                self.render()

    def test_doctype_and_entities_are_rejected_before_xml_parsing(self):
        self.write_tool('<!DOCTYPE svg [<!ENTITY x "bad">]>' + SVG)
        with self.assertRaisesRegex(ValueError, "SVG"):
            self.render()

    def test_source_image_and_url_attributes_are_rejected_before_renderer(self):
        self.tool.unlink()
        for source in ('digraph G {a [image="/tmp/secret.png"]}',
                       'digraph G {a [label=<<IMG SRC="/tmp/x"/> >]}',
                       'digraph G {a [URL="https://example.com"]}'):
            self.source.write_text(source)
            with self.assertRaisesRegex(ValueError, "DOT"):
                self.render()

    def test_svg_external_processing_instruction_is_rejected(self):
        raw = ('<?xml-stylesheet href="https://example.com/style.css"?>'
               + SVG).encode()
        with self.assertRaisesRegex(ValueError, "SVG"):
            diagrams.validate_svg(raw)

    def test_svg_missing_internal_css_reference_is_rejected(self):
        raw = ('<svg xmlns="http://www.w3.org/2000/svg" '
               'viewBox="0 0 20 20"><path style="clip-path:url(#missing)"/>'
               '</svg>').encode()
        with self.assertRaisesRegex(ValueError, "SVG"):
            diagrams.validate_svg(raw)

    def test_svg_duplicate_identifiers_are_rejected(self):
        raw = ('<svg xmlns="http://www.w3.org/2000/svg" '
               'viewBox="0 0 20 20"><path id="same"/><path id="same"/>'
               '</svg>').encode()
        with self.assertRaisesRegex(ValueError, "SVG"):
            diagrams.validate_svg(raw)

    def test_production_check_rejects_uppercase_css_import(self):
        from tools.documentation import check
        output = self.source.with_suffix(".svg")
        output.write_text('<svg xmlns="http://www.w3.org/2000/svg" '
                          'viewBox="0 0 20 20"><style>@IMPORT '
                          '"https://example.invalid/font.css";</style></svg>')
        with self.assertRaisesRegex(ValueError, "SVG|CSS|resource"):
            check.validate_svg(output)

    def test_production_check_rejects_escaped_css_identifier(self):
        from tools.documentation import check
        output = self.source.with_suffix(".svg")
        output.write_text('<svg xmlns="http://www.w3.org/2000/svg" '
                          'viewBox="0 0 20 20"><style>path{fill: '
                          r'\75rl(https://example.invalid/a.svg)' ';}</style></svg>')
        with self.assertRaisesRegex(ValueError, "SVG|CSS|resource"):
            check.validate_svg(output)

    def test_svg_escaped_external_css_attribute_is_rejected(self):
        raw = ('<svg xmlns="http://www.w3.org/2000/svg" '
               'viewBox="0 0 20 20"><path style="fill: '
               r'\75rl(https://example.invalid/a.svg)' '"/></svg>').encode()
        with self.assertRaisesRegex(ValueError, "SVG|CSS|resource"):
            diagrams.validate_svg(raw)

    def test_svg_escaped_local_css_fragment_is_preserved(self):
        raw = ('<svg xmlns="http://www.w3.org/2000/svg" '
               'viewBox="0 0 20 20"><defs><path id="glyph"/></defs>'
               '<path style="clip-path: '
               r'\75rl(#glyph)' '"/></svg>').encode()
        diagrams.validate_svg(raw)

    def test_svg_nonfinite_or_empty_viewbox_is_rejected(self):
        for viewport in ("0 0 inf 20", "0 0 nan 20", "0 0 0 20"):
            raw = ('<svg xmlns="http://www.w3.org/2000/svg" '
                   'viewBox="' + viewport + '"/>').encode()
            with self.assertRaisesRegex(ValueError, "SVG"):
                diagrams.validate_svg(raw)

    def test_symlink_source_is_rejected(self):
        actual = self.root / "actual.dot"
        self.source.rename(actual)
        self.source.symlink_to(actual)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.render()

    def test_full_check_verifies_bytes_without_mutating_sources_or_outputs(self):
        output = self.source.with_suffix(".svg")
        output.write_bytes(self.render())
        before = {path: (path.read_bytes(), path.stat().st_mtime_ns)
                  for path in self.folder.iterdir()}
        report = diagrams.check(self.root, env=self.environment, full=True)
        self.assertEqual(report["errors"], [])
        self.assertEqual(report["diagrams"], 1)
        after = {path: (path.read_bytes(), path.stat().st_mtime_ns)
                 for path in self.folder.iterdir()}
        self.assertEqual(before, after)

    def test_stale_svg_fails_full_check_and_is_never_rewritten(self):
        output = self.source.with_suffix(".svg")
        output.write_bytes(self.render())
        original = output.read_bytes()
        self.source.write_text('digraph G { graph [label="改变图"] }')
        report = diagrams.check(self.root, env=self.environment, full=True)
        self.assertIn("stale SVG", " ".join(report["errors"]))
        self.assertEqual(output.read_bytes(), original)

    def test_fast_check_uses_no_renderer(self):
        self.source.with_suffix(".svg").write_bytes(self.render())
        self.tool.unlink()
        self.assertEqual(diagrams.check(self.root, full=False)["errors"], [])

    def test_registry_escape_and_missing_pair_fail(self):
        self.registry("../outside.dot", "sample.svg")
        report = diagrams.check(self.root)
        self.assertIn("registry", " ".join(report["errors"]))
        self.registry()
        self.assertIn("missing SVG", " ".join(diagrams.check(self.root)["errors"]))

    def test_unregistered_supplemental_source_is_still_verified(self):
        self.source.with_suffix(".svg").write_bytes(self.render())
        (self.folder / "extra.dot").write_text('digraph Extra {}')
        report = diagrams.check(self.root)
        self.assertIn("missing SVG", " ".join(report["errors"]))


if __name__ == "__main__":
    unittest.main()
