"""Real commit hooks must not redirect child SDK checks to the parent index."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GitEnvironmentTests(unittest.TestCase):
    def git(self, root, *arguments):
        result = subprocess.run(["git", "-C", str(root), *arguments],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.strip()

    def initialize(self, root):
        root.mkdir()
        self.git(root, "init", "-q")
        self.git(root, "config", "user.name", "Nexus fixture")
        self.git(root, "config", "user.email", "fixture@example.invalid")

    def test_real_commit_hook_checks_submodule_and_preserves_index(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            sdk = work / "upstream-sdk"
            root = work / "main"
            self.initialize(sdk)
            (sdk / "source.h").write_text("int sdk_source;\n")
            self.git(sdk, "add", "source.h")
            self.git(sdk, "commit", "-qm", "fixture: sdk")
            expected = self.git(sdk, "rev-parse", "HEAD")
            self.initialize(root)
            self.git(root, "-c", "protocol.file.allow=always", "submodule",
                     "add", "-q", str(sdk), "vendor/sdk")
            self.git(root, "commit", "-qm", "fixture: parent")
            (root / "change.txt").write_text("staged change\n")
            self.git(root, "add", "change.txt")
            code = (
                "import json,sys; from pathlib import Path; "
                "from tools.configure.configure import dependency_identity; "
                "print(json.dumps(dependency_identity(Path(sys.argv[1]), "
                "'vendor/sdk', sys.argv[2])))"
            )
            hook = root / ".git" / "hooks" / "pre-commit"
            hook.write_text(
                f"#!{sys.executable}\n"
                "import os,subprocess,sys\n"
                "from pathlib import Path\n"
                f"sys.path.insert(0, {str(ROOT)!r})\n"
                "from scripts.ci import tdd_gate\n"
                "tdd_gate.ROOT = Path.cwd()\n"
                "before = tdd_gate.staged_tree(Path.cwd())\n"
                "print('real hook inherited GIT_INDEX_FILE: ' + "
                "str('GIT_INDEX_FILE' in os.environ), flush=True)\n"
                f"command = [sys.executable, '-c', {code!r}, "
                f"str(Path.cwd()), {expected!r}]\n"
                "child = tdd_gate.environment()\n"
                f"child['PYTHONPATH'] = {str(ROOT)!r}\n"
                "result = subprocess.run(command, env=child, "
                "capture_output=True, text=True)\n"
                "print(result.stdout + result.stderr, end='')\n"
                "if result.returncode: sys.exit(result.returncode)\n"
                "assert 'GIT_INDEX_FILE' in os.environ\n"
                "assert tdd_gate.staged_tree(Path.cwd()) == before\n"
                "assert 'GIT_INDEX_FILE' not in child\n"
            )
            hook.chmod(0o755)
            result = subprocess.run(["git", "-C", str(root), "commit", "-qm",
                                     "fixture: actual hook"],
                                    capture_output=True, text=True)
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, output)
            self.assertIn("real hook inherited GIT_INDEX_FILE: True", output)
            self.assertIn(expected, output)
            self.assertEqual(self.git(root / "vendor/sdk", "status",
                                      "--porcelain"), "")


if __name__ == "__main__":
    unittest.main()
