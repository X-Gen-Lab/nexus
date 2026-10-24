"""Receipt costs retain failures without adding overlapping wall time."""

import hashlib
import importlib
import json
from pathlib import Path
import tempfile
import unittest


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


if __name__ == "__main__":
    unittest.main()
