"""Container admission uses physical storage and preserves bounded concurrency."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools.evidence.common import EvidenceError
try:
    from tools.evidence import storage
except ImportError:
    storage = None


class StorageTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(storage, "physical container storage preflight missing")

    def test_vfs_physical_copy_can_reject_logical_size_that_would_fit(self):
        result = storage.plan(driver="vfs", image_size_bytes=500,
            image_root_bytes=1200, free_bytes=1300, reserve_bytes=100,
            build_bytes=50, metadata_floor_bytes=100)
        self.assertEqual(result["status"], "insufficient_space")
        self.assertEqual(result["container_bytes"], 1370)
        self.assertEqual(result["admitted_concurrency"], 0)

    def test_serial_admission_does_not_promise_two_vfs_copies(self):
        result = storage.plan(driver="vfs", image_size_bytes=500,
            image_root_bytes=1200, free_bytes=2000, reserve_bytes=100,
            build_bytes=50, metadata_floor_bytes=100, requested_concurrency=2)
        self.assertEqual(result["status"], "reduced_concurrency")
        self.assertEqual(result["maximum_concurrency"], 1)
        self.assertEqual(result["admitted_concurrency"], 1)

    def test_exact_boundary_and_optional_observed_peak_are_conservative(self):
        result = storage.plan(driver="vfs", image_size_bytes=500,
            image_root_bytes=1200, measured_container_bytes=2000,
            free_bytes=2350, reserve_bytes=100, build_bytes=50,
            metadata_floor_bytes=100)
        self.assertEqual(result["status"], "admitted")
        self.assertEqual(result["container_bytes"], 2250)
        self.assertEqual(result["admitted_concurrency"], 1)

    def test_unknown_driver_or_unobserved_vfs_root_is_not_admitted(self):
        for changes in ({"driver": "unknown"}, {"image_root_bytes": 0},
                        {"image_root_bytes": None}):
            with self.subTest(changes=changes), self.assertRaises(EvidenceError):
                storage.plan(**({"driver": "vfs", "image_size_bytes": 500,
                    "image_root_bytes": 1200, "free_bytes": 10000} | changes))

    def test_boolean_negative_and_unbounded_inputs_fail_closed(self):
        for field, value in (("free_bytes", True), ("build_bytes", -1),
                ("requested_concurrency", 0), ("requested_concurrency", 65),
                ("image_size_bytes", 2**64), ("reserve_bytes", 1.5)):
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                storage.plan(**({"driver": "vfs", "image_size_bytes": 500,
                    "image_root_bytes": 1200, "free_bytes": 10000} |
                    {field: value}))

    def test_requested_concurrency_product_cannot_overflow(self):
        with self.assertRaises(EvidenceError):
            storage.plan(driver="vfs", image_size_bytes=2**60,
                image_root_bytes=2**60, free_bytes=2**64 - 1,
                requested_concurrency=64)

    def test_overlay_read_only_layers_are_shared_and_budget_only_metadata_and_build(self):
        result = storage.plan(driver="overlay2", image_size_bytes=1000000,
            image_root_bytes=None, free_bytes=1000, reserve_bytes=100,
            build_bytes=200, metadata_floor_bytes=100, read_only=True,
            tmpfs_bytes=256, daemon_log_driver="none", requested_concurrency=2)
        self.assertEqual(result["status"], "admitted")
        self.assertEqual(result["copy_bytes"], 0)
        self.assertEqual(result["container_bytes"], 300)
        self.assertEqual(result["storage_method"], "shared_read_only_layers")

    def test_overlay_unknown_write_or_logging_bound_fails_closed(self):
        for changes in ({"read_only": False}, {"tmpfs_bytes": None},
                        {"daemon_log_driver": "json-file"}):
            with self.subTest(changes=changes), self.assertRaises(EvidenceError):
                storage.plan(**({"driver": "overlayfs", "image_size_bytes": 1000,
                    "image_root_bytes": None, "free_bytes": 10000,
                    "read_only": True, "tmpfs_bytes": 256,
                    "daemon_log_driver": "none"} | changes))

    def test_overlay_inspection_observes_real_driver_and_filesystem(self):
        with tempfile.TemporaryDirectory() as directory:
            replies = [{"Driver": "overlay2", "DockerRootDir": directory},
                [{"Id": "sha256:" + "d" * 64, "Size": 500,
                  "GraphDriver": {"Name": "overlay2"}}]]
            with mock.patch.object(storage, "run_adapter", side_effect=[
                    json.dumps(item).encode() for item in replies]):
                result = storage.inspect("image:tag", read_only=True,
                    tmpfs_bytes=256, daemon_log_driver="none")
        self.assertEqual(result["status"], "admitted")
        self.assertIsNone(result["image_root_bytes"])

    def test_allocated_tree_counts_hardlinks_once_and_never_follows_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "file").write_bytes(b"x" * 4096)
            (root / "link").hardlink_to(root / "file")
            (root / "outside").symlink_to("/does/not/exist")
            expected = sum(path.lstat().st_blocks * 512 for path in
                (root, root / "file", root / "outside"))
            self.assertEqual(storage.allocated_tree(root)["allocated_bytes"], expected)

    def test_scan_rejects_symlink_root_and_entry_overflow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "file").write_text("input")
            alias = root / "alias"
            alias.symlink_to(root)
            with self.assertRaises(EvidenceError):
                storage.allocated_tree(alias)
            with self.assertRaises(EvidenceError):
                storage.allocated_tree(root, maximum_entries=1)

    def test_inspect_resolves_actual_vfs_chain_and_free_filesystem(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            layers = ["sha256:" + "a" * 64, "sha256:" + "b" * 64]
            chain = hashlib.sha256((layers[0] + " " + layers[1]).encode()).hexdigest()
            cache = root / "image/vfs/layerdb/sha256" / chain / "cache-id"
            cache.parent.mkdir(parents=True)
            cache.write_text("c" * 64)
            image_root = root / "vfs/dir" / ("c" * 64)
            image_root.mkdir(parents=True)
            (image_root / "payload").write_bytes(b"x" * 8192)
            replies = [json.dumps({"Driver": "vfs", "DockerRootDir": str(root)}),
                json.dumps([{"Id": "sha256:" + "d" * 64, "Size": 500,
                    "GraphDriver": {"Name": "vfs", "Data": None},
                    "RootFS": {"Layers": layers}}])]
            with mock.patch.object(storage, "run_adapter",
                    side_effect=[item.encode() for item in replies]) as run:
                report = storage.inspect("sha256:" + "d" * 64,
                    reserve_bytes=0, build_bytes=0)
            self.assertEqual(report["status"], "admitted")
            self.assertEqual(report["image_root"]["path"], str(image_root))
            self.assertGreater(report["image_root_bytes"], 500)
            self.assertEqual(run.call_count, 2)
            self.assertIn("--host=unix:///var/run/docker.sock", run.call_args.args[0])

    def test_missing_host_layer_facts_never_fall_back_to_image_size(self):
        with tempfile.TemporaryDirectory() as directory:
            replies = [{"Driver": "vfs", "DockerRootDir": directory},
                [{"Id": "sha256:" + "d" * 64, "Size": 500,
                  "GraphDriver": {"Name": "vfs"},
                  "RootFS": {"Layers": ["sha256:" + "a" * 64]}}]]
            with mock.patch.object(storage, "run_adapter", side_effect=[
                    json.dumps(item).encode() for item in replies]), \
                    self.assertRaises(EvidenceError):
                storage.inspect("sha256:" + "d" * 64)

    def probe(self, directory, *, free=(5000, 3800, 5000), create=None,
              cleanup=None):
        replies = [{"Driver": "vfs", "DockerRootDir": directory},
            [{"Id": "sha256:" + "d" * 64, "Size": 500,
              "GraphDriver": {"Name": "vfs"}}]]
        executions = []
        def run(argv, log, **kwargs):
            executions.append(argv)
            action = create if "create" in argv else cleanup
            if isinstance(action, BaseException):
                raise action
            return {"argv": argv, "exit_code": action or 0}
        with mock.patch.object(storage, "run_adapter", side_effect=[
                json.dumps(item).encode() for item in replies]), \
                mock.patch.object(storage, "free_space", side_effect=free):
            try:
                result = storage.probe("sha256:" + "d" * 64,
                    output=Path(directory) / "probe", runner=run,
                    reserve_bytes=100, build_bytes=50,
                    metadata_floor_bytes=100, race_tolerance_bytes=0)
                return result, executions
            finally:
                self.executions = executions

    def test_create_only_probe_measures_copy_and_always_removes_it(self):
        with tempfile.TemporaryDirectory() as directory:
            result, executions = self.probe(directory)
        self.assertEqual(result["image_root_bytes"], 1200)
        self.assertEqual(result["status"], "admitted")
        self.assertEqual(result["observation_scope"], "single_create_only_free_space_delta")
        self.assertEqual(len(executions), 2)
        self.assertIn("create", executions[0])
        self.assertNotIn("start", executions[0])
        self.assertIn("--network", executions[0])
        self.assertIn("rm", executions[1])

    def test_probe_insufficient_bootstrap_never_creates_container(self):
        with tempfile.TemporaryDirectory() as directory:
            result, executions = self.probe(directory, free=(1000,))
        self.assertEqual(result["status"], "insufficient_bootstrap_space")
        self.assertEqual(executions, [])

    def test_invalid_probe_scheduling_is_rejected_before_any_daemon_operation(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(storage, "run_adapter") as query, \
                self.assertRaises(EvidenceError):
            storage.probe("image:tag", output=Path(directory) / "probe",
                          runner=lambda *args: None, requested_concurrency=0)
        query.assert_not_called()

    def test_cancelled_create_cleans_named_daemon_workload_and_retains_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(KeyboardInterrupt):
                self.probe(directory, free=(5000, 5000),
                           create=KeyboardInterrupt())
            report = json.loads((Path(directory) / "probe/preflight.json").read_text())
        self.assertEqual(report["status"], "cancelled")
        self.assertEqual(len(self.executions), 2)
        self.assertEqual(report["cleanup"]["exit_code"], 0)

    def test_probe_failed_cleanup_or_racing_free_space_is_never_admitted(self):
        for options in ({"cleanup": 1}, {"free": (5000, 5100, 5000)},
                        {"free": (5000, 3800, 4800)}):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                result, _ = self.probe(directory, **options)
                self.assertEqual(result["status"], "failed")

    def test_cancelled_probe_retains_cleanup_exception_and_original_cancellation(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(KeyboardInterrupt):
                self.probe(directory, free=(5000,), create=KeyboardInterrupt(),
                           cleanup=EvidenceError("cleanup unavailable"))
            report = json.loads((Path(directory) / "probe/preflight.json").read_text())
        self.assertEqual(report["status"], "cancelled")
        self.assertEqual(report["cleanup_failure"], "cleanup unavailable")


if __name__ == "__main__":
    unittest.main()
