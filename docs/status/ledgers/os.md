---
id: status.ledgers.os
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-os)=
# OS 原专项台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/os-execution.csv)。
源文件：`docs/design/os-execution.csv`；SHA-256：`c8fbbdb7d609f42bb838acdf6f5e808eaed943c13c307abf2455ca2f3e3f209c`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

原台账未定义单独退出字段。本页保留任务与 qualification_authority 等
原字段；具体合同和实际验证需按原路径复核，不补写另一份退出条件。

本表包含 55 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-os-os01)=
## OS01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS01 |
| `priority` | P1 |
| `task` | Finite deadline under continuous unrelated wake |
| `implementation_paths` | os/baremetal/notify.c;os/native/native.c;os/freertos/freertos.c |
| `evidence_paths` | tests/google/os_wait_test.cpp;tests/google/freertos_context_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os02)=
## OS02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS02 |
| `priority` | P1 |
| `task` | Final observed completion wins timeout competition |
| `implementation_paths` | os/wait.c;os/include/nexus/os/wait.h |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os03)=
## OS03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS03 |
| `priority` | P1 |
| `task` | Unified wait deadline/error/context contract |
| `implementation_paths` | os/include/nexus/os/wait.h;docs/design/os-contracts.md |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os04)=
## OS04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS04 |
| `priority` | P1 |
| `task` | Actual port startup stack minimum and alignment |
| `implementation_paths` | os/freertos/freertos.c;tools/configure/kernel.py |
| `evidence_paths` | tests/google/freertos_context_test.cpp;tools/configure/test_kernel.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os05)=
## OS05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS05 |
| `priority` | P1 |
| `task` | Task rejection before object mutation/kernel entry |
| `implementation_paths` | os/freertos/freertos.c;os/freertos/include/nexus/os/freertos.h |
| `evidence_paths` | tests/google/freertos_context_test.cpp |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os06)=
## OS06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS06 |
| `priority` | P1 |
| `task` | Baseline complete IRQ wake binding |
| `implementation_paths` | io/src/mechanisms/wake.c;soc/native/drivers/uart.c |
| `evidence_paths` | tests/google/wake_test.cpp;tests/google/irq_wake_test.cpp;tests/google/irq_wake_stm32_test.cpp;tests/google/irq_wake_gd32_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os07)=
## OS07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS07 |
| `priority` | P1 |
| `task` | Explicit NONE/PRIMASK/BASEPRI kernel policy |
| `implementation_paths` | io/include/nexus/io/wake.h;io/src/mechanisms/wake.c;tools/configure/kernel.py |
| `evidence_paths` | tests/google/wake_test.cpp;tools/configure/test_kernel.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os08)=
## OS08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS08 |
| `priority` | P1 |
| `task` | SysTick exact clock/rate/reload validation |
| `implementation_paths` | tools/configure/kernel.py;os/freertos/include/FreeRTOSConfig.h |
| `evidence_paths` | tools/configure/test_kernel.py;scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os09)=
## OS09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS09 |
| `priority` | P1 |
| `task` | External Tick strong setup hook |
| `implementation_paths` | os/freertos/tick.c;os/freertos/include/nexus/os/tick.h |
| `evidence_paths` | scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os10)=
## OS10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS10 |
| `priority` | P2 |
| `task` | Notification publisher/waiter lifecycle |
| `implementation_paths` | os/include/nexus/os/wait.h;os/native/include/nexus/os/native.h;docs/design/os-contracts.md |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os11)=
## OS11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS11 |
| `priority` | P2 |
| `task` | Single lifecycle owner and single joiner |
| `implementation_paths` | os/native/include/nexus/os/native.h;os/freertos/include/nexus/os/freertos.h |
| `evidence_paths` | tests/google/os_wait_test.cpp;tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os12)=
## OS12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS12 |
| `priority` | P2 |
| `task` | Native external cancellation/detach exclusion |
| `implementation_paths` | os/native/include/nexus/os/native.h;docs/design/os-contracts.md |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os13)=
## OS13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS13 |
| `priority` | P2 |
| `task` | Explicit queue close/drain/destroy |
| `implementation_paths` | os/freertos/include/nexus/os/freertos.h;os/freertos/freertos.c;os/native/native.c |
| `evidence_paths` | tests/contracts/os_freertos_runtime;tests/google/freertos_context_test.cpp |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os14)=
## OS14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS14 |
| `priority` | P2 |
| `task` | Queue absolute deadlines without repeated relative extension |
| `implementation_paths` | os/freertos/freertos.c;os/native/native.c |
| `evidence_paths` | tests/google/freertos_context_test.cpp;tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os15)=
## OS15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS15 |
| `priority` | P2 |
| `task` | Owner progresses during producer stop/drain/join |
| `implementation_paths` | components/bus_owner/src/bus_owner.c;X-Gen-Lab/nexus-examples/apps/uart_owner_runtime |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os16)=
## OS16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS16 |
| `priority` | P2 |
| `task` | SETTLED distinct from notification reclamation |
| `implementation_paths` | docs/design/os-contracts.md;os/include/nexus/os/wait.h |
| `evidence_paths` | tests/google/os_wait_test.cpp;X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os17)=
## OS17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS17 |
| `priority` | P2 |
| `task` | Error ownership and recovery obligations |
| `implementation_paths` | docs/design/os-contracts.md;docs/delivery/os.md |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os18)=
## OS18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS18 |
| `priority` | P2 |
| `task` | Maintained OS profiles and immutable IR |
| `implementation_paths` | tools/configure/kernel.py;tools/configure/ir.py;tools/configure/runtime.py |
| `evidence_paths` | tools/configure/test_kernel.py;scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os19)=
## OS19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS19 |
| `priority` | P2 |
| `task` | Separate CPU/IRQ facts from kernel policy |
| `implementation_paths` | tools/configure/kernel.py;tools/configure/cpu.py |
| `evidence_paths` | tools/configure/test_kernel.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os20)=
## OS20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS20 |
| `priority` | P2 |
| `task` | Optional kernel capability pruning |
| `implementation_paths` | os/freertos/include/FreeRTOSConfig.h;tools/configure/kernel.py |
| `evidence_paths` | scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os21)=
## OS21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS21 |
| `priority` | P2 |
| `task` | Explicit Idle stack and task name budgets |
| `implementation_paths` | tools/configure/kernel.py;os/freertos/hooks.c |
| `evidence_paths` | tools/configure/test_kernel.py;scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os22)=
## OS22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS22 |
| `priority` | P2 |
| `task` | Direct task notification optional adapter |
| `implementation_paths` | os/freertos/freertos.c;os/freertos/include/nexus/os/freertos.h |
| `evidence_paths` | tests/google/freertos_context_test.cpp;tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os23)=
## OS23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS23 |
| `priority` | P2 |
| `task` | Permanent task without join semaphore |
| `implementation_paths` | os/freertos/freertos.c;os/freertos/include/nexus/os/freertos.h |
| `evidence_paths` | tests/google/freertos_context_test.cpp;tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os24)=
## OS24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS24 |
| `priority` | P2 |
| `task` | Measure shared wait table before ABI change |
| `implementation_paths` | scripts/ci/os_wait_cost.py;tests/contracts/os_wait_cost |
| `evidence_paths` | scripts/ci/test_os_wait_cost.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os25)=
## OS25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS25 |
| `priority` | P2 |
| `task` | Stable operation descriptors rather than payload copy |
| `implementation_paths` | components/bus_owner/include/nexus/components/bus_owner.h;X-Gen-Lab/nexus-examples/apps/uart_owner_runtime |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os26)=
## OS26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS26 |
| `priority` | P2 |
| `task` | Actual OS object and linked image cost report |
| `implementation_paths` | scripts/ci/os_resources.py;scripts/ci/os_profiles.py;scripts/ci/os_wait_cost.py |
| `evidence_paths` | tests/contracts/os_wait_cost;scripts/ci/test_os_resources.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os27)=
## OS27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS27 |
| `priority` | P2 |
| `task` | Hot path instruction costs and measurement definitions |
| `implementation_paths` | scripts/ci/os_resources.py;scripts/ci/os_wait_cost.py;tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py;scripts/ci/test_os_resources.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os28)=
## OS28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS28 |
| `priority` | P1 |
| `task` | Deadline GoogleTest/GoogleMock regression |
| `implementation_paths` | tests/google/os_wait_test.cpp;tests/google/freertos_context_test.cpp |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os29)=
## OS29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS29 |
| `priority` | P1 |
| `task` | Startup stack/alignment/zero-effect regression |
| `implementation_paths` | tests/google/freertos_context_test.cpp;tools/configure/test_kernel.py |
| `evidence_paths` | tests/google/freertos_context_test.cpp |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os30)=
## OS30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS30 |
| `priority` | P1 |
| `task` | Baseline facts to provider wake regression |
| `implementation_paths` | tests/google/wake_test.cpp;tests/google/irq_wake_test.cpp;tests/google/irq_wake_stm32_test.cpp;tests/google/irq_wake_gd32_test.cpp |
| `evidence_paths` | tests/google/wake_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os31)=
## OS31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS31 |
| `priority` | P2 |
| `task` | Deterministic wait races and publisher isolation |
| `implementation_paths` | tests/google/os_wait_test.cpp;tests/google/freertos_context_test.cpp |
| `evidence_paths` | tests/google/os_wait_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os32)=
## OS32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS32 |
| `priority` | P2 |
| `task` | Queue close broadcast and drain regression |
| `implementation_paths` | tests/contracts/os_freertos_runtime;tests/google/freertos_context_test.cpp |
| `evidence_paths` | tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os33)=
## OS33

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS33 |
| `priority` | P2 |
| `task` | Returning task timeout/reuse/storage regression |
| `implementation_paths` | tests/contracts/os_freertos_runtime;tests/google/freertos_context_test.cpp |
| `evidence_paths` | tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os34)=
## OS34

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS34 |
| `priority` | P2 |
| `task` | Controlled concurrency and sanitizer validation |
| `implementation_paths` | tests/google/os_wait_test.cpp;X-Gen-Lab/nexus-examples/apps/uart_owner_runtime;docs/design/os-contracts.md |
| `evidence_paths` | tests/google/os_wait_test.cpp;X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os35)=
## OS35

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS35 |
| `priority` | P2 |
| `task` | Profiles included in permanent CI and SDK checks |
| `implementation_paths` | scripts/ci/os_profiles.py;.github/workflows |
| `evidence_paths` | scripts/ci/os_profiles.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os36)=
## OS36

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS36 |
| `priority` | P2 |
| `task` | Actual pinned POSIX kernel lifecycle integration |
| `implementation_paths` | tests/contracts/os_freertos_runtime |
| `evidence_paths` | tests/contracts/os_freertos_runtime |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os37)=
## OS37

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS37 |
| `priority` | P2 |
| `task` | Current OS contracts and support boundaries |
| `implementation_paths` | docs/design/os-contracts.md;docs/delivery/os.md;os/README.md;docs/guides/os-shutdown.md |
| `evidence_paths` | docs/design/os-execution.csv |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os38)=
## OS38

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS38 |
| `priority` | P2 |
| `task` | Kernel/port/overlay upgrade review workflow |
| `implementation_paths` | docs/design/os-contracts.md;os/freertos/prepare_m7_integer.py;dependencies |
| `evidence_paths` | os/freertos/test_m7_integer_overlay.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os39)=
## OS39

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS39 |
| `priority` | P2 |
| `task` | Backend acceptance contract and checklist |
| `implementation_paths` | docs/design/os-contracts.md;docs/delivery/os.md;docs/design/os-backend-integration.md |
| `evidence_paths` | tests/google/os_wait_test.cpp;tests/contracts/os_freertos_runtime;scripts/ci/os_backend_probe.py;scripts/ci/test_os_backend_probe.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os40)=
## OS40

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS40 |
| `priority` | P2 |
| `task` | External multiproducer IRQ UART owner example |
| `implementation_paths` | X-Gen-Lab/nexus-examples/apps/uart_owner_runtime |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os41)=
## OS41

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS41 |
| `priority` | P2 |
| `task` | External admission/drain/join/reclaim example |
| `implementation_paths` | X-Gen-Lab/nexus-examples/apps/uart_owner_runtime/lifecycle.c |
| `evidence_paths` | X-Gen-Lab/nexus-examples/tests/owner_lifecycle_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os42)=
## OS42

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS42 |
| `priority` | P2 |
| `task` | Bounded failure hooks with product-owned policy |
| `implementation_paths` | os/freertos/hooks.c;docs/design/os-contracts.md |
| `evidence_paths` | tools/hil/os_acceptance.py |
| `implementation_status` | implemented |
| `software_status` | pending_exact_head_qualification |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os43)=
## OS43

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS43 |
| `priority` | optional |
| `task` | CPU-local atomic shallow idle |
| `implementation_paths` | arch/include/nexus/arch/sleep.h;arch/arm/cortex_m/nx_arch_sleep.c |
| `evidence_paths` | tests/google/arch_sleep_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os44)=
## OS44

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS44 |
| `priority` | optional |
| `task` | Explicit FreeRTOS custom tickless timer port |
| `implementation_paths` | os/freertos/lowpower.c;os/freertos/include/nexus/os/lowpower.h |
| `evidence_paths` | tests/google/os_lowpower_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os45)=
## OS45

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS45 |
| `priority` | optional |
| `task` | Caller-sized diagnostics and actual trace hooks |
| `implementation_paths` | os/freertos/diagnostics.c;os/include/nexus/os/diagnostic.h;os/freertos/include/nexus/os/freertos_diagnostics.h |
| `evidence_paths` | tests/google/os_diagnostic_test.cpp |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os46)=
## OS46

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS46 |
| `priority` | optional |
| `task` | MPU user task kernel gateway |
| `implementation_paths` | os/freertos/mpu;os/freertos/include/nexus/os/freertos_mpu.h;os/freertos/include/nexus/os/freertos_user.h |
| `evidence_paths` | tests/google/freertos_mpu_test.cpp;scripts/ci/os_mpu.py;scripts/ci/test_os_mpu.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os47)=
## OS47

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS47 |
| `priority` | optional |
| `task` | TrustZone cross-world task service |
| `implementation_paths` | os/freertos/secure;os/freertos/include/nexus/os/secure_context.h;cmake/platform/Runtime.cmake |
| `evidence_paths` | tests/google/os_secure_context_test.cpp;tests/contracts/os_secure_runtime;scripts/ci/os_secure_runtime.py;scripts/ci/test_os_secure_runtime.py |
| `implementation_status` | implemented |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os48)=
## OS48

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS48 |
| `priority` | optional |
| `task` | Other RTOS backend admission contract |
| `implementation_paths` | docs/design/os-backend-integration.md;scripts/ci/os_backend_probe.py;tests/contracts/os_backend_probe |
| `evidence_paths` | scripts/ci/test_os_backend_probe.py;tests/google/os_wait_test.cpp |
| `implementation_status` | implemented_contract |
| `software_status` | no_additional_kernel_selected |
| `hardware_status` | not_applicable |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os49)=
## OS49

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS49 |
| `priority` | physical |
| `task` | Actual IRQ to task wake latency |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os50)=
## OS50

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS50 |
| `priority` | physical |
| `task` | Integer FP MVE register retention |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os51)=
## OS51

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS51 |
| `priority` | physical |
| `task` | Task stack and exception MSP peaks |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os52)=
## OS52

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS52 |
| `priority` | physical |
| `task` | Tick frequency and deadline accuracy |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os53)=
## OS53

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS53 |
| `priority` | physical |
| `task` | Lifecycle full-load stress and borrow settlement |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os54)=
## OS54

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS54 |
| `priority` | physical |
| `task` | Low-power restore clock/events |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |

(status-os-os55)=
## OS55

| 原字段 | 原值 |
| --- | --- |
| `task_id` | OS55 |
| `priority` | physical |
| `task` | Observed assert/overflow safe output |
| `implementation_paths` | tools/hil/os_acceptance.py |
| `evidence_paths` | tools/testing/test_os_acceptance.py |
| `implementation_status` | implemented_tooling |
| `software_status` | development_passed |
| `hardware_status` | not_executed |
| `qualification_authority` | fresh exact source/SDK/ELF evidence; physical requires admitted equipment records |
