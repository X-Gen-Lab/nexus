"""Corrupt or unmatched retained bytes must never acquire false symbols."""
import binascii
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import fault_decode
from tools.dev.provenance import provenance


def record():
    data = bytearray(160)
    struct.pack_into("<12I", data, 0, 0x5246584e, 1, 160, 7, 2, 3, 1,
                     3, 0xfffffffd, 0x20001000, 0x20002000, 0)
    data[44:60] = bytes(range(16))
    struct.pack_into("<I", data, 84, 0x08001235)
    struct.pack_into("<II", data, 152, binascii.crc32(data[:152]), 0x4346584e)
    return bytes(data)


def arm_elf(identity, *, loaded=True):
    """A Thumb executable with one defined 16-byte object and no debug data."""
    data = bytearray(0x2f0)
    base = 0x08000000
    names = b"\0.text\0.rodata\0.shstrtab\0.strtab\0.symtab\0"
    strings = b"\0nx_fault_build_id\0"
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                     b"\x7fELF\x01\x01\x01", 2, 40, 1, base | 1,
                     52, 0x200, 0x05000400, 52, 32, 1, 40, 6, 3)
    extent = 0x30 if loaded else 0x20
    struct.pack_into("<8I", data, 52, 1, 0x100, base, base, extent,
                     extent, 5, 4)
    data[0x120:0x130] = identity
    data[0x140:0x140 + len(names)] = names
    data[0x180:0x180 + len(strings)] = strings
    struct.pack_into("<IIIBBH", data, 0x1d0, 1, base + 0x20, 16, 0x11, 0, 2)
    sections = ((b".text", 1, 6, base, 0x100, 4, 0, 0),
                (b".rodata", 1, 2, base + 0x20, 0x120, 16, 0, 0),
                (b".shstrtab", 3, 0, 0, 0x140, len(names), 0, 0),
                (b".strtab", 3, 0, 0, 0x180, len(strings), 0, 0),
                (b".symtab", 2, 0, 0, 0x1c0, 32, 4, 16))
    for index, (name, kind, flags, address, offset, size, link, entry) in enumerate(sections, 1):
        struct.pack_into("<10I", data, 0x200 + index * 40, names.index(name),
                         kind, flags, address, offset, size, link, 0, 4, entry)
    return bytes(data)


class FaultDecodeTests(unittest.TestCase):
    def test_raw_wire_decodes_without_claiming_symbol_or_time(self):
        result = fault_decode.decode(record())
        self.assertEqual(result["sequence"], 7)
        self.assertEqual(result["core"]["pc"], 0x08001235)
        self.assertEqual(result["build_id"], bytes(range(16)).hex())
        self.assertNotIn("utc", result)
        self.assertNotIn("symbols", result)

    def test_truncated_pending_and_bad_crc_records_reject(self):
        for changed in (record()[:-1], record()[:156] + b"NXFW"):
            with self.assertRaises(ValueError):
                fault_decode.decode(changed)
        data = bytearray(record())
        data[60] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            fault_decode.decode(bytes(data))

    def test_unknown_version_is_not_interpreted_as_current(self):
        data = bytearray(record())
        struct.pack_into("<I", data, 4, 2)
        struct.pack_into("<I", data, 152, binascii.crc32(data[:152]))
        with self.assertRaisesRegex(ValueError, "version"):
            fault_decode.decode(bytes(data))

    def test_unidentified_record_rejects_with_valid_crc(self):
        data = bytearray(record())
        data[44:60] = bytes(16)
        struct.pack_into("<I", data, 152, binascii.crc32(data[:152]))
        with self.assertRaisesRegex(ValueError, "build ID"):
            fault_decode.decode(bytes(data))

    def test_exact_loaded_identity_is_required_before_invoking_addr2line(self):
        facts = fault_decode.decode(record())
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "firmware.elf"
            with patch("tools.dev.fault_decode.subprocess.run") as run:
                image.write_bytes(arm_elf(bytes(range(16)), loaded=False))
                with self.assertRaisesRegex(ValueError, "loaded"):
                    fault_decode.symbolize(facts, image, "addr2line")
                image.write_bytes(arm_elf(bytes(reversed(range(16)))))
                with self.assertRaisesRegex(ValueError, "build ID differs"):
                    fault_decode.symbolize(facts, image, "addr2line")
                run.assert_not_called()
                image.write_bytes(arm_elf(bytes(range(16))))
                run.return_value = subprocess.CompletedProcess([], 0, "function\n")
                self.assertEqual(fault_decode.symbolize(facts, image, "addr2line")
                                 ["addr2line"], ["function"])
                self.assertEqual(run.call_args.args[0][-2:], ["0x8001234", "0x0"])

    def test_bound_redaction_allows_only_fixed_numeric_facts_and_build_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "fault.bin"
            image = root / "firmware.elf"
            config = root / "resolved.json"
            receipt = root / "build-provenance.json"
            raw.write_bytes(record())
            image.write_bytes(arm_elf(bytes(range(16))))
            config.write_text('{"board": "fixture"}')
            report = provenance({"kind": "fixture"}, [config], [image],
                                [["cmake", "--build", str(root)]], configuration=config)
            receipt.write_text(json.dumps(report))
            args = [str(raw), "--elf", str(image), "--provenance", str(receipt),
                    "--redact"]
            with patch("tools.dev.fault_decode.subprocess.run") as run:
                stdout, stderr = io.StringIO(), io.StringIO()
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    self.assertEqual(fault_decode.main(args), 0)
                self.assertEqual(stderr.getvalue(), "")
                self.assertEqual(json.loads(stdout.getvalue()), {
                    "version": 1, "sequence": 7, "flags": 1,
                    "build_id": bytes(range(16)).hex()})
                run.assert_not_called()

    def test_bound_decode_rejects_wrong_elf_even_if_build_id_was_reused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "fault.bin"
            image = root / "firmware.elf"
            other = root / "different.elf"
            config = root / "resolved.json"
            receipt = root / "build-provenance.json"
            raw.write_bytes(record())
            image.write_bytes(arm_elf(bytes(range(16))))
            other.write_bytes(image.read_bytes() + b"changed")
            config.write_text('{"board": "fixture"}')
            report = provenance({"kind": "fixture"}, [config], [image],
                                [["cmake", "--build", str(root)]], configuration=config)
            receipt.write_text(json.dumps(report))
            args = [str(raw), "--elf", str(other), "--provenance", str(receipt)]
            with patch("tools.dev.fault_decode.subprocess.run") as run:
                for candidate, error in ((other, "ELF"), (image, "identity changed")):
                    if candidate == image:
                        config.write_text('{"board": "changed"}')
                    args[2] = str(candidate)
                    stdout, stderr = io.StringIO(), io.StringIO()
                    with redirect_stdout(stdout), redirect_stderr(stderr):
                        self.assertEqual(fault_decode.main(args), 1)
                    self.assertEqual(stdout.getvalue(), "")
                    self.assertIn(error, stderr.getvalue())
                run.assert_not_called()
