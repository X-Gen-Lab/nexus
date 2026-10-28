"""Documentation packages bind actual bytes and reject unsafe restoration."""

import functools
import csv
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import threading
import unittest
from unittest.mock import patch

from tools.documentation import delivery


class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base / "source"
        self.source.mkdir()
        self.build = self.base / "build"
        self.build.mkdir()
        (self.source / "CMakeLists.txt").write_text(
            "project(nexus VERSION 1.0.0 LANGUAGES C ASM)\n")
        policy = self.source / "dependencies/platform-compatibility.json"
        policy.parent.mkdir()
        policy_data = json.loads((Path(__file__).resolve().parents[2] /
                                 "dependencies/platform-compatibility.json").read_bytes())
        policy_data["semantic_version"] = "1.0.0"
        policy.write_text(json.dumps(policy_data))
        index = self.source / "docs/delivery/build-system-results.json"
        index.parent.mkdir(parents=True)
        index.write_text(json.dumps({"schema_version": 1, "source": {
            "canonical_code_commit": "a" * 40, "code_tree": "b" * 40},
            "implementation": {"physical_status": "not_executed"}}))
        self.git("init", "-q")
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.com",
                 "commit", "-qm", "Initial fixture")
        for relative in ("index.html", "en/index.html", "zh_CN/index.html",
                         "api/index.html"):
            path = self.build / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('<html><h1 id="overview">Nexus</h1></html>')
        license_path = self.build / "licenses/LICENSE.txt"
        license_path.parent.mkdir()
        license_path.write_text("MIT fixture\n")
        metadata = {"schema_version": 1, "kind": "nexus_documentation_build",
                    "status": "passed",
                    "source": delivery.observe_git_source(self.source),
                    "tools": {"sphinx": "fixture-test"},
                    "languages": ["en", "zh_CN"]}
        (self.build / "documentation-build.json").write_text(
            json.dumps(metadata))

    def git(self, *arguments):
        return subprocess.run(["git", "-C", str(self.source), *arguments],
                              capture_output=True, check=True).stdout

    def package(self):
        return delivery.package(self.build, self.base / "output",
                                source_root=self.source,
                                release_id="1.0.0-docs-test")

    def test_package_has_no_implicit_historical_qualification(self):
        result = self.package()
        manifest = delivery.verify(result["directory"])
        self.assertEqual(manifest["schema_version"], 2)
        self.assertEqual(manifest["qualification_references"], [])
        self.assertNotIn("qualified_code", manifest)
        self.assertEqual(manifest["physical_status"], "not_executed")
        paths = [record["path"] for record in manifest["files"]]
        self.assertNotIn(delivery.MANIFEST, paths)
        self.assertNotIn(delivery.CHECKSUMS, paths)
        self.assertIn("api/index.html", paths)
        self.assertTrue(Path(result["archive"]).is_file())

    def test_explicit_candidate_references_preserve_separate_scope_and_source(self):
        from tools.evidence import candidate
        first = self.base / "first-candidate.json"
        second = self.base / "second-candidate.json"
        projections = [self.candidate_projection(first, "a"),
                       self.candidate_projection(second, "b")]
        projections[0]["candidate_id"] = "fixture:qualified-a"
        with patch.object(candidate, "verified_projection",
                          side_effect=projections) as verifier:
            result = delivery.package(self.build, self.base / "output",
                source_root=self.source, release_id="1.0.0-docs-test",
                qualification_references=(first, second))
        self.assertEqual(verifier.call_count, 2)
        manifest = delivery.verify(result["directory"])
        references = manifest["qualification_references"]
        self.assertEqual(len(references), 2)
        self.assertEqual(references[0]["candidate_id"], "fixture:qualified-a")
        self.assertEqual([item["source"]["commit"] for item in references],
                         ["a" * 40, "b" * 40])
        self.assertNotEqual(manifest["documentation_source"]["commit"],
                            references[0]["source"]["commit"])
        self.assertEqual(references[0]["configuration_sha256"], "a" * 64)
        self.assertEqual(references[0]["physical_status"], "not_executed")
        self.assertEqual(references[0]["qualified_scopes"],
                         sorted(candidate.REQUIRED_SCOPES))
        self.assertNotIn("resolved", references[0])
        self.assertEqual(references[0]["verification"]["offline"],
                         "reference_metadata_only")
        self.assertFalse(references[0]["verification"]["original_evidence_included"])

    def candidate_projection(self, path, letter):
        from tools.evidence import candidate
        return {"candidate": {"path": str(path), "sha256": letter * 64,
                               "size": 123},
                "candidate_id": "fixture-" + letter,
                "source": {"commit": letter * 40, "tree": letter * 40,
                           "dirty": False, "submodules": []},
                "configuration_sha256": letter * 64,
                "artifacts": {role: {"sha256": letter * 64, "size": 12}
                              for role in ("elf", "bin", "map")},
                "qualified_scopes": sorted(candidate.REQUIRED_SCOPES),
                "physical_status": "not_executed",
                "product_promotion": "not_performed",
                "signature_status": "unsigned", "resolved": {}}

    def test_unverified_or_duplicate_candidate_is_rejected_before_output(self):
        from tools.evidence import candidate
        first = self.base / "first-candidate.json"
        with patch.object(candidate, "verified_projection",
                          side_effect=ValueError("missing original evidence")):
            with self.assertRaisesRegex(ValueError, "missing original"):
                delivery.package(self.build, self.base / "output",
                    source_root=self.source, release_id="1.0.0-docs-test",
                    qualification_references=(first,))
        self.assertFalse((self.base / "output").exists())

        projection = self.candidate_projection(first, "a")
        with patch.object(candidate, "verified_projection", return_value=projection):
            with self.assertRaisesRegex(ValueError, "duplicate"):
                delivery.package(self.build, self.base / "output",
                    source_root=self.source, release_id="1.0.0-docs-test",
                    qualification_references=(first, first))
        self.assertFalse((self.base / "output").exists())

    def test_rehashed_reference_cannot_claim_new_scope_or_physical_acceptance(self):
        projection = self.candidate_projection(self.base / "candidate.json", "a")
        projection.pop("resolved")
        projection["verification"] = dict(delivery.REFERENCE_VERIFICATION)
        for key, value in (("physical_status", "passed"),
                           ("qualified_scopes", ["all_platforms"]),
                           ("configuration_sha256", "")):
            with self.subTest(key=key):
                bad = json.loads(json.dumps(projection))
                bad[key] = value
                with self.assertRaisesRegex(ValueError, "scope|boundary"):
                    delivery._qualification_references([bad])

    def test_historical_schema_one_remains_verifiable_without_new_legacy_writer(self):
        result = self.package()
        directory = Path(result["directory"])
        manifest_path = directory / delivery.MANIFEST
        manifest = json.loads(manifest_path.read_bytes())
        manifest["schema_version"] = 1
        manifest.pop("qualification_references")
        manifest["qualified_code"] = {
            "commit": "a" * 40, "tree": "b" * 40,
            "index": "docs/delivery/build-system-results.json",
            "index_sha256": "c" * 64}
        manifest_path.write_bytes(delivery._json_bytes(manifest))
        payload = delivery._payload(directory)
        (directory / delivery.CHECKSUMS).write_bytes(delivery._checksums(payload))
        restored = delivery.verify(directory)
        self.assertEqual(restored["schema_version"], 1)
        self.assertEqual(restored["qualified_code"]["commit"], "a" * 40)

    def test_source_interface_version_and_epochs_use_checked_policy(self):
        result = self.package()
        manifest = delivery.verify(result["directory"])
        policy = json.loads((self.source /
                             "dependencies/platform-compatibility.json").read_bytes())
        self.assertEqual(manifest["source_interface"]["semantic_version"],
                         policy["semantic_version"])
        self.assertEqual(manifest["source_interface"]["factory_abi_epoch"],
                         policy["factory_abi_epoch"])
        self.assertEqual(manifest["source_interface"]["schemas"], policy["schemas"])

    def test_duplicate_metadata_keys_and_invalid_policy_version_reject(self):
        metadata_path = self.build / "documentation-build.json"
        original = metadata_path.read_text()
        metadata_path.write_text(original.replace('"schema_version": 1',
            '"schema_version": 1, "schema_version": 1'))
        with self.assertRaisesRegex(ValueError, "Duplicate|duplicate"):
            self.package()
        policy_path = self.source / "dependencies/platform-compatibility.json"
        policy = json.loads(policy_path.read_bytes())
        policy["semantic_version"] = "not-a-version"
        policy_path.write_text(json.dumps(policy))
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.com",
                 "commit", "-qm", "Change policy")
        metadata = json.loads(original)
        metadata["source"] = delivery.observe_git_source(self.source)
        metadata_path.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, "version|policy"):
            self.package()

    def test_dirty_and_stale_build_sources_cannot_package(self):
        (self.source / "untracked.md").write_text("Uncommitted source")
        with self.assertRaisesRegex(ValueError, "clean|dirty"):
            self.package()
        (self.source / "untracked.md").unlink()
        (self.source / "CMakeLists.txt").write_text(
            "project(nexus VERSION 1.0.1 LANGUAGES C ASM)\n")
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.com",
                 "commit", "-qm", "Change fixture")
        with self.assertRaisesRegex(ValueError, "stale|source"):
            self.package()

    def test_failed_running_or_stale_success_metadata_cannot_package(self):
        path = self.build / "documentation-build.json"
        original = json.loads(path.read_bytes())
        for status in ("failed", "running", None):
            metadata = json.loads(json.dumps(original))
            if status is None:
                del metadata["status"]
            else:
                metadata["status"] = status
            path.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, "passed|status|successful"):
                self.package()
        stale = json.loads(json.dumps(original))
        stale["source"]["commit"] = "0" * 40
        path.write_text(json.dumps(stale))
        with self.assertRaisesRegex(ValueError, "stale|source"):
            self.package()

    def test_rehashed_failed_build_metadata_cannot_verify(self):
        result = self.package()
        directory = Path(result["directory"])
        metadata_path = directory / "documentation-build.json"
        metadata = json.loads(metadata_path.read_bytes())
        metadata["status"] = "failed"
        metadata_path.write_text(json.dumps(metadata))
        manifest_path = directory / delivery.MANIFEST
        manifest = json.loads(manifest_path.read_bytes())
        record = next(row for row in manifest["files"]
                      if row["path"] == "documentation-build.json")
        record["bytes"] = metadata_path.stat().st_size
        record["sha256"] = hashlib.sha256(metadata_path.read_bytes()).hexdigest()
        manifest_path.write_text(json.dumps(manifest))
        checksums = []
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.name != delivery.CHECKSUMS:
                checksums.append(hashlib.sha256(path.read_bytes()).hexdigest() +
                                 "  " + path.relative_to(directory).as_posix())
        (directory / delivery.CHECKSUMS).write_text("\n".join(checksums) + "\n")
        with self.assertRaisesRegex(ValueError, "passed|status|successful"):
            delivery.verify(directory)

    def test_missing_language_api_or_license_rejects_incomplete_payload(self):
        for relative in ("en/index.html", "zh_CN/index.html", "api/index.html",
                         "licenses/LICENSE.txt"):
            path = self.build / relative
            original = path.read_bytes()
            path.unlink()
            with self.assertRaisesRegex(ValueError, "required|incomplete"):
                self.package()
            path.write_bytes(original)

    def test_required_entries_cannot_be_empty_or_only_whitespace(self):
        for relative in ("en/index.html", "zh_CN/index.html", "api/index.html",
                         "licenses/LICENSE.txt"):
            path = self.build / relative
            original = path.read_bytes()
            for contents in (b"", b" \n\t"):
                path.write_bytes(contents)
                with self.assertRaisesRegex(ValueError, "empty|nonempty|incomplete"):
                    self.package()
            path.write_bytes(original)

    def test_source_and_payload_symlinks_are_rejected(self):
        link = self.build / "escape.txt"
        link.symlink_to(self.source / "CMakeLists.txt")
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.package()
        link.unlink()
        linked_build = self.base / "linked-build"
        linked_build.symlink_to(self.build, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            delivery.package(linked_build, self.base / "output",
                             source_root=self.source, release_id="docs-test")

    def test_package_output_cannot_modify_source_inputs(self):
        source = delivery.observe_git_source(self.source)
        destination = self.source / "docs/packages"
        with self.assertRaisesRegex(ValueError, "source|overlap|input"):
            delivery.package(self.build, destination, source_root=self.source,
                             release_id="docs-fault")
        self.assertEqual(delivery.observe_git_source(self.source), source)
        self.assertFalse(destination.exists())

    def test_publish_webroot_cannot_overlap_input_package(self):
        result = self.package()
        directory = Path(result["directory"])
        with self.assertRaisesRegex(ValueError, "source|overlap|input|payload"):
            delivery.publish(directory, directory / "webroot")
        delivery.verify(directory)
        self.assertFalse((directory / "webroot").exists())

    def test_directory_verification_rejects_changed_unlisted_and_missing_files(self):
        result = self.package()
        directory = Path(result["directory"])
        page = directory / "en/index.html"
        original = page.read_bytes()
        page.write_text("Tampered")
        with self.assertRaisesRegex(ValueError, "digest|bytes|hash"):
            delivery.verify(directory)
        page.write_bytes(original)
        extra = directory / "unlisted.txt"
        extra.write_text("Unlisted")
        with self.assertRaisesRegex(ValueError, "unlisted|payload|closure"):
            delivery.verify(directory)
        extra.unlink()
        page.unlink()
        with self.assertRaisesRegex(ValueError, "missing|payload|closure"):
            delivery.verify(directory)

    def test_manifest_digest_is_an_external_trust_anchor(self):
        result = self.package()
        delivery.verify(result["directory"],
                        expected_manifest_sha256=result["manifest_sha256"])
        with self.assertRaisesRegex(ValueError, "manifest.*digest|manifest.*hash"):
            delivery.verify(result["directory"], expected_manifest_sha256="0" * 64)

    def test_rehashed_malformed_identity_and_self_reference_are_rejected(self):
        result = self.package()
        directory = Path(result["directory"])
        original = json.loads((directory / delivery.MANIFEST).read_bytes())
        for mutation in ("missing_source", "physical_claim", "self_reference"):
            manifest = json.loads(json.dumps(original))
            if mutation == "missing_source":
                del manifest["documentation_source"]
            elif mutation == "physical_claim":
                manifest["physical_status"] = "passed"
            else:
                manifest["files"].append({"path": delivery.MANIFEST,
                    "bytes": 0, "sha256": "0" * 64})
            (directory / delivery.MANIFEST).write_text(json.dumps(manifest))
            checksums = []
            for path in sorted(directory.rglob("*")):
                if path.is_file() and path.name != delivery.CHECKSUMS:
                    checksums.append(hashlib.sha256(path.read_bytes()).hexdigest() +
                                     "  " + path.relative_to(directory).as_posix())
            (directory / delivery.CHECKSUMS).write_text("\n".join(checksums) + "\n")
            with self.assertRaisesRegex(ValueError,
                                        "identity|physical|self-reference"):
                delivery.verify(directory)

    def test_offline_links_reject_escaping_absolute_and_missing_targets(self):
        for href in ("../../../escape.html", "/en/index.html", "missing.html",
                     "javascript:alert(1)", "index.html#missing"):
            (self.build / "en/index.html").write_text(
                f'<html><a href="{href}">invalid</a></html>')
            with self.assertRaisesRegex(ValueError, "link|fragment|relative|escape"):
                self.package()

    def test_offline_assets_reject_external_dependencies(self):
        (self.build / "en/index.html").write_text(
            '<script src="https://cdn.example.invalid/code.js"></script>')
        with self.assertRaisesRegex(ValueError, "external|offline"):
            self.package()

    def test_offline_stylesheet_nested_dependencies_must_be_local(self):
        styles = self.build / "_static/style.css"
        styles.parent.mkdir()
        for contents in ('@import "https://cdn.example.invalid/font.css";',
                         'body{background:url(https://cdn.example.invalid/a.png)}',
                         'body{background:url(missing.png)}'):
            styles.write_text(contents)
            with self.assertRaisesRegex(ValueError, "external|offline|link"):
                self.package()
        (styles.parent / "local.png").write_bytes(b"Fixture image")
        styles.write_text('body{background:url("local.png")}')
        delivery.verify(self.package()["directory"])

    def test_css_escaped_resource_identifier_cannot_hide_external_dependency(self):
        styles = self.build / "_static/style.css"
        styles.parent.mkdir()
        styles.write_text(r"body{background:\75rl(https://example.invalid/image.png)}")
        with self.assertRaisesRegex(ValueError, "external|offline|escape|CSS"):
            self.package()

    def test_offline_svg_rejects_active_and_external_dependencies(self):
        image = self.build / "_static/diagram.svg"
        image.parent.mkdir()
        for contents in ('<svg xmlns="http://www.w3.org/2000/svg">'
                         '<script>alert(1)</script></svg>',
                         '<svg xmlns="http://www.w3.org/2000/svg">'
                         '<image href="https://example.invalid/image.png"/></svg>'):
            image.write_text(contents)
            with self.assertRaisesRegex(ValueError, "SVG|external|offline"):
                self.package()

    def test_valid_relative_links_and_fragments_are_preserved(self):
        (self.build / "en/index.html").write_text(
            '<a href="../api/index.html#overview">API</a>')
        manifest = delivery.verify(self.package()["directory"])
        self.assertIn("api/index.html", [row["path"] for row in manifest["files"]])

    def test_restore_is_independent_and_archive_bytes_are_reproducible(self):
        first = self.package()
        second = delivery.package(self.build, self.base / "other-output",
                                  source_root=self.source,
                                  release_id="1.0.0-docs-test")
        self.assertEqual(Path(first["archive"]).read_bytes(),
                         Path(second["archive"]).read_bytes())
        restored = self.base / "independent-root"
        manifest = delivery.restore(first["archive"], restored,
                                    expected_manifest_sha256=first["manifest_sha256"])
        self.assertEqual(manifest, delivery.verify(restored))
        self.assertEqual((restored / "api/index.html").read_bytes(),
                         (self.build / "api/index.html").read_bytes())

    def test_archive_traversal_links_and_duplicate_members_are_rejected(self):
        for name, kind in (("../escape.txt", tarfile.REGTYPE),
                           ("link.txt", tarfile.SYMTYPE),
                           ("hard.txt", tarfile.LNKTYPE)):
            archive = self.base / "bad.tar.gz"
            with tarfile.open(archive, "w:gz") as output:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.linkname = "../outside"
                output.addfile(member)
            with self.assertRaisesRegex(ValueError, "escape|path|link|regular"):
                delivery.restore(archive, self.base / "bad-restored")
        archive = self.base / "duplicate.tar.gz"
        with tarfile.open(archive, "w:gz") as output:
            for _ in range(2):
                member = tarfile.TarInfo("same.txt")
                member.size = 1
                output.addfile(member, io.BytesIO(b"x"))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            delivery.restore(archive, self.base / "duplicate-restored")

    def test_failed_restore_leaves_no_partial_destination(self):
        archive = self.base / "invalid.tar.gz"
        archive.write_bytes(b"Not an archive")
        destination = self.base / "restored"
        with self.assertRaises(ValueError):
            delivery.restore(archive, destination)
        self.assertFalse(destination.exists())

    def test_existing_restore_destination_is_never_replaced(self):
        result = self.package()
        destination = self.base / "restore-existing"
        destination.mkdir()
        (destination / "keep.txt").write_text("User data")
        with self.assertRaisesRegex(ValueError, "exists|existing"):
            delivery.restore(result["archive"], destination)
        self.assertEqual((destination / "keep.txt").read_text(), "User data")

    def test_publish_is_immutable_and_stable_links_are_relative(self):
        result = self.package()
        webroot = self.base / "webroot"
        receipt = delivery.publish(result["directory"], webroot)
        self.assertTrue((webroot / "versions/1.0.0-docs-test/api/index.html").exists())
        self.assertIn("../versions/1.0.0-docs-test/", (webroot /
                      "stable/index.html").read_text())
        self.assertEqual(receipt["manifest_sha256"], result["manifest_sha256"])
        delivery.publish(result["directory"], webroot)
        existing = webroot / "versions/1.0.0-docs-test/en/index.html"
        existing.write_text("Changed publication")
        with self.assertRaisesRegex(ValueError, "digest|bytes|immutable|hash"):
            delivery.publish(result["directory"], webroot)

    def test_publish_download_is_checked_and_same_id_cannot_change_archive(self):
        result = self.package()
        webroot = self.base / "webroot"
        receipt = delivery.publish(result["directory"], webroot,
                                   archive=result["archive"])
        download = webroot / "downloads/1.0.0-docs-test.tar.gz"
        self.assertEqual(download.read_bytes(), Path(result["archive"]).read_bytes())
        self.assertEqual(receipt["archive_sha256"], result["archive_sha256"])
        download.write_bytes(b"Changed archive")
        with self.assertRaisesRegex(ValueError, "immutable|archive|bytes"):
            delivery.publish(result["directory"], webroot,
                             archive=result["archive"])

    def test_invalid_release_identity_cannot_escape_publication(self):
        for release_id in ("../outside", "/absolute", "docs test", "."):
            with self.assertRaisesRegex(ValueError, "release"):
                delivery.package(self.build, self.base / "bad-output",
                                 source_root=self.source, release_id=release_id)

    def test_post_deploy_fetches_all_bytes_and_detects_live_tampering(self):
        result = self.package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot, archive=result["archive"])
        handler = functools.partial(QuietHandler, directory=str(webroot))
        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        base_url = f"http://127.0.0.1:{server.server_port}"
        receipt = delivery.post_deploy(base_url, "1.0.0-docs-test",
                                      result["manifest_sha256"],
                                      receipt_path=self.base / "post-deploy.json",
                                      expected_archive_sha256=result["archive_sha256"])
        self.assertEqual(receipt["status"], "verified")
        self.assertGreater(receipt["verified_files"], 4)
        self.assertEqual(receipt["archive_sha256"], result["archive_sha256"])
        self.assertTrue(receipt["stable_verified"])
        page = webroot / "versions/1.0.0-docs-test/en/index.html"
        page.write_text("Live tampering")
        with self.assertRaisesRegex(ValueError, "digest|bytes|hash"):
            delivery.post_deploy(base_url, "1.0.0-docs-test",
                                 result["manifest_sha256"],
                                 receipt_path=self.base / "post-deploy.json")
        self.assertNotEqual(json.loads((self.base / "post-deploy.json").read_bytes())
                            ["status"], "verified")
        page.write_bytes((self.build / "en/index.html").read_bytes())
        stable = webroot / "stable/index.html"
        stable.write_text('<a href="/wrong-version/en/index.html">Wrong</a>')
        with self.assertRaisesRegex(ValueError, "stable|relative"):
            delivery.post_deploy(base_url, "1.0.0-docs-test",
                                 result["manifest_sha256"])
        delivery.publish(result["directory"], webroot)
        (webroot / "downloads/1.0.0-docs-test.tar.gz").write_bytes(b"Broken")
        with self.assertRaisesRegex(ValueError, "archive|digest|bytes"):
            delivery.post_deploy(base_url, "1.0.0-docs-test",
                                 result["manifest_sha256"],
                                 expected_archive_sha256=result["archive_sha256"])

    def alias_inputs(self):
        rows = [
            {"old_url": "en/user_guide/uart.html",
             "target_url": "en/reference/history.html",
             "semantics": "historical_contract_version_boundary",
             "old_source": "docs/archive/uart.rst", "source_revision": "a" * 40},
            {"old_url": "zh_CN/user_guide/uart.html",
             "target_url": "zh_CN/reference/history.html",
             "semantics": "historical_contract_version_boundary",
             "old_source": "docs/archive/uart.rst", "source_revision": "a" * 40},
            {"old_url": "en/getting_started.html",
             "target_url": "en/tutorials/source-sdk.html",
             "semantics": "current_contract_reorganized",
             "old_source": "docs/sphinx/getting_started.rst",
             "source_revision": "a" * 40}]
        for relative in ("en/reference/history.html", "zh_CN/reference/history.html",
                         "en/tutorials/source-sdk.html"):
            file = self.build / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('<html><h1 id="overview">Canonical fixture</h1></html>')
        for row in rows:
            source = self.source / row["old_source"]
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("Frozen documentation fixture\n")
        csv_path = self.source / "docs/reference/redirects.csv"
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        with csv_path.open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.com",
                 "commit", "-qm", "Declare fixture aliases")
        metadata_path = self.build / "documentation-build.json"
        metadata = json.loads(metadata_path.read_bytes())
        metadata["source"] = delivery.observe_git_source(self.source)
        metadata_path.write_text(json.dumps(metadata))
        return rows

    def alias_package(self):
        self.alias_inputs()
        delivery.install_alias_catalog(self.build, self.source)
        return self.package()

    def test_alias_catalog_is_bound_and_current_alias_does_not_claim_api_change(self):
        result = self.alias_package()
        manifest = delivery.verify(result["directory"])
        self.assertIn("documentation-aliases.json", [row["path"] for row in manifest["files"]])
        page = Path(result["directory"]) / "en/getting_started.html"
        self.assertIn("same current platform contract", page.read_text())
        self.assertNotIn("different API contract", page.read_text())

    def test_publish_restores_root_bookmarks_and_api_boundary(self):
        result = self.alias_package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot)
        self.assertIn("../../versions/1.0.0-docs-test/en/reference/history.html",
                      (webroot / "en/user_guide/uart.html").read_text())
        self.assertIn("same current platform contract",
                      (webroot / "en/getting_started.html").read_text())
        self.assertIn("../versions/1.0.0-docs-test/en/index.html",
                      (webroot / "en/index.html").read_text())
        api = (webroot / "api/index.html").read_text()
        self.assertIn("historical API contract", api)
        self.assertIn("en/reference/history.html", api)
        delivery.publish(result["directory"], webroot)

    def test_malformed_catalog_rejects_before_package_and_publish(self):
        rows = self.alias_inputs()
        valid = {"schema_version": 1, "kind": "nexus_documentation_aliases",
                 "source": {"path": "docs/reference/redirects.csv", "sha256": "a" * 64},
                 "aliases": rows, "canonical_collisions": []}
        variants = []
        unknown = json.loads(json.dumps(valid))
        unknown["extra"] = True
        variants.append(unknown)
        for old_url in ("../escape.html", "versions/overwrite.html", "api/index.html",
                        "en/index.html", "downloads/overwrite.html",
                        "documentation-gateways.json/alias.html", "index.html/alias.html"):
            item = json.loads(json.dumps(valid))
            item["aliases"][0]["old_url"] = old_url
            variants.append(item)
        duplicate = json.loads(json.dumps(valid))
        duplicate["aliases"].append(duplicate["aliases"][0])
        variants.append(duplicate)
        for field, value in (("semantics", "guess_contract"),
                             ("target_url", "en/index.html"),
                             ("target_url", "absent.html")):
            item = json.loads(json.dumps(valid))
            item["aliases"][0][field] = value
            variants.append(item)
        for catalog in variants:
            (self.build / "documentation-aliases.json").write_text(json.dumps(catalog))
            with self.assertRaisesRegex(ValueError, "alias|gateway|reserved|escape|fields"):
                self.package()
        self.assertFalse((self.base / "output").exists())

    def test_catalog_install_rejects_existing_unowned_payload_collision(self):
        self.alias_inputs()
        collision = self.build / "en/getting_started.html"
        collision.write_text("Existing input")
        with self.assertRaisesRegex(ValueError, "alias|collision|existing"):
            delivery.install_alias_catalog(self.build, self.source)
        self.assertEqual(collision.read_text(), "Existing input")
        self.assertFalse((self.build / "documentation-aliases.json").exists())

    def test_alias_directory_and_parent_collisions_reject_before_first_write(self):
        rows = self.alias_inputs()
        conflict = self.build / rows[1]["old_url"]
        conflict.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "alias|collision"):
            delivery.install_alias_catalog(self.build, self.source)
        self.assertFalse((self.build / rows[0]["old_url"]).exists())
        self.assertFalse((self.build / "documentation-aliases.json").exists())
        conflict.rmdir()
        parent = self.build / "en/user_guide"
        parent.write_text("Parent is an existing file")
        with self.assertRaisesRegex(ValueError, "alias|collision"):
            delivery.install_alias_catalog(self.build, self.source)
        self.assertEqual(parent.read_text(), "Parent is an existing file")

    def test_catalog_directory_collision_rejects_before_first_alias_write(self):
        rows = self.alias_inputs()
        (self.build / "documentation-aliases.json").mkdir()
        with self.assertRaisesRegex(ValueError, "alias|catalog|collision"):
            delivery.install_alias_catalog(self.build, self.source)
        self.assertFalse((self.build / rows[0]["old_url"]).exists())

    def test_control_file_descendants_reject_before_catalog_install(self):
        rows = self.alias_inputs()
        rows[0]["old_url"] = "documentation-gateways.json/alias.html"
        csv_path = self.source / "docs/reference/redirects.csv"
        with csv_path.open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        with self.assertRaisesRegex(ValueError, "alias|reserved|namespace"):
            delivery.install_alias_catalog(self.build, self.source)
        self.assertFalse((self.build / "documentation-gateways.json").exists())

    def test_canonical_collision_preserves_version_page_and_binds_source_and_html(self):
        self.alias_inputs()
        original = self.source / "docs/getting_started.md"
        original.write_text("---\nsource_language: en\n---\n# Current page\n")
        page = self.build / "en/getting_started.html"
        page.write_text("<html><h1>Canonical current page</h1></html>")
        before = page.read_bytes()
        delivery.install_alias_catalog(self.build, self.source)
        catalog = json.loads((self.build / "documentation-aliases.json").read_bytes())
        self.assertEqual(page.read_bytes(), before)
        record = catalog["canonical_collisions"][0]
        self.assertEqual(record["source_path"], "docs/getting_started.md")
        self.assertEqual(record["source_sha256"], hashlib.sha256(original.read_bytes()).hexdigest())
        self.assertEqual(record["html_sha256"], hashlib.sha256(before).hexdigest())
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.com",
                 "commit", "-qm", "Record current canonical fixture")
        metadata_path = self.build / "documentation-build.json"
        metadata = json.loads(metadata_path.read_bytes())
        metadata["source"] = delivery.observe_git_source(self.source)
        metadata_path.write_text(json.dumps(metadata))
        result = self.package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot)
        self.assertIn("same current platform contract", (webroot / "en/getting_started.html").read_text())

    def test_existing_gateway_owner_is_checked_for_new_no_catalog_package(self):
        result = self.alias_package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot)
        stable = webroot / "stable/index.html"
        stable.write_text("External edit")
        catalog = json.loads((self.build / "documentation-aliases.json").read_bytes())
        for row in catalog["aliases"]:
            (self.build / row["old_url"]).unlink()
        (self.build / "documentation-aliases.json").unlink()
        bare = delivery.package(self.build, self.base / "bare-output",
                                source_root=self.source, release_id="new-docs")
        with self.assertRaisesRegex(ValueError, "gateway|owned|digest"):
            delivery.publish(bare["directory"], webroot)
        self.assertEqual(stable.read_text(), "External edit")
        self.assertFalse((webroot / "versions/new-docs").exists())

    def test_dangling_owner_symlink_rejects_no_catalog_publish_before_writes(self):
        result = self.package()
        webroot = self.base / "webroot"
        webroot.mkdir()
        marker = webroot / "documentation-gateways.json"
        marker.symlink_to(webroot / "missing-owner.json")
        with self.assertRaisesRegex(ValueError, "symlink"):
            delivery.publish(result["directory"], webroot)
        self.assertTrue(marker.is_symlink())
        self.assertEqual(list(webroot.iterdir()), [marker])

    def test_root_gateway_collision_and_symlink_reject_before_any_publication(self):
        result = self.alias_package()
        for symbolic in (False, True):
            webroot = self.base / ("symbolic-root" if symbolic else "collision-root")
            page = webroot / "en/getting_started.html"
            page.parent.mkdir(parents=True)
            if symbolic:
                page.symlink_to(self.source / "CMakeLists.txt")
            else:
                page.write_text("Unowned root document")
            with self.assertRaisesRegex(ValueError, "unowned|collision|symlink|gateway"):
                delivery.publish(result["directory"], webroot)
            self.assertFalse((webroot / "versions").exists())
            if not symbolic:
                self.assertEqual(page.read_text(), "Unowned root document")

    def test_modified_owned_gateway_cannot_be_silently_replaced(self):
        result = self.alias_package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot)
        page = webroot / "en/getting_started.html"
        page.write_text("External alteration")
        with self.assertRaisesRegex(ValueError, "gateway|owned|digest|bytes"):
            delivery.publish(result["directory"], webroot)
        self.assertEqual(page.read_text(), "External alteration")

    def test_post_deploy_checks_every_root_alias_and_api_boundary(self):
        result = self.alias_package()
        webroot = self.base / "webroot"
        delivery.publish(result["directory"], webroot, archive=result["archive"])
        server = ThreadingHTTPServer(("127.0.0.1", 0),
            functools.partial(QuietHandler, directory=str(webroot)))
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        url = f"http://127.0.0.1:{server.server_port}"
        receipt = delivery.post_deploy(url, "1.0.0-docs-test", result["manifest_sha256"],
                                       expected_archive_sha256=result["archive_sha256"])
        self.assertEqual(receipt["verified_root_gateways"], 6)
        (webroot / "en/user_guide/uart.html").write_text("Changed root alias")
        with self.assertRaisesRegex(ValueError, "alias|gateway|digest|bytes"):
            delivery.post_deploy(url, "1.0.0-docs-test", result["manifest_sha256"])


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *arguments):
        pass


if __name__ == "__main__":
    unittest.main()
