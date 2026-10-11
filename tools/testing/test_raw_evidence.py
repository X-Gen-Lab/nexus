"""Git must preserve execution evidence bytes instead of normalizing CRLF."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RawEvidenceTests(unittest.TestCase):
    def check_bytes(self, name, expected):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / ".gitattributes").write_bytes(
                (ROOT / ".gitattributes").read_bytes())
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"source bytes\r\nretained evidence\r\n")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True,
                           capture_output=True)
            result = subprocess.run(["git", "-C", str(root), "show", ":" + name],
                                    check=True, capture_output=True)
            self.assertEqual(result.stdout, expected)

    def test_platform_raw_evidence_retains_actual_crlf_bytes(self):
        self.check_bytes("tools/testing/evidence/probe/source.c.raw",
                         b"source bytes\r\nretained evidence\r\n")

    def test_vendor_source_snapshot_retains_actual_crlf_bytes(self):
        self.check_bytes("tools/testing/evidence/probe/vendors/startup.S",
                         b"source bytes\r\nretained evidence\r\n")

    def test_kernel_raw_evidence_retains_actual_crlf_bytes(self):
        self.check_bytes("tests/evidence/kernel/run.log",
                         b"source bytes\r\nretained evidence\r\n")

    def test_owned_production_source_still_uses_normalized_lf(self):
        self.check_bytes("io/src/ordinary.c",
                         b"source bytes\nretained evidence\n")


if __name__ == "__main__":
    unittest.main()
