"""Require every external Action to match the reviewed full-SHA lock."""
import argparse
import json
from pathlib import Path
import re


def check(root):
    lock = json.loads((root / "dependencies/actions.lock.json").read_text())
    if lock.get("schema_version") != 1:
        raise ValueError("unsupported action lock schema")
    entries = lock.get("actions", [])
    expected = {}
    for entry in entries:
        repository, sha = entry["repository"], entry["sha"]
        if repository in expected or not re.fullmatch(r"[0-9a-f]{40}", sha):
            raise ValueError("duplicate repository or malformed locked SHA")
        expected[repository] = sha
    errors, count = [], 0
    files = set((root / ".github").rglob("*.yml")) | set((root / ".github").rglob("*.yaml"))
    for path in sorted(files):
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if line.lstrip().startswith("#"):
                continue
            match = re.search(r"(?:^|\s)uses:\s*([^\s#]+)", line)
            if not match:
                continue
            target = match.group(1).strip("'\"")
            if target.startswith("./"):
                continue
            count += 1
            action, separator, revision = target.rpartition("@")
            repository = "/".join(action.split("/")[:2])
            if not separator or not re.fullmatch(r"[0-9a-f]{40}", revision):
                errors.append(f"{path.relative_to(root)}:{number}: floating/invalid action {target}")
            elif expected.get(repository) != revision:
                errors.append(f"{path.relative_to(root)}:{number}: action does not match lock: {target}")
    if not count:
        errors.append("zero external action references")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    root = parser.parse_args().root
    try:
        errors = check(root)
    except (OSError, ValueError, KeyError, TypeError) as error:
        errors = [f"invalid action lock: {error}"]
    for error in errors:
        print(error)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
