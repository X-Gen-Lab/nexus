# Qiming STM32F407ZG core-board schematic reference

This independent minimum-system board has its own STM32F407ZGT6, power supply,
8 MHz HSE and 32.768 kHz LSE. It is not assumed to plug into the separately
drawn V4.1 high-configuration board. `schematic-2020-05-05` identifies the
uploaded drawing; the physical PCB revision remains unknown.

`board.json` records the original PDF SHA-256, sheet and relevant nets. The PDF
itself is not republished. These declarations establish reviewed schematic
connections, not fitted parts, connector continuity or physical qualification.

| Binding | Exact reviewed connection |
| --- | --- |
| `led0`, `led1` | PG13, PG14, active low; initial high before output mode |
| `key0`, `key1` | PF6, PF7, switches to ground; input with explicit pull-up |
| `key0-edge`, `key1-edge` | Alternative EXTI owners of those same switches |
| `uart1` | USART1 PA9/PA10 to the CH340C and download circuit |
| `uart2`, `uart3` | USART2 PA2/PA3; USART3 PB10/PB11 |
| `uart4`, `uart5`, `uart6` | UART4 PA0/PA1; UART5 PC12/PD2; USART6 PC6/PC7 |
| `i2c1` | PB6/PB7, 4.7 kOhm pull-ups; drawn AT24C02 address pins grounded |
| `spi1`, `nor-cs` | SPI1 PB3/PB4/PB5 and PG8 CS, conditional NOR fitting |
| `can1`, `can2` | PB8/PB9 and PB12/PB13, exported without on-board transceivers |
| `usbfs` | PA11/PA12 to J1; explicit declared device connection |
| `lcd-backlight` | PF10 GPIO, initially low for the identified active-high module |
| `touch-clk`, `touch-mosi`, `touch-miso`, `touch-cs` | PA5, PB1, PB2, PB0; mixed GPIO, not a hardware SPI route |
| `touch-pen`, `touch-pen-edge` | PF11 input or alternative EXTI11 owner |

Only D0/LED0 and D1/LED1 are drawn as controlled LEDs. PG15/LED2 is an exported
net, not a third fitted LED. Only S2/PF6 and S3/PF7 are drawn as switches;
PF8/KEY2 is an exported net. The key inputs have no drawn external pull-ups.
Select `pull = "up"` explicitly. The input-only key routes reject output mode.

## Explicit hardware choices

Hardware choices have no implicit selected value. `[hardware]` declares the
reviewed connection intended by one assembly; it is not inspection evidence.

- NOR transport requires `nor_population = "w25q128"` and
  `debug_port = "swd"`. The drawing marks the NOR footprint **unpopulated**.
  Full JTAG/SWO conflicts with PB3/PB4; PA13/PA14 remain reserved for SWD.
- CAN routes require `can1_transceiver = "external"` or
  `can2_transceiver = "external"`. External transceiver type, supply and
  termination still need review.
- USB requires `usb_port = "j1-device"`. No host, HS or automatic VBUS/ID
  capability is inferred from the connector.
- The user-identified ILI9341 16-bit module selects
  `display_module = "ili9341-16bit"`; XPT2046 GPIO connections select
  `touch_module = "xpt2046-gpio"`. Both default to unselected. These GPIO
  bindings do not establish a delivered FSMC/display/touch device driver.

Choose either GPIO or EXTI for a switch or PEN pin. They are alternative owners;
selecting both rejects the duplicate physical pin. IRQ notifications are hints,
not substitutes for the device's authoritative state.

## Minimal source and installed-SDK selection

```sh
python tools/configure/configure.py list-bindings --board stm32f407zg_qiming_core
python tools/configure/configure.py check --assembly boards/stm32f407zg_qiming_core/assemblies/minimal.toml
```

The maintained minimal assembly selects the two actual LEDs, two pulled-up
inputs and USART1. It starts no application worker, watchdog or external memory.
The UART1 byte-event path is also selectable for UART2/3/4/5/6. Additional
controllers expose only their implemented `irq-byte-event` mode; this package
does not imply DMA modes for every UART.

## Display and touch without fitted SRAM

`assemblies/display-touch.toml` selects the ILI9341 NE4 endpoint and XPT2046
GPIO/EXTI transport with backlight initially off. It needs no external SRAM:
ILI9341 has its own GRAM. The FSMC owner claims only twenty LCD bus pads;
NE3 and SRAM-only address/byte-lane pads remain unselected.

`assemblies/display-touch-sram.toml` preserves the explicit LCD/touch plus
IS62WV51216 CPU-window reference: forty-one FSMC pads and an authored
`sram_population = "is62wv51216"` declaration.
The core schematic marks this SRAM footprint DNP; the explicit combined
profile requires actual fitting before execution.
`sram-window.toml` remains the separate forty-pad SRAM-only runtime profile.
None of these selections establishes physical module continuity or HIL.

`display-touch.toml` 仅选择 ILI9341 的 NE4 并口与 XPT2046 GPIO／EXTI，背光初始
关闭。ILI9341 自带 GRAM，不要求外部 SRAM；FSMC 仅占用 20 个 LCD 总线引脚，
不选择 NE3 或 SRAM 专用引脚。`display-touch-sram.toml` 显式保留 LCD／触摸加
SRAM 的 41 引脚组合，要求声明并安装 IS62WV51216；核心板原理图将此 SRAM 标为 DNP，使用组合配置前必须确认实际安装。
`sram-window.toml` 继续作为独立的 40 引脚运行时 SRAM 参考。这些配置不是实板验收。

## Explicit RTC backup ownership

`assemblies/rtc-adopt.toml` selects the actual Y1 32.768 kHz PC14/PC15
oscillator route and polling Alarm A. It requires an external boot owner to
prepare the exact LSE crystal, RTC clock, DBP access, locked write protection,
BCD 24-hour calendar, BYPSHAD and PRER 127/255 profile before Platform start.
An unprepared power-on board rejects this `adopt-retained` profile; lookup never
provisions a date or resets the backup domain.

To initialize an empty reset-like backup domain, author `provision-empty` and
an explicit `initial` table with all seven raw `year`, `month`, `day`, `weekday`,
`hour`, `minute` and `second` fields. Year is 00..99; weekday is independent of
the date. Both policies require explicit `allow_set`, `timeout_us` and
`poll_limit`. `allow_set = false` keeps later calendar replacement closed.
The C provider checks PC14/PC15 electrical admission without rewriting GPIO.
Their exclusive resource claims prevent another selected pin owner even though
they receive no ordinary GPIO snapshot or restore operation.

Provisioning leaves autonomous backup clocks/calendar effects running. Public
alarm stop can succeed while private Platform close returns `BUSY`; the
remaining ownership requires a reset or a future explicit handoff, not forced
clock removal. The battery is marked unpopulated, so neither policy establishes
power-off retention. No Alarm B, wakeup timer, tamper, timestamp, calibration,
IRQ wake or physical qualification is implied.

## Explicit external SRAM startup placement

`assemblies/sram-startup.toml` declares a fitted IS62WV51216 on FSMC NE3 and
selects `startup-memory` plus a separate 4 KiB placement budget. The source
schematic marks SRAM unpopulated: this is an opt-in software reference, not
proof that SRAM exists on a particular board. Its encoded read/write timings
are declared assumptions for HCLK up to 168 MHz. Verify actual part suffix,
speed grade, power, fitting and electrical timing before execution.

The selected `NX_MEMORY_ADDSRAM_DATA`, `NX_MEMORY_ADDSRAM_BSS` and
`NX_MEMORY_ADDSRAM_NOINIT` variable attributes place objects inside the 1 MiB
NE3 window at 0x68000000. A read-only Flash boot plan validates complete primary
RAM, optional CCM and external section bounds before reset-time pin changes.
It references the same NE3 timing object later adopted by the runtime owner.
DATA is copied from internal Flash, BSS is cleared and NOINIT is untouched.
Initialize NOINIT explicitly before reading it; no physical retention is
promised. Internal SRAM still holds the MSP, heap and ordinary objects; code
and vectors stay in internal Flash. External SRAM receives no DMA permission.

This first profile requires a full reset-address baremetal image and at least
512 bytes of internal MSP. The maintained non-LTO compiler profiles have a
source-bound static bootstrap stack proof; 512 is an admission bound, not a
measured peak or a bound for nonmaskable faults. Offset/layout images, image
prefix/suffix reservations, MPU placement and startup LTO are refused. Optional
CCM uses its existing initialization path once.

Placed globals retain the bank and forty SRAM pads for their complete lifetime.
Platform stop therefore returns `BUSY`. Read-only memory-window geometry marks
`startup_backing`; whole-bank runtime acquire returns `UNSUPPORTED`, preventing
an aliasing lease over globals. Use `cpu-window` without an addsram placement
budget when a caller-owned runtime bank lease is required instead.

## Remaining physical and advanced boundaries

SRAM IS62WV51216 and the backup battery are also marked unpopulated. No external
SRAM is automatically added to the linker or startup initialization plan.
Complete audio and camera support remain separate
implementation work, not capabilities created by this Board declaration.

LCD reset is tied to global NRST. There is no independent LCD GPIO reset.
Some FSMC address nets also carry exported wireless control labels. Review the
selected combination before attaching modules. TF card detect is not wired to
a MCU pin. Core P4 SWC/SWD labels differ from U1 JTCK/JTMS labels; confirm the
CAD netlist or continuity. Both USB connectors share VCC5 without drawn source
isolation, so fixture supply selection must be explicit.

Physical HIL is `not_executed`; no waveform, throughput, power, reset, retention
or fitted-module result is implied by resolver acceptance or an ARM link.
