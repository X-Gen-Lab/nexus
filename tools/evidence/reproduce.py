#!/usr/bin/env python3
"""Run two independent clean, offline OCI builds and compare actual ELF/BIN."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import os
from pathlib import Path
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, command,
    fields, file_identity, load_json, regular_file, verify_file_identity)
from tools.evidence import consumer
from tools.evidence import storage
from tools.evidence.container import docker_prefix, verify as verify_environment
from tools.evidence.identity import (git_source, verify_source, verify_sdk_package,
                                     resolved_identity)
from tools.evidence.inputs import tree_identity
from tools.measurement.elf import Elf32, binary_from_elf, image_window
from tools.measurement.resources import resolved_float_abi, validate_elf_configuration


def copy_tracked(source: Path, destination: Path) -> dict:
    """Copy Git-tracked bytes and fixed submodules, excluding host caches/.git."""
    destination.mkdir()
    def copy_repository(repository, target):
        listing = subprocess.run(["git", "-C", str(repository), "ls-files",
                                  "--cached", "-z"], capture_output=True, check=True)
        for name in listing.stdout.decode().split("\0"):
            if not name:
                continue
            path = repository / name
            if path.is_symlink():
                raise EvidenceError("tracked source symlink requires explicit support")
            if path.is_dir():
                (target / name).mkdir(parents=True, exist_ok=True)
                copy_repository(path, target / name)
            elif path.is_file():
                (target / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target / name)
            else:
                raise EvidenceError("tracked source input is absent")
    copy_repository(source, destination)
    return tree_identity(destination)


def verify_source_sdk(identity: dict, source: dict) -> dict:
    """Accept only the complete publishable SDK from the same clean source tree."""
    manifest = verify_file_identity(identity)
    if manifest.name != ".nexus-source-sdk.json":
        raise EvidenceError("verified installed source SDK manifest required")
    package = verify_sdk_package(manifest.parent)
    if (package["publishable"] is not True or package["source_dirty"] is not False or
            package["source_revision"] != source["commit"] or
            package["source_tree"] != source["tree"]):
        raise EvidenceError("source SDK must bind the same clean qualified Git commit/tree")
    return package


def copy_source_sdk(identity: dict, source: dict, destination: Path) -> dict:
    """Copy only verified source bytes and required CMake exports, without .git."""
    package = verify_source_sdk(identity, source)
    source_root = Path(identity["path"]).parent
    root = destination / "share/nexus/src"
    root.mkdir(parents=True)
    for name in [*package["files_sha256"], ".nexus-source-sdk.json"]:
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_root / name, target)
    for name in package["package_files_sha256"]:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_root.parents[2] / name, target)
    verify_source_sdk(file_identity(root / ".nexus-source-sdk.json"), source)
    return tree_identity(destination)


def run_logged(argv: list[str], log: Path, *, timeout_s: int = 900,
               cwd: Path | None = None) -> dict:
    """Retain actual stdout/stderr and nonzero status, including tool failure."""
    command(argv)
    if type(timeout_s) is not int or not 0 < timeout_s <= 1800:
        raise EvidenceError("invalid build command timeout")
    log.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    process = None
    def stop():
        if process is None:
            return
        if os.name == "posix":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                # The group already exited; the wait below still reaps the child.
                pass
        elif process.poll() is None:
            process.kill()
        process.wait(timeout=10)
    try:
        with log.open("wb") as output:
            process = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=output,
                stderr=subprocess.STDOUT, start_new_session=os.name == "posix",
                cwd=cwd)
            while process.poll() is None:
                if time.monotonic() - started >= timeout_s:
                    raise subprocess.TimeoutExpired(argv, timeout_s)
                if os.fstat(output.fileno()).st_size > 32 * 1024 * 1024:
                    raise EvidenceError("command raw log exceeded 32 MiB bound")
                try:
                    process.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    # Polling continues to enforce the deadline and log-size bound.
                    pass
            if os.fstat(output.fileno()).st_size > 32 * 1024 * 1024:
                raise EvidenceError("command raw log exceeded 32 MiB bound")
        exit_code = process.returncode
        failure = None
        if os.name == "posix":
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                # No group remains after parent exit, so there is no orphan to kill.
                pass
            else:
                stop()
                exit_code, failure = 125, "command left background processes"
    except subprocess.TimeoutExpired:
        stop()
        exit_code, failure = 124, "build command timed out"
    except EvidenceError as error:
        stop()
        exit_code, failure = 125, str(error)
    except OSError as error:
        stop()
        log.write_text("adapter executable unavailable\n")
        exit_code, failure = 127, str(error)
    except BaseException:
        # Cancellation must not leave a compiler or hardware adapter running.
        stop()
        raise
    # A success with no output still has a nonempty command execution record.
    if not log.stat().st_size:
        log.write_text("command produced no stdout/stderr\n")
    return {"argv": argv, "exit_code": exit_code, "failure": failure,
            "elapsed_s": time.monotonic() - started, "raw": file_identity(log)}


def run_container_logged(argv: list[str], log: Path, *, container_name: str,
                         docker: str = "docker", timeout_s: int = 900) -> dict:
    """Stop the named daemon-owned container after CLI failure or cancellation."""
    if (not isinstance(container_name, str) or not container_name.startswith("nexus-")
            or len(container_name) > 128 or any(
                character not in "abcdefghijklmnopqrstuvwxyz0123456789-_"
                for character in container_name)):
        raise EvidenceError("bounded Nexus container name required")

    def cleanup() -> dict:
        return run_logged([*docker_prefix(docker), "rm", "--force", container_name],
                          log.with_name(log.name + ".cleanup.log"), timeout_s=30)

    try:
        execution = run_logged(argv, log, timeout_s=timeout_s)
    except BaseException as error:
        # Killing the CLI process group cannot stop a Docker daemon workload.
        try:
            removal = cleanup()
            if removal["exit_code"]:
                error.add_note("container cleanup failed; retained raw cleanup log")
        except BaseException as cleanup_error:
            error.add_note("container cleanup did not complete: " + str(cleanup_error))
        raise
    if execution["exit_code"]:
        execution["container_cleanup"] = cleanup()
    return execution


def validate_spec(spec: dict) -> None:
    version = spec.get("schema_version")
    if type(version) is not int or version not in {1, 2}:
        raise EvidenceError("unsupported reproduction schema")
    fields(spec, {"schema_version", "source", "environment", "toolchain",
        "assembly", "commands", "artifacts", "source_date_epoch", "source_sdk"} |
        ({"consumer", "resolved"} if version == 2 else set()))
    verify_source(spec["source"], clean=True)
    verify_source_sdk(spec["source_sdk"], spec["source"])
    if version == 2:
        consumer.verify(spec["consumer"])
        consumer.relative_path(spec["resolved"], "resolved configuration")
        root = Path(spec["consumer"]["source"]["root"]) / spec["consumer"]["project"]
        regular_file(root / consumer.relative_path(spec["assembly"], "consumer assembly"))
    fields(spec["toolchain"], {"path", "sha256", "files", "bytes"})
    actual = tree_identity(Path(spec["toolchain"]["path"]))
    if actual != {key: spec["toolchain"][key] for key in actual}:
        raise EvidenceError("declared toolchain tree differs from actual bytes")
    if (type(spec["source_date_epoch"]) is not int or
            not 0 <= spec["source_date_epoch"] <= 0xffffffff):
        raise EvidenceError("nonnegative SOURCE_DATE_EPOCH required")
    if not isinstance(spec["commands"], list) or not spec["commands"]:
        raise EvidenceError("nonempty build command enumeration required")
    for argv in spec["commands"]:
        command(argv, {key: "/declared/" + key for key in
                      ("source", "build", "toolchain", "assembly", "sdk", "consumer")})
    if not isinstance(spec["artifacts"], dict) or set(spec["artifacts"]) != {"elf", "bin"}:
        raise EvidenceError("actual ELF and BIN comparison required")
    for value in [spec["assembly"], *spec["artifacts"].values()]:
        consumer.relative_path(value, "declared artifact/assembly")


def validate_build_artifacts(build: Path, artifacts: dict,
                             resolved_path: str = "generated/resolved.json") -> None:
    """Recheck the actual linked ELF/BIN against this build's resolved input."""
    resolved, _ = resolved_identity(build / consumer.relative_path(
        resolved_path, "resolved configuration"))
    image = image_window(resolved)
    elf = Elf32(Path(artifacts["elf"]["path"]).read_bytes(),
                float_abi=resolved_float_abi(resolved))
    validate_elf_configuration(elf, resolved)
    if Path(artifacts["bin"]["path"]).read_bytes() != binary_from_elf(
            elf, image["origin"], image["origin"] + image["size"]):
        raise EvidenceError("offline BIN bytes differ from linked ELF")


def container_argv(spec: dict, run: dict, expected: list[str], image: str, *,
                   container_name: str, docker: str = "docker",
                   legacy: bool = False) -> list[str]:
    """Build the complete offline Docker boundary for production and proof.

    `legacy` admits only the exact schema-1 shape before storage admission added
    disabled daemon logging. Proof callers additionally require that the report
    has no storage_preflight; new execution always uses the current boundary.
    This pure function does not access the old host paths in archived receipts.
    """
    version = spec.get("schema_version")
    if (type(version) is not int or version not in {1, 2} or
            type(legacy) is not bool or (legacy and version != 1)):
        raise EvidenceError("unsupported offline Docker boundary version")
    if (not isinstance(container_name, str) or len(container_name) > 128 or
            not re.fullmatch(r"nexus-[a-z0-9][a-z0-9_-]*", container_name)):
        raise EvidenceError("bounded named Nexus reproduction container required")
    if (not isinstance(docker, str) or not docker or len(docker) > 4096 or
            docker.startswith("-") or any(character in docker for character in "\0\r\n") or
            (not Path(docker).is_absolute() and
             not re.fullmatch(r"[A-Za-z0-9_.-]+", docker))):
        raise EvidenceError("one explicit Docker executable required")
    if not isinstance(image, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise EvidenceError("immutable OCI image config identity required")
    epoch = spec.get("source_date_epoch")
    if type(epoch) is not int or not 0 <= epoch <= 0xffffffff:
        raise EvidenceError("bounded SOURCE_DATE_EPOCH required")
    user = run.get("container_user")
    if (not isinstance(user, str) or not re.fullmatch(r"[0-9]{1,10}:[0-9]{1,10}", user) or
            any(int(value) > 0xffffffff for value in user.split(":"))):
        raise EvidenceError("bounded explicit bind-mount UID:GID required")

    def absolute_path(value):
        if (not isinstance(value, str) or not value or len(value) > 4096 or
                any(character in value for character in ",\0\r\n") or
                not Path(value).is_absolute() or value == "/" or
                any(part in {"", ".", ".."} for part in value[1:].split("/"))):
            raise EvidenceError("canonical absolute Docker boundary path required")
        return value

    package = absolute_path(run["package_directory"])
    package_mount = absolute_path(run["package_mount"])
    build = absolute_path(run["output_directory"])
    build_mount = absolute_path(run["output_mount"])
    toolchain = absolute_path(spec["toolchain"]["path"])
    source_mount = absolute_path(run["source_mount"])
    if source_mount != package_mount + "/share/nexus/src":
        raise EvidenceError("source mount must belong to the complete SDK prefix")
    assembly = consumer.relative_path(spec["assembly"], "reproduction assembly").as_posix()
    replacements = {"source": source_mount, "build": build_mount,
        "toolchain": "/toolchain", "sdk": package_mount,
        "assembly": source_mount + "/" + assembly}
    mounts = [(package, package_mount, True), (build, build_mount, False),
              (toolchain, "/toolchain", True)]
    if version == 2:
        consumer_directory = absolute_path(run["consumer_directory"])
        consumer_mount = absolute_path(run["consumer_mount"])
        mounts.append((consumer_directory, consumer_mount, True))
        replacements.update(consumer=consumer_mount,
                            assembly=consumer_mount + "/" + assembly)
    for index, (_, destination, _) in enumerate(mounts):
        for _, other, _ in mounts[:index]:
            if (Path(destination).is_relative_to(other) or
                    Path(other).is_relative_to(destination)):
                raise EvidenceError("Docker boundary mount destinations must be independent")
    argv = [*docker_prefix(docker), "run", "--rm", "--name", container_name,
        "--user", user, "--network", "none", "--read-only",
        *([] if legacy else ["--log-driver", "none"]),
        "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
        "--pids-limit", "256", "--tmpfs", "/tmp:rw,nosuid,nodev,size=256m",
        "--env", "HTTP_PROXY=", "--env", "HTTPS_PROXY=",
        "--env", "ALL_PROXY=", "--env", "http_proxy=",
        "--env", "https_proxy=", "--env", "all_proxy=",
        "--env", "SOURCE_DATE_EPOCH=" + str(epoch), "--env", "LC_ALL=C",
        "--env", "TZ=UTC"]
    for host, destination, readonly in mounts:
        argv += ["--mount", f"type=bind,src={host},dst={destination}" +
                 (",readonly" if readonly else "")]
    argv += ["--workdir", build_mount, image, *command(expected, replacements)]
    return command(argv)


@contextmanager
def storage_lease(path: Path | None = None):
    """Serialize daemon-owned copies; never unlink the shared lock inode."""
    if os.name != "posix":
        raise EvidenceError("formal Docker storage admission requires POSIX locking")
    import fcntl
    path = path or Path(tempfile.gettempdir()) / (
        "nexus-offline-reproduction-" + str(os.getuid()) + ".lock")
    descriptor = None
    locked = False
    try:
        descriptor = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    except OSError as error:
        raise EvidenceError("cannot acquire the regular Docker storage lease") from error
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid():
            raise EvidenceError("storage lease must be a regular file owned by this user")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            locked = True
        except BlockingIOError as error:
            raise EvidenceError("another offline reproduction is already active") from error
        yield
    finally:
        if descriptor is not None:
            if locked:
                fcntl.flock(descriptor, fcntl.LOCK_UN)
            os.close(descriptor)


def reproduce(spec_path: Path, report_path: Path, *, docker: str = "docker") -> dict:
    try:
        with storage_lease():
            return _reproduce(spec_path, report_path, docker=docker)
    except EvidenceError as error:
        report = {"schema_version": 1, "kind": "offline_clean_rebuild",
            "status": "failed", "hermetic_status": "not_established",
            "reproducible_status": "not_established", "runs": [],
            "physical_status": "not_executed", "failure": str(error)}
        atomic_json(report_path, report)
        return report


def _reproduce(spec_path: Path, report_path: Path, *, docker: str) -> dict:
    spec = load_json(spec_path)
    report = {"schema_version": 1, "kind": "offline_clean_rebuild",
              "status": "failed", "hermetic_status": "not_established",
              "reproducible_status": "not_established", "runs": [],
              "physical_status": "not_executed", "spec": file_identity(spec_path)}
    try:
        validate_spec(spec)
        environment = verify_environment(Path(spec["environment"]), docker=docker)
        if not hasattr(os, "getuid") or not hasattr(os, "getgid"):
            raise EvidenceError("formal bind-mount execution requires POSIX UID/GID")
        container_user = f"{os.getuid()}:{os.getgid()}"
        report["source"] = spec["source"]
        report["environment"] = file_identity(spec["environment"])
        report["toolchain"] = spec["toolchain"]
        report["source_sdk"] = spec["source_sdk"]
        if "consumer" in spec:
            report["consumer"] = spec["consumer"]
        report["declared_input_boundary"] = {
            "source": "verified complete publishable source SDK from the clean Git tree, read-only",
            "toolchain": "declared content-hashed tree, read-only",
            "environment": "verified OCI root filesystem",
            "network": "none", "credentials": "no host credential mounts",
            "outputs": "independent empty build directory per run"}
        if "consumer" in spec:
            report["declared_input_boundary"]["consumer"] = (
                "separate clean Git project; tracked regular files only, read-only")
        runs_root = report_path.absolute().parent / (report_path.stem + "-runs")
        if runs_root.exists():
            raise EvidenceError("reproduction needs new independent clean run roots")
        runs_root.mkdir(parents=True)
        try:
            preflight = storage.inspect(environment["image_config_id"], docker=docker,
                read_only=True, tmpfs_bytes=256 * storage.MIB,
                daemon_log_driver="none")
        except EvidenceError as error:
            report["storage_inspection_fallback"] = str(error)
            preflight = storage.probe(environment["image_config_id"],
                output=runs_root / "storage-probe", runner=run_logged, docker=docker)
        report["storage_preflight"] = preflight
        if preflight.get("admitted_concurrency") != 1:
            raise EvidenceError("physical Docker storage preflight did not admit one container")
        for index in range(2):
            run_root = runs_root / ("run-a" if index == 0 else "different-path-b")
            run_root.mkdir()
            package_snapshot = run_root / "source-sdk-prefix"
            package_identity = copy_source_sdk(spec["source_sdk"], spec["source"], package_snapshot)
            snapshot = package_snapshot / "share/nexus/src"
            snapshot_identity = tree_identity(snapshot)
            build = run_root / "build"
            build.mkdir()
            mount_package = "/inputs/sdk-a" if index == 0 else "/other/sdk-b"
            mount_source = mount_package + "/share/nexus/src"
            mount_build = "/work/build-a" if index == 0 else "/different/build-b"
            run = {"source_snapshot": snapshot_identity,
                   "source_package_snapshot": package_identity,
                   "package_directory": str(package_snapshot), "package_mount": mount_package,
                   "source_directory": str(snapshot), "output_directory": str(build),
                   "source_mount": mount_source, "output_mount": mount_build,
                   "clean_output": True, "container_user": container_user, "commands": [],
                   "artifacts": {}, "status": "failed"}
            report["runs"].append(run)
            if "consumer" in spec:
                consumer_snapshot = run_root / "consumer"
                consumer_identity = consumer.copy(spec["consumer"], consumer_snapshot)
                mount_consumer = "/inputs/consumer-a" if index == 0 else "/other/consumer-b"
                regular_file(consumer_snapshot / spec["assembly"])
                from tools.evidence.proof import sdk_file_identity
                run.update(consumer_directory=str(consumer_snapshot),
                    consumer_mount=mount_consumer, consumer_snapshot=consumer_identity,
                    consumer_files={path.relative_to(consumer_snapshot).as_posix():
                        sdk_file_identity(path) for path in sorted(consumer_snapshot.rglob("*"))
                        if path.is_file()})
            else:
                regular_file(snapshot / spec["assembly"])
            for number, argv in enumerate(spec["commands"]):
                admission = storage.plan(driver=preflight["driver"],
                    image_size_bytes=preflight["image_size_bytes"],
                    image_root_bytes=preflight["image_root_bytes"],
                    free_bytes=storage.free_space(Path(preflight["docker_root"])),
                    reserve_bytes=preflight["reserve_bytes"],
                    build_bytes=preflight["build_reserve_bytes"],
                    read_only=True, tmpfs_bytes=256 * storage.MIB,
                    daemon_log_driver="none")
                run.setdefault("storage_admissions", []).append(admission)
                if admission["admitted_concurrency"] != 1:
                    raise EvidenceError("available physical storage fell below the container budget")
                container_name = "nexus-repro-" + os.urandom(8).hex()
                docker_argv = container_argv(spec, run, argv,
                    environment["image_config_id"], container_name=container_name,
                    docker=docker)
                execution = run_container_logged(
                    docker_argv, run_root / f"command-{number}.log",
                    docker=docker, container_name=container_name)
                run["commands"].append(execution)
                if execution["exit_code"]:
                    raise EvidenceError("offline build command failed; retained actual log")
            if tree_identity(package_snapshot) != package_identity:
                raise EvidenceError("read-only source SDK prefix snapshot changed")
            if "consumer" in spec and tree_identity(consumer_snapshot) != consumer_identity:
                raise EvidenceError("read-only external consumer snapshot changed")
            for role, relative in spec["artifacts"].items():
                path = regular_file(build / relative)
                run["artifacts"][role] = file_identity(path)
            selected_resolved = spec.get("resolved", "generated/resolved.json")
            validate_build_artifacts(build, run["artifacts"], selected_resolved)
            run["resolved"] = file_identity(build / selected_resolved)
            run["status"] = "passed"
        verify_source(spec["source"], clean=True)
        verify_source_sdk(spec["source_sdk"], spec["source"])
        if "consumer" in spec:
            consumer.verify(spec["consumer"])
        if tree_identity(Path(spec["toolchain"]["path"])) != actual_toolchain(spec):
            raise EvidenceError("declared toolchain changed during reproduction")
        if report["runs"][0]["source_package_snapshot"] != report["runs"][1]["source_package_snapshot"]:
            raise EvidenceError("independent source SDK prefix snapshots differ")
        differences = [role for role in ("elf", "bin") if
            report["runs"][0]["artifacts"][role]["sha256"] !=
            report["runs"][1]["artifacts"][role]["sha256"]]
        report["different_artifacts"] = differences
        report["hermetic_status"] = "passed_declared_container_boundary"
        if differences:
            raise EvidenceError("independent ELF/BIN builds are not byte-identical")
        report.update(status="passed", reproducible_status="bit_exact_elf_bin")
    except (EvidenceError, OSError, ValueError, KeyError, TypeError,
            subprocess.SubprocessError) as error:
        report["failure"] = str(error)
    except BaseException as error:
        report.update(status="cancelled", failure=str(error) or type(error).__name__)
        atomic_json(report_path, report)
        raise
    atomic_json(report_path, report)
    return report


def actual_toolchain(spec: dict) -> dict:
    return {key: spec["toolchain"][key] for key in ("sha256", "files", "bytes")}


def prepare_spec(source: Path, assembly: Path, environment: Path, toolchain: Path,
                 workload: str = "empty", *, source_sdk: Path,
                 consumer_source: Path | None = None, consumer_project: str = ".",
                 artifacts: dict | None = None, resolved: str | None = None) -> dict:
    """Declare exact current inputs before invoking two clean Docker builds."""
    source = source.resolve()
    assembly = regular_file(assembly)
    if consumer_source is None and not assembly.is_relative_to(source):
        raise EvidenceError("reproduction assembly must belong to the tracked source input")
    if workload not in {"empty", "gpio", "uart", "spi"}:
        raise EvidenceError("unknown minimum reproduction workload")
    source_record = git_source(source)
    verify_source(source_record, clean=True)
    sdk_identity = file_identity(source_sdk)
    verify_source_sdk(sdk_identity, source_record)
    epoch = subprocess.run(["git", "-C", str(source), "show", "-s", "--format=%ct", "HEAD"],
                           capture_output=True, check=True)
    path_maps = ("-ffile-prefix-map={source}=. -fdebug-prefix-map={source}=. "
                 "-ffile-prefix-map={build}=build -fdebug-prefix-map={build}=build")
    spec = {"schema_version": 1, "source": source_record, "source_sdk": sdk_identity,
        "environment": str(environment.absolute()),
        "toolchain": {"path": str(toolchain.absolute()), **tree_identity(toolchain)},
        "assembly": assembly.relative_to(source).as_posix() if consumer_source is None else "",
        "source_date_epoch": int(epoch.stdout),
        "commands": [["cmake", "-S", "{source}", "-B", "{build}", "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE={source}/cmake/toolchains/arm-gcc.cmake",
            "-DNEXUS_ASSEMBLY_FILE={assembly}", "-DNEXUS_BUILD_TESTS=OFF",
            "-DNEXUS_WORKLOAD=" + workload, "-DCMAKE_C_FLAGS=" + path_maps,
            "-DCMAKE_ASM_FLAGS=" + path_maps],
            ["cmake", "--build", "{build}", "--parallel", "2"]],
        "artifacts": {"elf": "bin/nexus_firmware.elf", "bin": "bin/nexus_firmware.bin"}}
    if consumer_source is not None:
        selected = consumer.identify(consumer_source, consumer_project)
        project_root = Path(selected["source"]["root"]) / selected["project"]
        if not assembly.is_relative_to(project_root):
            raise EvidenceError("assembly must belong to the declared external consumer project")
        consumer_maps = path_maps + " -ffile-prefix-map={consumer}=consumer -fdebug-prefix-map={consumer}=consumer"
        spec.update(schema_version=2, consumer=selected,
            assembly=assembly.relative_to(project_root).as_posix(),
            resolved=resolved or "nexus-sdk/generated/resolved.json")
        spec["commands"][0] = ["cmake", "-S", "{consumer}", "-B", "{build}", "-G", "Ninja",
            "-DNexus_DIR={sdk}/lib/cmake/Nexus",
            "-DCMAKE_TOOLCHAIN_FILE={source}/cmake/toolchains/arm-gcc.cmake",
            "-DNEXUS_ASSEMBLY_FILE={assembly}", "-DNEXUS_BUILD_TESTS=OFF",
            "-DNEXUS_BUILD_WORKLOAD=OFF", "-DCMAKE_C_FLAGS=" + consumer_maps,
            "-DCMAKE_ASM_FLAGS=" + consumer_maps]
    elif resolved is not None:
        raise EvidenceError("custom resolved path requires an external consumer project")
    if artifacts is not None:
        spec["artifacts"] = artifacts
    validate_spec(spec)
    return spec


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--docker", default="docker")
    parser.add_argument("--prepare-spec", action="store_true")
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--assembly", type=Path)
    parser.add_argument("--environment", type=Path)
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--source-sdk", type=Path)
    parser.add_argument("--consumer-source", type=Path)
    parser.add_argument("--consumer-project", default=".")
    parser.add_argument("--resolved")
    parser.add_argument("--artifact-elf")
    parser.add_argument("--artifact-bin")
    parser.add_argument("--workload", choices=("empty", "gpio", "uart", "spi"), default="empty")
    parser.add_argument("--qualification", type=Path)
    args = parser.parse_args()
    if args.prepare_spec:
        if None in (args.assembly, args.environment, args.toolchain, args.source_sdk):
            parser.error("prepare requires --assembly --environment --toolchain --source-sdk")
        try:
            if bool(args.artifact_elf) != bool(args.artifact_bin):
                raise EvidenceError("declare both --artifact-elf and --artifact-bin")
            spec = prepare_spec(args.source, args.assembly, args.environment,
                args.toolchain, args.workload, source_sdk=args.source_sdk,
                consumer_source=args.consumer_source, consumer_project=args.consumer_project,
                resolved=args.resolved, artifacts={"elf": args.artifact_elf,
                    "bin": args.artifact_bin} if args.artifact_elf else None)
            atomic_json(args.spec, spec)
            print("Declared current source/config/toolchain/OCI inputs: " + str(args.spec))
            return 0
        except (EvidenceError, OSError, ValueError, subprocess.SubprocessError) as error:
            print("Reproduction spec rejected: " + str(error), file=sys.stderr)
            return 1
    if args.report is None:
        parser.error("execution requires --report")
    if args.spec.resolve() == args.report.resolve():
        parser.exit(1, "report cannot overwrite reproduction spec\n")
    if args.qualification:
        if args.qualification.resolve() in (args.spec.resolve(), args.report.resolve()):
            parser.exit(1, "qualification cannot overwrite reproduction inputs/report\n")
        args.qualification.unlink(missing_ok=True)
    report = reproduce(args.spec, args.report, docker=args.docker)
    if args.qualification:
        if report["status"] == "passed":
            from tools.evidence.qualification import create_qualification
            elf = Path(report["runs"][0]["artifacts"]["elf"]["path"])
            resolved = Path(report["runs"][0]["resolved"]["path"])
            checks = [{"name": f"clean_rebuild_{run_index}_{command_index}", **execution}
                for run_index, run in enumerate(report["runs"])
                for command_index, execution in enumerate(run["commands"])]
            qualified = create_qualification("reproducibility", report["source"],
                resolved, elf, checks, evidence=[file_identity(args.report)])
            atomic_json(args.qualification, qualified)
    print("Independent offline ELF/BIN rebuild: " + report["status"])
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
