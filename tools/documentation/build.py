"""Build preview or verified documentation in one owned, locked workspace."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import stat
import threading
import uuid

from tools.dev import process
from tools.dev.build_lock import build_lock
from tools.documentation import check, diagram_localization, diagrams, install
from tools.evidence.identity import observe_git_source


SCHEMA = 2
OWNER = ".nexus-documentation-output.json"
REPORT = "documentation-build.json"


def record(path, value):
    """Publish one complete JSON record without modifying an earlier record."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    try:
        temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def prepare_output(root, destination):
    """Admit an owned root; never remove its marker, lock or child contents."""
    root, destination = Path(root).resolve(), Path(destination).absolute()
    if any(path.is_symlink() for path in (destination, *destination.parents)):
        raise ValueError("documentation output cannot use a symlink")
    destination = destination.resolve()
    if (destination == root or root.is_relative_to(destination)
            or destination.is_relative_to(root / "docs")
            or (destination.is_relative_to(root)
                and destination.relative_to(root).parts[0] != "build")):
        raise ValueError("documentation output cannot overlap source inputs")
    api_roots = set(_api_roots(root))
    rendered = set(check.dependencies(root, require_files=False)) | api_roots
    if (any(path.resolve().is_relative_to(destination) for path in rendered)
            or any(destination.is_relative_to(path.resolve()) for path in api_roots)):
        raise ValueError("documentation output cannot remove rendered inputs")
    expected = {"schema_version": SCHEMA, "owner": "nexus-documentation",
                "root": str(root)}
    marker = destination / OWNER
    created = False
    try:
        destination.mkdir(parents=True, exist_ok=False)
        created = True
    except FileExistsError:
        pass
    if created:
        # A competing creator sees an uninitialized directory and must reject;
        # it cannot create a marker or clear the first writer's files.
        with marker.open("x") as stream:
            json.dump(expected, stream)
            stream.write("\n")
    elif (not marker.is_file() or marker.is_symlink()
          or json.loads(marker.read_text()) != expected):
        raise ValueError("existing output lacks current documentation ownership")
    for name in (OWNER, "site", "work", "cache", REPORT, ".nexus-build.lock"):
        path = destination / name
        if path.is_symlink():
            raise ValueError("documentation workspace cannot use a symlink")
        if path.is_file() and path.stat().st_nlink != 1:
            raise ValueError("documentation workspace cannot use a hardlink")
    return destination


def _validate_workspace(output):
    """Reject linked outputs before a producer can overwrite external bytes."""
    for path in output.rglob("*"):
        details = path.lstat()
        if stat.S_ISLNK(details.st_mode):
            raise ValueError("documentation workspace cannot use a symlink")
        if stat.S_ISREG(details.st_mode) and details.st_nlink != 1:
            raise ValueError("documentation workspace cannot use a hardlink")


def _clear(path):
    if path.is_symlink():
        raise ValueError("documentation child cannot use a symlink")
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def _hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True,
        separators=(",", ":")).encode()).hexdigest()


def _file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inventory(folder):
    if folder.is_symlink():
        raise ValueError("documentation artifact root contains a symlink")
    result = {}
    for path in sorted(folder.rglob("*")):
        if path.is_symlink():
            raise ValueError("documentation artifact contains a symlink")
        if path.is_file():
            if path.stat().st_nlink != 1:
                raise ValueError("documentation artifact contains a hardlink")
            result[path.relative_to(folder).as_posix()] = _file_hash(path)
    return result


def _api_roots(root):
    doxygen = root / "Doxyfile"
    if doxygen.is_file():
        text = doxygen.read_text().replace("\\\n", " ")
        for match in re.finditer(r"^INPUT\s*\+?=\s*(.*)$", text, re.MULTILINE):
            for entry in shlex.split(match[1], comments=True):
                yield root / entry


def inputs(root):
    """Observe authored, included, API and renderer inputs as actual bytes."""
    paths = set(check.sources(root)) | set(check.adjacent_sources(root))
    paths.update(check.dependencies(root))
    for name in ("Doxyfile", "LICENSE", "dependencies/platform-compatibility.json",
                 "dependencies/docs-tools.lock.json",
                 "dependencies/docs-python-linux-cp312.txt"):
        if (root / name).is_file():
            paths.add(root / name)
    for name in ("docs/assets", "docs/sphinx", "docs/locale", "tools/documentation",
                 "tools/configure", "boards", "soc"):
        for path in (root / name).rglob("*"):
            if (path.is_file() and "__pycache__" not in path.parts
                    and path.suffix in {".md", ".py", ".json", ".toml", ".po",
                                        ".dot", ".svg", ".csv", ".css", ".js", ".html"}
                    and not path.name.startswith("test_")):
                paths.add(path)
    for path in _api_roots(root):
        if path.is_dir():
            paths.update(path.rglob("*.h"))
        elif path.is_file():
            paths.add(path)
    result = {}
    for path in sorted(paths):
        if (not path.is_file() or path.is_symlink()
                or not path.resolve().is_relative_to(root)
                or any(p.is_symlink() for p in path.parents)):
            raise ValueError("invalid documentation input: " + str(path))
        result[path.relative_to(root).as_posix()] = _file_hash(path)
    return result


def execute(command, root, environment, workspace, stage, timeout, *, cancel=None):
    result = process.run(command, cwd=root, env=environment,
                         log=workspace / (stage + ".log"), timeout=timeout,
                         stage="documentation-" + stage, cancel_event=cancel,
                         reject_background=cancel is not None)
    if result.returncode:
        raise ValueError(f"{stage} failed with exit {result.returncode}; "
                         f"receipt: {result.receipt_path}")
    return str(result.receipt_path)


def _environment(context, root, source, mode, languages, xml, gettext):
    environment = context.environment()
    environment.update({"NEXUS_DOCS_MODE": mode,
        "NEXUS_DOCS_REVISION": source["commit"],
        "NEXUS_DOCS_CODE_REVISION": source["commit"],
        "NEXUS_DOCS_LANGUAGES": ",".join(dict.fromkeys(("en", *languages))),
        "NEXUS_DOCS_LANGUAGE": "en", "NEXUS_DOCS_DOXYGEN_XML": str(xml),
        "NEXUS_DOCS_GETTEXT_ROOT": str(gettext),
        "NEXUS_DOCS_TRANSLATION_ROOT": str(root / "docs/locale")})
    return environment


def extract_messages(root, work, context, source, *, mode="verify",
                     languages=("en", "zh_CN"), timeout=600, api=None,
                     generate_api=True, gettext_workspace=None,
                     changed_inputs=(), rebuild_all=False):
    """Generate real XML and POT once; expose them for translation maintenance."""
    root, work = Path(root).resolve(), Path(work)
    work.mkdir(parents=True, exist_ok=True)
    prefix = Path(context.identity["prefix"])
    api = Path(api or work / "api")
    gettext_work = Path(gettext_workspace or work / "gettext-work")
    environment = _environment(context, root, source, mode, languages,
                               api / "xml", gettext_work / "pot")
    environment.update(NEXUS_DOCS_CHANGED_INPUTS=json.dumps(list(changed_inputs)),
                       NEXUS_DOCS_REBUILD_ALL="1" if rebuild_all else "0")
    policy = json.loads((root / "dependencies/platform-compatibility.json").read_text())
    environment["NEXUS_DOCS_VERSION"] = policy["semantic_version"]
    def quoted(path):
        return '"' + str(path).replace("\\", "\\\\").replace('"', '\\"') + '"'
    config = work / "Doxyfile"
    config.write_text('@INCLUDE = ' + quoted(root / "Doxyfile") + '\n'
        + 'OUTPUT_DIRECTORY = ' + quoted(api) + '\nGENERATE_HTML = NO\n'
        + 'GENERATE_XML = YES\nPROJECT_NUMBER = "' + policy["semantic_version"] + '"\n')
    receipts = []
    if generate_api:
        receipts.append(execute([str(prefix / "bin/doxygen"), str(config)], root,
                                environment, work, "doxygen", timeout))
    command = [str(prefix / "venv/bin/python"), "-m", "sphinx", "-n", "-W",
               "--keep-going", "-c", str(root / "docs/sphinx"),
               "-d", str(gettext_work / "doctrees"), "-b", "gettext",
               str(root / "docs"), str(gettext_work / "pot")]
    receipts.append(execute(command, root, environment, work, "gettext", timeout))
    for name in changed_inputs:
        path = Path(name)
        if name.startswith("docs/") and path.suffix == ".md" and not (root / path).exists():
            (gettext_work / "pot" / path.relative_to("docs").with_suffix(".pot")).unlink(
                missing_ok=True)
    shutil.copytree(gettext_work / "pot", work / "gettext")
    environment["NEXUS_DOCS_GETTEXT_ROOT"] = str(work / "gettext")
    return environment, receipts


def _cached(folder, key):
    marker = folder / "cache-input.json"
    try:
        value = json.loads(marker.read_text())
        return (value["schema_version"] == SCHEMA and value["key"] == key
                and value["files"] == inventory(folder / "payload"))
    except (OSError, ValueError, KeyError, TypeError):
        return False


def _save_cache(folder, key):
    record(folder / "cache-input.json", {"schema_version": SCHEMA,
        "key": key, "files": inventory(folder / "payload")})


def build(root, destination, *, mode="verify", language="all", tool_prefix=None,
          timeout=600):
    if mode not in {"preview", "verify"} or language not in {"en", "zh_CN", "all"}:
        raise ValueError("unsupported documentation mode or language")
    root = Path(root).resolve()
    output = prepare_output(root, destination)
    with build_lock(output):
        _validate_workspace(output)
        languages = ("en", "zh_CN") if language == "all" else (language,)
        metadata = {"schema_version": SCHEMA, "kind": "nexus_documentation_build",
            "status": "running", "mode": mode,
            "scope": "all" if language == "all" else "locale:" + language,
            "languages": list(languages), "qualification": False}
        record(output / REPORT, metadata)
        site, work, cache = (output / name for name in ("site", "work", "cache"))
        receipts = []
        try:
            _clear(site)
            _clear(work)
            cache.mkdir(exist_ok=True)
            context = install.context(tool_prefix)
            source, observed = observe_git_source(root), inputs(root)
            metadata.update(source=source, source_hash=_hash(observed), tools=context.identity)
            environment = context.environment()
            prior = {}
            state = cache / "inputs.json"
            if mode == "preview" and state.is_file():
                try:
                    value = json.loads(state.read_text())
                except (OSError, ValueError):
                    value = None
                if isinstance(value, dict) and all(
                        isinstance(n, str) and not Path(n).is_absolute()
                        and ".." not in Path(n).parts and isinstance(h, str)
                        and re.fullmatch(r"[a-f0-9]{64}", h)
                        for n, h in value.items()):
                    prior = value
            changed = sorted(name for name in observed.keys() | prior.keys()
                             if observed.get(name) != prior.get(name))
            rebuild_all = any(not name.startswith("docs/") or
                              not name.endswith(".md") for name in changed)
            report = check.scan(root)
            record(work / "source-check.json", report)
            if report["errors"]:
                raise ValueError("documentation source check failed: " + "\n".join(report["errors"]))
            diagram_report = diagrams.check(root, env=environment, full=mode == "verify")
            if diagram_report["errors"]:
                raise ValueError("diagram verification failed: " + "\n".join(diagram_report["errors"]))
            tool_key = _hash({"tools": context.identity,
                "implementation": {n: h for n, h in observed.items()
                                   if n.startswith("tools/documentation/")}})
            api_names = {n: h for n, h in observed.items()
                         if n.endswith(".h") or n in {"Doxyfile", "dependencies/platform-compatibility.json"}}
            api_key = _hash({"files": api_names, "tools": tool_key})
            api_cache = cache / "api"
            api = work / "api"
            if mode == "preview" and _cached(api_cache, api_key):
                shutil.copytree(api_cache / "payload", api)
                metadata["api_cache_hit"] = True
            message_key = _hash({"api": api_key, "tools": tool_key,
                "inputs": {n: h for n, h in observed.items()
                           if not n.startswith("docs/locale/")
                           and not n.endswith((".dot", ".svg", ".labels.json"))}})
            message_cache = cache / "gettext"
            if (mode == "preview" and metadata.get("api_cache_hit")
                    and _cached(message_cache, message_key)):
                shutil.copytree(message_cache / "payload", work / "gettext")
                environment = _environment(context, root, source, mode, languages,
                                           api / "xml", work / "gettext")
                environment["NEXUS_DOCS_VERSION"] = json.loads((root /
                    "dependencies/platform-compatibility.json").read_text())["semantic_version"]
                metadata["gettext_cache_hit"] = True
            else:
                environment, extracted = extract_messages(root, work, context, source,
                    mode=mode, languages=languages, timeout=timeout,
                    generate_api=not metadata.get("api_cache_hit", False),
                    gettext_workspace=cache / "gettext-work" / tool_key
                        if mode == "preview" else None,
                    changed_inputs=changed, rebuild_all=rebuild_all)
                receipts.extend(extracted)
                if mode == "preview":
                    _clear(message_cache)
                    shutil.copytree(work / "gettext", message_cache / "payload")
                    _save_cache(message_cache, message_key)
            if mode == "preview" and not metadata.get("api_cache_hit"):
                _clear(api_cache)
                shutil.copytree(api, api_cache / "payload")
                _save_cache(api_cache, api_key)
            translations = check.validate_translations(root,
                pot_root=work / "gettext", languages=languages)
            if mode == "verify" and translations["errors"]:
                raise ValueError("translation gate failed: " + "\n".join(translations["errors"]))
            metadata["translations"] = translations
            graph_root = work / "diagrams"
            graph_key = _hash({"tools": tool_key, "languages": languages,
                "inputs": {n: h for n, h in observed.items()
                           if n.startswith("docs/assets/diagrams/")}})
            graph_cache = cache / "diagrams"
            if mode == "preview" and _cached(graph_cache, graph_key):
                shutil.copytree(graph_cache / "payload", graph_root)
                metadata["diagram_cache_hit"] = True
            else:
                diagram_localization.write(root / "docs/assets/diagrams", env=environment,
                                           output=graph_root, languages=languages)
                if mode == "preview":
                    _clear(graph_cache)
                    shutil.copytree(graph_root, graph_cache / "payload")
                    _save_cache(graph_cache, graph_key)
            config_key = _hash({"tools": tool_key, "revision": source["commit"],
                "config": {n: h for n, h in observed.items()
                           if n.startswith(("docs/sphinx/", "tools/documentation/"))}})
            jobs = min(len(languages), process.parallel_jobs(environment))
            cancelled = threading.Event()
            def html(selected):
                locale_env = dict(environment)
                locale_env.update(NEXUS_DOCS_LANGUAGE=selected,
                    NEXUS_DOCS_LANGUAGES=",".join(languages),
                    NEXUS_DOCS_DIAGRAM_ROOT=str(graph_root),
                    NEXUS_DOCS_CHANGED_INPUTS=json.dumps(changed),
                    NEXUS_DOCS_REBUILD_ALL="1" if rebuild_all else "0")
                locale_work = (cache / "html" / config_key / selected
                               if mode == "preview" else work / "html" / selected)
                locale_work.mkdir(parents=True, exist_ok=True)
                prefix = Path(context.identity["prefix"])
                command = [str(prefix / "venv/bin/python"), "-m", "sphinx", "-n", "-W",
                    "--keep-going", "-c", str(root / "docs/sphinx"), "-d",
                    str(locale_work / "doctrees")]
                if mode == "verify":
                    command.append("-E")
                command += ["-b", "html", "-D", "language=" + selected,
                            str(root / "docs"), str(locale_work / "site")]
                try:
                    receipt = execute(command, root, locale_env, work,
                                      "html-" + selected, timeout, cancel=cancelled)
                except BaseException:
                    cancelled.set()
                    raise
                for name in set(prior) - set(observed):
                    path = Path(name)
                    if name.startswith("docs/") and path.suffix == ".md":
                        stale = locale_work / "site" / path.relative_to("docs").with_suffix(".html")
                        stale.unlink(missing_ok=True)
                shutil.copytree(locale_work / "site", site / selected)
                return receipt
            with ThreadPoolExecutor(max_workers=jobs) as pool:
                receipts.extend(pool.map(html, languages))
            (site / "index.html").write_text('<!doctype html><html lang="en"><meta charset="utf-8">'
                '<title>Nexus documentation</title><h1>Nexus documentation</h1><p>'
                + ' · '.join('<a href="' + value + '/index.html">' + value + '</a>'
                             for value in languages) + '</p></html>')
            licenses = site / "licenses"
            licenses.mkdir()
            shutil.copyfile(root / "LICENSE", licenses / "LICENSE.txt")
            shutil.copytree(Path(context.identity["prefix"]) / "licenses",
                            licenses / "documentation-tools")
            shutil.copytree(root / "docs/assets/diagrams", site / "sources/diagrams")
            if inputs(root) != observed or observe_git_source(root) != source:
                raise ValueError("source identity changed during documentation build")
            if mode == "preview":
                record(state, observed)
            metadata.update(status="passed" if mode == "verify" else "ready",
                            command_receipts=receipts, source_check=report,
                            files=inventory(site))
            record(output / REPORT, metadata)
            return metadata
        except BaseException as error:
            _clear(site)
            # Sphinx mutates its environments before later stages can fail.
            # They no longer describe the last successful input manifest.
            for mutable in (cache / "gettext-work", cache / "html"):
                if mutable.exists():
                    shutil.rmtree(mutable)
            metadata.update(status="failed", error=str(error), command_receipts=receipts)
            record(output / REPORT, metadata)
            raise
