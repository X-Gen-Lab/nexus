"""Corrupt or unmatched retained bytes must never acquire false symbols."""
import binascii
import struct
import unittest

from tools.dev import fault_decode


def record():
    data = bytearray(160)
    struct.pack_into("<12I", data, 0, 0x5246584e, 1, 160, 7, 2, 3, 1,
                     3, 0xfffffffd, 0x20001000, 0x20002000, 0)
    data[44:60] = bytes(range(16))
    struct.pack_into("<I", data, 84, 0x08001235)
    struct.pack_into("<II", data, 152, binascii.crc32(data[:152]), 0x4346584e)
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
