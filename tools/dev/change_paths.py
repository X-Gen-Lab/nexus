"""One conservative path policy shared by commit gates and GitHub CI."""

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys

BEHAVIOR_DOCUMENTS = {
    "CONTRIBUTING.md", "CONTRIBUTING_CN.md",
    ".kiro/steering/comment-standards.md",
    "docs/sphinx/development/coding_standards.rst",
}


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
        result["workflows"] |= workflow
        result["code"] |= name in BEHAVIOR_DOCUMENTS or not document
    return result


def behavioral(name):
    result = classify([name])
    return result["code"] or result["workflows"]


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
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args(arguments)
    try:
        result = classify(changed_paths(args.root, args.base, args.head))
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
