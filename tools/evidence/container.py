#!/usr/bin/env python3
"""Seal a real local Docker image as an OCI layout, without inventing RepoDigests."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest, fields,
    file_identity, load_json, run_adapter, sha256_value)


def docker_prefix(executable: str = "docker") -> list[str]:
    return ["env", "-u", "DOCKER_HOST", "-u", "DOCKER_CONTEXT", "-u", "DOCKER_TLS",
            "-u", "DOCKER_TLS_VERIFY", "-u", "DOCKER_CERT_PATH", executable,
            "--host=unix:///var/run/docker.sock"]


def inspect_image(image: str, executable: str = "docker") -> dict:
    output = run_adapter([*docker_prefix(executable), "image", "inspect", image], 30)
    value = json.loads(output)
    if not isinstance(value, list) or len(value) != 1:
        raise EvidenceError("one actual Docker image identity required")
    image = value[0]
    sha256_value(image["Id"].removeprefix("sha256:"))
    if image["Os"] != "linux" or image["Architecture"] != "amd64":
        raise EvidenceError("formal container currently supports linux/amd64")
    return image


def seal(image: str, output: Path, *, docker: str = "docker",
         dockerfile: Path | None = None) -> dict:
    observed = inspect_image(image, docker)
    if output.exists():
        raise EvidenceError("OCI seal destination must be new")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-oci-", dir=output.parent))
    try:
        archive = temporary / "docker-export.tar"
        process = subprocess.run([*docker_prefix(docker), "image", "save", "-o",
                                  str(archive), observed["Id"]], capture_output=True)
        if process.returncode:
            raise EvidenceError("actual Docker image export failed")
        with tarfile.open(archive, "r") as exported:
            members = exported.getmembers()
            if len({entry.name for entry in members}) != len(members):
                raise EvidenceError("ambiguous Docker archive members")
            entries = json.load(exported.extractfile("manifest.json"))
            if len(entries) != 1:
                raise EvidenceError("single image export required")
            entry = entries[0]
            blob_root = temporary / "blobs" / "sha256"
            blob_root.mkdir(parents=True)
            def import_blob(name, media_type):
                member = exported.getmember(name)
                if not member.isfile():
                    raise EvidenceError("Docker image blob is not a regular file")
                stream = exported.extractfile(member)
                destination = temporary / "incoming-blob"
                with destination.open("wb") as target:
                    shutil.copyfileobj(stream, target, 1024 * 1024)
                sha = digest(destination)
                size = destination.stat().st_size
                destination.replace(blob_root / sha)
                return {"mediaType": media_type, "digest": "sha256:" + sha,
                        "size": size}
            config = import_blob(entry["Config"], "application/vnd.oci.image.config.v1+json")
            if config["digest"] != observed["Id"]:
                raise EvidenceError("exported image config differs from actual image")
            layers = [import_blob(name, "application/vnd.oci.image.layer.v1.tar")
                      for name in entry["Layers"]]
            expected_layers = observed["RootFS"]["Layers"]
            if [layer["digest"] for layer in layers] != expected_layers:
                raise EvidenceError("exported layer bytes differ from image rootfs")
        manifest = {"schemaVersion": 2,
                    "mediaType": "application/vnd.oci.image.manifest.v1+json",
                    "config": config, "layers": layers}
        encoded = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
        manifest_sha = hashlib.sha256(encoded).hexdigest()
        (blob_root / manifest_sha).write_bytes(encoded)
        atomic_json(temporary / "oci-layout", {"imageLayoutVersion": "1.0.0"})
        atomic_json(temporary / "index.json", {"schemaVersion": 2, "manifests": [{
            "mediaType": manifest["mediaType"], "digest": "sha256:" + manifest_sha,
            "size": len(encoded), "platform": {"os": "linux", "architecture": "amd64"}}]})
        lock = {"schema_version": 1, "kind": "oci_tool_environment",
                "oci_manifest_sha256": manifest_sha,
                "image_config_id": observed["Id"], "rootfs_layers": expected_layers,
                "platform": "linux/amd64",
                "observed_repo_digests": observed.get("RepoDigests", []),
                "preparation_network": "allowed_only_before_formal_build",
                "formal_build_network": "none",
                "dockerfile": file_identity(dockerfile) if dockerfile else None}
        atomic_json(temporary / "environment.json", lock)
        archive.unlink()
        temporary.replace(output)
        return lock
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def verify(lock_path: Path, *, docker: str = "docker", runtime: bool = True) -> dict:
    lock = load_json(lock_path)
    fields(lock, {"schema_version", "kind", "oci_manifest_sha256", "image_config_id",
        "rootfs_layers", "platform", "observed_repo_digests", "preparation_network",
        "formal_build_network", "dockerfile"})
    if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1 or
            lock["kind"] != "oci_tool_environment"):
        raise EvidenceError("unsupported OCI environment lock")
    sha256_value(lock["oci_manifest_sha256"])
    root = lock_path.parent / "blobs" / "sha256"
    manifest_path = root / lock["oci_manifest_sha256"]
    if digest(manifest_path) != lock["oci_manifest_sha256"]:
        raise EvidenceError("OCI manifest digest mismatch")
    manifest = load_json(manifest_path)
    descriptors = [manifest["config"], *manifest["layers"]]
    for descriptor in descriptors:
        sha = descriptor["digest"].removeprefix("sha256:")
        sha256_value(sha)
        blob = root / sha
        if blob.stat().st_size != descriptor["size"] or digest(blob) != sha:
            raise EvidenceError("OCI image blob changed")
    if (manifest["config"]["digest"] != lock["image_config_id"] or
            [entry["digest"] for entry in manifest["layers"]] != lock["rootfs_layers"]):
        raise EvidenceError("sealed OCI manifest/config/rootfs identities differ")
    if not runtime:
        return lock
    observed = inspect_image(lock["image_config_id"], docker)
    if (manifest["config"]["digest"] != observed["Id"] or
            observed["RootFS"]["Layers"] != lock["rootfs_layers"] or
            [entry["digest"] for entry in manifest["layers"]] != lock["rootfs_layers"]):
        raise EvidenceError("runtime image differs from sealed OCI manifest")
    return lock


def load(lock_path: Path, *, docker: str = "docker") -> dict:
    """Restore the actual sealed bytes offline, never pull a mutable image tag."""
    lock = verify(lock_path, docker=docker, runtime=False)
    root = lock_path.parent / "blobs" / "sha256"
    manifest = load_json(root / lock["oci_manifest_sha256"])
    config_sha = manifest["config"]["digest"].removeprefix("sha256:")
    docker_manifest = [{"Config": config_sha + ".json", "RepoTags": [],
        "Layers": [entry["digest"].removeprefix("sha256:") + "/layer.tar"
                   for entry in manifest["layers"]]}]
    with tempfile.TemporaryDirectory(prefix="nexus-oci-import-") as temporary:
        archive = Path(temporary) / "image.tar"
        with tarfile.open(archive, "w") as target:
            target.add(root / config_sha, arcname=config_sha + ".json", recursive=False)
            for entry in manifest["layers"]:
                sha = entry["digest"].removeprefix("sha256:")
                target.add(root / sha, arcname=sha + "/layer.tar", recursive=False)
            metadata = Path(temporary) / "manifest.json"
            metadata.write_text(json.dumps(docker_manifest) + "\n")
            target.add(metadata, arcname="manifest.json", recursive=False)
        run_adapter([*docker_prefix(docker), "image", "load", "--input", str(archive)], 300)
    return verify(lock_path, docker=docker)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("seal", "verify", "load"))
    parser.add_argument("--image")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--dockerfile", type=Path)
    parser.add_argument("--docker", default="docker")
    args = parser.parse_args()
    try:
        if args.operation == "seal":
            if not args.image or not args.output:
                parser.error("seal requires --image and --output")
            result = seal(args.image, args.output, docker=args.docker,
                          dockerfile=args.dockerfile)
        else:
            if not args.lock:
                parser.error("verify requires --lock")
            result = (load(args.lock, docker=args.docker) if args.operation == "load"
                      else verify(args.lock, docker=args.docker))
        print("Verified actual OCI manifest sha256:" + result["oci_manifest_sha256"])
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError,
            tarfile.TarError) as error:
        print("OCI environment rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
