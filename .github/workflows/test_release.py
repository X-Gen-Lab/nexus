"""Execute release workflow preparation and fail-closed publication boundaries."""

from datetime import datetime, timedelta, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import tarfile
import unittest.mock

import yaml

ROOT = Path(__file__).resolve().parents[2]
COMMIT = "a" * 40


def workflow():
    return yaml.load((ROOT / ".github/workflows/release.yml").read_text(),
                     Loader=yaml.BaseLoader)


def consumer_module():
    specification = importlib.util.spec_from_file_location(
        "release_consumer_fixture", ROOT / "cmake/package/verify_consumers.py")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


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
                "import argparse,importlib.util,json,subprocess,sys\nfrom pathlib import Path\n"
                f"sys.path.insert(0, {str(ROOT)!r})\n"
                f"s=importlib.util.spec_from_file_location('consumer_fixture', {str(ROOT / 'cmake/package/verify_consumers.py')!r})\n"
                "m=importlib.util.module_from_spec(s);s.loader.exec_module(m)\n"
                "p=argparse.ArgumentParser();p.add_argument('--prefix');"
                "p.add_argument('--output');a=p.parse_args();o=Path(a.output)\n"
                "o.mkdir(parents=True)\n"
                "(o/'.nexus-sdk-consumer-check').write_text('owned SDK consumer verification\\n')\n"
                "for n in ('native','stm32','gd32','h743 baremetal','h743 freertos','u575 baremetal','u575 freertos'):\n"
                " m.prepare_consumer(Path(a.prefix),o,n,lambda c: subprocess.run(c,check=True,capture_output=True),version='0.1.0')\n"
                "(o/'command-00.log').write_text('passed consumer command\\n')\n"
                f"(o/'verification.json').write_text(json.dumps({{'kind': 'source_sdk_consumer_verification', 'status': 'passed', 'source_revision': {COMMIT!r}, 'physical_status': 'not_executed', 'commands': [{{'argv': ['git','init'], 'exit_code': 0, 'log': 'command-00.log'}}]}}))\n")
            (source / "tools").mkdir()
            shutil.copytree(ROOT / "tools/release", source / "tools/release")
            for command in (["git", "init", "--quiet"],
                            ["git", "add", "tools/release/transport.py"],
                            ["git", "-c", "user.name=Fixture", "-c",
                             "user.email=fixture@example.invalid", "commit",
                             "--quiet", "-m", "Versioned transport builder"]):
                subprocess.run(command, cwd=source, check=True,
                               capture_output=True)
            builder = subprocess.check_output(["git", "rev-parse", "HEAD"],
                                              cwd=source, text=True).strip()
            environment = {**os.environ, "GITHUB_WORKSPACE": str(source),
                           "RUNNER_TEMP": str(temporary), "GITHUB_SHA": builder,
                           "NEXUS_EXPECTED_COMMIT": COMMIT}
            steps = workflow()["jobs"]["source-sdk"]["steps"]
            selected = [step for step in steps if step.get("run") and any(
                value in step["run"] for value in
                ("package_source_sdk.py", "verify_consumers.py",
                 "nexus-consumer-transport.py", "tools/release/delivery.py stage"))]
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

    def test_workflow_keeps_source_checkout_and_names_separate_from_builder_signer(self):
        document = workflow()
        for job_name in ("guard", "source-sdk", "publish"):
            checkout = document["jobs"][job_name]["steps"][0]
            self.assertEqual(checkout["with"]["ref"],
                             "${{ inputs.expected_commit }}")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            calls = root / "calls.jsonl"
            gh = root / "gh"
            gh.write_text("#!" + sys.executable + "\nimport json,os,sys\n"
                "with open(os.environ['CALLS'],'a') as f: f.write(json.dumps(sys.argv[1:])+'\\n')\n"
                "print(json.dumps({'run_attempt':1}))\n")
            gh.chmod(0o755)
            environment = {**os.environ, "PATH": str(root) + os.pathsep + os.environ["PATH"],
                "CALLS": str(calls), "RUNNER_TEMP": str(root),
                "NEXUS_EXPECTED_COMMIT": COMMIT, "GITHUB_SHA": "b" * 40,
                "GITHUB_REF": "refs/heads/delivery/0.1.0", "GITHUB_RUN_ID": "321",
                "NEXUS_CI_RUN_ID": "123", "GH_REPO": "X-Gen-Lab/nexus"}
            steps = document["jobs"]["publish"]["steps"]
            for step in steps:
                if step.get("name", "").startswith(("Download this run", "Verify the actual GitHub")):
                    subprocess.run(["bash", "-e", "-o", "pipefail", "-c", step["run"]],
                        cwd=ROOT, env=environment, capture_output=True, check=True)
            observed = [json.loads(line) for line in calls.read_text().splitlines()]
            downloads = [call for call in observed if call[:2] == ["run", "download"]
                         and "--name" in call]
            self.assertEqual({call[call.index("--name") + 1] for call in downloads},
                {"nexus-source-sdk-" + COMMIT, "nexus-source-sdk-attestation-" + COMMIT,
                 "nexus-documentation-" + COMMIT})
            verify = next(call for call in observed if call[:2] == ["attestation", "verify"])
            self.assertEqual(verify[verify.index("--signer-digest") + 1], "b" * 40)
            self.assertEqual(verify[verify.index("--source-digest") + 1], "b" * 40)
            self.assertEqual(verify[verify.index("--source-ref") + 1],
                             "refs/heads/delivery/0.1.0")

    def test_workflow_guard_rejects_arbitrary_dispatch_refs(self):
        step = workflow()["jobs"]["guard"]["steps"][1]
        actual = subprocess.check_output(["git", "rev-parse", "HEAD"],
                                         cwd=ROOT, text=True).strip()
        for ref in ("refs/heads/main", "refs/heads/delivery/0.1.0",
                    "refs/heads/unreviewed", "refs/tags/v0.1.0"):
            result = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", step["run"]],
                cwd=ROOT, env={**os.environ, "GITHUB_REF": ref,
                    "NEXUS_EXPECTED_COMMIT": actual, "NEXUS_PUBLISH": "true"},
                capture_output=True, text=True)
            self.assertEqual(result.returncode == 0,
                             ref in {"refs/heads/main", "refs/heads/delivery/0.1.0"})
        result = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", step["run"]],
            cwd=ROOT, env={**os.environ, "GITHUB_REF": "refs/heads/delivery/0.1.0",
                "NEXUS_EXPECTED_COMMIT": actual, "NEXUS_PUBLISH": "false",
                "GITHUB_SHA": "b" * 40}, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Standalone qualification", result.stderr)

    def test_publisher_runs_after_qualified_sdk_with_skipped_transitive_jobs(self):
        document = workflow()["jobs"]
        condition = document["publish"]["if"].strip()
        condition = condition.removeprefix("${{").removesuffix("}}").strip()
        ancestors = set()

        def visit(job):
            dependencies = document[job].get("needs", [])
            if isinstance(dependencies, str):
                dependencies = [dependencies]
            for dependency in dependencies:
                if dependency not in ancestors:
                    ancestors.add(dependency)
                    visit(dependency)

        visit("publish")
        # Actual run 38686564636: direct dependencies succeeded, while reusable
        # qualification ancestors were deliberately skipped after full CI.
        observed = {job: "success" for job in ancestors}
        observed.update(platform="skipped", contracts="skipped")

        def admitted(publishing, results, cancelled=False):
            # GitHub adds success() when an expression has no status function;
            # skipped transitive dependencies then short-circuit the job.
            if not re.search(r"\b(?:always|success|failure|cancelled)\s*\(", condition):
                if any(results[job] != "success" for job in ancestors):
                    return False
            for term in re.split(r"\s*&&\s*", condition.strip()):
                if term == "always()":
                    continue
                if term == "!cancelled()":
                    if cancelled:
                        return False
                    continue
                if term == "inputs.publish":
                    if not publishing:
                        return False
                    continue
                comparison = re.fullmatch(r"needs\.([\w-]+)\.result == '([\w]+)'", term)
                self.assertIsNotNone(comparison, "Unsupported workflow condition")
                job, expected = comparison.groups()
                if results[job] != expected:
                    return False
            return True

        self.assertTrue(admitted(True, observed),
                        "Qualified SDK must reach publication despite skipped ancestors")
        self.assertFalse(admitted(False, observed))
        self.assertFalse(admitted(True, observed, cancelled=True),
                         "Manual cancellation must prevent publication")
        for job in ("guard", "source-sdk"):
            for result in ("failure", "cancelled", "skipped"):
                with self.subTest(job=job, result=result):
                    self.assertFalse(admitted(True, {**observed, job: result}))


class ConsumerTransportTests(unittest.TestCase):
    def fixture(self):
        prepare_consumer = consumer_module().prepare_consumer
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        base = Path(temporary.name)
        raw = base / "raw consumers"
        raw.mkdir()
        (raw / ".nexus-sdk-consumer-check").write_text(
            "owned SDK consumer verification\n")
        for name in ("native", "stm32", "gd32", "h743 baremetal",
                     "h743 freertos", "u575 baremetal", "u575 freertos"):
            prepare_consumer(base / "prefix", raw, name,
                lambda command: subprocess.run(command, check=True,
                    capture_output=True), version="0.1.0")
        (raw / "command-00.log").write_bytes(b"actual consumer output\n")
        (raw / "verification.json").write_text(json.dumps({
            "kind": "source_sdk_consumer_verification", "status": "passed",
            "source_revision": COMMIT, "physical_status": "not_executed",
            "commands": [{"argv": ["git", "init"], "exit_code": 0,
                          "log": "command-00.log"}]}))
        return base, raw

    def test_real_consumer_git_metadata_survives_transport_and_identity_boundary(self):
        from tools.maintenance.job_identity import _inventory
        from tools.release import transport
        from tools.release.delivery import stage
        base, raw = self.fixture()
        (raw / "verification.json").chmod(0o600)
        (raw / "consumer.elf").write_bytes(b"\x7fELF consumer bytes")
        (raw / "consumer.elf").chmod(0o755)
        source = base / "checkout"
        source.mkdir()
        prefix = base / "prefix"
        payload = prefix / "share/nexus/src"
        payload.mkdir(parents=True)
        (payload / "payload").write_bytes(b"verified SDK")
        # SDK verification is an external port; the real stage and identity
        # inventory execute against the actual git-init consumer fixtures.
        with unittest.mock.patch("tools.release.delivery.verify_sdk"):
            stage(source, prefix, raw, source / "build/raw", COMMIT)
            with self.assertRaisesRegex(ValueError,
                    "artifact must remain inside the checkout"):
                _inventory(source, [source / "build/raw"],
                           source / "build/job-identities/raw")
            packed = base / "transport"
            transport.pack(raw, packed, COMMIT)
            # Actions normalizes outer artifact file permissions to 0644.
            # Original raw modes are evidence inside the immutable archive.
            for item in packed.iterdir():
                item.chmod(0o644)
            transport.verify(packed, COMMIT)
            stage(source, prefix, packed, source / "build/packed", COMMIT)
        selected, missing = _inventory(source, [source / "build/packed"],
                                      source / "build/job-identities/packed")
        self.assertTrue(selected)
        self.assertEqual(missing, [])
        transport.verify(packed, COMMIT)
        self.assertEqual((packed / "verification.json").read_bytes(),
                         (raw / "verification.json").read_bytes())
        with tarfile.open(packed / "consumer-raw.tar.gz") as archive:
            self.assertEqual(archive.getmember("verification.json").mode, 0o600)
            self.assertEqual(archive.getmember("consumer.elf").mode, 0o755)
            restored = {member.name: archive.extractfile(member).read()
                        for member in archive if member.isfile()}
        original = {path.relative_to(raw).as_posix(): path.read_bytes()
                    for path in raw.rglob("*") if path.is_file()}
        self.assertEqual(restored, original)
        self.assertEqual(len(list(raw.rglob(".git"))), 7)

    def test_rejects_links_overlap_existing_output_and_unowned_raw(self):
        from tools.release import transport
        for change in ("symlink", "overlap", "existing", "unowned", "hardlink"):
            with self.subTest(change=change):
                base, raw = self.fixture()
                output = base / "transport"
                if change == "symlink":
                    (raw / "linked").symlink_to(raw / "verification.json")
                elif change == "overlap":
                    output = raw / "transport"
                elif change == "existing":
                    output.mkdir()
                elif change == "unowned":
                    (raw / ".nexus-sdk-consumer-check").unlink()
                else:
                    os.link(raw / "verification.json", raw / "hardlinked")
                with self.assertRaises(ValueError):
                    transport.pack(raw, output, COMMIT)

    def test_git_metadata_must_remain_empty_init_fixture(self):
        from tools.release import transport
        for name, contents in (("config", "[remote \"origin\"]\nurl=secret\n"),
                               ("index", "tracked index"),
                               ("objects/ab/object", "history"),
                               ("hooks/pre-commit", "active hook")):
            with self.subTest(name=name):
                base, raw = self.fixture()
                path = raw / "native source/.git" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents)
                with self.assertRaises(ValueError):
                    transport.pack(raw, base / "transport", COMMIT)

    def test_modified_report_archive_traversal_and_duplicate_entries_are_rejected(self):
        from tools.release import transport
        for change in ("report", "traversal", "duplicate", "archive-link", "mode"):
            with self.subTest(change=change):
                base, raw = self.fixture()
                output = base / "transport"
                transport.pack(raw, output, COMMIT)
                if change == "report":
                    (output / "verification.json").write_text("{}")
                elif change == "mode":
                    rewritten = base / "modified.tar.gz"
                    with tarfile.open(output / "consumer-raw.tar.gz") as original:
                        with tarfile.open(rewritten, "w:gz") as archive:
                            for entry in original:
                                if entry.name == "verification.json":
                                    entry.mode ^= 1
                                archive.addfile(entry, original.extractfile(entry)
                                                if entry.isfile() else None)
                    rewritten.replace(output / "consumer-raw.tar.gz")
                else:
                    with tarfile.open(output / "consumer-raw.tar.gz", "w:gz") as archive:
                        for _ in range(2 if change == "duplicate" else 1):
                            entry = tarfile.TarInfo("../escape" if
                                change == "traversal" else "verification.json")
                            if change == "archive-link":
                                entry.type, entry.linkname = tarfile.SYMTYPE, "../escape"
                                archive.addfile(entry)
                            elif change == "duplicate":
                                entry.size = (raw / "verification.json").stat().st_size
                                with (raw / "verification.json").open("rb") as stream:
                                    archive.addfile(entry, stream)
                            else:
                                archive.addfile(entry)
                if change != "report":
                    manifest = json.loads((output / "consumer-raw-manifest.json").read_text())
                    manifest["archive"] = {key: value for key, value in
                        transport.identity(output / "consumer-raw.tar.gz").items()
                        if key in {"sha256", "size"}}
                    (output / "consumer-raw-manifest.json").write_text(json.dumps(manifest))
                expected = {"report": "report|Passed strict", "traversal": "Unsafe transport member",
                            "duplicate": "Duplicate", "archive-link": "nonregular",
                            "mode": "complete raw inventory"}[change]
                with self.assertRaisesRegex(ValueError, expected):
                    transport.verify(output, COMMIT)

    def test_failed_source_mismatched_or_unbounded_raw_evidence_is_rejected(self):
        from tools.release import transport
        for change in ("source", "command", "count", "bytes"):
            with self.subTest(change=change):
                base, raw = self.fixture()
                report = json.loads((raw / "verification.json").read_text())
                if change == "source":
                    report["source_revision"] = "b" * 40
                elif change == "command":
                    report["commands"][0]["exit_code"] = 1
                (raw / "verification.json").write_text(json.dumps(report))
                limits = {"MAX_FILES": 2 if change == "count" else transport.MAX_FILES,
                          "MAX_BYTES": 2 if change == "bytes" else transport.MAX_BYTES}
                with unittest.mock.patch.multiple(transport, **limits):
                    with self.assertRaises(ValueError):
                        transport.pack(raw, base / "transport", COMMIT)

    def test_builder_receipt_extends_exact_release_closure_without_rewriting_source(self):
        from tools.release import transport
        base, raw = self.fixture()
        inputs = base / "inputs"
        packed = inputs / "sdk/delivery/sdk-consumers"
        transport.pack(raw, packed, COMMIT)
        output = base / "assets"
        output.mkdir()
        evidence = output / "source-sdk-identity.json"
        evidence.write_bytes(b"original C identity")
        manifest = {"source_commit": COMMIT, "tag": "v0.1.0",
            "source_sdk_run_id": 321, "assets": [{"name": evidence.name,
                "size": evidence.stat().st_size,
                "sha256": hashlib.sha256(evidence.read_bytes()).hexdigest()}]}
        (output / "release-manifest.json").write_text(json.dumps(manifest))
        workflow_ref = "X-Gen-Lab/nexus/.github/workflows/release.yml@refs/heads/delivery/0.1.0"
        transport.attach(output, inputs, COMMIT, "b" * 40,
                         workflow_ref, 321, 1)
        receipt = json.loads((output / "delivery-workflow.json").read_text())
        self.assertEqual(receipt["source_commit"], COMMIT)
        self.assertEqual(receipt["builder_commit"], "b" * 40)
        self.assertEqual(receipt["original_failed_run_id"], 38684283007)
        self.assertEqual(evidence.read_bytes(), b"original C identity")
        extended = json.loads((output / "release-manifest.json").read_text())
        self.assertEqual({item["name"] for item in extended["assets"]},
                         {"delivery-workflow.json", evidence.name})


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
            api = unittest.mock.Mock()
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
        api = unittest.mock.Mock()
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
