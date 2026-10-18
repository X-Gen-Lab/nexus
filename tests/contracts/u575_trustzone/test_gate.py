"""Mutate actual paired ARM ELF bytes; never substitute a host mock image."""

import json
import os
from pathlib import Path
import shlex
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from scripts.ci.u575_trustzone import compile_profile, validate_pair
from tools.evidence.common import EvidenceError
from tools.measurement.elf import Elf32


class TrustZonePairGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["NEXUS_U575_PAIR_BUILD"])
        cls.secure = (cls.directory / "secure/u575_secure.elf").read_bytes()
        cls.nonsecure = (cls.directory / "nonsecure/u575_nonsecure.elf").read_bytes()
        cls.gateway = Elf32(cls.secure, float_abi="soft").symbol(
            "nx_reference_gpio") & ~1
        cls.gateways = {name: Elf32(cls.secure, float_abi="soft").symbol(name) & ~1
                        for name in ("nx_reference_gpio", "nx_reference_gpio_batch")}

    def validate(self, secure=None, nonsecure=None, imported=None):
        return validate_pair(
            self.secure if secure is None else secure,
            self.nonsecure if nonsecure is None else nonsecure,
            self.gateways if imported is None else imported)

    def ns_word(self, index, value):
        elf = Elf32(self.nonsecure, float_abi="soft")
        vectors = next(s for s in elf.sections if s["name"] == ".isr_vector")
        data = bytearray(self.nonsecure)
        struct.pack_into("<I", data, vectors["offset"] + index * 4, value)
        return bytes(data)

    def test_actual_secure_and_nonsecure_images_agree(self):
        self.assertEqual(self.validate()["gateway_address"], self.gateway)

    def test_ns_stack_outside_partition_rejected(self):
        with self.assertRaises(EvidenceError):
            self.validate(nonsecure=self.ns_word(0, 0x20044008))

    def test_ns_reset_cannot_enter_secure_image(self):
        with self.assertRaises(EvidenceError):
            self.validate(nonsecure=self.ns_word(1, 0x0c000241))

    def test_ns_reset_cannot_enter_reserved_vector_page(self):
        # Keep all three Reset identities consistent, so the range contract
        # itself must reject code inside the first reserved KiB of NS Flash.
        value = 0x08100259
        data = bytearray(self.ns_word(1, value))
        struct.pack_into("<I", data, 24, value)
        elf = Elf32(self.nonsecure, float_abi="soft")
        symbols = next(s for s in elf.sections if s["name"] == ".symtab")
        names = next(s for s in elf.sections if s["name"] == ".strtab")
        strings = self.nonsecure[names["offset"]:
                                 names["offset"] + names["size"]]
        changed = 0
        for offset in range(symbols["offset"],
                            symbols["offset"] + symbols["size"], 16):
            name = struct.unpack_from("<I", data, offset)[0]
            if elf.string(strings, name) == "NonSecure_Reset_Handler":
                struct.pack_into("<I", data, offset + 4, value)
                changed += 1
        self.assertEqual(changed, 1)
        with self.assertRaisesRegex(EvidenceError, "reserved vector"):
            self.validate(nonsecure=bytes(data))

    def test_ns_privileged_exception_vector_cannot_be_smuggled(self):
        reset = Elf32(self.nonsecure, float_abi="soft").entry
        for index in (2, 3, 11, 15, 141):
            with self.subTest(index=index), self.assertRaises(EvidenceError):
                self.validate(nonsecure=self.ns_word(index, reset))

    def test_import_library_must_name_this_exact_gateway(self):
        with self.assertRaises(EvidenceError):
            self.validate(imported=self.gateway + 8)

    def test_both_exact_imports_are_required(self):
        with self.assertRaises(EvidenceError):
            self.validate(imported={"nx_reference_gpio": self.gateway})
        values = dict(self.gateways)
        values["nx_reference_gpio_batch"] += 8
        with self.assertRaises(EvidenceError):
            self.validate(imported=values)

    def test_nsc_gateway_must_start_with_actual_sg_encoding(self):
        elf = Elf32(self.secure, float_abi="soft")
        section = next(s for s in elf.sections if s["name"] == ".gnu.sgstubs")
        data = bytearray(self.secure)
        offset = section["offset"] + self.gateway - section["address"]
        data[offset:offset + 4] = b"\x00\xbf\x00\xbf"
        with self.assertRaises(EvidenceError):
            self.validate(secure=bytes(data))

    def test_async_batch_gateway_requires_its_actual_sg_encoding(self):
        elf = Elf32(self.secure, float_abi="soft")
        gateway = elf.symbol("nx_reference_gpio_batch") & ~1
        section = next(s for s in elf.sections if s["name"] == ".gnu.sgstubs")
        data = bytearray(self.secure)
        offset = section["offset"] + gateway - section["address"]
        data[offset:offset + 4] = b"\x00\xbf\x00\xbf"
        with self.assertRaises(EvidenceError):
            self.validate(secure=bytes(data))

    def test_secure_gateway_load_address_cannot_leave_its_entry(self):
        data = bytearray(self.secure)
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        changed = 0
        for index in range(header[10]):
            offset = header[5] + index * 32
            kind, _, virtual = struct.unpack_from("<III", data, offset)
            if kind == 1 and virtual == 0x0c0fe000:
                struct.pack_into("<I", data, offset + 12, virtual - 32)
                changed += 1
        self.assertEqual(changed, 1)
        with self.assertRaisesRegex(EvidenceError, "Flash load address"):
            self.validate(secure=bytes(data))

    def test_gateway_section_bytes_must_belong_to_the_actual_load(self):
        elf = Elf32(self.secure, float_abi="soft")
        index = next(i for i, item in enumerate(elf.sections)
                     if item["name"] == ".gnu.sgstubs")
        section = elf.sections[index]
        data = bytearray(self.secure)
        copied = len(data)
        data.extend(data[section["offset"]:
                         section["offset"] + section["size"]])
        table = struct.unpack_from("<I", data, 32)[0]
        struct.pack_into("<I", data, table + index * 40 + 16, copied)
        with self.assertRaisesRegex(EvidenceError, "allocated section"):
            self.validate(secure=bytes(data))

    def test_swapping_security_domains_rejected(self):
        with self.assertRaises(EvidenceError):
            self.validate(secure=self.nonsecure, nonsecure=self.secure)

    def test_reported_stack_reservations_must_match_actual_sections(self):
        for domain, original in (("secure", self.secure),
                                 ("nonsecure", self.nonsecure)):
            elf = Elf32(original, float_abi="soft")
            index = next(i for i, item in enumerate(elf.sections)
                         if item["name"] == ".stack")
            data = bytearray(original)
            header = struct.unpack_from("<I", data, 32)[0] + index * 40
            struct.pack_into("<I", data, header + 20, 1024)
            with self.subTest(domain=domain), self.assertRaisesRegex(
                    EvidenceError, "stack reservation"):
                self.validate(**{domain: bytes(data)})

    def test_hard_float_image_cannot_expand_no_fp_contract(self):
        data = bytearray(self.nonsecure)
        flags = struct.unpack_from("<I", data, 36)[0]
        struct.pack_into("<I", data, 36, (flags & ~0x600) | 0x400)
        with self.assertRaises(EvidenceError):
            self.validate(nonsecure=bytes(data))

    def compile_commands(self, stage):
        return json.loads((self.directory / stage /
                           "compile_commands.json").read_text())

    def test_actual_compile_graph_has_explicit_domain_and_soft_abi(self):
        for stage in ("secure", "nonsecure"):
            rows = self.compile_commands(stage)
            argv = rows[0].get("arguments") or shlex.split(rows[0]["command"])
            with self.subTest(stage=stage):
                compile_profile(self.directory / stage, stage, argv[0])

    def modified_compile(self, stage, transform):
        rows = self.compile_commands(stage)
        argv = rows[0].get("arguments") or shlex.split(rows[0]["command"])
        compiler = argv[0]
        rows[0].pop("command", None)
        rows[0]["arguments"] = transform(argv)
        with tempfile.TemporaryDirectory() as name:
            build = Path(name)
            (build / "compile_commands.json").write_text(json.dumps(rows))
            compile_profile(build, stage, compiler)

    def test_domain_flag_cannot_disappear_or_cross_to_ns(self):
        with self.assertRaises(EvidenceError):
            self.modified_compile("secure", lambda argv: [
                item for item in argv if item != "-mcmse"])
        with self.assertRaises(EvidenceError):
            self.modified_compile("nonsecure", lambda argv: argv + ["-mcmse"])

    def test_effective_abi_override_is_rejected_in_actual_graph(self):
        with self.assertRaises(EvidenceError):
            self.modified_compile("secure", lambda argv:
                                  argv + ["-mfloat-abi=hard"])


    def test_portable_profile_checks_actual_original_database(self):
        from tools.evidence.u575_pair import compile_outputs
        report = json.loads((self.directory / "report.json").read_text())
        compiler = report["tools"]["gcc"]["path"]
        for stage in ("secure", "nonsecure"):
            database = json.loads((self.directory / stage /
                                   "compile_commands.json").read_text())
            outputs = compile_outputs(database, stage, compiler)
            self.assertEqual(len(outputs), len(database))
            self.assertEqual(len(outputs), len(set(outputs)))

    def test_nonsecure_map_binds_original_secure_import_object(self):
        from tools.evidence.u575_pair import map_inputs
        report = json.loads((self.directory / "report.json").read_text())
        stage = self.directory / "nonsecure"
        paths = map_inputs((stage / "u575_nonsecure.map").read_bytes(),
                           str(stage))
        self.assertIn(report["artifacts"]["secure_import"]["path"], paths)


if __name__ == "__main__":
    unittest.main()
