"""Execute release workflow preparation and fail-closed publication boundaries."""

from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import tarfile
import unittest
from unittest import mock

import yaml

ROOT = Path(__file__).resolve().parents[2]
COMMIT = "a" * 40


def workflow():
    return yaml.load((ROOT / ".github/workflows/release.yml").read_text(),
                     Loader=yaml.BaseLoader)


class SDKWorkflowExecutionTests(unittest.TestCase):
    def test_actual_workflow_prepares_outside_checkout_then_stages_verified_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, temporary = base / "checkout", base / "runner temporary"
            (source / "cmake/package").mkdir(parents=True)
            (source / ".venv/bin").mkdir(parents=True)
            temporary.mkdir()
            (source / ".venv/bin/python").write_text(
                "#!/bin/sh\nexec " + shlex.quote(sys.executable) + ' "$@"\n')
            (source / ".venv/bin/python").chmod(0o755)
            # Package/consumer commands are external ports. The real staging
            # implementation, workflow shell and copied byte inventory execute.
            (source / "cmake/package/package_source_sdk.py").write_text(
                "import argparse,json\nfrom pathlib import Path\n"
                "p=argparse.ArgumentParser();p.add_argument('--source');"
                "p.add_argument('--output');p.add_argument('--verify');a=p.parse_args()\n"
                "if a.output:\n"
                " s=Path(a.source).resolve();o=Path(a.output).resolve()\n"
                " if o.is_relative_to(s):raise SystemExit('SDK output inside checkout')\n"
                " d=o/'share/nexus/src';d.mkdir(parents=True)\n"
                " (d/'payload').write_bytes(b'approved source bytes')\n"
                f" (d/'.nexus-source-sdk.json').write_text(json.dumps({{'source_revision': {COMMIT!r}, 'publishable': True, 'source_dirty': False}}))\n"
                "if a.verify:\n"
                " d=Path(a.verify)\n"
                " if (d/'payload').read_bytes()!=b'approved source bytes':raise SystemExit('changed payload')\n"
                " print((d/'.nexus-source-sdk.json').read_text())\n")
            (source / "cmake/package/verify_consumers.py").write_text(
                "import argparse,json\nfrom pathlib import Path\n"
                "p=argparse.ArgumentParser();p.add_argument('--prefix');"
                "p.add_argument('--output');a=p.parse_args();o=Path(a.output)\n"
                "o.mkdir(parents=True)\n"
                f"(o/'verification.json').write_text(json.dumps({{'kind': 'source_sdk_consumer_verification', 'status': 'passed', 'source_revision': {COMMIT!r}, 'physical_status': 'not_executed'}}))\n")
            (source / "tools").mkdir()
            shutil.copytree(ROOT / "tools/release", source / "tools/release")
            environment = {**os.environ, "GITHUB_WORKSPACE": str(source),
                           "RUNNER_TEMP": str(temporary), "GITHUB_SHA": COMMIT,
                           "NEXUS_EXPECTED_COMMIT": COMMIT}
            steps = workflow()["jobs"]["source-sdk"]["steps"]
            selected = [step for step in steps if step.get("run") and any(
                value in step["run"] for value in
                ("package_source_sdk.py", "verify_consumers.py", "tools/release/delivery.py stage"))]
            self.assertGreaterEqual(len(selected), 3)
            for step in selected:
                result = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", step["run"]],
                    cwd=source, env=environment, capture_output=True, text=True,
                    check=False, timeout=20)
                self.assertEqual(result.returncode, 0,
                                 step["name"] + ": " + result.stdout + result.stderr)
            staged = source / "build/delivery/source-sdk/share/nexus/src/payload"
            self.assertEqual(staged.read_bytes(), b"approved source bytes")
            self.assertTrue((source / "build/delivery/staging-copy.json").is_file())


class ReleaseAdmissionTests(unittest.TestCase):
    def run_record(self):
        return {"id": 123, "run_attempt": 1, "path": ".github/workflows/ci.yml",
                "event": "workflow_dispatch", "head_sha": COMMIT,
                "status": "completed", "conclusion": "success",
                "head_branch": "main", "created_at": datetime.now(
                    timezone.utc).isoformat(), "html_url": "https://example.invalid/run/123"}

    def jobs_record(self):
        document = yaml.load((ROOT / ".github/workflows/ci.yml").read_text(),
                             Loader=yaml.BaseLoader)
        names = []
        for job in document["jobs"].values():
            if "uses" not in job:
                names.append(job["name"])
                continue
            called = yaml.load((ROOT / job["uses"]).read_text(),
                               Loader=yaml.BaseLoader)
            for item in called["jobs"].values():
                matrix = item.get("strategy", {}).get("matrix", {})
                if matrix:
                    key, values = next(iter(matrix.items()))
                    names.extend(item["name"].replace("${{ matrix." + key + " }}", value)
                                 for value in values)
                else:
                    names.append(item["name"])
        return {"total_count": len(names), "jobs": [{"name": name,
                "status": "completed", "conclusion": "success"} for name in names]}

    def test_ci_requires_complete_successful_domains_and_matrix(self):
        from tools.release.delivery import require_jobs
        valid = self.jobs_record()
        require_jobs(valid)
        for name in ("Build Docs", "Native contracts (native-tsan)",
                     "ARM link and budgets (stm32-sky-baremetal)",
                     "Provider assembly audits (h743_assembly)"):
            jobs = [job for job in valid["jobs"] if job["name"] != name]
            with self.subTest(name=name), self.assertRaises(ValueError):
                require_jobs({"total_count": len(jobs), "jobs": jobs})
        substituted = self.jobs_record()
        for job in substituted["jobs"]:
            if job["name"] == "Native contracts (native-tsan)":
                job["name"] = "Native contracts (fabricated-substitute)"
        with self.assertRaisesRegex(ValueError, "matrix identities differ"):
            require_jobs(substituted)
        valid["jobs"][0]["conclusion"] = "skipped"
        with self.assertRaises(ValueError):
            require_jobs(valid)

    def test_guard_executes_exact_source_rejection(self):
        result = subprocess.run([sys.executable, "tools/release/delivery.py", "guard",
            "--expected-commit", COMMIT, "--actual-commit", "b" * 40], cwd=ROOT,
            capture_output=True, text=True, check=False, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Expected commit differs", result.stderr)

    def test_requires_fresh_full_ci_for_exact_source(self):
        from tools.release.delivery import require_ci
        require_ci(self.run_record(), COMMIT)
        require_ci({**self.run_record(), "event": "push"}, COMMIT)
        for changed in ({"head_sha": "b" * 40}, {"event": "pull_request"},
                        {"conclusion": "failure"}, {"status": "in_progress"},
                        {"path": ".github/workflows/release.yml"},
                        {"created_at": (datetime.now(timezone.utc) -
                                        timedelta(days=2)).isoformat()}):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                require_ci({**self.run_record(), **changed}, COMMIT)

    def test_publisher_verifies_all_uploaded_bytes_before_publishing(self):
        from tools.release.delivery import publish
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "asset.tar.gz").write_bytes(b"verified archive")
            manifest = {"tag": "v0.1.0", "source_commit": COMMIT,
                        "assets": [{"name": "asset.tar.gz", "size": 16,
                                    "sha256": hashlib.sha256(
                                        b"verified archive").hexdigest()}]}
            (root / "release-manifest.json").write_text(json.dumps(manifest))
            api = mock.Mock()
            api.get.side_effect = [self.run_record(), self.jobs_record(), {"object": {"sha": COMMIT,
                "type": "commit"}}, {"object": {"sha": COMMIT, "type": "commit"}},
                None, []]
            api.create.return_value = {"id": 5, "upload_url": "upload"}
            api.upload.return_value = {"id": 7, "name": "asset.tar.gz", "size": 16}
            api.asset_bytes.return_value = b"changed bytes"
            with self.assertRaisesRegex(ValueError, "[Uu]ploaded release asset"):
                publish(root, COMMIT, 123, api=api)
            api.publish.assert_not_called()

    def publisher_fixture(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "asset.tar.gz").write_bytes(b"verified archive")
        (root / "release-manifest.json").write_text(json.dumps({
            "tag": "v0.1.0", "source_commit": COMMIT,
            "assets": [{"name": "asset.tar.gz", "size": 16,
                        "sha256": hashlib.sha256(b"verified archive").hexdigest()}]}))
        api = mock.Mock()
        uploaded = []

        def get(path):
            if path == "/actions/runs/123":
                return self.run_record()
            if "/jobs?" in path:
                return self.jobs_record()
            if path.startswith("/git/ref/"):
                return {"object": {"type": "commit", "sha": COMMIT}}
            if path.startswith("/releases/tags/"):
                return None
            if path.startswith("/releases/5/assets?"):
                return uploaded
            raise AssertionError("Unexpected external API request: " + path)

        def upload(release, path):
            asset = {"id": len(uploaded) + 1, "name": path.name,
                     "size": path.stat().st_size}
            uploaded.append(asset)
            return asset

        api.get.side_effect = get
        api.create.return_value = {"id": 5, "upload_url": "upload"}
        api.upload.side_effect = upload
        api.asset_bytes.side_effect = lambda asset: (root / asset["name"]).read_bytes()
        return root, api

    def test_complete_immutable_assets_are_verified_then_preview_is_published(self):
        from tools.release.delivery import publish
        root, api = self.publisher_fixture()
        publish(root, COMMIT, 123, api=api)
        self.assertEqual(api.asset_bytes.call_count, 2)
        self.assertTrue(api.create.call_args.args[0]["draft"])
        self.assertTrue(api.create.call_args.args[0]["prerelease"])
        api.publish.assert_called_once_with(5)

    def test_tag_mismatch_or_existing_release_cannot_create_or_upload(self):
        from tools.release.delivery import publish
        for bad_path, response in (("/git/ref/tags/v0.1.0",
                {"object": {"type": "commit", "sha": "b" * 40}}),
                ("/releases/tags/v0.1.0", {"id": 4, "draft": True})):
            root, api = self.publisher_fixture()
            original = api.get.side_effect
            api.get.side_effect = lambda path: response if path == bad_path else original(path)
            with self.subTest(path=bad_path), self.assertRaises(ValueError):
                publish(root, COMMIT, 123, api=api)
            api.create.assert_not_called()
            api.upload.assert_not_called()

    def test_tag_move_during_upload_leaves_the_release_draft(self):
        from tools.release.delivery import publish
        root, api = self.publisher_fixture()
        original = api.get.side_effect
        calls = 0

        def get(path):
            nonlocal calls
            if path == "/git/ref/tags/v0.1.0":
                calls += 1
                if calls == 2:
                    return {"object": {"type": "commit", "sha": "b" * 40}}
            return original(path)

        api.get.side_effect = get
        with self.assertRaisesRegex(ValueError, "release tag differs"):
            publish(root, COMMIT, 123, api=api)
        self.assertEqual(api.upload.call_count, 2)
        api.publish.assert_not_called()

    def test_hidden_documentation_bytes_must_survive_the_real_upload_contract(self):
        from tools.evidence.identity import canonical_digest
        from tools.release.delivery import verify_identity
        docs = yaml.load((ROOT / ".github/workflows/docs-build.yml").read_text(),
                         Loader=yaml.BaseLoader)
        upload = next(step for step in docs["jobs"]["build"]["steps"]
                      if step.get("uses", "").startswith("actions/upload-artifact@"))
        self.assertEqual(upload["with"].get("include-hidden-files"), "true")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            hidden = root / "documentation/en/.buildinfo"
            hidden.parent.mkdir(parents=True)
            payload = b"observed Sphinx build identity"
            record = {"kind": "ci_job_identity", "job": "documentation",
                "job_result": "success", "source": {"dirty": False, "commit": COMMIT},
                "run": {"GITHUB_RUN_ID": "123", "GITHUB_RUN_ATTEMPT": "1"},
                "missing_artifacts": [], "artifacts": [{
                    "relative_path": "build/documentation/en/.buildinfo",
                    "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest()}]}
            record["record_sha256"] = canonical_digest(record)
            identity = root / "identity.json"
            identity.write_text(json.dumps(record))
            with self.assertRaisesRegex(ValueError, "Job artifact bytes changed"):
                verify_identity(identity, root, COMMIT, "documentation", 123, 1)
            hidden.write_bytes(payload)
            verify_identity(identity, root, COMMIT, "documentation", 123, 1)

    def test_evidence_archive_retains_raw_logs_hidden_docs_and_identity_paths(self):
        from tools.release.delivery import archive_evidence
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs = root / "inputs"
            paths = ("sdk/delivery/sdk-consumers/raw-build.log",
                     "sdk/delivery/sdk-consumers/firmware/image.elf",
                     "sdk/delivery/staging-copy.json",
                     "sdk/job-identities/source-sdk/identity.json",
                     "sdk/job-identities/source-sdk/python-version.log",
                     "docs/documentation/en/.buildinfo",
                     "docs/docs-job-identity/identity.json",
                     "attestation/delivery/attestation/bundle.json",
                     "ci-evidence/native-native-tsan/contracts.log",
                     "ci-run.json", "ci-jobs.json")
            for name in paths:
                path = inputs / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("observed " + name)
            archive = root / "evidence.tar.gz"
            archive_evidence(inputs, archive)
            with tarfile.open(archive) as stream:
                for name in paths:
                    self.assertEqual(stream.extractfile(name).read(),
                                     (inputs / name).read_bytes())

    def test_contents_write_is_confined_to_publisher(self):
        document = workflow()
        self.assertEqual(document["permissions"]["contents"], "read")
        self.assertIn("expected_commit", document["on"]["workflow_dispatch"]["inputs"])
        for name, job in document["jobs"].items():
            self.assertEqual(job.get("permissions", {}).get("contents", "read"),
                             "write" if name == "publish" else "read")


if __name__ == "__main__":
    unittest.main()
