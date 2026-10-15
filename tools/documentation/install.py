#!/usr/bin/env python3
"""Prepare the exact documentation closure without privileged installation.

Preparation may download locked bytes. Formal documentation builds use the
resulting prefix offline. An existing prefix is never overwritten implicitly.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import venv
import zipfile

ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / "dependencies/docs-tools.lock.json"


def digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def validate_lock(locked: dict) -> None:
    """Reject incomplete or ambiguous archive identities before preparation."""
    if locked.get("schema_version") != 1:
        raise ValueError("unsupported documentation lock schema")
    target = locked.get("target", {})
    if (target.get("system"), target.get("machine"), target.get("python")) != (
            "Linux", "x86_64", "3.12"):
        raise ValueError("unsupported exact documentation target tuple")
    names = set()
    for group in ("wheels", "native", "doxygen"):
        entries = ([locked.get(group)] if group == "doxygen"
                   else locked.get(group))
        if not isinstance(entries, list) or not entries:
            raise ValueError("empty documentation closure: " + group)
        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError("invalid archive identity: " + group)
            name = entry.get("filename", "")
            if (not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._+~-]*", name)
                    or name in names):
                raise ValueError("unsafe or duplicate archive filename")
            names.add(name)
            if not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", "")):
                raise ValueError("invalid archive digest: " + name)
            if type(entry.get("size")) is not int or entry["size"] <= 0:
                raise ValueError("invalid archive size: " + name)
            if not entry.get("url", "").startswith("https://"):
                raise ValueError("archive URL must use HTTPS: " + name)
            if not entry.get("licenses"):
                raise ValueError("missing archive license inventory: " + name)
    normalize = lambda value: re.sub(r"[-_.]+", "-", value).lower()
    wheels = {normalize(w["name"]): w["version"] for w in locked["wheels"]}
    if len(wheels) != len(locked["wheels"]):
        raise ValueError("duplicate Python distribution")
    for name, version in locked.get("direct_requirements", {}).items():
        if wheels.get(normalize(name)) != version:
            raise ValueError("direct requirement absent from closure: " + name)


def verify_blob(path: Path, locked: dict) -> None:
    if not path.is_file() or path.is_symlink():
        raise ValueError("missing regular archive: " + str(path))
    if path.stat().st_size != locked["size"]:
        raise ValueError("archive size mismatch: " + path.name)
    if digest(path) != locked["sha256"]:
        raise ValueError("archive digest mismatch: " + path.name)


def verify_requirements(path: Path, locked: dict) -> None:
    """The pip input is a derived view of the authoritative closure."""
    actual = [line.strip() for line in path.read_text().splitlines()
              if line.strip() and not line.lstrip().startswith("#")]
    expected = [f'{w["name"]}=={w["version"]} --hash=sha256:{w["sha256"]}'
                for w in locked["wheels"]]
    if actual != expected:
        raise ValueError("documentation requirements differ from lock")


def fetch_blob(cache: Path, locked: dict, *, offline: bool) -> Path:
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / locked["filename"]
    if path.exists():
        verify_blob(path, locked)
        return path
    if offline:
        raise ValueError("offline archive missing: " + str(path))
    temporary = path.with_suffix(path.suffix + ".download")
    try:
        with urllib.request.urlopen(locked["url"], timeout=60) as source:
            with temporary.open("wb") as destination:
                shutil.copyfileobj(source, destination)
        verify_blob(temporary, locked)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
    return path


def extract_tar(archive: Path, destination: Path) -> None:
    """Official Doxygen archive contains only regular files and directories."""
    with tarfile.open(archive) as contents:
        for member in contents.getmembers():
            path = PurePosixPath(member.name)
            if (path.is_absolute() or ".." in path.parts or
                    not (member.isfile() or member.isdir())):
                raise ValueError("unsafe archive member: " + member.name)
        contents.extractall(destination, filter="data")


def tree_files(prefix: Path) -> dict:
    excluded = {"__pycache__", "font-cache", "licenses"}
    return {p.relative_to(prefix).as_posix(): digest(p)
            for p in sorted(prefix.rglob("*")) if p.is_file()
            and not p.is_symlink() and not excluded.intersection(p.parts)
            and p.name != "installation.json" and p.suffix != ".pyc"}


def tree_links(prefix: Path) -> dict:
    return {p.relative_to(prefix).as_posix(): os.readlink(p)
            for p in sorted(prefix.rglob("*")) if p.is_symlink()}


def verify_installed(prefix: Path, receipt: dict) -> None:
    files = receipt.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError("empty installed tool inventory")
    if set(tree_files(prefix)) != set(files):
        raise ValueError("installed file inventory changed")
    for name, expected in files.items():
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe installed inventory path")
        path = prefix / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError("installed tool drift: " + name)
    if "links" in receipt and tree_links(prefix) != receipt["links"]:
        raise ValueError("installed link inventory changed")


def diagnose(prefix: Path | str | None = None) -> dict:
    """Validate the receipt against the current lock and observed bytes."""
    folder = Path(prefix or ROOT / "build/docs-tools").resolve()
    receipt_path = folder / "installation.json"
    if not receipt_path.is_file():
        raise ValueError("documentation tools not installed: " + str(folder))
    receipt = json.loads(receipt_path.read_text())
    if receipt.get("lock_sha256") != digest(LOCK):
        raise ValueError("documentation installation uses a different lock")
    verify_installed(folder, receipt)
    if receipt.get("elf_closure"):
        observe_elf_closure(folder, [folder / path
                                    for path in receipt["elf_closure"]])
    for name, expected in receipt.get("licenses", {}).items():
        path = folder / "licenses" / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError("installed license drift: " + name)
    return {"prefix": str(folder), "lock_sha256": receipt["lock_sha256"],
            "tools": receipt["tools"], "target": receipt["target"],
            "installation_receipt": str(receipt_path),
            "installation_sha256": digest(receipt_path)}


def environment(prefix: Path | str | None = None) -> dict:
    """Return a verified subprocess environment; never install implicitly."""
    observed = diagnose(prefix)
    folder = Path(observed["prefix"])
    values = dict(os.environ)
    values["PATH"] = os.pathsep.join((str(folder / "bin"),
                                      str(folder / "venv/bin"),
                                      values.get("PATH", "")))
    values["PYTHONNOUSERSITE"] = "1"
    values["NEXUS_DOCS_TOOL_PREFIX"] = str(folder)
    values["NEXUS_DOCS_TOOL_IDENTITY"] = observed["installation_receipt"]
    return values


def shell_wrapper(prefix: Path, tool: str, executable: Path) -> None:
    # The loader and all non-kernel runtime libraries come from locked debs.
    # Paths are resolved from the installed wrapper, independently of cwd.
    script = """#!/bin/sh
set -eu
prefix=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export GVBINDIR="$prefix/native/usr/lib/x86_64-linux-gnu/graphviz"
export FONTCONFIG_FILE="$prefix/fonts.conf"
export FONTCONFIG_PATH="$prefix"
unset LD_PRELOAD LD_AUDIT LD_LIBRARY_PATH
library_path="$prefix/native/usr/lib/x86_64-linux-gnu"
library_path="$library_path:$prefix/native/lib/x86_64-linux-gnu"
exec "$prefix/native/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2" \\
  --library-path "$library_path" \\
  "$prefix/EXECUTABLE" "$@"
""".replace("EXECUTABLE", executable.relative_to(prefix).as_posix())
    path = prefix / "bin" / tool
    path.write_text(script)
    path.chmod(0o755)


def observe_elf_closure(prefix: Path, executables: list[Path]) -> dict:
    """Prove tools and renderer libraries avoid host fallback."""
    native = prefix / "native"
    directory = native / "usr/lib/x86_64-linux-gnu"
    loader = directory / "ld-linux-x86-64.so.2"
    library_path = str(directory) + ":" + str(native / "lib/x86_64-linux-gnu")
    observed = {}
    clean_env = {k: v for k, v in os.environ.items() if not k.startswith("LD_")}
    for executable in executables:
        result = subprocess.run([str(loader), "--library-path", library_path,
                                 "--list", str(executable)], check=True,
                                text=True, capture_output=True, env=clean_env,
                                timeout=60)
        paths = re.findall(r"=>\s+(/[^\s]+)", result.stdout)
        if not paths or "not found" in result.stdout:
            raise ValueError("unresolved native tool closure")
        for path in paths:
            if not Path(path).resolve().is_relative_to(native.resolve()):
                raise ValueError("native library escaped prefix: " + path)
        observed[executable.relative_to(prefix).as_posix()] = result.stdout
    return observed


def collect_common_licenses(native: Path, output: Path) -> None:
    """Copy complete shared texts and reject unresolved copyright references."""
    shared = native / "usr/share/common-licenses"
    if not shared.is_dir():
        raise ValueError("shared license archive missing")
    destination = output / "common-licenses"
    destination.mkdir(exist_ok=True)
    for source in shared.iterdir():
        if source.is_file():
            if not source.resolve().is_relative_to(shared.resolve()):
                raise ValueError("shared license link escapes archive")
            shutil.copyfile(source, destination / source.name)
    for copyright in output.glob("*.txt"):
        for name in re.findall(r"/usr/share/common-licenses/([A-Za-z0-9.\-]+)",
                               copyright.read_text(errors="replace")):
            name = name.rstrip(".")
            if not (destination / name).is_file():
                raise ValueError("unresolved shared license: " + name)


def prepare(prefix: Path, cache: Path, *, offline: bool,
            lock_path: Path = LOCK) -> dict:
    locked = json.loads(lock_path.read_text())
    validate_lock(locked)
    if lock_path.resolve() == LOCK.resolve():
        verify_requirements(ROOT / "dependencies/docs-python-linux-cp312.txt",
                            locked)
    if (platform.system(), platform.machine(), sys.version_info[:2]) != (
            "Linux", "x86_64", (3, 12)):
        raise ValueError("install requires Linux x86_64 CPython 3.12")
    if prefix.exists():
        raise ValueError("prefix exists; use a fresh prefix: " + str(prefix))
    # Verify the complete archive closure before creating any usable prefix.
    archives = {}
    for entry in locked["wheels"] + locked["native"] + [locked["doxygen"]]:
        folder = cache / ("wheels" if entry in locked["wheels"] else "archives")
        archives[entry["filename"]] = fetch_blob(folder, entry, offline=offline)
    prefix.mkdir(parents=True)
    try:
        native = prefix / "native"
        native.mkdir()
        for entry in locked["native"]:
            subprocess.run(["dpkg-deb", "--extract",
                            str(archives[entry["filename"]]), str(native)],
                           check=True, timeout=120)
        extract_tar(archives[locked["doxygen"]["filename"]], prefix)
        plugins = native / "usr/lib/x86_64-linux-gnu/graphviz"
        # Remove unsupported renderers rather than loading host GTK/GD/GS ports.
        allowed = ("libgvplugin_core.", "libgvplugin_dot_layout.",
                   "libgvplugin_pango.")
        for path in plugins.iterdir():
            if path.is_file() or path.is_symlink():
                if not path.name.startswith(allowed):
                    path.unlink()
        venv.EnvBuilder(with_pip=True).create(prefix / "venv")
        python = prefix / "venv/bin/python"
        requirements = prefix / "python-requirements.txt"
        requirements.write_text("".join(
            f'{w["name"]}=={w["version"]} --hash=sha256:{w["sha256"]}\n'
            for w in locked["wheels"]))
        subprocess.run([str(python), "-m", "pip", "install", "--no-index",
                        "--find-links", str(cache / "wheels"),
                        "--require-hashes", "-r", str(requirements)],
                       check=True, timeout=300)
        subprocess.run([str(python), "-m", "pip", "check"], check=True,
                       timeout=60)
        (prefix / "bin").mkdir()
        shell_wrapper(prefix, "dot", native / "usr/bin/dot")
        shell_wrapper(prefix, "fc-match", native / "usr/bin/fc-match")
        shell_wrapper(prefix, "doxygen", prefix / "doxygen-1.14.0/bin/doxygen")
        # Exact CJK fonts, independent of the host's installed font set.
        from xml.sax.saxutils import escape
        (prefix / "fonts.conf").write_text(
            '<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd">\n'
            '<fontconfig><dir>' + escape(str(native / "usr/share/fonts")) +
            '</dir><cachedir>' + escape(str(prefix / "font-cache")) +
            '</cachedir></fontconfig>\n')
        subprocess.run([str(prefix / "bin/dot"), "-c"], check=True,
                       timeout=60)
        executables = [native / "usr/bin/dot", native / "usr/bin/fc-match",
                       prefix / "doxygen-1.14.0/bin/doxygen"]
        executables += [path for path in plugins.glob("*.so.*")
                        if not path.is_symlink()]
        elf_closure = observe_elf_closure(prefix, executables)
        # Licenses are payload, not just unverified registry labels.
        license_dir = prefix / "licenses"
        license_dir.mkdir()
        for entry in locked["native"]:
            for name in entry["licenses"]:
                source = native / name
                if not source.is_file():
                    raise ValueError("missing native license: " + name)
                shutil.copyfile(source, license_dir / (entry["name"] + ".txt"))
        shutil.copyfile(prefix / locked["doxygen"]["licenses"][0],
                        license_dir / "doxygen.txt")
        for entry in locked["wheels"]:
            with zipfile.ZipFile(archives[entry["filename"]]) as wheel:
                for number, name in enumerate(entry["licenses"]):
                    if name.startswith("vendored:"):
                        source = ROOT / name.removeprefix("vendored:")
                        content = source.read_bytes()
                        if hashlib.sha256(content).hexdigest() != entry[
                                "license_source"]["text_sha256"]:
                            raise ValueError("vendored license digest mismatch")
                    else:
                        if wheel.getinfo(name).is_dir():
                            continue
                        content = wheel.read(name)
                    destination = license_dir / (entry["name"] +
                                                 f"-{number}.txt")
                    destination.write_bytes(content)
        collect_common_licenses(native, license_dir)
        observations = {}
        commands = (
            ("doxygen", [str(prefix / "bin/doxygen"), "--version"]),
            ("graphviz", [str(prefix / "bin/dot"), "-V"]),
            ("font", [str(prefix / "bin/fc-match"), "Noto Sans CJK SC"]))
        for name, command in commands:
            result = subprocess.run(command, text=True, capture_output=True,
                                    check=True, timeout=60)
            observations[name] = (result.stdout + result.stderr).strip()
        if '"Noto Sans CJK SC"' not in observations["font"]:
            raise ValueError("exact CJK font resolution failed")
        receipt = {"schema_version": 1, "lock_sha256": digest(lock_path),
                   "python": sys.version, "target": locked["target"],
                   "host_kernel": platform.release(),
                   "tools": observations, "files": tree_files(prefix),
                   "links": tree_links(prefix),
                   "elf_closure": elf_closure,
                   "archives": {e["filename"]: e["sha256"] for e in
                                locked["wheels"] + locked["native"] +
                                [locked["doxygen"]]},
                   "licenses": {p.relative_to(license_dir).as_posix(): digest(p)
                                for p in license_dir.rglob("*") if p.is_file()}}
        verify_installed(prefix, receipt)
        (prefix / "installation.json").write_text(
            json.dumps(receipt, indent=2) + "\n")
        return receipt
    except BaseException:
        # Remove only the prefix created by this invocation. No success receipt
        # survives a failed command, partial extraction or font mismatch.
        shutil.rmtree(prefix)
        raise


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args(argv)
    try:
        receipt = prepare(args.prefix.resolve(), args.cache.resolve(),
                          offline=args.offline)
        print(json.dumps({"prefix": str(args.prefix.resolve()),
                          "tools": receipt["tools"],
                          "lock_sha256": receipt["lock_sha256"]}, indent=2))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print("documentation preparation failed: " + str(error),
              file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
