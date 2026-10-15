# Maintenance scripts

Builds use `python tools/dev/dev.py configure|build|test --preset <name>` and
one assembly JSON. Native CMake arguments pass through to the maintained presets.
Configuration, source SDK, ELF measurement, HIL and artifact sealing tools live in
`tools/` and `cmake/package/`; see [delivery](../docs/delivery/README.md).

`scripts/ci/` owns formatting, backslash Doxygen comments, text hygiene,
Conventional Commits, pinned Actions, analyzers, ARM archive installation and
reviewed vendor-source identity checks. Missing tools and zero scope fail checks.
`scripts/validation/junit.py` verifies fresh actual CTest output; it does not
invent test results. `scripts/tools/format.py` applies the root `.clang-format`
when explicitly requested. Commit hooks check and never silently stage changes.

```sh
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python tools/dev/dev.py configure --preset native-debug
.venv/bin/python tools/dev/dev.py build --preset native-debug --parallel 4
.venv/bin/python tools/dev/dev.py test --preset native-debug
```

The source checkout initializes only three pinned Git dependencies: FreeRTOS,
CMSIS Core and the STM32 device package. The reviewed GD32 SDK import is tracked.
ARM link checks and Native models do not establish physical board qualification.
