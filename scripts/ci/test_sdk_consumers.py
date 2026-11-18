"""Test SDK change admission and the committed, relocated consumer handoff."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.ci import sdk_consumers


class SDKChangeTests(unittest.TestCase):
    def test_private_edit_skips_but_shared_lock_and_unknown_expand(self):
        private = "soc/other/other_family/drivers/uart/uart.c"
        device = "components/devices/tmp117/src/tmp117.c"
        self.assertEqual(sdk_consumers.sdk_changes([private]), [])
        self.assertEqual(sdk_consumers.sdk_changes([private, device]), [device])
        for name in ("dependencies/toolchains.lock.json", "future/new.build",
                     "core/include/nexus/core.h", "soc/family/package.json"):
            self.assertIn(name, sdk_consumers.sdk_changes([private, name]))

    def test_fresh_sdk_is_moved_and_revision_checked_before_consumption(self):
        with tempfile.TemporaryDirectory() as parent:
            root = Path(parent) / "checkout"
            root.mkdir()
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(["git", "-c", "user.name=test", "-c", "user.email=test@example.invalid",
                            "commit", "--allow-empty", "-qm", "SDK input"], cwd=root, check=True)
            head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root,
                                           text=True).strip()
            fake_head = [head]
            actions = []

            def run(command, **options):
                actions.append(command)
                if "cmake/package/package_source_sdk.py" in command:
                    path = Path(command[command.index("--output") + 1]) / "share/nexus/src"
                    path.mkdir(parents=True)
                    (path / ".nexus-source-sdk.json").write_text(json.dumps(
                        {"source_revision": fake_head[0], "publishable": True, "source_dirty": False}))
                if "cmake/package/verify_consumers.py" in command:
                    moved = Path(command[command.index("--prefix") + 1])
                    self.assertTrue((moved / "share/nexus/src/.nexus-source-sdk.json").is_file())
                    self.assertFalse(moved.with_name("export").exists())

            device = ["components/devices/tmp117/src/tmp117.c"]
            sdk_consumers.verify_checkout(root, head, device, Path(parent) / "report", run=run)
            self.assertIn("--changed-path", actions[-1])
            self.assertIn(device[0], actions[-1])
            actions.clear()
            fake_head[0] = "c" * 40
            with self.assertRaisesRegex(ValueError, "revision"):
                sdk_consumers.verify_checkout(root, head, device, Path(parent) / "bad", run=run)
            self.assertFalse(any("cmake/package/verify_consumers.py" in call for call in actions))

    def test_repository_ci_uses_exact_change_identity_and_fresh_export(self):
        import yaml
        root = Path(__file__).resolve().parents[2]
        jobs = yaml.safe_load((root / ".github/workflows/enterprise-tools.yml").read_text())["jobs"]
        steps = jobs["sdk-consumers"]["steps"]
        self.assertIn("github.event.pull_request.head.sha", steps[0]["with"]["ref"])
        commands = "\n".join(step.get("run", "") for step in steps)
        self.assertIn("scripts/ci/sdk_consumers.py", commands)
        self.assertIn("--plan", commands)
        self.assertIn("--output build/quality/sdk-consumers", commands)
