"""Trusted deployment verifies public bytes and the active version anchor."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

from tools.documentation import delivery
from tools.documentation import test_delivery as fixture
from tools.documentation import verify_publication as publication

ROOT = Path(__file__).resolve().parents[2]


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixture.DeliveryTests(
            "test_package_records_distinct_source_and_qualification")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.base
        self.result = self.fixture.package()
        self.public = self.root / "public"
        delivery.publish(self.result["directory"], self.public,
                         archive=self.result["archive"])
        self.record = publication.create_record(self.public, self.result)

    def test_record_covers_all_payload_download_and_entry_files(self):
        actual = {path.relative_to(self.public).as_posix()
                  for path in self.public.rglob("*") if path.is_file()}
        recorded = {entry["path"] for entry in self.record["files"]}
        self.assertEqual(recorded, actual)
        self.assertIn("index.html", actual)
        self.assertIn("stable/index.html", actual)
        self.assertIn("downloads/index.html", actual)
        self.assertIn(self.record["archive_path"], actual)
        result = publication.verify(self.public, self.record)
        self.assertEqual(result["status"], "verified")
        self.assertEqual(result["documentation_commit"],
                         self.record["documentation_commit"])

    def test_changed_public_payload_or_root_page_is_rejected(self):
        for relative in ("index.html", "stable/index.html",
                         "versions/1.0.0-docs-test/en/index.html"):
            path = self.public / relative
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            with self.subTest(path=relative), self.assertRaises(ValueError):
                publication.verify(self.public, self.record)
            path.write_bytes(original)

    def test_unrecorded_and_missing_public_files_are_rejected(self):
        extra = self.public / "extra.txt"
        extra.write_text("unrecorded")
        with self.assertRaisesRegex(ValueError, "inventory"):
            publication.verify(self.public, self.record)
        extra.unlink()
        (self.public / "downloads/index.html").unlink()
        with self.assertRaises(ValueError):
            publication.verify(self.public, self.record)

    def test_symlink_files_directories_and_root_are_rejected(self):
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "file").write_text("outside")
        for name, target in (("linked-file", outside / "file"),
                             ("linked-directory", outside)):
            link = self.public / name
            link.symlink_to(target)
            with self.subTest(name=name), self.assertRaisesRegex(
                    ValueError, "link"):
                publication.verify(self.public, self.record)
            link.unlink()
        root_link = self.root / "public-link"
        root_link.symlink_to(self.public, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "link"):
            publication.verify(root_link, self.record)

    def test_archive_and_inventory_path_escape_are_rejected(self):
        for field, value in (("archive_path", "../outside.tar.gz"),
                             ("archive_path", "/tmp/absolute.tar.gz")):
            broken = copy.deepcopy(self.record)
            broken[field] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                publication.verify(self.public, broken)
        broken = copy.deepcopy(self.record)
        broken["files"][0]["path"] = "../outside"
        with self.assertRaises(ValueError):
            publication.verify(self.public, broken)

    def test_wrong_source_or_manifest_anchor_is_rejected(self):
        for field, value in (("documentation_commit", "e" * 40),
                             ("manifest_sha256", "f" * 64),
                             ("archive_sha256", "d" * 64)):
            broken = copy.deepcopy(self.record)
            broken[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                publication.verify(self.public, broken)

    def test_duplicate_inventory_and_invalid_size_are_rejected(self):
        for changed in ("duplicate", "size", "digest"):
            broken = copy.deepcopy(self.record)
            if changed == "duplicate":
                broken["files"].append(broken["files"][0])
            elif changed == "size":
                broken["files"][0]["size"] = True
            else:
                broken["files"][0]["sha256"] = "placeholder"
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                publication.verify(self.public, broken)

    def test_reinventoried_old_stable_is_rejected(self):
        older = delivery.package(self.fixture.build, self.root / "old-output",
                                 source_root=self.fixture.source,
                                 release_id="1.0.0-docs-old")
        delivery.publish(older["directory"], self.public,
                         archive=older["archive"])
        with self.assertRaisesRegex(ValueError, "stable|active"):
            publication.create_record(self.public, self.result)

    def test_reinventoried_stable_missing_fragment_is_rejected(self):
        path = self.public / "stable/index.html"
        path.write_text(path.read_text().replace(
            'en/index.html"', 'en/index.html#missing-contract"'))
        with self.assertRaisesRegex(ValueError, "fragment|anchor"):
            publication.create_record(self.public, self.result)

    def test_reinventoried_root_external_script_is_rejected(self):
        path = self.public / "index.html"
        path.write_text(path.read_text() +
                        '<script src="https://example.invalid/runtime.js">'
                        '</script>')
        with self.assertRaisesRegex(ValueError, "resource|offline"):
            publication.create_record(self.public, self.result)

    def test_all_historical_versions_and_archives_remain_bound(self):
        older = delivery.package(self.fixture.build, self.root / "old-output",
                                 source_root=self.fixture.source,
                                 release_id="1.0.0-docs-old")
        delivery.publish(older["directory"], self.public,
                         archive=older["archive"])
        delivery.publish(self.result["directory"], self.public,
                         archive=self.result["archive"])
        record = publication.create_record(self.public, self.result)
        publication.verify(self.public, record)
        history = self.public / "versions/1.0.0-docs-old/en/index.html"
        history.write_text("changed historical payload")
        with self.assertRaises(ValueError):
            publication.verify(self.public, record)

    def test_changed_original_package_anchor_cannot_create_record(self):
        broken = dict(self.result)
        broken["archive_sha256"] = "0" * 64
        with self.assertRaises(ValueError):
            publication.create_record(self.public, broken)

    def test_missing_active_archive_cannot_create_record(self):
        (self.public / self.record["archive_path"]).unlink()
        with self.assertRaises(ValueError):
            publication.create_record(self.public, self.result)

    def test_isolated_deployed_stdlib_bootstrap_without_source_git(self):
        deployed = self.root / "deployment"
        for relative in ("tools/documentation/delivery.py",
                         "tools/evidence/identity.py",
                         "tools/evidence/common.py",
                         "tools/configure/metadata.py"):
            target = deployed / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        script = deployed / "verifier/verify_documentation.py"
        script.parent.mkdir()
        producer = ROOT / "tools/documentation/verify_publication.py"
        shutil.copyfile(producer, script)
        record_path = deployed / "publication.json"
        record_path.write_text(json.dumps(self.record))
        for command in (["init", "-q"], ["add", "."],
                        ["-c", "user.name=Test", "-c",
                         "user.email=test@example.com", "commit", "-qm",
                         "Separate trusted delivery branch"]):
            subprocess.run(["git", "-C", str(deployed)] + command,
                           check=True, capture_output=True)
        deployment_commit = subprocess.check_output(
            ["git", "-C", str(deployed), "rev-parse", "HEAD"],
            text=True).strip()
        self.assertNotEqual(deployment_commit,
                            self.record["documentation_commit"])
        # Destruction applies only to this test's synthetic Git fixture, proving
        # the verifier cannot silently depend on the original checkout.
        shutil.rmtree(self.fixture.source)
        result = subprocess.run([sys.executable, str(script), str(self.public),
                                 str(record_path)], cwd=deployed,
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["status"], "verified")

    def test_record_inside_public_is_rejected(self):
        record_path = self.public / "publication.json"
        record_path.write_text(json.dumps(self.record))
        arguments = [str(self.public), str(record_path)]
        self.assertNotEqual(publication.main(arguments), 0)

    def test_duplicate_publication_json_fields_are_rejected(self):
        record_path = self.root / "publication.json"
        text = json.dumps(self.record).replace('"schema_version": 1',
            '"schema_version": 1, "schema_version": 1')
        record_path.write_text(text)
        arguments = [str(self.public), str(record_path)]
        self.assertNotEqual(publication.main(arguments), 0)


if __name__ == "__main__":
    unittest.main()
