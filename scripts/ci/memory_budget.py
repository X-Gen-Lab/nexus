"""Shared authority and actual-overflow probes for optional-memory fixtures."""
from __future__ import annotations

import json
import os
from pathlib import Path

try:
    from . import arch_compile
except ImportError:
    import arch_compile

from tools.evidence.common import file_identity, fields, sha256_value


def rejected(recorder, argv, label, diagnostic):
    """Retain genuine nonzero output and reject a failure for the wrong reason."""
    try:
        recorder.execute(argv, label)
    except ValueError:
        record = recorder.commands[-1]
        if record.get("returncode") != 1:
            raise
        output = "\n".join(Path(record[key]["path"]).read_text()
                           for key in ("stdout", "stderr"))
        if diagnostic not in output:
            raise ValueError("Negative execution failed for a different reason")
        record["expected_rejection"] = diagnostic
        return record
    raise ValueError("Negative execution unexpectedly accepted unsupported storage")


def check_budget_tamper(report):
    """A changed generated JSON is rejected before any measurement claim."""
    if (type(report.get("schema_version")) is not int or
            report.get("schema_version") != 1 or
            report.get("kind") != "elf_resource_budget" or
            report.get("status") != "failed" or
            report.get("failure") !=
            "resource budget differs from the resolved memory contract" or
            "metrics" in report or "violations" in report):
        raise ValueError("Wrong canonical resource budget tamper rejection")


def check_budget_overflow(report, region):
    """Require the exact live 36-byte fixture against its authored 32-byte cap."""
    try:
        actual = report["metrics"]["ram"][region]["reserved_bytes"]
    except (KeyError, TypeError) as error:
        raise ValueError("Missing actual authored RAM budget measurement") from error
    if (type(report.get("schema_version")) is not int or
            report.get("schema_version") != 1 or
            report.get("kind") != "elf_resource_budget" or
            report.get("status") != "failed" or "failure" in report or
            type(actual) is not int or actual != 36 or report.get("violations") != [
                {"metric": "ram." + region, "actual": 36, "maximum": 32}]):
        raise ValueError("Wrong actual authored per-domain budget overflow")


def withdrawn_elf(elf, identity):
    """Keep the measured image identity after the firmware target withdraws it."""
    if any(os.path.lexists(elf.with_suffix(ext)) for ext in (".elf", ".bin", ".map")):
        raise ValueError("Rejected firmware outputs remain published")
    fields(identity, {"path", "size", "sha256"})
    if (identity["path"] != str(elf) or type(identity["size"]) is not int
            or identity["size"] <= 0):
        raise ValueError("Wrong measured firmware identity")
    return {"path": str(elf), "bytes": identity["size"],
            "sha256": sha256_value(identity["sha256"])}


def write_budget_assembly(template, output, region):
    """Change the reviewed authored limit, never the derived JSON authority."""
    template, output = Path(template), Path(output)
    if output.exists() or output.is_symlink():
        raise ValueError("Authored budget output must be unused")
    text = template.read_text()
    selected = "[memory.regions." + region + "]\nlimit_bytes = 128"
    if region not in {"ccm", "addsram", "tcm"} or text.count(selected) != 1:
        raise ValueError("Reviewed optional-memory authored budget differs")
    output.write_text(text.replace(selected, selected.replace("128", "32")))


def execute_budget_overflow(root, recorder, tools, fixture, template, output,
                            region, target, parallel=2):
    """Resolve and link a fresh authored overrun through production CMake."""
    assembly = output / (region + "-authored-budget.toml")
    write_budget_assembly(template, assembly, region)
    build = output / (region + "-authored-overflow")
    recorder.execute([tools["cmake"], "-S", str(fixture), "-B", str(build),
        "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
        "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
        "-DPython3_EXECUTABLE=" + tools["python"],
        "-DNEXUS_ASSEMBLY_FILE=" + str(assembly)],
        region + "-authored-budget-configure")
    command = rejected(recorder, [tools["cmake"], "--build", str(build),
        "--target", target, "--parallel", str(parallel)],
        region + "-authored-budget-build", "ELF budget: failed")
    elf = build / "bin" / (target + ".elf")
    report_path = elf.with_suffix(".resources.json")
    generated = build / "nexus/generated"
    resolved_path = generated / "resolved.json"
    budget_path = generated / "resource-budget.json"
    resolved = json.loads(resolved_path.read_text())
    report = json.loads(report_path.read_text())
    if (resolved["memory_budgets"]["regions"][region]["limit_bytes"] != 32 or
            report.get("resolved") != file_identity(resolved_path) or
            report.get("budget") != file_identity(budget_path)):
        raise ValueError("Measured overrun differs from the authored budget inputs")
    check_budget_overflow(report, region)
    return {"kind": "authored_budget_overflow", "region": region,
        "command": command, "assembly": arch_compile.digest(assembly),
        "elf": withdrawn_elf(elf, report.get("elf")), "elf_status": "withdrawn",
        "report": arch_compile.digest(report_path),
        "resolved": arch_compile.digest(resolved_path),
        "budget": arch_compile.digest(budget_path)}
