"""Build and inspect actual U575 Secure/NonSecure reference images.

This gate proves software bytes and a paired CMSE linkage boundary. It never
programs option bytes, runs a station or seals a single-ELF software candidate.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.ci import arch_compile, provider_assemblies
from tools.configure import dependencies
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   regular_file)
from tools.evidence.identity import git_source
from tools.measurement.elf import Elf32, binary_from_elf
from tools.measurement.soc_costs import decode_function


def section(elf, name):
    matches = [item for item in elf.sections if item["name"] == name]
    if len(matches) != 1:
        raise EvidenceError("one required section missing: " + name)
    return matches[0]


def bytes_at(elf, address, size):
    for item in elf.sections:
        if (item["kind"] != 8 and item["address"] <= address
                and address + size <= item["address"] + item["size"]):
            offset = item["offset"] + address - item["address"]
            return elf.data[offset:offset + size]
    raise EvidenceError("required bytes leave the file-backed section")


def contained(base, size, regions):
    return any(first <= base < base + size <= end for first, end in regions)


def validate_pair(secure_bytes, nonsecure_bytes, imported):
    """Validate real vectors, LOAD permissions, gateway and partition bytes."""
    secure = Elf32(secure_bytes, float_abi="soft")
    nonsecure = Elf32(nonsecure_bytes, float_abi="soft")
    stacks = []
    for elf, flash, ram, top, reset in (
            (secure, (0x0c000000, 0x0c100000),
             (0x30000000, 0x30030000), 0x30030000,
             "Secure_Reset_Handler"),
            (nonsecure, (0x08100000, 0x08110000),
             (0x20040000, 0x20044000), 0x20044000,
             "NonSecure_Reset_Handler")):
        vectors = section(elf, ".isr_vector")
        if (vectors["address"] != flash[0] or vectors["size"] != 568
                or vectors["kind"] == 8 or vectors["flags"] != 2):
            raise EvidenceError("exact U575 vector placement required")
        values = struct.unpack("<142I", bytes_at(elf, flash[0], 568))
        if (values[0] != top or values[1] != elf.entry
                or values[1] != elf.symbol(reset)
                or not flash[0] <= values[1] & ~1 < flash[1]):
            raise EvidenceError("actual vector stack/reset differs from partition")
        if elf is nonsecure and any(values[2:]):
            raise EvidenceError("NS privileged exception handlers are forbidden")
        if elf is nonsecure and (values[1] & ~1) < flash[0] + 1024:
            raise EvidenceError("NS Reset enters the reserved vector page")
        stack = section(elf, ".stack")
        expected = 4096 if elf is secure else 2048
        if (stack["address"] != top - expected or stack["size"] != expected
                or stack["kind"] != 8 or stack["flags"] != 3
                or elf.symbols.get("__stack_bottom") != stack["address"]
                or elf.symbols.get("__stack_top") != top):
            raise EvidenceError("actual stack reservation differs from partition")
        stacks.append(stack["size"])
        for load in elf.loads:
            if (load["memory"] and contained(
                    load["virtual"], load["memory"], (flash,))
                    and load["physical"] != load["virtual"]):
                raise EvidenceError("Flash load address differs from its VMA")
            if load["memory"] and not contained(
                    load["virtual"], load["memory"], (flash, ram)):
                raise EvidenceError("LOAD leaves its security-domain partition")
            if load["files"] and not contained(
                    load["physical"], load["files"], (flash,)):
                raise EvidenceError("initialization LOAD leaves owned Flash")
            if load["flags"] & 1 and not contained(
                    load["virtual"], load["files"], (flash,)):
                raise EvidenceError("executable RAM is not in the reference")
            if load["flags"] & 2 and not contained(
                    load["virtual"], load["memory"], (ram,)):
                raise EvidenceError("Flash is not writable reference storage")
        if elf.symbol("__data_load") & 3:
            raise EvidenceError("startup copy requires aligned DATA LMA")
        # Resolve each allocated file-backed section through exactly one LOAD.
        # A copied SG section outside LOAD bytes cannot become flash evidence.
        binary_from_elf(elf, flash[0], flash[1])
    nsc = section(secure, ".gnu.sgstubs")
    gateway_names = ("nx_reference_gpio", "nx_reference_gpio_batch")
    gateways = {name: secure.symbol(name) & ~1 for name in gateway_names}
    gateway = gateways["nx_reference_gpio"]
    if (nsc["address"] != 0x0c0fe000 or nsc["size"] == 0
            or nsc["size"] > 0x2000 or nsc["address"] & 31
            or len(set(gateways.values())) != len(gateway_names)
            or any(not nsc["address"] <= address < nsc["address"] + nsc["size"]
                   or bytes_at(secure, address, 4) != b"\x7f\xe9\x7f\xe9"
                   for address in gateways.values())):
        raise EvidenceError("actual SG veneer missing from exact NSC partition")
    if (type(imported) is not dict or set(imported) != set(gateway_names)
            or any(type(imported[name]) is not int or
                   imported[name] != address or
                   nonsecure.symbol(name) & ~1 != address
                   for name, address in gateways.items())):
        raise EvidenceError("Secure import and NS gateway addresses differ")
    for name in ("nx_stm32u575_tz_prepare", "nx_stm32u575_tz_launch",
                 "nx_arch_nonsecure_check", "nx_user_io_broker_gpio",
                 "nx_stm32u575_secure_batch_submit",
                 "nx_stm32u575_secure_batch_service",
                 "nx_stm32u575_secure_batch_ns_spans"):
        if not 0x0c000000 <= secure.symbol(name) & ~1 < 0x0c0fe000:
            raise EvidenceError("trusted implementation missing from Secure Flash")
    return {"gateway_address": gateway, "gateways": gateways,
            "nsc_bytes": nsc["size"],
            "ns_vector_count": 142, "ns_exception_handlers": 0,
            "secure_stack_bytes": stacks[0], "nonsecure_stack_bytes": stacks[1],
            "float_abi": "soft", "principal": "one fixed NS world"}


def imported_gateways(raw):
    values = {}
    for name in ("nx_reference_gpio", "nx_reference_gpio_batch"):
        matches = re.findall(r"^" + name + r"\s+[TtAa]\s+([0-9a-fA-F]+)(?:\s|$)",
                             raw, re.MULTILINE)
        if len(matches) != 1:
            raise EvidenceError("both actual gateway import symbols required")
        values[name] = int(matches[0], 16) & ~1
    return values


def source_inputs(root):
    """Bind the complete maintained graph and fixture, including new files."""
    paths = {Path(item["path"]) for item in
             provider_assemblies.source_inputs(root, [])}
    runtime_paths, _ = dependencies.snapshot(mode="runtime")
    paths.update(Path(path) for path in runtime_paths.values())
    paths.add(Path(__file__).resolve())
    for directory in (root / "tests/contracts/u575_trustzone",
                      root / "vendors/st/cmsis_device_u5/Include",
                      root / "vendors/arm/CMSIS_5/CMSIS/Core/Include"):
        paths.update(path for path in directory.rglob("*") if path.is_file()
                     and "__pycache__" not in path.parts)
    return [arch_compile.digest(path) for path in sorted(paths)]


def compile_profile(build, stage, compiler):
    """Check effective CPU/security flags in each real compiler invocation."""
    path = build / "compile_commands.json"
    commands = json.loads(path.read_text())
    if not isinstance(commands, list) or not commands:
        raise EvidenceError("actual paired compile database required")
    for row in commands:
        argv = row.get("arguments") or shlex.split(row["command"])
        cpu = [arg for arg in argv if arg.startswith("-mcpu=")]
        abi = [arg for arg in argv if arg.startswith("-mfloat-abi=")]
        if (Path(argv[0]).resolve() != Path(compiler).resolve()
                or cpu != ["-mcpu=cortex-m33"] or abi != ["-mfloat-abi=soft"]
                or "-mthumb" not in argv
                or any(arg.startswith("-mfpu=") for arg in argv)
                or ("-mcmse" in argv) != (stage == "secure")):
            raise EvidenceError("actual compiler CPU/security ABI differs")
    return arch_compile.digest(path)


def _capture_file(path):
    """Hash bounded regular bytes while retaining their observation metadata."""
    from tools.evidence import u575_pair
    path = regular_file(path, nonempty=False)
    before = path.stat()
    if before.st_size > u575_pair.MAX_BYTES:
        raise EvidenceError("pair capture input exceeds bound")
    checksum = hashlib.sha256()
    remaining = before.st_size
    with path.open("rb") as stream:
        while remaining:
            block = stream.read(min(u575_pair.CHUNK, remaining))
            if not block:
                raise EvidenceError("pair capture input changed")
            checksum.update(block)
            remaining -= len(block)
        if stream.read(1):
            raise EvidenceError("pair capture input changed")
    after = path.stat()
    keys = ("st_dev", "st_ino", "st_mode", "st_uid", "st_gid", "st_size",
            "st_mtime_ns", "st_ctime_ns")
    if any(getattr(before, key) != getattr(after, key) for key in keys):
        raise EvidenceError("pair capture input changed")
    return {"path": str(path), "bytes": before.st_size,
            "sha256": checksum.hexdigest(), "mode": before.st_mode & 0o777,
            "device": before.st_dev, "inode": before.st_ino,
            "uid": before.st_uid, "gid": before.st_gid,
            "mtime_ns": before.st_mtime_ns, "ctime_ns": before.st_ctime_ns}


def _capture_metadata(label):
    """Bound the read itself, including growth after the observed size."""
    from tools.evidence import u575_pair
    path = regular_file(label, nonempty=False)
    size = path.stat().st_size
    if size > u575_pair.MAX_METADATA:
        raise EvidenceError("pair metadata exceeds bound")
    with path.open("rb") as stream:
        raw = stream.read(size + 1)
    if len(raw) != size:
        raise EvidenceError("pair metadata changed during read")
    return raw


def begin_pair_context(output, tools, recorder, source):
    """Retain actual GCC-selected libraries before any configure/build call."""
    from tools.evidence import u575_pair
    u575_pair._source(source)
    control = Path(output) / "pair-context"
    if (control.exists() or any(p.is_symlink() for p in
                               (control, *control.parents))):
        raise EvidenceError("pair context control must be fresh and unaliased")
    if set(tools) != u575_pair.TOOLS:
        raise EvidenceError("exact original pair tool set required")
    # The original gate resolves tool links to their actual file identities.
    tool_before = {name: _capture_file(Path(path).resolve())
                   for name, path in tools.items()}
    compiler = tool_before["gcc"]
    libraries, aliases = {}, set()
    # A query uses the same ABI as both actual stages, never host defaults.
    for name in ("libgcc.a", "libc_nano.a", "libnosys.a"):
        raw = recorder.execute([tools["gcc"], "-mcpu=cortex-m33", "-mthumb",
            "-mfloat-abi=soft", "--print-file-name=" + name],
            "pair-before-" + name)
        lines = raw.splitlines()
        if len(lines) != 1 or not Path(lines[0]).is_absolute():
            raise EvidenceError(
                "actual absolute GCC library selection required")
        # Reject symlink components before collapsing legitimate bin/../lib.
        regular_file(lines[0])
        path = Path(os.path.normpath(lines[0]))
        row = _capture_file(path)
        alias = (row["device"], row["inode"])
        if alias in aliases:
            raise EvidenceError("pair library choices must not alias")
        aliases.add(alias)
        libraries[name] = row
    control.mkdir()
    copies = control / "before-libraries"
    copies.mkdir()
    for name, row in libraries.items():
        destination = copies / name
        u575_pair._copy_retained(Path(row["path"]), destination,
            {"size": row["bytes"], "sha256": row["sha256"],
             "mode": row["mode"]})
        if _capture_file(row["path"]) != row:
            raise EvidenceError("original pair library changed before build")
        row["copy"] = _capture_file(destination)
    value = {"source": json.loads(json.dumps(source)), "compiler": compiler,
             "tools": tool_before, "libraries": libraries,
             "observed_ns": time.time_ns()}
    atomic_json(control / "libraries-before.json", value)
    return value


def finish_pair_context(output, report, before):
    """Publish only the exact typed closure after the unchanged build gate."""
    from tools.evidence import u575_pair
    control = Path(output) / "pair-context"
    targets = [control / leaf for leaf in
               ("libraries-after.json", "history.json", "context.json")]
    if any(p.exists() or p.is_symlink() for p in targets):
        raise EvidenceError("pair context outputs must be absent")
    selected = u575_pair.collect_report(report)
    if report["source"] != before["source"]:
        raise EvidenceError("original pair source changed")
    for name, snapshot in before["tools"].items():
        if _capture_file(snapshot["path"]) != snapshot:
            raise EvidenceError("original pair tool changed")
        reported = u575_pair._identity(report["tools"][name])
        if any(reported[key] != snapshot["bytes" if key == "size" else key]
               for key in ("path", "size", "sha256")):
            raise EvidenceError("original pair tool report changed")
    after = {}
    for name, row in before["libraries"].items():
        original = {key: value for key, value in row.items() if key != "copy"}
        if (_capture_file(row["path"]) != original or
                _capture_file(row["copy"]["path"]) != row["copy"]):
            raise EvidenceError("original or retained pair library changed")
        after[name] = original
    known_libraries = {row["path"]: row for row in after.values()}
    emitted, not_emitted = {}, {}
    for stage in report["build_inputs"]:
        name = stage["stage"]
        database = regular_file(stage["compile_commands"]["path"])
        directory = database.parent
        for leaf in u575_pair.BUNDLE:
            path = directory / "nexus-runtime/generated" / leaf
            selected[str(path)] = {"path": str(path)}
        if database.stat().st_size > u575_pair.MAX_METADATA:
            raise EvidenceError("actual compile database exceeds bound")
        declared = u575_pair.compile_outputs(
            json.loads(_capture_metadata(database)), name,
            report["tools"]["gcc"]["path"])
        emitted[name], not_emitted[name] = [], []
        for label in declared:
            path = Path(label)
            if path.is_symlink():
                raise EvidenceError("compiler output cannot be a symlink")
            if path.exists():
                selected[label] = {"path": label}
                emitted[name].append(label)
            else:
                not_emitted[name].append(label)
        api = {row["path"]: row for row in stage.get("cmake_file_api", [])}
        indexes = [label for label in api if Path(label).name.startswith(
            "index-")]
        if len(indexes) != 1:
            raise EvidenceError("one exact stage File API index required")
        def read_api(label):
            if label not in api:
                raise EvidenceError("File API reply missing from typed inputs")
            path = regular_file(label)
            if path.stat().st_size > u575_pair.MAX_METADATA:
                raise EvidenceError("File API metadata exceeds bound")
            return _capture_metadata(path)
        graph = u575_pair.cmake_link_graph(indexes[0], str(directory), name,
            report["artifacts"][name]["path"], read_api)
        if set(graph["inputs"]) != set(api):
            raise EvidenceError(
                "unrelated File API replies are not capture inputs")
        local = set(emitted[name]) | set(graph["libraries"])
        for label in graph["libraries"]:
            regular_file(label)
        if name == "nonsecure":
            local.add(report["artifacts"]["secure_import"]["path"])
        map_path = regular_file(report["artifacts"][name + "_map"]["path"])
        if map_path.stat().st_size > u575_pair.MAX_METADATA:
            raise EvidenceError("actual map exceeds bound")
        links = u575_pair.map_inputs(_capture_metadata(map_path), str(directory))
        for label in links:
            path = Path(label)
            if label in known_libraries:
                expected = known_libraries[label]
                prior = selected.get(label)
                if prior and "sha256" in prior and (
                        prior["sha256"] != expected["sha256"] or
                        prior.get("size", prior.get("bytes")) !=
                        expected["bytes"]):
                    raise EvidenceError("original pair library report changed")
                selected[label] = expected
            elif path.is_relative_to(directory):
                if label not in local:
                    raise EvidenceError("unexpected local pair map LOAD")
                selected.setdefault(label, {"path": label})
            elif name == "nonsecure" and label in local:
                selected.setdefault(label, {"path": label})
            else:
                raise EvidenceError("unknown external pair map LOAD")
    if len(selected) > u575_pair.MAX_ENTRIES:
        raise EvidenceError("pair capture input count exceeds bound")
    files = [_capture_file(path) for path in sorted(selected)]
    if sum(row["bytes"] for row in files) > u575_pair.MAX_BYTES:
        raise EvidenceError("pair capture closure exceeds bound")
    # The original report is still authoritative; history cannot replace it.
    for row in files:
        prior = selected[row["path"]]
        if (row["path"] in known_libraries and
                row != known_libraries[row["path"]]):
            raise EvidenceError("original pair library changed during capture")
        if "sha256" in prior and (prior["sha256"] != row["sha256"] or
                prior.get("size", prior.get("bytes")) != row["bytes"]):
            raise EvidenceError("original pair report input changed")
    atomic_json(targets[0], {"libraries": after,
                            "observed_ns": time.time_ns()})
    history = {"files": files, "libraries_before": before["libraries"],
               "libraries_after": after, "emitted": emitted,
               "not_emitted": not_emitted}
    atomic_json(targets[1], history)
    context = {"schema_version": 1, "kind": "u575_pair_build_context",
               "source": report["source"], "tools": report["tools"],
               "report": file_identity(Path(output) / "report.json"),
               "history": [file_identity(targets[1])]}
    def read_metadata(label):
        path = regular_file(label, nonempty=False)
        if path.stat().st_size > u575_pair.MAX_METADATA:
            raise EvidenceError("pair metadata exceeds bound")
        return _capture_metadata(path)
    u575_pair.build_pair(context, read_metadata)
    atomic_json(targets[2], context)
    return targets[2]


def capture_cmake_inputs(build, stage):
    """Retain replies for only the actual executable's dependency closure."""
    from tools.evidence import u575_pair
    indexes = list((build / ".cmake/api/v1/reply").glob("index-*.json"))
    if len(indexes) != 1:
        raise EvidenceError("one fresh CMake File API index required")
    records = {}
    def read(label):
        row = _capture_file(label)
        if row["bytes"] > u575_pair.MAX_METADATA:
            raise EvidenceError("File API reply exceeds metadata bound")
        records[label] = {key: row[key] for key in ("path", "bytes", "sha256")}
        raw = _capture_metadata(label)
        if hashlib.sha256(raw).hexdigest() != row["sha256"]:
            raise EvidenceError("File API reply changed")
        return raw
    graph = u575_pair.cmake_link_graph(str(indexes[0]), str(build), stage,
        str(build / ("u575_" + stage + ".elf")), read)
    return [records[label] for label in graph["inputs"]]


def run(root, output, compiler="arm-none-eabi-gcc", parallel=2,
        capture_pair_context=False):
    root = root.resolve()
    output = provider_assemblies.fresh_output(output, root)
    report = {"schema_version": 1, "kind": "u575_trustzone_software_pair",
              "status": "failed", "physical_status": "not_executed",
              "candidate_sealing": "not_established_for_multi_image_pair"}
    recorder = provider_assemblies.Recorder(output / "commands", root)
    report["commands"] = recorder.commands
    try:
        if root != ROOT.resolve():
            raise EvidenceError("selected root differs from actual gate source")
        if any(name.startswith("GIT_") for name in os.environ):
            raise EvidenceError("Git environment overrides cannot identify source")
        if type(parallel) is not int or not 1 <= parallel <= 4:
            raise EvidenceError("bounded build parallelism required")
        source = git_source(root)
        if capture_pair_context and source["dirty"]:
            raise EvidenceError("pair capture requires a clean build source")
        fixture = root / "tests/contracts/u575_trustzone"
        inputs = source_inputs(root)
        report["source"], report["source_inputs"] = source, inputs
        tools = arch_compile.tools(compiler)
        for name in ("cmake", "ninja"):
            tools[name] = shutil.which(name)
            if not tools[name]:
                raise EvidenceError("missing actual build tool: " + name)
        tools["python"] = sys.executable
        report["tools"] = {name: arch_compile.digest(Path(path).resolve())
                           for name, path in tools.items()}
        recorder.env["PATH"] = str(Path(tools["gcc"]).parent) + os.pathsep + recorder.env["PATH"]
        for name, tool in tools.items():
            recorder.execute([tool, "--version"], name + "-version")
        before_context = (begin_pair_context(output, tools, recorder, source)
                          if capture_pair_context else None)
        report["build_inputs"] = []
        for stage in ("secure", "nonsecure"):
            build = output / stage
            if capture_pair_context:
                query = build / ".cmake/api/v1/query/codemodel-v2"
                query.parent.mkdir(parents=True)
                query.write_bytes(b"")
            argv = [tools["cmake"], "-S", str(fixture), "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                    "-DCMAKE_C_COMPILER=" + tools["gcc"],
                    "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                    "-DPython3_EXECUTABLE=" + tools["python"],
                    "-DNEXUS_TRUSTZONE_STAGE=" + stage]
            if stage == "nonsecure":
                argv.append("-DNEXUS_SECURE_IMPORT_LIBRARY=" + str(
                    output / "secure/secure-import.o"))
            recorder.execute(argv, stage + "-configure")
            recorder.execute([tools["cmake"], "--build", str(build), "--parallel",
                              str(parallel)], stage + "-build")
            dependencies = recorder.execute([tools["ninja"], "-C", str(build),
                "-t", "deps"], stage + "-dependencies")
            stage_inputs = {"stage": stage,
                "compile_commands": compile_profile(build, stage, tools["gcc"]),
                "compiler_dependencies": [arch_compile.digest(path) for path in
                    provider_assemblies.dependency_paths(dependencies, build)]}
            if capture_pair_context:
                stage_inputs["cmake_file_api"] = capture_cmake_inputs(
                    build, stage)
            report["build_inputs"].append(stage_inputs)
        secure_path = output / "secure/u575_secure.elf"
        nonsecure_path = output / "nonsecure/u575_nonsecure.elf"
        import_path = output / "secure/secure-import.o"
        raw = recorder.execute([tools["nm"], "--defined-only", "-P",
                                str(import_path)], "import-library")
        report["checks"] = validate_pair(
            secure_path.read_bytes(), nonsecure_path.read_bytes(),
            imported_gateways(raw))
        secure = Elf32(secure_path.read_bytes(), float_abi="soft")
        nonsecure = Elf32(nonsecure_path.read_bytes(), float_abi="soft")
        cross_call = recorder.execute([tools["objdump"], "-d", "-z",
            "--disassemble=__gnu_cmse_nonsecure_call", str(secure_path)],
            "actual-cmse-call")
        decoded = decode_function(secure, "__gnu_cmse_nonsecure_call",
                                  cross_call.encode("ascii"))
        if not any(item["mnemonic"] == "blxns"
                   for item in decoded["instructions"]):
            raise EvidenceError("real CMSE NonSecure call instruction missing")
        report["cross_call"] = decoded
        forbidden = {"msr", "cpsid", "cpsie", "svc", "mcr", "vmsr"}
        inspections = []
        for symbol in nonsecure.defined_symbols:
            if symbol["type"] != 2 or symbol["size"] == 0:
                continue
            # GNU CMSE import symbols are absolute gateways, not local NS code.
            # validate_pair binds both imports to their actual Secure SG bytes.
            if (symbol["name"] in {"nx_reference_gpio", "nx_reference_gpio_batch"}
                    and symbol["section"] == 0xfff1):
                continue
            name = symbol["name"]
            raw = recorder.execute([tools["objdump"], "-d", "-z",
                                    "--disassemble=" + name, str(nonsecure_path)],
                                   "ns-function-" + str(len(inspections)))
            decoded = decode_function(nonsecure, name, raw.encode("ascii"))
            if any(item["mnemonic"].split(".")[0] in forbidden
                   for item in decoded["instructions"]):
                raise EvidenceError("NS image contains privileged exception control")
            inspections.append(decoded)
        if not inspections:
            raise EvidenceError("NonSecure code inspection cannot be empty")
        report["nonsecure_functions"] = inspections
        report["artifacts"] = {"secure": file_identity(secure_path),
            "nonsecure": file_identity(nonsecure_path),
            "secure_import": file_identity(import_path),
            "secure_map": file_identity(output / "secure/u575_secure.map"),
            "nonsecure_map": file_identity(output / "nonsecure/u575_nonsecure.map")}
        if source_inputs(root) != inputs:
            raise EvidenceError("maintained graph input snapshot changed")
        for stage in report["build_inputs"]:
            provider_assemblies.verify_inputs(stage["compiler_dependencies"])
            provider_assemblies.verify_inputs([stage["compile_commands"]])
        provider_assemblies.verify_inputs(list(report["tools"].values()))
        if git_source(root) != source:
            raise EvidenceError("source identity changed during paired build")
        report["status"] = "passed"
        if capture_pair_context:
            atomic_json(output / "report.json", report)
            finish_pair_context(output, report, before_context)
    except (OSError, ValueError, KeyError, TypeError) as error:
        report["status"] = "failed"
        report["failure"] = str(error)
    # A published context anchors immutable original report bytes.
    if not (capture_pair_context and report["status"] == "passed"):
        atomic_json(output / "report.json", report)
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--parallel", type=int, default=2)
    parser.add_argument("--capture-pair-context", action="store_true")
    args = parser.parse_args()
    try:
        return run(args.root, args.output, args.compiler, args.parallel,
                   args.capture_pair_context)
    except (OSError, ValueError) as error:
        print("U575 pair rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
