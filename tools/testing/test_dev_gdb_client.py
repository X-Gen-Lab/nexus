"""Opt-in batch GDB owns its pipe server and preserves equipment contracts."""

from contextlib import chdir, redirect_stdout
import io
import json
import os
from pathlib import Path
import shlex
import signal
import socket
import sys
import threading
import time
import unittest
from unittest.mock import patch

from tools.dev import device, process
from tools.evidence.common import file_identity
from tools.testing import test_dev_device as equipment_fixture


class GDBClientTests(unittest.TestCase):
    # Reuse the unchanged provenance/equipment fixture, not its test cases.
    def setUp(self):
        equipment_fixture.DeveloperDeviceTests.setUp(self)
        self.client = self.root / "gdb '$tool;literal"
        self.client.write_text(
            '#!' + sys.executable + '\n'
            'import json, pathlib, shlex, subprocess, sys, time\n'
            'args=sys.argv[1:]\n'
            'pathlib.Path("gdb-argv.json").write_text(json.dumps(args))\n'
            'pipe=next(x.split("=",1)[1] for x in args if '
            'x.startswith("--eval-command=target extended-remote | "))\n'
            'words=shlex.split(pipe.split("|",1)[1])\n'
            'assert words.pop(0)=="exec"\n'
            'child=subprocess.Popen(words,stdin=subprocess.PIPE,\n'
            ' stdout=subprocess.PIPE)\n'
            'child.stdin.write(b"PING\\n");child.stdin.flush()\n'
            'answer=child.stdout.readline()\n'
            'child.stdin.close()\n'
            'code=child.wait()\n'
            'if code or answer!=b"PONG\\n":sys.exit(17)\n'
            'print("software pipe attach/detach observed")\n')
        self.client.chmod(0o700)
        self.tool.write_text(
            '#!' + sys.executable + '\n'
            'import json, pathlib, sys\n'
            'pathlib.Path("server-argv.json").write_text(json.dumps(sys.argv[1:]))\n'
            'if sys.stdin.buffer.readline()==b"PING\\n":\n'
            ' sys.stdout.buffer.write(b"PONG\\n");sys.stdout.buffer.flush()\n')
        self.target['probe']['tool'] = file_identity(self.tool)
        self.target['gdb_client'] = {'tool': file_identity(self.client)}
        self.write_target()

    def write_target(self):
        equipment_fixture.DeveloperDeviceTests.write_target(self)

    def plan(self, action):
        return device.plan(action, self.target_path, self.provenance,
                           elf=self.elf, timeout=2, port=3333)

    def client_plan(self, timeout=2):
        return device.plan('debug', self.target_path, self.provenance,
                           elf=self.elf, timeout=timeout, client=True)

    def test_default_server_ignores_unselected_client_and_client_is_debug_only(self):
        self.target['gdb_client']['tool']['sha256'] = '0' * 64
        self.write_target()
        ordinary = self.plan('debug')
        self.assertEqual(ordinary['command'][0], str(self.tool))
        self.assertEqual(ordinary['debug_port'], 3333)
        with self.assertRaisesRegex(ValueError, 'identity|changed'):
            self.client_plan()
        with self.assertRaisesRegex(ValueError, 'debug'):
            device.plan('flash', self.target_path, self.provenance, client=True)

    def test_default_cli_still_prints_its_real_tcp_endpoint(self):
        output = io.StringIO()
        with redirect_stdout(output), patch.object(
                device, 'execute', return_value={'exit_code': 0}):
            status = device.main('debug', [
                '--target', str(self.target_path), '--provenance',
                str(self.provenance), '--execute', '--output',
                str(self.root / 'default-cli')])
        self.assertEqual(status, 0)
        self.assertIn('target extended-remote 127.0.0.1:3333', output.getvalue())

    def test_preview_quotes_literal_server_arguments_and_keeps_elf_as_argv(self):
        with patch('subprocess.Popen') as launched:
            prepared = self.client_plan()
        launched.assert_not_called()
        command = prepared['command']
        self.assertEqual(command[0], str(self.client))
        self.assertIn('--batch', command)
        self.assertIn('--nx', command)
        self.assertIn('--init-eval-command=set auto-load off', command)
        self.assertIn('--se=' + str(self.elf), command)
        target = next(item.removeprefix('--eval-command=') for item in command
                      if item.startswith('--eval-command=target extended-remote'))
        server = shlex.split(target.split('|', 1)[1])
        self.assertEqual(server[:2], ['exec', str(self.tool)])
        self.assertIn('gdb_port pipe', server)
        self.assertIn('log_output openocd.log', server)
        self.assertIn('--eval-command=info threads', command)
        self.assertEqual(command[-1], '--eval-command=detach')
        self.assertIsNone(prepared['debug_port'])

    def test_actual_pipe_success_keeps_station_and_tcp_foreign_listener_separate(self):
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen()
            listener.settimeout(0.05)
            output = self.root / 'attached'
            report = device.execute(self.client_plan(), output)
            self.assertEqual(report['exit_code'], 0)
            with self.assertRaises(TimeoutError):
                listener.accept()
        self.assertFalse(Path(self.target['station_lock']).exists())
        arguments = json.loads((output / 'gdb-argv.json').read_text())
        self.assertIn('--se=' + str(self.elf), arguments)
        self.assertFalse((self.root / 'literal').exists())
        self.assertFalse(report['qualification'])
        self.assertEqual(report['physical_status'], 'executed_unqualified')

    def test_relative_provenance_elf_still_loads_the_verified_original_bytes(self):
        content = self.client.read_text().replace(
            'args=sys.argv[1:]\n', 'args=sys.argv[1:]\n'
            'image=next(x.split("=",1)[1] for x in args '
            'if x.startswith("--se="))\n'
            'assert pathlib.Path(image).read_bytes()[:6]==b"\\x7fELF\\x01\\x01"\n')
        self.client.write_text(content)
        self.target['gdb_client']['tool'] = file_identity(self.client)
        self.write_target()
        provenance = json.loads(self.provenance.read_text())
        for artifact in provenance['artifacts']:
            if artifact['path'] == str(self.elf):
                artifact['path'] = self.elf.name
        self.provenance.write_text(json.dumps(provenance))
        output = self.root / 'relative-image'
        with chdir(self.root):
            prepared = self.client_plan()
            report = device.execute(prepared, output)
        self.assertEqual(report['exit_code'], 0,
                         (output / 'command.log').read_text())
        self.assertEqual(prepared['elf']['path'], str(self.elf))
        self.assertIn('--se=' + str(self.elf), prepared['command'])
        self.assertFalse((output / self.elf.name).exists())

    def test_commands_are_bound_and_changed_bytes_reject_before_any_execution(self):
        script = self.root / 'commands [literal];$.gdb'
        script.write_text('info registers\n')
        self.target['gdb_client']['commands'] = file_identity(script)
        self.write_target()
        prepared = self.client_plan()
        self.assertIn('--command=' + str(script), prepared['command'])
        script.write_text('changed bytes\n')
        with patch('subprocess.Popen') as launched:
            with self.assertRaisesRegex(ValueError, 'identity|changed'):
                device.execute(prepared, self.root / 'rejected')
        launched.assert_not_called()
        self.assertFalse((self.root / 'rejected').exists())

    def test_client_nonzero_keeps_raw_failure_and_station_lock(self):
        self.client.write_text('#!' + sys.executable + '\nimport sys\n'
                               'print("client failed",file=sys.stderr)\nsys.exit(9)\n')
        self.target['gdb_client']['tool'] = file_identity(self.client)
        self.write_target()
        report = device.execute(self.client_plan(), self.root / 'client-failed')
        self.assertEqual(report['exit_code'], 9)
        self.assertTrue(report['station_quarantined'])
        self.assertIn('client failed',
                      (self.root / 'client-failed/command.log').read_text())

    def test_server_early_failure_keeps_raw_failure_and_station_lock(self):
        self.tool.write_text('#!' + sys.executable + '\nimport sys\n'
                             'print("server failed",file=sys.stderr)\nsys.exit(7)\n')
        self.target['probe']['tool'] = file_identity(self.tool)
        self.write_target()
        report = device.execute(self.client_plan(), self.root / 'server-failed')
        self.assertEqual(report['exit_code'], 17)
        self.assertTrue(report['station_quarantined'])
        self.assertIn('server failed',
                      (self.root / 'server-failed/command.log').read_text())

    def test_changed_command_file_after_client_exit_cannot_report_success(self):
        script = self.root / 'reviewed.gdb'
        script.write_text('info registers\n')
        self.target['gdb_client']['commands'] = file_identity(script)
        self.write_target()
        actual_run = process.run

        def changed(*args, **kwargs):
            result = actual_run(*args, **kwargs)
            script.write_text('changed during execution\n')
            return result

        with patch.object(process, 'run', side_effect=changed):
            report = device.execute(self.client_plan(), self.root / 'changed')
        self.assertEqual(report['child_exit_code'], 0)
        self.assertEqual(report['exit_code'], 2)
        self.assertTrue(report['station_quarantined'])

    def test_sigint_withdraws_the_owned_client_server_chain(self):
        self.tool.write_text('#!' + sys.executable + '\n'
                             'import os, pathlib, time\n'
                             'pathlib.Path("server.pid").write_text(str(os.getpid()))\n'
                             'time.sleep(60)\n')
        self.target['probe']['tool'] = file_identity(self.tool)
        self.write_target()
        output = self.root / 'cancelled'
        ready = output / 'server.pid'

        def cancel():
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if ready.exists():
                    os.kill(os.getpid(), signal.SIGINT)
                    return
                time.sleep(0.005)

        interrupter = threading.Thread(target=cancel)
        interrupter.start()
        try:
            report = device.execute(self.client_plan(timeout=10), output)
        finally:
            interrupter.join(timeout=6)
        self.assertFalse(interrupter.is_alive())
        self.assertEqual(report['exit_code'], 130)
        receipt = json.loads(Path(report['receipt']).read_text())
        self.assertEqual(receipt['outcome'], 'cancelled')
        self.assertTrue(receipt['withdrawal_complete'])
        current = process._linux_process(int(ready.read_text()))
        self.assertTrue(current is None or current['state'] in ('Z', 'X'))

    def test_timeout_withdraws_server_descendants_before_returning_quarantine(self):
        self.tool.write_text(
            '#!' + sys.executable + '\n'
            'import json, os, pathlib, subprocess, sys, time\n'
            'child=subprocess.Popen([sys.executable,"-c",\n'
            ' "import time; time.sleep(60)"])\n'
            'pathlib.Path("owned.json").write_text(\n'
            ' json.dumps([os.getpid(),child.pid]))\n'
            'time.sleep(60)\n')
        self.target['probe']['tool'] = file_identity(self.tool)
        self.write_target()
        output = self.root / 'timeout'
        report = device.execute(self.client_plan(timeout=1), output)
        self.assertEqual(report['exit_code'], 124)
        receipt = json.loads(Path(report['receipt']).read_text())
        self.assertEqual(receipt['outcome'], 'timed_out')
        self.assertTrue(receipt['withdrawal_complete'])
        for pid in json.loads((output / 'owned.json').read_text()):
            current = process._linux_process(pid)
            self.assertTrue(current is None or current['state'] in ('Z', 'X'))
        self.assertTrue(report['station_quarantined'])

    def test_pipe_control_characters_and_conflicting_cli_port_are_rejected(self):
        path = self.root / 'line\nbreak.cfg'
        path.write_text('reviewed but not a one-line GDB pipe argument\n')
        self.target['probe']['interface_config'] = file_identity(path)
        self.write_target()
        with self.assertRaisesRegex(ValueError, 'control|one.line'):
            self.client_plan()
        with self.assertRaisesRegex(ValueError, 'port|pipe'):
            device.main('debug', ['--target', str(self.target_path),
                        '--provenance', str(self.provenance), '--client',
                        '--port', '3333'])


if __name__ == '__main__':
    unittest.main()
