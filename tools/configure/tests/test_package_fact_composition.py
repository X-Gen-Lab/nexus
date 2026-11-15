"""One manifest selects facts; queries and builds share selected system roles."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, experience, packages
from tools.configure.tests.f4_fixture import copy_package


class PackageFactCompositionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.owner = copy_package(self.root)
        self.manifest_path = self.owner / "package.json"
        self.manifest = json.loads(self.manifest_path.read_text())
        for relative in ("dependencies/platform-compatibility.json",
                         "boards/stm32f407ve_sky_qingchun/board.json"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(configure.ROOT / relative, target)
        self.assembly = self.root / "assembly.toml"
        self.assembly.write_text('schema=2\nboard="stm32f407ve_sky_qingchun"\n'
            'backend="baremetal"\nclock="hse8-pll168"\n'
            '[memory]\nmain_stack_bytes=2048\n')

    def fact(self, name):
        return self.owner / self.manifest["facts"][name]

    def update_fact(self, name, change):
        path = self.fact(name)
        value = json.loads(path.read_text())
        change(value)
        path.write_text(json.dumps(value) + "\n")

    def resolve(self):
        with patch.object(configure, "dependency_identity",
                side_effect=lambda _root, name, expected=None:
                {"path": name, "commit": expected}):
            return configure.resolve_ir(self.assembly, self.root)

    def test_manifest_alone_selects_relocated_reviewed_references(self):
        self.update_fact("soc", lambda value: value.pop("references", None))
        original = self.fact("references")
        relocated = self.owner / "reviewed" / "documents.json"
        relocated.parent.mkdir()
        original.rename(relocated)
        self.manifest["facts"]["references"] = "reviewed/documents.json"
        self.manifest_path.write_text(json.dumps(self.manifest) + "\n")
        result = self.resolve()
        self.assertIsNotNone(result.fact_references)
        self.assertEqual(result.fact_references["documents"]["rm0090"]["revision"], "22")
        inputs = {item["path"] for item in result.inputs}
        self.assertIn(relocated.relative_to(self.root).as_posix(), inputs)

    def test_manifest_reference_bytes_cannot_bypass_independent_review(self):
        self.update_fact("soc", lambda value: value.pop("references", None))
        self.update_fact("references", lambda value:
            value["documents"]["rm0090"].update(revision="unreviewed-revision"))
        with self.assertRaisesRegex(ValueError, "revision|review"):
            self.resolve()

    def test_query_and_build_reserve_the_same_selected_timebase(self):
        self.update_fact("soc", lambda value: value.pop("reserved_resources", None))
        self.update_fact("bindings", lambda value: value["system"]["timebase"].update(
            timer="TIM5", irq="TIM5_IRQn", gate="RCC_APB1ENR_TIM5EN",
            reset="RCC_APB1RSTR_TIM5RST", handler="TIM5_IRQHandler"))
        result = self.resolve()
        with patch.object(configure.sdk, "verify"):
            query = experience.board_catalogue(configure, self.root,
                board="stm32f407ve_sky_qingchun")
        def selected(claims):
            return {item["key"] for item in claims if item["owner"] == "soc-timebase"}
        expected = {"controller:TIM5", "irq:TIM5"}
        self.assertEqual({item.key for item in result.claims
                          if item.owner == "soc-timebase"}, expected)
        self.assertEqual(selected(query["claims"]), expected)

    def test_split_soc_facts_compose_once_with_explicit_ownership(self):
        soc_path = self.fact("soc")
        expected = packages.read_soc(packages.load(self.manifest_path))
        soc = dict(expected)
        for slot, fields in (("controllers", ("controller_groups", "can_domain")),
                             ("clocks", ("clock_profiles",))):
            value = {"schema_version": 1}
            for key in fields:
                if key in soc:
                    value[key] = soc.pop(key)
            path = self.owner / (slot + ".json")
            path.write_text(json.dumps(value) + "\n")
            self.manifest["facts"][slot] = path.name
        soc_path.write_text(json.dumps(soc) + "\n")
        self.manifest_path.write_text(json.dumps(self.manifest) + "\n")
        reader = configure._SnapshotReader({})
        package = packages.load(self.manifest_path, read=reader)
        self.assertEqual(packages.read_soc(package, read=reader), expected)
        for slot in ("soc", "controllers", "clocks"):
            self.assertIn(package.fact(slot), reader.snapshots)

    def test_split_fields_cannot_have_a_second_or_unknown_author(self):
        package = packages.load(self.manifest_path)
        original = self.fact("soc").read_text()
        for key, value in (("clock_profiles", {"copied": {}}),
                           ("references", "an-unselected-path.json")):
            data = json.loads(original)
            data[key] = value
            self.fact("soc").write_text(json.dumps(data) + "\n")
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "owner|references"):
                packages.read_soc(package)
        self.fact("soc").write_text(original)
        self.update_fact("clocks", lambda value: value.update(register_write="forbidden"))
        with self.assertRaisesRegex(ValueError, "unknown fields"):
            packages.read_soc(package)

    def test_split_facts_cannot_acquire_a_new_identity_after_the_first_read(self):
        reader = packages.FactReader()
        package = packages.load(self.manifest_path, read=reader)
        packages.read_soc(package, read=reader)
        self.update_fact("clocks", lambda value: value["clock_profiles"].update(
            unreviewed={"core_hz": 1}))
        with self.assertRaisesRegex(ValueError, "changed after its first read"):
            packages.read_soc(package, read=reader)

    def test_production_review_cannot_disappear_or_become_source_only(self):
        original = self.manifest_path.read_text()
        self.manifest["facts"].pop("references")
        self.manifest_path.write_text(json.dumps(self.manifest) + "\n")
        with self.assertRaisesRegex(ValueError, "references|review"):
            self.resolve()
        self.manifest_path.write_text(original)
        self.manifest = json.loads(original)
        references = self.fact("references").read_text()
        self.update_fact("references", lambda value: value.update(schema_version=1))
        with self.assertRaisesRegex(ValueError, "[Ss]chema|[Rr]eference"):
            self.resolve()
        self.fact("references").write_text(references)
        self.manifest["facts"].pop("review_profiles")
        self.manifest_path.write_text(json.dumps(self.manifest) + "\n")
        with self.assertRaisesRegex(ValueError, "profile"):
            self.resolve()

    def test_exact_part_capture_limit_is_an_explicit_physical_fact(self):
        result = self.resolve()
        self.assertEqual(result.physical_part.capabilities["dcmi_bits"], 12)
        board = self.root / "boards/stm32f407ve_sky_qingchun/board.json"
        value = json.loads(board.read_text())
        value["soc"] = "STM32F407ZGT6"
        board.write_text(json.dumps(value) + "\n")
        result = self.resolve()
        self.assertEqual(result.physical_part.capabilities["dcmi_bits"], 14)

    def test_production_package_cannot_delete_both_review_inputs(self):
        self.manifest["facts"].pop("references")
        self.manifest["facts"].pop("review_profiles")
        self.manifest_path.write_text(json.dumps(self.manifest) + "\n")
        with self.assertRaisesRegex(ValueError, "[Pp]roduction.*review"):
            self.resolve()

    def test_reference_source_only_package_retains_explicit_test_scope(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/declarative_gd32f470/gpio-only.toml", test_support=True)
        self.assertEqual(result.provider_context.package.scope, "reference")
        self.assertIsNone(result.fact_references)
        self.assertFalse(result.physical_qualified)


if __name__ == "__main__":
    unittest.main()
