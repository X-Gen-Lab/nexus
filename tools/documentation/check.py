"""Offline documentation checks and bounded, explicitly requested URL probes."""

import csv
import json
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

EXCLUDED = {"archive", "locale", "_build", ".doctrees"}
LEGACY_SPHINX = {"index.rst", "api/index.rst", "api.rst", "getting_started.rst",
                 "architecture.rst", "configuration.rst", "migration.rst",
                 "os.rst", "support.rst", "development/index.rst"}
LINK = re.compile(r"!?\[[^\]]*\]\((<[^>]+>|[^)]+)\)")
REFERENCE = re.compile(r"^\s*\[[^\]]+\]:\s*(\S+)", re.MULTILINE)
FENCE = re.compile(r"^(`{3,}|~{3,})([^\n]*)\n(.*?)^\1\s*$",
                   re.MULTILINE | re.DOTALL)
CRITICAL_PAGES = ("index", "tutorials/source-sdk", "tutorials/async-io",
                  "guides/board-porting", "guides/os-shutdown", "guides/migration",
                  "reference/configuration/assembly", "explanation/factory",
                  "explanation/ownership", "reference/platforms/support",
                  "reference/platforms/qualification")


def metadata(contents):
    """Read only the small authored metadata fields, not another config DSL."""
    if not contents.startswith("---\n"):
        return {}
    block, separator, _ = contents[4:].partition("\n---")
    if not separator:
        raise ValueError("unterminated page metadata")
    result = {}
    for line in block.splitlines():
        match = re.fullmatch(r"([a-z_]+):\s*(.*?)\s*", line)
        if match and match[1] in {"id", "type", "audience", "source_language"}:
            if match[1] in result:
                raise ValueError("duplicate metadata field: " + match[1])
            result[match[1]] = match[2].strip('"\'')
    if result.get("source_language", "en") not in {"en", "zh_CN"}:
        raise ValueError("unsupported source_language")
    return result


def configuration_content(contents, suffix):
    """Use standard RST code-block directives with the same production parser."""
    if suffix != ".rst":
        return contents
    lines, result, index = contents.splitlines(), [], 0
    while index < len(lines):
        match = re.match(r"^\.\. code-block::\s*(toml|json)\s*$", lines[index])
        if not match:
            result.append(lines[index]); index += 1
            continue
        language = match[1]
        index += 1
        while index < len(lines) and (not lines[index].strip() or
                                     lines[index].lstrip().startswith(":")):
            index += 1
        body = []
        while index < len(lines) and (not lines[index].strip() or
                                     lines[index].startswith((" ", "\t"))):
            body.append(lines[index]); index += 1
        import textwrap
        result.extend(["```" + language, textwrap.dedent("\n".join(body)).rstrip(), "```"])
    return "\n".join(result) + "\n"


def is_source(root, path):
    docs = root / "docs"
    return (path.is_relative_to(docs) and path.suffix in {".md", ".rst"}
            and (path.name != "README.md" or path == docs / "assets/diagrams/README.md")
            and path.relative_to(docs).parts[0] != "api"
            and not EXCLUDED.intersection(path.relative_to(docs).parts)
            and not (path.is_relative_to(docs / "sphinx") and
                     str(path.relative_to(docs / "sphinx")) in LEGACY_SPHINX))


def sources(root):
    return sorted(path for path in (root / "docs").rglob("*") if is_source(root, path))


def anchors(contents):
    """Check authored labels and heading fragments; Sphinx checks full roles."""
    result = set(re.findall(r"^\(([^)]+)\)=\s*$", contents, re.MULTILINE))
    result.update(re.findall(r"^\.\. _([^:]+):\s*$", contents, re.MULTILINE))
    result.update(re.findall(r'\bid=["\']([^"\']+)["\']', contents))
    used = {}
    for line in contents.splitlines():
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
        if match:
            slug = re.sub(r"[^\w\s-]", "", match[1].lower()).strip()
            slug = re.sub(r"\s+", "-", slug)
            number = used.get(slug, 0)
            used[slug] = number + 1
            result.add(slug if not number else f"{slug}-{number}")
    return result


def navigation(path, contents):
    blocks = re.findall(r"```\{toctree\}[^\n]*\n(.*?)^```\s*$",
                        contents, re.MULTILINE | re.DOTALL)
    blocks += re.findall(r"^\.\. toctree::\s*\n((?:[ \t]+.*\n|\n)+)",
                         contents, re.MULTILINE)
    entries = []
    for block in blocks:
        for line in block.splitlines():
            entry = line.strip()
            if not entry or entry.startswith(":"):
                continue
            match = re.search(r"<([^>]+)>$", entry)
            entry = match[1] if match else entry
            if "://" in entry or entry == "self":
                continue
            destination = path.parent / entry
            if any(character in entry for character in "*?["):
                entries.extend(item.resolve() for item in path.parent.glob(entry)
                               if item.suffix in {".md", ".rst"}
                               and item != path and item.name != "README.md")
                continue
            if not destination.suffix:
                destination = next((destination.with_suffix(suffix)
                                    for suffix in (".md", ".rst")
                                    if destination.with_suffix(suffix).is_file()),
                                   destination.with_suffix(".md"))
            entries.append(destination.resolve())
    return entries


def validate_svg(path):
    from tools.documentation import diagrams
    diagrams.validate_svg(path.read_bytes())


def validate_ledger(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    identifiers = [row["task_id"] for row in rows]
    if len(set(identifiers)) != len(rows):
        raise ValueError("duplicate task ID")
    tasks = {row["task_id"]: row for row in rows}
    visiting, visited = set(), set()

    def visit(task):
        if task in visiting:
            raise ValueError("documentation dependency cycle")
        if task in visited:
            return
        visiting.add(task)
        for dependency in filter(None, tasks[task]["dependencies"].split(";")):
            if dependency not in tasks:
                raise ValueError("missing documentation dependency: " + dependency)
            visit(dependency)
        visiting.remove(task)
        visited.add(task)

    for task in tasks:
        visit(task)
    return rows


def validate_translations(root, *, pot_root=None, languages=("en", "zh_CN"),
                          critical_pages=CRITICAL_PAGES):
    """Critical paths require reviewed translations; other fallback stays visible."""
    from tools.documentation import sphinx_ext
    root = Path(root)
    records, errors = [], []
    for docname in critical_pages:
        page = root / "docs" / (docname + ".md")
        if not page.is_file():
            errors.append("missing critical page: " + docname)
            continue
        source = page.read_text()
        messages = ()
        if pot_root is not None:
            pot = Path(pot_root) / (docname + ".pot")
            if not pot.is_file():
                errors.append("missing critical gettext output: " + docname)
                continue
            messages = tuple(sphinx_ext.read_catalog(pot)[1])
        for language in languages:
            status = sphinx_ext.translation_status(source, docname, language,
                                                   root / "docs/locale", messages)
            records.append({"page": docname, **status})
            if status["status"] not in {"original", "reviewed"}:
                errors.append(f"critical translation {docname}/{language}: {status['status']}")
    return {"errors": errors, "records": records,
            "status": "failed" if errors else "passed"}


def external_links(urls, *, timeout=5, retries=1):
    """Separate HTTP failures from unreachable destinations with finite budgets."""
    if not 0 < timeout <= 30 or not 0 <= retries <= 2:
        raise ValueError("external URL budgets must be bounded")
    records = []
    for url in sorted(set(urls)):
        if urllib.parse.urlsplit(url).scheme not in {"http", "https"}:
            continue
        for attempt in range(retries + 1):
            request = urllib.request.Request(url, headers={
                "User-Agent": "Nexus-documentation-validation/1"})
            try:
                with urllib.request.urlopen(request, timeout=timeout) as response:
                    record = {"url": url, "status": "passed",
                              "http_status": response.status, "attempts": attempt + 1}
                break
            except urllib.error.HTTPError as error:
                record = {"url": url, "status": "http_error",
                          "http_status": error.code, "attempts": attempt + 1}
                break
            except (urllib.error.URLError, TimeoutError, OSError) as error:
                record = {"url": url, "status": "network_error",
                          "error": str(error), "attempts": attempt + 1}
        records.append(record)
    return records


def scan(root, *, check_diagrams=True, check_configuration=True,
         check_navigation=True, external=False):
    root = Path(root).resolve()
    errors, identifiers, urls, graph, config_records = [], {}, set(), {}, []
    pages = sources(root)
    for path in pages:
        label = str(path.relative_to(root))
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents):
            errors.append(label + ": source symlink rejected")
            continue
        contents = path.read_text()
        try:
            meta = metadata(contents)
            labels = re.findall(r"^\(([^)]+)\)=\s*$", contents, re.MULTILINE)
            labels += re.findall(r"^\.\. _([^:]+):\s*$", contents, re.MULTILINE)
            if len(set(labels)) != len(labels):
                errors.append(label + ": duplicate anchor")
            identifier = meta.get("id")
            if identifier in identifiers:
                errors.append(label + ": duplicate page ID " + identifier)
            if identifier:
                identifiers[identifier] = label
            graph[path.resolve()] = navigation(path, contents)
            for target in graph[path.resolve()]:
                if not target.is_file():
                    errors.append(label + ": missing toctree target " + str(target))
            includes = re.findall(r"```\{literalinclude\}\s+([^\n]+)", contents)
            includes += re.findall(r"^\.\. literalinclude::\s*(\S+)",
                                   contents, re.MULTILINE)
            for include in includes:
                destination = (path.parent / include.strip()).resolve()
                if not destination.is_file() or not destination.is_relative_to(root):
                    errors.append(label + ": missing/escaping literalinclude " + include)
                elif check_configuration and destination.suffix == ".toml":
                    from tools.configure import configure, runtime
                    import tomllib
                    raw = tomllib.loads(destination.read_text())
                    result = (configure.resolve(destination, root) if "schema" in raw
                              else runtime.resolve(destination))
                    config_records.append({"source": str(destination), "kind": "literalinclude",
                                           "resolved": True})
            plain = FENCE.sub("", contents)
            targets = [match[1] for match in LINK.finditer(plain)]
            targets += REFERENCE.findall(plain)
            for target in targets:
                target = target.strip().split(' "', 1)[0].strip("<>")
                parsed = urllib.parse.urlsplit(target)
                if parsed.scheme or target.startswith("//"):
                    if parsed.scheme in {"http", "https"}:
                        urls.add(target)
                    continue
                destination = (path.parent / urllib.parse.unquote(parsed.path)).resolve()
                if not parsed.path:
                    destination = path
                if not destination.exists():
                    errors.append(label + ": missing link " + target)
                elif (destination.is_file() and parsed.fragment
                      and destination.suffix in {".md", ".rst"}):
                    if urllib.parse.unquote(parsed.fragment) not in anchors(destination.read_text()):
                        errors.append(label + ": missing anchor " + target)
            normalized = configuration_content(contents, path.suffix)
            if check_configuration and "```toml" in normalized:
                from tools.configure import documentation
                config_records.extend(documentation.check_document(path, root, contents=normalized))
            if check_configuration:
                from tools.configure.metadata import pairs
                for block in FENCE.finditer(normalized):
                    if block[2].strip() == "json":
                        json.loads(block[3], object_pairs_hook=pairs)
        except (OSError, ValueError, TypeError, KeyError) as error:
            errors.append(label + ": " + str(error))
    if check_navigation:
        start = (root / "docs/index.md").resolve()
        reachable, pending = set(), [start]
        while pending:
            page = pending.pop()
            if page not in reachable:
                reachable.add(page)
                pending.extend(graph.get(page, []))
        for page in graph.keys() - reachable:
            errors.append(str(page.relative_to(root)) + ": unreachable from site navigation")
    if check_diagrams:
        registry = root / "docs/assets/diagrams/diagram-index.csv"
        if registry.is_file():
            with registry.open() as stream:
                diagram_rows = list(csv.DictReader(stream))
            for row in diagram_rows:
                if row["render_status"] != "rendered":
                    errors.append(row["diagram_id"] + ": required diagram not rendered")
                    continue
                for field in ("source_dot", "rendered_svg"):
                    path = registry.parent / row[field]
                    if not row[field] or not path.is_file():
                        errors.append(row["diagram_id"] + ": missing diagram pair")
                try:
                    validate_svg(registry.parent / row["rendered_svg"])
                except (OSError, ValueError, ET.ParseError) as error:
                    errors.append(row["diagram_id"] + ": " + str(error))
    ledger = root / "docs/design/documentation-system-execution.csv"
    if ledger.is_file():
        try:
            rows = validate_ledger(ledger)
            view = root / "docs/design/documentation-improvements.md"
            if view.is_file():
                contents = view.read_text()
                for row in rows:
                    expected = "| " + " | ".join(row[key].replace("|", "\\|") for key in
                        ("task_id", "priority", "task", "exit_condition")) + " |"
                    if contents.splitlines().count(expected) != 1:
                        errors.append(row["task_id"] + ": checklist differs from ledger")
        except (OSError, ValueError, KeyError) as error:
            errors.append(str(error))
    probes = external_links(urls) if external else []
    return {"schema_version": 1, "kind": "nexus_documentation_source_check",
            "status": "failed" if errors else "passed", "errors": errors,
            "pages": len(pages), "page_ids": len(identifiers),
            "configuration_examples": config_records,
            "external_links": probes, "qualification": False}
