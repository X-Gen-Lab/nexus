"""Build preview or verified documentation in one owned, locked workspace."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import threading
import time
import uuid

from tools.dev import process
from tools.dev.build_lock import build_lock
from tools.documentation import (check, diagram_localization, diagrams, install,
                                 publication)
from tools.evidence.identity import observe_git_source


SCHEMA = 2
OWNER = ".nexus-documentation-output.json"
REPORT = "documentation-build.json"
_ORCHESTRATION = {
    "tools/documentation/build.py", "tools/documentation/cli.py",
    "tools/documentation/install.py", "tools/documentation/publication.py",
    "tools/documentation/check_browser_layout.py",
}
_DOXYGEN_OPTIONS = ("GENERATE_HTML = NO", "GENERATE_XML = YES")
_RENDER_ENVIRONMENT = {
    "LANG", "LANGUAGE", "TZ", "SOURCE_DATE_EPOCH", "PYTHONPATH", "PYTHONHOME",
    "PYTHONHASHSEED", "PYTHONNOUSERSITE", "FONTCONFIG_FILE", "FONTCONFIG_PATH",
    "FONTCONFIG_SYSROOT", "GDFONTPATH", "DOTFONTPATH", "LD_LIBRARY_PATH", "LD_PRELOAD",
}


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


def inputs(root, *, rendered=None):
    """Observe authored, included, API and renderer inputs as actual bytes."""
    if rendered is None:
        pages = check.sources(root)
        rendered = set(pages) | check.dependencies(root, pages)
    paths = set(rendered) | set(check.adjacent_sources(root))
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


def _parser_inputs(observed):
    """Fingerprint rendered syntax and transforms, independently of invocation."""
    return {name: digest for name, digest in observed.items()
            if name.startswith("docs/sphinx/")
            or (name.startswith("tools/documentation/")
                and name not in _ORCHESTRATION and not name.endswith(".md"))
            or name == "dependencies/platform-compatibility.json"}


def _message_inputs(root):
    """Use the shared include syntax; image-only assets do not define messages."""
    pending, parsed, names = list(check.sources(root)), set(), set()
    while pending:
        page = pending.pop()
        if page in parsed:
            continue
        parsed.add(page)
        names.add(page.relative_to(root).as_posix())
        for kind, target in check.file_inputs(page.read_text()):
            if kind not in {"include", "literalinclude"}:
                continue
            path = check.resource_path(root, page, target, kind=kind)
            names.add(path.relative_to(root).as_posix())
            if kind == "include":
                pending.append(path)
    return names


def _rebuild_all(changed, parser, rendered, *, prior):
    """Keep known data changes local; unknown render inputs stay conservative."""
    if not prior:
        return True
    for name in changed:
        if name in parser or name.startswith("docs/sphinx/"):
            return True
        if (name.startswith("tools/documentation/")
                and name not in _ORCHESTRATION and not name.endswith(".md")):
            return True
        if (name in rendered or name in _ORCHESTRATION
                or name.endswith((".md", ".h"))
                or name in {"Doxyfile", "LICENSE"}
                or name.startswith(("docs/assets/", "docs/locale/", "boards/",
                                    "soc/", "tools/configure/"))):
            continue
        return True
    return False


def _preserve_xml_times(current, previous):
    """Retain dependency mtimes only after fresh XML has identical bytes."""
    if previous is None:
        return
    for path in (current / "xml").rglob("*.xml"):
        old = previous / path.relative_to(current)
        if old.is_file() and path.read_bytes() == old.read_bytes():
            os.utime(path, ns=(path.stat().st_atime_ns, old.stat().st_mtime_ns))


def _xml_shape(api):
    """Structural changes require fresh global C references and XML observers."""
    return _hash({"files": [path.relative_to(api).as_posix()
                            for path in sorted((api / "xml").rglob("*.xml"))],
                  "index": _file_hash(api / "xml/index.xml")})


def _admit(root, work, tool_prefix, *, parallel):
    """Only tool observation overlaps; source checks follow their own snapshot."""
    def tools():
        started = time.perf_counter()
        context = install.context(tool_prefix)
        return context, time.perf_counter() - started

    def source():
        started = time.perf_counter()
        identity = observe_git_source(root)
        pages = check.sources(root)
        rendered_paths = set(pages) | check.dependencies(root, pages)
        observed = inputs(root, rendered=rendered_paths)
        parser = _parser_inputs(observed)
        rendered = {path.relative_to(root).as_posix()
                    for path in rendered_paths}
        snapshot_seconds = time.perf_counter() - started
        started = time.perf_counter()
        # This admission already validated the include closure. The build's
        # diagram gate below owns the prepared renderer; standalone check
        # retains both validations by default.
        report = check.scan(root, check_diagram_outputs=False,
                            check_dependencies=False)
        record(work / "source-check.json", report)
        return ((identity, observed, parser, rendered, report), snapshot_seconds,
                time.perf_counter() - started)

    started = time.perf_counter()
    if parallel:
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures = (pool.submit(tools), pool.submit(source))
            results, errors = [], []
            for future in futures:
                try:
                    results.append(future.result())
                except Exception as error:
                    errors.append(error)
            if errors:
                raise errors[0]
    else:
        results = (tools(), source())
    (context, tool_seconds), (state, snapshot_seconds, check_seconds) = results
    return context, state, {"tools_verification": tool_seconds,
        "source_snapshot": snapshot_seconds, "source_checks": check_seconds,
        "admission_wall": time.perf_counter() - started}


def _doxygen_config(root, api):
    """Use exactly the same effective overrides for execution and cache identity."""
    def quoted(path):
        return '"' + str(path).replace("\\", "\\\\").replace('"', '\\"') + '"'
    policy = json.loads((root / "dependencies/platform-compatibility.json").read_text())
    return '\n'.join(('@INCLUDE = ' + quoted(root / "Doxyfile"),
        'OUTPUT_DIRECTORY = ' + quoted(api), *_DOXYGEN_OPTIONS,
        'PROJECT_NUMBER = "' + policy["semantic_version"] + '"', ''))


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
        "NEXUS_DOCS_SOURCE_DIRTY": "1" if source["dirty"] else "0",
        "NEXUS_DOCS_LANGUAGES": ",".join(dict.fromkeys(("en", *languages))),
        "NEXUS_DOCS_LANGUAGE": "en", "NEXUS_DOCS_DOXYGEN_XML": str(xml),
        "NEXUS_DOCS_GETTEXT_ROOT": str(gettext),
        "NEXUS_DOCS_TRANSLATION_ROOT": str(root / "docs/locale")})
    return environment


def extract_messages(root, work, context, source, *, mode="verify",
                     languages=("en", "zh_CN"), timeout=600, api=None,
                     generate_api=True, gettext_workspace=None,
                     changed_inputs=(), rebuild_all=False, previous_api=None,
                     doxygen_config=None):
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
    config = work / "Doxyfile"
    config.write_text(doxygen_config if doxygen_config is not None
                      else _doxygen_config(root, api))
    receipts = []
    if generate_api:
        receipts.append(execute([str(prefix / "bin/doxygen"), str(config)], root,
                                environment, work, "doxygen", timeout))
        _preserve_xml_times(api, previous_api)
    if gettext_workspace is not None:
        gettext_work = Path(gettext_workspace) / _xml_shape(api)
        environment["NEXUS_DOCS_GETTEXT_ROOT"] = str(gettext_work / "pot")
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
    started = time.perf_counter()
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
        work, cache = (output / name for name in ("work", "cache"))
        site = work / "site"
        receipts = []
        timings = metadata["timings_seconds"] = {}
        try:
            _clear(work)
            site.mkdir()
            metadata["publication_id"] = publication.mark(site)
            cache.mkdir(exist_ok=True)
            context, source_state, admission = _admit(root, work, tool_prefix,
                parallel=mode == "preview" and process.parallel_jobs(os.environ) > 1)
            source, observed, parser, rendered, report = source_state
            timings.update(admission)
            metadata.update(source=source, source_hash=_hash(observed), tools=context.identity)
            environment = context.environment()
            environment_key = _hash({name: value for name, value in environment.items()
                if name in _RENDER_ENVIRONMENT or name.startswith("LC_")})
            metadata["render_environment_sha256"] = environment_key
            execution_key = {"tools": context.identity, "environment": environment_key}
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
            rebuild_all = _rebuild_all(changed, parser, rendered, prior=prior)
            stage = time.perf_counter()
            if report["errors"]:
                raise ValueError("documentation source check failed: " + "\n".join(report["errors"]))
            diagram_report = diagrams.check(root, env=environment, full=mode == "verify")
            if diagram_report["errors"]:
                raise ValueError("diagram verification failed: " + "\n".join(diagram_report["errors"]))
            timings["source_checks"] += time.perf_counter() - stage
            stage = time.perf_counter()
            parser_key = _hash({"execution": execution_key, "implementation": parser})
            api_names = {n: h for n, h in observed.items()
                         if n.endswith(".h") or n in {"Doxyfile", "dependencies/platform-compatibility.json"}}
            api_cache = cache / "api"
            api = work / "api"
            doxygen_config = _doxygen_config(root, api)
            api_key = _hash({"files": api_names, "config": doxygen_config,
                             "execution": execution_key})
            if mode == "preview" and _cached(api_cache, api_key):
                shutil.copytree(api_cache / "payload", api)
                metadata["api_cache_hit"] = True
            message_names = _message_inputs(root)
            message_key = _hash({"api": api_key, "parser": parser_key,
                "inputs": {name: observed[name] for name in sorted(message_names)}})
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
                    gettext_workspace=cache / "gettext-work" / parser_key
                        if mode == "preview" else None,
                    changed_inputs=changed, rebuild_all=rebuild_all,
                    previous_api=api_cache / "payload" if mode == "preview" else None,
                    doxygen_config=doxygen_config)
                receipts.extend(extracted)
                if mode == "preview":
                    _clear(message_cache)
                    shutil.copytree(work / "gettext", message_cache / "payload")
                    _save_cache(message_cache, message_key)
            if mode == "preview" and not metadata.get("api_cache_hit"):
                _clear(api_cache)
                shutil.copytree(api, api_cache / "payload")
                _save_cache(api_cache, api_key)
            timings["messages"] = time.perf_counter() - stage
            stage = time.perf_counter()
            translations = check.validate_translations(root,
                pot_root=work / "gettext", languages=languages)
            if mode == "verify" and translations["errors"]:
                raise ValueError("translation gate failed: " + "\n".join(translations["errors"]))
            metadata["translations"] = translations
            timings["translations"] = time.perf_counter() - stage
            stage = time.perf_counter()
            graph_root = work / "diagrams"
            graph_key = _hash({"execution": execution_key, "languages": languages,
                "implementation": {name: digest for name, digest in parser.items()
                                   if name.startswith("tools/documentation/diagram")},
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
            timings["diagrams"] = time.perf_counter() - stage
            stage = time.perf_counter()
            config_key = _hash({"parser": parser_key, "xml_files": _xml_shape(api)})
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
            timings["html"] = time.perf_counter() - stage
            stage = time.perf_counter()
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
            timings["publication_inputs"] = time.perf_counter() - stage
            timings["prepublication_total"] = time.perf_counter() - started
            publication.publish(output, site, metadata, record=record)
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
