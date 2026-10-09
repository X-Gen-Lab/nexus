"""Exercise the actual coverage workflow at its shell/tool boundary.

The controlled tools model third-party capture failure and empty trace output;
these tests do not claim to run lcov or validate its internal filtering code.
"""
import fnmatch
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
# boards/soc have no instrumented translation units in the Native profile.
# Physical/ARM coverage needs its own execution evidence and source scope.
NATIVE_INSTRUMENTED = ("arch", "runtime", "hal", "osal", "framework", "services", "platforms")


def coverage_commands():
    lines = (ROOT / ".github/workflows/build-matrix.yml").read_text().splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == "- name: Generate Coverage")
    run = start + 1
    if lines[run].strip() != "run: |":
        raise AssertionError("Coverage step must expose an executable shell block")
    body = []
    for line in lines[run + 1:]:
        if line and len(line) - len(line.lstrip()) < 10:
            break
        body.append(line[10:])
    return "\n".join(body)


TOOL = r'''
import fnmatch
import json
import os
from pathlib import Path
import sys

root = Path.cwd()
tool = Path(sys.argv[0]).name
args = sys.argv[1:]
with (root / "tool-calls.jsonl").open("a") as log:
    log.write(json.dumps({"tool": tool, "args": args}) + "\n")
if tool == "genhtml":
    (root / "coverage_html").mkdir(exist_ok=True)
    (root / "coverage_html/index.html").write_text("fixture report")
    sys.exit(0)
if "--capture" not in args or any("ignore-errors" in arg for arg in args):
    sys.exit(31)
fixture = json.loads((root / "capture-fixture.json").read_text())
if fixture["mode"] == "error":
    print("controlled capture failure", file=sys.stderr)
    sys.exit(47)
def options(name):
    return [args[i + 1] for i, arg in enumerate(args[:-1]) if arg == name]
includes = options("--include")
excludes = options("--exclude")
for kind, patterns in (("include", includes), ("exclude", excludes)):
    for pattern in patterns:
        if not any(fnmatch.fnmatchcase(str(root / source), pattern)
                   for source in fixture["sources"]):
            print("controlled unused " + kind + " pattern: " + pattern, file=sys.stderr)
            sys.exit(24)
records = []
if fixture["mode"] != "empty":
    for relative in fixture["sources"]:
        source = str(root / relative)
        if includes and not any(fnmatch.fnmatchcase(source, pattern) for pattern in includes):
            continue
        if any(fnmatch.fnmatchcase(source, pattern) for pattern in excludes):
            continue
        if relative.startswith(("tests/", "ext/", "vendors/")):
            print("controlled third-party function endline mismatch", file=sys.stderr)
            sys.exit(23)
        records.append("SF:" + source + "\nDA:1,1\nend_of_record\n")
output = Path(options("--output-file")[0])
output.write_text("TN:nonempty fixture header\n" + "".join(records))
'''


@unittest.skipUnless(shutil.which("bash"), "Workflow boundary needs bash")
class CoverageScopeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nexus-coverage-contract-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        tools = self.root / "tools"
        tools.mkdir()
        for name in ("lcov", "genhtml"):
            path = tools / name
            path.write_text(f"#!{sys.executable}\n" + TOOL)
            path.chmod(0o755)
        self.env = os.environ.copy()
        self.env["PATH"] = str(tools) + os.pathsep + self.env.get("PATH", "")
        self.env["GITHUB_WORKSPACE"] = str(self.root)
        self.script = re.sub(r"\$\{\{\s*github\.workspace\s*\}\}",
                             str(self.root), coverage_commands())

    def execute(self, mode="normal"):
        sources = [f"{directory}/src/owned.c" for directory in NATIVE_INSTRUMENTED]
        sources += ["tests/contract.cpp", "ext/googletest/gtest.cpp"]
        (self.root / "capture-fixture.json").write_text(json.dumps({"mode": mode, "sources": sources}))
        return subprocess.run(["bash", "-e", "-o", "pipefail", "-c", self.script],
                              cwd=self.root, env=self.env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def calls(self):
        return [json.loads(line) for line in (self.root / "tool-calls.jsonl").read_text().splitlines()]

    def test_capture_keeps_owned_sources_before_third_party_mismatch(self):
        result = self.execute()
        self.assertEqual(result.returncode, 0, result.stderr)
        captured = [line[3:] for line in (self.root / "coverage.info").read_text().splitlines()
                    if line.startswith("SF:")]
        self.assertEqual(set(captured), {str(self.root / f"{directory}/src/owned.c")
                                        for directory in NATIVE_INSTRUMENTED})
        self.assertEqual([call["tool"] for call in self.calls()], ["lcov", "genhtml"])
        arguments = self.calls()[0]["args"]
        self.assertFalse(any("ignore-errors" in arg for arg in arguments))
        self.assertEqual(arguments[arguments.index("--directory") + 1], "build/linux-gcc-coverage")
        inclusions = [arguments[i + 1] for i, arg in enumerate(arguments[:-1]) if arg == "--include"]
        self.assertEqual(set(inclusions), {str(self.root / directory / "*")
                                          for directory in NATIVE_INSTRUMENTED})
        exclusions = [arguments[i + 1] for i, arg in enumerate(arguments[:-1]) if arg == "--exclude"]
        self.assertEqual(exclusions, [])
        for relative in ("tests/contract.cpp", "ext/googletest/gtest.cpp", "vendors/st/hal.c"):
            self.assertFalse(any(fnmatch.fnmatchcase(str(self.root / relative), pattern) for pattern in inclusions),
                             f"Capture includes third-party source {relative}")

    def test_unused_exclude_is_rejected_before_report(self):
        self.script = self.script.replace('--output-file coverage.info',
                                          f'--exclude "{self.root}/vendors/*" --output-file coverage.info')
        result = self.execute()
        self.assertEqual(result.returncode, 24, result.stderr)
        self.assertIn('unused exclude pattern', result.stderr)
        self.assertEqual([call["tool"] for call in self.calls()], ["lcov"])

    def test_nonempty_trace_without_source_records_stops_before_report(self):
        result = self.execute("empty")
        self.assertNotEqual(result.returncode, 0)
        self.assertGreater((self.root / "coverage.info").stat().st_size, 0)
        self.assertEqual([call["tool"] for call in self.calls()], ["lcov"])
        self.assertFalse((self.root / "coverage_html/index.html").exists())

    def test_capture_error_is_not_ignored_or_replaced_with_a_successful_report(self):
        result = self.execute("error")
        self.assertEqual(result.returncode, 47, result.stderr)
        self.assertEqual([call["tool"] for call in self.calls()], ["lcov"])
        self.assertFalse((self.root / "coverage_html/index.html").exists())


if __name__ == "__main__":
    unittest.main()
