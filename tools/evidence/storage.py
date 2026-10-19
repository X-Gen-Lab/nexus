#!/usr/bin/env python3
"""Admit bounded Docker work from observed physical storage, without mutation.

VFS copies an entire unpacked image for every container. Docker's logical image
Size omits filesystem allocation and metadata costs, so it is not an admission
budget. The read-only observer reads the immutable final VFS layer. An explicit
create-only probe is available when those host facts cannot be read; it always
removes its named stopped container. Neither path starts workloads, prunes or
repairs daemon state.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys

from tools.evidence.common import (EvidenceError, atomic_json, run_adapter,
                                   sha256_value)
from tools.evidence.container import docker_prefix

MIB = 1024 * 1024


def unsigned(value, name, *, positive=False):
    """Reject booleans and implicit numeric conversions at the boundary."""
    if (type(value) is not int or not (1 if positive else 0) <= value < 2**64):
        raise EvidenceError("bounded unsigned storage value required: " + name)
    return value


def plan(*, driver, image_size_bytes, image_root_bytes, free_bytes,
         requested_concurrency=1, reserve_bytes=256 * MIB, build_bytes=64 * MIB,
         metadata_floor_bytes=64 * MIB, measured_container_bytes=None,
         read_only=False, tmpfs_bytes=None, daemon_log_driver=None):
    """Return conservative admission; callers must honour reduced concurrency.

    The ten-percent/floor metadata margin is an explicit scheduling reserve,
    not a proof of a workload's maximum disk usage. Bind-mount build output needs
    its own budget. Recheck before each new container and stop after a failed
    cleanup; previous admission cannot promise future filesystem capacity.
    """
    if driver not in {"vfs", "overlay2", "overlayfs"}:
        raise EvidenceError("unreviewed Docker storage driver")
    for name, value in (("image_size_bytes", image_size_bytes),
            ("free_bytes", free_bytes),
            ("reserve_bytes", reserve_bytes), ("build_bytes", build_bytes),
            ("metadata_floor_bytes", metadata_floor_bytes)):
        unsigned(value, name, positive=name.startswith("image_"))
    if type(requested_concurrency) is not int or not 1 <= requested_concurrency <= 64:
        raise EvidenceError("requested concurrency must be an integer in [1, 64]")
    if measured_container_bytes is not None:
        unsigned(measured_container_bytes, "measured_container_bytes", positive=True)
    if driver == "vfs":
        unsigned(image_root_bytes, "image_root_bytes", positive=True)
        copy_bytes = max(image_size_bytes, image_root_bytes,
                         measured_container_bytes or 0)
        method = "physical_full_image_copy"
    else:
        if read_only is not True or daemon_log_driver != "none":
            raise EvidenceError("overlay admission requires read-only layers and no daemon log files")
        unsigned(tmpfs_bytes, "tmpfs_bytes", positive=True)
        if tmpfs_bytes > 4 * 1024 * MIB or image_root_bytes is not None:
            raise EvidenceError("explicit bounded overlay tmpfs and shared image required")
        copy_bytes, method = 0, "shared_read_only_layers"
    metadata_bytes = max(metadata_floor_bytes, (copy_bytes + 9) // 10)
    container_bytes = copy_bytes + metadata_bytes + build_bytes
    required = reserve_bytes + requested_concurrency * container_bytes
    if not 0 < container_bytes < 2**64 or required >= 2**64:
        raise EvidenceError("storage budget must be nonzero and within unsigned bound")
    available = max(0, free_bytes - reserve_bytes)
    maximum = min(64, available // container_bytes)
    admitted = min(requested_concurrency, maximum)
    status = ("insufficient_space" if not admitted else
              "reduced_concurrency" if admitted < requested_concurrency else
              "admitted")
    return {"schema_version": 1, "kind": "container_storage_preflight",
        "status": status, "driver": driver, "storage_method": method,
        "image_size_bytes": image_size_bytes,
        "image_root_bytes": image_root_bytes, "free_bytes": free_bytes,
        "measured_container_bytes": measured_container_bytes,
        "copy_bytes": copy_bytes, "metadata_reserve_bytes": metadata_bytes,
        "build_reserve_bytes": build_bytes, "reserve_bytes": reserve_bytes,
        "container_bytes": container_bytes,
        "read_only": read_only, "tmpfs_bytes": tmpfs_bytes,
        "daemon_log_driver": daemon_log_driver,
        "requested_concurrency": requested_concurrency,
        "maximum_concurrency": maximum, "admitted_concurrency": admitted,
        "required_for_requested_bytes": required,
        "cleanup_policy": "stop named container on failure or cancellation; "
            "failed cleanup blocks further work",
        "limitations": ["Point-in-time free-space observation; not a disk quota.",
            "No daemon prune, storage-driver change or capacity guarantee."]}


def allocated_tree(root, *, maximum_entries=500000):
    """Count actual allocated blocks once per inode; never traverse symlinks."""
    root = Path(root)
    if root.is_symlink() or not root.is_dir():
        raise EvidenceError("physical image root must be a real directory")
    if type(maximum_entries) is not int or not 1 <= maximum_entries <= 1000000:
        raise EvidenceError("bounded physical image scan required")
    pending, seen, total, entries = [root], set(), 0, 0
    while pending:
        path = pending.pop()
        info = path.lstat()
        entries += 1
        if entries > maximum_entries:
            raise EvidenceError("physical image scan entry limit exceeded")
        identity = (info.st_dev, info.st_ino)
        if identity not in seen:
            seen.add(identity)
            total += info.st_blocks * 512
        if stat.S_ISDIR(info.st_mode):
            with os.scandir(path) as children:
                pending.extend(Path(child.path) for child in children)
    return {"path": str(root.absolute()), "allocated_bytes": total,
            "entries": entries, "unique_inodes": len(seen),
            "method": "lstat st_blocks*512; hardlinks deduplicated; symlinks not followed"}


def vfs_root(docker_root, image):
    """Resolve Docker's final immutable chain through its local VFS layer DB."""
    root = Path(docker_root)
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise EvidenceError("observed local Docker root unavailable")
    layers = image.get("RootFS", {}).get("Layers")
    if not isinstance(layers, list) or not 1 <= len(layers) <= 256:
        raise EvidenceError("bounded immutable image layer list required")
    chain = None
    for layer in layers:
        if not isinstance(layer, str) or not layer.startswith("sha256:"):
            raise EvidenceError("invalid immutable image layer")
        sha256_value(layer[7:])
        chain = (layer if chain is None else "sha256:" +
                 hashlib.sha256((chain + " " + layer).encode()).hexdigest())
    cache = root / "image/vfs/layerdb/sha256" / chain[7:] / "cache-id"
    if cache.is_symlink() or not cache.is_file() or cache.stat().st_size > 256:
        raise EvidenceError("immutable local VFS layer cache identity unavailable")
    cache_id = cache.read_text().strip()
    sha256_value(cache_id)
    return root / "vfs/dir" / cache_id


def inspect(image, *, docker="docker", requested_concurrency=1,
            reserve_bytes=256 * MIB, build_bytes=64 * MIB,
            measured_container_bytes=None, read_only=False, tmpfs_bytes=None,
            daemon_log_driver=None):
    """Observe the managed local daemon, immutable image and real free blocks."""
    if not isinstance(image, str) or not image or len(image) > 256:
        raise EvidenceError("bounded actual Docker image selector required")
    try:
        daemon = json.loads(run_adapter([*docker_prefix(docker), "info",
            "--format", "{{json .}}"], 30))
        images = json.loads(run_adapter([*docker_prefix(docker), "image",
            "inspect", image], 30))
        if not isinstance(images, list) or len(images) != 1:
            raise EvidenceError("one actual image storage identity required")
        observed = images[0]
        identity = observed["Id"]
        if not isinstance(identity, str) or not identity.startswith("sha256:"):
            raise EvidenceError("actual immutable image ID required")
        sha256_value(identity[7:])
        driver = daemon["Driver"]
        if driver != observed["GraphDriver"]["Name"]:
            raise EvidenceError("observed daemon and image storage drivers differ")
        if driver in {"overlay2", "overlayfs"}:
            root = Path(daemon["DockerRootDir"])
            if not root.is_absolute() or root.is_symlink() or not root.is_dir():
                raise EvidenceError("observed local Docker root unavailable")
            result = plan(driver=driver, image_size_bytes=observed["Size"],
                image_root_bytes=None, free_bytes=free_space(root),
                requested_concurrency=requested_concurrency,
                reserve_bytes=reserve_bytes, build_bytes=build_bytes,
                read_only=read_only, tmpfs_bytes=tmpfs_bytes,
                daemon_log_driver=daemon_log_driver)
            result.update(image_config_id=identity, docker_root=str(root))
            return result
        if driver != "vfs":
            raise EvidenceError("unreviewed Docker storage driver")
        root = vfs_root(daemon["DockerRootDir"], observed)
        physical = allocated_tree(root)
        filesystem = os.statvfs(root)
        result = plan(driver="vfs", image_size_bytes=observed["Size"],
            image_root_bytes=physical["allocated_bytes"],
            free_bytes=filesystem.f_bavail * filesystem.f_frsize,
            requested_concurrency=requested_concurrency,
            reserve_bytes=reserve_bytes, build_bytes=build_bytes,
            measured_container_bytes=measured_container_bytes)
        result.update(image_config_id=identity, image_root=physical,
                      docker_root=daemon["DockerRootDir"])
        return result
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise EvidenceError("actual physical Docker storage observation failed: " +
                            str(error)) from error


def free_space(root):
    """Observe blocks available to the current workload on the Docker filesystem."""
    filesystem = os.statvfs(root)
    return filesystem.f_bavail * filesystem.f_frsize


def probe(image, *, output, runner, docker="docker", requested_concurrency=1,
          reserve_bytes=256 * MIB, build_bytes=64 * MIB,
          metadata_floor_bytes=64 * MIB, race_tolerance_bytes=4 * MIB):
    """Measure one stopped VFS container copy with mandatory named cleanup.

    Use when the daemon's immutable image layer metadata is not readable. The
    image is created but never started; no host paths or credentials are mounted.
    A three-times-logical bootstrap reserve guards creation. The observed delta
    is one sample, not an exact universal image size or a future capacity promise.
    The caller serializes whole reproduction jobs and must check before each
    container. `runner` retains actual argv, raw streams and return codes.
    """
    unsigned(race_tolerance_bytes, "race_tolerance_bytes")
    if race_tolerance_bytes > 64 * MIB:
        raise EvidenceError("probe race tolerance exceeds 64 MiB")
    if not callable(runner):
        raise EvidenceError("retained command runner required")
    if (not isinstance(image, str) or not re.fullmatch(
            r"[A-Za-z0-9][A-Za-z0-9._/:@-]{0,255}", image)):
        raise EvidenceError("bounded Docker image selector required")
    # Reject malformed scheduling inputs before even querying daemon state.
    plan(driver="vfs", image_size_bytes=1, image_root_bytes=1, free_bytes=0,
         requested_concurrency=requested_concurrency,
         reserve_bytes=reserve_bytes, build_bytes=build_bytes,
         metadata_floor_bytes=metadata_floor_bytes)
    output = Path(output)
    if output.exists():
        raise EvidenceError("physical storage probe requires a new evidence root")
    output.mkdir(parents=True)
    report = {"schema_version": 1, "kind": "container_storage_probe",
        "status": "failed", "observation_scope":
            "single_create_only_free_space_delta", "commands": [],
        "physical_status": "not_executed", "container_started": False}
    name = "nexus-storage-" + os.urandom(12).hex()
    interrupted = None
    try:
        daemon = json.loads(run_adapter([*docker_prefix(docker), "info",
            "--format", "{{json .}}"], 30))
        images = json.loads(run_adapter([*docker_prefix(docker), "image",
            "inspect", image], 30))
        if (daemon["Driver"] != "vfs" or not isinstance(images, list) or
                len(images) != 1 or images[0]["GraphDriver"]["Name"] != "vfs"):
            raise EvidenceError("create-only physical probe requires one VFS image")
        observed = images[0]
        identity = observed["Id"]
        if not isinstance(identity, str) or not identity.startswith("sha256:"):
            raise EvidenceError("immutable image identity required")
        sha256_value(identity[7:])
        root = Path(daemon["DockerRootDir"])
        if not root.is_absolute() or root.is_symlink() or not root.is_dir():
            raise EvidenceError("observed local Docker root unavailable")
        size = unsigned(observed["Size"], "image_size_bytes", positive=True)
        # Validate every scheduling input before any daemon-owned allocation.
        bootstrap = plan(driver="vfs", image_size_bytes=size,
            image_root_bytes=size * 3, free_bytes=free_space(root),
            requested_concurrency=1, reserve_bytes=reserve_bytes,
            build_bytes=build_bytes, metadata_floor_bytes=metadata_floor_bytes)
        report.update(image_config_id=identity, docker_root=str(root),
            bootstrap=bootstrap, requested_concurrency=requested_concurrency,
            race_tolerance_bytes=race_tolerance_bytes)
        if bootstrap["admitted_concurrency"] != 1:
            report["status"] = "insufficient_bootstrap_space"
        else:
            before = bootstrap["free_bytes"]
            report["before_create_free_bytes"] = before
            try:
                execution = runner([*docker_prefix(docker), "create", "--name", name,
                    "--network", "none", "--read-only", "--cap-drop", "ALL",
                    "--security-opt", "no-new-privileges", identity],
                    output / "create.log", timeout_s=120)
                report["commands"].append(execution)
                if execution["exit_code"]:
                    raise EvidenceError("create-only storage probe command failed")
                report["after_create_free_bytes"] = free_space(root)
            finally:
                original = sys.exception()
                try:
                    report["cleanup"] = runner([*docker_prefix(docker), "rm", "--force", name],
                        output / "cleanup.log", timeout_s=30)
                    report["after_cleanup_free_bytes"] = free_space(root)
                except BaseException as cleanup_error:
                    # Cancellation also needs a receipt. Preserve an earlier
                    # failure as primary while retaining this cleanup failure.
                    report["cleanup_failure"] = str(cleanup_error) or type(cleanup_error).__name__
                    if original is not None:
                        original.add_note("named storage probe cleanup did not complete")
                        raise original
                    raise
            if report["cleanup"]["exit_code"]:
                raise EvidenceError("probe cleanup failed; no further containers admitted")
            after = report["after_create_free_bytes"]
            recovered = report["after_cleanup_free_bytes"]
            delta = before - after
            if (delta < size or abs(recovered - before) > race_tolerance_bytes):
                raise EvidenceError("storage probe inconsistent or concurrent filesystem change")
            admitted = plan(driver="vfs", image_size_bytes=size,
                image_root_bytes=delta, free_bytes=recovered,
                requested_concurrency=requested_concurrency,
                reserve_bytes=reserve_bytes, build_bytes=build_bytes,
                metadata_floor_bytes=metadata_floor_bytes)
            report.update(admitted)
            report["kind"] = "container_storage_probe"
            report["limitations"].append("Observed create/cleanup sample; external "
                "filesystem pressure below tolerance cannot be excluded.")
    except BaseException as error:
        # Record cancellation before finally writes the receipt, then rethrow
        # it below. Catching Exception alone would omit its cancelled status.
        report["failure"] = str(error) or type(error).__name__
        if not isinstance(error, Exception):
            report["status"] = "cancelled"
            interrupted = error
        elif not isinstance(error, (EvidenceError, OSError, ValueError, KeyError, TypeError)):
            interrupted = error
    finally:
        atomic_json(output / "preflight.json", report)
    if interrupted is not None:
        raise interrupted
    return report
