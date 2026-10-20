---
id: status.ledgers.platform-refinement
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-platform-refinement)=
# I/O 完善台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/platform-refinement-execution.csv)。
源文件：`docs/design/platform-refinement-execution.csv`；SHA-256：`775c626d571c734c924a53fdf5a7eb49490d7f3c07e135ce8b589d4afda0da0c`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 60 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-platform-refinement-io-01)=
## IO-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-01 |
| `task` | ADC mode reset |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32f407/drivers/adc.c |
| `evidence_paths` | tools/testing/evidence/adc-iteration |
| `exit_condition` | stream to polling clears scan sequence before sample |
| `owner` | codex |

(status-platform-refinement-io-02)=
## IO-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-02 |
| `task` | ADC retry calibration |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/gd32f470/drivers/adc.c |
| `evidence_paths` | tools/testing/evidence/adc-iteration |
| `exit_condition` | retry repeats bounded stable reset calibration |
| `owner` | codex |

(status-platform-refinement-io-03)=
## IO-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-03 |
| `task` | GPIO inferred mask validation |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/configure.py |
| `evidence_paths` | tools/configure/test_configure.py |
| `exit_condition` | omitted mask cannot authorize extra initial bits |
| `owner` | codex |

(status-platform-refinement-io-04)=
## IO-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-04 |
| `task` | Current support documentation |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/documentation.py;docs/delivery/support.md |
| `evidence_paths` | tools/testing/evidence/configure-iteration |
| `exit_condition` | mode MPU split and reviewed route statements match production |
| `owner` | codex |

(status-platform-refinement-io-05)=
## IO-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-05 |
| `task` | SPI polling owner identity |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | components/bus_owner/src/spi_owner.c |
| `evidence_paths` | tools/testing/evidence/spi-polling-identity-green.log |
| `exit_condition` | foreign controller rejected before borrowing or hardware |
| `owner` | codex |

(status-platform-refinement-io-06)=
## IO-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-06 |
| `task` | Example startup rollback |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | os/freertos/freertos.c;X-Gen-Lab/nexus-examples |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/evidence/startup-rollback;X-Gen-Lab/nexus-examples/tests/evidence/scheduler-return |
| `exit_condition` | close drain join or prestart discard; preserve cleanup responsibility |
| `owner` | codex |

(status-platform-refinement-io-07)=
## IO-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-07 |
| `task` | I/O service diagnostics |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io;components |
| `evidence_paths` | tools/testing/evidence/io-observer-iteration |
| `exit_condition` | progress and failures observable without hidden worker |
| `owner` | codex |

(status-platform-refinement-io-08)=
## IO-08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-08 |
| `task` | Mode ownership cancellation matrix |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;scripts/ci/io_lifecycle_matrix.py |
| `evidence_paths` | tools/testing/evidence/io-lifecycle-iteration |
| `exit_condition` | 203 reviewed bounded properties; source/object/binary capture and canonical all-plan executable admission; final fresh complete plan is the qualification authority |
| `owner` | codex |

(status-platform-refinement-io-09)=
## IO-09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-09 |
| `task` | Quarantine integration and recovery |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | components;X-Gen-Lab/nexus-examples |
| `evidence_paths` | tools/testing/evidence/io-observer-iteration |
| `exit_condition` | retain borrowed storage; explicit recovery owner |
| `owner` | codex |

(status-platform-refinement-io-10)=
## IO-10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-10 |
| `task` | Stream capacity backpressure budget |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/stream.c;tools/hil |
| `evidence_paths` | tools/testing/evidence/io-observer-iteration |
| `exit_condition` | bounded capacity and caller policy; no overwritten loans |
| `owner` | codex |

(status-platform-refinement-io-11)=
## IO-11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-11 |
| `task` | Optional read-only capabilities |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/factory.py;tools/configure/providers/common.py |
| `evidence_paths` | tools/testing/evidence/configure-iteration |
| `exit_condition` | selected descriptors; simulated Native flags; no hot path cost |
| `owner` | codex |

(status-platform-refinement-io-12)=
## IO-12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-12 |
| `task` | UART RX DMA and IDLE |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32f407/drivers/uart_receive.c;soc/gd32f470/drivers/uart_receive.c |
| `evidence_paths` | tools/testing/evidence/uart-iteration;tools/testing/evidence/configure-streams |
| `exit_condition` | reviewed DMA routes; finite and continuous ownership |
| `owner` | codex |

(status-platform-refinement-io-13)=
## IO-13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-13 |
| `task` | UART duplex DMA blocks |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32f407/drivers/uart_receive.c;soc/gd32f470/drivers/uart_receive.c |
| `evidence_paths` | tools/testing/evidence/uart-iteration;tools/testing/evidence/configure-streams |
| `exit_condition` | TX DMA plus RX blocks without default state growth |
| `owner` | codex |

(status-platform-refinement-io-14)=
## IO-14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-14 |
| `task` | UART RTS CTS and RS485 |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/uart_rs485.c;soc;tools/configure |
| `evidence_paths` | tools/testing/evidence/uart-iteration;docs/design/uart-dma-rs485.md |
| `exit_condition` | reviewed pins; actual TC; DE delay borrowing contract |
| `owner` | codex |

(status-platform-refinement-io-15)=
## IO-15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-15 |
| `task` | Continuous ADC DMA |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/stream.c;soc/*/drivers/adc_continuous.c |
| `evidence_paths` | tools/testing/evidence/adc-iteration;tools/testing/evidence/configure-streams |
| `exit_condition` | preposted targets; hardware address replacement proof |
| `owner` | codex |

(status-platform-refinement-io-16)=
## IO-16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-16 |
| `task` | ADC channel timing expansion |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/adc_observer.c;soc;tools/configure |
| `evidence_paths` | tools/testing/evidence/adc-iteration |
| `exit_condition` | sample-time and scan rate hardware bounds |
| `owner` | codex |

(status-platform-refinement-io-17)=
## IO-17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-17 |
| `task` | DMA topology facts |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/dma.c;tools/configure |
| `evidence_paths` | tools/testing/evidence/configure-iteration |
| `exit_condition` | common opaque IDs; provider validates reviewed physical topology |
| `owner` | codex |

(status-platform-refinement-io-18)=
## IO-18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-18 |
| `task` | SPI one-way DMA |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/spi.c;soc/*/drivers/spi_dma.c |
| `evidence_paths` | tools/testing/evidence/spi-memory-arm-commands.json |
| `exit_condition` | caller dummy discard; actual spans; per-direction MINC |
| `owner` | codex |

(status-platform-refinement-io-19)=
## IO-19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-19 |
| `task` | I2C interrupt DMA async |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/i2c.c;soc/common/i2c_v1_async.c;soc/*/drivers/i2c_async.c |
| `evidence_paths` | tools/testing/evidence/i2c-async-iteration;tools/testing/evidence/configure-streams |
| `exit_condition` | ACK final byte sequence; BTF STOP; request final access |
| `owner` | codex |

(status-platform-refinement-io-20)=
## IO-20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-20 |
| `task` | I2C physical bus clear |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io/src/i2c_bus_clear.c |
| `evidence_paths` | tests/evidence/i2c-bus-clear |
| `exit_condition` | bounded pulses STOP; open-drain and cleanup ownership |
| `owner` | codex |

(status-platform-refinement-io-21)=
## IO-21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-21 |
| `task` | Additional reviewed controller routes |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/*/drivers;tests/google |
| `evidence_paths` | tools/testing/evidence/multi-controller-iteration |
| `exit_condition` | static multi-controller assembly across three exact parts and two backends; 18 authored firmware fixtures, pure cold I2C identity and retained lifecycle; final maintained ARM and clean SDK qualification required |
| `owner` | codex |

(status-platform-refinement-io-22)=
## IO-22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-22 |
| `task` | GD additional SRAM and TCM linker |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | core/include/nexus/core/memory.h;soc/gd32f470/memory_init.c;tools/configure/memory.py;cmake/platform |
| `evidence_paths` | tools/testing/evidence/memory-iteration;tools/testing/evidence/configure-memory-metadata |
| `exit_condition` | explicit sections and budgets; DMA TCM rejection |
| `owner` | codex |

(status-platform-refinement-io-23)=
## IO-23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-23 |
| `task` | Three Board electrical fact review |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | boards |
| `evidence_paths` | pending |
| `exit_condition` | actual PCB revision connector crystal and NOR identity |
| `owner` | codex |

(status-platform-refinement-io-24)=
## IO-24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-24 |
| `task` | Resolver emitter common limits |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers/common.py;tools/configure/providers/*/bindings.py |
| `evidence_paths` | tools/testing/evidence/configure-limits-clock |
| `exit_condition` | factory limits derive from public contract; reviewed clock tuple; four selected ARM graphs with resource/retention evidence |
| `owner` | codex |

(status-platform-refinement-io-25)=
## IO-25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-25 |
| `task` | Physical FP MVE task context |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | real target observations; current three boards are M4F |
| `owner` | codex |

(status-platform-refinement-io-26)=
## IO-26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-26 |
| `task` | Physical clock tick deadline accuracy |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | bound timer drift tick and monotonic evidence |
| `owner` | codex |

(status-platform-refinement-io-27)=
## IO-27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-27 |
| `task` | Runtime MSP task stack peaks |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil;os |
| `evidence_paths` | tools/testing/evidence/stack-owner-iteration |
| `exit_condition` | bounded quiescent stack scan and actual pinned kernel ownership rounds; physical MCU stack/soak not executed |
| `owner` | codex |

(status-platform-refinement-io-28)=
## IO-28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-28 |
| `task` | OS owner lifecycle endurance |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | blocked |
| `implementation_paths` | os;components;tools/hil |
| `evidence_paths` | tools/testing/evidence/stack-owner-iteration |
| `exit_condition` | bounded quiescent stack scan and actual pinned kernel ownership rounds; physical MCU stack/soak not executed |
| `owner` | codex |

(status-platform-refinement-io-29)=
## IO-29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-29 |
| `task` | Board tickless continuous timer port |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | blocked |
| `implementation_paths` | soc;os/freertos |
| `evidence_paths` | tools/testing/evidence/timebase-lowpower-iteration |
| `exit_condition` | explicit fixed-profile shallow WFI timebase port; actual4ARM/defaultomission/model50; physical wake accuracy unexecuted |
| `owner` | codex |

(status-platform-refinement-io-30)=
## IO-30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-30 |
| `task` | Fail-stop and product safety output hooks |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | blocked |
| `implementation_paths` | core/src/fault.c;os;X-Gen-Lab/nexus-examples |
| `evidence_paths` | tools/testing/evidence/fault-observation;tests/evidence/threadx-fault-observer |
| `exit_condition` | stateless observer before retaining fail-stop; preserves stronger incoming masks and actual fault context; external safety policy and physical outputs separately qualified |
| `owner` | codex |

(status-platform-refinement-io-31)=
## IO-31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-31 |
| `task` | Physical restricted MPU isolation |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | software fixtures cannot prove actual memory fault isolation |
| `owner` | codex |

(status-platform-refinement-io-32)=
## IO-32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-32 |
| `task` | Physical split security isolation |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | requires matching secure target and real SAU attribution |
| `owner` | codex |

(status-platform-refinement-io-33)=
## IO-33

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-33 |
| `task` | Selectable OS object cost |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | os/freertos/freertos.c;scripts/ci/os_resources.py;tests/contracts/os_resources |
| `evidence_paths` | tools/testing/evidence/os-optimization;/workspace/nexus-delivery/bd6df2c7a1b9a6cb613947c0528f998ff21d31e3/github/all-cortex/reports/os-resources/report.json |
| `exit_condition` | actual eight ABI resource variants and 56 commands refreshed, 1136 evidence identity references verified; final clean source costs remain independently bound |
| `owner` | codex |

(status-platform-refinement-io-34)=
## IO-34

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-34 |
| `task` | Additional real RTOS backend |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | os/threadx;tools/configure/threadx.py |
| `evidence_paths` | tests/evidence/threadx-integration;tools/testing/evidence/threadx-dependency;tools/testing/evidence/threadx-context-iteration |
| `exit_condition` | official locked ThreadX Linux and CortexM4 backend; actual62Google and two complete62 TSan executions plus sixBoardARM; Linux scheduler state snapshots and signal posture repaired; final clean installed SDK consumers required; no physical qualification |
| `owner` | codex |

(status-platform-refinement-io-35)=
## IO-35

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-35 |
| `task` | Optional nominal factory IDs |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/factory.py;tools/configure/test_factory_features.py |
| `evidence_paths` | tools/testing/evidence/configure-iteration |
| `exit_condition` | C and C++ compile reject cross-kind IDs; no default RAM cost |
| `owner` | codex |

(status-platform-refinement-io-36)=
## IO-36

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-36 |
| `task` | Executable TOML documentation |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/documentation.py;tools/configure/test_documentation.py |
| `evidence_paths` | tools/testing/evidence/configure-iteration |
| `exit_condition` | resolve every complete snippet; explicit fragment rationale |
| `owner` | codex |

(status-platform-refinement-io-37)=
## IO-37

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-37 |
| `task` | Provider property failure matrix |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | tests/google;scripts/ci/io_lifecycle_matrix.py |
| `evidence_paths` | tools/testing/evidence/io-lifecycle-iteration |
| `exit_condition` | 203 bounded properties from same fresh unfiltered production target execution; canonical all-plan/source/object/binary identity; final strict complete plan required |
| `owner` | codex |

(status-platform-refinement-io-38)=
## IO-38

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-38 |
| `task` | New mode resources and GC |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/measurement/retention.py;soc/*/CMakeLists.txt;tests/contracts/*_assembly |
| `evidence_paths` | tools/testing/evidence/configure-streams;tools/testing/evidence/provider-empty-input-iteration |
| `exit_condition` | maintained fixture-driven ARM assembly gate checks exact inputs, budgets and selected/omitted operation retention; final whole 87-fixture execution recorded outside source; legal empty SDK identities retained; 35 admission regressions and 92 related checks passed |
| `owner` | codex |

(status-platform-refinement-io-39)=
## IO-39

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-39 |
| `task` | Physical poll IRQ DMA benchmark |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | same artifact clock load optimization and raw timing |
| `owner` | codex |

(status-platform-refinement-io-40)=
## IO-40

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-40 |
| `task` | CI artifact source identity |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | .github/actions/record-identity;tools/maintenance/job_identity.py |
| `evidence_paths` | tools/testing/evidence/ci-identity |
| `exit_condition` | every job actual checkout tree deps config tool identity |
| `owner` | codex |

(status-platform-refinement-io-41)=
## IO-41

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-41 |
| `task` | Evidence retention and attestation |
| `implementation_status` | implemented |
| `software_status` | runtime_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | .github/workflows/release.yml;dependencies/actions.lock.json |
| `evidence_paths` | tools/testing/evidence/ci-identity |
| `exit_condition` | immutable archive and provenance; no fabricated signature |
| `owner` | codex |

(status-platform-refinement-io-42)=
## IO-42

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-42 |
| `task` | API schema SDK compatibility version |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies/platform-compatibility.json;tools/configure/metadata.py;tools/maintenance/compatibility.py;cmake/package |
| `evidence_paths` | tools/testing/evidence/source-compatibility;tools/testing/evidence/source-compatibility-integration;tools/testing/evidence/configure-memory-metadata;tools/testing/evidence/source-sdk-archive-exclusion-iteration |
| `exit_condition` | canonical epochs and exact source-only SDK closure; 45 SDK boundary regressions passed; same-HEAD owned archive exclusion removes 183996010 bytes; final relocated clean canonical consumers required |
| `owner` | codex |

(status-platform-refinement-io-43)=
## IO-43

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-43 |
| `task` | Agent maintenance responsibility |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | .github/maintainer-roles.json;tools/maintenance/ownership.py |
| `evidence_paths` | tools/testing/evidence/agent-maintenance |
| `exit_condition` | Codex owns all domains; no named people required |
| `owner` | codex |

(status-platform-refinement-io-44)=
## IO-44

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-44 |
| `task` | Remote rules and required checks |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | .github/workflows |
| `evidence_paths` | pending |
| `exit_condition` | 2026-10-20 GET confirms main unprotected and empty effective rules and rulesets; actual main protection PUT returns 403 Resource not accessible by integration; readback confirms enforcement absent; no fake CODEOWNERS |
| `owner` | codex |

(status-platform-refinement-io-45)=
## IO-45

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-45 |
| `task` | Defect review release execution ownership |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;tools/evidence/reproduce.py;.github/workflows |
| `evidence_paths` | tools/testing/evidence/ci-identity;tools/testing/evidence/fixture-lifetime-iteration;tools/testing/evidence/freertos-failstop-parent-iteration |
| `exit_condition` | Codex owns every domain; independent review and actual failure evidence retained; release validation distinct from ordinary CI and promotion remains external; three fixture lifetime gaps closed with 224 before/after regular and sanitizer executions; failstop eight cases passed regular/ASan/TSan after actual old timeout reproduction |
| `owner` | codex |

(status-platform-refinement-io-46)=
## IO-46

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-46 |
| `task` | Locked dependency upgrade maintenance |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/dependency_proposals.py;.github/workflows/dependency-maintenance.yml |
| `evidence_paths` | tools/testing/evidence/source-compatibility;https://github.com/X-Gen-Lab/nexus/actions/runs/39221613405 |
| `exit_condition` | weekly read-only schedule executed successfully on main 2730b0acec456fe0c2dd03dccd3b6fb35400e1b4; upgrades require exact evidence |
| `owner` | codex |

(status-platform-refinement-io-47)=
## IO-47

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-47 |
| `task` | Real HIL station binding |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | three physical probes serial and chip identity unavailable |
| `owner` | codex |

(status-platform-refinement-io-48)=
## IO-48

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-48 |
| `task` | Physical clock reset GPIO watchdog |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | actual station capture and reset reason |
| `owner` | codex |

(status-platform-refinement-io-49)=
## IO-49

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-49 |
| `task` | Physical UART SPI DMA fault waveforms |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | logic-analyzer and serial raw capture matching artifact |
| `owner` | codex |

(status-platform-refinement-io-50)=
## IO-50

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-50 |
| `task` | Physical I2C electrical protocol |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | ACK NACK STOP line voltage bus clear observed |
| `owner` | codex |

(status-platform-refinement-io-51)=
## IO-51

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-51 |
| `task` | Physical ADC PWM EXTI |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | real stimulus and independent instrument observations |
| `owner` | codex |

(status-platform-refinement-io-52)=
## IO-52

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-52 |
| `task` | Physical Flash power interruption |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil;X-Gen-Lab/nexus-examples |
| `evidence_paths` | pending |
| `exit_condition` | destructive test region explicitly owned externally |
| `owner` | codex |

(status-platform-refinement-io-53)=
## IO-53

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-53 |
| `task` | Physical concurrent endurance and latency |
| `implementation_status` | blocked |
| `software_status` | pending |
| `hardware_status` | blocked |
| `implementation_paths` | tools/hil |
| `evidence_paths` | pending |
| `exit_condition` | same firmware soak interrupt latency stack load evidence |
| `owner` | codex |

(status-platform-refinement-io-54)=
## IO-54

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-54 |
| `task` | CAN platform interface and controllers |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io;soc |
| `evidence_paths` | tools/testing/evidence/can-iteration;tools/testing/evidence/configure-can |
| `exit_condition` | classic CAN IRQ request/frame rings; shared filter-domain ownership; two generated instances; no physical bus claim |
| `owner` | codex |

(status-platform-refinement-io-55)=
## IO-55

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-55 |
| `task` | Ethernet MAC PHY raw frames |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io;soc;components/dp83848;tools/configure/ethernet.py |
| `evidence_paths` | tools/testing/evidence/ethernet-iteration;tools/testing/evidence/configure-ethernet |
| `exit_condition` | RMII borrowed descriptor/frame buffers; verified cold SRAM gates and generation; actual two-family factory/model tests and four ZG ARM graphs; physical PHY/link/PCB not executed |
| `owner` | codex |

(status-platform-refinement-io-56)=
## IO-56

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-56 |
| `task` | USB clock endpoints lifecycle |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io;soc;tools/configure/usb_device.py |
| `evidence_paths` | tools/testing/evidence/usb-device-iteration;tools/testing/evidence/usb-clock-iteration;tools/testing/evidence/configure-usb |
| `exit_condition` | DWCFS PIO endpoints and epoch-preserving assembly; actual48MHz proof and NVIC drain models/six selectedARM; no physical USB claim |
| `owner` | codex |

(status-platform-refinement-io-57)=
## IO-57

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-57 |
| `task` | Timer capture advanced PWM |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | io; soc/*/drivers; tools/configure/timer_advanced.py |
| `evidence_paths` | tools/testing/evidence/timer-iteration;tools/testing/evidence/timer-factory-iteration |
| `exit_condition` | two-family timer capture and complementary PWM deadtime break ownership; actual shared-resource and cold cleanup regressions, selected/default ARM graphs; physical output and timing not executed |
| `owner` | codex |

(status-platform-refinement-io-58)=
## IO-58

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-58 |
| `task` | SoC sleep and clock lifecycle |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;os |
| `evidence_paths` | tools/testing/evidence/clock-domain-iteration |
| `exit_condition` | caller-owned clock leases and exact fixed-profile shallow sleep; actual drain/wake/clock readback models; dynamic PLL/STOP and physical timing not supported |
| `owner` | codex |

(status-platform-refinement-io-59)=
## IO-59

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-59 |
| `task` | Additional SoC Board complete vertical |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32g474;boards/stm32g474re_hse24_reference;tools/configure/providers/stm32g474 |
| `evidence_paths` | tools/testing/evidence/g474-iteration;tests/evidence/g474-factory-integration |
| `exit_condition` | exact STM32G474RET6 GPIO LPUART factory/startup clock/linker longitudinal slice; actual regular sanitizer and selected/empty ARM; reference PCB and other peripherals unqualified |
| `owner` | codex |

(status-platform-refinement-io-60)=
## IO-60

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-60 |
| `task` | Narrow user I/O isolation contracts |
| `implementation_status` | implemented |
| `software_status` | targeted_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | components/user_io_broker |
| `evidence_paths` | tools/testing/evidence/user-io-broker-iteration |
| `exit_condition` | trusted privileged GPIO gateway validates ranges and retains user loans until actual drain; optional component; SVC MPU TZ gateway and physical isolation not implemented |
| `owner` | codex |
