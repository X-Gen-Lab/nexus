#!/usr/bin/env python3
"""Run two independent clean, offline OCI builds and compare actual ELF/BIN."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, command,
    fields, file_identity, load_json, regular_file, stream_digest, verify_file_identity)
from tools.evidence.container import docker_prefix, verify as verify_environment
from tools.evidence.identity import git_source, verify_source, verify_sdk_package
from tools.measurement.elf import Elf32, binary_from_elf


def tree_identity(root: Path) -> dict:
    if root.is_symlink() or not root.is_dir():
        raise EvidenceError("declared input directory missing or symlink")
    records = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise EvidenceError("undeclared symlink in input tree")
        if path.is_file():
            with path.open("rb") as stream:
                sha256 = stream_digest(stream)
            records.append({"path": path.relative_to(root).as_posix(),
                            "size": path.stat().st_size,
                            "sha256": sha256})
    if not records:
        raise EvidenceError("empty declared input directory")
    encoded = json.dumps(records, sort_keys=True, separators=(",", ":")).encode()
    return {"sha256": hashlib.sha256(encoded).hexdigest(), "files": len(records),
            "bytes": sum(entry["size"] for entry in records)}


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


def run_logged(argv: list[str], log: Path, *, timeout_s: int = 900) -> dict:
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
                stderr=subprocess.STDOUT, start_new_session=os.name == "posix")
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
        log.write_text("adapter executable unavailable\n")
        exit_code, failure = 127, str(error)
    # A success with no output still has a nonempty command execution record.
    if not log.stat().st_size:
        log.write_text("command produced no stdout/stderr\n")
    return {"argv": argv, "exit_code": exit_code, "failure": failure,
            "elapsed_s": time.monotonic() - started, "raw": file_identity(log)}


def validate_spec(spec: dict) -> None:
    fields(spec, {"schema_version", "source", "environment", "toolchain",
        "assembly", "commands", "artifacts", "source_date_epoch", "source_sdk"})
    if type(spec["schema_version"]) is not int or spec["schema_version"] != 1:
        raise EvidenceError("unsupported reproduction schema")
    verify_source(spec["source"], clean=True)
    verify_source_sdk(spec["source_sdk"], spec["source"])
    fields(spec["toolchain"], {"path", "sha256", "files", "bytes"})
    actual = tree_identity(Path(spec["toolchain"]["path"]))
    if actual != {key: spec["toolchain"][key] for key in actual}:
        raise EvidenceError("declared toolchain tree differs from actual bytes")
    if type(spec["source_date_epoch"]) is not int or spec["source_date_epoch"] < 0:
        raise EvidenceError("nonnegative SOURCE_DATE_EPOCH required")
    if not isinstance(spec["commands"], list) or not spec["commands"]:
        raise EvidenceError("nonempty build command enumeration required")
    for argv in spec["commands"]:
        command(argv, {key: "/declared/" + key for key in
                      ("source", "build", "toolchain", "assembly")})
    if not isinstance(spec["artifacts"], dict) or set(spec["artifacts"]) != {"elf", "bin"}:
        raise EvidenceError("actual ELF and BIN comparison required")
    for value in [spec["assembly"], *spec["artifacts"].values()]:
        if (not isinstance(value, str) or not value or Path(value).is_absolute()
                or ".." in Path(value).parts or "," in value):
            raise EvidenceError("declared artifact/assembly must be contained relative path")


def reproduce(spec_path: Path, report_path: Path, *, docker: str = "docker") -> dict:
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
        report["declared_input_boundary"] = {
            "source": "verified complete publishable source SDK from the clean Git tree, read-only",
            "toolchain": "declared content-hashed tree, read-only",
            "environment": "verified OCI root filesystem",
            "network": "none", "credentials": "no host credential mounts",
            "outputs": "independent empty build directory per run"}
        runs_root = report_path.absolute().parent / (report_path.stem + "-runs")
        if runs_root.exists():
            raise EvidenceError("reproduction needs new independent clean run roots")
        runs_root.mkdir(parents=True)
        for index in range(2):
            run_root = runs_root / ("run-a" if index == 0 else "different-path-b")
            run_root.mkdir()
            package_snapshot = run_root / "source-sdk-prefix"
            package_identity = copy_source_sdk(spec["source_sdk"], spec["source"], package_snapshot)
            snapshot = package_snapshot / "share/nexus/src"
            snapshot_identity = tree_identity(snapshot)
            regular_file(snapshot / spec["assembly"])
            build = run_root / "build"
            build.mkdir()
            mount_package = "/inputs/sdk-a" if index == 0 else "/other/sdk-b"
            mount_source = mount_package + "/share/nexus/src"
            mount_build = "/work/build-a" if index == 0 else "/different/build-b"
            mount_toolchain = "/toolchain"
            replacements = {"source": mount_source, "build": mount_build,
                "toolchain": mount_toolchain,
                "assembly": mount_source + "/" + spec["assembly"]}
            run = {"source_snapshot": snapshot_identity,
                   "source_package_snapshot": package_identity,
                   "package_directory": str(package_snapshot), "package_mount": mount_package,
                   "source_directory": str(snapshot), "output_directory": str(build),
                   "source_mount": mount_source, "output_mount": mount_build,
                   "clean_output": True, "container_user": container_user, "commands": [],
                   "artifacts": {}, "status": "failed"}
            report["runs"].append(run)
            for number, argv in enumerate(spec["commands"]):
                expanded = command(argv, replacements)
                container_name = "nexus-repro-" + os.urandom(8).hex()
                docker_argv = [*docker_prefix(docker), "run", "--rm", "--name",
                    container_name, "--user", container_user, "--network", "none", "--read-only",
                    "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
                    "--pids-limit", "256", "--tmpfs", "/tmp:rw,nosuid,nodev,size=256m",
                    "--env", "HTTP_PROXY=", "--env", "HTTPS_PROXY=",
                    "--env", "ALL_PROXY=", "--env", "http_proxy=",
                    "--env", "https_proxy=", "--env", "all_proxy=",
                    "--env", "SOURCE_DATE_EPOCH=" + str(spec["source_date_epoch"]),
                    "--env", "LC_ALL=C", "--env", "TZ=UTC",
                    "--mount", f"type=bind,src={package_snapshot},dst={mount_package},readonly",
                    "--mount", f"type=bind,src={build},dst={mount_build}",
                    "--mount", f"type=bind,src={spec['toolchain']['path']},dst={mount_toolchain},readonly",
                    "--workdir", mount_build, environment["image_config_id"], *expanded]
                execution = run_logged(docker_argv, run_root / f"command-{number}.log")
                run["commands"].append(execution)
                if execution["exit_code"]:
                    # docker CLI timeout does not establish container termination.
                    subprocess.run([*docker_prefix(docker), "rm", "--force", container_name],
                                   capture_output=True, timeout=30, check=False)
                    raise EvidenceError("offline build command failed; retained actual log")
            if tree_identity(package_snapshot) != package_identity:
                raise EvidenceError("read-only source SDK prefix snapshot changed")
            for role, relative in spec["artifacts"].items():
                path = regular_file(build / relative)
                if role == "elf":
                    Elf32(path.read_bytes())
                run["artifacts"][role] = file_identity(path)
            from tools.evidence.identity import resolved_identity
            resolved, _ = resolved_identity(build / "generated/resolved.json")
            flash = resolved["flash"]
            elf = Elf32(Path(run["artifacts"]["elf"]["path"]).read_bytes())
            if Path(run["artifacts"]["bin"]["path"]).read_bytes() != binary_from_elf(
                    elf, flash["origin"], flash["origin"] + flash["size"]):
                raise EvidenceError("offline BIN bytes differ from linked ELF")
            run["status"] = "passed"
        verify_source(spec["source"], clean=True)
        verify_source_sdk(spec["source_sdk"], spec["source"])
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
    atomic_json(report_path, report)
    return report


def actual_toolchain(spec: dict) -> dict:
    return {key: spec["toolchain"][key] for key in ("sha256", "files", "bytes")}


def prepare_spec(source: Path, assembly: Path, environment: Path, toolchain: Path,
                 workload: str = "empty", *, source_sdk: Path) -> dict:
    """Declare exact current inputs before invoking two clean Docker builds."""
    source = source.resolve()
    assembly = regular_file(assembly)
    if not assembly.is_relative_to(source):
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
    return {"schema_version": 1, "source": source_record, "source_sdk": sdk_identity,
        "environment": str(environment.absolute()),
        "toolchain": {"path": str(toolchain.absolute()), **tree_identity(toolchain)},
        "assembly": assembly.relative_to(source).as_posix(),
        "source_date_epoch": int(epoch.stdout),
        "commands": [["cmake", "-S", "{source}", "-B", "{build}", "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE={source}/cmake/toolchains/arm-gcc.cmake",
            "-DNEXUS_ASSEMBLY_FILE={assembly}", "-DNEXUS_BUILD_TESTS=OFF",
            "-DNEXUS_WORKLOAD=" + workload, "-DCMAKE_C_FLAGS=" + path_maps,
            "-DCMAKE_ASM_FLAGS=" + path_maps],
            ["cmake", "--build", "{build}", "--parallel", "2"]],
        "artifacts": {"elf": "bin/nexus_firmware.elf", "bin": "bin/nexus_firmware.bin"}}


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
    parser.add_argument("--workload", choices=("empty", "gpio", "uart", "spi"), default="empty")
    parser.add_argument("--qualification", type=Path)
    args = parser.parse_args()
    if args.prepare_spec:
        if None in (args.assembly, args.environment, args.toolchain, args.source_sdk):
            parser.error("prepare requires --assembly --environment --toolchain --source-sdk")
        try:
            spec = prepare_spec(args.source, args.assembly, args.environment,
                                args.toolchain, args.workload, source_sdk=args.source_sdk)
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
    report = reproduce(args.spec, args.report, docker=args.docker)
    if args.qualification:
        args.qualification.unlink(missing_ok=True)
        if report["status"] == "passed":
            from tools.evidence.qualification import create_qualification
            elf = Path(report["runs"][0]["artifacts"]["elf"]["path"])
            resolved = elf.parent.parent / "generated/resolved.json"
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
