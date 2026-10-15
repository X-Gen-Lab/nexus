Debugging Contracts and Firmware
================================

Start from an exact source/config/Board/backend and retain the actual build log,
generated bundle, ELF/map and test enumeration. A different source or stale
configuration cannot be debugged as the same delivery identity.

Host and Firmware
-----------------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
   arm-none-eabi-gdb build/stm32-armgcc-debug/bin/nexus_contract_firmware.elf

The GDB command opens the already-built ELF for inspection; it does not connect
to a probe. Actual application ELF belongs to the external consumer build, not
an in-tree ``applications`` path. For MCU build/static checks use the named
preset and ``scripts/ci/validate_firmware_elf.py`` before any hardware stage.

Diagnose Ownership
------------------

Read Runtime/HAL OFFLINE/PARTIAL/READY and original/cleanup/restore status. READY
is infrastructure only. BUSY may indicate device children/regions/operations,
OSAL waiters/objects, running kernel or unquiesced IRQ/DMA. Cleanup error retains
owners; HAL PARTIAL keeps an admission fence and blocks new acquisition. Original
owners may settle/recover and retry; do not discard references or borrowed buffers.

Typed provider operations reject ISR/active incoming masks before side effects.
Check task/scheduler/backend capability, priority rules and original finite
budgets. UART wire TC differs from memory completion; SPI cancellation acceptance
is not terminal settlement. Native capture/time and faulting register models do
not establish hardware timing.

Hardware is Deferred
--------------------

No board/probe/serial/power operation has been executed for this delivery.
A later stage needs observed PCB/chip/probe/port identity, exact ELF-BIN digest,
reviewed bounded commands and cleanup/lease evidence. Missing station/template
values cannot become passing qualification. See the HIL readiness documentation.
