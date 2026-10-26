"""Resolve authored Markdown examples; explicit fragments remain unqualified."""
from __future__ import annotations

import json
from pathlib import Path
import re
import tempfile
import tomllib

from tools.configure import authored, configure, runtime

FENCE = re.compile(r"^```(?P<language>toml|json)[ \t]*\n"
                   r"(?P<body>.*?)^```[ \t]*$", re.MULTILINE | re.DOTALL)
FRAGMENT = re.compile(r"<!-- nexus-toml-fragment:\s*(.*?)\s*-->$")


def _runtime(block, following, document, temporary):
    relative = block.get("cpu_facts")
    if (not isinstance(relative, str) or not relative or
            Path(relative).is_absolute() or ".." in Path(relative).parts):
        raise ValueError("Runtime facts must be a contained relative path")
    facts = document.parent / relative
    if any(path.is_symlink() for path in (facts, *facts.parents)):
        raise ValueError("Runtime documentation facts must not be a symlink")
    if facts.is_file():
        contents = facts.read_bytes()
    else:
        match = FENCE.search(following)
        if match is None or match["language"] != "json":
            raise ValueError("Runtime example requires adjacent JSON facts")
        contents = match["body"].encode("utf-8")
    destination = temporary / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(contents)


def check_document(document, root=configure.ROOT, *, contents=None):
    """Run production resolution; a fragment marker requires a stated reason."""
    document = Path(document)
    contents = document.read_text(encoding="utf-8") if contents is None else contents
    openings = len(re.findall(r"^```toml[ \t]*$", contents, re.MULTILINE))
    matches = [match for match in FENCE.finditer(contents)
               if match["language"] == "toml"]
    if openings != len(matches):
        raise ValueError(f"TOML documentation has an unterminated fence: {document}")
    records = []
    for match in matches:
        line = contents.count("\n", 0, match.start()) + 1
        label = f"{document}:{line}"
        try:
            block = tomllib.loads(match["body"])
            prefix = contents[:match.start()].rstrip().splitlines()
            fragment = FRAGMENT.fullmatch(prefix[-1]) if prefix else None
            if fragment:
                reason = fragment[1].strip()
                if not reason:
                    raise ValueError("TOML fragment marker requires a reason")
                records.append({"document": str(document), "line": line,
                                "kind": "fragment", "reason": reason})
                continue
            with tempfile.TemporaryDirectory(prefix="nexus-doc-config-") as name:
                temporary = Path(name)
                if "schema" in block:
                    # Preserve authored validation before using the explicit
                    # legacy projection to relocate external package paths.
                    normalized = authored.normalize(
                        block, document, root, configure.fail, configure.obj,
                        configure.integer, configure.text)
                    package = Path(normalized["board_package"])
                    if not package.is_absolute():
                        normalized["board_package"] = str(document.parent / package)
                    if normalized.get("layout") is not None:
                        layout = Path(normalized["layout"])
                        if not layout.is_absolute():
                            normalized["layout"] = str(document.parent / layout)
                    assembly = temporary / "assembly.json"
                    assembly.write_text(json.dumps(normalized), encoding="utf-8")
                    result = configure.resolve(assembly, root)
                    digest, kind = result["configuration_sha256"], "assembly"
                elif "cpu_facts" in block:
                    _runtime(block, contents[match.end():], document, temporary)
                    assembly = temporary / "runtime.toml"
                    assembly.write_text(match["body"], encoding="utf-8")
                    result = runtime.resolve(assembly)
                    digest, kind = result.configuration_sha256, "runtime"
                else:
                    raise ValueError("Partial TOML requires an explicit fragment marker")
            records.append({"document": str(document), "line": line,
                            "kind": kind, "configuration_sha256": digest})
        except (OSError, ValueError, TypeError) as error:
            raise ValueError(f"TOML documentation rejected at {label}: {error}") from error
    return records


def check(root=configure.ROOT):
    """Check maintained design/delivery/configuration docs, never archives."""
    root = Path(root).resolve()
    paths = {root / "tools/configure/README.md"}
    for directory in ("docs/design", "docs/delivery"):
        paths.update((root / directory).rglob("*.md"))
    records = []
    for path in sorted(paths):
        if path.is_file():
            records.extend(check_document(path, root))
    return records
