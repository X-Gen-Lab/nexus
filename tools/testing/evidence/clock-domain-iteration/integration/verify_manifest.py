"""Verify the recorded IO58 integration sidecar without rebuilding firmware."""

from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    evidence = Path(__file__).resolve().parent
    repository = evidence.parents[4]
    manifest_file = evidence / "manifest.json"
    manifest = json.loads(manifest_file.read_text())
    records = (
        manifest["source"]["files"]
        + manifest["fixture"]["files"]
        + manifest["raw_execution_files"]
        + [manifest["historical_manifest"]["file"]]
    )
    for bundle in manifest["artifacts"]:
        records += bundle["files"] + [bundle["source_boundary"]]
    for record in records:
        path = Path(record["path"])
        if not path.is_absolute():
            path = repository / path
        assert path.is_file(), str(path)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == record["sha256"], str(path)
        assert path.stat().st_size == record["size"], str(path)
    tools = json.loads((evidence / "tool-identities.json").read_text())
    for tool in tools:
        record = tool["executable"]
        path = Path(record["path"])
        if not path.is_absolute():
            path = repository / path
        assert hashlib.sha256(path.read_bytes()).hexdigest() == record["sha256"]
    ignore_command = [
        "git", "check-ignore",
        "tools/testing/evidence/clock-domain-iteration/integration/stm32-selected-final2/sleep_consumer.elf",
        "tools/testing/evidence/clock-domain-iteration/integration/stm32-selected-final2/sleep_consumer.bin",
    ]
    ignored = subprocess.run(
        ignore_command, cwd=repository, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    assert ignored.returncode == 0
    result = {
        "status": "passed",
        "command": [sys.executable, str(Path(__file__).resolve())],
        "verifier_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "manifest_sha256": hashlib.sha256(manifest_file.read_bytes()).hexdigest(),
        "referenced_files_verified": len(records),
        "tool_executables_verified": len(tools),
        "binary_ignore_check": {
            "command": ignore_command,
            "exit_code": ignored.returncode,
            "output": ignored.stdout,
        },
        "physical_hil": "not_executed",
    }
    (evidence / "manifest-integrity.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
