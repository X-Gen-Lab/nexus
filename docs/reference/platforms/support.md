---
id: docs-reference-platforms-support
type: reference
audience: consumer
source_language: en
---

(docs-reference-platforms-support)=
# Read exact platform support

Support has five separate dimensions: chip fact, maintained provider/mode,
reviewed Board route, executed software scope and physical station scope.
The [derived capability matrix](../../delivery/capabilities.md) owns mode/route/
fixture/source associations. The [support record](../../delivery/support.md) owns
current detailed limits; this page explains how to select them.

| Platform | Maintained selection and essential boundary |
| --- | --- |
| Native | Independent host models, POSIX behavior and real selected RTOS host tests; no MCU qualification |
| F407ZG Qiming / F407VE Sky | Exact M4F parts and separate Board facts; broad explicit F407 modes, baremetal/FreeRTOS and reviewed ThreadX M4 subset |
| F470ZG Liangshan | Exact GD32 M4F provider and Board; broad explicit modes, baremetal/FreeRTOS and reviewed ThreadX M4 subset |
| F103RB / GD303RC references | Exact legacy GPIO/UART/SPI plus finite channel-DMA UART, IRQ-message I2C, physical Flash, single ADC and fixed PWM; finite RX has rearm gaps and I2C priority-zero IRQs cannot notify a kernel |
| G474RE source reference | GPIO, six IRQ UARTs, polling SPI, fixed PWM, IWDG and EXTI; selected ADC1 bulb shots/finite TIM6 DMA, polling I2C1 and ECC Flash; no continuous ADC or inherited F407 modes |
| H743ZI reference selections | Fixed HSI32/HSI64 and Cache off/on; finite USART3 TX DMA or RX DMA-IDLE, optional initialized D2 SRAM1 and selected SPI/I2C/ADC/PWM/EXTI/IWDG/Flash; Flash requires Cache off; no continuous RX or running-state handoff |
| U575ZI-Q default reference | Externally established TZEN=0/SWAP_BANK=0 and MSIS4; selected GPIO/UART/IWDG and finite SPI/I2C/ADC/PWM/EXTI/Flash; external ADC supply admission; no DMA or dual-domain factory |
| U575 separate dual-domain reference | TZEN=1, no OS; synchronous scalar gateway and copied cooperative jobs of at most eight PC7 commands in Secure storage; no retained NS pointer or generic asynchronous cross-domain transport |
| CPU-only Runtime | M0/M0+/M3/M4/M7/M23/M33/M55/M85 Core/Arch and selected baremetal/FreeRTOS software profiles; no generated SoC/Board/factory |

The [F103 source boundary](../../../soc/stm32f103/README.md) separates the
CPU-readable 128 KiB Flash-density check from its declared orderable part.
ES096 section2.2.2 prevents normal firmware from observing the debug-only
device group and revision. Standalone startup does not require a debugger, and
an unavailable zero read cannot establish a reviewed silicon revision.

ThreadX currently maintains Native Linux GNU and M4 GNU profiles; it does not
inherit the FreeRTOS Cortex-M, MPU, split-security or tickless matrix. Platform
startup does not enter either kernel. The application owns scheduler startup.

![CPU primitive, runtime, SoC and physical qualification boundaries](../../assets/diagrams/cortex-m-capabilities.svg)

FIG-13 separates CPU software coverage from exact SoC integration and physical
acceptance. FPU, DSP, Cache, MVE, SAU and IRQ width are explicit required facts;
a CPU name never enables an optional capability automatically.

## Check a selection before using it

1. Find the exact full part and Board package, including unknown physical facts.
2. Inspect the selected controller route and mode in the capability matrix.
3. Resolve the actual complete assembly; unsupported modes and conflicts reject.
4. Check software evidence for its exact source/configuration, not a nearby tuple.
5. Check an actual station record before claiming physical behavior.

All maintained MCU physical HIL remains not executed in the current records.
Default Board bindings do not cover every connector, transceiver or external part.
Finite block modes include rearm gaps. Continuous modes need explicit replacement
storage and service budgets; they do not promise losslessness under arbitrary load.
Flash geometry is physical, while product partitions and update policy are external.
ECC `program_flags` can require proven fresh erase before each program unit;
all-FF readback cannot establish erased ECC history. Every started pulse settles
before return, and deadlines only prevent later pulses. These source mechanisms
and their developer tests do not inherit final qualification from historical
results. Exact current-source software execution is recorded separately.

![Selected U575 synchronous Secure/NonSecure reference](../../assets/diagrams/u575-security-domains.svg)

FIG-14 applies only to its declared TZEN=1 synchronous reference. It does not
change the default TZEN=0 Platform configuration or establish physical isolation.
Read [qualification](qualification.md) before treating any software result as a release.
