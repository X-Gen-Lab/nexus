# Maintained source SDK references

These authored TOML files are published with the source SDK. Each file selects
one explicit Board, backend and resource budget. Products keep their own
assemblies outside this repository.

The `nucleo-h743-*` references select STM32H743ZIT6 on NUCLEO-H743ZI2,
fixed HSI64 and Cache off. The `nucleo-u575-*` references select STM32U575ZIT6Q
on NUCLEO-U575ZI-Q, fixed MSIS4, with externally established TZEN=0 and
SWAP_BANK=0. `empty` selects no I/O; `uart` selects the reviewed LED and
IRQ-byte console route. The H743 console remains USART3 on PD8/PD9; the U575
console remains USART1 on PA9/PA10. Baremetal and FreeRTOS are separate files.
These measurement references explicitly name their instances `led0` and
`uart3` or `uart1` for the bundled linkage workload. Instance IDs belong to the
authored assembly; external SDK consumers may choose different names.

These are software references, not physical HIL qualification. Board revisions,
power, solder bridges, option bytes and silicon revisions require actual station
observations. A reference does not enable DMA, cache coherency, a secure image
pair, dynamic clocks or STOP modes. The authored selection and its actual linked
image determine support; filenames do not grant additional capabilities.

For example, configure a UART linkage workload with
`NEXUS_ASSEMBLY_FILE=tools/configure/assemblies/nucleo-h743-baremetal-uart.toml`
and `NEXUS_WORKLOAD=uart`. Use an absolute assembly path when configuring from
another working directory. Firmware tests still need the matching ARM toolchain.
Offline qualification separately requires two fresh network-disabled builds
from a complete publishable SDK and retains both original ELF/BIN/map outputs.
