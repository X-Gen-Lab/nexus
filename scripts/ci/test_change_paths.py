"""Exercise CI path policy and required jobs using the workflow's actual rules.

This stdlib-only test evaluator supports the positive literal, * and ** globs
currently used in ci.yml. It is not a replacement for, or an execution test of,
dorny/paths-filter. Complex/negative syntax fails instead of guessing semantics.
Deletion paths are classified just like additions, as in the action's unqualified
rules; tests check the gate decisions for docs-only and mixed changes as well.
"""

import ast
import re
import unittest
from pathlib import Path

from check_required_jobs import check_required_jobs, required_jobs


WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"


def read_filters(contents):
    """Read the existing inline filters block without a YAML runtime dependency."""
    lines = contents.splitlines()
    starts = [index for index, line in enumerate(lines) if line.strip() == "filters: |"]
    if len(starts) != 1:
        raise ValueError("one inline workflow filter block required")
    start = starts[0]
    indent = len(lines[start]) - len(lines[start].lstrip())
    filters = {}
    current = None
    for line in lines[start + 1:]:
        if not line.strip():
            continue
        if len(line) - len(line.lstrip()) <= indent:
            break
        stripped = line.strip()
        if re.fullmatch(r"[a-z]+:", stripped):
            current = stripped[:-1]
            if current in filters:
                raise ValueError("duplicate filter")
            filters[current] = []
        elif stripped.startswith("- ") and current is not None:
            pattern = ast.literal_eval(stripped[2:])
            compile_glob(pattern)  # Reject unsupported rules even without matches.
            filters[current].append(pattern)
        else:
            raise ValueError("unsupported filter structure")
    if set(filters) != {"code", "docs", "workflows"} or not all(filters.values()):
        raise ValueError("three nonempty CI filters required")
    return filters


def compile_glob(pattern):
    if (not isinstance(pattern, str) or not pattern
            or not re.fullmatch(r"[A-Za-z0-9_./*+-]+", pattern)
            or "***" in pattern or pattern.startswith("/")
            or any(part in ("", ".", "..") for part in pattern.split("/"))):
        raise ValueError("unsupported positive glob syntax")
    # The workflow's **.md matches Markdown at every depth. Other globstars
    # occupy complete path segments; embedded globstars are outside this subset.
    if pattern != "**.md" and any("**" in part and part != "**" for part in pattern.split("/")):
        raise ValueError("unsupported embedded globstar")
    expression = []
    index = 0
    while index < len(pattern):
        if pattern[index:index + 3] == "**/":
            expression.append(r"(?:[^/]+/)*")
            index += 3
        elif pattern[index:index + 2] == "**":
            # Literal directory/** also covers that changed gitlink itself.
            # **/Kconfig*/** requires a descendant of the wildcard directory.
            directory = pattern[:index - 1].rsplit("/", 1)[-1]
            if (index == len(pattern) - 2 and index > 0 and pattern[index - 1] == "/"
                    and "*" not in directory):
                expression.pop()
                expression.append(r"(?:/.*)?")
            else:
                expression.append(r".*")
            index += 2
        elif pattern[index] == "*":
            expression.append(r"[^/]*")
            index += 1
        else:
            expression.append(re.escape(pattern[index]))
            index += 1
    return re.compile("".join(expression))


def classify(filters, changes):
    # Paths alone are relevant: no action qualifier excludes deletions.
    return {name: str(any(compile_glob(pattern).fullmatch(path)
                         for _, path in changes for pattern in patterns)).lower()
            for name, patterns in filters.items()}


class ChangePathsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.filters = read_filters(WORKFLOW.read_text(encoding="utf-8"))

    def decisions(self, changes):
        return required_jobs(classify(self.filters, changes), "pull_request")

    def test_production_sources_and_runtime_boundaries_require_builds(self):
        paths = (
            "hal/src/nx_hal.c", "osal/adapters/baremetal/osal_baremetal.c",
            "framework/config/src/config.c", "services/storage/src/storage.c",
            "soc/stm32f407/controllers/spi/stm32_spi_sync.c",
            "runtime/src/nx_runtime.c", "runtime/src/nx_platform_info.c",
            "arch/cortex_m/interrupt.c", "soc/stm32f407/flash.c",
            "boards/stm32f4discovery/spi.c", "runtime/support-matrix.json",
            "runtime/contracts/manifest.json", "runtime/contracts/src/main.c",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(self.decisions([("modified", path)]),
                                 {"build-test": True, "code-quality": True, "docs": True})

    def test_build_configuration_and_dependency_identity_require_builds(self):
        paths = (
            "CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json",
            "runtime/contracts/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
            "soc/stm32f407/linker/stm32f407.ld",
            "Kconfig", "runtime/Kconfig", "platforms/native/Kconfig.platform",
            "configs/stm32f407_freertos_defconfig", ".config", ".config.product",
            "runtime/contracts/.config.board", "runtime/contracts/minimal_defconfig",
            "scripts/kconfig/generate_config.py", ".gitmodules", "nexus.lock",
            "dependencies/actions.lock.json", "deps/sdk.lock", "third_party/sdk/source.c",
            "requirements.txt", "runtime/contracts/requirements-build.txt",
            "runtime/contracts/pyproject.toml", "runtime/contracts/poetry.lock",
            "runtime/contracts/uv.lock", ".clang-tidy", ".clang-format",
            ".clang-format-dirs", ".editorconfig", ".pre-commit-config.yaml",
            ".kiro/steering/comment-standards.md",
            "docs/sphinx/development/coding_standards.rst",
            "CONTRIBUTING.md", "CONTRIBUTING_CN.md",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertTrue(self.decisions([("modified", path)])["build-test"])

    def test_sdk_and_rtos_gitlink_updates_require_builds(self):
        for path in ("vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4",
                     "vendors/st/stm32f4xx_hal_driver", "ext/freertos", "ext/googletest"):
            with self.subTest(path=path):
                self.assertTrue(self.decisions([("modified", path)])["build-test"])

    def test_deletions_and_mixed_changes_cannot_justify_build_skips(self):
        for path in ("runtime/src/nx_platform_info.c", "vendors/st/cmsis_device_f4",
                     "runtime/contracts/profile.json", "cmake/toolchains/arm-gcc.cmake"):
            for changes in ([("deleted", path)],
                            [("modified", "README.md"), ("deleted", path)]):
                with self.subTest(changes=changes):
                    outputs = classify(self.filters, changes)
                    decisions = required_jobs(outputs, "pull_request")
                    self.assertTrue(decisions["build-test"])
                    needs = {"changes": {"result": "success", "outputs": outputs}}
                    needs.update({job: {"result": "success"} for job in decisions})
                    self.assertTrue(check_required_jobs(needs, "pull_request").passed)
                    needs["build-test"]["result"] = "skipped"
                    self.assertFalse(check_required_jobs(needs, "pull_request").passed)

    def test_docs_only_changes_allow_build_skips_but_require_documentation(self):
        for changes in ([("modified", "README.md")],
                        [("deleted", "docs/guide/old.rst")],
                        [("modified", "docs/guide/new.rst"), ("modified", "CHANGELOG.md")]):
            with self.subTest(changes=changes):
                outputs = classify(self.filters, changes)
                self.assertEqual(required_jobs(outputs, "pull_request"),
                                 {"build-test": False, "code-quality": True, "docs": True})
                needs = {"changes": {"result": "success", "outputs": outputs},
                         "build-test": {"result": "skipped"}, "code-quality": {"result": "success"},
                         "docs": {"result": "success"}}
                self.assertTrue(check_required_jobs(needs, "pull_request").passed)
                needs["docs"]["result"] = "skipped"
                self.assertFalse(check_required_jobs(needs, "pull_request").passed)

    def test_workflow_and_action_changes_require_build_and_quality(self):
        for path in (".github/workflows/ci.yml", ".github/actions/setup-build/action.yml"):
            with self.subTest(path=path):
                decisions = self.decisions([("modified", path)])
                self.assertTrue(decisions["build-test"])
                self.assertTrue(decisions["code-quality"])

    def test_unsupported_filter_syntax_fails_instead_of_approximating(self):
        for pattern in ("!docs/**", "*.{c,h}", "src/[ab].c", "src/?.c", "@(src|inc)/**",
                        "src***", "src**file.c", "../src/**", "/src/**"):
            with self.subTest(pattern=pattern):
                with self.assertRaises(ValueError):
                    compile_glob(pattern)


if __name__ == "__main__":
    unittest.main()
