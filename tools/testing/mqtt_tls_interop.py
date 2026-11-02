"""Real host-broker integration; no physical or lwIP qualification is inferred."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import sys
import time
import uuid

from tools.dev import process

DOCKER = ["docker", "--host=unix:///var/run/docker.sock"]
IMAGE = (
    "eclipse-mosquitto@sha256:"
    "09b62ff32c2e7b64d4b431b9a89236073289b68db273de2b089edeb9b0d92b5c"
)
BROKER_CONFIG = """listener 8883 0.0.0.0
allow_anonymous true
persistence false
cafile /fixtures/ca.pem
certfile /fixtures/server.pem
keyfile /fixtures/server.key.pem
require_certificate true
use_identity_as_username true
tls_version tlsv1.2
ciphers ECDHE-ECDSA-AES128-GCM-SHA256
log_dest stdout
log_type all
"""


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    args.client = args.client.resolve(strict=True)
    args.output_root = args.output_root.absolute()
    if not args.client.is_file() or not os.access(args.client, os.X_OK):
        parser.error("--client must be an existing executable")
    if args.output_root.exists() or not args.output_root.parent.is_dir():
        parser.error("--output-root must be new with an existing parent")
    return args


def main():
    args = arguments()
    base = args.output_root
    base.mkdir(mode=0o700)
    credentials = base / "credentials"
    credentials.mkdir(mode=0o700)
    broker = base / "broker"
    broker.mkdir(mode=0o700)
    evidence = base / "evidence"
    evidence.mkdir(mode=0o700)
    env = os.environ.copy()
    # Select only the managed local daemon. Preserve proxy/CA/registry identity.
    for key in ("DOCKER_HOST", "DOCKER_CONTEXT", "DOCKER_TLS",
                "DOCKER_TLS_VERIFY", "DOCKER_CERT_PATH"):
        env.pop(key, None)
    owner = uuid.uuid4().hex
    name = "nexus-mqtt-tls-e2e-" + owner
    record = {
        "owner": owner, "container_name": name, "image_selector": IMAGE,
        "physical_status": "not_executed", "qualification": False,
        "scope": "Native POSIX socket only; no lwIP/MCU path",
        "receipts": [], "cases": [], "cleanup": [],
        "client_binary_sha256": hashlib.sha256(args.client.read_bytes()).hexdigest(),
    }
    (evidence / "owner.json").write_text(json.dumps(record, indent=2) + "\n")
    cid = None
    sequence = 0
    result = 1
    primary = None

    def run(label, command, *, required=True, timeout=30):
        nonlocal sequence
        sequence += 1
        receipt = process.run(
            command, cwd=base, env=env,
            log=evidence / f"{sequence:02d}-{label}.log",
            timeout=timeout, stage="mqtt-tls-e2e-" + label,
        )
        record["receipts"].append(str(receipt.receipt_path))
        if required and receipt.returncode:
            raise RuntimeError(f"{label} failed {receipt.returncode}; "
                               f"receipt={receipt.receipt_path}")
        return receipt

    def stop_signal(number, frame):
        del frame
        raise SystemExit(128 + number)

    previous_handlers = {
        number: signal.signal(number, stop_signal)
        for number in (signal.SIGTERM, signal.SIGINT)
    }
    try:
        version = run("openssl-version", ["openssl", "version", "-a"])
        openssl_path = shutil.which("openssl")
        if openssl_path is None:
            raise RuntimeError("OpenSSL host fixture tool unavailable")
        record["openssl"] = {
            "version": version.stdout,
            "binary_sha256": hashlib.sha256(Path(openssl_path).read_bytes()).hexdigest(),
        }
        # Ephemeral test-only P256 identities avoid dated checked-in credentials.
        def credential(label, command):
            return run(label, ["openssl", *command])

        credential("test-ca", [
            "req", "-new", "-x509", "-newkey", "ec", "-pkeyopt",
            "ec_paramgen_curve:P-256", "-nodes", "-days", "2",
            "-subj", "/CN=Nexus Host Interop CA",
            "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign",
            "-keyout", str(credentials / "ca.key.pem"),
            "-out", str(credentials / "ca.pem"),
        ])
        for kind, usage, serial in (("server", "serverAuth", "2"),
                                    ("client", "clientAuth", "3")):
            credential("test-" + kind + "-csr", [
                "req", "-new", "-newkey", "ec", "-pkeyopt",
                "ec_paramgen_curve:P-256", "-nodes",
                "-subj", f"/CN={kind}.test",
                "-addext", f"subjectAltName=DNS:{kind}.test",
                "-addext", "basicConstraints=critical,CA:FALSE",
                "-addext", "keyUsage=critical,digitalSignature",
                "-addext", "extendedKeyUsage=" + usage,
                "-keyout", str(credentials / f"{kind}.key.pem"),
                "-out", str(credentials / f"{kind}.csr.pem"),
            ])
            credential("test-" + kind + "-cert", [
                "x509", "-req", "-in", str(credentials / f"{kind}.csr.pem"),
                "-CA", str(credentials / "ca.pem"),
                "-CAkey", str(credentials / "ca.key.pem"),
                "-set_serial", serial, "-days", "2", "-copy_extensions", "copy",
                "-out", str(credentials / f"{kind}.pem"),
            ])
        for kind in ("ca", "client"):
            credential("test-" + kind + "-der", [
                "x509", "-in", str(credentials / f"{kind}.pem"),
                "-outform", "DER", "-out", str(credentials / f"{kind}.der"),
            ])
        credential("test-client-key-der", [
            "pkey", "-in", str(credentials / "client.key.pem"), "-outform", "DER",
            "-out", str(credentials / "client.key.der"),
        ])
        for item in credentials.iterdir():
            item.chmod(0o600)
        record["test_credential_hashes"] = {
            item.name: hashlib.sha256(item.read_bytes()).hexdigest()
            for item in sorted(credentials.iterdir())
        }
        # Only the broker's own test identity/trust enters its read-only mount.
        for filename in ("ca.pem", "server.pem", "server.key.pem"):
            shutil.copyfile(credentials / filename, broker / filename)
            (broker / filename).chmod(0o600)
        (broker / "mosquitto.conf").write_text(BROKER_CONFIG)
        (broker / "mosquitto.conf").chmod(0o600)
        identity = run("image-identity", DOCKER + [
            "image", "inspect", IMAGE, "--format",
            '{{json .RepoDigests}}|{{.Id}}|{{.Architecture}}|{{.Os}}',
        ])
        record["image_identity"] = identity.stdout.strip()
        created = run("create", DOCKER + [
            "create", "--name", name, "--label", "nexus.e2e.owner=" + owner,
            "--user", f"{os.getuid()}:{os.getgid()}",
            "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
            "--read-only", "--tmpfs", "/tmp:rw,noexec,nosuid,size=1m",
            "--memory", "64m", "--cpus", "1", "--pids-limit", "32",
            "--network", "bridge", "--publish", "127.0.0.1::8883",
            "--mount", f"type=bind,src={broker},dst=/fixtures,readonly",
            IMAGE, "mosquitto", "-c", "/fixtures/mosquitto.conf", "-v",
        ])
        created_id = created.stdout.strip()
        if re.fullmatch(r"[0-9a-f]{64}", created_id) is None:
            raise RuntimeError("Invalid created owned container identity")
        cid = created_id
        record["container_id"] = cid
        (evidence / "owner.json").write_text(json.dumps(record, indent=2) + "\n")
        run("start", DOCKER + ["start", cid])
        port = run("published-port", DOCKER + [
            "inspect", cid, "--format",
            '{{json (index .NetworkSettings.Ports "8883/tcp")}}',
        ])
        mapping = json.loads(port.stdout)
        if len(mapping) != 1 or mapping[0]["HostIp"] != "127.0.0.1":
            raise RuntimeError("Broker publication is not loopback-only")
        record["port_mapping"] = mapping
        deadline = time.monotonic() + 10
        while True:
            logs = run("ready-log", DOCKER + ["logs", cid])
            if "mosquitto version 2.0.22 running" in logs.stdout:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("Owned broker did not become ready")
            time.sleep(0.1)
        version = run("broker-version", DOCKER + ["exec", cid, "mosquitto", "-h"])
        record["broker_version"] = version.stdout.splitlines()[:2]
        binary = run("broker-binary-sha", DOCKER + [
            "exec", cid, "sha256sum", "/usr/sbin/mosquitto",
        ])
        record["broker_binary_sha256"] = binary.stdout.strip()
        for mode in ("clean", "fault", "missing-client"):
            case = run(mode, [str(args.client), mapping[0]["HostPort"], mode,
                              str(credentials)], required=False, timeout=20)
            record["cases"].append({
                "name": mode, "exit_code": case.returncode,
                "receipt": str(case.receipt_path), "stdout": case.stdout,
            })
            print(mode, case.returncode, case.stdout, flush=True)
            if case.returncode:
                raise RuntimeError(f"{mode} integration case failed")
        result = 0
    except BaseException:
        primary = sys.exc_info()
        record["primary_error"] = {
            "type": primary[0].__name__, "message": str(primary[1]),
        }
    finally:
        # Docker client withdrawal does not retract daemon-owned resources.
        # Defer repeated controlled cancellation until bounded withdrawal ends.
        def cleanup_signal(number, frame):
            nonlocal result
            del frame
            record.setdefault("cleanup_signals", []).append(number)
            result = 1

        for number in previous_handlers:
            signal.signal(number, cleanup_signal)
        try:
            selector = cid or name
            owned = run("cleanup-owner", DOCKER + [
                "inspect", selector, "--format",
                '{"id":{{json .Id}},"name":{{json .Name}},'
                '"owner":{{json (index .Config.Labels "nexus.e2e.owner")}}}',
            ], required=False)
            snapshot = json.loads(owned.stdout) if owned.returncode == 0 else {}
            locked_id = snapshot.get("id", "")
            valid = (
                owned.returncode == 0
                and re.fullmatch(r"[0-9a-f]{64}", locked_id) is not None
                and snapshot.get("name") == "/" + name
                and snapshot.get("owner") == owner
                and (cid is None or cid == locked_id)
            )
            if not valid:
                record["cleanup"].append({"ownership_unconfirmed": True})
                result = 1
            else:
                # Names can be rebound after inspection; only full IDs are used.
                selector = locked_id
                record["cleanup_container_id"] = selector
                logs = run("broker-final-log", DOCKER + ["logs", selector],
                           required=False)
                record["broker_log_receipt"] = str(logs.receipt_path)
                if logs.returncode:
                    result = 1
                stopped = run("stop", DOCKER + ["stop", "--time", "3", selector],
                              required=False)
                removed = run("remove", DOCKER + ["rm", "--force", selector],
                              required=False)
                absent = run("confirm-absent", DOCKER + [
                    "inspect", selector, "--format", '{{.Id}}',
                ], required=False)
                record["cleanup"].append({
                    "stop_exit": stopped.returncode,
                    "remove_exit": removed.returncode,
                    "absent_exit": absent.returncode,
                })
                if stopped.returncode or removed.returncode or not absent.returncode:
                    result = 1
        except BaseException as cleanup_error:
            record["cleanup_error"] = {
                "type": type(cleanup_error).__name__, "message": str(cleanup_error),
            }
            result = 1
        record["returncode"] = result
        (evidence / "result.json").write_text(json.dumps(record, indent=2) + "\n")
        print("evidence", evidence, "result", result, flush=True)
        for number, handler in previous_handlers.items():
            signal.signal(number, handler)
    if primary is not None:
        raise primary[1].with_traceback(primary[2])
    return result


if __name__ == "__main__":
    sys.exit(main())
