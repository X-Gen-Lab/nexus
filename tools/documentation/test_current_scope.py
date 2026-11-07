"""Historical source records stay intact outside current-site admission."""
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.documentation import check
from tools.configure import documentation as configuration_docs


class CurrentDocumentationScopeTests(unittest.TestCase):
    def test_configuration_examples_follow_current_source_scope(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            record = root / "docs/delivery/verification.md"
            record.parent.mkdir(parents=True)
            record.write_text("# Original record\n\n```toml\nschema = 2\n```\n")
            policy = root / configuration_docs.configure.metadata.POLICY
            policy.parent.mkdir(parents=True)
            shutil.copyfile(configuration_docs.configure.ROOT /
                            configuration_docs.configure.metadata.POLICY,
                            policy)
            self.assertEqual(configuration_docs.check(root), [])

    def test_frozen_delivery_record_is_not_current_source(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            record = root / "docs/delivery/soc-extensions-verification.md"
            record.parent.mkdir(parents=True)
            record.write_text("# Original record\n[old](removed.md)\n")
            self.assertFalse(check.is_source(root, record))

    def test_current_support_still_requires_valid_links(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            docs = root / "docs"
            (docs / "delivery").mkdir(parents=True)
            (docs / "index.md").write_text(
                "# Current\n\n```{toctree}\ndelivery/support\n```\n")
            current = docs / "delivery/support.md"
            current.write_text("# Support\n[missing](removed.md)\n")
            historical = docs / "delivery/verification.md"
            before = b"# Original evidence\n[old](also-removed.md)\n"
            historical.write_bytes(before)
            report = check.scan(root, check_diagrams=False,
                                check_configuration=False)
            self.assertTrue(any("support.md: missing" in item
                                for item in report["errors"]))
            self.assertFalse(any("verification.md:" in item
                                 for item in report["errors"]))
            self.assertEqual(historical.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
