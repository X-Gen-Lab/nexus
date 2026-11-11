"""Reviewed hardware references are source identities, never qualification."""
import re
from pathlib import PurePosixPath
from urllib.parse import urlparse

from tools.configure.providers.common import fail, integer


def text(value, context):
    if not isinstance(value, str) or not value.strip():
        fail(context + ": nonempty text required")


def fields(value, required, context):
    if not isinstance(value, dict) or set(value) != set(required):
        fail(f"Reference {context}: exact fields required")


def audit_sources(value, context, required=True):
    """Keep review links relative to the source SDK, never to transient logs."""
    if (not isinstance(value, list) or (required and not value) or
            any(not isinstance(name, str) for name in value) or
            len(value) != len(set(value))):
        fail("Reference audit " + context + ": source list required")
    for name in value:
        if (not isinstance(name, str) or not name or "\\" in name or
                PurePosixPath(name).is_absolute() or
                any(part in {"", ".", ".."} for part in name.split("/")) or
                name.startswith(("build/", "/"))):
            fail("Reference audit " + context + ": relative source required")


def software_audit(value, document):
    """A complete review records exclusions and constraints, never a pass bit."""
    fields(value, {"document_sha256", "revision_order", "maintained_modes",
                   "items"}, "audit")
    if document["document_id"] != "ES0182" or document["revision"] != "19":
        fail("Reference audit requires a reviewed ES0182 Rev19 document")
    if value["document_sha256"] != document["sha256"]:
        fail("Reference audit document identity differs from the locked errata")
    if value["revision_order"] != ["A", "Z", "1", "2", "Y", "4", "5", "6"]:
        fail("Reference audit revision order differs from ES0182 Table3")
    modes = value["maintained_modes"]
    if (not isinstance(modes, list) or not modes or
            any(not isinstance(mode, str) or
                not re.fullmatch(r"[a-z][a-z0-9_]*:[a-z0-9][a-z0-9-]*", mode)
                for mode in modes) or modes != sorted(set(modes))):
        fail("Reference audit maintained modes require a sorted exact catalog")
    counts = (3, 20, 3, 6, 1, 2, 4, 4, 6, 6, 9, 7, 1, 6, 2, 12)
    sections = {f"2.{group}.{item}" for group, count in enumerate(counts, 1)
                for item in range(1, count + 1)}
    rows = value["items"]
    if (not isinstance(rows, list) or len(rows) != len(sections) or
            any(not isinstance(row, dict) for row in rows)):
        fail("Reference audit must assess all 92 ES0182 sections")
    observed = []
    none = frozenset(("2.2.10", "2.2.18", "2.3.1", "2.4.1", "2.4.4",
                      "2.6.2", "2.7.3", "2.9.1", "2.9.5", "2.11.1",
                      "2.11.3", "2.11.4", "2.11.6", "2.13.1", "2.14.2",
                      "2.15.2", "2.16.2"))
    partial = frozenset(("2.2.14", "2.7.1", "2.7.4", "2.16.3", "2.16.7",
                         "2.16.10"))
    specific = {"2.2.1": "N-------", "2.2.2": "A-------",
                "2.2.8": "-NNNNNNN", "2.7.2": "DDDDDDDD"}
    for row in rows:
        fields(row, {"section", "title", "revisions", "assessment",
                     "condition", "handling", "implementation",
                     "regressions"}, "audit item")
        section = row["section"]
        if not isinstance(section, str) or section not in sections:
            fail("Reference audit contains a foreign section")
        observed.append(section)
        expected = specific.get(section, ("N" if section in none else
                                         "P" if section in partial else "A") * 8)
        if row["revisions"] != expected:
            fail("Reference audit applicability differs from ES0182 Table3/4")
        if row["assessment"] not in {"workaround", "excluded_mode",
                                      "external_constraint", "silicon_limit"}:
            fail("Reference audit requires an explicit software disposition")
        for name in ("title", "condition", "handling"):
            text(row[name], "Reference audit " + name)
        audit_sources(row["implementation"], "implementation")
        audit_sources(row["regressions"], "regressions",
                      required=row["assessment"] in {"workaround", "excluded_mode"})
    if set(observed) != sections:
        fail("Reference audit has duplicate or missing sections")


def validate(data, variants):
    """Validate source scope separately from physical silicon observations."""
    fields(data, {"schema_version", "documents", "package_review",
                  "errata_review", "limitations"}, "index")
    integer(data["schema_version"], 1, 1, "Reference schema")
    documents = data["documents"]
    fields(documents, {"datasheet", "reference_manual", "errata"}, "documents")
    for key, document in documents.items():
        fields(document, {"kind", "document_id", "revision", "document_date",
                          "url", "sha256", "bytes", "anchors"}, "document")
        if document["kind"] != key:
            fail("Reference document kind differs from its index")
        for field in ("document_id", "revision", "document_date", "url"):
            text(document[field], "Reference " + field)
        parsed = urlparse(document["url"])
        if (parsed.scheme != "https" or not parsed.hostname or
                parsed.username or parsed.password or parsed.fragment):
            fail("Reference URL requires an HTTPS source without credentials")
        if (not isinstance(document["sha256"], str) or
                not re.fullmatch(r"[0-9a-f]{64}", document["sha256"])):
            fail("Reference document requires an exact SHA256 identity")
        integer(document["bytes"], 1, context="Reference document bytes")
        anchors = document["anchors"]
        if not isinstance(anchors, dict) or not anchors:
            fail("Reference document requires reviewed section anchors")
        for anchor, value in anchors.items():
            text(anchor, "Reference anchor")
            text(value, "Reference section")
    package = data["package_review"]
    fields(package, {"document", "variants", "status", "scope"}, "package")
    if package["document"] != "datasheet":
        fail("Reference package review must cite the indexed datasheet")
    parts = package["variants"]
    if (not isinstance(parts, list) or not parts or
            any(not isinstance(part, str) for part in parts) or
            len(set(parts)) != len(parts) or set(parts) != set(variants)):
        fail("Reference package variants do not cover exact maintained parts")
    if package["status"] != "package_gpio_reviewed":
        fail("Reference package review scope cannot establish other facts")
    text(package["scope"], "Reference package scope")
    errata = data["errata_review"]
    fields(errata, {"document", "status", "silicon_revision",
                    "mode_workarounds",
                    "hardware_qualification", "identification",
                    "software_audit"}, "errata")
    required_scope = {"document": "errata",
                      "status": "software_audited_with_constraints",
                      "silicon_revision": "not_observed",
                      "mode_workarounds": "audited_with_constraints",
                      "hardware_qualification": "not_executed"}
    if any(errata[key] != value for key, value in required_scope.items()):
        fail("Reference errata scope cannot establish physical qualification")
    text(errata["identification"], "Reference silicon identification")
    software_audit(errata["software_audit"], documents["errata"])
    limits = data["limitations"]
    if not isinstance(limits, list) or not limits:
        fail("Reference review must retain its qualification limitations")
    for limit in limits:
        text(limit, "Reference qualification limitation")
    return data
