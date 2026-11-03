# Reviewed exact-part I/O selections

These development fixtures bring reviewed positive route selections into the
maintained capability table and the existing
`providers-multi_controller_assembly` software stage. Each flat TOML is an
explicit ARM build input. The shared C11 main starts the selected platform and
reads each selected public face through a volatile pointer, retaining the real
operations tables and initialized methods through section garbage collection.
The provider checker validates compiler inputs, the exact resolved selection,
CPU ABI, declared image/RAM limits and the post-link method-retention report.
Linking does not execute these operations or establish physical behavior.

The first eleven groups preserve 29 historically authored positive selections
for F103RB, F407ZG, G474RE, GD303RC, GD470ZG, H743ZI and U575ZI. Their software
Board provenance retains the original source revision. The current maintained
TOMLs use local software Board packages; `f407-output.toml` additionally selects
the reviewed ZG GPIOB PB2 output route. Historical execution receipts retain
their original identities and do not qualify the current source.

Five independent F407VE groups cover fourteen additional positive selections:

| Assembly | Selected controller duties |
| --- | --- |
| `ve-word-adc-can.toml` | USART1 word IRQ, ADC1 continuous trigger DMA, CAN1 and CAN2 internal loopback |
| `ve-rx-idle.toml` | USART1 idle RX DMA, EXTI0, GPIOB input and GPIOE input |
| `ve-rx-continuous.toml` | USART1 continuous RX DMA, GPIOA input and GPIOE output |
| `ve-duplex.toml` | USART1 finite duplex DMA and GPIOA output |
| `ve-duplex-continuous.toml` | USART1 continuous duplex DMA |

USART1 modes are mutually exclusive. ADC PA0/PA1 is separate from EXTI0/GPIOA
users; DMA streams and CAN pins are admitted by the normal production resource
graph. GPIOF/G modes reserved by the G474 SoC remain rejected. F407VE GPIOG and
Ethernet selections lack reviewed exact-part routes and remain rejected too.

All assemblies select baremetal, explicit clocks, `Os`, 2 KiB main stack and
zero libc heap. The static RAM limit is 16 KiB for F103RB and 32 KiB for the
other fixtures; all Flash limits are 64 KiB. These limits are checked against
actual linked images. The software Board packages declare no physical
PCB revision, fitted device, voltage, pull, oscillator measurement or external
wiring. They are not physical Board packages and cannot establish HIL, complete
series support, throughput, IRQ latency or product qualification. Firmware
remains C11; host selection regressions use Python's existing unittest domain.

Run the normal provider checker with this source tree, the platform ARM
toolchain and explicit TOML inputs. The formal software plan lists all sixteen
fixtures once in its existing provider stage; no separate discovery mechanism
or Source SDK consumer list is introduced here.
