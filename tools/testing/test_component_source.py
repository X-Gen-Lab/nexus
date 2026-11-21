"""Selected vendor files are admitted by identity, never by directory existence."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.maintenance import component_lock

ROOT = Path(__file__).resolve().parents[2]


class ComponentSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        lock = json.loads((ROOT / "dependencies/littlefs.lock.json").read_text())
        (self.root / "dependencies").mkdir()
        (self.root / "dependencies/littlefs.lock.json").write_text(json.dumps(lock))
        for name in lock["files"]:
            source = ROOT / lock["path"] / name
            target = self.root / lock["path"] / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)

    def test_exact_packaged_bytes_are_accepted(self):
        result = component_lock.verify(self.root, "littlefs", source_sdk=True)
        self.assertEqual(result["path"], str(self.root / "ext/littlefs"))

    def test_changed_or_missing_implementation_is_rejected(self):
        source = self.root / "ext/littlefs/lfs.c"
        source.write_bytes(source.read_bytes() + b"\n/* changed */\n")
        with self.assertRaisesRegex(ValueError, "differ"):
            component_lock.verify(self.root, "littlefs", source_sdk=True)
        source.unlink()
        with self.assertRaises((ValueError, OSError)):
            component_lock.verify(self.root, "littlefs", source_sdk=True)

    def test_packaged_scope_rejects_git_marker_and_escaping_path(self):
        marker = self.root / "ext/littlefs/.git"
        marker.write_text("gitdir: /untrusted\n")
        with self.assertRaisesRegex(ValueError, "Git"):
            component_lock.verify(self.root, "littlefs", source_sdk=True)
        marker.unlink()
        path = self.root / "dependencies/littlefs.lock.json"
        lock = json.loads(path.read_text())
        lock["files"]["../escape.c"] = "0" * 64
        path.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError, "path"):
            component_lock.verify(self.root, "littlefs", source_sdk=True)
