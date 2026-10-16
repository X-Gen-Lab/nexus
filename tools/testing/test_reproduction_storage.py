"""Real process locking keeps VFS image copies within the serial budget."""

from pathlib import Path
import tempfile
import unittest

from tools.evidence.common import EvidenceError
from tools.evidence import reproduce
from tools.evidence import storage


class ReproductionStorageTests(unittest.TestCase):
    def test_overlay_cannot_admit_an_unbounded_zero_budget(self):
        with self.assertRaises(EvidenceError):
            storage.plan(driver="overlay2", image_size_bytes=1,
                image_root_bytes=None, free_bytes=1024, reserve_bytes=0,
                metadata_floor_bytes=0, build_bytes=0, read_only=True,
                tmpfs_bytes=1024, daemon_log_driver="none")

    def test_concurrent_reproduction_is_rejected_and_release_allows_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            lease = Path(directory) / "daemon-storage.lock"
            with reproduce.storage_lease(lease):
                with self.assertRaisesRegex(EvidenceError, "already active"):
                    with reproduce.storage_lease(lease):
                        self.fail("second VFS workload was admitted")
            with reproduce.storage_lease(lease):
                self.assertTrue(lease.is_file())

    def test_cancellation_releases_lock_without_replacing_its_inode(self):
        with tempfile.TemporaryDirectory() as directory:
            lease = Path(directory) / "daemon-storage.lock"
            with self.assertRaises(KeyboardInterrupt):
                with reproduce.storage_lease(lease):
                    identity = lease.stat().st_ino
                    raise KeyboardInterrupt
            with reproduce.storage_lease(lease):
                self.assertEqual(lease.stat().st_ino, identity)

    def test_symlink_cannot_redirect_the_shared_storage_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "untouched"
            target.write_text("must not be truncated")
            lease = root / "daemon-storage.lock"
            lease.symlink_to(target)
            with self.assertRaises(EvidenceError):
                with reproduce.storage_lease(lease):
                    self.fail("symlink lock was accepted")
            self.assertEqual(target.read_text(), "must not be truncated")


if __name__ == "__main__":
    unittest.main()
