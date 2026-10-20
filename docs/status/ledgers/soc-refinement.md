---
id: status.ledgers.soc-refinement
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-soc-refinement)=
# SoC 原专项台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/soc-refinement-execution.csv)。
源文件：`docs/design/soc-refinement-execution.csv`；SHA-256：`7e275d999282421433bb32b5eb5556e4891e7384678d5cc032a97b3475137af5`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 55 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-soc-refinement-soc-01)=
## SOC-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-01 |
| `priority` | P0 |
| `scope` | revision |
| `task` | Current SoC documentation |
| `implementation_status` | documentation_updated |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/README.md;soc/gd32f470/README.md;soc/stm32h743/README.md;soc/stm32u575/README.md;tools/configure/README.md |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/soc-refinement.md |
| `exit_condition` | SoC configure README match maintained modes and ownership |
| `owner` | codex |

(status-soc-refinement-soc-02)=
## SOC-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-02 |
| `priority` | P1 |
| `scope` | automation |
| `task` | Capability declaration consistency |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/maintenance/capabilities.py |
| `exit_condition` | Facts routes constructors source and fixtures agree without claiming PCB qualification |
| `owner` | codex |

(status-soc-refinement-soc-03)=
## SOC-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-03 |
| `priority` | P1 |
| `scope` | revision |
| `task` | Exact package pin inventory |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc;tools/configure/configure.py;tools/configure/fact_references.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/configure/test_physical_facts.py;tools/configure/test_soc_facts.py |
| `exit_condition` | Every route validated against complete exact-part GPIO inventory |
| `owner` | codex |

(status-soc-refinement-soc-04)=
## SOC-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-04 |
| `priority` | P1 |
| `scope` | revision |
| `task` | GD SDK configuration input closure |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/configure.py;vendors/gigadevice/gd32f4xx/source.lock.json |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/configure/test_configure.py |
| `exit_condition` | GD source lock identity participates in resolved digest and import verification |
| `owner` | codex |

(status-soc-refinement-soc-05)=
## SOC-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-05 |
| `priority` | P1 |
| `scope` | maintenance |
| `task` | Datasheet and errata provenance |
| `implementation_status` | source_index_implemented |
| `software_status` | source_reviewed_applicability_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc;tools/configure/fact_references.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/configure/test_fact_references.py |
| `exit_condition` | Flash RAM pin DMA IRQ clock facts retain source versions and silicon-review boundaries |
| `owner` | codex |

(status-soc-refinement-soc-06)=
## SOC-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-06 |
| `priority` | P1 |
| `scope` | optimization |
| `task` | Immutable hardware binding descriptors |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers/descriptor.py;tools/configure/providers |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/configure/test_descriptors.py |
| `exit_condition` | Controller instances reuse explicit descriptors while independent assertions remain |
| `owner` | codex |

(status-soc-refinement-soc-07)=
## SOC-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-07 |
| `priority` | P2 |
| `scope` | optimization |
| `task` | Family integration descriptors |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers/descriptor.py;cmake/package/package_source_sdk.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/configure/test_descriptors.py;tests/contracts/test_source_sdk.py |
| `exit_condition` | Configuration family entry and dependency identities unified with CMake owning target graph |
| `owner` | codex |

(status-soc-refinement-soc-08)=
## SOC-08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-08 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Startup phase responsibilities |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/system.c;soc/gd32f470/system.c;soc/stm32g474/system.c;soc/stm32h743/system.c;soc/stm32u575/system.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/stm32_system_transaction_test.cpp;tests/google/gd32_system_transaction_test.cpp |
| `exit_condition` | Pre-RAM post-RAM and fail-stop restrictions explicit and tested |
| `owner` | codex |

(status-soc-refinement-soc-09)=
## SOC-09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-09 |
| `priority` | P1 |
| `scope` | defense |
| `task` | Cold-start admission |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers;soc |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/physical_platform_factory_test.cpp;tests/google/multi_controller_factory_test.cpp;tests/google/g474_factory_test.cpp |
| `exit_condition` | Observable selected live owners rejected before global clock modification |
| `owner` | codex |

(status-soc-refinement-soc-10)=
## SOC-10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-10 |
| `priority` | P1 |
| `scope` | optimization |
| `task` | Clock transaction consistency |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/system.c;soc/gd32f470/system.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/stm32_system_transaction_test.cpp;tests/google/gd32_system_transaction_test.cpp |
| `exit_condition` | Critical readback bounded failures safe source switching and retained rollback responsibility |
| `owner` | codex |

(status-soc-refinement-soc-11)=
## SOC-11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-11 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Clock residue semantics |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/system.c;soc/gd32f470/system.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/stm32_system_transaction_test.cpp;tests/google/gd32_system_transaction_test.cpp |
| `exit_condition` | Primary cleanup remaining obligations distinguished from allowed retained PWR Flash settings |
| `owner` | codex |

(status-soc-refinement-soc-12)=
## SOC-12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-12 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Time context restrictions |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/private;soc/gd32f470/private;soc/stm32g474/private |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/gd32_timebase_contract_test.cpp;core/include/nexus/core/time.h |
| `exit_condition` | Task configurable IRQ masks and NMI HardFault limitations align |
| `owner` | codex |

(status-soc-refinement-soc-13)=
## SOC-13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-13 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Time lifecycle validity |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/system.c;soc/gd32f470/timebase.c;soc/stm32g474/system.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/gd32_timebase_contract_test.cpp;tests/google/g474_system_test.cpp |
| `exit_condition` | Before-start failed-start and stopped reads have a documented tested valid domain |
| `owner` | codex |

(status-soc-refinement-soc-14)=
## SOC-14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-14 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Deadline boot domain |
| `implementation_status` | contract_implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/include;soc |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;core/include/nexus/core/time.h |
| `exit_condition` | Stop restart invalidates old deadlines and repeated-overflow limits remain explicit |
| `owner` | codex |

(status-soc-refinement-soc-15)=
## SOC-15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-15 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Bootstrap poll bounds |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/system.c;soc/gd32f470/system.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/stm32_system_transaction_test.cpp;tests/google/gd32_timebase_contract_test.cpp |
| `exit_condition` | Per-phase and total work bounds distinguish polls from measured elapsed time |
| `owner` | codex |

(status-soc-refinement-soc-16)=
## SOC-16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-16 |
| `priority` | P1 |
| `scope` | defense |
| `task` | GD stop controller coverage |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/gd32f470/timebase.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/gd32_timebase_contract_test.cpp |
| `exit_condition` | Secondary polling enabled-controller defenses match maintained set |
| `owner` | codex |

(status-soc-refinement-soc-17)=
## SOC-17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-17 |
| `priority` | P1 |
| `scope` | contract |
| `task` | Shallow sleep and tickless |
| `implementation_status` | contract_implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/timebase-lowpower.md;os/freertos |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/timebase_lowpower_test.cpp;docs/design/timebase-lowpower.md |
| `exit_condition` | OS tick SoC deadline and paused armed stop restrictions remain consistent |
| `owner` | codex |

(status-soc-refinement-soc-18)=
## SOC-18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-18 |
| `priority` | P1 |
| `scope` | contract |
| `task` | USB48 lifecycle |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407/usb_clock.c;soc/gd32f470/usb_clock.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/usb_clock_test.cpp |
| `exit_condition` | Shared consumers fail cleanup ownership and detached hardware precede clock release |
| `owner` | codex |

(status-soc-refinement-soc-19)=
## SOC-19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-19 |
| `priority` | P1 |
| `scope` | contract |
| `task` | No-init versus retention |
| `implementation_status` | documentation_updated |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/soc-refinement.md |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/soc-refinement.md |
| `exit_condition` | Software no-copy no-clear separated from physical reset and power retention |
| `owner` | codex |

(status-soc-refinement-soc-20)=
## SOC-20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-20 |
| `priority` | P1 |
| `scope` | optimization |
| `task` | G474 UART bounded critical sections |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32g474/drivers/uart.c |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/g474_provider_test.cpp |
| `exit_condition` | No runtime modulo and explicit bounded blackout preserving arbitrary capacity loss order |
| `owner` | codex |

(status-soc-refinement-soc-21)=
## SOC-21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-21 |
| `priority` | P2 |
| `scope` | optimization |
| `task` | GD selected EXTI dispatch |
| `implementation_status` | implemented |
| `software_status` | selected_profiles_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/gd32f470/drivers/exti.c;tools/configure/providers/gd32f470 |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/gd32_exti_test.cpp;tools/configure/test_gd32_exti_generation.py |
| `exit_condition` | Measured selected-line costs preserve shared priority pending clear and owner isolation |
| `owner` | codex |

(status-soc-refinement-soc-22)=
## SOC-22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-22 |
| `priority` | P1 |
| `scope` | maintenance |
| `task` | IRQ service drain budgets |
| `implementation_status` | contract_implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc;docs/design/soc-refinement.md |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/soc-execution-budgets.md |
| `exit_condition` | Modes document work bounds service obligations and timeout consequences |
| `owner` | codex |

(status-soc-refinement-soc-23)=
## SOC-23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-23 |
| `priority` | P1 |
| `scope` | measurement |
| `task` | Hot-path instruction baselines |
| `implementation_status` | measurement_tool_implemented |
| `software_status` | selected_profiles_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/soc_costs.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_costs.py |
| `exit_condition` | Fixed compiler optimization actual disassembly records change cost without invented cycles |
| `owner` | codex |

(status-soc-refinement-soc-24)=
## SOC-24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-24 |
| `priority` | P1 |
| `scope` | feature |
| `task` | F407 CCM placement |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32f407;tools/configure/memory.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/stm32_memory_boot_test.cpp;scripts/ci/stm32_memory.py |
| `exit_condition` | Explicit selected data bss noinit init budget bounds and DMA rejection with default zero cost |
| `owner` | codex |

(status-soc-refinement-soc-25)=
## SOC-25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-25 |
| `priority` | P1 |
| `scope` | defense |
| `task` | GD RAM initializer consistency |
| `implementation_status` | implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/gd32f470/memory_init.c;tools/configure/memory.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tests/google/gd32_memory_boot_test.cpp;scripts/ci/gd32_memory.py |
| `exit_condition` | Linker facts boot spans and first-write validation agree preserving noinit |
| `owner` | codex |

(status-soc-refinement-soc-26)=
## SOC-26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-26 |
| `priority` | P2 |
| `scope` | maintenance |
| `task` | Shared controller engine ownership |
| `implementation_status` | contract_implemented |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/soc-refinement.md;soc/common;io/private |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/delivery/architecture.md;docs/design/soc-refinement.md |
| `exit_condition` | Public private directory target SDK boundaries and selected retention stay explicit |
| `owner` | codex |

(status-soc-refinement-soc-27)=
## SOC-27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-27 |
| `priority` | P1 |
| `scope` | automation |
| `task` | Per-mode image cost deltas |
| `implementation_status` | measurement_tool_implemented |
| `software_status` | selected_profiles_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/soc_costs.py;tools/testing/test_soc_costs.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_costs.py |
| `exit_condition` | Comparable empty selected backend mode ELF budgets with explained growth |
| `owner` | codex |

(status-soc-refinement-soc-28)=
## SOC-28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-28 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Additional fixed clock profiles |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32h743/system.c;arch/cortex_m/nx_arch_cache_control.c;soc/stm32u575/system.c;tools/configure/test_clock_sources.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;soc/stm32h743/README.md;soc/stm32u575/README.md;tests/google/h743_system_test.cpp;scripts/ci/stm32h743.py;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md |
| `exit_condition` | An exact reviewed oscillator profile has implementation rollback and HIL |
| `owner` | codex |

(status-soc-refinement-soc-29)=
## SOC-29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-29 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Additional G474 peripheral modes |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32g474/drivers/exti.c;soc/stm32g474/soc.json;soc/stm32g474/routes.json;tools/configure/providers/stm32g474 |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/stm32g474.md;tests/google/g474_exti_test.cpp;tests/google/g474_factory_test.cpp;tests/contracts/g474_assembly;scripts/ci/provider_assemblies.py;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md |
| `exit_condition` | Each selected controller mode closes facts provider generated model ARM and HIL |
| `owner` | codex |

(status-soc-refinement-soc-30)=
## SOC-30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-30 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Additional controllers routes packages |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32g474;soc/stm32h743;soc/stm32u575;boards;tests/contracts/g474_assembly/exti_board |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;soc/stm32g474/routes.json;soc/stm32h743/routes.json;soc/stm32u575/routes.json;docs/design/stm32g474.md;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md |
| `exit_condition` | Exact hardware and independent resources budgets Board sources and isolation reviewed |
| `owner` | codex |

(status-soc-refinement-soc-31)=
## SOC-31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-31 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Non-M4 full SoC integration |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32h743;soc/stm32u575;arch;os |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;soc/stm32h743/README.md;soc/stm32u575/README.md;scripts/ci/stm32h743.py;scripts/ci/stm32u575.py;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md |
| `exit_condition` | Exact selected MCU startup clock memory IRQ peripherals OS and unit-specific HIL |
| `owner` | codex |

(status-soc-refinement-soc-32)=
## SOC-32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-32 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Complex DMA cache security domains |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | arch/cortex_m/nx_arch_cache_control.c;arch/cortex_m/nx_arch_nonsecure.c;soc/stm32h743/drivers/uart_dma.c;soc/stm32h743/system.c;soc/stm32u575/security.c;tools/configure/memory.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;soc/stm32h743/README.md;tests/google/arch_cache_control_test.cpp;tests/google/h743_dma_test.cpp;tests/google/h743_dma_factory_test.cpp;tests/contracts/h743_assembly;scripts/ci/cache_control_compile.py;scripts/ci/stm32h743.py;docs/design/u575-trustzone-contract.md;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md |
| `exit_condition` | Per-engine reachability alignment cache security faults verified on exact target |
| `owner` | codex |

(status-soc-refinement-soc-33)=
## SOC-33

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-33 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Bootloader and running-state handoff |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;docs/design/integration-contracts.md;components/stm32f407_handoff;soc/stm32f407/handoff.c;tests/google/stm32f407_handoff_test.cpp |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/soc-refinement.md;docs/design/io-extension-boundaries.md;tools/testing/evidence/io-extensions-20261020/README.md |
| `exit_condition` | Clock VTOR masks FP SysTick DMA RAM and live owners have exact entry contract |
| `owner` | codex |

(status-soc-refinement-soc-34)=
## SOC-34

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-34 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Dynamic frequency changes |
| `implementation_status` | not_implemented |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | soc |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/soc-refinement.md |
| `exit_condition` | Consumer protocol reconfiguration electrical order rollback and deadline handling verified |
| `owner` | codex |

(status-soc-refinement-soc-35)=
## SOC-35

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-35 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Deep STOP continuous time |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `implementation_paths` | components/stm32u575_retained_time;components/stm32u575_stop2;soc/stm32u575/private/stm32u575_rtc_calendar.h;soc/stm32u575/stop2.c;tests/google/u575_retained_time_test.cpp;tests/google/u575_stop2_test.cpp |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/timebase-lowpower.md;docs/design/u575-retained-time.md;tests/google/u575_retained_time_test.cpp;tests/google/u575_stop2_test.cpp |
| `exit_condition` | Exact RTC LPTIM retention wake recovery precision and power physically verified |
| `owner` | codex |

(status-soc-refinement-soc-36)=
## SOC-36

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-36 |
| `priority` | P2 |
| `scope` | conditional_extension |
| `task` | Userspace MPU TrustZone transport |
| `implementation_status` | partial_reference_implemented |
| `software_status` | reference_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | arch/cortex_m/nx_arch_nonsecure.c;soc/stm32u575/security.c;components/user_io_broker;tests/contracts/u575_trustzone;tests/google/user_io_async_test.cpp |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;docs/design/u575-trustzone-contract.md;docs/design/user-io-broker.md;tests/google/arch_nonsecure_test.cpp;tests/google/u575_security_test.cpp;tests/contracts/u575_trustzone/README.md;scripts/ci/u575_trustzone.py;docs/delivery/soc-extensions-results.json;docs/delivery/soc-extensions-verification.md;tests/google/user_io_async_test.cpp |
| `exit_condition` | Actual permissions address copy borrow async exit gateway and hardware isolation verified |
| `owner` | codex |

(status-soc-refinement-soc-37)=
## SOC-37

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-37 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Station binding |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-38)=
## SOC-38

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-38 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | PCB electrical facts |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-39)=
## SOC-39

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-39 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Fixed clock profile |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-40)=
## SOC-40

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-40 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Startup rollback faults |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-41)=
## SOC-41

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-41 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Monotonic time |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-42)=
## SOC-42

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-42 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | RAM DMA domains |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-43)=
## SOC-43

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-43 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | UART |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-44)=
## SOC-44

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-44 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | SPI |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-45)=
## SOC-45

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-45 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | I2C |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-46)=
## SOC-46

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-46 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | ADC |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-47)=
## SOC-47

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-47 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | EXTI capture |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-48)=
## SOC-48

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-48 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | PWM break |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-49)=
## SOC-49

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-49 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Classic CAN |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-50)=
## SOC-50

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-50 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | USB FS device |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-51)=
## SOC-51

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-51 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Ethernet PHY |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-52)=
## SOC-52

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-52 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Flash power loss |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-53)=
## SOC-53

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-53 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Watchdog |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-54)=
## SOC-54

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-54 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Shallow sleep tickless |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |

(status-soc-refinement-soc-55)=
## SOC-55

| 原字段 | 原值 |
| --- | --- |
| `task_id` | SOC-55 |
| `priority` | HIL |
| `scope` | physical_acceptance |
| `task` | Load endurance |
| `implementation_status` | tooling_implemented |
| `software_status` | tooling_software_validated |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/soc_acceptance.py |
| `evidence_paths` | docs/delivery/soc-refinement-results.json;docs/delivery/soc-refinement-verification.md;tools/testing/test_soc_hil.py;tools/hil/README.md |
| `exit_condition` | Exact admitted station selected modes raw instrument capture and case-specific assertions pass |
| `owner` | codex |
