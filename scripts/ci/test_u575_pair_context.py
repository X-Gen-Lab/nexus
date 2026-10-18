"""Tiny capture boundaries, not a compiler run or CMSE qualification."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts.ci import u575_trustzone as gate
from tools.evidence.common import EvidenceError
from tools.testing import test_u575_pair_evidence as fixtures


class QueryRecorder:
    def __init__(self, paths):
        self.paths = paths
        self.calls = []

    def execute(self, argv, label):
        self.calls.append((list(argv), label))
        return str(self.paths[argv[-1].split("=", 1)[1]]) + "\n"


class PairContextCapture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        pure = fixtures.ExactTypedClosure()
        pure.setUp()
        def relocate(value):
            return value.replace("/old", str(self.root / "old")).replace(
                "/tools", str(self.root / "tools")).replace(
                "/unrelated", str(self.root / "unrelated"))
        self.report = json.loads(relocate(json.dumps(pure.value)))
        for label, raw in pure.blobs.items():
            if label == "/history.json":
                continue
            target = Path(relocate(label))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(relocate(raw.decode()).encode())
        self.output = self.root / "old"
        self.library_paths = {}
        for name in ("libgcc.a", "libc_nano.a", "libnosys.a"):
            path = self.root / "tools" / name
            path.write_bytes(b"!<arch>\n" + name.encode())
            self.library_paths[name] = path
        self.tools = {name: item["path"] for name, item in
                      self.report["tools"].items()}
        for stage in self.report["build_inputs"]:
            name = stage["stage"]
            directory = self.output / name
            reply = directory / ".cmake/api/v1/reply"
            reply.mkdir(parents=True)
            library = directory / "libcurrent.a"
            library.write_bytes(b"current CMake static library")
            values = {"index-fixture.json": {"reply": {"codemodel-v2": {
                "kind": "codemodel", "version": {"major": 2},
                "jsonFile": "codemodel.json"}}},
                "codemodel.json": {"paths": {"build": str(directory)},
                    "configurations": [{"targets": [
                        {"id": "app-id", "name": "u575_" + name,
                         "jsonFile": "app.json"},
                        {"id": "lib-id", "name": "current_library",
                         "jsonFile": "lib.json"}]}]},
                "app.json": {"id": "app-id", "name": "u575_" + name,
                    "type": "EXECUTABLE", "artifacts": [
                        {"path": "image.elf"}],
                    "dependencies": [{"id": "lib-id"}]},
                "lib.json": {"id": "lib-id", "name": "current_library",
                    "type": "STATIC_LIBRARY", "artifacts": [
                        {"path": library.name}]}}
            stage["cmake_file_api"] = []
            for leaf, value in values.items():
                path = reply / leaf
                path.write_text(json.dumps(value))
                stage["cmake_file_api"].append(fixtures.identity(str(path)))
            path = Path(self.report["artifacts"][name + "_map"]["path"])
            path.write_bytes(path.read_bytes() + ("LOAD " + str(library) +
                                                 "\n").encode())
        self.recorder = QueryRecorder(self.library_paths)
        self.refresh_report()

    def refresh_report(self):
        values = list(self.report["source_inputs"])
        values += list(self.report["tools"].values())
        values += list(self.report["artifacts"].values())
        for row in self.report["build_inputs"]:
            values += [row["compile_commands"]] + row["compiler_dependencies"]
            values += row.get("cmake_file_api", [])
        for row in self.report["commands"]:
            values += [row["stdout"], row["stderr"]]
        for item in values:
            raw = Path(item["path"]).read_bytes()
            key = "bytes" if "bytes" in item else "size"
            item[key] = len(raw)
            item["sha256"] = hashlib.sha256(raw).hexdigest()
        (self.output / "report.json").write_text(json.dumps(self.report))

    def begin(self):
        return gate.begin_pair_context(self.output, self.tools, self.recorder,
                                       self.report["source"])

    def finish(self, before):
        self.refresh_report()
        return gate.finish_pair_context(self.output, self.report, before)

    def test_before_queries_and_complete_bytes_precede_future_build(self):
        before = self.begin()
        self.assertEqual(len(self.recorder.calls), 3)
        self.assertEqual(set(before["libraries"]), set(self.library_paths))
        for name, record in before["libraries"].items():
            original = self.library_paths[name].read_bytes()
            self.assertEqual(Path(record["copy"]["path"]).read_bytes(),
                             original)
            self.assertEqual(record["sha256"],
                             hashlib.sha256(original).hexdigest())
        for argv, _label in self.recorder.calls:
            self.assertEqual(argv[1:4], ["-mcpu=cortex-m33", "-mthumb",
                                         "-mfloat-abi=soft"])

    def test_relative_unresolved_library_query_is_rejected(self):
        self.recorder.paths["libgcc.a"] = "libgcc.a"
        with self.assertRaises(EvidenceError):
            self.begin()

    def test_alias_library_choices_are_rejected(self):
        self.recorder.paths["libc_nano.a"] = self.library_paths["libgcc.a"]
        with self.assertRaises(EvidenceError):
            self.begin()

    def test_symlink_library_is_rejected(self):
        link = self.root / "linked.a"
        link.symlink_to(self.library_paths["libgcc.a"])
        self.recorder.paths["libgcc.a"] = link
        with self.assertRaises(EvidenceError):
            self.begin()

    def test_dirty_source_is_rejected_before_query_or_copy(self):
        self.report["source"]["dirty"] = True
        with self.assertRaises(EvidenceError):
            self.begin()
        self.assertFalse(self.recorder.calls)

    def test_existing_or_symlink_control_directory_is_rejected(self):
        (self.output / "pair-context").symlink_to(self.root / "tools")
        with self.assertRaises(EvidenceError):
            self.begin()

    def test_changed_original_library_rejected_without_context(self):
        before = self.begin()
        self.library_paths["libgcc.a"].write_bytes(b"changed")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            self.finish(before)
        self.assertFalse((self.output / "pair-context/context.json").exists())

    def test_changed_library_mode_rejected(self):
        before = self.begin()
        self.library_paths["libgcc.a"].chmod(0o444)
        with self.assertRaisesRegex(EvidenceError, "changed"):
            self.finish(before)

    def test_changed_retained_before_bytes_rejected(self):
        before = self.begin()
        copy_path = before["libraries"]["libgcc.a"]["copy"]["path"]
        Path(copy_path).write_bytes(b"changed")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            self.finish(before)

    def test_changed_compiler_rejected(self):
        before = self.begin()
        Path(self.tools["gcc"]).write_bytes(b"different compiler")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            self.finish(before)

    def test_changed_noncompiler_reader_cannot_rebind_report(self):
        before = self.begin()
        Path(self.tools["nm"]).write_bytes(b"different trusted reader")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            self.finish(before)
        self.assertFalse((self.output / "pair-context/context.json").exists())

    def test_context_preserves_exact_closure_and_explicit_unbuilt(self):
        before = self.begin()
        path = self.finish(before)
        context = json.loads(path.read_text())
        self.assertEqual(context["source"], self.report["source"])
        self.assertEqual(context["tools"], self.report["tools"])
        self.assertEqual(context["kind"], "u575_pair_build_context")
        history = json.loads(Path(context["history"][0]["path"]).read_text())
        self.assertEqual(len(history["libraries_before"]), 3)
        self.assertEqual(history["not_emitted"]["nonsecure"],
                         [str(self.output / "nonsecure/built.obj")])
        self.assertEqual(history["emitted"]["secure"],
                         [str(self.output / "secure/built.obj")])
        self.assertNotIn(str(self.root / "unrelated"),
                         {row["path"] for row in history["files"]})
        self.assertEqual(context["report"]["sha256"], hashlib.sha256(
            (self.output / "report.json").read_bytes()).hexdigest())

    def test_unrelated_local_load_cannot_be_promoted_by_map_alone(self):
        before = self.begin()
        unrelated = self.output / "secure/unrelated.a"
        unrelated.write_bytes(b"arbitrary local bytes")
        path = Path(self.report["artifacts"]["secure_map"]["path"])
        path.write_bytes(path.read_bytes() + ("LOAD " + str(unrelated) +
                                             "\n").encode())
        with self.assertRaisesRegex(EvidenceError, "local.*LOAD"):
            self.finish(before)
        self.assertFalse((self.output / "pair-context/context.json").exists())

    def test_library_change_after_initial_after_snapshot_is_rejected(self):
        before = self.begin()
        library = self.library_paths["libgcc.a"]
        actual_open = Path.open
        reads = 0
        changed = False
        def changed_external_file(path, mode="r", *args, **kwargs):
            nonlocal reads, changed
            if path == library and mode == "rb":
                reads += 1
            if (path == self.output / "secure/built.obj" and mode == "rb"
                    and not changed):
                library.write_bytes(b"changed after initial observation")
                changed = True
            return actual_open(path, mode, *args, **kwargs)
        with mock.patch.object(Path, "open", changed_external_file):
            with self.assertRaisesRegex(EvidenceError, "changed"):
                self.finish(before)
        self.assertEqual(reads, 2)
        self.assertTrue(changed)
        self.assertFalse((self.output / "pair-context/context.json").exists())

    def test_unknown_external_load_is_rejected(self):
        before = self.begin()
        path = Path(self.report["artifacts"]["secure_map"]["path"])
        path.write_bytes(path.read_bytes() + b"LOAD /unknown/libc.a\n")
        with self.assertRaisesRegex(EvidenceError, "unknown external"):
            self.finish(before)
        self.assertFalse((self.output / "pair-context/context.json").exists())

    def test_missing_internal_load_is_rejected(self):
        before = self.begin()
        path = Path(self.report["artifacts"]["secure_map"]["path"])
        path.write_bytes(path.read_bytes() + ("LOAD " + str(
            self.output / "secure/missing.obj") + "\n").encode())
        with self.assertRaises(EvidenceError):
            self.finish(before)

    def test_missing_import_relation_is_rejected(self):
        before = self.begin()
        path = Path(self.report["artifacts"]["nonsecure_map"]["path"])
        text = path.read_text().replace("LOAD " + str(
            self.output / "secure/import.o") + "\n", "")
        path.write_text(text)
        with self.assertRaisesRegex(EvidenceError, "import.*LOAD"):
            self.finish(before)

    def test_generated_bundle_symlink_is_rejected(self):
        before = self.begin()
        path = self.output / "secure/nexus-runtime/generated/nexus_config.h"
        path.unlink()
        path.symlink_to(self.library_paths["libgcc.a"])
        with self.assertRaises(EvidenceError):
            self.finish(before)

    def test_existing_context_is_not_overwritten(self):
        before = self.begin()
        path = self.output / "pair-context/context.json"
        path.write_bytes(b"owned by previous attempt")
        with self.assertRaises(EvidenceError):
            self.finish(before)
        self.assertEqual(path.read_bytes(), b"owned by previous attempt")

    def test_failed_original_report_cannot_publish_context(self):
        before = self.begin()
        self.report["status"] = "failed"
        with self.assertRaises(EvidenceError):
            self.finish(before)

    def test_original_gate_root_boundary_still_rejects_foreign_source(self):
        output = self.root / "build/refused-gate"
        result = gate.run(self.root, output, capture_pair_context=True)
        self.assertEqual(result, 1)
        report = json.loads((output / "report.json").read_text())
        self.assertIn("selected root differs", report["failure"])
        self.assertFalse((output / "pair-context/context.json").exists())


if __name__ == "__main__":
    unittest.main()
