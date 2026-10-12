# Actual U575 assembly fixtures

`empty.toml` and `io.toml` select exact STM32U575ZIT6Q, TZEN0 MSIS4MHz, SRAM1 and
the source-reviewed MB1549 Board. Their explicit FreeRTOS variants additionally
retain the real selected M33 NTZ port. CMake uses the maintained platform graph,
official U575 startup and owned SystemInit; fixture code never replaces providers.

These images retain startup, monotonic time, factory faces, readonly method tables
and real provider methods for actual ELF resource/retention checks. They are
link fixtures, not executed firmware or products. All physical statuses remain
`not_executed`; source/configuration/tool identities and raw compiler artifacts
must be recorded for each execution. No prior CPU-only M33 fixture establishes
U575 chip/Board support, and no archived build count is inherited.
