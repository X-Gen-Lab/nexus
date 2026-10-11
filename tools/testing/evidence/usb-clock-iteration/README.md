# USB clock development evidence

This bundle executes the real selected SoC clock implementations. The external
CPU context, clock snapshots and MMIO stores are host fault boundaries; no clock
implementation is mocked. It is a development snapshot, not clean-source SDK,
full firmware, USB electrical or physical timing qualification.

- `usb-api-red.command.json` records the actual missing private API compile RED.
- `*-boundary-red.xml` records one failed source-readiness boundary regression
  per family. The gate store can lose crystal readiness before start returns;
  both providers now validate the actual final source and gate before success.
- `*-phase-red.xml` records two actual failed cleanup-owner cases per family.
  Failed acquisition or stop cannot admit a consumer after source recovery.
- `*-phase-green.xml` records all 11 STM32 and 16 GD32 cases, without filtering
  or skipped cases. Each command JSON retains the actual argv and exit code;
  its original output is the adjacent `.log` file.

Focused commands used the isolated external driver:

```text
python /workspace/nexus-usb-clock-iteration/build.py stm usb-phase-stm-green-build
python /workspace/nexus-usb-clock-iteration/build.py gd usb-phase-gd-green-build
python /workspace/nexus-usb-clock-iteration/run.py usb-phase-stm-green /workspace/nexus-usb-clock-iteration/stm-test --gtest_output=xml:tools/testing/evidence/usb-clock-iteration/stm-phase-green.xml
python /workspace/nexus-usb-clock-iteration/run.py usb-phase-gd-green /workspace/nexus-usb-clock-iteration/gd-test --gtest_output=xml:tools/testing/evidence/usb-clock-iteration/gd-phase-green.xml
```

The pinned GoogleTest/GoogleMock libraries come from the focused development
build. The maintained integration target is `tests/google/usb_clock.cmake`.
Firmware remains C11 and does not link the framework or these host fixtures.

`usb-*-arm-final.command.json` compiles each real production unit with the pinned
SDK register headers and GNU Arm 14.3, Cortex-M4 hard-FPU ABI, `-Os` and strict
owned C warnings. `usb-arm-nm.log` measures caller state as 8/16 bytes. Actual
object `.text` plus immutable ops `.rodata` totals 444/1000 bytes; both units
have zero global `.data`/`.bss`. These are object costs, not whole firmware or
cycle measurements. Unselected zero cost still requires selected source graph
and final ELF/GC verification in the USB assembly delivery.

STM32 validates the established 8 MHz/M8/N336/Q7 profile and only owns the USBFS
gate. GD32 keeps the reviewed 200 MHz main PLL unchanged: UM Rev3.3 §4.3.24 and
§4.3.26 give the independent PLLSAIN192/P4 and CK48 mux route, using the existing
25 MHz/M25 input. The locked SDK provides the actual masks. Crystal tolerance,
PLL lock latency and USB signaling remain physical tests **not executed**.
The official User Manual PDF SHA-256 is
`c8e1d20d7cde338892d08dcb233345e03f7960256a60a36c755c576d9cd65c13`.

Exclusive authorization is a resource-owner contract. Enabled peer gates are
checked before GD32 source changes; clock-gated external owners must also be
quiescent. The lease has no discovery registry. Controller release follows
actual USB/IRQ detachment; decrementing a reference is not that proof. A failed
drain retains clock control for explicit stop retry. All waits have finite Core
deadlines and a poll bound for a stalled clock.

`development.json` binds source, original logs/reports and retained objects.
`types.c.raw` preserves the original external layout fixture bytes; its actual
historical compile argv remains unchanged. The raw fixture is evidence data,
not a maintained production source or a new configuration authority.
