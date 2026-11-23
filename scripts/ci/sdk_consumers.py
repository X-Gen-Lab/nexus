"""Package the current commit, relocate its SDK and run affected consumers."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tomllib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev.change_paths import changed_paths, classify
from tools.maintenance.consumer_profiles import (SERVICE_CONSUMERS,
                                                 service_directory)
from tools.maintenance.source_catalogue import ALL_SOURCE_DEPENDENCIES

# These suppliers are selected by consumer CMake helpers or provider modes.
# Other optional suppliers come from the canonical consumer assemblies below.
EXPLICIT_COMPONENTS = ("mcuboot", "mqtt", "network", "network-loaned-rx",
                       "st-usb-device", "st-usb-host", "tls")
DEVICE_INPUTS = ("components/devices/tmp117/src/", "cmake/package/verification/device_tmp117/")


def sdk_changes(paths):
    """Private MCU implementation uses its own ARM gate. Unknown inputs widen."""
    classify(paths)
    selected = []
    for name in paths:
        if name.startswith(DEVICE_INPUTS):
            selected.append(name)
            continue
        if name in ("scripts/ci/sdk_consumers.py", ".github/workflows/enterprise-tools.yml"):
            selected.append(name)
            continue
        path = Path(name)
        if (name.startswith(("docs/", "scripts/ci/", ".github/", "tools/testing/")) or
                path.suffix.lower() in {".md", ".rst"} or
                name.startswith(("tools/configure/tests/", "tests/support/native/tests/")) or
                (name.startswith(("soc/", "boards/", "arch/", "core/", "os/", "io/"))
                 and path.suffix.lower() in {".c", ".s", ".h", ".inc"} and
                 "include" not in path.parts)):
            continue
        selected.append(name)
    return sorted(set(selected))


def full_components(root):
    """Select only supplier profiles used by maintained SDK consumers."""
    supplied = {profile for item in ALL_SOURCE_DEPENDENCIES
                for profile in item.component_profiles}
    selected = set(EXPLICIT_COMPONENTS)
    for specification in SERVICE_CONSUMERS:
        assembly = (service_directory(root, specification) /
                    specification.assembly)
        authored = tomllib.loads(assembly.read_text())
        selected.update(supplied.intersection(authored.get("components", ())))
    return tuple(sorted(selected))


def verify_checkout(root, head, changes, output, *, allow_network=False, run=subprocess.run):
    """Use a fresh publisher output and bind both consumer domains to its revision."""
    minimal = changes and all(name.startswith(DEVICE_INPUTS) for name in changes)
    profile = ["--backend", "baremetal" if minimal else "freertos", "--host-support"]
    if not minimal:
        for component in full_components(root):
            profile.extend(("--component", component))
    with tempfile.TemporaryDirectory(prefix="nexus-sdk-check-") as temporary:
        directory = Path(temporary)
        exported, relocated = directory / "export", directory / "relocated sdk"
        python = sys.executable
        run([python, "-B", "tools/dev/dependencies.py", "prepare", "--scope", "sdk",
             *profile, "--report", str(directory / "dependencies.json"),
             *(["--allow-network"] if allow_network else [])], cwd=root, check=True)
        run([python, "-B", "cmake/package/package_source_sdk.py", "--source", str(root),
             "--output", str(exported), *profile], cwd=root, check=True)
        exported.rename(relocated)
        identity = json.loads((relocated / "share/nexus/src/.nexus-source-sdk.json").read_bytes())
        if (identity.get("source_revision") != head or identity.get("source_dirty") is not False
                or identity.get("publishable") is not True):
            raise ValueError("Relocated SDK revision differs from the committed change")
        verify = [python, "-B", "cmake/package/verify_consumers.py",
                  "--prefix", str(relocated), "--output", str(output)]
        # For the initial commit or an enormous change use the complete plan.
        if changes is not None and len(changes) <= 128:
            for path in changes:
                verify.extend(("--changed-path", path))
        run(verify, cwd=root, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=os.environ.get("NEXUS_CHANGE_BASE"))
    parser.add_argument("--head", default=os.environ.get("NEXUS_CHANGE_HEAD") or "HEAD")
    parser.add_argument("--all", action="store_true", default=os.environ.get("NEXUS_SDK_ALL") == "true")
    parser.add_argument("--plan", action="store_true", help="print selection without preparing an SDK")
    parser.add_argument("--output", type=Path, default=Path("build/quality/sdk-consumers"))
    parser.add_argument("--allow-network", action="store_true",
                        help="allow dependency preparation to initialize selected submodules")
    args = parser.parse_args(argv)
    try:
        def commit(revision):
            return subprocess.check_output(["git", "rev-parse", "--verify", "--end-of-options",
                                            revision + "^{commit}"], cwd=ROOT, text=True).strip()
        head = commit(args.head)
        if head != commit("HEAD"):
            raise ValueError("SDK verification requires the selected commit as checkout HEAD")
        changes = (None if args.all else sdk_changes(changed_paths(ROOT, args.base, head)))
        selected = changes is None or bool(changes)
        print(json.dumps({"source_revision": head, "base": args.base, "selected": selected,
                          "changed_paths": changes}, sort_keys=True), flush=True)
        if args.plan:
            if output := os.environ.get("GITHUB_OUTPUT"):
                with open(output, "a") as stream:
                    stream.write("selected=" + str(selected).lower() + "\n")
            return 0
        if selected:
            verify_checkout(ROOT, head, changes, args.output.resolve(),
                            allow_network=args.allow_network)
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print("SDK change verification failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
