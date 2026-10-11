import json
from pathlib import Path
import tempfile
import unittest
from check_action_pins import check


class ActionLockPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / ".github/workflows").mkdir(parents=True)
        (self.root / "dependencies").mkdir()
        self.sha = "a" * 40
        (self.root / "dependencies/actions.lock.json").write_text(json.dumps({
            "schema_version": 1, "actions": [{"repository": "actions/checkout", "sha": self.sha}]}))
        self.workflow = self.root / ".github/workflows/ci.yml"

    def test_locked_and_local_actions_pass(self):
        self.workflow.write_text("  - uses: actions/checkout@" + self.sha + " # release\n  - uses: ./.github/actions/build\n")
        self.assertEqual(check(self.root), [])

    def test_floating_tag_cannot_pass(self):
        self.workflow.write_text("  - uses: actions/checkout@v7\n")
        self.assertTrue(check(self.root))

    def test_unknown_full_sha_cannot_pass(self):
        self.workflow.write_text("  - uses: actions/checkout@" + "b" * 40 + "\n")
        self.assertTrue(check(self.root))

    def test_unknown_repository_cannot_pass(self):
        self.workflow.write_text("  - uses: unknown/action@" + self.sha + "\n")
        self.assertTrue(check(self.root))

    def test_nested_action_uses_repository_identity(self):
        self.workflow.write_text("  - uses: actions/checkout/nested@" + self.sha + "\n")
        self.assertEqual(check(self.root), [])

    def test_zero_references_cannot_pass(self):
        self.workflow.write_text("  - uses: ./.github/actions/build\n")
        self.assertTrue(check(self.root))

    def test_yaml_extension_cannot_bypass_policy(self):
        self.workflow.write_text("  - uses: actions/checkout@" + self.sha + "\n")
        (self.workflow.parent / "other.yaml").write_text("  - uses: actions/checkout@main\n")
        self.assertTrue(check(self.root))

    def test_duplicate_lock_entry_rejected(self):
        path = self.root / "dependencies/actions.lock.json"
        lock = json.loads(path.read_text())
        lock["actions"] *= 2
        path.write_text(json.dumps(lock))
        with self.assertRaises(ValueError):
            check(self.root)


if __name__ == "__main__":
    unittest.main()
