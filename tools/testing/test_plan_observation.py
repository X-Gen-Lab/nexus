"""Previously observed Google cases never authorize a new execution or HIL."""

import hashlib
import json
from pathlib import Path
import unittest

from scripts.ci import io_lifecycle_matrix as matrix
from tools.testing import test_io_lifecycle_matrix as fixtures


class CompleteGoogleIdentityTests(unittest.TestCase):
    def fixture(self):
        model = fixtures.IOLifecycleMatrixTests()
        model.setUp()
        self.addCleanup(model.doCleanups)
        source = model.root / "tests/google/uncatalogued_test.cpp"
        source.write_text("TEST(Uncatalogued, Actual) { EXPECT_TRUE(true); }\n")
        obj = (model.build / "CMakeFiles/nexus_uncatalogued_test.dir"
               / "uncatalogued_test.cpp.o")
        obj.parent.mkdir(parents=True)
        obj.write_bytes(b"second actual object fixture")
        binary = model.build / "bin/nexus_uncatalogued_test"
        binary.write_bytes(b"second actual binary fixture")
        database = model.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entries.append({"directory": str(model.build), "file": str(source),
                        "arguments": ["cc", "-c", str(source), "-o", str(obj)]})
        database.write_text(json.dumps(entries))
        manifest = model.build / "google-contracts.json"
        document = json.loads(manifest.read_text())
        document["executables"].append(str(binary))
        manifest.write_text(json.dumps(document))
        matrix.capture(model.root, model.build, model.catalogue, model.snapshot)
        model.write_execution()
        xml = model.execution / "google-1.xml"
        xml.write_text(
            '<testsuites name="AllTests" tests="1" failures="0" errors="0"'
            ' skipped="0"><testsuite name="Uncatalogued" tests="1">'
            '<testcase classname="Uncatalogued" name="Actual" status="run"'
            ' result="completed"/></testsuite></testsuites>')
        (model.execution / "discovery-1.txt").write_text(
            "Uncatalogued.\n  Actual\n")
        summary = model.execution / "execution.json"
        record = json.loads(summary.read_text())
        record["suites"].append({
            "binary": binary.name, "passed": 1,
            "report_sha256": hashlib.sha256(xml.read_bytes()).hexdigest(),
            "cases": [{"classname": "Uncatalogued", "name": "Actual",
                       "status": "passed"}]})
        summary.write_text(json.dumps(record))
        for kind, arguments in (
            ("discovery", [str(binary), "--gtest_filter=*",
                           "--gtest_list_tests"]),
            ("execution", [str(binary), "--gtest_filter=*", "--gtest_repeat=1",
                           "--gtest_also_run_disabled_tests",
                           "--gtest_output=xml:" + str(xml)])):
            (model.execution / (kind + "-1.command.json")).write_text(json.dumps(
                {"command": arguments, "cwd": str(model.root), "exit_code": 0}))
        return model, source, obj, binary

    def test_uncatalogued_source_object_and_binary_cannot_reuse_observation(self):
        for field in ("source", "object", "binary"):
            with self.subTest(field=field):
                model, source, obj, binary = self.fixture()
                path = {"source": source, "object": obj, "binary": binary}[field]
                path.write_bytes(path.read_bytes() + b"changed input\n")
                with self.assertRaisesRegex(ValueError, "changed"):
                    matrix.audit(model.root, model.build, model.catalogue,
                                 model.snapshot, model.execution)


def metadata_fixture(test):
    """Actual subprocess receipts over explicitly synthetic compiler metadata."""
    import os
    import sys
    from tools.dev import process

    model, extra_source, extra_object, extra_binary = (
        CompleteGoogleIdentityTests.fixture(test))
    header = model.root / "include/shared.h"
    header.parent.mkdir()
    header.write_text("#define SHARED_VALUE 1\n")
    archive = model.build / "libshared.a"
    archive.write_bytes(b"shared archive identity fixture")
    entries = json.loads((model.build / "compile_commands.json").read_text())
    objects = [Path(row["arguments"][-1]) for row in entries]
    sources = [Path(row["file"]) for row in entries]
    node_paths = [*objects, *sources, model.binary, extra_binary, archive]
    dot = "digraph ninja {\n" + "".join(
        f'"node{index}" [label={json.dumps(str(path))}]\n'
        for index, path in enumerate(node_paths)) + "}\n"
    dependencies = "".join(
        f"{obj}: #deps 2, deps mtime {obj.stat().st_mtime_ns} (VALID)\n"
        f"    {source}\n    {header}\n\n"
        for obj, source in zip(objects, sources, strict=True))
    ninja = model.root / "metadata_ninja.py"
    ninja.write_text(
        f"#!{sys.executable}\nimport sys\n"
        "if sys.argv[-3:] == ['-t', 'graph', 'nexus_google_contracts']:\n"
        f"    print({dot!r}, end='')\n"
        "elif sys.argv[-2:] == ['-t', 'deps']:\n"
        f"    print({dependencies!r}, end='')\n"
        "else: raise SystemExit(2)\n")
    ninja.chmod(0o755)
    (model.build / "CMakeCache.txt").write_text(
        "CMAKE_GENERATOR:INTERNAL=Ninja\n"
        f"CMAKE_MAKE_PROGRAM:FILEPATH={ninja}\n")
    (model.build / "build.ninja").write_text("# Explicit graph fixture\n")
    (model.build / "CMakeFiles/rules.ninja").write_text("# Link rules fixture\n")
    (model.build / ".ninja_deps").write_bytes(b"dependency database fixture")

    def execute(argv, environment, log):
        result = process.run(argv, cwd=model.root, env=environment, log=log,
                             timeout=30, stage=log.stem)
        if result.returncode:
            raise ValueError("metadata subprocess failed")
        return result.stdout

    model.metadata_execute = execute
    model.metadata_environment = dict(os.environ)
    model.metadata_environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return model, header, archive


class CompleteCompilerMetadataTests(unittest.TestCase):
    def test_shared_header_archive_and_failed_receipt_reject_reuse(self):
        for mutation in ("header", "archive", "receipt", "rules"):
            with self.subTest(mutation=mutation):
                model, header, archive = metadata_fixture(self)
                matrix.capture_complete(model.root, model.build,
                                        model.catalogue, model.snapshot,
                                        model.metadata_execute,
                                        model.metadata_environment)
                snapshot = json.loads(model.snapshot.read_text())
                self.assertEqual(snapshot["compiler_inputs"]["scope"],
                                 "current_ninja_materialized_context_superset")
                files = snapshot["compiler_inputs"]["files"]
                self.assertIn(str(header), files)
                self.assertIn(str(archive), files)
                if mutation == "receipt":
                    path = model.execution / "input-graph.command.json"
                    receipt = json.loads(path.read_text())
                    receipt["exit_code"] = 1
                    path.write_text(json.dumps(receipt))
                else:
                    path = {"header": header, "archive": archive,
                            "rules": model.build / "CMakeFiles/rules.ninja"}[mutation]
                    path.write_bytes(path.read_bytes() + b"changed\n")
                with self.assertRaises(ValueError):
                    matrix.verify_snapshot(model.root, model.build,
                                           model.catalogue, model.snapshot)

    def test_metadata_changed_after_receipt_check_is_rejected(self):
        from unittest.mock import patch
        from tools.dev import qualification

        model, _header, _archive = metadata_fixture(self)
        checked = qualification.completed_command_inputs
        changed = []

        def during_read(log, **keywords):
            selected = checked(log, **keywords)
            if log.stem == "input-graph" and not changed:
                changed.append(None)
                log.write_text(log.read_text().replace(
                    "digraph ninja {\n", "digraph ninja {\n// changed\n"))
            return selected

        with patch.object(qualification, "completed_command_inputs",
                          side_effect=during_read):
            with self.assertRaises(ValueError):
                matrix.capture_complete(model.root, model.build,
                                        model.catalogue, model.snapshot,
                                        model.metadata_execute,
                                        model.metadata_environment)

    def test_cached_object_or_archive_alias_cannot_count_as_materialized_input(self):
        for kind in ("object", "archive"):
            with self.subTest(kind=kind):
                model, _header, archive = metadata_fixture(self)
                entries = json.loads((model.build / "compile_commands.json").read_text())
                selected = (Path(entries[-1]["arguments"][-1])
                            if kind == "object" else archive)
                cached = model.root / ("cached-" + selected.name)
                selected.rename(cached)
                selected.symlink_to(cached)
                with self.assertRaises(ValueError):
                    matrix.capture_complete(model.root, model.build,
                                            model.catalogue, model.snapshot,
                                            model.metadata_execute,
                                            model.metadata_environment)


def discovery_fixture(test):
    """Retain real case-list receipts, without any case execution report."""
    import sys

    model, header, archive = metadata_fixture(test)
    binaries = [model.binary, model.build / "bin/nexus_uncatalogued_test"]
    listings = ["Provider.\n  Retains\n",
                "Instances/Unknown.\n  Value/0  # GetParam() = 1\n"
                "  DISABLED_Value/1  # GetParam() = 2\n"]
    for binary, listing in zip(binaries, listings, strict=True):
        binary.write_text(
            f"#!{sys.executable}\nimport pathlib,sys\n"
            "if sys.argv[1:] != ['--gtest_filter=*','--gtest_list_tests']:\n"
            "    pathlib.Path(__file__+'.executed').write_text('unexpected')\n"
            "    raise SystemExit(7)\n"
            f"print({listing!r},end='')\n")
        binary.chmod(0o755)
    model.snapshot = model.execution / "io-property-inputs.json"
    matrix.capture_complete(model.root, model.build, model.catalogue,
                            model.snapshot, model.metadata_execute,
                            model.metadata_environment)
    for index, binary in enumerate(binaries):
        model.metadata_execute(
            [str(binary), "--gtest_filter=*", "--gtest_list_tests"],
            model.metadata_environment,
            model.execution / ("discovery-" + str(index) + ".txt"))
    (model.execution / "execution.json").unlink()
    plan = model.root / "plan.json"
    plan.write_text(json.dumps({"schema_version": 1, "stages": [
        {"name": "native", "checker": "native_contracts",
         "args": ["--preset", "native-debug"]}]}))
    presets = model.root / "CMakePresets.json"
    presets.write_text(json.dumps({"version": 6,
        "configurePresets": [{"name": "native-debug", "generator": "Ninja",
                              "binaryDir": "${sourceDir}/build"}],
        "testPresets": [{"name": "native-debug",
                         "configurePreset": "native-debug"}]}))
    checker = model.root / "scripts/ci/native_contracts.py"
    checker.parent.mkdir(parents=True)
    checker.write_text("# Explicit checker metadata fixture\n")
    model.preview_plan = plan
    model.catalogue.rename(model.root / "tools-catalogue.json")
    catalogue = model.root / "tools/testing/io_lifecycle_matrix.json"
    catalogue.parent.mkdir(parents=True)
    (model.root / "tools-catalogue.json").rename(catalogue)
    model.catalogue = catalogue
    return model, header, archive


class ObservedPlanPreviewTests(unittest.TestCase):
    def test_explicit_discovery_is_readonly_and_preserves_unknown_case_scope(self):
        from unittest.mock import patch
        from tools.dev import plan

        model, _header, _archive = discovery_fixture(self)
        with patch("subprocess.run", side_effect=AssertionError("spawned")), \
                patch("subprocess.Popen", side_effect=AssertionError("spawned")):
            result = plan.preview(model.root, model.preview_plan,
                                  model.root / "future", environment={},
                                  observations={"native-debug": model.snapshot})
        stage = result["stages"][0]
        self.assertEqual(stage["status"], "not_executed")
        self.assertFalse(result["qualification"])
        self.assertEqual(stage["coverage"]["case_count"], 3)
        cases = stage["coverage"]["observed_cases"]
        self.assertEqual([(case["suite"], case["name"]) for case in cases], [
            ("Provider", "Retains"),
            ("Instances/Unknown", "DISABLED_Value/1"),
            ("Instances/Unknown", "Value/0")])
        self.assertEqual(cases[1]["catalogue_status"], "unknown")
        self.assertEqual(cases[0]["properties"][0]["invariant"],
                         "unproved drain retains loan")
        self.assertEqual(stage["hardware"]["assessment"], "not_required")
        self.assertFalse((model.root / "future").exists())
        self.assertFalse((model.execution / "execution.json").exists())

    def test_stale_source_failed_discovery_or_partial_snapshot_rejects(self):
        from tools.dev import plan

        for mutation in ("source", "receipt", "partial"):
            with self.subTest(mutation=mutation):
                model, _header, _archive = discovery_fixture(self)
                if mutation == "source":
                    model.source.write_text(model.source.read_text() + "changed\n")
                elif mutation == "receipt":
                    path = model.execution / "discovery-1.command.json"
                    record = json.loads(path.read_text())
                    record["exit_code"] = 1
                    path.write_text(json.dumps(record))
                else:
                    record = json.loads(model.snapshot.read_text())
                    record.pop("compiler_inputs")
                    model.snapshot.write_text(json.dumps(record))
                with self.assertRaises(ValueError):
                    plan.preview(model.root, model.preview_plan,
                                 model.root / "future", environment={},
                                 observations={"native-debug": model.snapshot})

    def test_filtered_stale_or_replaced_raw_discovery_is_not_admitted(self):
        from tools.dev import plan

        for mutation in ("filtered", "stale", "raw", "duplicate"):
            with self.subTest(mutation=mutation):
                model, _header, _archive = discovery_fixture(self)
                view = model.execution / "discovery-1.command.json"
                record = json.loads(view.read_text())
                original = Path(record["attempt_receipt"])
                if mutation == "raw":
                    Path(record["stdout"]["path"]).write_text("Replaced.\n  Case\n")
                elif mutation == "duplicate":
                    # Successful raw duplicate listing is still not a case set.
                    binary = model.build / "bin/nexus_uncatalogued_test"
                    text = binary.read_text()
                    start = text.index("print(")
                    binary.write_text(text[:start] +
                                      "print('Duplicate.\\n  Same\\n  Same')\n")
                    matrix.capture_complete(model.root, model.build,
                                            model.catalogue, model.snapshot,
                                            model.metadata_execute,
                                            model.metadata_environment)
                    for index, selected in enumerate((model.binary, binary)):
                        model.metadata_execute(
                            [str(selected), "--gtest_filter=*",
                             "--gtest_list_tests"], model.metadata_environment,
                            model.execution / ("discovery-" + str(index) + ".txt"))
                else:
                    if mutation == "filtered":
                        record["command"][1] = "--gtest_filter=Selected*"
                    else:
                        record["started_ns"] = 1
                    view.write_text(json.dumps(record))
                    original.write_text(json.dumps({key: value for key, value in
                        record.items() if key != "attempt_receipt"}))
                with self.assertRaises(ValueError):
                    plan.preview(model.root, model.preview_plan,
                                 model.root / "future", environment={},
                                 observations={"native-debug": model.snapshot})

    def test_transient_snapshot_cannot_replace_retained_freshness_boundary(self):
        from unittest.mock import patch
        from tools.dev import plan

        model, _header, _archive = discovery_fixture(self)
        # Capture after actual discovery: those receipts are now stale.
        matrix.capture_complete(model.root, model.build, model.catalogue,
                                model.snapshot, model.metadata_execute,
                                model.metadata_environment)
        original = model.snapshot.read_bytes()
        replacement = json.loads(original)
        replacement["not_before_ns"] = 1
        verify = matrix.verify_snapshot
        calls = []

        def during_verify(*arguments):
            calls.append(None)
            if len(calls) == 1:
                model.snapshot.write_text(json.dumps(replacement))
                try:
                    return verify(*arguments)
                finally:
                    model.snapshot.write_bytes(original)
            return verify(*arguments)

        with self.assertRaises(ValueError):
            plan.preview(model.root, model.preview_plan,
                         model.root / "control-future", environment={},
                         observations={"native-debug": model.snapshot})
        with patch.object(matrix, "verify_snapshot", side_effect=during_verify):
            with self.assertRaises(ValueError):
                plan.preview(model.root, model.preview_plan,
                             model.root / "future", environment={},
                             observations={"native-debug": model.snapshot})
        self.assertEqual(model.snapshot.read_bytes(), original)

    def test_transient_catalogue_cannot_supply_unretained_property_meaning(self):
        from unittest.mock import patch
        from tools.dev import plan

        model, _header, _archive = discovery_fixture(self)
        original = model.catalogue.read_bytes()
        replacement = json.loads(original)
        replacement["properties"][0]["invariant"] = "unretained meaning"
        validate = matrix.catalogue_inputs
        calls = []

        def during_validate(root, path):
            calls.append(None)
            if len(calls) == 2:
                path.write_text(json.dumps(replacement))
                try:
                    return validate(root, path)
                finally:
                    path.write_bytes(original)
            return validate(root, path)

        with patch.object(matrix, "catalogue_inputs", side_effect=during_validate):
            with self.assertRaises(ValueError):
                plan.preview(model.root, model.preview_plan,
                             model.root / "future", environment={},
                             observations={"native-debug": model.snapshot})
        self.assertEqual(model.catalogue.read_bytes(), original)

    def test_transient_receipt_cannot_replace_retained_discovery_time(self):
        from unittest.mock import patch
        from tools.dev import plan, qualification

        model, _header, _archive = discovery_fixture(self)
        matrix.capture_complete(model.root, model.build, model.catalogue,
                                model.snapshot, model.metadata_execute,
                                model.metadata_environment)
        views = {model.execution / ("discovery-" + str(index) + ".command.json")
                 for index in range(2)}
        originals = {view: view.read_bytes() for view in views}
        read = Path.read_bytes
        completed = qualification.completed_command_inputs
        ready, changed = set(), set()

        def after_validation(log, **keywords):
            selected = completed(log, **keywords)
            ready.add(log.with_suffix(".command.json"))
            return selected

        def during_read(path):
            if path in views and path in ready and path not in changed:
                changed.add(path)
                replacement = json.loads(originals[path])
                replacement["started_ns"] = json.loads(
                    model.snapshot.read_text())["not_before_ns"] + 1
                path.write_text(json.dumps(replacement))
                try:
                    return read(path)
                finally:
                    path.write_bytes(originals[path])
            return read(path)

        with patch.object(qualification, "completed_command_inputs",
                          side_effect=after_validation), \
                patch.object(Path, "read_bytes", new=during_read):
            with self.assertRaises(ValueError):
                plan.preview(model.root, model.preview_plan,
                             model.root / "future", environment={},
                             observations={"native-debug": model.snapshot})
        for view, original in originals.items():
            self.assertEqual(view.read_bytes(), original)

    def test_change_during_reader_is_rejected_and_unmatched_input_is_explicit(self):
        from unittest.mock import patch
        from tools.dev import plan

        model, header, _archive = discovery_fixture(self)
        verify = matrix.verify_snapshot
        calls = []

        def during_read(*arguments):
            calls.append(None)
            if len(calls) == 2:
                header.write_text("#define SHARED_VALUE 9\n")
            return verify(*arguments)

        with patch.object(matrix, "verify_snapshot", side_effect=during_read):
            with self.assertRaises(ValueError):
                plan.preview(model.root, model.preview_plan,
                             model.root / "future", environment={},
                             observations={"native-debug": model.snapshot})
        with self.assertRaisesRegex(ValueError, "no matching"):
            plan.preview(model.root, model.preview_plan,
                         model.root / "future", environment={},
                         observations={"native-release": model.snapshot})

    def test_discovery_changed_after_receipt_check_cannot_become_new_baseline(self):
        from unittest.mock import patch
        from scripts.ci import native_contracts
        from tools.dev import plan

        model, _header, _archive = discovery_fixture(self)
        parse = native_contracts.discovered_cases
        changed = []

        def during_read(output):
            parsed = parse(output)
            if not changed:
                changed.append(None)
                log = model.execution / "discovery-0.txt"
                log.write_text(log.read_text() + "// changed after receipt\n")
            return parsed

        with patch.object(native_contracts, "discovered_cases",
                          side_effect=during_read):
            with self.assertRaises(ValueError):
                plan.preview(model.root, model.preview_plan,
                             model.root / "future", environment={},
                             observations={"native-debug": model.snapshot})


class ObserveOnlyProducerTests(unittest.TestCase):
    def test_case_list_only_preserves_prior_evidence_without_success_summary(self):
        from contextlib import redirect_stderr, redirect_stdout
        import io
        from unittest.mock import patch
        from scripts.ci import native_contracts
        from scripts.ci import test_native_contracts as native_fixtures

        model = native_fixtures.NativeExecutionTests()
        model.setUp()
        self.addCleanup(model.doCleanups)
        prior = model.summary.read_bytes()
        with patch.object(native_contracts, "ROOT", model.root), \
                patch.object(matrix, "capture_complete"), \
                patch.object(matrix, "verify_snapshot"), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            status = native_contracts.main(["--preset", "native-debug",
                                            "--observe-only"])
        self.assertEqual(status, 0)
        self.assertFalse(model.summary.exists())
        self.assertTrue(any(path.read_bytes() == prior for path in
                            model.output.glob("execution.before-observe-*.json")))
        for binary in model.binaries:
            self.assertFalse(binary.with_suffix(".count").exists())
        self.assertFalse((model.build / "ctest-results.xml").exists())

    def test_input_mutated_by_case_list_cannot_publish_reusable_observation(self):
        from contextlib import redirect_stderr, redirect_stdout
        import io
        import sys
        from unittest.mock import patch
        from scripts.ci import native_contracts
        from tools.dev import plan

        model, _header, _archive = discovery_fixture(self)
        model.binary.write_text(
            f"#!{sys.executable}\nfrom pathlib import Path\n"
            f"p=Path({str(model.source)!r})\n"
            "p.write_text(p.read_text()+'// changed during listing\\n')\n"
            "print('Provider.\\n  Retains')\n")
        output = io.StringIO()
        with patch.object(native_contracts, "ROOT", model.root), \
                redirect_stdout(output), redirect_stderr(output):
            status = native_contracts.main(["--preset", "native-debug",
                                            "--observe-only"])
        self.assertEqual(status, 1, output.getvalue())
        self.assertIn("changed", output.getvalue())
        self.assertFalse((model.execution / "execution.json").exists())
        with self.assertRaises(ValueError):
            plan.preview(model.root, model.preview_plan, model.root / "future",
                         environment={},
                         observations={"native-debug": model.snapshot})


class ActualGoogleObservationTests(unittest.TestCase):
    def test_real_ninja_google_discovery_and_default_runner(self):
        """Use actual Google libraries; shim fixtures do not prove this path."""
        from contextlib import redirect_stderr, redirect_stdout
        import io
        import os
        import shutil
        import tempfile
        from unittest.mock import patch
        from scripts.ci import native_contracts
        from tools.dev import plan, process

        directory = tempfile.TemporaryDirectory(prefix="nexus observed Google ")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        build = root / "build/native-debug"
        reviewed = root / "reviewed.cpp"
        reviewed.write_text(
            '#include <gtest/gtest.h>\n#include "shared.h"\n'
            'TEST(Reviewed, Value) { EXPECT_EQ(shared_value(), 7); '
            'EXPECT_GE(shared_value(), 0); }\n')
        (root / "unknown.cpp").write_text(
            '#include <gtest/gtest.h>\n'
            'class Params : public testing::TestWithParam<int> {};\n'
            'TEST_P(Params, Value) { EXPECT_GT(GetParam(), 0); }\n'
            'INSTANTIATE_TEST_SUITE_P(Inputs, Params, testing::Values(1,2));\n'
            'TEST(Ordinary, DISABLED_Future) { EXPECT_TRUE(true); }\n')
        (root / "shared.h").write_text('int shared_value();\n')
        (root / "shared.cpp").write_text(
            '#include "shared.h"\nint shared_value() { return 7; }\n')
        project = Path(__file__).resolve().parents[2]
        shutil.copytree(project / "ext/googletest", root / "ext/googletest",
                        ignore=shutil.ignore_patterns(".git"))
        (root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(observed CXX)\nenable_testing()\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'add_subdirectory(ext/googletest EXCLUDE_FROM_ALL)\n'
            'add_library(shared STATIC shared.cpp)\n'
            'add_custom_target(nexus_google_contracts)\n'
            'foreach(name IN ITEMS reviewed unknown)\n'
            'add_executable(nexus_${name}_test ${name}.cpp)\n'
            'target_link_libraries(nexus_${name}_test PRIVATE '
            'gtest_main shared)\n'
            'set_property(TARGET nexus_${name}_test PROPERTY '
            'NEXUS_GOOGLE_CONTRACT TRUE)\n'
            'add_dependencies(nexus_google_contracts nexus_${name}_test)\n'
            'add_test(NAME ${name} COMMAND nexus_${name}_test)\n'
            'endforeach()\n'
            f'include("{project / "tests/google/plan.cmake"}")\n')
        (root / "CMakePresets.json").write_text(json.dumps({
            "version": 6, "configurePresets": [{"name": "native-debug",
                "generator": "Ninja", "binaryDir": "${sourceDir}/build/native-debug"}],
            "testPresets": [{"name": "native-debug",
                              "configurePreset": "native-debug"}]}))
        catalogue = root / "tools/testing/io_lifecycle_matrix.json"
        catalogue.parent.mkdir(parents=True)
        catalogue.write_text(json.dumps({"schema": 1, "scope": "real fixture",
            "properties": [{"id": "real", "family": "native",
                "peripheral": "shared", "mode": "model",
                "target": "nexus_reviewed_test", "case": "Reviewed.Value",
                "test_source": "reviewed.cpp",
                "production_sources": ["shared.cpp"], "model_sources": [],
                "assertions": ["EXPECT_EQ(shared_value(), 7)",
                               "EXPECT_GE(shared_value(), 0)"],
                "calls": ["shared_value()"], "invariant": "returns seven",
                "limitations": "host fixture"}],
            "unproven": [{"scope": "hardware", "reason": "not assessed"}]}))
        checker = root / "scripts/ci/native_contracts.py"
        checker.parent.mkdir(parents=True)
        checker.write_text("# Owned runner metadata fixture\n")
        authored = root / "plan.json"
        authored.write_text(json.dumps({"schema_version": 1, "stages": [{
            "name": "observed", "checker": "native_contracts",
            "args": ["--preset", "native-debug"]}]}))
        env = dict(os.environ)
        for name, argv in (
            ("configure", [shutil.which("cmake"), "--preset", "native-debug"]),
            ("build", [shutil.which("cmake"), "--build", str(build),
                       "--target", "nexus_google_contracts", "--parallel", "2"])):
            result = process.run(argv, cwd=root, env=env,
                                 log=root / (name + ".log"), timeout=120,
                                 stage=name)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        output = io.StringIO()
        with patch.object(native_contracts, "ROOT", root), \
                redirect_stdout(output), redirect_stderr(output):
            self.assertEqual(native_contracts.main([
                "--preset", "native-debug", "--observe-only"]), 0,
                output.getvalue())
        snapshot = build / "tdd/io-property-inputs.json"
        with patch("subprocess.Popen", side_effect=AssertionError("spawned")), \
                patch("subprocess.run", side_effect=AssertionError("spawned")):
            result = plan.preview(root, authored, root / "future", environment={},
                                  observations={"native-debug": snapshot})
        self.assertEqual(result["observations"][0]["case_count"], 4)
        self.assertEqual(sum(row["catalogue_status"] == "reviewed" for row in
                             result["observations"][0]["cases"]), 1)
        self.assertFalse((build / "tdd/execution.json").exists())
        with patch.object(native_contracts, "ROOT", root), \
                redirect_stdout(output), redirect_stderr(output):
            self.assertEqual(native_contracts.main([
                "--preset", "native-debug"]), 0, output.getvalue())
        summary = json.loads((build / "tdd/execution.json").read_text())
        self.assertEqual(summary["passed"], 4)
        self.assertEqual(summary["ctest"]["passed"], 2)


if __name__ == "__main__":
    unittest.main()
