#!/usr/bin/env python3
"""Seal an unsigned U575 software pair without promoting a candidate.

Only caller-pinned report/history identities authorize the selected closure.
Original paths are inert labels offline; archived code is never executed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import shlex
import sys
import tarfile
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.ci.u575_trustzone import imported_gateways, validate_pair
from tools.evidence import store
from tools.evidence.common import (EvidenceError, atomic_json, fields,
    parse_json, regular_file, run_adapter, sha256_value, stream_digest)
from tools.evidence.identity import canonical_digest, observe_git_source
from tools.measurement.elf import Elf32
from tools.measurement.soc_costs import decode_function

ARTIFACTS = frozenset(("secure", "nonsecure", "secure_import", "secure_map",
                       "nonsecure_map"))
TOOLS = frozenset(("gcc", "nm", "objdump", "readelf", "cmake", "ninja",
                   "python"))
BUNDLE = ("resolved-cpu.json", "nexus_config.h", "selection.cmake",
          "input_paths.json", "generator-identity.json",
          "configuration_code_paths.json", ".nexus-runtime-bundle",
          ".nexus-cpu-bundle")
MAX_BYTES = 128 * 1024 * 1024
MAX_METADATA = 8 * 1024 * 1024
MAX_ENTRIES = 4096
MAX_EXPANDED = 160 * 1024 * 1024
CHUNK = 64 * 1024
STATUS = {"signature_status": "unsigned", "software_status": "not_qualified",
          "physical_status": "not_executed", "qualified": False,
          "promoted": False}


def _encoded(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       allow_nan=False) + "\n").encode()


def _label(value, base=None):
    if (not isinstance(value, str) or not value or "\0" in value
            or "\\" in value):
        raise EvidenceError("invalid original path label")
    path = value if value.startswith("/") else posixpath.join(base or "", value)
    if not path.startswith("/"):
        raise EvidenceError("absolute original path label required")
    return posixpath.normpath(path)


def _identity(value):
    if not isinstance(value, dict):
        raise EvidenceError("typed file identity required")
    key = "size" if "size" in value else "bytes"
    fields(value, {"path", key, "sha256"})
    size = value[key]
    if type(size) is not int or not 0 <= size <= MAX_BYTES:
        raise EvidenceError("file identity size exceeds bound")
    sha256_value(value["sha256"])
    return {"path": _label(value["path"]), "size": size,
            "sha256": value["sha256"]}


def _source(value):
    fields(value, {"root", "commit", "tree", "dirty", "submodules"})
    if (value["dirty"] is not False or
            any(not isinstance(value[k], str) or
                not re.fullmatch(r"[0-9a-f]{40}", value[k])
                for k in ("commit", "tree")) or
            not isinstance(value["submodules"], list) or
            any(not isinstance(row, str) or not row.startswith(" ")
                for row in value["submodules"])):
        raise EvidenceError("original clean locked build-source required")
    _label(value["root"])


def collect_report(report):
    """Collect documented positions, never recursively discover identities."""
    if (type(report.get("schema_version")) is not int or
            report["schema_version"] != 1 or
            report.get("kind") != "u575_trustzone_software_pair" or
            report.get("status") != "passed" or
            report.get("physical_status") != "not_executed" or
            report.get("candidate_sealing") !=
            "not_established_for_multi_image_pair"):
        raise EvidenceError("successful original software pair report required")
    _source(report["source"])
    if set(report["artifacts"]) != ARTIFACTS or set(report["tools"]) != TOOLS:
        raise EvidenceError("exact five artifacts and seven tools required")
    selected = {}

    def add(value):
        item = _identity(value)
        if item["path"] in selected and selected[item["path"]] != item:
            raise EvidenceError("conflicting original file identities")
        selected[item["path"]] = item

    if (not isinstance(report["source_inputs"], list)
        or not report["source_inputs"]):
        raise EvidenceError("original source inputs required")
    for value in report["source_inputs"]:
        add(value)
    for values in (report["artifacts"], report["tools"]):
        for value in values.values():
            add(value)
    stages = report["build_inputs"]
    if (not isinstance(stages, list) or len(stages) != 2 or
            {row["stage"] for row in stages} != {"secure", "nonsecure"}):
        raise EvidenceError("exact two stage input records required")
    for row in stages:
        fields(row, {"stage", "compile_commands", "compiler_dependencies"},
               {"cmake_file_api"})
        add(row["compile_commands"])
        if not isinstance(row["compiler_dependencies"], list):
            raise EvidenceError("compiler dependency list required")
        for value in row["compiler_dependencies"]:
            add(value)
        if "cmake_file_api" in row:
            entries = row["cmake_file_api"]
            if (not isinstance(entries, list) or
                    not 1 <= len(entries) <= MAX_ENTRIES):
                raise EvidenceError(
                    "bounded typed File API input list required")
            reply = posixpath.join(posixpath.dirname(
                row["compile_commands"]["path"]), ".cmake/api/v1/reply")
            names = set()
            for value in entries:
                item = _identity(value)
                if (posixpath.dirname(item["path"]) != reply or
                        not item["path"].endswith(".json") or
                        item["path"] in names):
                    raise EvidenceError("duplicate or escaped File API input")
                names.add(item["path"])
                add(value)
    if not isinstance(report["commands"], list) or not report["commands"]:
        raise EvidenceError("original executed command records required")
    for row in report["commands"]:
        if type(row.get("returncode")) is not int or row["returncode"] != 0:
            raise EvidenceError("unsuccessful original command")
        if not row.get("argv") or not isinstance(row["argv"], list):
            raise EvidenceError("original command argv required")
        if "stdout" not in row or "stderr" not in row:
            raise EvidenceError("both original raw streams required")
        add(row["stdout"])
        add(row["stderr"])
    if len(selected) > MAX_ENTRIES:
        raise EvidenceError("report input count exceeds bound")
    return selected


def compile_outputs(database, stage, compiler):
    """Validate inert argv without opening historical compiler paths."""
    if (stage not in ("secure", "nonsecure") or not isinstance(database, list)
        or not database):
        raise EvidenceError("actual paired compile database required")
    result = []
    for row in database:
        argv = row.get("arguments") or shlex.split(row["command"])
        if (not isinstance(argv, list) or not argv or
                any(not isinstance(arg, str) for arg in argv) or
                _label(argv[0], row["directory"]) != _label(compiler) or
                [a for a in argv if a.startswith("-mcpu=")] !=
                ["-mcpu=cortex-m33"] or
                [a for a in argv if a.startswith("-mfloat-abi=")] !=
                ["-mfloat-abi=soft"] or "-mthumb" not in argv or
                any(a.startswith("-mfpu=") for a in argv) or
                ("-mcmse" in argv) != (stage == "secure")):
            raise EvidenceError("actual compiler CPU/security ABI differs")
        outputs = [argv[i + 1] for i,
            arg in enumerate(argv[:-1]) if arg == "-o"]
        sources = [argv[i + 1] for i,
            arg in enumerate(argv[:-1]) if arg == "-c"]
        target = _label(row["output"], row["directory"])
        if (len(outputs) != 1 or len(sources) != 1 or
                _label(outputs[0], row["directory"]) != target or
                _label(sources[0], row["directory"]) != _label(row["file"])):
            raise EvidenceError("compile output/source declaration differs")
        result.append(target)
    if len(result) != len(set(result)) or len(result) > MAX_ENTRIES:
        raise EvidenceError("duplicate or excessive compile outputs")
    return result


def map_inputs(raw, directory):
    values = []
    for line in raw.decode("utf-8").splitlines():
        if line.startswith("LOAD ") and line != "LOAD linker stubs":
            values.append(_label(line[5:], directory))
    if not values or len(values) > MAX_ENTRIES:
        raise EvidenceError("actual map LOAD inputs required within bound")
    return values


class _Inflated:
    """One bounded gzip member; reject concatenation and bad trailers."""
    def __init__(self, raw):
        self.raw = raw
        self.decoder = zlib.decompressobj(31)
        self.pending = b""
        self.buffer = b""
        self.total = 0
        self.done = False

    def read(self, count):
        while len(self.buffer) < count and not self.done:
            data = self.pending or self.raw.read(CHUNK)
            if not data:
                raise EvidenceError("truncated gzip trailer")
            try:
                block = self.decoder.decompress(data, CHUNK)
            except zlib.error as error:
                raise EvidenceError("invalid gzip CRC or framing") from error
            self.pending = self.decoder.unconsumed_tail
            self.total += len(block)
            if self.total > MAX_EXPANDED:
                raise EvidenceError("expanded gzip bound exceeded")
            self.buffer += block
            if self.decoder.eof:
                if self.decoder.unused_data or self.raw.read(1):
                    raise EvidenceError("gzip trailing bytes or second member")
                self.done = True
        value, self.buffer = self.buffer[:count], self.buffer[count:]
        return value


def validate_archive(path):
    """Inspect raw headers before tarfile can expand PAX or sparse records."""
    path = regular_file(path)
    if path.stat().st_size > MAX_BYTES:
        raise EvidenceError("compressed archive bound exceeded")
    seen, logical = {}, 0
    with path.open("rb") as raw:
        stream = _Inflated(raw)
        while True:
            header = stream.read(512)
            if len(header) != 512:
                raise EvidenceError("truncated TAR header/end")
            if header == b"\0" * 512:
                if stream.read(512) != b"\0" * 512:
                    raise EvidenceError("two TAR end blocks required")
                while True:
                    tail = stream.read(CHUNK)
                    if not tail:
                        return seen
                    if any(tail):
                        raise EvidenceError("nonzero bytes after TAR end")
            if header[156:157] not in (b"0", b"\0", b"5"):
                raise EvidenceError(
                    "TAR extensions/links/sparse/special forbidden")
            try:
                item = tarfile.TarInfo.frombuf(header, "utf-8", "strict")
            except (tarfile.TarError, ValueError, UnicodeError) as error:
                raise EvidenceError("invalid raw TAR header") from error
            name = item.name.rstrip("/") if item.isdir() else item.name
            if (not name or "\\" in name or PurePosixPath(name).is_absolute() or
                    any(p in ("", ".", "..") for p in name.split("/")) or
                    name in seen or len(seen) >= MAX_ENTRIES or
                    item.mode & ~0o777 or item.size < 0 or
                    item.size > MAX_BYTES or (item.isdir() and item.size)):
                raise EvidenceError("unsafe archive path/type/size/count bound")
            if (name in ("delivery-manifest.json", "payload/pair.json") and
                    item.size > MAX_METADATA):
                raise EvidenceError("archive metadata bound exceeded")
            logical += item.size
            if logical > MAX_BYTES:
                raise EvidenceError("archive logical size bound exceeded")
            seen[name] = {"size": item.size, "mode": item.mode,
                          "kind": "directory" if item.isdir() else "file"}
            remaining = (item.size + 511) // 512 * 512
            while remaining:
                block = stream.read(min(CHUNK, remaining))
                if not block:
                    raise EvidenceError("truncated TAR payload")
                remaining -= len(block)


def _read_json(raw):
    if len(raw) > MAX_METADATA:
        raise EvidenceError("pair metadata bound exceeded")
    return parse_json(raw)


def _history(raw):
    value = _read_json(raw)
    if (not isinstance(value.get("files"), list)
        or len(value["files"]) > MAX_ENTRIES):
        raise EvidenceError("bounded trusted history file inventory required")
    result = {}
    for row in value["files"]:
        item = _identity({"path": row["path"], "bytes": row["bytes"],
                          "sha256": row["sha256"]})
        mode = row["mode"]
        if type(mode) is not int or not 0 <= mode <= 0o777:
            raise EvidenceError("trusted history mode invalid")
        item["mode"] = mode
        if item["path"] in result:
            raise EvidenceError("duplicate trusted history label")
        result[item["path"]] = item
    return result


def _context(value):
    fields(value, {"schema_version", "kind", "report", "source", "tools",
                   "history"})
    if (type(value["schema_version"]) is not int
        or value["schema_version"] != 1 or
            value["kind"] != "u575_pair_build_context" or
            set(value["tools"]) != TOOLS or not isinstance(value["history"],
                list)
            or not 1 <= len(value["history"]) <= 4):
        raise EvidenceError("external pair build context is invalid")
    _source(value["source"])
    for item in [value["report"], *value["history"], *value["tools"].values()]:
        _identity(item)


def _checked_read(read, identity):
    if set(identity) == {"id", "path", "size", "sha256", "mode"}:
        identity = {key: identity[key] for key in ("path", "size", "sha256")}
    item = _identity(identity)
    if item["size"] > MAX_METADATA:
        raise EvidenceError("selected metadata bound exceeded")
    raw = read(item["path"])
    if (len(raw) != item["size"] or
            hashlib.sha256(raw).hexdigest() != item["sha256"]):
        raise EvidenceError("retained metadata identity differs")
    return raw


def cmake_link_graph(index, directory, stage, image, read):
    """Read one fresh File API target closure without opening old labels."""
    directory, index = _label(directory), _label(index)
    reply = posixpath.join(directory, ".cmake/api/v1/reply")
    if (posixpath.dirname(index) != reply or
            not posixpath.basename(index).startswith("index-") or
            not index.endswith(".json")):
        raise EvidenceError("exact stage File API index required")
    inputs = []
    def load(label):
        if label in inputs or len(inputs) >= MAX_ENTRIES:
            raise EvidenceError("duplicate or excessive File API reply")
        inputs.append(label)
        return _read_json(read(label))
    def reply_path(name):
        if (not isinstance(name, str) or not name.endswith(".json") or
                name in (".", "..") or "/" in name or "\\" in name):
            raise EvidenceError("File API reply path escapes its stage")
        return posixpath.join(reply, name)
    entry = load(index)["reply"]["codemodel-v2"]
    if entry.get("kind") != "codemodel" or entry.get("version", {}).get(
            "major") != 2:
        raise EvidenceError("codemodel-v2 reply required")
    model = load(reply_path(entry["jsonFile"]))
    configurations = model["configurations"]
    if (_label(model["paths"]["build"]) != directory or
            not isinstance(configurations, list) or len(configurations) != 1):
        raise EvidenceError("one exact stage codemodel configuration required")
    rows = configurations[0]["targets"]
    if not isinstance(rows, list) or not 1 <= len(rows) <= MAX_ENTRIES:
        raise EvidenceError("bounded File API target table required")
    targets, filenames = {}, set()
    for row in rows:
        identifier, path = row["id"], reply_path(row["jsonFile"])
        if (not isinstance(identifier, str) or not identifier or
                identifier in targets or path in filenames):
            raise EvidenceError("duplicate File API target identity or reply")
        filenames.add(path)
        targets[identifier] = row
    roots = [row for row in rows if row["name"] == "u575_" + stage]
    if len(roots) != 1:
        raise EvidenceError("one actual stage executable target required")
    completed, visiting, libraries = set(), set(), []
    def visit(identifier):
        if identifier in completed:
            return
        if identifier in visiting or len(visiting) >= 64:
            raise EvidenceError("cyclic or excessive File API dependency depth")
        if identifier not in targets:
            raise EvidenceError("unknown File API target dependency")
        visiting.add(identifier)
        row = targets[identifier]
        value = load(reply_path(row["jsonFile"]))
        if value.get("id") != identifier or value.get("name") != row["name"]:
            raise EvidenceError("File API target identity mismatch")
        artifacts = value.get("artifacts", [])
        if not isinstance(artifacts, list):
            raise EvidenceError("invalid File API artifact list")
        paths = [_label(item["path"], directory) for item in artifacts]
        if (len(paths) != len(set(paths)) or any(
                posixpath.commonpath((directory, path)) != directory
                for path in paths)):
            raise EvidenceError("File API artifact leaves its stage")
        if identifier == roots[0]["id"]:
            if value.get("type") != "EXECUTABLE" or paths != [_label(image)]:
                raise EvidenceError("actual File API executable differs")
        elif value.get("type") == "STATIC_LIBRARY":
            if len(paths) != 1 or paths[0] in libraries:
                raise EvidenceError(
                    "one unique static library artifact required")
            libraries.extend(paths)
        dependencies = value.get("dependencies", [])
        if (not isinstance(dependencies, list) or
                len(dependencies) > MAX_ENTRIES):
            raise EvidenceError("invalid File API target dependencies")
        identifiers = [item["id"] for item in dependencies]
        if len(identifiers) != len(set(identifiers)):
            raise EvidenceError("duplicate File API target dependency")
        for dependency in identifiers:
            visit(dependency)
        visiting.remove(identifier)
        completed.add(identifier)
    visit(roots[0]["id"])
    return {"inputs": inputs, "libraries": sorted(libraries)}


def build_pair(context, read):
    """Derive the exact typed closure from externally pinned history data."""
    _context(context)
    report = _read_json(_checked_read(read, context["report"]))
    selected = collect_report(report)
    if (report["source"] != context["source"]
        or report["tools"] != context["tools"]):
        raise EvidenceError("original source/tool context mismatch")
    history, documents = {}, []
    for identity in context["history"]:
        raw_history = _checked_read(read, identity)
        documents.append(_read_json(raw_history))
        for name, row in _history(raw_history).items():
            if name in history and history[name] != row:
                raise EvidenceError("trusted history inventories conflict")
            history[name] = row
    captured = ["cmake_file_api" in stage for stage in report["build_inputs"]]
    libraries = {}
    if any(captured):
        if not all(captured):
            raise EvidenceError("both captured stage File API graphs required")
        snapshots = [doc for doc in documents if "libraries_before" in doc]
        if len(snapshots) != 1:
            raise EvidenceError("one before/after library snapshot required")
        before = snapshots[0]["libraries_before"]
        after = snapshots[0].get("libraries_after")
        names = {"libgcc.a", "libc_nano.a", "libnosys.a"}
        if (not isinstance(before, dict) or not isinstance(after, dict) or
                set(before) != names or set(after) != names):
            raise EvidenceError(
                "three GCC-selected library identities required")
        for name in sorted(names):
            row = before[name]
            original = {key: value for key, value in row.items()
                        if key != "copy"}
            if original != after[name]:
                raise EvidenceError("original library before/after changed")
            item = _identity({key: original[key] for key in
                              ("path", "bytes", "sha256")})
            item["mode"] = original["mode"]
            if item["path"] in libraries:
                raise EvidenceError("original libraries cannot alias")
            libraries[item["path"]] = item
    added = {}
    for item in [context["report"], *context["history"]]:
        entry = _identity(item)
        # External anchors authorize these metadata documents themselves.
        prior = history.get(entry["path"])
        if prior and any(entry[k] != prior[k] for k in ("size", "sha256")):
            raise EvidenceError("external context/history metadata conflict")
        entry["mode"] = prior["mode"] if prior else 0o600
        added[entry["path"]] = entry

    def include(name):
        name = _label(name)
        if name not in history:
            raise EvidenceError("required input missing from trusted history")
        prior = selected.get(name)
        if prior and any(prior[k] != history[name][k]
                         for k in ("size", "sha256")):
            raise EvidenceError("typed report/history input conflict")
        selected[name] = {key: history[name][key] for key in
                          ("path", "size", "sha256")}
        return name

    bundles, outputs, links = {}, {}, {}
    for stage in report["build_inputs"]:
        name = stage["stage"]
        directory = posixpath.dirname(stage["compile_commands"]["path"])
        bundle = posixpath.join(directory, "nexus-runtime/generated")
        bundles[name] = {leaf: include(posixpath.join(bundle, leaf)) for leaf in
                         BUNDLE}
        database = json.loads(_checked_read(read, stage["compile_commands"]))
        declared = compile_outputs(database, name,
            report["tools"]["gcc"]["path"])
        outputs[name] = [{"path": path, "emitted": path in history}
                         for path in declared]
        for path in declared:
            if path in history:
                include(path)
        raw = _checked_read(read, report["artifacts"][name + "_map"])
        links[name] = [include(path) for path in map_inputs(raw, directory)]
        if "cmake_file_api" in stage:
            api = {_identity(row)["path"]: row
                   for row in stage["cmake_file_api"]}
            indexes = [path for path in api if posixpath.basename(
                path).startswith("index-")]
            if len(indexes) != 1:
                raise EvidenceError("one exact File API index required")
            def api_read(label):
                if label not in api:
                    raise EvidenceError(
                        "File API reply missing from typed inputs")
                return _checked_read(read, api[label])
            graph = cmake_link_graph(indexes[0], directory, name,
                report["artifacts"][name]["path"], api_read)
            if set(graph["inputs"]) != set(api):
                raise EvidenceError("File API contains unrelated typed replies")
            allowed = {row["path"] for row in outputs[name] if row["emitted"]}
            allowed.update(graph["libraries"])
            if name == "nonsecure":
                allowed.add(report["artifacts"]["secure_import"]["path"])
            for label in links[name]:
                if posixpath.commonpath((directory, label)) == directory:
                    if label not in allowed:
                        raise EvidenceError(
                            "unexpected local File API map LOAD")
                elif (label not in libraries and not (name == "nonsecure" and
                        label == report["artifacts"]["secure_import"]["path"])):
                    raise EvidenceError("unknown external captured map LOAD")
    imported = _identity(report["artifacts"]["secure_import"])["path"]
    if imported not in links["nonsecure"]:
        raise EvidenceError(
            "NonSecure import object missing from actual map LOAD")
    for path, item in selected.items():
        if path not in history or any(item[key] != history[path][key]
                                      for key in ("size", "sha256")):
            raise EvidenceError("original report input/history mismatch")
        item["mode"] = history[path]["mode"]
        if path in libraries and any(item[key] != libraries[path][key]
                                     for key in ("size", "sha256", "mode")):
            raise EvidenceError("original library captured bytes changed")
    selected.update(added)
    if (len(selected) > MAX_ENTRIES
        or sum(r["size"] for r in selected.values()) > MAX_BYTES):
        raise EvidenceError("pair selected closure exceeds bound")
    members = [{"id": "data/%06d" % i, **selected[path]} for i, path in
               enumerate(sorted(selected))]
    ids = {row["path"]: row["id"] for row in members}
    roles = {"report": ids[_identity(context["report"])["path"]],
             "history": [ids[_identity(x)["path"]] for x in context["history"]],
             "artifacts": {k: ids[_identity(v)["path"]] for k, v in
                           report["artifacts"].items()},
             "bundles": {k: {leaf: ids[path] for leaf, path in v.items()}
                         for k, v in bundles.items()},
             "compile_outputs": outputs, "link_inputs": links}
    return {"schema_version": 1, "kind": "u575_pair_roles", "context": context,
            "members": members, "roles": roles, **STATUS}


def _bundle_checks(pair, read):
    records = {row["path"]: row for row in pair["members"]}
    roles, context = pair["roles"], pair["context"]
    by_id = {row["id"]: row for row in pair["members"]}
    for stage, bundle in roles["bundles"].items():
        def raw(leaf):
            return _checked_read(read, by_id[bundle[leaf]])
        resolved = _read_json(raw("resolved-cpu.json"))
        digest = resolved.pop("configuration_sha256", None)
        if digest != canonical_digest(resolved):
            raise EvidenceError("runtime configuration digest differs")
        cpu = resolved["cpu_profile"]["cpu"]
        if (cpu["arch"] != "cortex-m33" or cpu["float_abi"] != "soft" or
                cpu["fpu"] != "none" or cpu["security"] != stage or
                resolved["cpu_profile"]["backend"] != "baremetal"):
            raise EvidenceError("runtime security/ABI configuration differs")
        for leaf, token in (("nexus_config.h", '#define NEXUS_CONFIG_SHA256 "'),
                            ("selection.cmake", 'set(NEXUS_CONFIG_SHA256 "')):
            if raw(leaf).count((token + digest + '"').encode()) != 1:
                raise EvidenceError(
                    "runtime generated configuration marker differs")
        paths = _read_json(raw("input_paths.json"))
        fields(paths, {"assembly", "cpu_facts", "platform_metadata"})
        data = [_checked_read(read, records[_label(paths[k])]) for k in
                ("assembly", "cpu_facts", "platform_metadata")]
        inputs_hash = hashlib.sha256()
        for label, value in zip((b"assembly", b"cpu-facts",
            b"platform-metadata"),
                                data):
            inputs_hash.update(label + b"\0" + len(value).to_bytes(8,
                "big") + value)
        if inputs_hash.hexdigest() != resolved["input_sha256"]:
            raise EvidenceError("runtime authored input digest differs")
        code = _read_json(raw("configuration_code_paths.json"))
        generator = _read_json(raw("generator-identity.json"))
        if (generator.get("scope") != "runtime" or
                generator.get(
                    "identity_kind") != "configuration-generator-source"):
            raise EvidenceError("runtime generator identity is invalid")
        entries = generator["files"]
        if set(code) != {row["path"] for row in entries}:
            raise EvidenceError("runtime generator input closure differs")
        for row in entries:
            if records[_label(code[row["path"]])]["sha256"] != row["sha256"]:
                raise EvidenceError("runtime generator source differs")
        if generator["sha256"] != canonical_digest(entries):
            raise EvidenceError("runtime generator digest differs")
        if not raw(".nexus-runtime-bundle") or not raw(".nexus-cpu-bundle"):
            raise EvidenceError("runtime bundle marker missing")


def _reader_identities(readers, report):
    if set(readers) != {"nm", "objdump"}:
        raise EvidenceError("two externally pinned trusted readers required")
    actual = {}
    for name, value in readers.items():
        item = _identity(value)
        path = regular_file(item["path"])
        with path.open("rb") as stream:
            checksum = stream_digest(stream)
        if (path.stat().st_size != item["size"] or checksum != item["sha256"] or
                item["sha256"] != report["tools"][name]["sha256"]):
            raise EvidenceError("trusted reader identity differs")
        actual[name] = item
    return actual


def inspect_pair(pair, read, readers):
    """Recompute paired ELF boundaries with trusted external data readers."""
    fields(pair, {"schema_version", "kind", "context", "members", "roles",
                  *STATUS})
    if (type(pair["schema_version"]) is not int or pair["schema_version"] != 1
        or
            pair["kind"] != "u575_pair_roles"):
        raise EvidenceError("strict pair schema required")
    expected = build_pair(pair["context"], read)
    if _encoded(pair) != _encoded(expected):
        raise EvidenceError("pair roles or exact allowed member union differs")
    report = _read_json(_checked_read(read, pair["context"]["report"]))
    pinned = _reader_identities(readers, report)
    _bundle_checks(pair, read)
    artifacts = {k: _checked_read(read, v) for k,
        v in report["artifacts"].items()}
    with tempfile.TemporaryDirectory(prefix="u575-pair-readers-") as directory:
        paths = {}
        for name in ("secure", "nonsecure", "secure_import"):
            path = Path(directory) / name
            path.write_bytes(artifacts[name])
            paths[name] = path
        def execute(name, argv):
            return run_adapter([pinned[name]["path"], *argv], 30,
                               cwd=directory).decode("ascii")
        imported = imported_gateways(execute("nm", ["-P",
            str(paths["secure_import"])]))
        checks = validate_pair(artifacts["secure"], artifacts["nonsecure"],
            imported)
        if checks != report["checks"]:
            raise EvidenceError("recomputed pair boundaries differ from report")
        secure = Elf32(artifacts["secure"], float_abi="soft")
        ns = Elf32(artifacts["nonsecure"], float_abi="soft")
        raw = execute("objdump", ["-d", "-z",
                      "--disassemble=__gnu_cmse_nonsecure_call",
                          str(paths["secure"])])
        decoded = decode_function(secure, "__gnu_cmse_nonsecure_call",
            raw.encode("ascii"))
        if (decoded != report["cross_call"] or not any(
                row["mnemonic"] == "blxns" for row in decoded["instructions"])):
            raise EvidenceError("actual CMSE NonSecure call differs")
        inspected = []
        for symbol in ns.defined_symbols:
            if symbol["type"] != 2 or not symbol["size"]:
                continue
            if symbol["name"] in imported and symbol["section"] == 0xfff1:
                continue
            raw = execute("objdump", ["-d", "-z",
                "--disassemble=" + symbol["name"],
                                      str(paths["nonsecure"])])
            value = decode_function(ns, symbol["name"], raw.encode("ascii"))
            if any(row["mnemonic"].split(".")[0] in
                   {"msr", "cpsid", "cpsie", "svc", "mcr", "vmsr"}
                   for row in value["instructions"]):
                raise EvidenceError("NS privileged instruction present")
            inspected.append(value)
        if not inspected or inspected != report["nonsecure_functions"]:
            raise EvidenceError("actual NonSecure function inspection differs")
    if _reader_identities(readers, report) != pinned:
        raise EvidenceError("trusted readers changed during inspection")
    return checks


def _live_read(path):
    file = regular_file(path, nonempty=False)
    if file.stat().st_size > MAX_METADATA:
        raise EvidenceError("metadata input bound exceeded")
    return file.read_bytes()


def _envelope(value):
    fields(value, {"schema_version", "kind", "archive", "manifest_sha256",
                   "files", "bytes", "signature_status", "product_promotion"})
    if (type(value["schema_version"]) is not int
        or value["schema_version"] != 1 or
            value["kind"] != "portable_evidence" or
            value["signature_status"] != "unsigned" or
            value["product_promotion"] is not False or
            type(value["files"]) is not int
                or not 1 <= value["files"] <= MAX_ENTRIES or
            type(value["bytes"]) is not int
                or not 0 <= value["bytes"] <= MAX_BYTES):
        raise EvidenceError("strict portable envelope required")
    _identity(value["archive"])
    sha256_value(value["manifest_sha256"])


def verify(manifest_path, expected_sha256, readers, *, archive=None):
    """Verify only against an independent complete outer-manifest digest."""
    sha256_value(expected_sha256)
    before = observe_git_source(ROOT)
    raw = _live_read(manifest_path)
    if hashlib.sha256(raw).hexdigest() != expected_sha256:
        raise EvidenceError("external manifest anchor differs")
    manifest = _read_json(raw)
    fields(manifest, {"schema_version", "kind", "pair", "portable"})
    if (type(manifest["schema_version"]) is not int or
            manifest["schema_version"] != 1 or
            manifest["kind"] != "u575_pair_build_evidence"):
        raise EvidenceError("unsupported pair evidence manifest")
    _envelope(manifest["portable"])
    if manifest["pair"].get("path") != "pair.json":
        raise EvidenceError("exact pair.json role required")
    pair_identity = _identity({**manifest["pair"], "path": "/pair.json"})
    path = regular_file(archive or manifest["portable"]["archive"]["path"])
    headers = validate_archive(path)
    package = manifest["portable"]
    with tarfile.open(path, "r:gz") as content:
        tree = _read_json(content.extractfile(
            content.getmember("delivery-manifest.json")).read())
    if (type(tree.get("schema_version")) is not int or
            tree["schema_version"] != 1 or tree.get("kind") != "evidence_tree"):
        raise EvidenceError("strict inner evidence tree schema required")
    result = store.verify_package(package, archive_override=path)
    cached = {}
    with tarfile.open(path, "r:gz") as content:
        member = content.getmember("payload/pair.json")
        if member.size != pair_identity["size"] or member.size > MAX_METADATA:
            raise EvidenceError("pair manifest size differs")
        pair_raw = content.extractfile(member).read()
        if hashlib.sha256(pair_raw).hexdigest() != pair_identity["sha256"]:
            raise EvidenceError("pair manifest identity differs")
        pair = _read_json(pair_raw)
        if (not isinstance(pair.get("members"), list)
            or len(pair["members"]) > MAX_ENTRIES):
            raise EvidenceError("bounded pair member table required")
        tree_rows = {row["path"]: row for row in tree["entries"]}
        expected_names = {"payload/" + row["id"] for row in pair["members"]}
        expected_names |= {"payload/pair.json", "payload/data",
            "delivery-manifest.json"}
        if set(headers) != expected_names:
            raise EvidenceError("archive exact pair closure differs")
        rows = {row["path"]: row for row in pair["members"]}
        if len(rows) != len(pair["members"]):
            raise EvidenceError("duplicate original member label")
        for index, row in enumerate(pair["members"]):
            fields(row, {"id", "path", "size", "sha256", "mode"})
            _identity({k: row[k] for k in ("path", "size", "sha256")})
            if type(row["mode"]) is not int or not 0 <= row["mode"] <= 0o777:
                raise EvidenceError("invalid member mode")
            if row["id"] != "data/%06d" % index:
                raise EvidenceError("noncanonical pair data ID")
            item = content.getmember("payload/" + row["id"])
            if row["sha256"] != tree_rows[row["id"]]["sha256"]:
                raise EvidenceError(
                    "pair member digest differs from portable inventory")
            if (not item.isfile() or item.size != row["size"] or
                    item.mode != row["mode"]):
                raise EvidenceError("pair member size/mode differs")
        def read(label):
            row = rows[_label(label)]
            if row["size"] > MAX_METADATA:
                raise EvidenceError("metadata read bound exceeded")
            if row["id"] not in cached:
                if sum(map(len, cached.values())) + row["size"] > MAX_METADATA:
                    raise EvidenceError(
                        "cumulative metadata cache bound exceeded")
                cached[row["id"]] = content.extractfile(
                    content.getmember("payload/" + row["id"])).read()
            value = cached[row["id"]]
            if hashlib.sha256(value).hexdigest() != row["sha256"]:
                raise EvidenceError("pair data digest differs")
            return value
        checks = inspect_pair(pair, read, readers)
    if observe_git_source(ROOT) != before:
        raise EvidenceError("live verifier source changed")
    return {"status": "verified", **STATUS, "files": result["files"],
            "bytes": result["bytes"], "manifest_sha256": expected_sha256,
            "build_source": pair["context"]["source"],
                "verifier_source": before,
            "checks": checks}


def _copy_retained(source, target, row):
    """Never read/copy past the declared prefix except one EOF observation."""
    target.parent.mkdir(exist_ok=True)
    checksum = hashlib.sha256()
    remaining = row["size"]
    with source.open("rb") as src, target.open("xb") as dst:
        while remaining:
            block = src.read(min(CHUNK, remaining))
            if not block or len(block) > remaining:
                raise EvidenceError("seal source bytes changed")
            dst.write(block)
            checksum.update(block)
            remaining -= len(block)
        if src.read(1):
            raise EvidenceError("seal source bytes changed")
    if checksum.hexdigest() != row["sha256"]:
        raise EvidenceError("seal source bytes changed")
    target.chmod(row["mode"])


def _fresh_output(output, context, pair):
    output = Path(output).absolute()
    if (output.exists() or any(p.is_symlink() for p in [output,
        *output.parents])):
        raise EvidenceError("pair seal output must be new and unaliased")
    output = output.resolve()
    if output.exists():
        raise EvidenceError("pair seal output must be new and unaliased")
    for row in pair["members"]:
        path = regular_file(row["path"], nonempty=False)
        if output == path or output in path.parents:
            raise EvidenceError("seal output overlaps retained inputs")
    source_root = Path(context["source"]["root"]).resolve()
    if output == source_root or source_root in output.parents:
        raise EvidenceError("seal output overlaps original source tree")
    return output


def seal(context_path, expected_context_sha256, output, readers):
    """Seal exactly the externally anchored typed closure; never rebuild."""
    before = observe_git_source(ROOT)
    raw = _live_read(context_path)
    sha256_value(expected_context_sha256)
    if hashlib.sha256(raw).hexdigest() != expected_context_sha256:
        raise EvidenceError("external build context anchor differs")
    context = _read_json(raw)
    pair = build_pair(context, _live_read)
    inspect_pair(pair, _live_read, readers)
    output = _fresh_output(output, context, pair)
    payload = output / "payload"
    payload.mkdir(parents=True)
    for row in pair["members"]:
        source = regular_file(row["path"], nonempty=False)
        target = payload / row["id"]
        _copy_retained(source, target, row)
    pair_raw = _encoded(pair)
    (payload / "pair.json").write_bytes(pair_raw)
    package = store.export_tree(payload, output / "store")
    manifest = {"schema_version": 1, "kind": "u575_pair_build_evidence",
                "pair": {"path": "pair.json", "size": len(pair_raw),
                         "sha256": hashlib.sha256(pair_raw).hexdigest()},
                "portable": package}
    if observe_git_source(ROOT) != before:
        raise EvidenceError("live verifier source changed during seal")
    atomic_json(output / "manifest.json", manifest)
    checksum = hashlib.sha256(
        (output / "manifest.json").read_bytes()).hexdigest()
    return verify(output / "manifest.json", checksum, readers)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    for name in ("seal", "verify"):
        command = sub.add_parser(name)
        command.add_argument("--nm", type=Path, required=True)
        command.add_argument("--nm-sha256", required=True)
        command.add_argument("--objdump", type=Path, required=True)
        command.add_argument("--objdump-sha256", required=True)
        if name == "seal":
            command.add_argument("--context", type=Path, required=True)
            command.add_argument("--expected-context-sha256", required=True)
            command.add_argument("--output", type=Path, required=True)
        else:
            command.add_argument("--manifest", type=Path, required=True)
            command.add_argument("--expected-manifest-sha256", required=True)
            command.add_argument("--archive", type=Path)
    args = parser.parse_args()
    try:
        readers = {name: {"path": str(getattr(args, name).absolute()),
                         "size": regular_file(getattr(args,
                             name)).stat().st_size,
                         "sha256": getattr(args, name + "_sha256")}
                   for name in ("nm", "objdump")}
        if args.operation == "seal":
            result = seal(args.context, args.expected_context_sha256,
                          args.output, readers)
        else:
            result = verify(args.manifest, args.expected_manifest_sha256,
                            readers, archive=args.archive)
        print(json.dumps(result, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print("U575 pair evidence rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
