---
id: status.ledgers.factory-platform
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-factory-platform)=
# Factory 与 TDD 台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/factory-platform-execution.csv)。
源文件：`docs/design/factory-platform-execution.csv`；SHA-256：`49215c9d35115fd258b86d516869215950c78ee5579ca440d9427de731780836`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 24 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-factory-platform-fp-001)=
## FP-001

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-001 |
| `phase` | P0 |
| `area` | architecture |
| `task` | factory + shared polymorphic face contracts |
| `roles` | TL;Core |
| `implementation_paths` | docs/design/next-generation-platform.md;io/include/nexus/io |
| `software_exit` | immutable faces; independent contexts; cold factory; explicit lifecycle |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-002)=
## FP-002

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-002 |
| `phase` | P0 |
| `area` | testing |
| `task` | GoogleTest and GoogleMock offline host framework |
| `roles` | QA;Build |
| `implementation_paths` | tests/google/CMakeLists.txt;dependencies/googletest.lock.json;dependencies/source |
| `software_exit` | exact archive SHA; C++ host only; no firmware dependency |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-003)=
## FP-003

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-003 |
| `phase` | P0 |
| `area` | automation |
| `task` | TDD and commit checks |
| `roles` | QA;Build |
| `implementation_paths` | scripts/ci/tdd_gate.py;.pre-commit-config.yaml;AGENTS.md;tools/testing |
| `software_exit` | actual complete fresh execution; no filters/skips; staged snapshot; recorded RED/GREEN |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-004)=
## FP-004

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-004 |
| `phase` | P1 |
| `area` | configuration |
| `task` | TOML schema2 authoring and immutable typed IR |
| `roles` | Build;BSP |
| `implementation_paths` | tools/configure/authored.py;tools/configure/ir.py;tools/configure/configure.py |
| `software_exit` | strict fields; exact resources; frozen nested values; common CLI frontend |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-005)=
## FP-005

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-005 |
| `phase` | P1 |
| `area` | factory |
| `task` | generated static instances and typed IDs |
| `roles` | Core;Build |
| `implementation_paths` | tools/configure/factory.py;tools/configure/bindings.py |
| `software_exit` | constant face; uint16 IDs; invalid returns NULL; controller and endpoint identities distinct |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-006)=
## FP-006

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-006 |
| `phase` | P1 |
| `area` | native |
| `task` | multiple independent Native providers |
| `roles` | Core;QA |
| `implementation_paths` | io/native;tests/google/native_instances_test.cpp;tests/google/native_factory_test.cpp |
| `software_exit` | actual generated factory; independent resources; stopped admission; borrowed storage retained |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-007)=
## FP-007

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-007 |
| `phase` | P2 |
| `area` | platform |
| `task` | three boards and baremetal/FreeRTOS composition |
| `roles` | BSP;Build |
| `implementation_paths` | boards;soc;CMakePresets.json |
| `software_exit` | six real ARM preset ELF/BIN/map and budget reports; explicit routes |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-008)=
## FP-008

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-008 |
| `phase` | P2 |
| `area` | io |
| `task` | all nine interface kinds share static ops |
| `roles` | Core;BSP |
| `implementation_paths` | io/include/nexus/io;io/src;soc/*/drivers;io/native |
| `software_exit` | production SDK private; all calls use exact context; C/C++ ABI consumers |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-009)=
## FP-009

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-009 |
| `phase` | P2 |
| `area` | components |
| `task` | common narrow ports and separate application examples |
| `roles` | Components;Build |
| `implementation_paths` | components;X-Gen-Lab/nexus-examples |
| `software_exit` | generic components and actual relocated consumers; external source pin matches core |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-010)=
## FP-010

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-010 |
| `phase` | P3 |
| `area` | wake |
| `task` | explicit bounded IRQ wake |
| `roles` | Core;BSP |
| `implementation_paths` | io/include/nexus/io/wake.h;io/src/wake.c;tests/google/irq_wake_test.cpp |
| `software_exit` | latched predicates; actual publisher priority ceiling; detach publishers before release |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-011)=
## FP-011

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-011 |
| `phase` | P3 |
| `area` | dma |
| `task` | STM32 USART1 finite TX DMA |
| `roles` | BSP;QA |
| `implementation_paths` | soc/stm32f407/drivers/uart_dma.c;tests/google/stm32_uart_dma_test.cpp |
| `software_exit` | DMA2 stream7 channel4; memory-domain rejection; memory TC distinct from wire TC; quarantine |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-012)=
## FP-012

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-012 |
| `phase` | P3 |
| `area` | dma |
| `task` | GD32 USART0 finite TX DMA |
| `roles` | BSP;QA |
| `implementation_paths` | soc/gd32f470/drivers/uart_dma.c;tests/google/gd32_uart_dma_test.cpp |
| `software_exit` | DMA1 channel7 selector4; independent wire and engine drain; late IRQ protection |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-013)=
## FP-013

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-013 |
| `phase` | P3 |
| `area` | dma |
| `task` | STM32 SPI1 full duplex finite DMA |
| `roles` | BSP;QA |
| `implementation_paths` | soc/stm32f407/drivers/spi_dma.c;tests/google/stm32_spi_dma_test.cpp |
| `software_exit` | DMA2 RXstream0/TXstream3 channel3; nonoverlapping buffers; CS retained until idle |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-014)=
## FP-014

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-014 |
| `phase` | P3 |
| `area` | dma |
| `task` | GD32 SPI4 full duplex finite DMA |
| `roles` | BSP;QA |
| `implementation_paths` | soc/gd32f470/drivers/spi_dma.c;tests/google/gd32_spi_dma_test.cpp |
| `software_exit` | DMA1 RXchannel3/TXchannel4 selector2; no poll-as-async; exact CS interval |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-015)=
## FP-015

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-015 |
| `phase` | P3 |
| `area` | components |
| `task` | separate optional async SPI owner executor |
| `roles` | Core;QA |
| `implementation_paths` | components/bus_owner/src/spi_async_owner.c;tests/google/spi_async_owner_test.cpp |
| `software_exit` | reject foreign bus before admission; preserve payload during quarantine; small polling executor |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-016)=
## FP-016

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-016 |
| `phase` | P4 |
| `area` | stream |
| `task` | bounded fixed-block loans and Native producers |
| `roles` | Core;QA |
| `implementation_paths` | io/src/stream.c;io/native/stream.c;tests/google/native_async_test.cpp |
| `software_exit` | never overwrite loan; epoch identity; loss ordering; stop joins producers and consumers |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-017)=
## FP-017

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-017 |
| `phase` | P4 |
| `area` | stream |
| `task` | STM32 and GD32 UART RX IDLE/block providers |
| `roles` | BSP;QA |
| `implementation_paths` | soc/*/drivers/uart_stream.c;tests/google/*uart_stream_test.cpp |
| `software_exit` | exact mode; whole/IDLE prefix publication; bounded loss; final block and held consumer survive stop |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-018)=
## FP-018

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-018 |
| `phase` | P4 |
| `area` | adc |
| `task` | STM32 and GD32 timer trigger DMA block providers |
| `roles` | BSP;QA |
| `implementation_paths` | soc/*/drivers/adc_stream.c;tests/google/*adc_stream_test.cpp |
| `software_exit` | timer and conversion limits; disabled normal DMA before publication; pause/rearm without overwrite |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-019)=
## FP-019

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-019 |
| `phase` | P4 |
| `area` | controllers |
| `task` | multiple GD32 controllers and reviewed resources |
| `roles` | BSP;Build |
| `implementation_paths` | soc/gd32f470/private/gd32f470_provider.h;tests/google/gd32_multi_test.cpp;tests/contracts/gd32_assembly/multi.toml |
| `software_exit` | independent UART/SPI/I2C descriptors; exact controller clocks and generated IRQ state |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-020)=
## FP-020

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-020 |
| `phase` | P4 |
| `area` | performance |
| `task` | actual ELF interface and resource cost audit |
| `roles` | QA;Build |
| `implementation_paths` | tools/measurement/interfaces.py;tools/measurement/compare.py;tests/contracts/test_interface_cost.py |
| `software_exit` | readonly two-pointer face; no writable ops; actual retained symbols; O2/Os/O3 +/- LTO |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-021)=
## FP-021

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-021 |
| `phase` | P5 |
| `area` | sdk |
| `task` | clean source SDK and external pin |
| `roles` | Build;Components |
| `implementation_paths` | cmake/package;X-Gen-Lab/nexus-examples |
| `software_exit` | clean current HEAD; relocated C/C++ Native and ARM consumers; no host framework in firmware SDK |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-022)=
## FP-022

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-022 |
| `phase` | P5 |
| `area` | evidence |
| `task` | offline reproduction and software candidate |
| `roles` | Build;QA |
| `implementation_paths` | tools/evidence;docs/delivery/qualification.md |
| `software_exit` | same clean source/config/dependency/tool/ELF; fresh nonempty tests; two network-none builds |
| `implementation_status` | implemented |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-023)=
## FP-023

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-023 |
| `phase` | P5 |
| `area` | hil |
| `task` | three-board HIL tooling and unbound plans |
| `roles` | QA;BSP |
| `implementation_paths` | tools/hil/prepare.py;tools/hil/admit.py;tools/hil/fixtures |
| `software_exit` | explicit station/PCB/probe/serial; missing identity and budget rejected; no invented physical pass |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |

(status-factory-platform-fp-024)=
## FP-024

| 原字段 | 原值 |
| --- | --- |
| `task_id` | FP-024 |
| `phase` | P5 |
| `area` | delivery |
| `task` | scope freeze and conditional physical acceptance |
| `roles` | TL;QA;Build |
| `implementation_paths` | docs/delivery/support.md;docs/delivery/verification.md;docs/delivery/qualification.md |
| `software_exit` | software and physical status separate; physical execution only when connected; exact tested tuples |
| `implementation_status` | implemented |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh external evidence for exact source HEAD |
