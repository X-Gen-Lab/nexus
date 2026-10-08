#!/usr/bin/env python3
"""Run the maintained CMake/CTest preset workflow without a second build model."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(command):
    print("+ " + " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=ROOT, check=True)


def settings_for(presets, name):
    preset = presets[name]
    parents = preset.get("inherits", [])
    if isinstance(parents, str):
        parents = [parents]
    values = {}
    for parent in reversed(parents):
        values.update(settings_for(presets, parent))
    values.update(preset.get("cacheVariables", {}))
    return values


def main():
    data = json.loads((ROOT / "CMakePresets.json").read_text())
    presets = {item["name"]: item for item in data["configurePresets"]}
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", required=True,
                        choices=[name for name, item in presets.items() if not item.get("hidden")])
    parser.add_argument("--stage", choices=["configure", "build", "test", "lint", "docs", "all"], default="all")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    settings = settings_for(presets, args.preset)
    host_tests = settings.get("NEXUS_BUILD_TESTS") == "ON"
    if args.stage == "test" and not host_tests:
        parser.error("this preset disables host tests; use a test-enabled Native preset or the corresponding HIL workflow")
    try:
        if args.stage in ("configure", "build", "all"):
            run(["cmake", "--preset", args.preset])
        if args.stage in ("build", "all"):
            run(["cmake", "--build", "--preset", args.preset, "--parallel", args.jobs])
        if args.stage == "test" or (args.stage == "all" and host_tests):
            report = ROOT / "build" / args.preset / "ctest-results.xml"
            run(["ctest", "--preset", args.preset, "--parallel", args.jobs,
                 "--output-on-failure", "--no-tests=error", "--output-junit", report])
        if args.stage == "all" and not host_tests:
            if settings.get("NEXUS_PLATFORM") == "native":
                print("Application build completed; this preset runs its finite Native example separately.")
            else:
                print("Embedded compilation completed. Hardware execution requires HIL evidence.")
        if args.stage == "lint":
            run([sys.executable, ROOT / "scripts/tools/format.py", "--check"])
        if args.stage == "docs":
            run([sys.executable, ROOT / "scripts/tools/docs.py", "-t", "doxygen"])
    except (subprocess.CalledProcessError, OSError) as error:
        print(f"CI command failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
