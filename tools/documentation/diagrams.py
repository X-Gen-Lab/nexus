"""Safe path-based SVG rendering and read-only source/output verification.

DOT is the authored source. Cairo converts labels to local glyph paths so a
reader needs neither Graphviz nor the build host's fonts. Accessible title and
node/edge descriptions preserve the text separately. Rendering proves byte
consistency only; independent source review establishes diagram semantics.
"""

from __future__ import annotations

import csv
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

SVG_NAMESPACE = "http://www.w3.org/2000/svg"
XLINK_NAMESPACE = "http://www.w3.org/1999/xlink"
LABEL = re.compile(r'label\s*=\s*"((?:\\.|[^"\\])*)"', re.DOTALL)
FORBIDDEN = {"script", "foreignObject", "animate", "animateMotion",
             "animateTransform", "set"}
ET.register_namespace("", SVG_NAMESPACE)
ET.register_namespace("xlink", XLINK_NAMESPACE)


def _regular(path):
    path = Path(path).absolute()
    if any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError("diagram symlink path rejected: " + str(path))
    if not path.is_file():
        raise ValueError("diagram regular file missing: " + str(path))
    return path


def validate_source(contents):
    """Forbid external input/resources before invoking the DOT parser."""
    if (re.search(r"<\s*IMG\b", contents, re.IGNORECASE) or
            re.search(r"\b(?:image|shapefile|imagepath|fontpath|stylesheet|"
                      r"href|URL|target)\s*=", contents, re.IGNORECASE)):
        raise ValueError("DOT cannot load images, files or external resources")
    if not re.search(r"\b(?:strict\s+)?(?:di)?graph\b", contents):
        raise ValueError("DOT graph declaration missing")


def _css(contents):
    # Comments do not create dependencies. Decode escapes before identifying
    # url()/@import so escaped syntax cannot evade offline verification.
    contents = re.sub(r"/\*.*?\*/", "", contents, flags=re.DOTALL)
    contents = re.sub(r"\\(?:\r\n|[\r\n\f])", "", contents)
    def unescape(match):
        if match[1]:
            value = int(match[1], 16)
            if not 0 < value <= 0x10ffff or 0xd800 <= value <= 0xdfff:
                raise ValueError("Offline CSS escape contains an invalid codepoint")
            return chr(value)
        return match[2]
    contents = re.sub(r"\\([0-9a-fA-F]{1,6})(?:\r\n|[ \t\r\n\f])?"
                      r"|\\([^\r\n\f])", unescape, contents)
    expressions = (r"url\(\s*([^)]*?)\s*\)",
                   r"@import\s+([\"'][^\"']+[\"'])")
    links = []
    for expression in expressions:
        for match in re.finditer(expression, contents, re.IGNORECASE):
            link = match[1].strip().strip("\"'")
            if "\\" in link:
                raise ValueError("Offline CSS resource link contains an escape")
            if link:
                links.append(link)
    return links


def _local_css(contents, identifiers):
    # Decode escapes before enforcing the local SVG resource policy.
    try:
        resources = _css(contents)
    except ValueError as error:
        raise ValueError("SVG CSS resource syntax rejected") from error
    for resource in resources:
        if not resource.startswith("#") or resource[1:] not in identifiers:
            raise ValueError("SVG cannot contain external/missing CSS resources")


def validate_svg(contents):
    """Allow local glyph definitions while rejecting active/external content."""
    if (b"<!DOCTYPE" in contents or b"<!ENTITY" in contents or
            b"<?xml-stylesheet" in contents):
        raise ValueError("SVG cannot contain DOCTYPE, entities or stylesheets")
    try:
        tree = ET.fromstring(contents)
    except (ET.ParseError, UnicodeError) as error:
        raise ValueError("SVG renderer output is malformed") from error
    if tree.tag != "{" + SVG_NAMESPACE + "}svg":
        raise ValueError("SVG requires the exact root namespace")
    try:
        viewport = tuple(float(value) for value in tree.attrib["viewBox"].split())
    except (KeyError, ValueError) as error:
        raise ValueError("SVG requires a numeric viewBox") from error
    if (len(viewport) != 4 or
            not all(math.isfinite(value) for value in viewport) or
            viewport[2] <= 0 or viewport[3] <= 0):
        raise ValueError("SVG requires a finite positive viewBox")
    declared = [node.get("id") for node in tree.iter() if node.get("id")]
    identifiers = set(declared)
    if len(declared) != len(identifiers):
        raise ValueError("SVG identifiers must be unique")
    for node in tree.iter():
        if node.tag.split("}")[-1] in FORBIDDEN:
            raise ValueError("SVG contains executable or unsupported elements")
        for key, value in node.attrib.items():
            name = key.split("}")[-1].lower()
            if name.startswith("on"):
                raise ValueError("SVG cannot contain event handlers")
            if name in {"href", "src"}:
                if not value.startswith("#") or value[1:] not in identifiers:
                    raise ValueError("SVG cannot contain external/missing resources")
            _local_css(value, identifiers)
        if node.tag.split("}")[-1] == "style" and node.text:
            _local_css(node.text, identifiers)
    return tree


def render(source, env=None):
    """Return canonical SVG bytes; do not write source, SVG or the registry.

    The caller supplies install.environment(prefix) for formal builds. An
    explicit ``dot`` key is accepted for a verified tool or test boundary. No
    tools are downloaded or installed implicitly. Execution has a 30 s budget.
    """
    source = _regular(source)
    contents = source.read_text(encoding="utf-8")
    validate_source(contents)
    environment = dict(os.environ if env is None else env)
    executable = environment.pop("dot", None) or shutil.which(
        "dot", path=environment.get("PATH", ""))
    if not executable:
        raise ValueError("diagram renderer is unavailable")
    try:
        result = subprocess.run([str(executable), "-Tsvg:cairo"],
                                input=contents.encode("utf-8"), env=environment,
                                capture_output=True, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValueError("diagram renderer could not complete") from error
    if result.returncode != 0 or result.stderr:
        raise ValueError("diagram renderer failed: " + result.stderr.decode(
            "utf-8", errors="replace")[:2048])
    tree = validate_svg(result.stdout)
    if any(node.tag.split("}")[-1] == "text" for node in tree.iter()):
        raise ValueError("SVG Cairo renderer must embed glyph paths")
    labels = [label.replace("\\n", "\n").replace("\\l", "\n").replace(
        "\\r", "\n").replace('\\"', '"') for label in LABEL.findall(contents)
        if label]
    title = ET.Element("{" + SVG_NAMESPACE + "}title", id=source.stem + "-title")
    title.text = labels[0] if labels else source.stem
    description = ET.Element("{" + SVG_NAMESPACE + "}desc",
                             id=source.stem + "-description")
    description.text = "; ".join(labels[1:])
    tree.insert(0, description)
    tree.insert(0, title)
    tree.set("role", "img")
    tree.set("aria-labelledby", title.get("id") + " " + description.get("id"))
    ET.register_namespace("", SVG_NAMESPACE)
    ET.register_namespace("xlink", XLINK_NAMESPACE)
    return ET.tostring(tree, encoding="utf-8", xml_declaration=True) + b"\n"


def _registry(folder):
    path = _regular(folder / "diagram-index.csv")
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if not {"diagram_id", "source_dot", "rendered_svg"}.issubset(
                reader.fieldnames or ()):
            raise ValueError("diagram registry fields missing")
        rows = list(reader)
    if not rows:
        raise ValueError("diagram registry cannot be empty")
    identities, sources = set(), set()
    for row in rows:
        identity = row["diagram_id"]
        if not re.fullmatch(r"FIG-[0-9]{2}", identity) or identity in identities:
            raise ValueError("diagram registry identity invalid or duplicated")
        identities.add(identity)
        dot, svg = row["source_dot"], row["rendered_svg"]
        if (not dot or not svg or Path(dot).name != dot or Path(svg).name != svg
                or Path(dot).suffix != ".dot" or Path(svg).suffix != ".svg"
                or Path(dot).stem != Path(svg).stem or dot in sources):
            raise ValueError("diagram registry source/output path invalid")
        sources.add(dot)
    return rows


def check(root, *, env=None, full=False):
    """Verify registered and supplemental pairs without changing any file.

    Fast checks use no renderer. Full checks additionally compare canonical
    output from the supplied tools with each checked-in SVG, and reject stale
    bytes. Neither mode claims independent semantic or physical acceptance.
    """
    folder = Path(root).absolute() / "docs/assets/diagrams"
    errors, checked = [], 0
    try:
        rows = _registry(folder)
    except ValueError as error:
        return {"status": "failed", "diagrams": 0, "errors": [str(error)]}
    declared = {row["source_dot"] for row in rows}
    for name in sorted(declared | {path.name for path in folder.glob("*.dot")}):
        source = folder / name
        output = source.with_suffix(".svg")
        try:
            _regular(source)
            validate_source(source.read_text(encoding="utf-8"))
            if not output.is_file():
                raise ValueError("missing SVG pair: " + output.name)
            _regular(output)
            actual = output.read_bytes()
            validate_svg(actual)
            if full and render(source, env) != actual:
                raise ValueError("stale SVG differs from DOT/tool output: "
                                 + output.name)
            checked += 1
        except (ValueError, OSError, UnicodeError) as error:
            errors.append(name + ": " + str(error))
    for output in folder.glob("*.svg"):
        if not output.with_suffix(".dot").is_file():
            errors.append("missing DOT pair: " + output.name)
    from tools.documentation import diagram_localization
    localized = diagram_localization.check(folder, env=env, full=full)
    errors.extend(localized["errors"])
    return {"status": "failed" if errors else "passed", "diagrams": checked,
            "localized_diagrams": localized["localized_diagrams"],
            "errors": errors, "render_check": bool(full)}
