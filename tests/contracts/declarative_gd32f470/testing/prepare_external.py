"""Copy only public reference, supplier and host-test inputs to a fresh directory."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess


PACKAGE = Path(__file__).resolve().parents[1]
REPOSITORY = PACKAGE.parents[2]


def copy_tree(source, destination):
    if any(path.is_symlink() for path in source.rglob('*') if '.git' not in path.parts):
        raise ValueError('External inputs cannot follow symlinks')
    ignored = shutil.ignore_patterns('.git', '__pycache__', '*.pyc')
    shutil.copytree(source, destination, ignore=ignored, symlinks=False)
    if any(path.is_symlink() for path in destination.rglob('*')):
        raise ValueError('External inputs cannot contain symlinks')


def prepare(output):
    output = output.absolute()
    if output.exists():
        raise ValueError('External reference output must not already exist')
    if output.is_relative_to(PACKAGE):
        raise ValueError('External reference cannot overwrite its maintained inputs')
    supplier = REPOSITORY / 'vendors/gigadevice/gd32f4xx'
    reference = json.loads((PACKAGE / 'references.json').read_bytes())['supplier']
    if reference['lock'] != 'source.lock.json':
        raise ValueError('Supplier lock must be source.lock.json')
    lock_path = supplier / reference['lock']
    if lock_path.is_symlink():
        raise ValueError('Supplier lock must be a regular file')
    lock_bytes = lock_path.read_bytes()
    if hashlib.sha256(lock_bytes).hexdigest() != reference['sha256']:
        raise ValueError('Supplier lock differs from the maintained reference')
    spec = importlib.util.spec_from_file_location(
        'verify_gd32_source', REPOSITORY / 'scripts/ci/verify_vendor_source.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    verifier.verify(supplier)
    lock = json.loads(lock_bytes)
    output.mkdir(parents=True)
    copy_tree(PACKAGE, output / 'provider')
    target = output / 'suppliers/gd32f4xx'
    for name in ['source.lock.json', 'README.md', *(row['path'] for row in lock['files'])]:
        source = supplier / name
        if source.is_symlink() or not source.is_file():
            raise ValueError('Supplier input must be a public regular file')
        destination = target / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    copy_tree(REPOSITORY / 'ext/googletest', output / 'support/googletest')
    copy_tree(REPOSITORY / 'tests/cmsis_host', output / 'support/cmsis_host')
    files = {path.relative_to(output).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in output.rglob('*') if path.is_file()}
    googletest = subprocess.run(['git', '-C', str(REPOSITORY / 'ext/googletest'),
                                'rev-parse', 'HEAD'], check=True,
                               capture_output=True, text=True).stdout.strip()
    (output / 'inputs.json').write_text(json.dumps({
        'scope': 'external reference inputs; not part of the core SDK identity',
        'googletest_commit': googletest, 'files_sha256': files}, indent=2) + '\n')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    print(prepare(parser.parse_args().output))
