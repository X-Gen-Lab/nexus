"""Receipt costs retain failures without adding overlapping wall time."""

import hashlib
import importlib
import json
import os
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


class DeveloperTimingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def receipt(self, name, start, finish, outcome="completed", exit_code=0,
                command=None):
        directory = self.root / (name + ".attempts") / "one"
        directory.mkdir(parents=True)
        raw = {}
        for channel in ("stdout", "stderr"):
            path = directory / (channel + ".txt")
            path.write_bytes(b"")
            raw[channel] = {"path": str(path), "bytes": 0,
                            "sha256": hashlib.sha256(b"").hexdigest()}
        data = {"schema": 1, "stage": name, "started_ns": start,
                "finished_ns": finish,
                "duration_seconds": (finish - start) / 1e9,
                "outcome": outcome, "exit_code": exit_code,
                "command": command or ["ctest"], **raw}
        path = directory / "receipt.json"
        path.write_text(json.dumps(data))
        return path

    def summary(self, paths=None):
        module = importlib.import_module("tools.dev.timings")
        return module.summarize(paths or [self.root])

    def test_parallel_attempts_have_union_wall_time_and_keep_failure_cost(self):
        self.receipt("test", 1_000_000_000, 4_000_000_000)
        self.receipt("compile", 2_000_000_000, 5_000_000_000,
                     "timed_out", 124, ["cmake", "--build", "build"])
        result = self.summary()
        self.assertEqual(result["observed_wall_seconds"], 4)
        self.assertEqual(result["cumulative_attempt_seconds"], 6)
        self.assertEqual(result["outcomes"], {"completed": 1, "timed_out": 1})
        self.assertEqual(result["phases"]["compile"]["failed"], 1)
        self.assertIsNone(result["cpu_seconds"])
        self.assertIsNone(result["peak_rss_bytes"])
        self.assertFalse(result["qualification"])

    def test_latest_view_and_explicit_path_do_not_count_attempt_twice(self):
        path = self.receipt("test", 0, 1_000_000_000)
        view = self.root / "latest.command.json"
        view.write_text(json.dumps({"attempt_receipt": str(path)}))
        result = self.summary([self.root, path, view])
        self.assertEqual(result["attempt_count"], 1)
        self.assertEqual(result["cumulative_attempt_seconds"], 1)

    def test_changed_raw_output_rejects_summary(self):
        path = self.receipt("test", 0, 1)
        path.with_name("stdout.txt").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "raw|identity"):
            self.summary()

    def test_parent_alias_is_one_attempt_without_hiding_symlink_components(self):
        path = self.receipt("test", 0, 1_000_000_000)
        alias = path.parent / ".." / path.parent.name / path.name
        result = self.summary([path, alias])
        self.assertEqual(result["attempt_count"], 1)
        self.assertEqual(result["cumulative_attempt_seconds"], 1)
        link = self.root / "alias"
        link.symlink_to(path.parent, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.summary([link / ".." / path.parent.name / path.name])

    def test_running_attempt_is_incomplete_without_invented_elapsed_cost(self):
        path = self.receipt("compile", 0, 1)
        data = json.loads(path.read_text())
        data.update(outcome="running", finished_ns=None,
                    duration_seconds=None, exit_code=None)
        path.write_text(json.dumps(data))
        result = self.summary()
        self.assertEqual(result["incomplete_attempts"], 1)
        self.assertEqual(result["observed_wall_seconds"], 0)
        self.assertIsNone(result["attempts"][0]["duration_seconds"])

    def test_inconsistent_duration_or_false_success_is_rejected(self):
        path = self.receipt("test", 0, 1_000_000_000)
        data = json.loads(path.read_text())
        for change in ({"duration_seconds": -1}, {"outcome": "running"}):
            path.write_text(json.dumps({**data, **change}))
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.summary()

    def test_empty_input_does_not_establish_a_baseline(self):
        with self.assertRaisesRegex(ValueError, "receipt"):
            self.summary()

    def test_resource_samples_preserve_scope_without_summing_nested_cpu(self):
        outer = self.receipt("outer", 0, 4_000_000_000)
        inner = self.receipt("inner", 1_000_000_000, 3_000_000_000)
        usage = {"method": "linux_wait4",
                 "scope": "waited_process_and_reaped_descendants",
                 "user_cpu_seconds": 0.5, "system_cpu_seconds": 0.25,
                 "maximum_process_rss_bytes": 4096}
        for path in (outer, inner):
            data = json.loads(path.read_text())
            path.write_text(json.dumps({**data, "resource_usage": usage}))
        result = self.summary()
        self.assertEqual(result["measured_resource_attempts"], 2)
        self.assertEqual(result["attempts"][0]["resource_usage"], usage)
        self.assertIsNone(result["cpu_seconds"])
        self.assertIsNone(result["peak_rss_bytes"])
        self.assertEqual(result["maximum_observed_process_rss_bytes"], 4096)

    def test_malformed_resource_sample_cannot_become_an_observation(self):
        path = self.receipt("test", 0, 1_000_000_000)
        original = json.loads(path.read_text())
        usage = {"method": "linux_wait4",
                 "scope": "waited_process_and_reaped_descendants",
                 "user_cpu_seconds": 0.5, "system_cpu_seconds": 0.25,
                 "maximum_process_rss_bytes": 4096}
        for change in ({"user_cpu_seconds": -1},
                       {"system_cpu_seconds": float("nan")},
                       {"maximum_process_rss_bytes": True},
                       {"scope": "whole_parallel_tree"}):
            path.write_text(json.dumps({**original,
                                        "resource_usage": {**usage, **change}}))
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.summary()


class ExecutionCostComparisonTests(unittest.TestCase):
    """Complete executed plans keep source, context and attempt boundaries."""

    def setUp(self):
        from tools.dev import qualification
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = {
            "root": str(self.root), "commit": "1" * 40, "tree": "2" * 40,
            "dirty": False, "submodules": [],
        }
        self.observation = patch.object(
            qualification, "observe_git_source", return_value=self.source)
        self.observation.start()
        self.addCleanup(self.observation.stop)
        script = self.root / qualification.CHECKERS["configure"][0]
        script.parent.mkdir(parents=True)
        script.write_text("print('actual checker output')\n")
        self.plan = {"schema_version": 1, "formal": True, "stages": [
            {"name": "first", "checker": "configure",
             "args": ["--output", "{output}/one"], "timeout_s": 10},
            {"name": "second", "checker": "configure",
             "args": ["--output", "{output}/two"], "timeout_s": 10},
        ]}
        self.plan_path = self.root / "plan.json"
        self.write_json(self.plan_path, self.plan)
        self.environment = self.oci_environment(self.root / "environment")

    def write_json(self, path, data):
        path.write_text(json.dumps(data, sort_keys=True) + "\n")

    def oci_environment(self, root, layer=b"fixture image layer"):
        """Small sealed-layout fixture exercises the real byte verifier."""
        blobs = root / "blobs/sha256"
        blobs.mkdir(parents=True)

        def descriptor(data, media):
            digest = hashlib.sha256(data).hexdigest()
            (blobs / digest).write_bytes(data)
            return {"digest": "sha256:" + digest, "size": len(data),
                    "mediaType": media}

        config = descriptor(b'{"os":"linux","architecture":"amd64"}',
                            "application/vnd.oci.image.config.v1+json")
        layers = [descriptor(layer, "application/vnd.oci.image.layer.v1.tar")]
        manifest = {"schemaVersion": 2, "config": config, "layers": layers}
        manifest_blob = descriptor(json.dumps(manifest).encode(),
            "application/vnd.oci.image.manifest.v1+json")
        path = root / "environment.json"
        self.write_json(path, {
            "schema_version": 1, "kind": "oci_tool_environment",
            "oci_manifest_sha256": manifest_blob["digest"][7:],
            "image_config_id": config["digest"],
            "rootfs_layers": [row["digest"] for row in layers],
            "platform": "linux/amd64", "observed_repo_digests": [],
            "preparation_network": "allowed_only_before_formal_build",
            "formal_build_network": "none", "dockerfile": None,
        })
        return path

    def executions(self, *, bound=True, warm_environment=None):
        from tools.dev import qualification
        reports = []
        for name in ("cold", "warm"):
            environment = (warm_environment if name == "warm" and
                           warm_environment is not None else self.environment)
            options = {"environment_report": environment} if bound else {}
            report = qualification.execute(
                self.plan, self.root, self.root / name, **options)
            self.assertEqual(report["status"], "executed_passed")
            reports.append(self.root / name / "report.json")
        return reports

    def compare(self, cold, warm, **options):
        from tools.dev import timings
        return timings.compare_executions(
            options.pop("plan", self.plan_path), cold, warm,
            root=self.root,
            cold_environment=options.pop("cold_environment", self.environment),
            warm_environment=options.pop("warm_environment", self.environment),
            cold_cache_state="empty build roots; fixture declaration",
            warm_cache_state="existing inputs; fixture declaration", **options)

    def test_complete_executions_compare_wall_union_without_qualification(self):
        cold, warm = self.executions()
        result = self.compare(cold, warm)
        self.assertFalse(result["qualification"])
        self.assertEqual(result["stage_count"], 2)
        self.assertEqual(result["cold"]["cost"]["attempt_count"], 2)
        self.assertEqual(result["warm"]["cost"]["attempt_count"], 2)
        self.assertEqual(result["wall_seconds_saved"],
                         result["cold"]["cost"]["observed_wall_seconds"] -
                         result["warm"]["cost"]["observed_wall_seconds"])
        self.assertIsNone(result["cache_hits"])
        self.assertIsNone(result["cpu_seconds_saved"])
        self.assertIn("declaration", result["cold"]["cache_state_declared"])

    def test_historical_unbound_execution_is_valid_but_not_comparable(self):
        from tools.dev import qualification
        cold, warm = self.executions(bound=False)
        self.assertEqual(qualification.verify_execution(
            self.plan, cold, self.root)["status"], "verified")
        with self.assertRaisesRegex(
                ValueError, "environment.*bound|bound.*environment"):
            self.compare(cold, warm)

    def test_same_report_and_hardlinked_report_are_not_independent(self):
        cold, warm = self.executions()
        with self.assertRaisesRegex(ValueError, "independent"):
            self.compare(cold, cold)
        alias = self.root / "aliased-report.json"
        os.link(cold, alias)
        with self.assertRaisesRegex(ValueError, "independent"):
            self.compare(cold, alias)

    def test_source_coverage_and_command_drift_reject_comparison(self):
        cold, warm = self.executions()
        original = json.loads(warm.read_text())
        mutations = [
            {**original, "source_after": {**self.source, "tree": "3" * 40}},
            {**original, "stages": original["stages"][:1]},
            {**original, "stages": [
                {**original["stages"][0], "argv": [sys.executable, "other"]},
                original["stages"][1]]},
        ]
        for data in mutations:
            self.write_json(warm, data)
            with self.subTest(data=data), self.assertRaises(ValueError):
                self.compare(cold, warm)
        self.write_json(warm, original)
        altered_plan = self.root / "other-plan.json"
        self.write_json(altered_plan, {**self.plan, "stages": [
            {**self.plan["stages"][0], "args": ["other"]},
            self.plan["stages"][1]]})
        with self.assertRaises(ValueError):
            self.compare(cold, warm, plan=altered_plan)

    def test_environment_drift_and_arbitrary_json_are_rejected(self):
        from tools.dev import qualification
        cold, warm = self.executions()
        other = self.oci_environment(self.root / "other-environment", b"other")
        with self.assertRaisesRegex(ValueError, "environment"):
            self.compare(cold, warm, warm_environment=other)
        data = json.loads(self.environment.read_text())
        self.write_json(self.environment, {
            **data, "observed_repo_digests": ["changed"]})
        with self.assertRaisesRegex(ValueError, "environment|identity"):
            self.compare(cold, warm)
        arbitrary = self.root / "arbitrary.json"
        self.write_json(arbitrary, {
            "schema_version": 1, "kind": "caller_claim"})
        with self.assertRaises(ValueError):
            qualification.execute(self.plan, self.root, self.root / "rejected",
                                  environment_report=arbitrary)
        self.assertFalse((self.root / "rejected").exists())

    def test_changed_actual_output_and_failed_attempt_are_rejected(self):
        cold, warm = self.executions()
        report = json.loads(warm.read_text())
        view = Path(report["stages"][0]["command_receipt"]["path"])
        receipt = json.loads(view.read_text())
        stream = Path(receipt["stdout"]["path"])
        contents = stream.read_bytes()
        stream.write_bytes(b"replacement output")
        with self.assertRaises(ValueError):
            self.compare(cold, warm)
        stream.write_bytes(contents)
        receipt["exit_code"] = 1
        self.write_json(view, receipt)
        with self.assertRaises(ValueError):
            self.compare(cold, warm)

    def test_environment_change_during_execution_remains_failed(self):
        from tools.dev import qualification
        script = self.root / qualification.CHECKERS["configure"][0]
        script.write_text(
            "from pathlib import Path\n"
            f"path = Path({str(self.environment)!r})\n"
            "path.write_text(path.read_text() + ' ')\n")
        report = qualification.execute(
            self.plan, self.root, self.root / "changed-environment",
            environment_report=self.environment)
        self.assertEqual(report["status"], "failed")
        self.assertIn("environment", report["failure"])
        self.assertTrue(all(row["exit_code"] == 0 for row in report["stages"]))

    def test_comparison_requires_explicit_cache_declarations(self):
        cold, warm = self.executions()
        from tools.dev import timings
        with self.assertRaisesRegex(ValueError, "cache"):
            timings.compare_executions(
                self.plan_path, cold, warm, root=self.root,
                cold_environment=self.environment,
                warm_environment=self.environment,
                cold_cache_state="", warm_cache_state="declared warm")


    def test_distinct_bound_environment_bytes_do_not_compare(self):
        other = self.oci_environment(
            self.root / "different-environment", b"other")
        cold, warm = self.executions(warm_environment=other)
        with self.assertRaisesRegex(ValueError, "identical environment"):
            self.compare(cold, warm, warm_environment=other)

    def test_missing_or_changed_oci_blob_rejects_binding(self):
        from tools.dev import qualification
        lock = json.loads(self.environment.read_text())
        layer = self.environment.parent / "blobs/sha256" / (
            lock["rootfs_layers"][0].removeprefix("sha256:"))
        layer.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "blob changed"):
            qualification.execute(
                self.plan, self.root, self.root / "changed-blob",
                environment_report=self.environment)
        self.assertFalse((self.root / "changed-blob").exists())

    def test_cli_compares_and_never_overwrites_an_existing_output(self):
        from contextlib import redirect_stdout
        import io
        from tools.dev import timings
        cold, warm = self.executions()
        destination = self.root / "comparison.json"
        argv = ["--compare", "--plan", str(self.plan_path),
                "--root", str(self.root), "--cold-report", str(cold),
                "--warm-report", str(warm),
                "--cold-environment", str(self.environment),
                "--warm-environment", str(self.environment),
                "--cold-cache-state", "declared cold",
                "--warm-cache-state", "declared warm",
                "--json", str(destination)]
        with redirect_stdout(io.StringIO()):
            self.assertEqual(timings.main(argv), 0)
        result = json.loads(destination.read_text())
        self.assertEqual(result["kind"],
                         "development_execution_cost_comparison")
        contents = destination.read_bytes()
        with redirect_stdout(io.StringIO()), self.assertRaises(FileExistsError):
            timings.main(argv)
        self.assertEqual(destination.read_bytes(), contents)

    def test_cli_rejects_partial_or_mixed_modes_before_reading_inputs(self):
        from contextlib import redirect_stderr
        import io
        from tools.dev import timings
        for argv in (["--compare"],
                     ["--plan", "missing.json", "missing-receipt"]):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    timings.main(argv)
                self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
