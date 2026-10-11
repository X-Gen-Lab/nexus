"""Authored Markdown examples use the actual platform and runtime resolvers."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, documentation


class DocumentationConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.document = Path(self.directory.name) / "example.md"

    def check(self, contents):
        self.document.write_text(contents)
        return documentation.check_document(self.document, gate.ROOT)

    def test_complete_assembly_executes_real_resource_checks(self):
        assembly = (gate.ROOT /
                    "tools/configure/assemblies/native-gpio.toml").read_text()
        rows = self.check("```toml\n" + assembly + "```\n")
        self.assertEqual(rows[0]["kind"], "assembly")
        self.assertEqual(len(rows[0]["configuration_sha256"]), 64)
        stale = assembly.replace('mode = "output"', 'mode = "unmaintained"')
        with self.assertRaisesRegex(ValueError, "Unsupported mode"):
            self.check("```toml\n" + stale + "```\n")

    def test_partial_examples_require_explicit_reason(self):
        fragment = '[gpio.led0]\nbinding="led0"\nmode="output"\n'
        with self.assertRaisesRegex(ValueError, "fragment"):
            self.check("```toml\n" + fragment + "```\n")
        rows = self.check("<!-- nexus-toml-fragment: binding comes from product -->\n"
                          "```toml\n" + fragment + "```\n")
        self.assertEqual(rows[0]["kind"], "fragment")
        self.assertIn("product", rows[0]["reason"])
        with self.assertRaisesRegex(ValueError, "reason"):
            self.check("<!-- nexus-toml-fragment: -->\n"
                       "```toml\n" + fragment + "```\n")

    def test_fragments_still_require_valid_toml(self):
        with self.assertRaises(ValueError):
            self.check("<!-- nexus-toml-fragment: incomplete composition -->\n"
                       "```toml\n[broken\n```\n")

    def test_runtime_uses_adjacent_inline_facts_and_actual_resolver(self):
        facts = json.loads((gate.ROOT / "soc/stm32f407/soc.json").read_text())
        package = {"schema_version": 1, "cpu": facts["cpu"],
                   "irq": facts["irq"], "clock_hz": 168000000}
        contents = ('```toml\nschema_version=1\ncpu_facts="cpu.json"\n'
                    'backend="freertos"\noptimization="Os"\n```\n'
                    '```json\n' + json.dumps(package) + '\n```\n')
        rows = self.check(contents)
        self.assertEqual(rows[0]["kind"], "runtime")
        with self.assertRaisesRegex(ValueError, "backend"):
            self.check(contents.replace('backend="freertos"',
                                        'backend="not-a-kernel"'))

    def test_runtime_facts_escape_is_rejected_before_writing(self):
        with self.assertRaisesRegex(ValueError, "relative|escape"):
            self.check('```toml\nschema_version=1\ncpu_facts="../cpu.json"\n'
                       'backend="baremetal"\noptimization="Os"\n```\n')

    def test_unterminated_toml_fence_cannot_disappear_from_the_gate(self):
        with self.assertRaisesRegex(ValueError, "unterminated"):
            self.check('```toml\nschema=2\nboard="native_reference"\n')

    def test_runtime_facts_parent_symlink_is_rejected(self):
        actual = self.document.parent / "actual"
        actual.mkdir()
        (actual / "cpu.json").write_text("{}")
        link = self.document.parent / "linked"
        link.symlink_to(actual, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.check('```toml\nschema_version=1\ncpu_facts="linked/cpu.json"\n'
                       'backend="baremetal"\noptimization="Os"\n```\n')


if __name__ == "__main__":
    unittest.main()
