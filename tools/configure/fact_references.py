"""Reviewed hardware references are source identities, never qualification."""
import re
from urllib.parse import urlparse

try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


def text(value, context):
    if not isinstance(value, str) or not value.strip():
        fail(context + ": nonempty text required")


def fields(value, required, context):
    if not isinstance(value, dict) or set(value) != set(required):
        fail(f"Reference {context}: exact fields required")


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
                    "hardware_qualification", "identification"}, "errata")
    required_scope = {"document": "errata", "status": "applicability_only",
                      "silicon_revision": "not_observed",
                      "mode_workarounds": "not_fully_audited",
                      "hardware_qualification": "not_executed"}
    if any(errata[key] != value for key, value in required_scope.items()):
        fail("Reference errata scope cannot establish physical qualification")
    text(errata["identification"], "Reference silicon identification")
    limits = data["limitations"]
    if not isinstance(limits, list) or not limits:
        fail("Reference review must retain its qualification limitations")
    for limit in limits:
        text(limit, "Reference qualification limitation")
    return data
