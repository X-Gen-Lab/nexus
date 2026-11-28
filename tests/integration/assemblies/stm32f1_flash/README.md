# STM32F1 internal Flash references

These opt-in HSI8 assemblies link one physical Flash port. The consumer reads
the geometry and the first and last physical bytes.

| Assembly | Device | Capacity | Page size | Reference |
| --- | --- | ---: | ---: | --- |
| `c6.toml` | STM32F103C6T6A | 32 KiB | 1 KiB | PM0075 Table 2, DS5936 Tables 2 and 28, ES0348 §2.2.10 |
| `rb.toml` | STM32F103RBT6 | 128 KiB | 1 KiB | PM0075 Table 3, DS5319 Table 28, ES096 §2.2.11 |
| `f100rb.toml` | STM32F100RBT6B | 128 KiB | 1 KiB | PM0063 Table 2, DS6517 Tables 2 and 27, ES093 §2.2.8 |
| `f101rb.toml` | STM32F101RBT6 | 128 KiB | 1 KiB | PM0075 Table 3, DS5318 Tables 2 and 27, ES096 §2.2.11 |
| `f102rb.toml` | STM32F102RBT6 | 128 KiB | 1 KiB | PM0075 Table 3, DS5933 Tables 2 and 27, ES096 §2.2.11 |
| `f101c6.toml` | STM32F101C6T6A | 32 KiB | 1 KiB | PM0075 Table 2, DS5935 Table 27, ES0348 §2.2.10 |
| `f102c6.toml` | STM32F102C6T6A | 32 KiB | 1 KiB | PM0075 Table 2, DS5934 Table 27, ES0348 §2.2.10 |
| `f101ze.toml` | STM32F101ZET6 | 512 KiB | 2 KiB | PM0075 Table 4, DS5791 Table 29, ES0340 §2.2.10 |
| `f100ze.toml` | STM32F100ZET6B | 512 KiB | 2 KiB | PM0063 Table 3, DS5944 Table 28, ES0136 §2.2.7 |
| `f101zg.toml` | STM32F101ZGT6 | 1024 KiB | 2 KiB | PM0068 Table 1, DS6557 Table 29, ES0346 §2.2.8 |
| `ze.toml` | STM32F103ZET6 | 512 KiB | 2 KiB | PM0075 Table 4, DS5792 Table 29, ES0340 §2.2.10 |
| `zg.toml` | STM32F103ZGT6 | 1024 KiB | 2 KiB | PM0068 Table 1 and §§1.3, 2.3, DS6558 Table 29, ES0346 §2.2.8 |

Each reference requires an explicit nominal 3300 mV Board binding. The
datasheets specify an inclusive 2000–3600 mV programming range and up to
40 ms for one page erase under the stated conditions. The nominal HSI8
timebase requires overflow service within 65.536 ms. Budget for Flash
execution stalls when choosing a watchdog or interrupt latency limit.
Products declare their own writable regions.

For F101ZG and F103ZG, Bank 1 spans `0x08000000–0x0807FFFF`. Bank 2 spans
`0x08080000–0x080FFFFF`. Reads may cross this boundary when both banks are
idle. A program or erase call must stay in one bank. Split a larger write
region at offset `0x80000`. A cross-bank request returns
`NX_ERROR_UNSUPPORTED` before unlocking either bank. A failed release holds
the physical owner until recovery relocks the selected bank and proves both
banks idle.

The production-C model covers page erase, halfword program, protection
errors, delayed BSY, the Bank boundary and retained ownership. The software
reference covers ordinary serialized I/O. Read-while-write concurrency,
mass erase, option bytes, OTP, boot switching and board timing are outside
this scope.

`f105vc.toml` and `f107vc.toml` select read-only Flash on HSI8. Both expose
256 KiB at `0x08000000` with 128 physical 2 KiB pages through the same typed
factory. The face supports bounded reads and geometry. It offers no program,
erase, recovering writer, or maintenance operation. The FPEC remains locked,
and the Board binding requires no programming voltage assumption.

PM0075 Rev2 Table 5 and the pinned HAL specify 2 KiB pages. DS6014 Rev10
Table 29 labels the 40 ms maximum for a 1 KiB page. ES022 Rev11 §2.2.8
specifies delayed BSY after STRT. A writable profile requires an authoritative
time bound for the exact 2 KiB page.

[ST DS6014 Rev10](https://www.st.com/resource/en/datasheet/stm32f107vc.pdf) · [ST ES022 Rev11](https://www.st.com/resource/en/errata_sheet/es022-stm32f105xx-and-stm32f107xx-device-errata-stmicroelectronics.pdf)

[ST PM0075 Rev2](https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf) · [ST PM0063 Rev2](https://www.st.com/resource/en/programming_manual/pm0063-stm32f100xx-value-line-flash-programming-stmicroelectronics.pdf) · [ST PM0068 Rev2](https://www.st.com/resource/en/programming_manual/pm0068-stm32f10xxx-xldensity-flash-programming-stmicroelectronics.pdf)
