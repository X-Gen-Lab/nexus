#!/usr/bin/env python3
"""Check prerequisites for an explicit maintained preset; never install unpinned tools."""
import argparse
import importlib.metadata
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
MAINTAINED_SUBMODULES = ['ext/freertos', 'ext/googletest', 'vendors/arm/CMSIS_5',
                         'vendors/st/cmsis_device_f4', 'vendors/st/stm32f4xx_hal_driver']
sys.path.insert(0, str(ROOT / 'scripts/ci'))
from ci_build import settings_for


def main(arguments=None):
    data = json.loads((ROOT / 'CMakePresets.json').read_text())
    presets = {item['name']: item for item in data['configurePresets']}
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', required=True, choices=[name for name, value in presets.items() if not value.get('hidden')])
    parser.add_argument('--init-deps', action='store_true', help='initialize the repository-pinned Git submodules')
    parser.add_argument('--install-arm-toolchain', type=Path, metavar='DIRECTORY', help='use the locked ARM installer (supported host only)')
    args = parser.parse_args(arguments)
    settings = settings_for(presets, args.preset)
    try:
        if args.init_deps:
            subprocess.run(['git', 'submodule', 'update', '--init', '--recursive', '--', *MAINTAINED_SUBMODULES], cwd=ROOT, check=True)
        if args.install_arm_toolchain:
            subprocess.run([sys.executable, str(ROOT / 'scripts/ci/install_arm_toolchain.py'), '--install-dir', str(args.install_arm_toolchain)], cwd=ROOT, check=True)
        missing = []
        for command in ('git', 'cmake'):
            if not shutil.which(command):
                missing.append(command)
        if not shutil.which('ninja') and settings.get('CMAKE_C_COMPILER') != 'cl':
            missing.append('ninja')
        compiler = 'arm-none-eabi-gcc' if settings.get('NEXUS_PLATFORM') in ('stm32', 'gd32f470') else settings.get('CMAKE_C_COMPILER', 'cc')
        if not shutil.which(compiler):
            missing.append(compiler)
        try:
            if importlib.metadata.version('kconfiglib') != '14.1.0':
                missing.append('kconfiglib==14.1.0')
        except importlib.metadata.PackageNotFoundError:
            missing.append('kconfiglib==14.1.0')
        if shutil.which('cmake'):
            version = subprocess.run(['cmake', '--version'], capture_output=True, text=True, check=True).stdout
            match = re.search(r'cmake version ([0-9]+)\.([0-9]+)', version)
            if not match or tuple(map(int, match.groups())) < (3, 21):
                missing.append('CMake>=3.21')
        deps = subprocess.run(['git', 'submodule', 'status', '--recursive', '--', *MAINTAINED_SUBMODULES], cwd=ROOT, capture_output=True, text=True, check=True)
        if any(line[:1] != ' ' for line in deps.stdout.splitlines()):
            missing.append('repository-pinned submodules (--init-deps)')
        if missing:
            print('Missing or mismatched prerequisites: ' + ', '.join(missing), file=sys.stderr)
            return 1
        print(f'Prerequisites available for {args.preset}. Build and test with scripts/ci/ci_build.py.')
        print('Compiler availability is not toolchain/board qualification; retain actual build and HIL evidence.')
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        print(f'Prerequisite check failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
