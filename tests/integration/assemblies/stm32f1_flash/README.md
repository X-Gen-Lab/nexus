# STM32F103 internal Flash references

These opt-in HSI8 assemblies link one physical Flash port. The consumer reads
the geometry and the first and last physical bytes.

| Assembly | Device | Capacity | Page size | Reference |
| --- | --- | ---: | ---: | --- |
| `rb.toml` | STM32F103RBT6 | 128 KiB | 1 KiB | PM0075 Table 3, DS5319 Table 28, ES096 §2.2.11 |
| `ze.toml` | STM32F103ZET6 | 512 KiB | 2 KiB | PM0075 Table 4, DS5792 Table 29, ES0340 §2.2.10 |
| `zg.toml` | STM32F103ZGT6 | 1024 KiB | 2 KiB | PM0068 Table 1 and §§1.3, 2.3, DS6558 Table 29, ES0346 §2.2.8 |

Each reference requires an explicit nominal 3300 mV Board binding. The
datasheets specify an inclusive 2000–3600 mV programming range and up to
40 ms for one page erase under the stated conditions. The nominal HSI8
timebase requires overflow service within 65.536 ms. Budget for Flash
execution stalls when choosing a watchdog or interrupt latency limit.
Products declare their own writable regions.

The ZG Bank 1 spans `0x08000000–0x0807FFFF`. Bank 2 spans
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

[ST PM0068 Rev2](https://www.st.com/resource/en/programming_manual/pm0068-stm32f10xxx-xldensity-flash-programming-stmicroelectronics.pdf)
