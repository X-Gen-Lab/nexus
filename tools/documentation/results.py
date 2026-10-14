"""Admit the retained documentation result index without fetching evidence.

The original CSV digest seals its exit conditions. This check compares task
identities, statuses and scopes with that authority, and checks the qualified
code identity against its retained index. Historical execution sources never
inherit the working HEAD. Archive member bytes need a separate delivery check.
"""

import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from urllib.parse import unquote, urlsplit


INDEX_PATH = "docs/status/documentation-results.json"
TASK_AUTHORITY = "docs/design/documentation-system-execution.csv"
QUALIFIED_INDEX = "docs/delivery/build-system-results.json"
TASK_IDS = tuple(f"DOC-{number:02d}" for number in range(1, 59))
TOP_FIELDS = frozenset((
    "schema_version", "kind", "status", "validation_scope", "task_authority",
    "task_authority_sha256", "task_count", "formal_evidence_required_for",
    "source", "qualified_code_reference", "documentation_package",
    "evidence_delivery", "offline_tool_cache", "actual_scope_receipts",
    "evidence", "tasks", "retention", "limitations", "physical_hil",
    "qualification_boundary",
))
SCOPE_RECEIPTS = frozenset((
    "clean_source_build", "normal_commit_gates",
    "browser_desktop_narrow_zoom_print", "published_site_and_download",
    "independent_download_restore", "restored_browser",
    "final_independent_review", "accepted_task_records",
))


class _Invalid(ValueError):
    pass


def _fail(location, reason):
    raise _Invalid(f"{location}: {reason}")


def _object(value, location, fields=None):
    if not isinstance(value, dict):
        _fail(location, "expected object")
    if fields is not None:
        missing = set(fields) - value.keys()
        unknown = value.keys() - set(fields)
        if missing:
            _fail(location, "missing fields: " + ", ".join(sorted(missing)))
        if unknown:
            _fail(location, "unknown fields: " + ", ".join(sorted(unknown)))
    return value


def _list(value, location, *, nonempty=False):
    if not isinstance(value, list) or (nonempty and not value):
        kind = "nonempty list" if nonempty else "list"
        _fail(location, "expected " + kind)
    return value


def _text(value, location):
    if not isinstance(value, str) or not value.strip():
        _fail(location, "expected nonempty string")
    return value


def _integer(value, location, minimum=0):
    if type(value) is not int or value < minimum:
        _fail(location, f"expected integer >= {minimum}")


def _hex(value, location, length):
    if not isinstance(value, str) or not re.fullmatch(
            "[0-9a-f]{" + str(length) + "}", value):
        _fail(location, f"expected {length}-digit lowercase hexadecimal digest")


def _equal(value, expected, location):
    if type(value) is not type(expected) or value != expected:
        _fail(location, f"expected {expected!r}")


def _relative(value, location):
    _text(value, location)
    if (value != value.strip() or "\\" in value or "#" in value or
            "?" in value or ":" in value or any(ord(c) < 32 for c in value) or
            value.startswith("/") or
            any(part in {"", ".", ".."} for part in value.split("/")) or
            PurePosixPath(value).as_posix() != value):
        _fail(location, "expected normalized relative path")
    return value


def _file(root, value, location):
    path = root / _relative(value, location)
    if any(item.is_symlink() for item in (path, *path.parents)):
        _fail(location, "symlink rejected")
    if not path.is_file():
        _fail(location, "missing file: " + value)
    return path


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            _fail("JSON", "duplicate field: " + key)
        result[key] = value
    return result


def _json(path, location):
    try:
        return json.loads(path.read_text(encoding="utf-8"),
                          object_pairs_hook=_pairs)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail(location, "invalid JSON: " + str(error))


def _digest(path, expected, location):
    _hex(expected, location, 64)
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        _fail(location, "does not match retained file bytes")


def _url(value, location, commit=None):
    _text(value, location)
    parsed = urlsplit(value)
    try:
        # urlsplit leaves invalid/non-numeric ports unchecked until this
        # property is read; hostname alone is insufficient URL admission.
        parsed.port
    except ValueError:
        _fail(location, "invalid URL port")
    if (parsed.scheme != "https" or not parsed.hostname or parsed.username or
            parsed.password or parsed.fragment or parsed.query or
            any(ord(c) <= 32 for c in value)):
        _fail(location, "expected absolute HTTPS URL without credentials")
    parts = unquote(parsed.path).strip("/").split("/")
    if any(part in {"", ".", ".."} for part in parts):
        _fail(location, "non-normalized URL path")
    if parsed.hostname == "raw.githubusercontent.com":
        if len(parts) < 4:
            _fail(location, "missing immutable Git object URL")
        _hex(parts[2], location, 40)
    if commit is not None:
        if (parsed.hostname != "raw.githubusercontent.com" or
                parts[:3] != ["X-Gen-Lab", "nexus", commit]):
            _fail(location, "URL must bind declared immutable storage_commit")
    return parsed


def _receipt(value, location, records, scope="scope"):
    _object(value, location, {"member_path", "bytes", "sha256", scope})
    member = _relative(value["member_path"], location + ".member_path")
    if not member.startswith("payload/"):
        _fail(location + ".member_path", "expected payload archive member")
    _integer(value["bytes"], location + ".bytes", 1)
    _hex(value["sha256"], location + ".sha256", 64)
    _text(value[scope], location + "." + scope)
    identity = (value["bytes"], value["sha256"])
    if member in records and records[member] != identity:
        _fail(location, "conflicting identities for member: " + member)
    records[member] = identity


def _parts(value, location, total):
    seen, size = set(), 0
    for number, item in enumerate(_list(value, location, nonempty=True)):
        name = f"{location}[{number}]"
        _object(item, name, {"path", "bytes", "sha256"})
        path = _relative(item["path"], name + ".path")
        if path in seen:
            _fail(location, "duplicate part path: " + path)
        seen.add(path)
        _integer(item["bytes"], name + ".bytes", 1)
        _hex(item["sha256"], name + ".sha256", 64)
        size += item["bytes"]
    if size != total:
        _fail(location, "part bytes do not equal archive_bytes")


def _authority(root, document):
    _equal(document["task_authority"], TASK_AUTHORITY, "task_authority")
    path = _file(root, document["task_authority"], "task_authority")
    _digest(path, document["task_authority_sha256"], "task_authority_sha256")
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        fields, rows = reader.fieldnames or [], list(reader)
    required = {"task_id", "validation_status", "validation_scope",
                "exit_condition", "evidence_paths"}
    if len(set(fields)) != len(fields) or not required.issubset(fields):
        _fail("task_authority", "missing or duplicate CSV columns")
    if any(None in row or any(v is None for v in row.values()) for row in rows):
        _fail("task_authority", "CSV row columns do not match header")
    if tuple(row["task_id"] for row in rows) != TASK_IDS:
        _fail("task_authority", "expected exact original 58 task IDs and order")
    for row in rows:
        for field in required:
            _text(row[field], "task_authority." + row["task_id"] + "." + field)
        members = row["evidence_paths"].split(";")
        expected = ("payload/final-closure/accepted-task-records.json#" +
                    row["task_id"])
        if INDEX_PATH not in members or expected not in members:
            _fail("task_authority." + row["task_id"],
                  "evidence paths lose original exit mapping")
    return rows


def _qualified(root, value):
    location = "qualified_code_reference"
    _object(value, location, {"commit", "tree", "index", "index_sha256"})
    for key in ("commit", "tree"):
        _hex(value[key], location + "." + key, 40)
    _equal(value["index"], QUALIFIED_INDEX, location + ".index")
    path = _file(root, value["index"], location + ".index")
    _digest(path, value["index_sha256"], location + ".index_sha256")
    code = _object(_json(path, location), location)
    _equal(code.get("kind"), "nexus_build_system_validation_index",
           location + ".kind")
    source = _object(code.get("source"), location + ".source")
    fields = (("canonical_code_commit", "commit"), ("code_tree", "tree"))
    for field, target in fields:
        _equal(source.get(field), value[target], location + ".source." + field)


def _delivery(value):
    location = "evidence_delivery"
    _object(value, location, {
        "status", "channel", "release_url", "storage_commit", "retention_tag",
        "archive_name", "archive_sha256", "archive_bytes", "parts_url",
        "parts_sha256", "parts", "inventory_url", "inventory_sha256", "members",
        "verifier_url", "verifier_sha256", "export_receipt_url",
        "export_receipt_sha256", "external_receipt_url",
        "external_receipt_sha256", "external_receipt_bytes",
        "excluded_duplicate_bodies", "boundary",
    })
    _equal(value["status"], "passed", location + ".status")
    _hex(value["storage_commit"], location + ".storage_commit", 40)
    for field in ("channel", "retention_tag", "boundary"):
        _text(value[field], location + "." + field)
    _relative(value["archive_name"], location + ".archive_name")
    for field in ("archive_sha256", "parts_sha256", "inventory_sha256",
                  "verifier_sha256", "export_receipt_sha256",
                  "external_receipt_sha256"):
        _hex(value[field], location + "." + field, 64)
    for field in ("archive_bytes", "members", "external_receipt_bytes"):
        _integer(value[field], location + "." + field, 1)
    _url(value["release_url"], location + ".release_url")
    for field in ("parts_url", "inventory_url", "verifier_url",
                  "export_receipt_url"):
        _url(value[field], location + "." + field, value["storage_commit"])
    _url(value["external_receipt_url"], location + ".external_receipt_url")
    _parts(value["parts"], location + ".parts", value["archive_bytes"])
    excluded_location = location + ".excluded_duplicate_bodies"
    excluded = _object(value["excluded_duplicate_bodies"], excluded_location,
                       {"files", "bytes", "index_url", "index_sha256",
                        "boundary"})
    for field in ("files", "bytes"):
        _integer(excluded[field], excluded_location + "." + field)
    _url(excluded["index_url"], excluded_location + ".index_url",
         value["storage_commit"])
    _hex(excluded["index_sha256"], excluded_location + ".index_sha256", 64)
    _text(excluded["boundary"], excluded_location + ".boundary")


def _validate(root, document):
    _object(document, "index", TOP_FIELDS)
    values = (("schema_version", 1),
              ("kind", "nexus_documentation_delivery_results_index"),
              ("status", "passed"),
              ("validation_scope", "accepted_docs_exit"), ("task_count", 58))
    for field, expected in values:
        _equal(document[field], expected, field)
    rows = _authority(root, document)
    records = {}
    source = _object(document["source"], "source", {
        "documentation_commit", "documentation_tree",
        "clean_execution_receipt", "boundary"})
    for field in ("documentation_commit", "documentation_tree"):
        _hex(source[field], "source." + field, 40)
    _text(source["boundary"], "source.boundary")
    _receipt(source["clean_execution_receipt"],
             "source.clean_execution_receipt", records)
    _qualified(root, document["qualified_code_reference"])
    package = _object(document["documentation_package"],
                      "documentation_package", {
                          "release_id", "manifest_sha256", "archive_sha256",
                          "archive_bytes", "payload_files", "download_url",
                          "pages_download_url"})
    release = package["release_id"]
    if not isinstance(release, str) or not re.fullmatch(
            r"docs-[0-9]{8}-" + source["documentation_commit"][:12], release):
        _fail("documentation_package.release_id",
              "does not bind documentation_commit")
    for field in ("manifest_sha256", "archive_sha256"):
        _hex(package[field], "documentation_package." + field, 64)
    for field in ("archive_bytes", "payload_files"):
        _integer(package[field], "documentation_package." + field, 1)
    for field in ("download_url", "pages_download_url"):
        url = _url(package[field], "documentation_package." + field)
        if PurePosixPath(url.path).name != release + ".tar.gz":
            _fail("documentation_package." + field,
                  "archive name does not bind release_id")
    _delivery(document["evidence_delivery"])
    cache = _object(document["offline_tool_cache"], "offline_tool_cache", {
        "storage_commit", "parts_url", "parts_sha256", "archive_sha256",
        "archive_bytes", "parts", "archive_members",
        "locked_configuration_members",
        "qualified_tuple", "actual_installation"})
    _hex(cache["storage_commit"], "offline_tool_cache.storage_commit", 40)
    _url(cache["parts_url"], "offline_tool_cache.parts_url",
         cache["storage_commit"])
    for field in ("parts_sha256", "archive_sha256"):
        _hex(cache[field], "offline_tool_cache." + field, 64)
    for field in ("archive_bytes", "archive_members",
                  "locked_configuration_members"):
        _integer(cache[field], "offline_tool_cache." + field, 1)
    _text(cache["qualified_tuple"], "offline_tool_cache.qualified_tuple")
    _parts(cache["parts"], "offline_tool_cache.parts", cache["archive_bytes"])
    _receipt(cache["actual_installation"],
             "offline_tool_cache.actual_installation", records)
    scopes = _object(document["actual_scope_receipts"], "actual_scope_receipts",
                     SCOPE_RECEIPTS)
    for key, receipt in scopes.items():
        _receipt(receipt, "actual_scope_receipts." + key, records)
    evidence = _object(document["evidence"], "evidence")
    for key, receipt in evidence.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", key):
            _fail("evidence", "invalid evidence key: " + key)
        _receipt(receipt, "evidence." + key, records, "original_scope")
    tasks = _list(document["tasks"], "tasks", nonempty=True)
    identifiers, referenced = [], set()
    for number, task in enumerate(tasks):
        location = f"tasks[{number}]"
        _object(task, location, {"task_id", "status", "validation_scope",
                                 "evidence_keys"})
        identifiers.append(_text(task["task_id"], location + ".task_id"))
        keys = _list(task["evidence_keys"], location + ".evidence_keys",
                     nonempty=True)
        for key in keys:
            _text(key, location + ".evidence_keys")
            if key not in evidence:
                _fail(location + ".evidence_keys",
                      "unknown evidence key: " + key)
        if len(set(keys)) != len(keys):
            _fail(location + ".evidence_keys", "duplicate evidence key")
        referenced.update(keys)
    if tuple(identifiers) != TASK_IDS:
        _fail("tasks", "expected exact original 58 task IDs and order")
    for task, row in zip(tasks, rows):
        for target, authority in (("status", "validation_status"),
                                  ("validation_scope", "validation_scope")):
            _equal(task[target], row[authority],
                   "tasks." + task["task_id"] + "." + target)
        _equal(task["status"], "passed", "tasks." + task["task_id"] + ".status")
        _equal(task["validation_scope"], "accepted_docs_exit",
               "tasks." + task["task_id"] + ".validation_scope")
    if evidence.keys() - referenced:
        _fail("evidence", "unreferenced evidence keys: " +
              ", ".join(sorted(evidence.keys() - referenced)))
    pending = _list(document["formal_evidence_required_for"],
                    "formal_evidence_required_for")
    expected = [row["task_id"] for row in rows
                if row["validation_status"] != "passed"]
    _equal(pending, expected, "formal_evidence_required_for")
    retention = _object(document["retention"], "retention", {
        "status", "channel", "policy", "restoration_scope"})
    _equal(retention["status"], "remote_retrieved_and_verified",
           "retention.status")
    for field in ("channel", "policy", "restoration_scope"):
        _text(retention[field], "retention." + field)
    limitations = _object(document["limitations"], "limitations", {
        "browser_tuple", "file_uri", "translation",
        "environment_branch_policy_api"})
    for field, value in limitations.items():
        _text(value, "limitations." + field)
    physical = _object(document["physical_hil"], "physical_hil", {
        "status", "changed_by_documentation"})
    _equal(physical["status"], "not_executed", "physical_hil.status")
    _equal(physical["changed_by_documentation"], False,
           "physical_hil.changed_by_documentation")
    _text(document["qualification_boundary"], "qualification_boundary")


def validate(root):
    """Return offline schema/authority errors without Git or archive access.

    A missing index is an error when this API is explicitly requested. Callers
    scanning minimal documentation fixtures can choose whether an index exists
    before invoking it. Ordinary commits check local bytes and declared closure;
    immutable archive retrieval and member verification remain delivery work.
    """
    try:
        root = Path(root).absolute()
        path = _file(root, INDEX_PATH, "index")
        _validate(root, _json(path, "index"))
        return []
    except (_Invalid, OSError, UnicodeError, csv.Error, ValueError) as error:
        return [INDEX_PATH + ": " + str(error)]
