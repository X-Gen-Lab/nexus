"""Verify only explicitly selected locked ST USB middleware sources."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json  # noqa: E402
from tools.maintenance import cmsis_device_lock, source_catalogue  # noqa: E402

REVIEWED = {
    "device": ("2.11.4", "2a0a3521ac4d84e6e494d37bed615e2d36c373f5",
               "b78e0cae5656ff8ac54f361521929162d96b6543",
               "8d0f585659f1666ca4fdbb13816a95b09189c30e62874fbbebfdd0cc93cd08c6"),
    "host": ("3.5.3", "9d46e44c858a77bf126920b7bcc258d59afa89b1",
             "fbc51c288c27e751943e3ea74f52d6fb716d762a",
             "875c3a9f8d7990ca087a77d390ac6b5be792967f5c49241a6d3117a1565b14be"),
}


def read_lock(root, role):
    """Read one already locked source declaration."""
    return parse_json(cmsis_device_lock.stable_snapshot(
        Path(root) / ("dependencies/stm32-usb-" + role + ".lock.json"))[0])


def verify(root, role, *, source_sdk=False):
    """Return the selected exact source identity after full byte admission."""
    if role not in REVIEWED:
        raise ValueError("ST USB requires an exact device or host role")
    root = Path(root).resolve()
    item = source_catalogue.dependency("stm32_usb_" + role)
    source = root / item.path
    version, commit, tree, mapping_hash = REVIEWED[role]
    lock = read_lock(root, role)
    fields(lock, {"schema", "name", "version", "commit", "tree", "upstream_url",
                  "fetch_url", "path", "license", "admission", "pairing", "files"},
           "ST USB source lock")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["name"] != item.identifier or lock["version"] != version or
            lock["commit"] != commit or lock["tree"] != tree or
            lock["upstream_url"] != item.upstream_url or
            lock["fetch_url"] != item.fetch_url or lock["path"] != item.path or
            lock["license"] != "LicenseRef-ST-SLA0044" or
            lock["admission"] != "organization_gitlink"):
        raise ValueError("ST USB requires its exact reviewed source identity")
    mapping = json.dumps(lock["files"], sort_keys=True, separators=(",", ":"))
    if hashlib.sha256(mapping.encode()).hexdigest() != mapping_hash:
        raise ValueError("ST USB source file map differs from the reviewed lock")
    hal = parse_json((root / "dependencies/stm32f4-hal.lock.json").read_bytes())
    if lock["pairing"] != hal["pairing"]:
        raise ValueError("ST USB requires its exact official CubeF4 pairing")
    if source.is_symlink() or source.resolve() != source or not source.is_dir():
        raise ValueError("ST USB sources require a canonical regular directory")
    if source_sdk:
        if (source / ".git").exists() or (source / ".git").is_symlink():
            raise ValueError("Packaged ST USB cannot contain Git metadata")
    else:
        cmsis_device_lock.verify_git(source, {"commit": commit},
                                      {"origin": item.upstream_url})
    inputs = [str(root / item.source_lock), str(Path(__file__).resolve())]
    identities = []
    for name, expected in sorted(lock["files"].items()):
        path = source / name
        raw, identity = cmsis_device_lock.stable_snapshot(path)
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError("ST USB source bytes differ from lock: " + name)
        identities.append((path, identity))
        inputs.append(str(path))
    if not source_sdk:
        cmsis_device_lock.verify_git(source, {"commit": commit},
                                      {"origin": item.upstream_url})
    for path, before in identities:
        _, after = cmsis_device_lock.stable_snapshot(path)
        if before != after:
            raise ValueError("ST USB source changed during admission")
    return {"role": role, "version": version, "commit": commit, "tree": tree,
            "configure_inputs": inputs}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--role", choices=tuple(REVIEWED), required=True)
    arguments = parser.parse_args(argv)
    try:
        manifest_path = arguments.root / ".nexus-source-sdk.json"
        packaged = manifest_path.exists() or manifest_path.is_symlink()
        if packaged:
            sys.path.insert(0, str(arguments.root / "cmake/package"))
            import package_source_sdk
            manifest = package_source_sdk.verify(arguments.root)
            path = "vendors/st/usb_" + arguments.role
            records = [record for record in manifest["dependencies"]
                       if record["path"] == path]
            if len(records) != 1 or (records[0]["commit"], records[0]["tree"]) != (
                    REVIEWED[arguments.role][1], REVIEWED[arguments.role][2]):
                raise ValueError("Source SDK lacks the selected exact ST USB source")
        result = verify(arguments.root, arguments.role, source_sdk=packaged)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
