# Qiming STM32F407ZG high-configuration V4.1 schematic reference

This is an independent STM32F407ZGT6 board with its own power supply, 8 MHz HSE
and 32.768 kHz LSE. It is not assumed to be a carrier for the separately drawn
core board. `V4.1` records the supplied file identity; the drawing title-block
revision and actual PCB revision remain unconfirmed.

`board.json` binds the uploaded three-sheet PDF by SHA-256 and records reviewed
MCU connections. The original PDF is not republished. Declarations are not proof
of fitting, connector continuity, electrical compatibility or physical support.

| Binding | Exact reviewed connection |
| --- | --- |
| `led0`, `led1`, `led2` | PE3, PE4, PG9, active low; initial high before output mode |
| `key0` through `key3` | PF9, PF8, PF7, PF6, switches to ground; explicit input pull-up |
| `key0-edge` through `key3-edge` | Alternative EXTI owners of those same pins |
| `uart1`, `uart3` | USART1 PA9/PA10; USART3 PB10/PB11 |
| `uart2`, `rs485-de` | USART2 PA2/PA3 through P15 and PG6 DE plus active-low RE |
| `uart4` | UART4 PA0/PA1 through the declared WiFi selection |
| `uart5`, `uart6` | UART5 PC12/PD2; USART6 PC6/PC7 |
| `i2c1` | PB6/PB7 and drawn AT24C02 with grounded address pins |
| `spi1`, `nor-cs` | SPI1 PB3/PB4/PB5 and PG8 CS |
| `can1`, `usbfs` | Alternative P21 connections on PA11/PA12 |
| `can2` | PB12/PB13 through its declared P15 selection |
| `lcd-backlight` | PF10 GPIO, initially low for the identified active-high module |
| `touch-clk`, `touch-mosi`, `touch-miso`, `touch-cs` | PA5, PB1, PB2, PB0; mixed GPIO rather than hardware SPI |
| `touch-pen`, `touch-pen-edge` | PF11 input or alternative EXTI11 owner |

The four directional keys lack drawn external pull-ups. Select input and
`pull = "up"`. The center key is global NRST, not a fifth GPIO key. GPIO and
EXTI alternatives cannot own the same physical switch or PEN pin together.

## Explicit hardware choices

No hardware option has an implicit selected value. One authored `[hardware]`
table declares the intended connection; it does not prove actual jumper state.

- `p21 = "usb"` and `p21 = "can1"` select incompatible PA11/PA12 paths.
- `p15 = "rs485"` selects UART2 and PG6 direction control;
  `p15 = "can2"` selects the CAN2 path. Check real jumper placement.
- `p5 = "wifi"` permits UART4. The Ethernet/WiFi pins overlap. The original
  Ethernet MDIO PA2 also conflicts with the default USART2 RS485 TX route.
- NOR SPI transport requires `nor_population = "fitted"` and
  `debug_port = "swd"`. The W25Q128 heading conflicts with the symbol's
  64 Mbit note: exact expected JEDEC identity and geometry require separate
  reviewed device configuration. Fitting never selects a model automatically.
- The identified panel uses `display_module = "ili9341-16bit"`; its XPT2046
  connection uses `touch_module = "xpt2046-gpio"`. These choices gate GPIO
  connections only and do not assert complete display/touch device support.

PA13/PA14 remain reserved for SWD. Full JTAG/SWO conflicts with NOR PB3/PB4.
The bare GPIO backlight and buzzer pads do not establish hardware PWM support.
LCD reset is global NRST, not an independently controlled GPIO.

## Minimal source and installed-SDK selection

```sh
python tools/configure/configure.py list-bindings --board stm32f407zg_qiming_v41
python tools/configure/configure.py check --assembly boards/stm32f407zg_qiming_v41/assemblies/minimal.toml
```

The minimal assembly selects the three actual LEDs, four pulled-up keys and
USART1. No application task, watchdog, PHY, NOR or external memory is started.
All six UARTs have the maintained byte-event selection after exact hardware
conditions; the additional UARTs do not acquire unsupported DMA modes.

## Display and touch without fitted SRAM

`assemblies/display-touch.toml` selects the ILI9341 NE4 endpoint and XPT2046
GPIO/EXTI transport with backlight initially off. It needs no external SRAM:
ILI9341 has its own GRAM. The FSMC owner claims only twenty LCD bus pads;
NE3 and SRAM-only address/byte-lane pads remain unselected.

`assemblies/display-touch-sram.toml` preserves the explicit LCD/touch plus
IS62WV51216 CPU-window reference: forty-one FSMC pads and an authored
`sram_population = "is62wv51216"` declaration.
The combined profile requires verified fitting; neither the drawing nor
resolver acceptance proves the installed SRAM or its exact suffix.
`sram-window.toml` remains the separate forty-pad SRAM-only runtime profile.
None of these selections establishes physical module continuity or HIL.

`display-touch.toml` 仅选择 ILI9341 的 NE4 并口与 XPT2046 GPIO／EXTI，背光初始
关闭。ILI9341 自带 GRAM，不要求外部 SRAM；FSMC 仅占用 20 个 LCD 总线引脚，
不选择 NE3 或 SRAM 专用引脚。`display-touch-sram.toml` 显式保留 LCD／触摸加
SRAM 的 41 引脚组合，要求声明并安装 IS62WV51216；组合配置要求确认实际安装；原理图和配置通过均不证明 SRAM 已安装或其精确后缀。
`sram-window.toml` 继续作为独立的 40 引脚运行时 SRAM 参考。这些配置不是实板验收。

## Explicit RTC backup ownership

`assemblies/rtc-adopt.toml` selects Y1 32.768 kHz on PC14/PC15 and polling
Alarm A. It deliberately requires an externally prepared retained LSE crystal,
RTC clock, DBP access, locked write protection, BCD 24-hour BYPSHAD calendar and
PRER 127/255 profile. A fresh unprepared board rejects `adopt-retained`.
Factory lookup never initializes hardware, supplies a date or resets backup.

`provision-empty` is a separate authored policy. It requires reset-like empty
backup state and an explicit `initial` table with raw `year` (00..99), `month`,
`day`, independent `weekday`, `hour`, `minute` and `second`. Every policy needs
explicit `allow_set`, finite `timeout_us` and finite `poll_limit`. The supplied
adoption profile forbids later calendar replacement. The C provider checks
oscillator pads without GPIO changes; their resource claims remain exclusive
while ordinary GPIO snapshot/restore excludes PC14/PC15.

Provisioning retains autonomous backup clock/calendar effects. Alarm stop does
not release that persistent duty: private Platform close reports `BUSY` until
reset or a future explicit ownership handoff. This is not a battery fitting or
retention guarantee. Alarm B, WUT, tamper, timestamp, calibration, IRQ wake and
power-off continuity remain outside this software profile.

## Explicit external SRAM startup placement

`assemblies/sram-startup.toml` explicitly selects an IS62WV51216 NE3 bank as
`startup-memory` and gives addsram its own 4 KiB placement budget. This selection
declares fitting and encoded read/write timing; it does not establish the actual
device suffix, speed grade, supply or physical timing. The supported window is
0x68000000..0x68100000. The chip's physical internal-memory facts remain separate
from this selected external region.

Use `NX_MEMORY_ADDSRAM_DATA`, `NX_MEMORY_ADDSRAM_BSS` and
`NX_MEMORY_ADDSRAM_NOINIT` for static variables. Reset validates a complete
immutable Flash boot plan before changing pads, using the same NE3 descriptor
object later adopted by the FSMC runtime owner. It copies external DATA, clears
external BSS and leaves NOINIT untouched. Primary objects, MSP and heap remain
internal; code and vectors remain in Flash. Optional CCM keeps its existing
initialization path once. No external DMA or power-off retention is granted.

The selected profile requires reset-address baremetal, a minimum 512-byte
internal MSP and a maintained non-LTO optimization profile. That minimum comes
from source-bound static bootstrap stack analysis with margin, not measured
peak or nonmaskable-fault coverage. Startup LTO, offset/layout images, image
prefix/suffix reservations and MPU placement require separate proofs and are
refused by this profile.

The bank and its forty SRAM pads persist while placed globals can be accessed.
Platform stop returns `BUSY`; read-only geometry reports `startup_backing` and
whole-bank runtime acquire returns `UNSUPPORTED`. Choose `cpu-window` without
an addsram budget for a separate runtime lease. Factory lookup itself never
changes memory, supplies timings or initializes hardware.

## Conditions before physical or advanced acceptance

MAX485 and MAX232 symbols show 3.3 V supplies although the standard named parts
specify 5 V. Verify the actual compatible part markings, supply and signal
levels. RS485/CAN termination is not software-switchable in this drawing.
Transceiver-relative `485_TX`/`485_RX` labels must not be mistaken for MCU
TX/RX: inspect P15's actual RO-to-RX and DI-to-TX paths.

The DP83848 oscillator frequency is unspecified; P5's MDIO net spelling differs
between sheets. The shown RX_DV signal is an optional RMII output, not proof
of standard CRS_DV wiring. PHY mode/address, 50 MHz reference, receive-valid
compatibility and continuity require review; no V4.1 Ethernet binding is
admitted by this initial package.

The TF socket has no connected MCU card-detect pin. Its SDIO pads overlap camera
and UART5 paths. Camera and complete audio providers
remain separate implementation work. The module headers do not prove fitted
OV2640, WM8978/VS1053, WiFi/Bluetooth, DS18B20 or HS0038 variants. The USB_HS
PB14/PB15 connector has an internal FS PHY path without ULPI; no 480 Mbps or
host capability is inferred.

Physical HIL is `not_executed`. Resolver acceptance, model tests and an ARM link
do not establish electrical, waveform, throughput, power or reset qualification.
