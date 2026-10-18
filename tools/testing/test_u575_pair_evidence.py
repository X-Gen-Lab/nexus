"""Bounded pair evidence contracts; tiny files are not CMSE proof."""

import copy
import gzip
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

from tools.evidence import store, u575_pair
from tools.evidence.common import EvidenceError


def identity(path, raw=b"x"):
    return {"path": path, "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()}


def report():
    values = {name: identity("/old/" + name) for name in
              u575_pair.ARTIFACTS}
    return {"schema_version": 1, "kind": "u575_trustzone_software_pair",
            "status": "passed", "physical_status": "not_executed",
            "candidate_sealing": "not_established_for_multi_image_pair",
            "source": {"root": "/old", "commit": "a" * 40,
                       "tree": "b" * 40, "dirty": False,
                       "submodules": []},
            "source_inputs": [identity("/old/source.c")],
            "tools": {name: identity("/tools/" + name) for name in
                      u575_pair.TOOLS},
            "artifacts": values,
            "build_inputs": [{"stage": stage,
                              "compile_commands": identity(
                                  "/old/" + stage + "/compile_commands.json"),
                              "compiler_dependencies": [identity(
                                  "/old/include.h")]} for stage in
                             ("secure", "nonsecure")],
            "commands": [{"argv": ["reader"], "cwd": "/old",
                          "returncode": 0,
                          "stdout": identity("/old/stdout"),
                          "stderr": identity("/old/stderr", b"")}],
            "checks": {}, "cross_call": {}, "nonsecure_functions": []}


class TypedPairInputs(unittest.TestCase):
    def test_exact_typed_positions_preserve_empty_raw(self):
        values = u575_pair.collect_report(report())
        self.assertEqual(values["/old/stderr"]["size"], 0)
        self.assertIn("/old/include.h", values)
        self.assertEqual(len(values), 18)

    def test_arbitrary_nested_identity_is_not_discovered(self):
        value = report()
        value["checks"]["untrusted"] = identity("/unrelated")
        self.assertNotIn("/unrelated", u575_pair.collect_report(value))

    def test_same_label_conflicting_identity_rejected(self):
        value = report()
        value["source_inputs"].append(identity("/old/source.c", b"changed"))
        with self.assertRaisesRegex(EvidenceError, "conflict"):
            u575_pair.collect_report(value)

    def test_boolean_schema_is_not_integer_schema(self):
        value = report()
        value["schema_version"] = True
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)

    def test_failed_command_cannot_be_sealed(self):
        value = report()
        value["commands"][0]["returncode"] = 1
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)

    def test_missing_raw_rejected(self):
        value = report()
        del value["commands"][0]["stderr"]
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)

    def test_role_extra_or_missing_rejected(self):
        for change in ("extra", "missing"):
            with self.subTest(change=change):
                value = report()
                if change == "extra":
                    value["artifacts"]["other"] = identity("/other")
                else:
                    del value["artifacts"]["secure_import"]
                with self.assertRaises(EvidenceError):
                    u575_pair.collect_report(value)

    def test_duplicate_stage_cannot_replace_nonsecure(self):
        value = report()
        value["build_inputs"][1]["stage"] = "secure"
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)

    def test_mixed_identity_shape_rejected(self):
        value = report()
        value["source_inputs"][0]["size"] = 1
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)

    def test_dirty_build_source_rejected(self):
        value = report()
        value["source"]["dirty"] = True
        with self.assertRaises(EvidenceError):
            u575_pair.collect_report(value)


class PortableCompileAndMap(unittest.TestCase):
    def database(self, stage="secure"):
        flags = ["/never/open/compiler", "-mcpu=cortex-m33", "-mthumb",
                 "-mfloat-abi=soft"]
        if stage == "secure":
            flags.append("-mcmse")
        flags += ["-o", "object.c.obj", "-c", "/old/source.c"]
        return [{"directory": "/old/build", "file": "/old/source.c",
                 "output": "object.c.obj", "arguments": flags}]

    def test_original_compiler_label_never_needs_to_exist(self):
        self.assertEqual(u575_pair.compile_outputs(
            self.database(), "secure", "/never/open/compiler"),
            ["/old/build/object.c.obj"])

    def test_rehashed_wrong_security_flags_rejected(self):
        value = self.database()
        value[0]["arguments"].remove("-mcmse")
        with self.assertRaises(EvidenceError):
            u575_pair.compile_outputs(value, "secure", "/never/open/compiler")

    def test_nonsecure_must_not_use_cmse(self):
        with self.assertRaises(EvidenceError):
            u575_pair.compile_outputs(self.database(), "nonsecure",
                                      "/never/open/compiler")

    def test_compiler_identity_and_output_must_agree(self):
        for mutate in ("compiler", "output"):
            value = self.database()
            if mutate == "compiler":
                value[0]["arguments"][0] = "/other/compiler"
            else:
                value[0]["output"] = "different.obj"
            with self.subTest(mutate=mutate), self.assertRaises(EvidenceError):
                u575_pair.compile_outputs(value, "secure",
                                          "/never/open/compiler")

    def test_map_retains_order_and_lexical_toolchain_labels(self):
        raw = (b"LOAD CMakeFiles/start.obj\n"
               b"LOAD /tools/bin/../lib/libgcc.a\n"
               b"LOAD /tools/bin/../lib/libgcc.a\n"
               b"LOAD linker stubs\n")
        self.assertEqual(u575_pair.map_inputs(raw, "/old/build"),
                         ["/old/build/CMakeFiles/start.obj",
                          "/tools/lib/libgcc.a", "/tools/lib/libgcc.a"])

    def test_map_without_actual_load_files_rejected(self):
        with self.assertRaises(EvidenceError):
            u575_pair.map_inputs(b"LOAD linker stubs\n", "/old/build")


class PairArchiveBoundary(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def archive(self, contents=None, suffix=b"", compressed_suffix=b""):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode="w", format=tarfile.USTAR_FORMAT
                          ) as archive:
            for name, data, kind in contents or [("data/000000", b"x", b"0")]:
                entry = tarfile.TarInfo(name)
                entry.type = kind
                entry.size = len(data)
                archive.addfile(entry, io.BytesIO(data))
        path = self.root / "archive.tar.gz"
        path.write_bytes(gzip.compress(output.getvalue() + suffix, mtime=0) +
                         compressed_suffix)
        return path

    def test_real_store_transport_is_structurally_accepted(self):
        source = self.root / "source"
        source.mkdir()
        (source / "empty").write_bytes(b"")
        package = store.export_tree(source, self.root / "store")
        entries = u575_pair.validate_archive(Path(package["archive"]["path"]))
        self.assertIn("delivery-manifest.json", entries)

    def test_duplicate_path_rejected(self):
        path = self.archive([("same", b"1", b"0"), ("same", b"2", b"0")])
        with self.assertRaises(EvidenceError):
            u575_pair.validate_archive(path)

    def test_unsafe_and_special_entries_rejected(self):
        for name, kind in (("../outside", b"0"), ("/absolute", b"0"),
                           ("link", b"2"), ("sparse", b"S"),
                           ("pax", b"x"), ("long", b"L")):
            with self.subTest(name=name), self.assertRaises(EvidenceError):
                u575_pair.validate_archive(self.archive([(name, b"", kind)]))

    def test_nonzero_tar_tail_and_second_tar_rejected(self):
        for tail in (b"junk", tarfile.TarInfo("other").tobuf()):
            with self.subTest(tail=tail[:4]), self.assertRaises(EvidenceError):
                u575_pair.validate_archive(self.archive(suffix=tail))

    def test_compressed_junk_or_member_rejected(self):
        for tail in (b"junk", gzip.compress(b"other")):
            with self.subTest(tail=tail[:4]), self.assertRaises(EvidenceError):
                u575_pair.validate_archive(self.archive(compressed_suffix=tail))

    def test_crc_isize_and_truncation_rejected(self):
        original = self.archive().read_bytes()
        for offset in (-8, -4, None):
            value = bytearray(original)
            if offset is None:
                value = value[:-1]
            else:
                value[offset] ^= 1
            path = self.root / "mutated.gz"
            path.write_bytes(value)
            with self.subTest(offset=offset), self.assertRaises(EvidenceError):
                u575_pair.validate_archive(path)

    def test_invalid_first_end_block_is_not_silently_skipped(self):
        raw = bytearray(gzip.decompress(self.archive().read_bytes()))
        raw[1024] = 88
        path = self.root / "invalid.gz"
        path.write_bytes(gzip.compress(raw))
        with self.assertRaises(EvidenceError):
            u575_pair.validate_archive(path)

    def test_logical_size_bound_checked_before_payload_read(self):
        entry = tarfile.TarInfo("huge")
        entry.size = u575_pair.MAX_BYTES + 1
        path = self.root / "huge.gz"
        path.write_bytes(gzip.compress(entry.tobuf() + b"\0" * 1024))
        with self.assertRaisesRegex(EvidenceError, "bound"):
            u575_pair.validate_archive(path)

    def test_symlink_archive_refused(self):
        original = self.archive()
        link = self.root / "link"
        link.symlink_to(original)
        with self.assertRaises(EvidenceError):
            u575_pair.validate_archive(link)


class PairSchemaAndRuntime(unittest.TestCase):
    def test_pair_identity_is_relative_not_a_historical_input_label(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            (source / "data").mkdir(parents=True)
            # Invalid context intentionally reaches semantic parsing only.
            raw = json.dumps({"members": [], "context": {}}).encode()
            (source / "pair.json").write_bytes(raw)
            package = store.export_tree(source, root / "store")
            manifest = {"schema_version": 1,
                        "kind": "u575_pair_build_evidence",
                        "pair": {"path": "pair.json", "size": len(raw),
                                 "sha256": hashlib.sha256(raw).hexdigest()},
                        "portable": package}
            encoded = json.dumps(manifest).encode()
            path = root / "manifest.json"
            path.write_bytes(encoded)
            with self.assertRaisesRegex(EvidenceError, "invalid fields"):
                u575_pair.verify(path, hashlib.sha256(encoded).hexdigest(), {})

    def test_pair_member_digest_must_match_portable_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            (source / "data").mkdir(parents=True)
            data = source / "data/000000"
            data.write_bytes(b"x")
            data.chmod(384)
            pair = {"schema_version": 1, "kind": "u575_pair_roles",
                "context": {},
                    "roles": {}, "members": [{"id": "data/000000",
                    "path": "/old/never-read-source.c", "size": 1,
                    "sha256": "0" * 64, "mode": 384}], **u575_pair.STATUS}
            pair_raw = json.dumps(pair).encode()
            (source / "pair.json").write_bytes(pair_raw)
            package = store.export_tree(source, root / "store")
            value = {"schema_version": 1, "kind": "u575_pair_build_evidence",
                     "pair": {"path": "pair.json", "size": len(pair_raw),
                              "sha256": hashlib.sha256(pair_raw).hexdigest()},
                     "portable": package}
            raw = json.dumps(value).encode()
            path = root / "manifest.json"
            path.write_bytes(raw)
            with self.assertRaisesRegex(EvidenceError, "member.*digest"):
                u575_pair.verify(path, hashlib.sha256(raw).hexdigest(), {})

    def test_rehashed_boolean_inner_tree_schema_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            (source / "data").mkdir(parents=True)
            pair_raw = json.dumps({"members": [], "context": {}}).encode()
            (source / "pair.json").write_bytes(pair_raw)
            package = store.export_tree(source, root / "store")
            archive_path = Path(package["archive"]["path"])
            changed = io.BytesIO()
            with tarfile.open(archive_path, "r:gz") as old:
                with tarfile.open(fileobj=changed, mode="w",
                                  format=tarfile.USTAR_FORMAT) as new:
                    for item in old:
                        data = old.extractfile(
                            item).read() if item.isfile() else b""
                        if item.name == "delivery-manifest.json":
                            value = json.loads(data)
                            value["schema_version"] = True
                            data = json.dumps(value).encode()
                            package["manifest_sha256"] = hashlib.sha256(
                                data).hexdigest()
                            item.size = len(data)
                        new.addfile(item,
                            io.BytesIO(data) if item.isfile() else None)
            compressed = gzip.compress(changed.getvalue())
            mutated = root / "mutated.tar.gz"
            mutated.write_bytes(compressed)
            package["archive"] = {"path": str(mutated), "size": len(compressed),
                                  "sha256": hashlib.sha256(
                                      compressed).hexdigest()}
            manifest = {"schema_version": 1, "kind": "u575_pair_build_evidence",
                        "pair": {"path": "pair.json", "size": len(pair_raw),
                                 "sha256": hashlib.sha256(
                                     pair_raw).hexdigest()},
                        "portable": package}
            encoded = json.dumps(manifest).encode()
            path = root / "manifest.json"
            path.write_bytes(encoded)
            with self.assertRaisesRegex(EvidenceError, "schema"):
                u575_pair.verify(path, hashlib.sha256(encoded).hexdigest(), {})

    def test_runtime_uses_maintained_labelled_length_prefixed_input_hash(self):
        from tools.evidence.identity import canonical_digest
        blobs, members, bundles = {}, [], {}
        def add(path, raw):
            blobs[path] = raw
            row = {"id": "data/%06d" % len(members), "path": path,
                   "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
                   "mode": 384}
            members.append(row)
            return row["id"]
        for stage in ("secure", "nonsecure"):
            paths = {"assembly": "/" + stage + ".toml",
                     "cpu_facts": "/" + stage + "-cpu.json",
                     "platform_metadata": "/policy.json"}
            for key, path in paths.items():
                if path not in blobs:
                    add(path, key.encode())
            digest = hashlib.sha256()
            for key, label in (("assembly", b"assembly"),
                               ("cpu_facts", b"cpu-facts"),
                               ("platform_metadata", b"platform-metadata")):
                raw = blobs[paths[key]]
                digest.update(label + b"\0" + len(raw).to_bytes(8, "big") + raw)
            resolved = {"cpu_profile": {"cpu": {"arch": "cortex-m33",
                        "float_abi": "soft", "fpu": "none", "security": stage},
                        "backend": "baremetal"},
                            "input_sha256": digest.hexdigest()}
            checksum = canonical_digest(resolved)
            resolved["configuration_sha256"] = checksum
            code_path = "/generator.py"
            if code_path not in blobs:
                add(code_path, b"generator")
            records = [{"path": "generator.py",
                        "sha256": hashlib.sha256(blobs[code_path]).hexdigest()}]
            generator = {"scope": "runtime",
                         "identity_kind": "configuration-generator-source",
                         "files": records, "sha256": canonical_digest(records)}
            leaves = {"resolved-cpu.json": json.dumps(resolved).encode(),
                      "nexus_config.h": ('#define NEXUS_CONFIG_SHA256 "' +
                                         checksum + '"').encode(),
                      "selection.cmake": ('set(NEXUS_CONFIG_SHA256 "' +
                                          checksum + '")').encode(),
                      "input_paths.json": json.dumps(paths).encode(),
                      "generator-identity.json": json.dumps(generator).encode(),
                      "configuration_code_paths.json": json.dumps(
                          {"generator.py": code_path}).encode(),
                      ".nexus-runtime-bundle": b"runtime",
                      ".nexus-cpu-bundle": b"cpu"}
            bundles[stage] = {name: add("/" + stage + "/" + name, raw)
                              for name, raw in leaves.items()}
        pair = {"members": members, "roles": {"bundles": bundles},
            "context": {}}
        self.assertIsNone(u575_pair._bundle_checks(pair, blobs.__getitem__))


class AdditionalArchiveBounds(PairArchiveBoundary):
    def test_member_count_bound(self):
        path = self.root / "count.gz"
        with gzip.open(path, "wb") as stream:
            for index in range(u575_pair.MAX_ENTRIES + 1):
                stream.write(tarfile.TarInfo("entry%d" % index).tobuf())
            stream.write(b"\0" * 1024)
        with self.assertRaisesRegex(EvidenceError, "bound"):
            u575_pair.validate_archive(path)

    def test_total_logical_bound_before_second_payload(self):
        path = self.root / "total.gz"
        size = u575_pair.MAX_BYTES // 2 + 1
        with gzip.open(path, "wb") as stream:
            item = tarfile.TarInfo("first")
            item.size = size
            stream.write(item.tobuf())
            for _ in range((size + 511) // 512 * 512 // 65536):
                stream.write(b"\0" * 65536)
            stream.write(b"\0" * (((size + 511) // 512 * 512) % 65536))
            item.name = "second"
            stream.write(item.tobuf())
        with self.assertRaisesRegex(EvidenceError, "logical size bound"):
            u575_pair.validate_archive(path)

    def test_expanded_bound_in_zero_padding(self):
        path = self.root / "expanded.gz"
        with gzip.open(path, "wb") as stream:
            for _ in range(u575_pair.MAX_EXPANDED // 65536 + 1):
                stream.write(b"\0" * 65536)
        with self.assertRaisesRegex(EvidenceError, "expanded gzip bound"):
            u575_pair.validate_archive(path)

    def test_manifest_size_rejected_before_tarfile_materialization(self):
        item = tarfile.TarInfo("delivery-manifest.json")
        item.size = u575_pair.MAX_METADATA + 1
        path = self.root / "metadata.gz"
        path.write_bytes(gzip.compress(item.tobuf() + b"\0" * 1024))
        with self.assertRaisesRegex(EvidenceError, "metadata bound"):
            u575_pair.validate_archive(path)


class ExactTypedClosure(unittest.TestCase):
    def setUp(self):
        self.blobs = {}
        self.value = report()
        self.value["artifacts"] = {}
        for stage in ("secure", "nonsecure"):
            for suffix, role in ((".elf", stage), (".map", stage + "_map")):
                path = "/old/" + stage + "/image" + suffix
                self.blobs[path] = b"tiny metadata fixture, not an ARM ELF"
                if suffix == ".map":
                    self.blobs[path] = b"LOAD /tools/libgcc.a\n"
                    if stage == "nonsecure":
                        self.blobs[path] += b"LOAD /old/secure/import.o\n"
                    self.blobs[path] += b"LOAD linker stubs\n"
                self.value["artifacts"][role] = identity(path, self.blobs[path])
        path = "/old/secure/import.o"
        self.blobs[path] = b"not a CMSE proof"
        self.value["artifacts"]["secure_import"] = identity(path,
            self.blobs[path])
        for item in u575_pair.collect_report(self.value).values():
            self.blobs.setdefault(item["path"],
                b"" if item["size"] == 0 else b"x")
        for row in self.value["build_inputs"]:
            stage = row["stage"]
            flags = ["/tools/gcc", "-mcpu=cortex-m33", "-mthumb",
                     "-mfloat-abi=soft"]
            if stage == "secure":
                flags.append("-mcmse")
            flags += ["-o", "built.obj", "-c", "/old/source.c"]
            database = [{"directory": "/old/" + stage,
                         "file": "/old/source.c", "output": "built.obj",
                         "arguments": flags}]
            path = row["compile_commands"]["path"]
            self.blobs[path] = json.dumps(database).encode()
            row["compile_commands"] = identity(path, self.blobs[path])
            for leaf in u575_pair.BUNDLE:
                path = "/old/" + stage + "/nexus-runtime/generated/" + leaf
                self.blobs[path] = b"bundle"
        self.blobs["/old/secure/built.obj"] = b"emitted object"
        self.blobs["/tools/libgcc.a"] = b"emitted link archive"
        self.blobs["/unrelated"] = b"not an allowed payload member"
        self.blobs["/old/report.json"] = json.dumps(self.value).encode()
        self.history = {"files": [{"path": path, "bytes": len(raw),
                                  "sha256": hashlib.sha256(raw).hexdigest(),
                                  "mode": 384} for path,
                                      raw in self.blobs.items()]}
        self.blobs["/history.json"] = json.dumps(self.history).encode()
        self.context = {"schema_version": 1, "kind": "u575_pair_build_context",
                        "report": identity("/old/report.json",
                            self.blobs["/old/report.json"]),
                        "source": copy.deepcopy(self.value["source"]),
                        "tools": copy.deepcopy(self.value["tools"]),
                        "history": [identity("/history.json",
                            self.blobs["/history.json"])]}

    def pair(self):
        return u575_pair.build_pair(self.context, self.blobs.__getitem__)

    def test_only_typed_union_and_actual_outputs_enter_closure(self):
        pair = self.pair()
        names = {row["path"] for row in pair["members"]}
        self.assertNotIn("/unrelated", names)
        self.assertIn("/old/secure/built.obj", names)
        self.assertNotIn("/old/nonsecure/built.obj", names)
        self.assertEqual(pair["roles"]["compile_outputs"]["nonsecure"],
                         [{"path": "/old/nonsecure/built.obj",
                             "emitted": False}])
        self.assertEqual(len(pair["roles"]["bundles"]["secure"]), 8)
        self.assertEqual(pair["software_status"], "not_qualified")

    def test_role_swap_rejected_before_any_reader_execution(self):
        pair = self.pair()
        roles = pair["roles"]["artifacts"]
        secure = roles["secure"]
        roles["secure"] = roles["nonsecure"]
        roles["nonsecure"] = secure
        with self.assertRaisesRegex(EvidenceError, "roles"):
            u575_pair.inspect_pair(pair, self.blobs.__getitem__, {})

    def test_unrelated_reanchored_member_still_rejected(self):
        pair = self.pair()
        pair["members"].append({"id": "data/999999", "path": "/unrelated",
                                "size": len(self.blobs["/unrelated"]),
                                    "mode": 384,
                                "sha256": hashlib.sha256(
                                    self.blobs["/unrelated"]).hexdigest()})
        with self.assertRaisesRegex(EvidenceError, "union"):
            u575_pair.inspect_pair(pair, self.blobs.__getitem__, {})

    def test_boolean_pair_schema_not_equal_to_integer_schema(self):
        pair = self.pair()
        pair["schema_version"] = True
        with self.assertRaisesRegex(EvidenceError, "schema"):
            u575_pair.inspect_pair(pair, self.blobs.__getitem__, {})

    def test_rehashed_unknown_map_library_not_self_authorized(self):
        path = self.value["artifacts"]["secure_map"]["path"]
        self.blobs[path] = b"LOAD /unrecorded/libc.a\n"
        self.value["artifacts"]["secure_map"] = identity(path, self.blobs[path])
        self.blobs["/old/report.json"] = json.dumps(self.value).encode()
        self.context["report"] = identity("/old/report.json",
            self.blobs["/old/report.json"])
        # Reanchor changed known data consistently, leaving the new library
        # absent so the actual map-closure contract is the rejecting oracle.
        history = copy.deepcopy(self.history)
        for row in history["files"]:
            if row["path"] in (path, "/old/report.json"):
                changed = identity(row["path"], self.blobs[row["path"]])
                row.update(bytes=changed["bytes"], sha256=changed["sha256"])
        self.blobs["/history.json"] = json.dumps(history).encode()
        self.context["history"] = [identity(
            "/history.json", self.blobs["/history.json"])]
        with self.assertRaisesRegex(EvidenceError,
            "missing from trusted history"):
            self.pair()

    def test_nonsecure_map_must_load_the_exact_retained_import_object(self):
        path = self.value["artifacts"]["nonsecure_map"]["path"]
        self.blobs[path] = self.blobs[path].replace(
            b"LOAD /old/secure/import.o\n", b"")
        self.value["artifacts"]["nonsecure_map"] = identity(
            path, self.blobs[path])
        self.blobs["/old/report.json"] = json.dumps(self.value).encode()
        self.context["report"] = identity(
            "/old/report.json", self.blobs["/old/report.json"])
        history = copy.deepcopy(self.history)
        for row in history["files"]:
            if row["path"] in (path, "/old/report.json"):
                item = identity(row["path"], self.blobs[row["path"]])
                row.update(bytes=item["bytes"], sha256=item["sha256"])
        self.blobs["/history.json"] = json.dumps(history).encode()
        self.context["history"] = [identity(
            "/history.json", self.blobs["/history.json"])]
        with self.assertRaisesRegex(EvidenceError, "import.*LOAD"):
            self.pair()

    def test_map_include_cannot_overwrite_an_original_typed_identity(self):
        path = self.value["artifacts"]["secure_map"]["path"]
        self.blobs[path] = b"LOAD /tools/gcc\n"
        self.value["artifacts"]["secure_map"] = identity(path, self.blobs[path])
        self.blobs["/tools/gcc"] = b"different compiler bytes"
        self.blobs["/old/report.json"] = json.dumps(self.value).encode()
        self.context["report"] = identity(
            "/old/report.json", self.blobs["/old/report.json"])
        history = copy.deepcopy(self.history)
        for row in history["files"]:
            if row["path"] in (path, "/tools/gcc", "/old/report.json"):
                item = identity(row["path"], self.blobs[row["path"]])
                row.update(bytes=item["bytes"], sha256=item["sha256"])
        self.blobs["/history.json"] = json.dumps(history).encode()
        self.context["history"] = [identity(
            "/history.json", self.blobs["/history.json"])]
        with self.assertRaisesRegex(EvidenceError, "conflict"):
            self.pair()

    def test_external_context_and_history_report_must_agree(self):
        history = copy.deepcopy(self.history)
        for row in history["files"]:
            if row["path"] == "/old/report.json":
                row["sha256"] = "0" * 64
        self.blobs["/history.json"] = json.dumps(history).encode()
        self.context["history"] = [identity(
            "/history.json", self.blobs["/history.json"])]
        with self.assertRaisesRegex(EvidenceError, "context/history"):
            self.pair()

    def test_source_tool_context_does_not_follow_rehashed_report(self):
        self.context["source"]["commit"] = "c" * 40
        with self.assertRaisesRegex(EvidenceError, "source/tool context"):
            self.pair()

    def test_raw_metadata_tampering_rejected(self):
        self.blobs["/old/secure/compile_commands.json"] += b"changed"
        with self.assertRaisesRegex(EvidenceError, "metadata identity"):
            self.pair()


class RetainedCopyAndOutputBoundary(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "input"
        self.target = self.root / "output" / "data"
        self.target.parent.mkdir()

    def row(self, declared):
        return {"path": str(self.source), "size": len(declared),
                "sha256": hashlib.sha256(declared).hexdigest(), "mode": 384}

    def test_oversized_source_never_written_past_declared_length(self):
        self.source.write_bytes(b"x" + b"tail" * 1024)
        with self.assertRaisesRegex(EvidenceError, "changed"):
            u575_pair._copy_retained(self.source, self.target, self.row(b"x"))
        self.assertLessEqual(self.target.stat().st_size, 1)

    def test_growing_stream_reads_only_declared_length_and_one_eof_byte(self):
        self.source.write_bytes(b"x")
        class GrowingReader(io.BytesIO):
            def __init__(self):
                super().__init__(b"x" * (2 * u575_pair.CHUNK))
                self.requests = []
            def read(self, count=-1):
                self.requests.append(count)
                return super().read(count)
        reader = GrowingReader()
        original_open = Path.open
        def external_open(path, *args, **kwargs):
            if path == self.source and args and args[0] == "rb":
                return reader
            return original_open(path, *args, **kwargs)
        with mock.patch.object(Path, "open", external_open):
            with self.assertRaisesRegex(EvidenceError, "changed"):
                u575_pair._copy_retained(
                    self.source, self.target, self.row(b"x"))
        self.assertLessEqual(self.target.stat().st_size, 1)
        self.assertLessEqual(sum(reader.requests), 2)

    def test_empty_declared_source_does_not_copy_new_bytes(self):
        self.source.write_bytes(b"new")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            u575_pair._copy_retained(self.source, self.target, self.row(b""))
        self.assertEqual(self.target.stat().st_size, 0)

    def test_exact_source_copies_bytes_and_mode(self):
        self.source.write_bytes(b"exact source")
        u575_pair._copy_retained(
            self.source, self.target, self.row(b"exact source"))
        self.assertEqual(self.target.read_bytes(), b"exact source")
        self.assertEqual(self.target.stat().st_mode & 511, 384)

    def test_wrong_prefix_digest_is_rejected(self):
        self.source.write_bytes(b"y")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            u575_pair._copy_retained(self.source, self.target, self.row(b"x"))
        self.assertEqual(self.target.read_bytes(), b"y")

    def test_ordinary_dotdot_cannot_enter_original_source_tree(self):
        source = self.root / "source"
        source.mkdir()
        (self.root / "other").mkdir()
        data = source / "input.c"
        data.write_bytes(b"source")
        output = self.root / "other" / ".." / "source" / "new-output"
        with self.assertRaisesRegex(EvidenceError, "source tree"):
            u575_pair._fresh_output(output,
                {"source": {"root": str(source)}},
                {"members": [{"path": str(data)}]})
        self.assertFalse((source / "new-output").exists())


class PortableCmakeLinkGraph(unittest.TestCase):
    def setUp(self):
        self.directory = "/old/secure"
        self.reply = self.directory + "/.cmake/api/v1/reply/"
        self.index = self.reply + "index-fixture.json"
        self.values = {
            self.index: {"reply": {"codemodel-v2": {
                "kind": "codemodel", "version": {"major": 2},
                "jsonFile": "codemodel.json"}}},
            self.reply + "codemodel.json": {
                "paths": {"build": self.directory},
                "configurations": [{"name": "", "targets": [
                    {"id": "app-id", "name": "u575_secure",
                     "jsonFile": "app.json"},
                    {"id": "interface-id", "name": "interface",
                     "jsonFile": "interface.json"},
                    {"id": "lib-id", "name": "actual_library",
                     "jsonFile": "lib.json"},
                    {"id": "unrelated-id", "name": "unrelated",
                     "jsonFile": "unrelated.json"}]}]},
            self.reply + "app.json": {"id": "app-id", "name": "u575_secure",
                "type": "EXECUTABLE", "artifacts": [{"path": "image.elf"}],
                "dependencies": [{"id": "interface-id"}]},
            self.reply + "interface.json": {"id": "interface-id",
                "name": "interface", "type": "UTILITY",
                "dependencies": [{"id": "lib-id"}]},
            self.reply + "lib.json": {"id": "lib-id",
                "name": "actual_library", "type": "STATIC_LIBRARY",
                "artifacts": [{"path": "nexus-runtime/libactual.a"}]},
            self.reply + "unrelated.json": {"id": "unrelated-id",
                "name": "unrelated", "type": "STATIC_LIBRARY",
                "artifacts": [{"path": "unrelated.a"}]}}
        self.reads = []

    def graph(self):
        def read(label):
            self.reads.append(label)
            return json.dumps(self.values[label]).encode()
        return u575_pair.cmake_link_graph(self.index, self.directory,
            "secure", "/old/secure/image.elf", read)

    def test_only_actual_executable_dependency_closure_is_selected(self):
        value = self.graph()
        self.assertEqual(value["libraries"],
                         ["/old/secure/nexus-runtime/libactual.a"])
        self.assertNotIn(self.reply + "unrelated.json", value["inputs"])
        self.assertEqual(set(value["inputs"]), set(self.reads))

    def test_duplicate_target_ids_or_reply_paths_are_rejected(self):
        for key in ("id", "jsonFile"):
            self.setUp()
            rows = self.values[self.reply + "codemodel.json"][
                "configurations"][0]["targets"]
            rows[1][key] = rows[0][key]
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.graph()

    def test_missing_or_cyclic_dependency_is_rejected(self):
        for dependencies in ([{"id": "absent"}], [{"id": "app-id"}]):
            self.values[self.reply + "lib.json"]["dependencies"] = dependencies
            with self.subTest(dependencies=dependencies):
                with self.assertRaises(EvidenceError):
                    self.graph()

    def test_reply_escape_and_artifact_escape_are_rejected(self):
        self.values[self.index]["reply"]["codemodel-v2"][
            "jsonFile"] = "../other.json"
        with self.assertRaises(EvidenceError):
            self.graph()
        self.setUp()
        self.values[self.reply + "lib.json"]["artifacts"] = [
            {"path": "../../outside.a"}]
        with self.assertRaises(EvidenceError):
            self.graph()

    def test_stage_root_image_and_target_identity_must_match(self):
        for field in ("build", "image", "target"):
            self.setUp()
            if field == "build":
                self.values[self.reply + "codemodel.json"]["paths"][
                    "build"] = "/other"
            elif field == "image":
                self.values[self.reply + "app.json"]["artifacts"] = [
                    {"path": "other.elf"}]
            else:
                self.values[self.reply + "lib.json"]["id"] = "different"
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.graph()

    def test_optional_api_refs_preserve_legacy_collection(self):
        value = report()
        expected = u575_pair.collect_report(value)
        item = identity(self.index, b"api")
        value["build_inputs"][0]["cmake_file_api"] = [item]
        actual = u575_pair.collect_report(value)
        self.assertEqual(set(actual), set(expected) | {self.index})
        self.assertEqual(actual[self.index]["sha256"], item["sha256"])

    def test_optional_api_type_duplicate_and_cross_stage_refs_rejected(self):
        for entries in (False, [], [identity(self.index)] * 2,
                        [identity("/elsewhere/index.json")]):
            value = report()
            value["build_inputs"][0]["cmake_file_api"] = entries
            with self.subTest(entries=entries):
                with self.assertRaises(EvidenceError):
                    u575_pair.collect_report(value)


class CapturedFileApiClosure(unittest.TestCase):
    def setUp(self):
        self.pure = ExactTypedClosure()
        self.pure.setUp()
        for stage in self.pure.value["build_inputs"]:
            name = stage["stage"]
            graph = PortableCmakeLinkGraph()
            graph.setUp()
            rows = []
            for path, value in graph.values.items():
                if path.endswith("unrelated.json"):
                    continue
                label = path.replace("/secure", "/" + name)
                raw = json.dumps(value).replace("/secure", "/" + name).replace(
                    "u575_secure", "u575_" + name).encode()
                self.pure.blobs[label] = raw
                rows.append(identity(label, raw))
            stage["cmake_file_api"] = rows
            library = "/old/" + name + "/nexus-runtime/libactual.a"
            self.pure.blobs[library] = b"actual graph archive fixture"
            path = stage["stage"] + "_map"
            label = self.pure.value["artifacts"][path]["path"]
            self.pure.blobs[label] += ("LOAD " + library + "\n").encode()
        self.before = {}
        for name in ("libgcc.a", "libc_nano.a", "libnosys.a"):
            label = "/tools/" + name
            self.pure.blobs.setdefault(label, b"before library fixture")
            row = {**identity(label, self.pure.blobs[label]), "mode": 384}
            self.before[name] = {**row, "copy": identity(
                "/control/" + name, self.pure.blobs[label])}
        self.refresh()

    def refresh(self):
        value, blobs = self.pure.value, self.pure.blobs
        for stage in value["build_inputs"]:
            for row in stage["cmake_file_api"]:
                row.update(identity(row["path"], blobs[row["path"]]))
        for row in value["artifacts"].values():
            row.update(identity(row["path"], blobs[row["path"]]))
        blobs["/old/report.json"] = json.dumps(value).encode()
        history = {"files": [{**identity(path, raw), "mode": 384}
            for path, raw in blobs.items() if path != "/history.json"],
            "libraries_before": self.before,
            "libraries_after": {name: {key: field for key, field in row.items()
                if key != "copy"} for name, row in self.before.items()}}
        blobs["/history.json"] = json.dumps(history).encode()
        self.pure.context["report"] = identity("/old/report.json",
            blobs["/old/report.json"])
        self.pure.context["history"] = [identity("/history.json",
            blobs["/history.json"])]

    def test_graph_retains_only_reachable_static_artifacts(self):
        pair = self.pure.pair()
        names = {row["path"] for row in pair["members"]}
        self.assertIn("/old/secure/nexus-runtime/libactual.a", names)
        self.assertIn("/old/nonsecure/.cmake/api/v1/reply/lib.json", names)
        self.assertFalse(any(path.endswith("unrelated.json") for path in names))

    def test_rehashed_unrelated_local_load_is_rejected_offline(self):
        label = "/old/secure/unrelated.a"
        self.pure.blobs[label] = b"not in actual executable target closure"
        self.pure.blobs["/old/secure/image.map"] += (
            "LOAD " + label + "\n").encode()
        self.refresh()
        with self.assertRaisesRegex(EvidenceError, "local.*LOAD"):
            self.pure.pair()

    def test_unrelated_typed_reply_cannot_expand_the_selected_graph(self):
        label = "/old/secure/.cmake/api/v1/reply/unrelated.json"
        self.pure.blobs[label] = b"{}"
        self.pure.value["build_inputs"][0]["cmake_file_api"].append(
            identity(label, b"{}"))
        self.refresh()
        with self.assertRaisesRegex(EvidenceError, "unrelated typed replies"):
            self.pure.pair()

    def test_unknown_external_load_cannot_be_after_authorized(self):
        label = "/tools/unknown.a"
        self.pure.blobs[label] = b"not selected before configure"
        self.pure.blobs["/old/secure/image.map"] += (
            "LOAD " + label + "\n").encode()
        self.refresh()
        with self.assertRaisesRegex(EvidenceError, "external.*LOAD"):
            self.pure.pair()

    def test_rehashed_library_bytes_must_equal_before_and_after(self):
        self.pure.blobs["/tools/libgcc.a"] = b"different library"
        self.refresh()
        with self.assertRaisesRegex(EvidenceError, "library.*changed"):
            self.pure.pair()


if __name__ == "__main__":
    unittest.main()
