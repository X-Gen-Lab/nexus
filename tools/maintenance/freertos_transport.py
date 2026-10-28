"""Verify the exact FreeRTOS nested-fetch edit without changing kernel code.

Git checkout admission proves the complete parent and tree relationship. SDK
admission checks exported review material only; its caller must first establish
trusted archive identity. Neither mode fetches objects or applies the patch.
"""
from __future__ import annotations
import difflib
import hashlib
import os
from pathlib import Path
import re
import subprocess

from tools.configure.metadata import fields, parse_json, snapshot
from tools.maintenance import source_catalogue as catalogue

DIRECTORY = "ext/freertos"
REVIEW_FILES = (
    "dependencies/freertos-transport.lock.json",
    "dependencies/reviews/freertos/upstream.gitmodules",
    "dependencies/reviews/freertos/transport.patch",
)
SHA = re.compile(r"[0-9a-f]{40}")
DIGEST = re.compile(r"[0-9a-f]{64}")


def _git(source, *arguments, parent_index=False):
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    environment.update({"GIT_CONFIG_NOSYSTEM": "1",
                        "GIT_CONFIG_GLOBAL": os.devnull,
                        "GIT_OPTIONAL_LOCKS": "0",
                        "GIT_NO_REPLACE_OBJECTS": "1"})
    if parent_index and "GIT_INDEX_FILE" in os.environ:
        environment["GIT_INDEX_FILE"] = os.environ["GIT_INDEX_FILE"]
    process = subprocess.run(["git", "-c", "core.fsmonitor=false", "-C",
        str(source), *arguments], capture_output=True, env=environment,
        timeout=30)
    if process.returncode:
        raise ValueError("Cannot verify FreeRTOS Git objects: " + arguments[0])
    return process.stdout


def _text(source, *arguments, parent_index=False):
    return _git(source, *arguments,
                parent_index=parent_index).decode("utf-8").strip()


def _own_checkout(source):
    marker = source / ".git"
    if (any(path.is_symlink() for path in (source, *source.parents)) or
            marker.is_symlink() or not marker.exists() or
            Path(_text(source, "rev-parse", "--show-toplevel")) != source):
        raise ValueError("FreeRTOS requires its own initialized Git checkout")


def _selected(root, selected_commit, *, parent_index=True):
    _own_checkout(root)
    row = _text(root, "ls-files", "--stage", "--", DIRECTORY,
                parent_index=parent_index)
    match = re.fullmatch(r"160000 ([0-9a-f]{40}) 0\text/freertos", row)
    if match is None:
        raise ValueError("FreeRTOS requires one exact active-index gitlink")
    if selected_commit is not None and selected_commit != match[1]:
        raise ValueError("FreeRTOS selected commit differs from parent gitlink")
    return match[1]


def _commit(value):
    if not isinstance(value, str) or SHA.fullmatch(value) is None:
        raise ValueError("Exact FreeRTOS selected commit required")
    return value


def _sha(data):
    return hashlib.sha256(data).hexdigest()


def _blob(data):
    header = b"blob " + str(len(data)).encode() + b"\0"
    return hashlib.sha1(header + data).hexdigest()


def _patch(baseline, selected):
    """Canonical text patch with full Git blob IDs."""
    body = "".join(difflib.unified_diff(
        baseline.decode("utf-8").splitlines(keepends=True),
        selected.decode("utf-8").splitlines(keepends=True),
        fromfile="a/.gitmodules", tofile="b/.gitmodules", n=3))
    return ("diff --git a/.gitmodules b/.gitmodules\nindex " +
            _blob(baseline) + ".." + _blob(selected) +
            " 100644\n" + body).encode()


def _transformed(baseline):
    catalogue.declared_modules(baseline, parent=DIRECTORY, upstream=True)
    if not baseline.endswith(b"\n"):
        raise ValueError("FreeRTOS reviewed declaration must end with newline")
    result = baseline
    children = catalogue.children(DIRECTORY)
    if len(children) != 2:
        raise ValueError("FreeRTOS review requires two nested sources")
    for child in children:
        if child.upstream_url == child.fetch_url:
            raise ValueError("FreeRTOS review requires two URL changes")
        pattern = (rb"(?m)^([ \t]*url[ \t]*=[ \t]*)" +
                   re.escape(child.upstream_url.encode()) + rb"([ \t]*\r?)$")
        result, count = re.subn(pattern,
            lambda match: match[1] + child.fetch_url.encode() + match[2],
            result)
        if count != 1:
            raise ValueError("FreeRTOS requires one URL per nested source")
    catalogue.declared_modules(result, parent=DIRECTORY)
    return result


def _review(root, selected_commit):
    paths = tuple(root / name for name in REVIEW_FILES)
    present = tuple(path.exists() or path.is_symlink() for path in paths)
    if not any(present):
        if any(child.upstream_url != child.fetch_url
               for child in catalogue.children(DIRECTORY)):
            raise ValueError("Controlled FreeRTOS sources require review")
        return None, dict.fromkeys(paths)
    if not all(present):
        raise ValueError("FreeRTOS transport review material is incomplete")
    observations = {}
    payloads = []
    for path in paths:
        data, identity = snapshot(path)
        observations[path] = identity
        payloads.append(data)
    lock = parse_json(payloads[0])
    fields(lock, {"schema", "kind", "dependency", "upstream_base",
        "selected_commit", "changed_paths", "baseline_modules_sha256",
        "selected_modules_sha256", "patch_sha256", "policy"},
        "FreeRTOS transport review")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["kind"] != "freertos-transport" or
            lock["dependency"] != "freertos" or
            lock["changed_paths"] != [".gitmodules"] or
            lock["policy"] != "only-approved-nested-fetch-urls"):
        raise ValueError("Unsupported FreeRTOS transport review contract")
    _commit(lock["upstream_base"])
    _commit(lock["selected_commit"])
    if (lock["selected_commit"] != selected_commit or
            lock["upstream_base"] == selected_commit):
        raise ValueError("FreeRTOS review selected commit differs from gitlink")
    for name in ("baseline_modules_sha256", "selected_modules_sha256",
                 "patch_sha256"):
        if (not isinstance(lock[name], str) or
                DIGEST.fullmatch(lock[name]) is None):
            raise ValueError("FreeRTOS review requires exact material SHA256")
    baseline = payloads[1]
    selected = _transformed(baseline)
    if (lock["baseline_modules_sha256"] != _sha(baseline) or
            lock["selected_modules_sha256"] != _sha(selected) or
            lock["patch_sha256"] != _sha(payloads[2])):
        raise ValueError("FreeRTOS review hash or URL transform differs")
    if payloads[2] != _patch(baseline, selected):
        raise ValueError("FreeRTOS review patch differs from exact URL edits")
    return {"lock": lock, "baseline": baseline, "selected": selected,
            "patch": payloads[2]}, observations


def _tree_proof(source, review, selected_commit):
    base = review["lock"]["upstream_base"]
    if any(_git(source, "cat-file", "-t", commit) != b"commit\n"
           for commit in (base, selected_commit)):
        raise ValueError("FreeRTOS review requires actual commit objects")
    # rev-list follows local grafts; the immutable commit header owns parents.
    raw = _git(source, "cat-file", "commit", selected_commit)
    header = raw.partition(b"\n\n")[0]
    parents = re.findall(rb"(?m)^parent ([0-9a-f]{40})$", header)
    if parents != [base.encode()]:
        raise ValueError("FreeRTOS transport commit requires one exact parent")
    changes = _git(source, "diff-tree", "--no-commit-id", "--no-renames",
                   "--raw", "-r", "-z", base, selected_commit)
    expected = (b":100644 100644 " + _blob(review["baseline"]).encode() + b" " +
                _blob(review["selected"]).encode() + b" M\0.gitmodules\0")
    if changes != expected:
        raise ValueError("FreeRTOS complete tree or file mode changed")
    if (_git(source, "show", base + ":.gitmodules") != review["baseline"] or
            _git(source, "show", selected_commit + ":.gitmodules") !=
            review["selected"]):
        raise ValueError("FreeRTOS Git declaration differs from URL review")
    change = _git(source, "diff", "--full-index", "--no-ext-diff",
        "--no-textconv", "--no-renames", "--no-color", "--no-relative",
        "--src-prefix=a/", "--dst-prefix=b/", "--diff-algorithm=myers",
        "--no-indent-heuristic", "--unified=3",
        base, selected_commit, "--", ".gitmodules")
    if change != review["patch"]:
        raise ValueError("FreeRTOS Git patch differs from review material")


def _unchanged(observations):
    changed = any(snapshot(path)[1] != identity
                  for path, identity in observations.items()
                  if identity is not None)
    if changed or any(path.exists() or path.is_symlink()
                      for path, identity in observations.items()
                      if identity is None):
        raise ValueError("FreeRTOS review inputs changed during verification")


def _checkout_identity(source, expected_commit, expected_origin):
    _own_checkout(source)
    actual = _text(source, "rev-parse", "HEAD")
    if actual != expected_commit:
        raise ValueError("FreeRTOS checkout differs from selected gitlink")
    origin = _text(source, "config", "--get-all", "remote.origin.url")
    if origin != expected_origin:
        raise ValueError("FreeRTOS origin differs from approved fetch URL")
    if _text(source, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("FreeRTOS dependency checkout is dirty")


def _children(root, source, selected_commit):
    for child in catalogue.children(DIRECTORY):
        relative = catalogue.relative_path(child)
        row = _text(source, "ls-tree", selected_commit, "--", relative)
        match = re.fullmatch(r"160000 commit ([0-9a-f]{40})\t" +
                             re.escape(relative), row)
        if match is None:
            raise ValueError("FreeRTOS nested gitlink missing from tree")
        _checkout_identity(root / child.path, match[1], child.fetch_url)


def provisional_migration_revisions(root, selected_commit):
    """Read candidate revisions from review bytes, without admitting Git source.

    This permits early rejection before an explicit fetch. Preparation must run
    migration_revisions() after fetching and before changing any checkout.
    """
    root = Path(root).absolute()
    selected_commit = _commit(selected_commit)
    review, observations = _review(root, selected_commit)
    _unchanged(observations)
    return frozenset({selected_commit} if review is None else
                     {selected_commit, review["lock"]["upstream_base"]})


def migration_revisions(root, selected_commit):
    """Allow an official old checkout only after proving the target Git tree.

    Preparation owns clean-checkout and origin preflight. This function inspects
    target objects without requiring HEAD already to be at the new selection.
    """
    root = Path(root).absolute()
    selected_commit = _commit(selected_commit)
    review, observations = _review(root, selected_commit)
    if review is None:
        _unchanged(observations)
        return frozenset({selected_commit})
    source = root / DIRECTORY
    _own_checkout(source)
    _tree_proof(source, review, selected_commit)
    _unchanged(observations)
    return frozenset({selected_commit, review["lock"]["upstream_base"]})


def verify(root, *, selected_commit=None, require_git=True, parent_index=True):
    """Admit Git source or review bytes inside an already trusted SDK.

    Staged commit gates honor the active parent index. Clean preparation passes
    parent_index=False to ignore a caller-supplied GIT_INDEX_FILE.
    """
    root = Path(root).absolute()
    if require_git:
        selected_commit = _selected(root, selected_commit,
                                    parent_index=parent_index)
    else:
        selected_commit = _commit(selected_commit)
    source = root / DIRECTORY
    module_path = source / ".gitmodules"
    module_bytes, module_identity = snapshot(module_path)
    review, observations = _review(root, selected_commit)
    observations[module_path] = module_identity
    catalogue.declared_modules(module_bytes, parent=DIRECTORY)
    if review is not None and module_bytes != review["selected"]:
        raise ValueError("FreeRTOS declaration differs from reviewed URL edits")
    if require_git:
        _own_checkout(source)
        if review is not None:
            _tree_proof(source, review, selected_commit)
        expected_origin = catalogue.dependency("freertos").fetch_url
        _checkout_identity(source, selected_commit, expected_origin)
        committed = _git(source, "show", selected_commit + ":.gitmodules")
        if committed != module_bytes:
            raise ValueError("FreeRTOS declaration differs from Git blob")
        _children(root, source, selected_commit)
        _checkout_identity(source, selected_commit, expected_origin)
        _selected(root, selected_commit, parent_index=parent_index)
    _unchanged(observations)
    return {"mode": "metadata-only" if review is not None else "upstream",
            "selected_commit": selected_commit,
            "upstream_base": review["lock"]["upstream_base"] if review else
                             selected_commit,
            "verification": "git-tree" if require_git else "exported-materials"}
