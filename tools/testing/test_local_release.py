"""Local preview admission preserves full executions without inventing CI IDs."""

from contextlib import ExitStack
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import qualification
from tools.evidence import candidate, store
from tools.evidence.common import EvidenceError, file_identity
from tools.evidence.identity import canonical_digest, git_source
from tools.release import delivery, transport
from tools.documentation import delivery as documentation_delivery


ROOT = Path(__file__).resolve().parents[2]


class LocalReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base / "source"
        self.source.mkdir()
        # A small owned checker exercises the real command runner, report and
        # original attempt receipts. It is a release-boundary fixture, not a
        # claim that one print statement is Nexus software qualification.
        self.plan = {"schema_version": 1, "formal": True, "stages": [
            {"name": "complete-fixture", "checker": "configure", "args": []},
            {"name": "source-sdk", "checker": "source_sdk", "args": [
                "--source", "{root}", "--output", "{output}/sdk"]},
            {"name": "sdk-consumers", "checker": "sdk_consumers", "args": [
                "--prefix", "{output}/sdk", "--output", "{output}/sdk-consumers"]}]}
        self.file("source/tools/dev/plans/software.json", json.dumps(self.plan))
        self.file("source/tools/dev/dev.py", "print('fixture command executed')\n")
        common = ("import hashlib,json,os,subprocess,sys\nfrom pathlib import Path\n"
            "root=Path(__file__).resolve().parents[2]\n"
            "env={k:v for k,v in os.environ.items() if not k.startswith('GIT_')}\n"
            "revision=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],env=env,text=True).strip()\n"
            "tree=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD^{tree}'],env=env,text=True).strip()\n"
            "output=Path(sys.argv[sys.argv.index('--output')+1])\n")
        self.file("source/cmake/package/package_source_sdk.py", common +
            "sdk=output/'share/nexus/src'\nsdk.mkdir(parents=True)\n"
            "(sdk/'dependencies').mkdir()\n"
            "(sdk/'dependencies/platform-compatibility.json').write_bytes((root/'dependencies/platform-compatibility.json').read_bytes())\n"
            "files={'source.txt':hashlib.sha256(b'source').hexdigest()}\n"
            "snapshot=hashlib.sha256(json.dumps(files,sort_keys=True,separators=(',',':')).encode()).hexdigest()\n"
            "identity={'kind':'nexus-installed-source-sdk','snapshot_sha256':snapshot,'source_revision':revision,'source_tree':tree,'publishable':True,'source_dirty':False,'files_sha256':files}\n"
            "(sdk/'.nexus-source-sdk.json').write_text(json.dumps(identity))\n"
            "(sdk/'cmake/package').mkdir(parents=True)\n"
            "(sdk/'cmake/package/package_source_sdk.py').write_text(\"from pathlib import Path\\nimport json\\nx=json.loads((Path(__file__).resolve().parents[2]/'.nexus-source-sdk.json').read_text())\\nprint(json.dumps(x))\\n\")\n"
            "print(json.dumps({'kind':'nexus_source_sdk_summary','sdk_kind':'nexus-installed-source-sdk','source_revision':revision,'publishable':True,'files':1,'snapshot_sha256':snapshot}))\n")
        self.file("source/cmake/package/verify_consumers.py", common +
            f"sys.path.insert(0,{str(ROOT)!r})\n"
            "from tools.dev.process import run\n"
            "output.mkdir(parents=True)\n"
            "sdk=Path(sys.argv[sys.argv.index('--prefix')+1])/'share/nexus/src'\n"
            "argv=[sys.executable,'-B',str(sdk/'cmake/package/package_source_sdk.py'),'--verify',str(sdk)]\n"
            "result=run(argv,cwd=root,env=env,log=output/'command-00.log',timeout=30,stage='source-sdk-consumer')\n"
            "receipt=json.loads(result.receipt_path.read_text())\n"
            "record={'argv':argv,'exit_code':result.returncode,'log':'command-00.log','receipt':str(result.receipt_path.relative_to(output)),**{k:receipt[k] for k in ('started_ns','finished_ns','duration_seconds','outcome','child_returncode','withdrawal_complete')}}\n"
            "(output/'verification.json').write_text(json.dumps({'kind':'source_sdk_consumer_verification','status':'passed','source_revision':revision,'source_tree':tree,'physical_status':'not_executed','commands':[record]}))\n")
        self.file("source/dependencies/platform-compatibility.json",
                  (ROOT / "dependencies/platform-compatibility.json").read_bytes())
        self.file("source/dependencies/tool.lock.json", '{"pinned": "fixture"}\n')
        self.file("source/dependencies/empty-input", b"").chmod(0o755)
        for argv in (["init", "--quiet"], ["add", "."],
                     ["-c", "user.name=Fixture", "-c",
                      "user.email=fixture@example.invalid", "commit", "--quiet",
                      "-m", "Fixture source"]):
            subprocess.run(["git", "-C", str(self.source), *argv],
                           check=True, capture_output=True,
                           env={key: value for key, value in os.environ.items()
                                if not key.startswith("GIT_")})
        self.source_identity = git_source(self.source)
        self.commit = self.source_identity["commit"]
        self.execution = self.base / "execution" / "report.json"
        qualification.execute(self.plan, self.source, self.execution.parent)
        self.sdk = self.package_tree("sdk", self.execution.parent / "sdk")
        self.consumers = self.package_tree("consumers", self.execution.parent / "sdk-consumers")
        self.candidate = self.package("candidate", {"candidate.json": b"{}"})
        self.docs = self.file("docs.tar.gz", b"immutable documentation fixture")
        self.output = self.base / "local-preview"
        self.arguments = dict(source=self.source, execution=self.execution,
            sdk_package=self.sdk, consumer_package=self.consumers,
            documentation=self.docs, candidate_packages=[self.candidate],
            output=self.output, expected_commit=self.commit)
        # These external SDK/docs/candidate ports have their own complete
        # contract suites. Here, real archives, source and execution receipts
        # exercise the local release implementation and rejection boundaries.
        self.ports = ExitStack()
        self.addCleanup(self.ports.close)
        self.sdk_check = self.ports.enter_context(patch.object(
            delivery, "verify_sdk", return_value={"snapshot_sha256": json.loads(
                (self.execution.parent / "sdk/share/nexus/src/.nexus-source-sdk.json").read_text())[
                    "snapshot_sha256"]}))
        self.transport_check = self.ports.enter_context(patch.object(
            transport, "verify", return_value={"consumer_policy": "fixture-policy",
                "files": transport.scan(self.execution.parent / "sdk-consumers")[0],
                "directories": transport.scan(self.execution.parent / "sdk-consumers")[1]}))
        self.docs_check = self.ports.enter_context(patch.object(
            documentation_delivery, "verify", return_value={
                "documentation_source": self.source_identity,
                "code_source": self.source_identity,
                "version": delivery.load_policy(self.source)["semantic_version"]}))
        self.candidate_check = self.ports.enter_context(patch.object(
            candidate, "verify", return_value={"candidate_id": "fixture-candidate",
                "source": self.source_identity,
                "configuration_sha256": "b" * 64,
                "artifacts": {role: {"sha256": hashlib.sha256(role.encode()).hexdigest(),
                                     "size": 1} for role in ("elf", "bin", "map")},
                "qualified_scopes": sorted(candidate.REQUIRED_SCOPES),
                "sdk_packages": [{"manifest_sha256": file_identity(
                    self.execution.parent / "sdk/share/nexus/src/.nexus-source-sdk.json")["sha256"]}],
                "physical_status": "not_executed", "signature_status": "unsigned"}))

    def file(self, name, contents):
        path = self.base / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents.encode() if isinstance(contents, str) else contents)
        return path

    def package(self, name, files):
        source = self.base / (name + "-input")
        for relative, contents in files.items():
            self.file(str(source.relative_to(self.base) / relative), contents)
        return self.package_tree(name, source)

    def package_tree(self, name, source):
        package = store.export_tree(source, self.base / (name + "-store"))
        return self.file(name + ".package.json", json.dumps(package))

    def bundle(self, **changes):
        return delivery.bundle_local(**{**self.arguments, **changes})

    def verify(self):
        return delivery.verify_local(self.source, self.output, self.commit)

    def test_bundle_and_verify_bind_original_bytes_with_explicit_local_trust(self):
        before = self.execution.read_bytes()
        result = self.bundle()
        self.assertEqual(result["status"], "verified_local_inputs")
        self.assertEqual(result["source"], self.source_identity)
        self.assertEqual(result["qualification"]["stage_count"], 3)
        self.assertFalse(result["publication_authorized"])
        self.assertEqual(result["signature_status"], "unsigned")
        self.assertEqual(result["trust"], "local_observation_no_oidc")
        self.assertNotIn("ci_run_id", result)
        self.assertNotIn("release_run_id", result)
        self.assertIn("codeql", result["unassessed_publication_exits"])
        self.assertEqual(self.execution.read_bytes(), before)
        self.assertTrue(any(item["original"]["path"] == str(self.execution)
                            for item in result["evidence"]))
        receipt = json.loads(self.execution.read_text())["stages"][0]["command_receipt"]
        original_attempt = json.loads(Path(receipt["path"]).read_text())["attempt_receipt"]
        self.assertTrue(any(item["original"]["path"] == original_attempt
                            for item in result["evidence"]))
        verified = self.verify()
        self.assertEqual(verified, result)

    def test_shortened_external_plan_cannot_replace_the_source_owned_plan(self):
        report = json.loads(self.execution.read_text())
        report["plan_sha256"] = canonical_digest({**self.plan, "stages": []})
        self.execution.write_text(json.dumps(report))
        with self.assertRaisesRegex(EvidenceError, "matching formal execution"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_unknown_outer_exit_is_not_replaced_by_passing_children(self):
        report = json.loads(self.execution.read_text())
        receipt = Path(report["stages"][0]["command_receipt"]["path"])
        value = json.loads(receipt.read_text())
        value.update(outcome="running", exit_code=None)
        receipt.write_text(json.dumps(value))
        report["stages"][0]["command_receipt"] = file_identity(receipt)
        self.execution.write_text(json.dumps(report))
        with self.assertRaisesRegex(EvidenceError, "original attempt|completed owned"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_absent_original_attempt_rejects_before_any_payload_copy(self):
        report = json.loads(self.execution.read_text())
        receipt = json.loads(Path(report["stages"][0]["command_receipt"]["path"]).read_text())
        Path(receipt["attempt_receipt"]).unlink()
        with self.assertRaisesRegex(EvidenceError, "missing|empty"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_raw_mutation_rejects_even_when_outer_report_is_rehashed(self):
        self.bundle()
        report = json.loads(self.execution.read_text())
        raw = Path(report["stages"][0]["raw"]["path"])
        raw.write_bytes(b"a newly hashed summary is not original execution\n")
        report["stages"][0]["raw"] = file_identity(raw)
        self.execution.write_text(json.dumps(report))
        with self.assertRaises(ValueError):
            self.verify()

    def test_dirty_or_different_source_cannot_inherit_old_full_execution(self):
        self.bundle()
        (self.source / "dependencies/tool.lock.json").write_text('{"pinned":"changed"}')
        with self.assertRaisesRegex(EvidenceError, "same clean source"):
            self.verify()
        with self.assertRaisesRegex(ValueError, "source|commit"):
            self.bundle(output=self.base / "other", expected_commit="f" * 40)

    def test_documentation_source_and_version_substitution_are_rejected(self):
        for role, value in (("version", "999.0.0"),
                            ("code_source", {**self.source_identity,
                                             "commit": "f" * 40})):
            record = {"documentation_source": self.source_identity,
                "code_source": self.source_identity,
                "version": delivery.load_policy(self.source)["semantic_version"]}
            self.docs_check.return_value = {**record, role: value}
            with self.subTest(role=role), self.assertRaisesRegex(
                    ValueError, "[Dd]ocumentation"):
                self.bundle()
            self.assertFalse(self.output.exists())

    def test_candidate_source_and_duplicate_configuration_are_rejected(self):
        original = self.candidate_check.return_value
        self.candidate_check.return_value = {**original,
            "source": {**self.source_identity, "commit": "f" * 40}}
        with self.assertRaisesRegex(ValueError, "[Cc]andidate source"):
            self.bundle()
        self.candidate_check.return_value = original
        with self.assertRaisesRegex(ValueError, "[Dd]uplicate candidate"):
            self.bundle(candidate_packages=[self.candidate, self.candidate])
        self.assertFalse(self.output.exists())

    def test_same_commit_sdk_cannot_replace_the_original_producer_snapshot(self):
        self.sdk_check.return_value = {"snapshot_sha256": "d" * 64}
        with self.assertRaisesRegex(ValueError, "actual qualified producer"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_candidate_must_use_the_exact_sdk_in_this_preview(self):
        original = self.candidate_check.return_value
        self.candidate_check.return_value = {**original,
            "sdk_packages": [{"manifest_sha256": "e" * 64}]}
        with self.assertRaisesRegex(ValueError, "Candidate SDK differs"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_rehashed_transport_cannot_substitute_another_producer_inventory(self):
        original = copy.deepcopy(self.transport_check.return_value)
        original["files"]["verification.json"]["sha256"] = "e" * 64
        self.transport_check.return_value = original
        with self.assertRaisesRegex(ValueError, "actual qualified producer"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_current_consumer_records_cannot_drop_or_reuse_original_commands(self):
        report = self.execution.parent / "sdk-consumers/verification.json"
        original = report.read_bytes()
        for mutation in ("duplicate", "missing-view", "prefix"):
            record = json.loads(original)
            view = report.parent / "command-00.command.json"
            before = view.read_bytes()
            if mutation == "duplicate":
                record["commands"].append(copy.deepcopy(record["commands"][0]))
            elif mutation == "prefix":
                record["commands"][0]["argv"][-1] = "/different/same-commit-sdk"
            else:
                view.unlink()
            report.write_text(json.dumps(record))
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.bundle()
            self.assertFalse(self.output.exists())
            report.write_bytes(original)
            view.write_bytes(before)

    def test_missing_selected_candidate_is_not_accepted_as_empty_selection(self):
        with self.assertRaisesRegex(ValueError, "candidate"):
            self.bundle(candidate_packages=[])
        self.assertFalse(self.output.exists())

    def test_corrupt_input_archive_never_publishes_a_bundle(self):
        package = json.loads(self.sdk.read_text())
        archive = Path(package["archive"]["path"])
        archive.write_bytes(archive.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "identity|archive"):
            self.bundle()
        self.assertFalse(self.output.exists())

    def test_copied_envelope_and_archive_cannot_be_substituted_after_prepare(self):
        self.bundle()
        result = self.verify()
        archive = self.output / result["artifacts"]["sdk"]["archive"]
        archive.write_bytes(archive.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "closure|bytes|changed"):
            self.verify()

    def test_rehashed_manifest_does_not_authorize_another_selected_input(self):
        self.bundle()
        path = self.output / delivery.LOCAL_MANIFEST
        value = json.loads(path.read_text())
        value["artifacts"]["candidates"][0]["verified"]["configuration_sha256"] = "c" * 64
        value["record_sha256"] = canonical_digest({key: item for key, item in value.items()
                                                   if key != "record_sha256"})
        path.write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, "verified|differ|changed"):
            self.verify()

    def test_rehashed_navigation_cannot_read_outside_the_bundle(self):
        self.bundle()
        path = self.output / delivery.LOCAL_MANIFEST
        original = path.read_bytes()
        for role in ("evidence", "envelope", "archive", "documentation"):
            value = json.loads(original)
            if role == "evidence":
                value["evidence"][0]["payload"] = "../foreign"
            elif role == "documentation":
                value["artifacts"]["documentation"]["archive"] = "../foreign"
            else:
                value["artifacts"]["sdk"][role] = "../foreign"
            value["record_sha256"] = canonical_digest({key: item for key, item in value.items()
                                                       if key != "record_sha256"})
            path.write_text(json.dumps(value))
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, "closure"):
                self.verify()
        path.write_bytes(original)

    def test_payload_modes_extra_members_and_symlinks_are_refused(self):
        self.bundle()
        path = self.output / "extra"
        for mutation in ("extra", "symlink", "mode"):
            with self.subTest(mutation=mutation):
                selected = self.output / self.verify()["evidence"][0]["payload"]
                mode = selected.stat().st_mode & 0o777
                if mutation == "extra":
                    path.write_text("undeclared")
                elif mutation == "symlink":
                    path.symlink_to(selected)
                else:
                    selected.chmod(mode ^ 0o100)
                try:
                    with self.assertRaisesRegex(ValueError, "closure|symlink"):
                        self.verify()
                finally:
                    if mutation == "mode":
                        selected.chmod(mode)
                    else:
                        path.unlink()

    def test_output_must_be_fresh_nonoverlapping_and_nonsymlink(self):
        for output in (self.source, self.execution.parent,
                       self.base / "sdk-store"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.bundle(output=output)
        alias = self.base / "alias"
        alias.symlink_to(self.base, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.bundle(output=alias / "new-bundle")
        self.assertFalse(self.output.exists())

    def test_insufficient_capacity_rejects_before_restoring_or_copying(self):
        usage = shutil.disk_usage(self.base)
        with patch.object(shutil, "disk_usage", return_value=usage._replace(free=1)):
            with self.assertRaisesRegex(ValueError, "capacity"):
                self.bundle()
        self.sdk_check.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_original_envelopes_and_archives_keep_their_exact_bytes(self):
        original = {path: path.read_bytes() for path in
                    (self.sdk, self.consumers, self.candidate, self.docs)}
        result = self.bundle()
        for role in ("sdk", "consumers"):
            record = result["artifacts"][role]
            self.assertEqual((self.output / record["envelope"]).read_bytes(),
                             Path(record["original_envelope"]["path"]).read_bytes())
        for path, contents in original.items():
            self.assertEqual(path.read_bytes(), contents)
        self.assertEqual(git_source(self.source), self.source_identity)

    def test_late_input_mutation_cannot_return_a_verified_bundle(self):
        verified = self.docs_check.return_value
        for role in ("envelope", "documentation"):
            selected = self.sdk if role == "envelope" else self.docs
            before = selected.read_bytes()
            def mutate(_path):
                selected.write_bytes(before + b"\n")
                return verified
            self.docs_check.side_effect = mutate
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, "changed"):
                self.bundle()
            self.assertFalse(self.output.exists())
            selected.write_bytes(before)
        self.docs_check.side_effect = None

    def test_late_packaged_mutation_is_rejected_before_verification_returns(self):
        self.bundle()
        verified = self.docs_check.return_value
        for role in ("manifest", "evidence"):
            record = json.loads((self.output / delivery.LOCAL_MANIFEST).read_text())
            selected = (self.output / delivery.LOCAL_MANIFEST if role == "manifest" else
                        self.output / record["evidence"][0]["payload"])
            before = selected.read_bytes()
            def mutate(_path):
                selected.write_bytes(before + b"\n")
                return verified
            self.docs_check.side_effect = mutate
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, "changed"):
                self.verify()
            selected.write_bytes(before)
        self.docs_check.side_effect = None

    def test_equal_empty_bytes_preserve_distinct_original_file_modes(self):
        result = self.bundle()
        empty = [item for item in result["evidence"] if item["original"]["size"] == 0]
        self.assertGreater(len({Path(item["original"]["path"]).stat().st_mode & 0o777
                                for item in empty}), 1)
        for item in empty:
            with self.subTest(original=item["original"]["path"]):
                self.assertEqual((self.output / item["payload"]).stat().st_mode & 0o777,
                                 Path(item["original"]["path"]).stat().st_mode & 0o777)

    def test_explicit_local_cli_prepares_and_rechecks_without_ci_run_ids(self):
        code = delivery.main(["prepare-local", "--source", str(self.source),
            "--execution", str(self.execution), "--sdk-package", str(self.sdk),
            "--consumer-package", str(self.consumers), "--documentation", str(self.docs),
            "--candidate-package", str(self.candidate), "--output", str(self.output),
            "--expected-commit", self.commit])
        self.assertEqual(code, 0)
        code = delivery.main(["verify-local", "--source", str(self.source),
            "--output", str(self.output), "--expected-commit", self.commit])
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
