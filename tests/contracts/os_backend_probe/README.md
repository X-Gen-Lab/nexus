# Backend onboarding probe

`native.json` is the executable example for
[the backend integration contract](../../../docs/design/os-backend-integration.md).
It selects existing real Native GoogleTest contracts. It declares no new kernel
support and performs no runtime backend registration.

Configure through the normal development entry point, then execute a fresh probe:

```sh
python tools/dev/dev.py configure --preset native-debug
python scripts/ci/os_backend_probe.py \
  --contract tests/contracts/os_backend_probe/native.json \
  --build-dir build/native-debug \
  --target nexus_os_wait_test \
  --output build/backend-probe-native-001
```

The output must not exist. Reruns use a different directory; previous evidence is
never deleted or overwritten. Add multiple `--target` arguments when the reviewed
contract spans several registered Google targets. Each selected target is fully
executed without case filtering. The runner checks fresh XML and exact discovered
identities, then measures its actual ELF with GNU `size`, `nm` and `readelf`.

For an MCU backend, supply these additional arguments against an already
configured production firmware build:

- `--firmware-build <build-directory>`
- `--firmware-target <reviewed-cmake-target>`
- `--firmware <linked-elf-inside-that-build>`
- `--cpu-resolution <generated-resolved-cpu.json-or-resolved.json>`
- `--compiler <arm-none-eabi-gcc>`

The existing CPU/backend resolver must already recognize the reviewed integration.
This tool cannot accept an unregistered RTOS by introducing another configuration
file or source graph. Kernel declarations include a source lock path within the
checkout, required retained static API symbols and an actual scheduler case group.
No placeholder declaration for another RTOS is provided.

`report.json` retains command exit codes, raw output digests, source/build/tool
identities, actual case records and ELF costs. Host measurements carry
`host_elf_not_mcu_budget`; MCU link measurements carry
`linked_mcu_elf_not_physical_timing`. Neither scope establishes board behavior,
real cycle counts, clean-source candidate qualification or product acceptance.
Run `python -m unittest discover -s scripts/ci -p test_os_backend_probe.py` for
the declaration, registration, coverage, CPU/ABI and resource rejection contracts.
