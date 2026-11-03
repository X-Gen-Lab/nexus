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
import stat
import struct
import sys
import time
import uuid

from tools.dev import process
from tools.evidence.common import parse_json

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


def case_names(transport):
    if transport in {"posix", "external-client"}:
        return ("clean", "fault", "missing-client")
    if transport == "lwip-test-relay":
        return ("clean", "backpressure", "fault", "deadline", "missing-client")
    raise ValueError("Unknown broker integration transport")


def transport_scope(transport):
    """Distinguish fixture orchestration from the selected client's evidence."""
    if transport == "posix":
        return "Native POSIX socket only; no lwIP/MCU path"
    if transport == "lwip-test-relay":
        return ("Native full client lwIP/TCP/TLS/MQTT; two TCP hops via test-only "
                "ciphertext relay, no TAP/PHY/MCU qualification")
    if transport == "external-client":
        return ("Externally owned host client; broker/CA and process execution "
                "only. Actual client transport, TLS and protocol behavior not "
                "assessed; no MCU/PHY qualification. Client exit zero alone "
                "does not establish an authenticated connection or path.")
    raise ValueError("Unknown broker integration transport")


def chain_report(mode, stdout):
    """Admit explicit host-chain counters, never a process exit alone."""
    if mode not in case_names("lwip-test-relay"):
        raise ValueError("Unknown full-chain case")
    metrics = {}
    fields = {"result", "client_loans_clear", "loans_clear", "relay_joined",
              "two_tcp_connections", "phy_qualified", "data_frames", "rx_frames",
              "provider_service_calls", "relay_sent", "relay_received",
              "relay_rx_frames", "relay_data_frames", "mqtt_events", "byte_budget",
              "replay_provider_calls", "deadline", "memory_busy", "max_chain",
              "paused_rounds"}
    for key, value in re.findall(r"\b([a-z][a-z0-9_]*)=([0-9]+)\b", stdout):
        if key not in fields:
            continue
        if key in metrics:
            raise ValueError(f"Duplicate chain metric: {key}")
        metrics[key] = int(value)
    required = {"result": 0, "client_loans_clear": 1, "loans_clear": 1,
                "relay_joined": 1, "two_tcp_connections": 1, "phy_qualified": 0}
    if any(metrics.get(key) != value for key, value in required.items()):
        raise ValueError("Incomplete full-chain withdrawal or scope")
    for key in ("data_frames", "rx_frames", "provider_service_calls",
                "relay_sent", "relay_received", "relay_rx_frames",
                "relay_data_frames"):
        if metrics.get(key, 0) <= 0:
            raise ValueError(f"No actual stack progress: {key}")
    if mode == "missing-client":
        if "missing_client_peer_fatal_alert=" not in stdout:
            raise ValueError("Missing actual broker authentication refusal")
    elif "authenticated_tls=TLSv1.2 " not in stdout:
        raise ValueError("Missing authenticated TLS session")
    if mode in ("clean", "backpressure"):
        if metrics.get("mqtt_events") != 15 or "mqtt_clean_close=locally_accepted" not in stdout:
            raise ValueError("Incomplete MQTT QoS1/ACK/close flow")
    if mode == "backpressure" and (
        metrics.get("byte_budget") != 17 or metrics.get("paused_rounds", 0) <= 0
    ):
        raise ValueError("Missing bounded partial relay mode")
    if mode in ("fault", "deadline"):
        if metrics.get("replay_provider_calls") != 0 or metrics.get("deadline") != int(mode == "deadline"):
            raise ValueError("Missing sticky failure/no-replay result")
    return metrics


PCAP_MAX_BYTES = 1024 * 1024
PCAP_MAX_FRAMES = 1024
PCAP_SNAPLEN = 1514


def captured_bytes(path, maximum):
    """Read one bounded regular file through a no-follow directory chain."""
    path = Path(path).absolute()
    if ".." in path.parts:
        raise ValueError("Capture path is not canonical")
    directory = os.open("/", os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    descriptor = None
    try:
        for part in path.parts[1:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW |
                            os.O_CLOEXEC, dir_fd=directory)
            os.close(directory)
            directory = child
        descriptor = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW |
                             os.O_CLOEXEC, dir_fd=directory)
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= maximum:
            raise ValueError("Capture file is empty, nonregular or exceeds its bound")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            data = stream.read(maximum + 1)
        after = os.fstat(descriptor)
        if (len(data) != before.st_size or
                (before.st_size, before.st_mtime_ns, before.st_ctime_ns) !=
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
            raise ValueError("Capture file changed during observation")
        return data, {"path": str(path), "size": len(data),
                      "sha256": hashlib.sha256(data).hexdigest()}
    except OSError as error:
        raise ValueError("Capture path is missing or aliases another path") from error
    finally:
        if descriptor is not None:
            os.close(descriptor)
        os.close(directory)


def read_pcap(path, direction, *, complete_tls):
    """Validate bounded raw frames and TLS framing; never decrypt TLS traffic."""
    if direction not in {"client", "relay"}:
        raise ValueError("Unknown capture direction")
    data, identity = captured_bytes(path, PCAP_MAX_BYTES)
    if len(data) < 24 or struct.unpack_from("<IHHIIII", data) != (
            0xa1b2c3d4, 2, 4, 0, 0, PCAP_SNAPLEN, 1):
        raise ValueError("Capture is not the complete reviewed Ethernet PCAP format")
    offset, stamp = 24, 0
    frames = arp = tcp_count = payload_count = resets = fins = 0
    initial = None
    ports = None
    segments = []
    source, peer = ((1, 2) if direction == "client" else (2, 1))
    while offset < len(data):
        if offset + 16 > len(data) or frames == PCAP_MAX_FRAMES:
            raise ValueError("Truncated capture record or frame bound exceeded")
        seconds, micros, included, original = struct.unpack_from("<IIII", data, offset)
        elapsed = seconds * 1000000 + micros
        if (micros >= 1000000 or not stamp <= elapsed <= 20000000 or
                included != original or not 14 <= included <= PCAP_SNAPLEN or
                offset + 16 + included > len(data)):
            raise ValueError("Incomplete, truncated or unbounded captured frame")
        stamp = elapsed
        frame = data[offset + 16:offset + 16 + included]
        offset += 16 + included
        frames += 1
        if frame[6:12] != bytes((2, 0, 0, 0, 0, source)):
            raise ValueError("Capture direction differs from its endpoint")
        protocol = int.from_bytes(frame[12:14], "big")
        if protocol == 0x0806:
            if len(frame) < 42:
                raise ValueError("Truncated captured ARP frame")
            arp += 1
            continue
        if (protocol != 0x0800 or len(frame) < 54 or frame[14] >> 4 != 4 or
                frame[23] != 6 or frame[26:30] != bytes((10, 0, 0, source)) or
                frame[30:34] != bytes((10, 0, 0, peer))):
            raise ValueError("Capture contains an unreviewed Ethernet/IP protocol")
        ihl = (frame[14] & 15) * 4
        size = int.from_bytes(frame[16:18], "big")
        start = 14 + ihl
        if (ihl < 20 or size < ihl + 20 or 14 + size > len(frame) or
                int.from_bytes(frame[20:22], "big") & 0x3fff):
            raise ValueError("Captured IPv4 bounds or fragmentation differ")
        sport, dport, sequence = struct.unpack_from("!HHI", frame, start)
        if (dport if source == 1 else sport) != 8883 or (
                ports is not None and ports != (sport, dport)):
            raise ValueError("Capture did not use one reviewed relay connection")
        ports = (sport, dport)
        thl = (frame[start + 12] >> 4) * 4
        if thl < 20 or ihl + thl > size:
            raise ValueError("Captured TCP header leaves its IP frame")
        flags = frame[start + 13]
        if flags & 2:
            base = (sequence + 1) & 0xffffffff
            if initial is not None and initial != base:
                raise ValueError("Capture contains another TCP connection")
            initial = base
        tcp_count += 1
        resets += bool(flags & 4)
        fins += bool(flags & 1)
        payload = frame[start + thl:14 + size]
        if payload:
            payload_count += 1
            segments.append(((sequence + bool(flags & 2)) & 0xffffffff, payload))
    if not frames or not tcp_count or initial is None or not segments:
        raise ValueError("Capture contains no complete TCP/TLS progress")
    stream = bytearray()
    for sequence, payload in sorted(segments,
                                   key=lambda item: (item[0] - initial) & 0xffffffff):
        position = (sequence - initial) & 0xffffffff
        overlap = min(max(len(stream) - position, 0), len(payload))
        if (position > len(stream) or position + len(payload) > PCAP_MAX_BYTES or
                stream[position:position + overlap] != payload[:overlap]):
            raise ValueError("Captured TCP stream has a gap or conflicting overlap")
        stream.extend(payload[overlap:])
    if stream[:1] != b"\x16":
        raise ValueError("Captured stream does not start with a TLS handshake")
    cursor = applications = records = 0
    partial = False
    while cursor < len(stream):
        if cursor + 5 > len(stream):
            partial = True
            break
        kind, major, minor, length = struct.unpack_from("!BBBH", stream, cursor)
        if (kind not in {20, 21, 22, 23} or major != 3 or
                not 1 <= minor <= 3 or not 0 < length <= 18432):
            raise ValueError("Captured stream contains unreviewed TLS record framing")
        if cursor + 5 + length > len(stream):
            partial = True
            break
        body = stream[cursor + 5:cursor + 5 + length]
        if kind == 23:
            if length < 24 or any(marker in body for marker in (
                    b"nexus/host/e2e", b"bounded TLS MQTT echo")):
                raise ValueError("Captured application payload exposes plaintext fixture")
            applications += 1
        records += 1
        cursor += 5 + length
    if complete_tls and (partial or applications == 0):
        raise ValueError("Successful case has an incomplete TLS application stream")
    return {"file": identity, "frames": frames, "arp_frames": arp,
            "tcp_frames": tcp_count, "tcp_payload_frames": payload_count,
            "tcp_resets": resets, "tcp_fins": fins, "tls_records": records,
            "tls_application_records": applications, "partial_tls_tail": partial,
            "payload_assessment": (
                "TLS framing and known-plaintext absence only; not decrypted")}


def capture_report(mode, directory, stdout, receipt, client_binary_sha256):
    """Bind exact capture bytes to this case, executable and completed command."""
    if mode not in case_names("lwip-test-relay") or not re.fullmatch(
            "[0-9a-f]{64}", client_binary_sha256):
        raise ValueError("Capture case or binary identity is not canonical")
    raw, receipt_identity = captured_bytes(receipt, 1024 * 1024)
    command = parse_json(raw)
    argv = command.get("command")
    if (type(command.get("schema")) is not int or command["schema"] != 1 or
            command.get("outcome") != "completed" or
            type(command.get("exit_code")) is not int or command["exit_code"] != 0 or
            command.get("withdrawal_complete") is not True or
            not isinstance(argv, list) or len(argv) != 5 or
            any(not isinstance(item, str) for item in argv) or
            argv[2] != mode or argv[4] != str(Path(directory).absolute())):
        raise ValueError("Capture command did not complete this exact case")
    _, binary = captured_bytes(argv[0], 64 * 1024 * 1024)
    if binary["sha256"] != client_binary_sha256:
        raise ValueError("Capture executable changed from its observed identity")
    observed_stdout = command.get("stdout")
    if not isinstance(observed_stdout, dict):
        raise ValueError("Capture command has no raw stdout identity")
    raw_stdout, output_identity = captured_bytes(observed_stdout.get("path", ""),
                                                  1024 * 1024)
    if (raw_stdout != stdout.encode("utf-8") or
            observed_stdout.get("bytes") != output_identity["size"] or
            observed_stdout.get("sha256") != output_identity["sha256"]):
        raise ValueError("Capture stdout differs from its command receipt")
    metrics = {}
    capture_metric = (
        r"\b((?:client|relay)_capture_(?:frames|bytes|complete))=([0-9]+)\b")
    for key, value in re.findall(capture_metric, stdout):
        if key in metrics:
            raise ValueError("Duplicate capture completion metric")
        metrics[key] = int(value)
    reports = {}
    for direction in ("client", "relay"):
        report = read_pcap(Path(directory) / (direction + ".pcap"), direction,
                           complete_tls=mode in {"clean", "backpressure"})
        if (metrics.get(direction + "_capture_complete") != 1 or
                metrics.get(direction + "_capture_frames") != report["frames"] or
                metrics.get(direction + "_capture_bytes") != report["file"]["size"]):
            raise ValueError("Capture file differs from complete writer observations")
        reports[direction] = report
    if mode == "fault" and sum(item["tcp_resets"] for item in reports.values()) == 0:
        raise ValueError("Fault capture contains no actual TCP reset")
    return {"case": mode, "client_binary_sha256": client_binary_sha256,
            "command_receipt": receipt_identity,
            "capture_tool_sha256": hashlib.sha256(
                Path(__file__).read_bytes()).hexdigest(),
            "capture_points": "successful IPC Ethernet TX before SETTLED",
            "timestamp_domain": "host monotonic elapsed microseconds",
            "physical_status": "not_executed", "directions": reports}


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--transport", choices=("posix", "lwip-test-relay",
                                                  "external-client"),
                        default="posix")
    args = parser.parse_args()
    args.client = args.client.resolve(strict=True)
    args.output_root = args.output_root.absolute()
    if not args.client.is_file() or not os.access(args.client, os.X_OK):
        parser.error("--client must be an existing executable")
    if (args.output_root.exists() or not args.output_root.parent.is_dir() or
            args.output_root.resolve() != args.output_root):
        parser.error("--output-root must be new with a canonical existing parent")
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
        "scope": transport_scope(args.transport),
        "transport": args.transport,
        "receipts": [], "cases": [], "cleanup": [],
        "client_binary_sha256": hashlib.sha256(args.client.read_bytes()).hexdigest(),
    }
    if args.transport == "lwip-test-relay":
        source_root = Path(__file__).resolve().parents[2]
        fixture_sources = (
            "tests/contracts/lwip_mqtt_interop/client.c",
            "tests/contracts/lwip_mqtt_interop/transport.cpp",
            "tests/contracts/lwip_mqtt_interop/fixture.h",
            "tests/contracts/lwip_mqtt_interop/pcap.h",
            "tests/google/support/lwip_frame_pair.h",
            "tools/testing/mqtt_tls_interop.py",
            "tools/evidence/common.py",
        )
        record["capture_source_observations"] = {
            name: captured_bytes(source_root / name, 1024 * 1024)[1]
            for name in fixture_sources}
        record["source_binding_scope"] = (
            "Observed test source hashes; actual binary/compiler closure is "
            "bound by the separate build evidence, not inferred from live files")
        (evidence / "cases").mkdir(mode=0o700)
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
        for mode in case_names(args.transport):
            command = [str(args.client), mapping[0]["HostPort"], mode,
                       str(credentials)]
            case_directory = evidence / "cases" / mode
            if args.transport == "lwip-test-relay":
                case_directory.mkdir(mode=0o700)
                command.append(str(case_directory))
            case = run(mode, command, required=False, timeout=20)
            record["cases"].append({
                "name": mode, "exit_code": case.returncode,
                "receipt": str(case.receipt_path), "stdout": case.stdout,
            })
            if case.returncode == 0 and args.transport == "lwip-test-relay":
                record["cases"][-1]["metrics"] = chain_report(mode, case.stdout)
                record["cases"][-1]["capture"] = capture_report(
                    mode, case_directory, case.stdout, case.receipt_path,
                    record["client_binary_sha256"])
            print(mode, case.returncode, case.stdout, flush=True)
            if case.returncode:
                raise RuntimeError(f"{mode} integration case failed")
        if args.transport == "lwip-test-relay" and any(
                captured_bytes(source_root / name, 1024 * 1024)[1] != identity
                for name, identity in record["capture_source_observations"].items()):
            raise ValueError("Capture fixture sources changed during execution")
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
