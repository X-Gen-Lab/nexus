"""An external supplier is verified before reference targets can be created."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock


PACKAGE = Path(__file__).resolve().parents[1]
REPOSITORY = PACKAGE.parents[2]


class SupplierAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = Path(self.supplied_root) if hasattr(self, 'supplied_root') else (
            REPOSITORY / 'vendors/gigadevice/gd32f4xx')
        self.vendor = self.root / 'supplier'
        shutil.copytree(source, self.vendor, ignore=shutil.ignore_patterns('.git', '__pycache__'))
        self.package = self.root / 'package'
        shutil.copytree(PACKAGE, self.package, ignore=shutil.ignore_patterns('__pycache__'))
        references = json.loads((self.package / 'references.json').read_text())
        self.assertEqual(references['supplier']['sha256'],
                         hashlib.sha256((source / 'source.lock.json').read_bytes()).hexdigest(),
                         'Supplier baseline does not match the maintained reference')
        self.sdk = self.root / 'sdk'
        script = self.sdk / 'scripts/ci/verify_vendor_source.py'
        script.parent.mkdir(parents=True)
        verifier_root = Path(os.environ.get('NEXUS_INSTALLED_SOURCE', REPOSITORY))
        verifier = verifier_root / 'scripts/ci/verify_vendor_source.py'
        shutil.copyfile(verifier, script)

    def configure(self, vendor):
        source = self.root / 'consumer'
        source.mkdir()
        sentinel = self.root / 'target-created'
        (source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.31)\nproject(supplier_probe NONE)\n'
            'set(NEXUS_TEST_SUPPORT ON)\n'
            'set(NEXUS_SOURCE_DIR "' + self.sdk.as_posix() + '")\n'
            'set(NEXUS_GD32_VENDOR_ROOT "' + vendor.as_posix() + '")\n'
            'function(add_library)\nfile(WRITE "' + sentinel.as_posix() + '" "created")\nendfunction()\n'
            'include("' + (self.package / 'CMakeLists.txt').as_posix() + '")\n')
        result = subprocess.run(['cmake', '-S', str(source), '-B', str(self.root / 'build')],
                                capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('GD32 supplier', result.stdout + result.stderr)
        self.assertFalse(sentinel.exists(), 'An unverified supplier created a target')

    def test_missing_supplier_is_rejected_before_targets(self):
        self.configure(self.root / 'missing')

    def test_changed_supplier_lock_is_rejected_before_targets(self):
        lock = self.vendor / 'source.lock.json'
        lock.write_bytes(lock.read_bytes() + b'\n')
        self.configure(self.vendor)

    def test_changed_supplier_payload_is_rejected_before_targets(self):
        payload = self.vendor / 'Firmware/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c'
        payload.write_bytes(payload.read_bytes() + b'\n')
        self.configure(self.vendor)


class PrepareExternalAdmissionTests(unittest.TestCase):
    def test_changed_lock_cannot_create_output_or_overwrite_sandbox_file(self):
        spec = importlib.util.spec_from_file_location(
            'prepare_gd32_external', PACKAGE / 'testing/prepare_external.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repository = root / 'repository'
            supplier = repository / 'vendors/gigadevice/gd32f4xx'
            supplier.mkdir(parents=True)
            (supplier / 'README.md').write_text('supplier fixture\n')
            (repository / 'outside.txt').write_bytes(b'untrusted replacement')
            sentinel = root / 'outside.txt'
            sentinel.write_bytes(b'unchanged sandbox sentinel')
            (supplier / 'source.lock.json').write_text(json.dumps({
                'files': [{'path': '../../../outside.txt'}]}))
            for support in ('ext/googletest', 'tests/cmsis_host'):
                (repository / support).mkdir(parents=True)
                (repository / support / 'public.txt').write_text('public fixture\n')
            output = root / 'external'
            error = None
            with mock.patch.object(module, 'REPOSITORY', repository), \
                    mock.patch.object(module.subprocess, 'run', return_value=
                                      subprocess.CompletedProcess([], 0, stdout='0' * 40)):
                try:
                    module.prepare(output)
                except (OSError, ValueError, subprocess.SubprocessError) as caught:
                    error = caught
            with self.subTest('rejected'):
                self.assertIsInstance(error, ValueError)
            with self.subTest('no output'):
                self.assertFalse(output.exists())
            with self.subTest('no write outside output'):
                self.assertEqual(sentinel.read_bytes(), b'unchanged sandbox sentinel')


if __name__ == '__main__':
    if os.environ.get('NEXUS_GD32_VENDOR_ROOT'):
        SupplierAdmissionTests.supplied_root = os.environ['NEXUS_GD32_VENDOR_ROOT']
    unittest.main()
