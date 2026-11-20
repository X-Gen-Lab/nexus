"""Published views stay coherent while a build replaces or fails its attempt."""

from contextlib import redirect_stdout
from http.server import ThreadingHTTPServer
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import urlopen

from tools.documentation import build, cli


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name) / "source"
        (self.root / "docs").mkdir(parents=True)
        self.output = build.prepare_output(self.root, self.root / "build/docs")
        self.old = self.view(self.output / "site", "old", "a")
        build.record(self.output / "documentation-build.json", self.old)
        build.record(self.output / "last-published.json", self.old)

    def view(self, folder, text, character):
        folder.mkdir(parents=True)
        for language in ("en", "zh_CN"):
            (folder / language).mkdir()
            (folder / language / "index.html").write_text(
                "<!doctype html><html><body><p>" + text + "</p></body></html>")
        (folder / "index.html").write_text("<html><body>languages</body></html>")
        identifier = character * 32
        build.record(folder / "documentation-view.json",
                     {"publication_id": identifier})
        return {"schema_version": 2, "kind": "nexus_documentation_build",
                "mode": "preview", "status": "ready", "scope": "all",
                "languages": ["en", "zh_CN"],
                "source": {"commit": character * 40, "dirty": False},
                "publication_id": identifier, "files": build.inventory(folder)}

    def start_cli_server(self):
        real_server = ThreadingHTTPServer
        started = threading.Event()
        servers = []

        def create(address, handler):
            server = real_server(address, handler)
            servers.append(server)
            started.set()
            return server

        def serve():
            with redirect_stdout(io.StringIO()):
                cli.main(["serve", "--build", str(self.output), "--port", "0"],
                         root=self.root)

        self.server_patch = patch.object(cli, "ThreadingHTTPServer", side_effect=create)
        self.server_patch.start()
        thread = threading.Thread(target=serve)
        thread.start()
        self.assertTrue(started.wait(5), "server did not admit the published view")
        server = servers[0]
        self.server = server

        def cleanup():
            server.shutdown()
            thread.join(5)
            self.server_patch.stop()
            self.assertFalse(thread.is_alive())

        self.addCleanup(cleanup)
        return f"http://127.0.0.1:{server.server_port}"

    def test_existing_server_labels_previous_view_after_failed_or_running_build(self):
        base = self.start_cli_server()
        for status, language, message in (
                ("running", "en", "Build in progress"),
                ("failed", "zh_CN", "本次构建失败")):
            with self.subTest(status=status):
                build.record(self.output / "documentation-build.json", {
                    "schema_version": 2, "kind": "nexus_documentation_build",
                    "status": status, "source": {"commit": "b" * 40}})
                with urlopen(base + "/" + language + "/index.html", timeout=5) as reply:
                    body = reply.read().decode()
                    self.assertEqual(reply.headers["X-Nexus-Build-State"], status)
                    self.assertEqual(reply.headers["X-Nexus-View-Commit"], "a" * 40)
                self.assertIn(message, body)
                self.assertIn("a" * 40, body)
                self.assertIn("<p>old</p>", body)
                self.assertNotIn("b" * 40, body)

    def test_new_server_can_admit_last_view_after_latest_attempt_failed(self):
        build.record(self.output / "documentation-build.json", {
            "schema_version": 2, "kind": "nexus_documentation_build",
            "status": "failed"})
        self.assertEqual(cli.serve_output(self.root, self.output), self.output / "site")

    def test_marker_mismatch_returns_service_unavailable(self):
        base = self.start_cli_server()
        build.record(self.output / "site/documentation-view.json",
                     {"publication_id": "b" * 32})
        with self.assertRaises(HTTPError) as rejected:
            urlopen(base + "/en/index.html", timeout=5)
        self.assertEqual(rejected.exception.code, 503)

    def test_invalid_published_languages_reject_before_starting_server(self):
        metadata = dict(self.old, languages=[{}])
        build.record(self.output / "last-published.json", metadata)
        with patch.object(cli, "ThreadingHTTPServer") as server, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["serve", "--build", str(self.output)],
                                      root=self.root), 2)
        server.assert_not_called()

    def test_malformed_attempt_rejects_before_starting_server(self):
        build.record(self.output / "documentation-build.json", [])
        with patch.object(cli, "ThreadingHTTPServer") as server, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["serve", "--build", str(self.output)],
                                      root=self.root), 2)
        server.assert_not_called()

    def test_publication_rolls_back_site_and_record_if_report_write_fails(self):
        from tools.documentation import publication
        candidate = self.output / "work/site"
        metadata = self.view(candidate, "new", "b")
        failed = []

        def write(path, value):
            if Path(path).name == "documentation-build.json" and not failed:
                failed.append(True)
                raise OSError("report publication failed")
            build.record(path, value)

        with self.assertRaisesRegex(OSError, "publication failed"):
            publication.publish(self.output, candidate, metadata, record=write)
        self.assertIn("<p>old</p>", (self.output / "site/en/index.html").read_text())
        self.assertEqual(json.loads((self.output / "last-published.json").read_text()),
                         self.old)
        self.assertEqual(cli.serve_output(self.root, self.output), self.output / "site")

    def test_slow_body_reader_does_not_hold_up_publication(self):
        from tools.documentation import publication
        candidate = self.output / "work/site"
        metadata = self.view(candidate, "new", "b")
        base = self.start_cli_server()
        opened, release, published = (threading.Event() for _ in range(3))
        errors, bodies = [], []
        handler = self.server.RequestHandlerClass
        copy = handler.copyfile

        def pause(instance, source, destination):
            opened.set()
            if not release.wait(5):
                errors.append("reader was never released")
            copy(instance, source, destination)

        def read():
            try:
                with urlopen(base + "/en/index.html", timeout=5) as response:
                    bodies.append(response.read().decode())
            except Exception as error:
                errors.append(error)

        def publish():
            try:
                publication.publish(self.output, candidate, metadata,
                                    record=build.record)
            except Exception as error:
                errors.append(error)
            finally:
                published.set()

        with patch.object(handler, "copyfile", pause):
            reader = threading.Thread(target=read)
            reader.start()
            self.assertTrue(opened.wait(5))
            writer = threading.Thread(target=publish)
            writer.start()
            try:
                self.assertTrue(published.wait(1),
                                "slow body transfer retained publication lock")
            finally:
                release.set()
                reader.join(5)
                writer.join(5)
        self.assertEqual(errors, [])
        self.assertIn("<p>old</p>", bodies[0])
        self.assertIn("<p>new</p>", (self.output / "site/en/index.html").read_text())

    def test_malformed_latest_attempt_returns_service_unavailable(self):
        base = self.start_cli_server()
        build.record(self.output / "documentation-build.json", [])
        with self.assertRaises(HTTPError) as rejected:
            urlopen(base + "/en/index.html", timeout=5)
        self.assertEqual(rejected.exception.code, 503)

    def test_http_reader_waits_across_real_process_directory_switch(self):
        from tools.documentation import publication
        del publication
        candidate = self.output / "work/site"
        metadata = self.view(candidate, "new", "b")
        record_file = self.output / "candidate.json"
        build.record(record_file, metadata)
        base = self.start_cli_server()
        code = """
import json, sys
from pathlib import Path
from unittest.mock import patch
from tools.documentation import build, publication
output, candidate, record_file = map(Path, sys.argv[1:])
rename = Path.rename
def pause(path, target):
    result = rename(path, target)
    if path == output / 'site':
        print('switch-gap', flush=True)
        assert sys.stdin.read(1) == 'x'
    return result
with patch.object(Path, 'rename', pause):
    publication.publish(output, candidate, json.loads(record_file.read_text()),
                        record=build.record)
"""
        child = subprocess.Popen([sys.executable, "-c", code, str(self.output),
            str(candidate), str(record_file)], cwd=Path(__file__).resolve().parents[2],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True)
        def stop_child():
            if child.poll() is None:
                child.kill()
            child.wait(timeout=5)
            for stream in (child.stdin, child.stdout, child.stderr):
                if stream is not None:
                    stream.close()

        self.addCleanup(stop_child)
        self.assertEqual(child.stdout.readline().strip(), "switch-gap")
        self.assertFalse((self.output / "site").exists())
        done, replies, errors = threading.Event(), [], []

        def read():
            try:
                with urlopen(base + "/en/index.html", timeout=5) as response:
                    replies.append((response.status, response.read().decode(),
                                    response.headers["X-Nexus-View-Commit"]))
            except Exception as error:
                errors.append(error)
            finally:
                done.set()

        reader = threading.Thread(target=read)
        reader.start()
        self.assertFalse(done.wait(0.1), "HTTP reader crossed an incomplete switch")
        stdout, stderr = child.communicate("x", timeout=5)
        self.assertEqual(child.returncode, 0, stdout + stderr)
        reader.join(5)
        self.assertFalse(reader.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(replies[0][0], 200)
        self.assertIn("<p>new</p>", replies[0][1])
        self.assertEqual(replies[0][2], "b" * 40)


if __name__ == "__main__":
    unittest.main()
