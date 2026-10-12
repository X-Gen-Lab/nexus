# Development setup

Use Python 3.11+, CMake 3.31.6, Ninja 1.13.2 and a C11 compiler. Install the
repository tools and both local Git hooks for each clone:

```sh
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
git submodule update --init --recursive ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4
python tools/dev/dev.py doctor
```

The installer preserves existing hooks using pre-commit's migration and rejects
an explicit `core.hooksPath`. CI uses `--skip-hooks` and executes the same checks.
Normal setup uses `install --install-hooks` to prepare the isolated Python
environments before the first commit, including when the outer tools already
match their pins. The TDD environment declares exact pre-commit, clang-format
and PyYAML versions for its four complete tool suites and real hook fixtures.
The contract checks these pins against the development and workflow locks.
Offline setup may use `PIP_NO_INDEX=1` and `PIP_FIND_LINKS` pointing to the checked
wheel export; this also applies when pre-commit prepares its isolated environment.
The export includes `dependencies/hook-build-tools.txt` for the local hook's
isolated build backend, in addition to the runtime tool requirements.
The root `.clang-format` and `.editorconfig` remain authoritative. Versions are
locked in `dependencies/development-tools.txt` and `environment-tools.txt`.

ARM uses GNU 14.3.rel1 with a hash-verified archive:

```sh
python scripts/ci/install_arm_toolchain.py --install-dir /your/toolchains/arm
```

Add that directory's `bin` to PATH. Use one maintained preset through
`python tools/dev/dev.py configure|build|test`. No dependency download occurs
during firmware configuration. Formal offline builds use the observed sealed
OCI environment; mutable host checks have a separate scope.
