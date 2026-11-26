"""One conservative path policy shared by commit gates and GitHub CI."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

BEHAVIOR_DOCUMENTS = {
    "AGENTS.md",
    "CONTRIBUTING.md", "CONTRIBUTING_CN.md",
    "docs/development/coding-standards.md",
}

# This handbook page and its translation are reviewed documentation-only inputs.
# Other paths retain full analysis until their role in the build is reviewed.
CODEQL_PAGE = "docs/development/workflow.md"
CODEQL_LOCALE = "docs/locale/zh_CN/LC_MESSAGES/development/workflow.po"
_OID = re.compile(r"[0-9a-f]{40}")
_FENCE = re.compile(r" {0,3}(`{3,}|~{3,})(.*)")
_PROSE = re.compile(r"[\w \n.,:;!?()/'\-`#，。；：？！、（）《》、“”‘’／—]+")


def _git(root, *arguments):
    return subprocess.run(["git", *arguments], cwd=root, capture_output=True,
                          check=True, timeout=30,
                          env={**os.environ, "GIT_NO_REPLACE_OBJECTS": "1"}).stdout


def _ordinary_prose(paragraph):
    """Accept ordinary unindented paragraphs and bounded literal inline spans."""
    if not _PROSE.fullmatch(paragraph) or not all(
            line and (line[0].isalpha() or line[0] == "`")
            for line in paragraph.splitlines()):
        return False
    spans = paragraph.split("`")
    return (len(spans) % 2 == 1 and
            all(span.strip() and "\n" not in span for span in spans[1::2]))


def _page_sections(text):
    """Keep Markdown structure, metadata, code blocks and directives byte-exact."""
    if re.search(r"^ {0,3}(:::|<)", text, re.MULTILINE):
        raise ValueError("reviewed page starts a directive or HTML block")
    protected, prose = [], set()
    fence = None
    for section in text.removesuffix("\n").split("\n\n"):
        structural = fence is not None
        for line in section.splitlines():
            marker = _FENCE.fullmatch(line)
            if marker:
                structural = True
                token, suffix = marker.groups()
                if fence is None:
                    fence = token
                elif token.startswith(fence) and not suffix.strip():
                    fence = None
        if not structural and _ordinary_prose(section):
            prose.add(" ".join(section.split()))
        else:
            protected.append(section)
    if fence is not None:
        raise ValueError("reviewed page has an unterminated fence")
    return protected, prose


def _locale_structure(text, source, prose):
    """Allow translations of verified prose and the corresponding source hash."""
    protected = []
    digest = hashlib.sha256(source.encode("utf-8")).hexdigest()
    for section in text.removesuffix("\n").split("\n\n"):
        if section.startswith('#\nmsgid ""\nmsgstr ""\n'):
            section = section.replace(f'"X-Nexus-Source-SHA256: {digest}\\n"',
                                      '"X-Nexus-Source-SHA256: <verified>\\n"')
        entry = re.fullmatch(r"(?:#:[^\n]*\n)*msgid (.+?)\nmsgstr (.+)",
                             section, flags=re.DOTALL)
        if entry:
            try:
                msgid, msgstr = ("".join(json.loads(line) for line in part.splitlines())
                                 for part in entry.groups())
            except (ValueError, TypeError):
                msgid, msgstr = "", ""
            # gettext wraps long paragraphs without changing their contents.
            if " ".join(msgid.split()) in prose and _ordinary_prose(msgstr):
                continue
        protected.append(section)
    return protected


def _codeql_text(root, revision, path):
    name = revision + ":" + path
    if int(_git(root, "cat-file", "-s", name)) > 64 * 1024:
        raise ValueError("reviewed documentation exceeds comparison bound")
    return _git(root, "cat-file", "blob", name).decode("utf-8")


def _codeql_prose_only(root, old, new):
    raw = _git(root, "diff", "--raw", "--no-abbrev", "--no-renames", "-z",
               "--no-ext-diff", "--ignore-submodules=none", old, new, "--")
    fields = raw.split(b"\x00")
    if not raw or fields.pop() or len(fields) % 2:
        return False
    paths = set()
    for metadata, path in zip(fields[::2], fields[1::2]):
        mode = rb":100644 100644 [0-9a-f]{40} [0-9a-f]{40} M"
        if (not re.fullmatch(mode, metadata) or
                path not in (CODEQL_PAGE.encode(), CODEQL_LOCALE.encode())):
            return False
        paths.add(path.decode())
    previous = _codeql_text(root, old, CODEQL_PAGE)
    current = _codeql_text(root, new, CODEQL_PAGE)
    old_structure, old_prose = _page_sections(previous)
    current_structure, current_prose = _page_sections(current)
    if old_structure != current_structure:
        return False
    if CODEQL_LOCALE in paths:
        old_locale = _codeql_text(root, old, CODEQL_LOCALE)
        new_locale = _codeql_text(root, new, CODEQL_LOCALE)
        return (_locale_structure(old_locale, previous, old_prose) ==
                _locale_structure(new_locale, current, current_prose))
    return True


def codeql_scope(root, event, base, head, checkout):
    """Permit skipping C++ only after proving the analyzed Git tree is prose-only."""
    decision = {"event": event, "base": base, "head": head, "checkout": checkout,
                "skip_cpp": False}
    if event not in {"push", "pull_request"}:
        return {**decision, "reason": "event requires full analysis"}
    try:
        if any(not _OID.fullmatch(oid or "") for oid in (base, head, checkout)):
            raise ValueError("full commit identities are required")
        if _git(root, "rev-parse", "HEAD").decode().strip() != checkout:
            raise ValueError("checkout does not match the event")
        ranges = [(base, checkout)]
        if event == "pull_request":
            parents = _git(root, "rev-list", "--parents", "-n", "1", checkout)
            if parents.decode().split() != [checkout, base, head]:
                raise ValueError("pull request merge parents differ from the event")
            ancestors = _git(root, "merge-base", "--all", base, head).decode().split()
            if len(ancestors) != 1:
                raise ValueError("pull request merge base is ambiguous")
            ranges.append((ancestors[0], head))
        else:
            if checkout != head:
                raise ValueError("push head differs from checkout")
            _git(root, "merge-base", "--is-ancestor", base, head)
        skip = all(_codeql_prose_only(root, old, new) for old, new in ranges)
    except (OSError, ValueError, subprocess.SubprocessError):
        return {**decision, "reason": "Git and content proof unavailable"}
    return {**decision, "skip_cpp": skip,
            "reason": ("reviewed prose change" if skip else
                       "full analysis required by changed inputs")}


def classify(names):
    """Unknown non-document inputs require execution, including gitlinks."""
    result = {"code": False, "docs": False, "workflows": False}
    for name in names:
        if (not isinstance(name, str) or not name or "\0" in name
                or name.startswith("/")
                or any(part in ("", ".", "..") for part in name.split("/"))):
            raise ValueError("Invalid repository-relative changed path")
        path = PurePosixPath(name)
        workflow = (name.startswith((".github/workflows/", ".github/actions/"))
                    or name in (".github/dependabot.yml",
                                ".github/dependabot.yaml"))
        document = (name.startswith("docs/")
                    or path.suffix.lower() in (".md", ".rst")
                    or path.name == "Doxyfile"
                    or path.name.startswith("Doxyfile."))
        result["docs"] |= document
        result["workflows"] |= workflow and not document
        result["code"] |= name in BEHAVIOR_DOCUMENTS or not document
    return result


def behavioral(name):
    result = classify([name])
    return result["code"] or result["workflows"]


def path_role(name):
    """Recognize reviewed ordinary inputs; unknown paths never justify a skip."""
    if not behavioral(name):
        return "documentation"
    path = PurePosixPath(name)
    if name.startswith("tests/behavior/") and (
            path.name == "CMakeLists.txt" or path.suffix.lower() in {
                ".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inc", ".s", ".cmake"}):
        return "behavior-test"
    if name.startswith(("tests/support/", "tests/integration/",
                        "cmake/package/verification/")):
        return "consumer-input"
    return "unknown"


def ci_scope(names):
    """Narrow ordinary tool jobs only for reviewed private behavior tests."""
    names = tuple(names)
    result = classify(names)
    result["tooling"] = any(path_role(name) not in {
        "documentation", "behavior-test"} for name in names)
    return result


def changed_paths(root, base=None, head="HEAD"):
    """Classify adds/deletes and both rename sides from actual Git bytes."""
    if base and base != "0" * 40:
        revisions = []
        for revision in (base, head):
            verified = subprocess.run(
                ["git", "rev-parse", "--verify", "--end-of-options",
                 revision + "^{commit}"], cwd=root,
                capture_output=True, text=True, check=True, timeout=30)
            revisions.append(verified.stdout.strip())
        command = ["git", "diff", "--name-only", "--no-renames", "-z",
                   *revisions, "--"]
    else:
        command = ["git", "ls-files", "-z"]
    result = subprocess.run(command, cwd=root, capture_output=True, check=True,
                            timeout=30)
    return [os.fsdecode(name) for name in result.stdout.split(b"\0") if name]


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--base", default=os.environ.get("NEXUS_CHANGE_BASE"))
    parser.add_argument("--head", default=os.environ.get("NEXUS_CHANGE_HEAD",
                                                       "HEAD"))
    parser.add_argument("--codeql", action="store_true")
    parser.add_argument("--event", default=os.environ.get("NEXUS_CHANGE_EVENT"))
    parser.add_argument("--checkout", default=os.environ.get("NEXUS_CHANGE_CHECKOUT"))
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args(arguments)
    try:
        if args.codeql:
            result = codeql_scope(args.root, args.event, args.base, args.head,
                                  args.checkout)
            print(json.dumps(result, sort_keys=True))
            if args.github_output:
                with args.github_output.open("a") as output:
                    output.write("skip_cpp=" + str(result["skip_cpp"]).lower() + "\n")
            return 0
        result = ci_scope(changed_paths(args.root, args.base, args.head))
        if args.github_output:
            with args.github_output.open("a") as output:
                for name, value in result.items():
                    output.write(name + "=" + str(value).lower() + "\n")
        print(json.dumps(result, sort_keys=True))
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as error:
        print("Change classification rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
