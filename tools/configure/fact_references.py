"""Reviewed hardware references are source identities, never qualification."""
from collections.abc import Mapping
import re
from pathlib import PurePosixPath
from urllib.parse import urlparse

from tools.configure.providers.common import fail, integer
from tools.configure import reference_profiles


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
    fields(value, {"profile", "document_sha256", "revision_order",
                   "maintained_modes", "items"}, "audit profile")
    profile = reference_profiles.select(value["profile"])
    if ((document["document_id"], document["revision"], document["sha256"]) !=
            (profile.document_id, profile.revision, profile.sha256) or
            value["document_sha256"] != document["sha256"]):
        fail("Reference audit profile document identity differs from its review")
    if value["revision_order"] != list(profile.revision_order):
        fail("Reference audit revision order differs from its profile")
    modes = value["maintained_modes"]
    if (not isinstance(modes, list) or not modes or
            any(not isinstance(mode, str) or
                not re.fullmatch(r"[a-z][a-z0-9_]*:[a-z0-9][a-z0-9-]*", mode)
                for mode in modes) or modes != sorted(set(modes))):
        fail("Reference audit maintained modes require a sorted exact catalog")
    sections = profile.sections
    rows = value["items"]
    if (not isinstance(rows, list) or len(rows) != len(sections) or
            any(not isinstance(row, dict) for row in rows)):
        fail("Reference audit must assess every section in its profile")
    observed = []
    for row in rows:
        fields(row, {"section", "title", "revisions", "assessment",
                     "condition", "handling", "implementation",
                     "regressions"}, "audit item")
        section = row["section"]
        if not isinstance(section, str) or section not in sections:
            fail("Reference audit contains a foreign section")
        observed.append(section)
        if row["revisions"] != profile.applicability[section]:
            fail("Reference audit applicability differs from its profile")
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
    if not isinstance(documents, dict) or not documents:
        fail("Reference documents require a named source index")
    kinds = []
    for key, document in documents.items():
        text(key, "Reference document name")
        fields(document, {"kind", "document_id", "revision", "document_date",
                          "url", "sha256", "bytes", "anchors"}, "document")
        if document["kind"] not in {"datasheet", "reference_manual", "errata"}:
            fail("Reference document kind is not maintained")
        kinds.append(document["kind"])
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
    if (kinds.count("reference_manual") != 1 or kinds.count("errata") != 1 or
            "datasheet" not in kinds):
        fail("Reference documents require datasheets, one manual and one errata")
    packages = data["package_review"]
    if not isinstance(packages, list) or not packages:
        fail("Reference package review requires explicit document/part groups")
    reviewed_parts = set()
    for package in packages:
        fields(package, {"document", "variants", "status", "scope"}, "package")
        name = package["document"]
        if (not isinstance(name, str) or name not in documents or
                documents[name]["kind"] != "datasheet"):
            fail("Reference package review must cite an indexed datasheet")
        parts = package["variants"]
        if (not isinstance(parts, list) or not parts or
                any(not isinstance(part, str) for part in parts) or
                len(set(parts)) != len(parts) or not set(parts).issubset(variants) or
                reviewed_parts.intersection(parts)):
            fail("Reference package variants require disjoint maintained parts")
        profile = reference_profiles.datasheet(documents[name])
        for part in parts:
            variant = variants[part]
            device = variant.get("device") if isinstance(variant, Mapping) else None
            if (not isinstance(device, str) or device not in profile.devices or
                    not part.casefold().startswith(device)):
                fail("Reference datasheet does not cover the exact part/device")
        reviewed_parts.update(parts)
        if package["status"] != "package_gpio_reviewed":
            fail("Reference package review scope cannot establish other facts")
        text(package["scope"], "Reference package scope")
    if reviewed_parts != set(variants):
        fail("Reference package variants do not cover exact maintained parts")
    errata = data["errata_review"]
    fields(errata, {"document", "status", "silicon_revision",
                    "mode_workarounds",
                    "hardware_qualification", "identification",
                    "software_audit"}, "errata")
    name = errata["document"]
    if (not isinstance(name, str) or name not in documents or
            documents[name]["kind"] != "errata"):
        fail("Reference errata review must cite the indexed errata document")
    required_scope = {"status": "software_audited_with_constraints",
                      "silicon_revision": "not_observed",
                      "mode_workarounds": "audited_with_constraints",
                      "hardware_qualification": "not_executed"}
    if any(errata[key] != value for key, value in required_scope.items()):
        fail("Reference errata scope cannot establish physical qualification")
    text(errata["identification"], "Reference silicon identification")
    software_audit(errata["software_audit"], documents[name])
    limits = data["limitations"]
    if not isinstance(limits, list) or not limits:
        fail("Reference review must retain its qualification limitations")
    for limit in limits:
        text(limit, "Reference qualification limitation")
    return data
