"""Validate the shared component catalogue without constructing a source graph."""

import json
from pathlib import Path
import unittest

from tools.configure import component_catalog


ROOT = Path(__file__).resolve().parents[3]


class ComponentCatalogTests(unittest.TestCase):
    def test_catalogue_owns_exact_names_and_leaf_entries(self):
        entries = component_catalog.load()
        self.assertEqual(entries["log"].target, "Nexus::Log")
        self.assertEqual(entries["log"].entry, "")
        self.assertEqual(entries["tls"].entry,
                         "components/protocols/tls/Select.cmake")
        for component in entries.values():
            if component.entry:
                self.assertTrue((ROOT / component.entry).is_file(),
                                component.entry)
        with self.assertRaises(TypeError):
            entries["other"] = entries["log"]

    def test_duplicate_ids_unknown_fields_and_invalid_entries_reject(self):
        row = {"id": "stack", "entry": "", "target": "Nexus::Stack"}
        variants = (
            [row, row],
            [dict(row, sources=["stack.c"])],
            [dict(row, entry="../outside.cmake")],
            [dict(row, entry="components/$unsafe.cmake")],
            [dict(row, target="arbitrary command")],
        )
        for entries in variants:
            with self.subTest(entries=entries):
                with self.assertRaises(ValueError):
                    component_catalog.parse({"schema_version": 1,
                                             "components": entries})

    def test_catalogue_has_no_source_or_dependency_graph(self):
        doc = json.loads((ROOT / "components/catalog.json").read_text())
        for item in doc["components"]:
            self.assertEqual(set(item), {"id", "entry", "target"})


if __name__ == "__main__":
    unittest.main()
