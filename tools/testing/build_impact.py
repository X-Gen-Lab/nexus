"""Select real production assemblies from the base and proposed source trees."""

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shlex
import subprocess
import tempfile
import tomllib
from types import SimpleNamespace

from tools.dev.change_paths import classify
from tools.maintenance.source_catalogue import ALL_SOURCE_DEPENDENCIES, runtime_selection


_ASSEMBLY_ROOTS = ("boards/", "tools/configure/assemblies/",
                   "tests/contracts/", "cmake/package/verification/")
_SOURCE_SUFFIXES = {".c", ".h", ".s", ".inc", ".hpp", ".cpp"}
_GRAPH = "nexus-target-inputs.json"


def _git(root, *args, data=None):
    return subprocess.check_output(["git", *args], cwd=root, input=data,
                                   timeout=60)


def _metadata(path):
    return ((path.startswith(_ASSEMBLY_ROOTS) and path.endswith(".toml"))
            or (path.startswith(("soc/", *_ASSEMBLY_ROOTS)) and
                PurePosixPath(path).name in {"board.json", "package.json"}))


def _base_files(root, base):
    try:
        tree = _git(root, "rev-parse", "--verify", "--quiet", "--end-of-options",
                    base + "^{tree}").decode().strip()
    except subprocess.CalledProcessError as error:
        if base != "HEAD" or error.returncode != 1:
            raise
        # A new repository can stage a complete board before its first commit.
        head = _git(root, "symbolic-ref", "HEAD").decode().strip()
        present = subprocess.run(["git", "show-ref", "--verify", "--quiet", head],
                                 cwd=root, timeout=30).returncode
        if present != 1:
            raise
        tree = _git(root, "hash-object", "-t", "tree", "--stdin", data=b"").decode().strip()
        return tree, {}
    blobs = {}
    for row in _git(root, "ls-tree", "-rz", tree).split(b"\0"):
        if row:
            entry, raw = row.split(b"\t", 1)
            name = os.fsdecode(raw)
            if _metadata(name) and entry.startswith(b"100"):
                blobs[name] = entry.split()[-1]
    ids = list(dict.fromkeys(blobs.values()))
    result = _git(root, "cat-file", "--batch", data=b"\n".join(ids) + b"\n") if ids else b""
    contents, offset = {}, 0
    for digest in ids:
        end = result.index(b"\n", offset)
        found, kind, size = result[offset:end].split()
        if found != digest or kind != b"blob":
            raise ValueError("Invalid Git metadata blob")
        offset = end + 1 + int(size)
        contents[digest] = result[end + 1:offset]
        offset += 1
    return tree, {name: contents[digest] for name, digest in blobs.items()}


def _working_files(root, names):
    tracked = {os.fsdecode(item) for item in
               _git(root, "ls-files", "-z").split(b"\0") if item}
    return {name: (root / name).read_bytes() for name in tracked.union(names)
            if _metadata(name) and (root / name).is_file()}


def _relative(parent, value):
    name = posixpath.normpath(posixpath.join(parent, value))
    if name == ".." or name.startswith(("../", "/")):
        raise ValueError("Selected assembly input escapes the repository")
    return name


def _inventory(files, *, previous=(), suppliers=False):
    packages, requirements, selections = {}, {}, {}
    boards = {}
    for name, raw in files.items():
        if name.startswith("soc/") and name.endswith("/package.json"):
            manifest = json.loads(raw)
            if manifest.get("scope", "production") == "production":
                family = manifest["family"]
                if family in packages:
                    raise ValueError("Duplicate production family " + family)
                packages[family] = str(PurePosixPath(name).parent)
                requirements[family] = tuple(manifest.get("dependencies", {}).items())
        elif name.endswith("/board.json"):
            boards[name] = json.loads(raw)
    assemblies = {}
    for name, raw in files.items():
        if not name.endswith(".toml"):
            continue
        config = tomllib.loads(raw.decode())
        if config.get("schema") != 2:
            if name in previous or "board" in config or "board_package" in config or (
                    name.startswith("tools/configure/assemblies/") or "/assemblies/" in name):
                raise ValueError("Expected assembly schema 2 in " + name)
            continue
        if config.get("backend") == "native":
            continue
        if not config.get("backend"):
            raise ValueError("Missing backend in " + name)
        parent = str(PurePosixPath(name).parent)
        if "soc_package" in config:
            manifest = _relative(parent, config["soc_package"]) + "/package.json"
            if manifest in files and json.loads(files[manifest]).get("scope") == "reference":
                continue
        if "board" in config:
            board = _relative("boards", config["board"]) + "/board.json"
        elif "board_package" in config:
            board = _relative(parent, config["board_package"]) + "/board.json"
        else:
            raise ValueError("Missing board in maintained assembly " + name)
        # Retain surviving references to a removed board so resolution fails.
        declaration = boards.get(board, {})
        family = declaration.get("soc_family")
        layout = _relative(parent, config["layout"]) if "layout" in config else None
        assemblies[name] = {"board": board, "family": family,
                            "part": declaration.get("soc"), "backend": config["backend"],
                            "package": packages.get(family), "layout": layout}
        if suppliers and family in packages:
            key = family, config["backend"], tuple(config.get("components", ()))
            if key not in selections:
                package = SimpleNamespace(family=family, dependencies=requirements[family])
                selections[key] = [item.identifier for item in runtime_selection(
                    family, backend=key[1], components=key[2], package=package)]
            assemblies[name]["suppliers"] = selections[key]
    return assemblies


def _common_build(name):
    if name in {"CMakeLists.txt", "tools/maintenance/source_catalogue.py",
                "dependencies/toolchains.lock.json", "os/reviewed_patch.py"}:
        return True
    if name.startswith(("cmake/platform/", "cmake/toolchains/")):
        return name.endswith(".cmake")
    return (name.startswith("tools/configure/") and not name.startswith(
            ("tools/configure/tests/", "tools/configure/assemblies/"))
            and name.endswith((".py", ".schema.json")))


def _hardware_input(name):
    if _common_build(name):
        return True
    if name.startswith(("os/freertos/", "os/threadx/")) and name.endswith((".patch", ".py")):
        return not PurePosixPath(name).name.startswith("test_")
    if name.startswith(("arch/native/", "os/native/")):
        return False
    if name.startswith(("boards/", "soc/", "arch/", "core/", "io/", "os/", "components/")):
        file = PurePosixPath(name)
        return (file.name == "CMakeLists.txt" or
                file.suffix.lower() in _SOURCE_SUFFIXES | {".json", ".toml", ".ld", ".lds", ".cmake"})
    return name.startswith(_ASSEMBLY_ROOTS) and name.endswith((".toml", ".json"))


def _receipt_path(root, assembly):
    return root / "build/affected-arm" / PurePosixPath(assembly).with_suffix("") / _GRAPH


def _unrelated(path, status, inputs, board):
    if path in inputs:
        return False
    if path.startswith("docs/") or PurePosixPath(path).suffix in {".md", ".rst"}:
        return True
    if path.startswith("boards/") and path != board:
        return PurePosixPath(path).suffix in {".toml", ".json"}
    return status == "M" and PurePosixPath(path).suffix.lower() in _SOURCE_SUFFIXES


def _dependencies(root, assembly, tree, differences, board):
    try:
        record = json.loads(_receipt_path(root, assembly).read_bytes())
        inputs = record["inputs"]
        previous = record["tree"]
        if (record["schema"] == 1 and re.fullmatch(r"[a-f0-9]{40,64}", previous) and
                record["assembly"] == assembly and isinstance(inputs, list) and inputs and
                all(isinstance(item, str) for item in inputs) and
                record["sha256"] == hashlib.sha256("\0".join(inputs).encode()).hexdigest()):
            inputs = set(inputs)
            if previous != tree:
                if previous not in differences:
                    raw = _git(root, "diff", "--no-renames", "--name-status", "-z",
                               previous, tree, "--").split(b"\0")[:-1]
                    differences[previous] = [(os.fsdecode(name), status.decode())
                                             for status, name in zip(raw[::2], raw[1::2])]
                if not all(_unrelated(path, status, inputs, board)
                           for path, status in differences[previous]):
                    return None
            return inputs
    except (FileNotFoundError, KeyError, TypeError, ValueError, subprocess.CalledProcessError):
        pass
    return None


def plan_hardware(root, paths, *, base="HEAD"):
    """Return current assembly names affected on either side and expansion reasons."""
    root = Path(root).resolve()
    paths = set(paths)
    classify(paths)
    suppliers = {name: item.identifier for item in ALL_SOURCE_DEPENDENCIES
                 for name in (item.path, item.source_lock) if name}
    suppliers["dependencies/freertos-transport.lock.json"] = "freertos"
    names = sorted(name for name in paths if name in suppliers or _hardware_input(name))
    result = {"assemblies": [], "contexts": {}, "reasons": {}, "fallbacks": []}
    if not names:
        return result
    tree, saved = _base_files(root, base)
    supplier_change = any(name in suppliers for name in names)
    old = _inventory(saved, suppliers=supplier_change)
    new = _inventory(_working_files(root, names), previous=old, suppliers=supplier_change)
    fallbacks, reasons = set(), {}
    package_roots = {row["package"] for row in (*old.values(), *new.values())
                     if row["package"]}
    # Only reuse compiler negative evidence for an existing, modified file.
    # Additions/renames can change include lookup; graph changes can change flags.
    modified = set(_git(root, "diff", "--no-renames", "--name-only", "--diff-filter=M",
                        "-z", tree, "--").decode().split("\0"))
    same_graph = all(PurePosixPath(path).suffix.lower() in _SOURCE_SUFFIXES and
                     path in modified for path in paths)
    graph, differences = {}, {}
    for path in names:
        owner = next((package for package in package_roots if
                      path.startswith(package + "/")), None)
        source = PurePosixPath(path).suffix.lower() in _SOURCE_SUFFIXES
        shared = _common_build(path) or path.startswith(("arch/", "core/", "io/", "os/", "components/"))
        for assembly in sorted(set(old) | set(new)):
            if assembly not in new:
                continue
            contexts = [view[assembly] for view in (old, new) if assembly in view]
            direct = any(path in (assembly, row["board"], row["layout"])
                         for row in contexts)
            if path in suppliers:
                if not any(suppliers[path] in row.get("suppliers", ()) for row in contexts):
                    continue
                reason = "selected runtime dependency"
            elif path.startswith(("os/freertos/", "os/threadx/")) and not any(
                    row["backend"] == path.split("/")[1] for row in contexts):
                continue
            elif direct:
                reason = "authored input"
            elif shared or (owner and any(owner == row["package"] for row in contexts)):
                if source and same_graph:
                    if assembly not in graph:
                        graph[assembly] = _dependencies(root, assembly, tree, differences,
                                                        new[assembly]["board"])
                    if graph[assembly] is not None:
                        if path not in graph[assembly]:
                            continue
                        reasons.setdefault(assembly, []).append(path + ": linked compiler input")
                        continue
                reason = ("unverified compiler closure" if source else
                          "shared package fact") + " -> " + (owner or "all production packages")
                fallbacks.add(path + ": " + reason)
            else:
                continue
            reasons.setdefault(assembly, []).append(path + ": " + reason)
    result.update(assemblies=sorted(reasons), reasons=reasons,
                  contexts={name: new[name] for name in reasons}, fallbacks=sorted(fallbacks))
    return result


def record_hardware_inputs(root, assembly, build, tree):
    """After a successful staged link, save its exact target's compiler dependencies."""
    root, build = Path(root).resolve(), Path(build).resolve()
    assembly = str(Path(assembly).relative_to(root)) if Path(assembly).is_absolute() else str(assembly)
    path = _receipt_path(root, assembly)
    if build != path.parent or not re.fullmatch(r"[a-f0-9]{40,64}", tree):
        raise ValueError("Hardware dependency receipt requires the staged tree and owned build")
    path.unlink(missing_ok=True)
    cache = (build / "CMakeCache.txt").read_text()
    ninja = re.search(r"^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$", cache, re.M)
    selected = re.search(r"^NEXUS_ASSEMBLY_FILE:(?:FILEPATH|PATH|STRING|UNINITIALIZED)=(.+)$", cache, re.M)
    if not ninja or not selected or Path(selected[1]).resolve() != root / assembly:
        return False

    def tool(*args):
        return subprocess.check_output([ninja[1], "-t", *args], cwd=build,
                                       text=True, timeout=60)

    inputs, objects = set(), set()

    def add(value):
        file = (build / value).resolve()
        if file.is_relative_to(root) and not file.is_relative_to(root / "build"):
            inputs.add(file.relative_to(root).as_posix())

    for command in tool("commands", "nexus_firmware").splitlines():
        argv = shlex.split(command)
        if "-c" in argv and "-o" in argv:
            objects.add(argv[argv.index("-o") + 1])
            add(argv[argv.index("-c") + 1])
    if not objects:
        return False
    found = set()
    for line in tool("deps", *sorted(objects)).splitlines():
        if not line:
            continue
        if line.startswith("    "):
            add(line[4:])
        else:
            output, marker, details = line.partition(": #deps ")
            if not marker or not details.endswith("(VALID)"):
                return False
            found.add(output)
    if found != objects:
        return False
    # Configuration facts may live outside the board tree. A subsequent change
    # to any of them invalidates negative compiler evidence for this image.
    for manifest in ("input_paths", "configuration_code_paths"):
        for value in json.loads((build / "generated" / (manifest + ".json")).read_text()).values():
            add(value)
    ordered = sorted(inputs)
    data = {"schema": 1, "tree": tree, "assembly": assembly, "inputs": ordered,
            "sha256": hashlib.sha256("\0".join(ordered).encode()).hexdigest()}
    with tempfile.NamedTemporaryFile(mode="w", dir=build, delete=False) as output:
        json.dump(data, output)
    os.replace(output.name, path)
    return True
