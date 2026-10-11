"""Real command failure, input-boundary and image-lock rejection behavior."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, command, file_identity
from tools.evidence.container import inspect_image
from tools.evidence.reproduce import copy_tracked, reproduce, run_logged, tree_identity, verify_source_sdk


class ReproductionTests(unittest.TestCase):
    def test_container_argv_fits_a_bounded_explicit_argument_contract(self):
        self.assertEqual(len(command(["argument"] * 68)), 68)
        with self.assertRaises(EvidenceError):
            command(["argument"] * 129)

    def test_development_dirty_or_other_source_sdk_cannot_qualify(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / ".nexus-source-sdk.json"
            atomic_json(manifest, {"kind": "provenance negative test fixture"})
            source = {"commit": "a" * 40, "tree": "b" * 40}
            base = {"publishable": True, "source_dirty": False,
                    "source_revision": source["commit"], "source_tree": source["tree"]}
            for changed in ({"publishable": False}, {"source_dirty": True},
                            {"source_revision": "c" * 40}, {"source_tree": "c" * 40}):
                # Isolate provenance rejection after the package validator returns.
                # This fixture is never accepted as a real source SDK.
                with self.subTest(changed=changed), patch(
                        "tools.evidence.reproduce.verify_sdk_package", return_value={**base, **changed}):
                    with self.assertRaisesRegex(EvidenceError, "same clean qualified"):
                        verify_source_sdk(file_identity(manifest), source)

    def test_ad_hoc_sdk_manifest_cannot_replace_complete_package(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / ".nexus-source-sdk.json"
            atomic_json(manifest, {"dependencies": [], "publishable": True})
            with self.assertRaises(ValueError):
                verify_source_sdk(file_identity(manifest), {"commit": "a" * 40, "tree": "b" * 40})

    def test_actual_external_nonzero_exit_and_raw_log_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged([sys.executable, "-c", "print('actual failure'); raise SystemExit(37)"],
                                Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 37)
            self.assertIn("actual failure", Path(report["raw"]["path"]).read_text())

    def test_missing_external_tool_is_nonzero(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged(["/does/not/exist"], Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 127)
            self.assertIsNotNone(report["failure"])

    def test_actual_timeout_is_nonzero(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged([sys.executable, "-c", "import time; time.sleep(10)"],
                                Path(directory) / "raw.log", timeout_s=1)
            self.assertEqual(report["exit_code"], 124)

    def test_background_process_cannot_fake_success(self):
        with tempfile.TemporaryDirectory() as directory:
            code = "import subprocess; subprocess.Popen(['sleep','10'])"
            report = run_logged([sys.executable, "-c", code], Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 125)

    def test_tree_identity_rejects_empty_and_symlink(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(EvidenceError):
                tree_identity(root)
            (root / "input").write_text("declared input")
            original = tree_identity(root)
            (root / "input").write_text("changed declared input")
            self.assertNotEqual(tree_identity(root), original)
            (root / "link").symlink_to(root / "input")
            with self.assertRaises(EvidenceError):
                tree_identity(root)

    def test_clean_source_snapshot_does_not_include_host_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "checkout"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            (source / "source.c").write_text("tracked source")
            subprocess.run(["git", "-C", str(source), "add", "source.c"], check=True)
            (source / "host-secret.txt").write_text("untracked must never be mounted")
            copy_tracked(source, root / "snapshot")
            self.assertTrue((root / "snapshot/source.c").is_file())
            self.assertFalse((root / "snapshot/host-secret.txt").exists())
            self.assertFalse((root / "snapshot/.git").exists())

    def test_missing_docker_cannot_establish_environment(self):
        with self.assertRaises(EvidenceError):
            inspect_image("nonexistent", executable="/does/not/exist")

    def test_invalid_spec_fails_with_explicit_unestablished_results(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec, report = root / "spec.json", root / "report.json"
            atomic_json(spec, {"schema_version": 1})
            result = reproduce(spec, report, docker="/does/not/exist")
            self.assertEqual(result["status"], "failed")
            self.assertEqual(result["hermetic_status"], "not_established")
            self.assertEqual(result["reproducible_status"], "not_established")
            self.assertEqual(json.loads(report.read_text()), result)


if __name__ == "__main__":
    unittest.main()
