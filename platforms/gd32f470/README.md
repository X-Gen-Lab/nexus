# GD32F470ZGT6 / Liangshan Pi platform

This platform implements the GD32F470ZGT6 on JLC's Liangshan Pi board. It is
separate from STM32 and from the historical, blocked GD32F407 target. The user
changed the requested GD32 device from F303 to this physical board; no F303
register map, startup vector, UID address or Flash geometry is reused.

## Boundaries and build

- `arch/cortex_m4` owns CPU interrupt state and barriers.
- `soc/gd32f470` owns the clock transition, interrupt vectors, dedicated
  timestamp timer, silicon identity and physical internal Flash port.
- `boards/gd32f470_liangshan` owns alternate functions, CS/DE, external-device
  wiring and initial idle levels. `nexus_board.h` describes the heartbeat LED.
- `soc/gd32f470/controllers` implements Nexus GPIO, UART and SPI contracts;
  `soc/gd32f470/sdk` owns the selected private SDK compilation context.
- `platforms/gd32f470/src` owns startup/lifecycle and assembles SoC/Board objects
  through the common `nexus_forward_component_objects()` helper.
- External applications own main, workers, scheduler, partitions and recovery.
  They consume `Nexus::Firmware` and bootstrap common `Nexus::Runtime`.

The implementation uses the original **official GD32F4xx Firmware Library
3.3.3**, with source/archive hashes and licenses in
`vendors/gigadevice/gd32f4xx/source.lock.json`. Configuration verifies every
imported file. The GCC startup is the official `startup_gd32f450_470.S`.
`Nexus::GD32SDK` is an explicit SDK opt-in; ordinary platform consumers do not
inherit vendor headers. Only required SDK peripheral translation units are
compiled. Drivers have no allocation and fixed, configuration-bounded pools.

With the locked ARM GNU 14.3 toolchain and build dependencies available:

```sh
cmake --preset gd32f470-armgcc-baremetal-debug
cmake --build --preset gd32f470-armgcc-baremetal-debug
```

The four supported presets vary `baremetal`/`freertos` and `debug`/`release`.
The Cortex-M4F port uses `fpv4-sp-d16` with hard-float ABI. The maintained board
clock is 25 MHz HXTAL, 200 MHz CPU, 50 MHz APB1, 100 MHz APB2; USB's 48 MHz clock
is not provided. Oscillator/PLL/voltage stability waits are bounded and failures
propagate. Board CS/DE idle levels are established before changing the clock.

## Physical resources

| Resource | Binding |
| --- | --- |
| LED2 | PD7, active high, device name `GPIOD7` |
| Console | USART0 PA9 TX / PA10 RX, AF7, `UART0`, 115200 8N1 default |
| Onboard W25Q64 | SPI4 PF7 SCK / PF8 MISO / PF9 MOSI, AF5, logical CS0 PF6, `SPI4` |
| External RS485 | Optional PB1 active-high DE; disabled by default; stock board has no onboard transceiver |
| Timestamp timebase | Reserved 32-bit TIMER1, 1 MHz; overflow IRQ extends it to 64 bits |
| Scheduler | SysTick; FreeRTOS 1 kHz tick, Cortex-M4F port |
| IRQ priorities | TIMER1 5, USART0 6, SysTick/PendSV 15; four preemption bits |
| Identity | UID `0x1FFF7A10`; density word `0x1FFF7A20` (Flash high16 / SRAM low16, KiB) |

The ZGT6 has **1 MiB Flash and 512 KiB SRAM**. Main SRAM (192 KiB) is the default
allocation region. Additional SRAM at `0x20030000` (256 KiB) and TCM at
`0x10000000` (64 KiB) are explicit unused linker regions. No DMA reachability or
startup initialization for arbitrary extra sections is promised. PCB revision
is a fixture/manufacturing input, not a fabricated silicon observation.

The physical Flash is `0x08000000..0x08100000` (1 MiB), exposed as 256
independently erasable **4 KiB pages**. Default firmware uses all of it and
creates no storage reservation. External `NEXUS_FLASH_LAYOUT_FILE` selects
consumer-owned regions and image bounds; image offset must remain zero.
The independent page erase is GD32F470's additional facility, not 128 KiB
sector erase.
Official UM Rev3.3 section 2.3.4 specifies `FMC_PEKEY`, `FMC_PECFG.PE_EN` and a
4 KiB-aligned address with `FMC_CTL.SN=0`; the locked SDK's
`fmc_page_erase()` implements it under the GD32F470 conditional. The port never
calls sector erase. It validates the physical Flash density and linker fence,
requires aligned 2-byte program units, rejects forbidden 0-to-1 changes and
verifies erased/programmed contents. The generated linker validates declared image/region separation. External
products serialize the synchronous Flash port and use a maintenance window: same-bank Flash operations may stall instruction/IRQ fetch and are not
safe during active control deadlines.

## Runtime contracts

UART implements one borrowed TX buffer and a monotonic ticket. A task polls to
enforce its finite deadline; a ticket is terminal only after the true TC flag
or a synchronous controller reset. The final DATA write cannot consume an
older sampled TC. Successful cancel settles IRQ/buffer ownership before DE
falls. Reset may truncate the frame; the RX stream receives an ordered
controller-reset error (raw bit31) instead of silently losing a byte. RX
records include ordered bytes/errors/overflow and a 1 microsecond timestamp
**sampled at IRQ observation**, which is not a start-bit capture. Ring capacity
is 128 by default. Sync TX rejects an already masked interrupt context; legacy
async-TX and sync-RX getters return NULL. UART DMA is not implemented.

SPI implements copied configuration and caller-owned, generation-protected
handles. Four device slots are available by default, with one queued request
per slot. All stock bindings address logical CS0; multiple handles can select
different modes/rates, while controller access remains exclusive. Polling uses
one total deadline; timeout, cancellation and I/O failure reset the shifter
before releasing CS and buffers. A queued request's deadline includes queue
wait, and a dedicated task calls `service()` to deliver exactly one terminal
callback. Cancel requests retain queued buffers until that callback. Lifecycle
and close reject active requests and callbacks. Generations survive reinit and
exhaustion fails closed. Requested rate is a ceiling; supported rates are
390625..50000000 Hz, rounded down by dividers 2..256. This backend has no DMA,
ISR transactions, hidden worker or legacy handle adapters.

The GPIO driver implements LED read/write/toggle and lifecycle. EXTI and power
interfaces return unsupported/NULL. Peripheral features outside the selected
GPIO/UART/SPI/timebase/Flash profile are not advertised as implemented.

## Validation boundary

All four real ARM presets compile and link with the official startup/SDK.
`tests/drivers/gd32f470` runs the production controller/Flash/timebase sources
against test-only faulting vendor models; it is also in the general driver
CTest suite. The fixtures test UART TC/cancel/loss/deadline/overflow, SPI
ownership/queue/deadline/drain, TIMER1 pending-wrap handling, silicon density
and independent Flash page boundaries. Flash fixtures verify independent page geometry, rejected bounds and retained
canaries outside the explicit test region; they do not establish a default
product partition. Selected sanitizer
options instrument these tests through the shared build options target.

**No physical board or hardware fixture was available for this implementation.**
Startup oscillator/voltage behavior, sleep/timebase behavior, actual pin mux,
UART error injection and DE timing, SPI waveform/flash JEDEC identity, Flash
power interruption, IRQ latency and control-cycle jitter require retained
board-identified HIL evidence before a production release. A host model, linker
map or successful cross-build does not establish those electrical properties.
The heartbeat has no MCU stdio transport; newlib nosys stubs are not a console
backend. TIMER1 must remain running and overflow servicing must not be blocked
for its entire 71-minute wrap period.

## Primary references

- [Official F470ZGT6 selector](https://www.gigadevice.com/product/mcu/mcus-product-selector/gd32f470zgt6)
- [GD32F470xx datasheet Rev2.2](https://www.gd32mcu.com/data/documents/datasheet/GD32F470xx_Datasheet_Rev2.2.pdf)
- [GD32F4xx user manual Rev3.3](https://www.gd32mcu.com/data/documents/userManual/GD32F4xx_User_Manual_Rev3.3.pdf): identity 1.6, independent page erase 2.3.4, timer width chapter 18
- [Official SDK catalog](https://www.gd32mcu.com/en/download?kw=GD32F4xx)
- [First-party Liangshan board wiki](https://wiki.lckfb.com/zh-hans/lspi/)
- [LED wiring](https://wiki.lckfb.com/zh-hans/lspi/beginner/led-lib.html)
- [USART0 binding](https://wiki.lckfb.com/zh-hans/lspi/beginner/uart.html)
- [SPI4/W25Q64 binding](https://wiki.lckfb.com/zh-hans/lspi/beginner/spi-flash.html)
