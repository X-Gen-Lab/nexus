# Maintenance scripts

Builds use `python tools/dev/dev.py configure|build|test --preset <name>` and
one schema-2 TOML assembly. Reviewed SoC and Board facts remain JSON. Native
CMake arguments pass through to the maintained presets.
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

The checkout has fourteen pinned Git submodules: FreeRTOS, ThreadX, GoogleTest,
Arm CMSIS Core and five pairs of ST device/HAL packages. The reviewed GD32F30x
and GD32F4xx SDK imports are tracked with per-file locks. The current gitlinks
and [dependency policy](../dependencies/platform-compatibility.json) own their
identity; directory presence does not enable a platform or mode.
ARM link checks and Native models do not establish physical board qualification.
