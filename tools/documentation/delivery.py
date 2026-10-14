"""Package, restore and publish verified documentation without firmware claims.

The manifest identifies payload bytes, never itself or the enclosing archive.
An externally retained manifest digest anchors verification; this is not signing.
"""

from __future__ import annotations

import gzip
import hashlib
from html import escape
from html.parser import HTMLParser
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile
from urllib.parse import quote, unquote, urlsplit
from urllib.request import urlopen
import xml.etree.ElementTree as ET

from tools.evidence.identity import observe_git_source
from tools.configure.metadata import load_policy, parse_json, policy_contract


MANIFEST = "documentation-manifest.json"
CHECKSUMS = "SHA256SUMS"
BUILD_METADATA = "documentation-build.json"
REQUIRED = ("index.html", "en/index.html", "zh_CN/index.html",
            "api/index.html", "licenses/LICENSE.txt", BUILD_METADATA)
MAX_FILES = 100000
MAX_BYTES = 1024 * 1024 * 1024
CODE_PATHS = ("CMakeLists.txt", "arch", "boards", "cmake", "components",
              "core", "dependencies", "io", "os", "soc")


def _json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2,
                       allow_nan=False) + "\n").encode("utf-8")


def _digest(contents):
    return hashlib.sha256(contents).hexdigest()


def _json(contents, label):
    try:
        value = parse_json(contents)
    except (ValueError, UnicodeError) as error:
        raise ValueError(f"Invalid documentation {label}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"Documentation {label} must be an object")
    return value


def _path(name):
    if (not isinstance(name, str) or not name or "\\" in name or
            any(ord(character) < 32 for character in name)):
        raise ValueError("Invalid documentation payload path")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in ("..", ".") for part in name.split("/")):
        raise ValueError("Documentation path escape rejected")
    if path.as_posix() != name or ":" in name or name.startswith("/"):
        raise ValueError("Documentation path must be a normalized relative path")
    return path


def _release_id(value):
    if (not isinstance(value, str) or
            not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", value)):
        raise ValueError("Invalid documentation release identity")
    return value


def _no_symlink(path):
    path = Path(path).absolute()
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("Documentation symlink path rejected")
    return path.resolve()


def _overlap(left, right):
    return left == right or left in right.parents or right in left.parents


def _payload(directory):
    directory = _no_symlink(directory)
    if not directory.is_dir():
        raise ValueError("Documentation directory does not exist")
    values = {}
    total = 0
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            raise ValueError("Documentation payload symlink rejected")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Documentation payload requires regular files")
        name = path.relative_to(directory).as_posix()
        _path(name)
        contents = path.read_bytes()
        total += len(contents)
        if len(values) >= MAX_FILES or total > MAX_BYTES:
            raise ValueError("Documentation payload exceeds bounded size")
        values[name] = contents
    return values


class _HTML(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.targets = set()
        self.links = []
        self.in_style = False

    def handle_starttag(self, tag, attributes):
        values = dict(attributes)
        for key in ("id", "name"):
            if values.get(key):
                self.targets.add(values[key])
        if tag == "base":
            raise ValueError("Offline documentation rejects base links")
        if tag == "style":
            self.in_style = True
        if values.get("style"):
            self.links.extend((link, True) for link in _css(values["style"]))
        for key in ("href", "src", "data", "action", "poster"):
            if values.get(key):
                active = key != "href" or tag == "link"
                self.links.append((values[key], active))
        if values.get("srcset"):
            for candidate in values["srcset"].split(","):
                self.links.append((candidate.strip().split()[0], True))

    def handle_endtag(self, tag):
        if tag == "style":
            self.in_style = False

    def handle_data(self, data):
        if self.in_style:
            self.links.extend((link, True) for link in _css(data))


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


def _svg(contents):
    if b"<!doctype" in contents.lower() or b"<!entity" in contents.lower():
        raise ValueError("Offline SVG rejects document types and entities")
    try:
        root = ET.fromstring(contents)
    except ET.ParseError as error:
        raise ValueError("Malformed offline SVG") from error
    parser = _HTML()
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1]
        if tag in {"script", "foreignObject", "animate", "animateMotion",
                   "animateTransform", "set"}:
            raise ValueError("Offline SVG rejects active elements")
        if element.get("id"):
            parser.targets.add(element.get("id"))
        for key, value in element.attrib.items():
            key = key.rsplit("}", 1)[-1]
            if key.lower().startswith("on"):
                raise ValueError("Offline SVG rejects event handlers")
            if key in ("href", "src"):
                parser.links.append((value, True))
            parser.links.extend((link, True) for link in _css(value))
        if tag == "style" and element.text:
            parser.links.extend((link, True) for link in _css(element.text))
    return parser


def _offline(payload):
    pages = {}
    for name, contents in payload.items():
        if name.endswith(".html"):
            parser = _HTML()
            try:
                parser.feed(contents.decode("utf-8"))
            except UnicodeError as error:
                raise ValueError(f"Documentation HTML is not UTF-8: {name}") from error
            pages[name] = parser
        elif name.endswith(".css"):
            parser = _HTML()
            parser.links.extend((link, True) for link in _css(contents.decode("utf-8")))
            pages[name] = parser
        elif name.endswith(".svg"):
            pages[name] = _svg(contents)
    count = 0
    for name, page in pages.items():
        for link, active in page.links:
            location = urlsplit(link)
            if location.scheme or location.netloc:
                if (active and location.scheme == "data" and
                        re.match(r"data:(image/|font/|application/font-)", link)):
                    continue
                if active:
                    raise ValueError(f"External offline resource: {name}: {link}")
                if location.scheme not in ("http", "https", "mailto"):
                    raise ValueError(f"Unsupported documentation link: {link}")
                continue
            decoded = unquote(location.path)
            if decoded.startswith("/") or "\\" in decoded:
                raise ValueError(f"Documentation link must be relative: {link}")
            parts = list(PurePosixPath(name).parent.parts)
            if decoded:
                for part in decoded.split("/"):
                    if part in ("", "."):
                        continue
                    if part == "..":
                        if not parts:
                            raise ValueError(f"Documentation link escape: {link}")
                        parts.pop()
                    else:
                        parts.append(part)
                target = "/".join(parts)
                if decoded.endswith("/"):
                    target += "/index.html"
            else:
                target = name
            if target not in payload:
                raise ValueError(f"Missing documentation link: {name}: {link}")
            if (location.fragment and target in pages and
                    unquote(location.fragment) not in pages[target].targets):
                raise ValueError(f"Missing documentation fragment: {name}: {link}")
            count += 1
    return count


def _manifest(payload, expected_manifest_sha256=None):
    if MANIFEST not in payload or CHECKSUMS not in payload:
        raise ValueError("Documentation manifest or checksums missing")
    raw = payload[MANIFEST]
    if expected_manifest_sha256 is not None and _digest(raw) != expected_manifest_sha256:
        raise ValueError("Documentation manifest digest mismatch")
    manifest = _json(raw, "manifest")
    if (type(manifest.get("schema_version")) is not int or
            manifest.get("schema_version") != 1 or
            manifest.get("kind") != "nexus_documentation_package"):
        raise ValueError("Unsupported documentation manifest schema")
    _release_id(manifest.get("release_id"))
    _manifest_identity(manifest)
    if manifest.get("physical_status") != "not_executed":
        raise ValueError("Documentation package cannot grant physical qualification")
    records = manifest.get("files")
    if not isinstance(records, list) or not records:
        raise ValueError("Documentation payload manifest is empty")
    declared = {}
    for record in records:
        if not isinstance(record, dict) or set(record) != {"path", "bytes", "sha256"}:
            raise ValueError("Invalid documentation payload record")
        name = record["path"]
        _path(name)
        if name in (MANIFEST, CHECKSUMS) or name in declared:
            raise ValueError("Documentation manifest has duplicate or self-reference")
        if (type(record["bytes"]) is not int or record["bytes"] < 0 or
                not isinstance(record["sha256"], str) or
                not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
            raise ValueError("Invalid documentation digest or bytes record")
        declared[name] = record
    actual = set(payload) - {MANIFEST, CHECKSUMS}
    if actual != set(declared):
        raise ValueError("Documentation payload closure has missing or unlisted files")
    if (not set(REQUIRED).issubset(actual) or
            any(not payload[name].strip() for name in REQUIRED)):
        raise ValueError("Documentation required payload is incomplete")
    for name, record in declared.items():
        if len(payload[name]) != record["bytes"] or _digest(payload[name]) != record["sha256"]:
            raise ValueError(f"Documentation payload digest/bytes mismatch: {name}")
    expected_checksums = _checksums(payload)
    if payload[CHECKSUMS] != expected_checksums:
        raise ValueError("Documentation checksums digest mismatch")
    metadata = _json(payload[BUILD_METADATA], "build metadata")
    if metadata.get("status") != "passed":
        raise ValueError("Documentation build metadata requires successful passed status")
    try:
        build_source = _identity(metadata["source"])
    except (KeyError, TypeError) as error:
        raise ValueError("Documentation build source identity missing") from error
    if (build_source != _identity(manifest["documentation_source"]) or
            metadata.get("tools") != manifest["tool_versions"] or
            metadata.get("languages") != manifest["languages"]):
        raise ValueError("Documentation build and manifest identities differ")
    _offline({name: payload[name] for name in declared})
    return manifest


def _manifest_identity(manifest):
    if (not isinstance(manifest.get("version"), str) or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", manifest["version"]) or
            manifest.get("languages") != ["en", "zh_CN"] or
            not isinstance(manifest.get("tool_versions"), dict) or
            not manifest["tool_versions"]):
        raise ValueError("Documentation manifest identity is incomplete")
    source_interface = manifest.get("source_interface")
    if not isinstance(source_interface, dict):
        raise ValueError("Documentation source interface policy identity missing")
    policy = {key: value for key, value in source_interface.items()
              if key != "policy_sha256"}
    policy_contract(policy)
    if (policy["semantic_version"] != manifest["version"] or
            not re.fullmatch(r"[0-9a-f]{64}",
                             str(source_interface.get("policy_sha256", "")))):
        raise ValueError("Documentation source interface version/policy differs")
    for name in ("documentation_source", "code_source"):
        source = manifest.get(name)
        if not isinstance(source, dict) or source.get("dirty") is not False:
            raise ValueError("Documentation manifest requires clean source identity")
        for key in ("commit", "tree"):
            if not re.fullmatch(r"[0-9a-f]{40}", str(source.get(key, ""))):
                raise ValueError("Documentation source commit/tree identity missing")
        submodules = source.get("submodules")
        if (not isinstance(submodules, list) or
                any(not isinstance(line, str) or line.startswith(("-", "+", "U"))
                    for line in submodules)):
            raise ValueError("Documentation source submodule identity is incomplete")
        if (not isinstance(source.get("paths"), list) or not source["paths"] or
                not re.fullmatch(r"[0-9a-f]{64}",
                                 str(source.get("tree_entries_sha256", "")))):
            raise ValueError("Documentation scoped source identity missing")
    if (_identity(manifest["code_source"]) !=
            _identity(manifest["documentation_source"])):
        raise ValueError("Documentation code and source snapshots differ")
    qualified = manifest.get("qualified_code")
    if not isinstance(qualified, dict):
        raise ValueError("Documentation qualified code reference missing")
    for key in ("commit", "tree"):
        if not re.fullmatch(r"[0-9a-f]{40}", str(qualified.get(key, ""))):
            raise ValueError("Documentation qualified code identity missing")
    if (qualified.get("index") != "docs/delivery/build-system-results.json" or
            not re.fullmatch(r"[0-9a-f]{64}", str(qualified.get("index_sha256", "")))):
        raise ValueError("Documentation qualified code evidence identity missing")


def _checksums(payload):
    return "".join(f"{_digest(payload[name])}  {name}\n" for name in sorted(payload)
                   if name != CHECKSUMS).encode("utf-8")


def _archive(payload, destination):
    with Path(destination).open("wb") as file:
        with gzip.GzipFile(filename="", mode="wb", fileobj=file, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for name in sorted(payload):
                    member = tarfile.TarInfo(name)
                    member.size = len(payload[name])
                    member.mode = 0o644
                    archive.addfile(member, io.BytesIO(payload[name]))


def _read_archive(path):
    path = _no_symlink(path)
    payload = {}
    total = 0
    try:
        with tarfile.open(path, mode="r:gz") as archive:
            for member in archive:
                _path(member.name)
                if member.name in payload:
                    raise ValueError("Documentation archive duplicate member")
                if not member.isfile() or member.issym() or member.islnk():
                    raise ValueError("Documentation archive requires regular files, no links")
                total += member.size
                if member.size < 0 or total > MAX_BYTES or len(payload) >= MAX_FILES:
                    raise ValueError("Documentation archive exceeds bounded size")
                stream = archive.extractfile(member)
                if stream is None:
                    raise ValueError("Missing documentation archive bytes")
                payload[member.name] = stream.read()
                if len(payload[member.name]) != member.size:
                    raise ValueError("Truncated documentation archive bytes")
    except (tarfile.TarError, OSError, EOFError) as error:
        raise ValueError("Invalid documentation archive") from error
    return payload


def _git_tree(root, paths):
    result = subprocess.run(["git", "-C", str(root), "ls-tree", "HEAD", "--", *paths],
                            capture_output=True, check=True)
    return {"paths": list(paths), "tree_entries_sha256": _digest(result.stdout)}


def _identity(source):
    return {key: source[key] for key in ("commit", "tree", "dirty", "submodules")}


def package(build_dir, output_dir, *, source_root, release_id, toolchain=None):
    """Bind a clean, unchanged source and complete built payload to one release ID."""
    release_id = _release_id(release_id)
    build_dir = _no_symlink(build_dir)
    output_dir = _no_symlink(output_dir)
    source_root = _no_symlink(source_root)
    if _overlap(output_dir, source_root) or _overlap(output_dir, build_dir):
        raise ValueError("Documentation package output must not overlap source inputs")
    source = observe_git_source(source_root)
    if source["dirty"]:
        raise ValueError("Documentation publication requires clean Git source")
    payload = _payload(build_dir)
    if MANIFEST in payload or CHECKSUMS in payload:
        raise ValueError("Documentation build contains reserved delivery files")
    if (not set(REQUIRED).issubset(payload) or
            any(not payload[name].strip() for name in REQUIRED)):
        raise ValueError("Documentation required build payload is incomplete")
    metadata = _json(payload[BUILD_METADATA], "build metadata")
    if metadata.get("status") != "passed":
        raise ValueError("Documentation build metadata requires successful passed status")
    if (type(metadata.get("schema_version")) is not int or
            metadata.get("schema_version") != 1 or
            metadata.get("kind") != "nexus_documentation_build" or
            metadata.get("languages") != ["en", "zh_CN"] or
            not isinstance(metadata.get("tools"), dict) or not metadata["tools"]):
        raise ValueError("Incomplete documentation build metadata")
    try:
        build_source = _identity(metadata["source"])
    except (KeyError, TypeError) as error:
        raise ValueError("Missing documentation build source identity") from error
    if build_source != _identity(source):
        raise ValueError("Stale documentation build source identity")
    if any(line.startswith(("-", "+", "U")) for line in source["submodules"]):
        raise ValueError("Documentation source submodules are incomplete or stale")
    version_match = re.search(r"project\(nexus\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
                              (source_root / "CMakeLists.txt").read_text())
    if not version_match:
        raise ValueError("Documentation version requires the CMake project version")
    policy = load_policy(source_root)
    if policy["semantic_version"] != version_match[1]:
        raise ValueError("Documentation CMake and compatibility policy versions differ")
    source_interface = dict(policy)
    source_interface["policy_sha256"] = _digest(
        (source_root / "dependencies/platform-compatibility.json").read_bytes())
    qualification_path = source_root / "docs/delivery/build-system-results.json"
    qualification_bytes = qualification_path.read_bytes()
    qualification = _json(qualification_bytes, "qualification index")
    qualified = qualification.get("source", {})
    if any(not re.fullmatch(r"[0-9a-f]{40}", str(qualified.get(key, "")))
           for key in ("canonical_code_commit", "code_tree")):
        raise ValueError("Missing exact qualified code source")
    documentation = _identity(source)
    documentation.update(_git_tree(source_root, ("docs", "tools/documentation")))
    code = _identity(source)
    code.update(_git_tree(source_root, CODE_PATHS))
    manifest = {
        "schema_version": 1, "kind": "nexus_documentation_package",
        "release_id": release_id, "version": version_match[1],
        "source_interface": source_interface,
        "documentation_source": documentation, "code_source": code,
        "qualified_code": {"commit": qualified["canonical_code_commit"],
                           "tree": qualified["code_tree"],
                           "index": "docs/delivery/build-system-results.json",
                           "index_sha256": _digest(qualification_bytes)},
        "physical_status": "not_executed", "languages": ["en", "zh_CN"],
        "tool_versions": metadata["tools"],
        "qualification_boundary": "Documentation delivery grants no new firmware or HIL qualification.",
        "files": [{"path": name, "bytes": len(contents), "sha256": _digest(contents)}
                  for name, contents in sorted(payload.items())]}
    if toolchain is not None:
        manifest["toolchain"] = toolchain
    payload[MANIFEST] = _json_bytes(manifest)
    payload[CHECKSUMS] = _checksums(payload)
    _manifest(payload)
    if _payload(build_dir) != {name: payload[name] for name in payload
                               if name not in (MANIFEST, CHECKSUMS)}:
        raise ValueError("Documentation build payload changed during packaging")
    if observe_git_source(source_root) != source:
        raise ValueError("Documentation source changed during packaging")
    output_dir.mkdir(parents=True, exist_ok=True)
    destination = output_dir / release_id
    archive_path = output_dir / (release_id + ".tar.gz")
    if destination.exists() or archive_path.exists():
        raise ValueError("Documentation package output already exists")
    with tempfile.TemporaryDirectory(prefix=".documentation-", dir=output_dir) as name:
        temporary = Path(name)
        directory = temporary / "payload"
        _write_payload(payload, directory)
        archive = temporary / "package.tar.gz"
        _archive(payload, archive)
        directory.rename(destination)
        archive.rename(archive_path)
    return {"directory": str(destination), "archive": str(archive_path),
            "manifest_sha256": _digest(payload[MANIFEST]),
            "archive_sha256": _digest(archive_path.read_bytes()),
            "release_id": release_id, "payload_files": len(manifest["files"])}


def _write_payload(payload, directory):
    directory.mkdir()
    for name, contents in payload.items():
        path = directory.joinpath(*_path(name).parts)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)


def verify(path, *, expected_manifest_sha256=None):
    """Check byte closure, checksums, identity schema and offline links."""
    path = _no_symlink(path)
    payload = _payload(path) if path.is_dir() else _read_archive(path)
    return _manifest(payload, expected_manifest_sha256)


def restore(archive, destination, *, expected_manifest_sha256=None):
    """Restore into a new independent root after validating all archive members."""
    destination = _no_symlink(destination)
    if destination.exists():
        raise ValueError("Documentation restore destination already exists")
    payload = _read_archive(archive)
    manifest = _manifest(payload, expected_manifest_sha256)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".documentation-restore-",
                                     dir=destination.parent) as name:
        staged = Path(name) / "payload"
        _write_payload(payload, staged)
        verify(staged, expected_manifest_sha256=expected_manifest_sha256)
        staged.rename(destination)
    return manifest


def _atomic_text(path, contents):
    path = _no_symlink(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".documentation-entry-",
                                     dir=path.parent) as name:
        temporary = Path(name) / "index.html"
        temporary.write_text(contents, encoding="utf-8")
        temporary.replace(path)


def publish(package_dir, webroot, *, archive=None):
    """Install an immutable version then atomically update the relative stable entry."""
    package_dir = _no_symlink(package_dir)
    webroot = _no_symlink(webroot)
    if _overlap(package_dir, webroot):
        raise ValueError("Documentation webroot must not overlap input payload")
    payload = _payload(package_dir)
    manifest = _manifest(payload)
    version = webroot / "versions" / manifest["release_id"]
    _no_symlink(version)
    digest = _digest(payload[MANIFEST])
    archive_digest = None
    download = _no_symlink(webroot / "downloads" /
                           (manifest["release_id"] + ".tar.gz"))
    archive_bytes = None
    if archive is not None:
        archive_payload = _read_archive(archive)
        _manifest(archive_payload, digest)
        if archive_payload != payload:
            raise ValueError("Documentation archive and publication bytes differ")
        archive_bytes = _no_symlink(archive).read_bytes()
        archive_digest = _digest(archive_bytes)
        if download.exists() and download.read_bytes() != archive_bytes:
            raise ValueError("Immutable documentation archive has different bytes")
    elif download.exists():
        verify(download, expected_manifest_sha256=digest)
        archive_digest = _digest(download.read_bytes())
    version.parent.mkdir(parents=True, exist_ok=True)
    if version.exists():
        verify(version, expected_manifest_sha256=digest)
        if _payload(version) != payload:
            raise ValueError("Immutable documentation version has different bytes")
    else:
        with tempfile.TemporaryDirectory(prefix=".documentation-publish-",
                                         dir=version.parent) as name:
            staged = Path(name) / "payload"
            _write_payload(payload, staged)
            staged.rename(version)
    if archive_bytes is not None and not download.exists():
        download.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".documentation-download-",
                                         dir=download.parent) as name:
            temporary = Path(name) / "archive.tar.gz"
            temporary.write_bytes(archive_bytes)
            temporary.rename(download)
    stable = _no_symlink(webroot / "stable")
    stable.mkdir(exist_ok=True)
    relative = f"../versions/{manifest['release_id']}/"
    download_link = ""
    if archive_digest is not None:
        download_link = (f'<p><a href="../downloads/{manifest["release_id"]}.tar.gz">'
                         f'Offline HTML archive</a></p><p>Archive SHA-256: '
                         f'<code>{archive_digest}</code></p>')
    contents = ("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
                "<title>Nexus documentation stable</title><h1>Nexus documentation</h1>"
                f'<p><a href="{escape(relative)}en/index.html">English</a> '
                f'<a href="{escape(relative)}zh_CN/index.html">中文</a> '
                f'<a href="{escape(relative)}api/index.html">API</a></p>'
                f'<p>Version: {escape(manifest["release_id"])}</p>{download_link}'
                f'<p><a href="{relative}{MANIFEST}">Manifest</a></p></html>\n')
    _atomic_text(stable / "index.html", contents)
    root_contents = ("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
                     "<title>Nexus documentation</title><h1>Nexus documentation</h1>"
                     '<p><a href="stable/index.html">Stable documentation</a></p>'
                     f'<p>Version: {escape(manifest["release_id"])}</p>'
                     f'<p><a href="versions/{manifest["release_id"]}/en/index.html">'
                     'English</a> '
                     f'<a href="versions/{manifest["release_id"]}/zh_CN/index.html">'
                     '中文</a> '
                     f'<a href="versions/{manifest["release_id"]}/api/index.html">'
                     'API</a></p><p><a href="downloads/index.html">Downloads</a></p>'
                     f'<p><a href="versions/{manifest["release_id"]}/{MANIFEST}">'
                     'Manifest</a></p></html>\n')
    _atomic_text(webroot / "index.html", root_contents)
    downloads = webroot / "downloads"
    downloads.mkdir(exist_ok=True)
    entries = []
    for path in sorted(downloads.glob("*.tar.gz")):
        _no_symlink(path)
        name = path.name[:-len(".tar.gz")]
        _release_id(name)
        sha256 = _digest(path.read_bytes())
        entries.append(f'<li><a href="{escape(path.name)}">{escape(name)}</a> '
                       f'<code>{sha256}</code></li>')
    _atomic_text(downloads / "index.html",
                 '<!doctype html><html lang="en"><meta charset="utf-8">'
                 '<title>Nexus documentation downloads</title><h1>Offline HTML</h1>'
                 '<ul>' + ''.join(entries) + '</ul></html>\n')
    result = {"release_id": manifest["release_id"], "manifest_sha256": digest,
              "directory": str(version), "stable": str(stable / "index.html"),
              "physical_status": "not_executed"}
    if archive_digest is not None:
        result["archive_sha256"] = archive_digest
        result["download"] = str(download)
    return result


def post_deploy(base_url, release_id, expected_manifest_sha256, receipt_path=None,
                *, expected_archive_sha256=None, verify_stable=True):
    """Invalidate an owned old receipt before observing a new deployment attempt."""
    path = _no_symlink(receipt_path) if receipt_path is not None else None
    state = {"schema_version": 1, "kind": "nexus_documentation_post_deploy",
             "status": "running", "physical_status": "not_executed"}
    if path is not None:
        if path.exists():
            previous = _json(path.read_bytes(), "post-deploy receipt")
            if (previous.get("schema_version") != 1 or
                    previous.get("kind") != state["kind"]):
                raise ValueError("Documentation receipt path contains unrelated data")
        _atomic_text(path, _json_bytes(state).decode("utf-8"))
    try:
        result = _post_deploy(base_url, release_id, expected_manifest_sha256,
                              expected_archive_sha256=expected_archive_sha256,
                              verify_stable=verify_stable)
    except Exception as error:
        if path is not None:
            state.update(status="failed", error=str(error))
            _atomic_text(path, _json_bytes(state).decode("utf-8"))
        raise
    if path is not None:
        _atomic_text(path, _json_bytes(result).decode("utf-8"))
    return result


def _post_deploy(base_url, release_id, expected_manifest_sha256,
                 *, expected_archive_sha256=None, verify_stable=True):
    """Fetch the actual version and every declared file; verify published byte identity."""
    release_id = _release_id(release_id)
    parsed = urlsplit(base_url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ValueError("Documentation deployment requires an HTTP(S) URL")
    if parsed.query or parsed.fragment or parsed.username or parsed.password:
        raise ValueError("Invalid documentation deployment base URL")
    root_url = base_url.rstrip("/") + "/"
    base = root_url + "versions/" + quote(release_id, safe="") + "/"
    observations = []
    def fetch(relative, prefix=base):
        try:
            url = prefix + quote(relative, safe="/")
            with urlopen(url, timeout=30) as response:
                if response.status != 200:
                    raise ValueError("Documentation HTTP response is not successful")
                contents = response.read(MAX_BYTES + 1)
                if len(contents) > MAX_BYTES:
                    raise ValueError("Published documentation file exceeds bounded size")
                observations.append({"url": url, "final_url": response.url,
                                     "status": response.status,
                                     "bytes": len(contents), "sha256": _digest(contents)})
                return contents
        except OSError as error:
            raise ValueError(f"Documentation HTTP verification failed: {relative}") from error
    raw = fetch(MANIFEST)
    if _digest(raw) != expected_manifest_sha256:
        raise ValueError("Published documentation manifest digest mismatch")
    manifest = _json(raw, "published manifest")
    if manifest.get("release_id") != release_id:
        raise ValueError("Published documentation release identity mismatch")
    payload = {MANIFEST: raw, CHECKSUMS: fetch(CHECKSUMS)}
    records = manifest.get("files")
    if not isinstance(records, list) or len(records) > MAX_FILES:
        raise ValueError("Published documentation manifest payload is invalid")
    total = 0
    for record in records:
        if not isinstance(record, dict):
            raise ValueError("Invalid published documentation payload record")
        name = record.get("path")
        _path(name)
        if name in payload:
            raise ValueError("Duplicate published documentation payload path")
        contents = fetch(name)
        total += len(contents)
        if total > MAX_BYTES:
            raise ValueError("Published documentation exceeds bounded size")
        payload[name] = contents
    _manifest(payload, expected_manifest_sha256)
    if verify_stable:
        stable = fetch("stable/index.html", root_url)
        parser = _HTML()
        parser.feed(stable.decode("utf-8"))
        required = {f"../versions/{release_id}/{name}" for name in
                    ("en/index.html", "zh_CN/index.html", "api/index.html", MANIFEST)}
        links = {link for link, _ in parser.links}
        if not required.issubset(links):
            raise ValueError("Published stable entry does not select the verified version")
        if any(urlsplit(link).scheme or link.startswith("/") for link in links):
            raise ValueError("Published stable links must be relative")
    archive_digest = None
    if expected_archive_sha256 is not None:
        archive_bytes = fetch(f"downloads/{release_id}.tar.gz", root_url)
        archive_digest = _digest(archive_bytes)
        if archive_digest != expected_archive_sha256:
            raise ValueError("Published documentation archive digest mismatch")
        with tempfile.TemporaryDirectory(prefix="documentation-http-") as name:
            archive = Path(name) / "download.tar.gz"
            archive.write_bytes(archive_bytes)
            verify(archive, expected_manifest_sha256=expected_manifest_sha256)
        if verify_stable and f"../downloads/{release_id}.tar.gz" not in links:
            raise ValueError("Published stable entry has no verified archive download")
        downloads = _HTML()
        downloads.feed(fetch("downloads/index.html", root_url).decode("utf-8"))
        if f"{release_id}.tar.gz" not in {link for link, _ in downloads.links}:
            raise ValueError("Published downloads entry has no verified archive")
    receipt = {"schema_version": 1, "kind": "nexus_documentation_post_deploy",
               "status": "verified", "base_url": base_url,
               "release_id": release_id, "manifest_sha256": _digest(raw),
               "verified_files": len(records), "stable_verified": verify_stable,
               "observations": observations, "physical_status": "not_executed"}
    if archive_digest is not None:
        receipt["archive_sha256"] = archive_digest
    return receipt
