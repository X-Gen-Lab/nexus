"""Close a separate Git consumer project without mounting host repository data."""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess

from tools.evidence.common import EvidenceError, fields, regular_file, sha256_value
from tools.evidence.identity import observe_git_source
from tools.evidence.inputs import records_identity, tree_identity


def relative_path(value: str, label: str, *, allow_root: bool = False) -> Path:
    """Validate portable paths before they enter Docker bind or command syntax."""
    if (not isinstance(value, str) or not value or "\\" in value or "," in value or
            "\0" in value or Path(value).is_absolute() or
            any(part in ("", "..", ".") for part in value.split("/"))):
        if not (allow_root and value == "."):
            raise EvidenceError(label + " requires a contained relative path")
    return Path(value)


def _files(root: Path, project: Path) -> list[tuple[str, Path, dict]]:
    result = subprocess.run(["git", "-C", str(root), "ls-tree", "-r", "--full-tree", "-z", "HEAD",
                             "--", project.as_posix()], capture_output=True,
        check=False, timeout=30, env={key: value for key, value in os.environ.items()
                                    if not key.startswith("GIT_")})
    if result.returncode:
        raise EvidenceError("external consumer needs a complete Git checkout")
    files = []
    for item in result.stdout.decode().split("\0"):
        if not item:
            continue
        metadata, name = item.split("\t", 1)
        mode, kind, blob = metadata.split()
        if mode == "160000":
            raise EvidenceError("consumer submodule is not an implicitly declared input")
        if mode not in {"100644", "100755"} or kind != "blob":
            raise EvidenceError("consumer requires regular committed files")
        path = regular_file(root / name, nonempty=False)
        size = path.stat().st_size
        actual_blob = hashlib.sha1() if len(blob) == 40 else hashlib.sha256()
        content = hashlib.sha256()
        actual_blob.update(f"blob {size}\0".encode())
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                actual_blob.update(block)
                content.update(block)
        executable = bool(path.stat().st_mode & 0o111)
        if (actual_blob.hexdigest() != blob or size != path.stat().st_size or
                executable != (mode == "100755")):
            raise EvidenceError("consumer working files differ from exact committed bytes/mode")
        relative = path.relative_to(root / project).as_posix()
        relative_path(relative, "consumer tracked file")
        files.append((relative, path, {"path": relative, "size": size,
                                      "sha256": content.hexdigest()}))
    if not files or not any(name == "CMakeLists.txt" for name, _, _ in files):
        raise EvidenceError("consumer project needs a tracked CMakeLists.txt")
    regular_file(root / project / "CMakeLists.txt")
    return sorted(files)


def contract(record: dict) -> None:
    """Validate archived declaration shape without consulting a live checkout."""
    fields(record, {"source", "project", "snapshot"})
    source = record["source"]
    fields(source, {"root", "commit", "tree", "dirty", "submodules"})
    if (not isinstance(source["root"], str) or not Path(source["root"]).is_absolute() or
            source["dirty"] is not False or not isinstance(source["submodules"], list) or
            len(source["submodules"]) > 4096 or any(not isinstance(item, str) or
            len(item) > 4096 for item in source["submodules"])):
        raise EvidenceError("clean bounded external Git source declaration required")
    for key in ("commit", "tree"):
        if not isinstance(source[key], str) or not re.fullmatch(
                r"(?:[0-9a-f]{40}|[0-9a-f]{64})", source[key]):
            raise EvidenceError("exact external Git object identities required")
    if len(source["commit"]) != len(source["tree"]):
        raise EvidenceError("external Git object formats differ")
    relative_path(record["project"], "consumer project", allow_root=True)
    snapshot = record["snapshot"]
    fields(snapshot, {"sha256", "files", "bytes"})
    sha256_value(snapshot["sha256"])
    if (type(snapshot["files"]) is not int or not 0 < snapshot["files"] <= 1000000 or
            type(snapshot["bytes"]) is not int or not 0 < snapshot["bytes"] < 2**64):
        raise EvidenceError("bounded nonempty external snapshot required")


def _identity(files: list[tuple[str, Path, dict]]) -> dict:
    return records_identity([record for _, _, record in files])


def identify(root: Path, project: str = ".") -> dict:
    """Bind a clean Git revision and only the selected tracked project bytes.

    Gitlinks outside this project are not mounted and need not be initialized.
    A gitlink inside the project is rejected: the platform SDK is a separate,
    fully verified declared input, never an implicit consumer dependency.
    """
    root = Path(root).absolute()
    selected = relative_path(project, "consumer project", allow_root=True)
    if root.resolve() != root or not root.is_dir():
        raise EvidenceError("external consumer requires a canonical directory")
    project_root = root / selected
    if (not project_root.is_dir() or any(path.is_symlink() for path in
            (project_root, *project_root.parents))):
        raise EvidenceError("external consumer project cannot contain symlink parents")
    files = _files(root, selected)
    source = observe_git_source(root)
    if source["dirty"]:
        raise EvidenceError("dirty external consumer cannot qualify reproduction")
    snapshot = _identity(files)
    if observe_git_source(root) != source or _identity(_files(root, selected)) != snapshot:
        raise EvidenceError("external consumer changed during input observation")
    record = {"source": source, "project": project, "snapshot": snapshot}
    contract(record)
    return record


def verify(record: dict) -> None:
    contract(record)
    if identify(Path(record["source"]["root"]), record["project"]) != record:
        raise EvidenceError("external consumer identity is stale or changed")


def copy(record: dict, destination: Path) -> dict:
    """Create a fresh .git-free snapshot, rejecting changes before or after copy."""
    verify(record)
    if destination.exists() or destination.is_symlink():
        raise EvidenceError("consumer snapshot requires a new destination")
    destination.mkdir(parents=True)
    try:
        root = Path(record["source"]["root"])
        for name, source, _ in _files(root, Path(record["project"])):
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        if tree_identity(destination) != record["snapshot"]:
            raise EvidenceError("external consumer snapshot differs from declared bytes")
        verify(record)
        return record["snapshot"]
    except BaseException:
        shutil.rmtree(destination)
        raise
