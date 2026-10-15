"""Reviewed reference indices reject fictitious identity and qualification."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate, fact_references


class FactReferenceContractTests(unittest.TestCase):
    def test_g474_cannot_omit_its_exact_official_reference_index(self):
        soc = gate.load(gate.ROOT / "soc/stm32g474/soc.json")
        soc.pop("references", None)
        with self.assertRaisesRegex(ValueError, "references"):
            gate.validate_soc(soc, "STM32G474RET6")

    def references(self, family="stm32f407"):
        package = gate.ROOT / "soc" / family
        return (json.loads((package / "references.json").read_text()),
                json.loads((package / "soc.json").read_text())["variants"])

    def test_pin_source_keeps_errata_and_hardware_limits_explicit(self):
        for family in ("stm32f407", "gd32f470", "stm32g474"):
            with self.subTest(family=family):
                data, variants = self.references(family)
                before = copy.deepcopy(data)
                self.assertEqual(fact_references.validate(data, variants), data)
                self.assertEqual(data, before)
                self.assertEqual(data["errata_review"]["status"],
                                 "applicability_only")
                self.assertEqual(
                    data["errata_review"]["hardware_qualification"],
                    "not_executed")

    def test_unknown_fields_cannot_invent_workaround_or_document_scope(self):
        for path in ((), ("documents", "datasheet"), ("package_review",),
                     ("errata_review",)):
            with self.subTest(path=path):
                data, variants = self.references()
                node = data
                for name in path:
                    node = node[name]
                node["unreviewed_extension"] = True
                with self.assertRaisesRegex(ValueError, "fields"):
                    fact_references.validate(data, variants)

    def test_reference_identity_requires_https_digest_and_nonzero_bytes(self):
        for field, value in (("url", "http://example.com/doc.pdf"),
                             ("url", "https://secret@example.com/doc.pdf"),
                             ("url", "https:///doc.pdf"),
                             ("sha256", "a" * 63), ("sha256", "G" * 64),
                             ("bytes", 0), ("bytes", True)):
            with self.subTest(field=field, value=value):
                data, variants = self.references()
                data["documents"]["datasheet"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_missing_revision_or_reviewed_section_is_rejected(self):
        for field, value in (("revision", ""), ("document_date", ""),
                             ("anchors", {}),
                             ("anchors", {"package_gpio": ""})):
            with self.subTest(field=field):
                data, variants = self.references()
                data["documents"]["datasheet"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_document_kind_and_review_link_cannot_be_swapped(self):
        for field, value in (("document", "not-present"),
                             ("document", "errata")):
            with self.subTest(value=value):
                data, variants = self.references()
                data["package_review"][field] = value
                with self.assertRaisesRegex(ValueError, "package"):
                    fact_references.validate(data, variants)
        data, variants = self.references()
        data["documents"]["errata"]["kind"] = "datasheet"
        with self.assertRaisesRegex(ValueError, "kind"):
            fact_references.validate(data, variants)

    def test_missing_duplicate_or_foreign_part_cannot_share_review(self):
        for parts in ([], ["STM32F407VET6"], ["OTHER_PACKAGE"],
                      ["STM32F407VET6", "STM32F407VET6"]):
            with self.subTest(parts=parts):
                data, variants = self.references()
                data["package_review"]["variants"] = parts
                with self.assertRaisesRegex(ValueError, "variants"):
                    fact_references.validate(data, variants)

    def test_document_availability_never_establishes_errata_or_hil(self):
        for field, value in (("status", "fully_audited"),
                             ("silicon_revision", "A"),
                             ("mode_workarounds", "all_qualified"),
                             ("hardware_qualification", "passed")):
            with self.subTest(field=field):
                data, variants = self.references()
                data["errata_review"][field] = value
                with self.assertRaisesRegex(ValueError, "qualification|scope"):
                    fact_references.validate(data, variants)



class ReferenceInputClosureTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for relative in ("dependencies/platform-compatibility.json",
                         "soc/gd32f470/soc.json", "soc/gd32f470/routes.json",
                         "soc/gd32f470/references.json",
                         "boards/gd32f470_liangshan/board.json"):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / relative, destination)
        vendor = "vendors/gigadevice/gd32f4xx"
        shutil.copytree(gate.ROOT / vendor, self.root / vendor)
        soc_path = self.root / "soc/gd32f470/soc.json"
        soc = json.loads(soc_path.read_text())
        soc["references"] = "references.json"
        soc_path.write_text(json.dumps(soc))
        self.references = self.root / "soc/gd32f470/references.json"
        self.assembly = self.root / "assembly.toml"
        self.assembly.write_text('schema=2\nboard="gd32f470_liangshan"\n'
                                 'backend="baremetal"\nclock="hxtal25-pll200"\n'
                                 '[memory]\nmain_stack_bytes=2048\n')

    def test_reference_index_is_bound_to_resolved_identity_and_input_closure(self):
        result = gate.resolve_ir(self.assembly, self.root)
        self.assertEqual(result.to_dict()["fact_references"],
                         json.loads(self.references.read_text()))
        inputs = {item["path"]: item["sha256"] for item in result["inputs"]}
        self.assertEqual(inputs["soc/gd32f470/references.json"],
                         hashlib.sha256(self.references.read_bytes()).hexdigest())
        before = result["configuration_sha256"]
        data = json.loads(self.references.read_text())
        data["documents"]["datasheet"]["revision"] += "-fixture"
        self.references.write_text(json.dumps(data))
        self.assertNotEqual(before, gate.resolve_ir(
            self.assembly, self.root)["configuration_sha256"])

    def test_reference_change_during_generation_revokes_old_bundle(self):
        output = self.root / "generated"
        generate = gate.generate
        gate.configure(self.assembly, output, self.root)

        def changed(result, destination):
            generate(result, destination)
            self.references.write_bytes(self.references.read_bytes() + b"\n")

        with patch.object(gate, "generate", side_effect=changed):
            with self.assertRaisesRegex(ValueError, "Input changed during"):
                gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_qualification_claim_in_reference_cannot_publish_a_bundle(self):
        data = json.loads(self.references.read_text())
        data["errata_review"]["hardware_qualification"] = "passed"
        self.references.write_text(json.dumps(data))
        output = self.root / "generated"
        with self.assertRaisesRegex(ValueError, "qualification"):
            gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())



if __name__ == "__main__":
    unittest.main()
