"""One admitted immutable fact package feeds the typed configuration stages.

Exact family/part and the selected variant share one fact owner. Consumed input
paths remain with that selection, and Board physical unknowns stay explicit.
"""

from dataclasses import fields, FrozenInstanceError
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.configure import configure


class FactStageTests(unittest.TestCase):
    def acquire(self, relative="tools/configure/assemblies/qiming-baremetal.toml"):
        path = configure.ROOT / relative
        assembly = configure.load_assembly(path)
        reads = {}

        def read(source):
            reads[source] = source.read_bytes()
            return configure.load(source)

        selected = configure._resolve_facts(
            assembly, root=configure.ROOT,
            assembly_directory=path.parent, read=read)
        return selected, reads

    def test_identity_has_one_fact_owner_and_selected_variant_view(self):
        selected, _ = self.acquire()
        self.assertEqual(selected.part, "STM32F407ZGT6")
        self.assertEqual(selected.family, "stm32f407")
        self.assertIs(selected.variant, selected.soc["variants"][selected.part])
        stored = {field.name for field in fields(selected)}
        self.assertFalse({"part", "family", "variant"} & stored)

    def test_boundary_freezes_nested_facts_not_only_the_outer_container(self):
        selected, _ = self.acquire()
        with self.assertRaises(FrozenInstanceError):
            selected.inputs = ()
        with self.assertRaises(TypeError):
            selected.soc["cpu"]["arch"] = "native"
        with self.assertRaises(TypeError):
            selected.board["soc"] = "other-part"

    def test_consumed_fact_bytes_cannot_be_missing_from_the_stage_inputs(self):
        selected, reads = self.acquire()
        self.assertTrue(reads)
        self.assertTrue(set(reads).issubset(selected.inputs))
        self.assertEqual(len(selected.inputs), len(set(selected.inputs)))
        self.assertTrue(all(isinstance(path, Path) for path in selected.inputs))

    def test_whole_resolver_uses_one_acquisition_and_keeps_physical_unknowns(self):
        stage = configure._resolve_facts
        with patch.object(configure, "_resolve_facts", wraps=stage) as observed:
            result = configure.resolve_ir(configure.ROOT /
                "tools/configure/assemblies/qiming-baremetal.toml")
        self.assertEqual(observed.call_count, 1)
        self.assertIsNone(result.physical_pcb_revision)
        self.assertFalse(result.physical_qualified)


if __name__ == "__main__":
    unittest.main()
