"""Coordinate complete site switches with readers of the last published view."""

from contextlib import contextmanager
from http.server import SimpleHTTPRequestHandler
import html
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat
import uuid

PUBLISHED = "last-published.json"
MARKER = "documentation-view.json"


def _plain(path):
    if any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError("documentation publication cannot use a symlink")
    if path.is_file() and path.stat().st_nlink != 1:
        raise ValueError("documentation publication cannot use a hardlink")


@contextmanager
def lock(output, *, write=False):
    """Readers share the short publication window, never the whole build lock."""
    path = Path(output).absolute() / ".nexus-documentation-publication.lock"
    _plain(path)
    descriptor = os.open(path, os.O_RDWR | os.O_CREAT |
                         getattr(os, "O_NOFOLLOW", 0), 0o600)
    with os.fdopen(descriptor, "a+b") as stream:
        details = os.fstat(stream.fileno())
        if not stat.S_ISREG(details.st_mode) or details.st_nlink != 1:
            raise ValueError("documentation publication requires a regular lock")
        if os.name == "posix":
            import fcntl
            fcntl.flock(stream, fcntl.LOCK_EX if write else fcntl.LOCK_SH)
        else:
            import msvcrt
            stream.seek(0)
            if not stream.read(1):
                stream.write(b"\0")
                stream.flush()
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_LOCK, 1)
        try:
            yield
        finally:
            if os.name != "posix":
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def mark(candidate):
    """Add a visible identity file before the caller inventories the candidate."""
    path = Path(candidate) / MARKER
    _plain(path)
    identifier = uuid.uuid4().hex
    path.write_text(json.dumps({"publication_id": identifier}) + "\n")
    return identifier


def read(output):
    """Read one published identity; the caller owns the publication read lock."""
    output = Path(output)
    report, marker = output / PUBLISHED, output / "site" / MARKER
    _plain(report)
    _plain(marker)
    value = json.loads(report.read_text())
    identity = json.loads(marker.read_text())
    if (not isinstance(value, dict) or not isinstance(identity, dict)
            or not isinstance(value.get("source"), dict)
            or not isinstance(value.get("publication_id"), str)
            or not re.fullmatch("[a-f0-9]{32}", value["publication_id"])
            or identity != {"publication_id": value["publication_id"]}):
        raise ValueError("documentation published identity is inconsistent")
    return value


def attempt(output, view):
    """Observe the latest attempt separately from the retained published view."""
    path = Path(output) / "documentation-build.json"
    _plain(path)
    latest = json.loads(path.read_text())
    if (not isinstance(latest, dict)
            or latest.get("status") not in {"running", "failed", "ready", "passed"}
            or (latest["status"] in {"ready", "passed"} and any(
                latest.get(key) != view.get(key)
                for key in ("publication_id", "source")))):
        raise ValueError("documentation attempt report is inconsistent")
    return latest


def publish(output, candidate, metadata, *, record):
    """Switch the complete tree and records under a short reader/writer lock.

    Two directory renames are coordinated with HTTP readers; this is not a
    single filesystem-atomic directory replacement. Ordinary failures roll back.
    A process interruption is rejected by the marker/record check on readers.
    """
    output, candidate = Path(output), Path(candidate)
    site, backup = output / "site", output / ".previous-site"
    for path in (site, backup, candidate, output / PUBLISHED):
        _plain(path)
    _plain(candidate / MARKER)
    identifier = json.loads((candidate / MARKER).read_text())
    if identifier != {"publication_id": metadata.get("publication_id")}:
        raise ValueError("documentation candidate identity differs")
    compact = {key: metadata[key] for key in (
        "schema_version", "kind", "mode", "status", "scope", "source",
        "languages", "files", "publication_id")}
    with lock(output, write=True):
        if backup.exists():
            raise ValueError("interrupted publication requires a fresh output")
        previous = (json.loads((output / PUBLISHED).read_text())
                    if (output / PUBLISHED).is_file() else None)
        moved = False
        try:
            if site.exists():
                site.rename(backup)
            candidate.rename(site)
            moved = True
            record(output / PUBLISHED, compact)
            record(output / "documentation-build.json", metadata)
        except BaseException:
            if moved:
                site.rename(candidate)
            if backup.exists():
                backup.rename(site)
            if previous is None:
                (output / PUBLISHED).unlink(missing_ok=True)
            else:
                record(output / PUBLISHED, previous)
            raise
    if backup.exists():
        shutil.rmtree(backup)


def handler(output):
    """Serve the published tree with visible latest-attempt and source state."""
    output = Path(output)

    class PublishedHandler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(output / "site"), **kwargs)

        def setup(self):
            super().setup()
            self.connection.settimeout(10)

        def _head(self):
            self._view, self._latest = None, None
            try:
                with lock(output):
                    self._view = read(output)
                    self._latest = attempt(output, self._view)
                    return self.send_head()
            except (OSError, ValueError, TypeError):
                self._view, self._latest = None, None
                self.send_error(503, "Documentation publication is unavailable")
                return None

        def do_GET(self):
            stream = self._head()
            if stream is not None:
                with stream:
                    self.copyfile(stream, self.wfile)

        def do_HEAD(self):
            stream = self._head()
            if stream is not None:
                stream.close()

        def end_headers(self):
            if self._view is not None and self._latest is not None:
                commit = self._view.get("source", {}).get("commit", "unknown")
                self.send_header("X-Nexus-View-Commit",
                                 re.sub(r"[^a-zA-Z0-9.-]", "?", str(commit)))
                self.send_header("X-Nexus-Build-State", self._latest["status"])
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

        def send_head(self):
            path = Path(self.translate_path(self.path))
            if path.is_dir() and self.path.split("?", 1)[0].endswith("/"):
                path = path / "index.html"
            status = self._latest["status"]
            if (status in {"running", "failed"} and path.suffix == ".html"
                    and path.is_file()):
                chinese = "/zh_CN/" in self.path
                message = (("构建进行中" if status == "running" else "本次构建失败")
                           + "；当前显示上次已发布来源：" if chinese else
                           ("Build in progress" if status == "running" else
                            "Latest build failed")
                           + "; showing previously published source: ")
                source = self._view["source"].get("commit", "unknown")
                commit = html.escape(str(source))
                banner = ('<aside role="alert" style="padding:1em;background:#fff3cd;'
                          'color:#332600">' + message + '<code>' + commit
                          + '</code></aside>').encode()
                payload = path.read_bytes()
                body = re.search(br"<body(?:\s[^>]*)?>", payload, re.IGNORECASE)
                position = body.end() if body else 0
                payload = payload[:position] + banner + payload[position:]
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                return io.BytesIO(payload)
            return super().send_head()

    return PublishedHandler
