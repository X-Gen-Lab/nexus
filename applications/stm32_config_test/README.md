# STM32F407 configuration display

This application uses the STM32F4DISCOVERY MB997 reference profile and LD4 on
PD12. It initializes the shared platform clock and NVIC once, displays configured
values alongside observed SYSCLK, silicon identity, Flash capacity and AIRCR
priority-group encoding, then blinks the LED and prints a summary every 5 seconds.

It is a read-only diagnostic example. It does not erase/program Flash, run a
storage journal, test power-fail recovery or certify any hardware configuration.
Physical PCB revision and board behavior require a separate HIL report.

## Build

A complete checkout, pinned submodules, Python Kconfig dependencies, CMake,
Ninja and an ARM GNU embedded toolchain are required. Configuration is generated
inside the selected build directory.

```sh
cmake --preset stm32-armgcc-debug
cmake --build --preset stm32-armgcc-debug --target stm32_config_test
```

The FreeRTOS configuration also builds this diagnostic application:

```sh
cmake --preset stm32-armgcc-freertos-debug
cmake --build --preset stm32-armgcc-freertos-debug --target stm32_config_test
```

This application's main loop uses the HAL timebase before starting a scheduler.
Use the generic `blinky` application to exercise OSAL task creation and scheduler
startup under the FreeRTOS profile.

## Observe

The ELF, BIN, HEX and map files are emitted to the selected build directory's
`bin` folder. Program only the reference board matching the effective profile.
Observe PD12 toggling every 500 ms. `printf` requires the product's configured
UART/ITM/debug transport; merely enabling a UART in Kconfig does not retarget
standard output. Without a transport, inspect identity, clock and NVIC through
the debugger.

Kconfig priority group `4` means four preemption bits, whereas the observed
CMSIS AIRCR encoding for that group is `3`. They are deliberately displayed as
separate values. Clock fallback and source tolerances still need physical
measurement; reporting a configured frequency is not such a measurement.
