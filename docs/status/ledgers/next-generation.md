---
id: status.ledgers.next-generation
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-next-generation)=
# 原平台重构台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/next-generation-execution.csv)。
源文件：`docs/design/next-generation-execution.csv`；SHA-256：`e2aa73331dd8779fd8f3802f358f09d5a197baccb565ded86982b5860472937b`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 33 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-next-generation-ng-001)=
## NG-001

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-001 |
| `batch` | B0 |
| `priority` | P0 |
| `area` | architecture |
| `task` | 冻结第一性原理契约与范围 |
| `depends_on` | （原字段为空） |
| `primary_role` | TL |
| `review_roles` | K1;B1;I1;Q1;R1 |
| `deliverables` | ADR;架构蓝图;接入/工程契约 |
| `software_exit` | 默认静态模型与managed边界一致;所有能力区分planned/implemented/qualified;冻结EXTI边沿/PWM/ADC单次scan/IWDG首发模式及deferred扩展 |
| `hardware_exit` | 不适用;不以设计计硬件资格 |
| `status` | implemented |
| `legacy_rf_refs` | RF-ARC-01 |
| `implementation_paths` | docs/design/next-generation-platform.md;docs/design/integration-contracts.md;docs/delivery/architecture.md |
| `evidence_paths` | docs/delivery/architecture.md;docs/delivery/support.md |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-002)=
## NG-002

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-002 |
| `batch` | B0 |
| `priority` | P0 |
| `area` | measurement |
| `task` | 定义最小workload与资源/时序预算schema |
| `depends_on` | NG-001 |
| `primary_role` | Q1 |
| `review_roles` | TL;K2;B1;B2 |
| `deliverables` | 最小GPIO/UART/SPI workload;成本口径;预算schema |
| `software_exit` | 真实基线输入和工具/配置/产物可绑定;不能以旧完整Board镜像代替最小成本 |
| `hardware_exit` | cycles/IRQ/端到端预算在真实station校准;当前未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BAS-01;RF-HIL-03 |
| `implementation_paths` | tools/measurement/compare.py;tools/measurement/resources.py;tools/measurement/workload.c |
| `evidence_paths` | build/measurement-qiming-v3/matrix.json;build/measurement-sky-v1/matrix.json;build/measurement-liangshan-v5/matrix.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-003)=
## NG-003

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-003 |
| `batch` | B1 |
| `priority` | P0 |
| `area` | tooling |
| `task` | 锁定正式工具环境与唯一dev入口 |
| `depends_on` | NG-001 |
| `primary_role` | R1 |
| `review_roles` | K1;Q1 |
| `deliverables` | 工具/源依赖锁;OCI环境;薄dev入口;基础formatter/warning/local checks |
| `software_exit` | fresh checkout用同一入口configure/build/test;原生命令与退出码保留;首个owned切片即执行格式与基础warning门禁;使用根.clang-format/.editorconfig现有80列风格而非另起配置 |
| `hardware_exit` | 不适用 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BLD-01;RF-CI-01 |
| `implementation_paths` | tools/dev/dev.py;tools/dev/Dockerfile;dependencies/environment.lock.json;scripts/setup/install_dev_tools.py |
| `evidence_paths` | dependencies/environment.lock.json;tools/dev/dev.py doctor;Git hooks/style gate |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-004)=
## NG-004

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-004 |
| `batch` | B1 |
| `priority` | P0 |
| `area` | configuration |
| `task` | 实现SoC/Board/assembly到单resolved校验 |
| `depends_on` | NG-001;NG-003 |
| `primary_role` | R1 |
| `review_roles` | B1;B2;K1;Q1 |
| `deliverables` | schema;窄resolver;负配置用例;原子生成 |
| `software_exit` | 未知键/非法模式/资源冲突/失效输入真实拒绝;CMake代码依赖仍唯一权威 |
| `hardware_exit` | 资料与实际PCB一致性待实板 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BRD-01;RF-BLD-01 |
| `implementation_paths` | tools/configure/configure.py;soc/native/soc.json;soc/*/*/soc.json;soc/native/routes.json;soc/*/*/routes.json;boards/*/board.json |
| `evidence_paths` | tests/contracts:contract_configuration_rejection;tools/configure/test_configure.py |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-005)=
## NG-005

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-005 |
| `batch` | B1 |
| `priority` | P0 |
| `area` | build |
| `task` | 实现静态绑定与最小firmware target |
| `depends_on` | NG-004 |
| `primary_role` | R1 |
| `review_roles` | K1;B1;B2 |
| `deliverables` | 配置头;binding C;linker输入;显式targets |
| `software_exit` | 两SoC最小bootstrap链接ELF含正确startup/vector/ABI;不冒充完整clock/rollback;生成器不生成业务或软件依赖图;最小Flash LOAD/RAM/MSP报告与预算比较 |
| `hardware_exit` | 启动时序和电气待实板 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BLD-01;RF-BLD-02 |
| `implementation_paths` | tools/configure/bindings.py;cmake/platform/Firmware.cmake;CMakeLists.txt |
| `evidence_paths` | six ARM preset ELF/BIN/map/resource reports;tests/contracts/*_assembly |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-006)=
## NG-006

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-006 |
| `batch` | B1 |
| `priority` | P1 |
| `area` | sdk |
| `task` | 定义外部源码SDK消费与依赖锁合同 |
| `depends_on` | NG-003;NG-005 |
| `primary_role` | R1 |
| `review_roles` | I1;Q1 |
| `deliverables` | 源码package入口;输入/许可manifest;外部consumer |
| `software_exit` | 独立移动路径consumer真实build/run/link;dirty包标不可发布 |
| `hardware_exit` | 不适用;包消费不构成硬件资格 |
| `status` | implemented |
| `legacy_rf_refs` | RF-SDK-01;RF-SDK-02 |
| `implementation_paths` | cmake/package/package_source_sdk.py;cmake/package/NexusConfig.cmake.in;tests/contracts/test_source_sdk.py |
| `evidence_paths` | tests/contracts/test_source_sdk.py;clean SDK relocated consumer pending |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-007)=
## NG-007

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-007 |
| `batch` | B2 |
| `priority` | P0 |
| `area` | arch |
| `task` | 实现最小Cortex-M4原语与monotonic时间合同 |
| `depends_on` | NG-005 |
| `primary_role` | K1 |
| `review_roles` | K2;B1;B2;Q1 |
| `deliverables` | 异常/临界区/atomic边界;cycle snapshot;时钟域 |
| `software_exit` | 保存恢复incoming mask;时间wrap/clock变化语义;无假时钟/未审屏障删除 |
| `hardware_exit` | 测量mask/cycle开销;timer精度;IRQ抢占 |
| `status` | implemented |
| `legacy_rf_refs` | RF-ARC-01;RF-OS-01 |
| `implementation_paths` | arch/include/nexus/arch/arch.h;arch/arm/cortex_m/nx_arch_cortex_m.c;core/src/time.c |
| `evidence_paths` | tests/contracts/core_contract_test.c;tests/google/arch_cortex_test.cpp;tests/google/arch_native_test.cpp;tests/google/gd32_arch_test.cpp;tests/contracts/gd32_system_test.c;tests/contracts/stm32_model |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-008)=
## NG-008

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-008 |
| `batch` | B2 |
| `priority` | P0 |
| `area` | soc |
| `task` | 接入STM32F407精确variant基础设施 |
| `depends_on` | NG-007 |
| `primary_role` | B1 |
| `review_roles` | K1;Q2 |
| `deliverables` | VE/ZG memory/clock/IRQ/startup;私有SDK;失败回滚 |
| `software_exit` | 精确density/ABI输入闭合;真实ELF;partial-init故障可回滚或保留责任 |
| `hardware_exit` | 复位/clock/Flash density/safe-output资格 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BOOT-01;RF-FIX-01;RF-FIX-02 |
| `implementation_paths` | soc/stm32/stm32f407/system.c;soc/stm32/stm32f407/soc.json;soc/stm32/stm32f407/CMakeLists.txt |
| `evidence_paths` | tests/contracts/stm32_io_test.c;tests/contracts/stm32_model;tests/contracts/stm32_assembly |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-009)=
## NG-009

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-009 |
| `batch` | B2 |
| `priority` | P0 |
| `area` | soc |
| `task` | 接入GD32F470ZG基础设施 |
| `depends_on` | NG-007 |
| `primary_role` | B2 |
| `review_roles` | K1;Q2 |
| `deliverables` | memory/clock/IRQ/startup;官方SDK lock;失败回滚 |
| `software_exit` | 真实ELF;精确芯片模式;时钟/IRQ/reset错误可诊断 |
| `hardware_exit` | 复位/clock/Flash density/safe-output资格 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BRD-02 |
| `implementation_paths` | soc/gd32/gd32f470/system.c;soc/gd32/gd32f470/timebase.c;soc/gd32/gd32f470/soc.json;vendors/gigadevice/gd32f4xx/source.lock.json |
| `evidence_paths` | tests/contracts/gd32_system_test.c;tests/contracts/gd32_io_test.c;tests/contracts/gd32_assembly |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-010)=
## NG-010

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-010 |
| `batch` | B2 |
| `priority` | P0 |
| `area` | board |
| `task` | 重建三首发Board只读资源包 |
| `depends_on` | NG-004;NG-008;NG-009 |
| `primary_role` | B1;B2 |
| `review_roles` | Q2;R1 |
| `deliverables` | 三Board facts/provenance;特殊电气hook;输入摘要 |
| `software_exit` | 同SoC已审路线换板无需修改controller;声明与实际操作一致;未知PCB/route显式 |
| `hardware_exit` | 实际PCB/晶振/接线/初值复核;当前未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BRD-01;RF-HIL-01 |
| `implementation_paths` | boards/stm32f407_qiming_v31/board.json;boards/stm32f407ve_sky_qingchun/board.json;boards/gd32f470_liangshan/board.json |
| `evidence_paths` | boards/*/README.md;tools/hil/fixtures;unknown PCB remains explicit |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-011)=
## NG-011

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-011 |
| `batch` | B2 |
| `priority` | P0 |
| `area` | gpio |
| `task` | 实现固定typed GPIO与授权mask批量操作 |
| `depends_on` | NG-010 |
| `primary_role` | K1 |
| `review_roles` | B1;B2;Q1 |
| `deliverables` | 固定GPIO合同;SoC实现;Native模型;batch |
| `software_exit` | 热路径无注册/字符串/通用ref/OS锁;原子set/reset;停机quiescence前提明确;mask子集与set/reset重叠拒绝;toggle/read语义;首切片map/预算比较 |
| `hardware_exit` | GPIO波形/复位初值/并发电气;批量cycles |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-FIX-02 |
| `implementation_paths` | io/include/nexus/io/gpio.h;soc/*/*/drivers/gpio.c;soc/native/drivers/gpio.c |
| `evidence_paths` | tests/contracts/native_io_test.c;tests/contracts/stm32_io_test.c;tests/contracts/gd32_io_test.c;actual resource matrices |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-012)=
## NG-012

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-012 |
| `batch` | B3 |
| `priority` | P0 |
| `area` | request |
| `task` | 实现caller-owned请求规则与通知port |
| `depends_on` | NG-007;NG-011 |
| `primary_role` | K2 |
| `review_roles` | K1;Q1;I1 |
| `deliverables` | admission/borrow/deadline/result合同;必要state helper |
| `software_exit` | REJECTED零借用;单owner取消/跨任务token寿命;全链唯一admission;SETTLED最后访问;wait进展/arm-recheck;必要最小竞态回归在首个切片执行;共享client不service;stop期间执行者持续drain后才join |
| `hardware_exit` | 真实IRQ/DMA drain证明在各provider验收 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-OS-01 |
| `implementation_paths` | core/include/nexus/core/request.h;core/src/request.c;os/wait.c |
| `evidence_paths` | tests/contracts/core_contract_test.c;tests/contracts/components_owner.c |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-013)=
## NG-013

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-013 |
| `batch` | B3 |
| `priority` | P0 |
| `area` | uart |
| `task` | 重建STM32 UART IRQ与接收profile |
| `depends_on` | NG-008;NG-012 |
| `primary_role` | B1 |
| `review_roles` | K2;I1;Q2 |
| `deliverables` | 直接typed UART;byte/event profile;按实例storage |
| `software_exit` | 真实IRQ实现编译;partial start/cancel/late IRQ/通知丢失模型;无闲置DMA/legacy池 |
| `hardware_exit` | TC/abort/RX丢失/时间精度/吞吐;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-02;RF-FIX-03;RF-HIL-02 |
| `implementation_paths` | soc/stm32/stm32f407/drivers/uart.c;io/include/nexus/io/uart.h |
| `evidence_paths` | tests/contracts/stm32_io_test.c;tests/contracts/stm32_model;actual ARM presets |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-014)=
## NG-014

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-014 |
| `batch` | B3 |
| `priority` | P0 |
| `area` | uart |
| `task` | 重建GD32 UART IRQ与接收profile |
| `depends_on` | NG-009;NG-012 |
| `primary_role` | B2 |
| `review_roles` | K2;I1;Q2 |
| `deliverables` | 直接typed UART;byte/event profile;按实例storage |
| `software_exit` | 错误/取消/TC合同与STM公共行为一致;能力差异显式 |
| `hardware_exit` | TC/abort/RX丢失/时间精度/吞吐;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-BRD-02;RF-HIL-02 |
| `implementation_paths` | soc/gd32/gd32f470/drivers/uart.c;io/include/nexus/io/uart.h |
| `evidence_paths` | tests/contracts/gd32_io_test.c;contract_gd32_io;actual ARM preset |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-015)=
## NG-015

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-015 |
| `batch` | B3 |
| `priority` | P0 |
| `area` | spi |
| `task` | 实现单总线执行者与SPI真实执行模式 |
| `depends_on` | NG-010;NG-012 |
| `primary_role` | B1;B2 |
| `review_roles` | K2;I2;Q2 |
| `deliverables` | 静态endpoint;短polling;按需求IRQ/DMA;无固定copy池 |
| `software_exit` | 一个wire-active;CS/config稳定;绝对deadline包含排队;service不伪异步 |
| `hardware_exit` | CS/clock/MISO/DMA abort/drain;只对实测模式资格化 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-HAL-04;RF-HIL-02 |
| `implementation_paths` | soc/*/*/drivers/spi.c;components/bus_owner/src/spi_owner.c;soc/native/drivers/spi.c |
| `evidence_paths` | tests/contracts/components_spi.c;tests/contracts/*_io_test.c;tests/contracts/*_assembly/*spi.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-016)=
## NG-016

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-016 |
| `batch` | B3 |
| `priority` | P1 |
| `area` | i2c |
| `task` | 实现明确范围的MCU I2C provider |
| `depends_on` | NG-010;NG-012 |
| `primary_role` | B1;B2 |
| `review_roles` | K2;I2;Q2 |
| `deliverables` | I2C endpoint;reviewed routes;transaction/STOP/recovery |
| `software_exit` | ACK/NACK/STOP/repeated START/故障范围明确;实际ARM链接;无成功stub |
| `hardware_exit` | 真实连线/总线挂死/recovery/波形;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-03;RF-HIL-02 |
| `implementation_paths` | soc/*/*/drivers/i2c.c;tools/configure/*_bindings.py;soc/native/drivers/i2c.c |
| `evidence_paths` | tests/contracts/*_io_test.c;tests/contracts/*_assembly/*i2c.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-017)=
## NG-017

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-017 |
| `batch` | B3 |
| `priority` | P0 |
| `area` | flash |
| `task` | 重建全物理Flash与受限region接口 |
| `depends_on` | NG-008;NG-009;NG-012 |
| `primary_role` | K2 |
| `review_roles` | B1;B2;I2;Q2 |
| `deliverables` | 精确几何;region端口;脉冲deadline/错误合同 |
| `software_exit` | 范围/overflow/erase alignment拒绝;不预留产品区;结束前硬件不再访问 |
| `hardware_exit` | erase/program/功耗电压/掉电;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-FLS-01;RF-FLS-02;RF-FLS-03 |
| `implementation_paths` | soc/*/*/drivers/flash.c;io/src/mechanisms/flash_region.c;io/include/nexus/io/flash.h |
| `evidence_paths` | tests/contracts/*_io_test.c;tests/contracts/components_flash_storage.c;tests/contracts/components_storage.c |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-018)=
## NG-018

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-018 |
| `batch` | B3 |
| `priority` | P1 |
| `area` | watchdog |
| `task` | 实现watchdog与reset-cause垂直合同 |
| `depends_on` | NG-010;NG-012 |
| `primary_role` | B1;B2 |
| `review_roles` | K1;I2;Q2 |
| `deliverables` | IWDG配置/feed/reset cause;真实SoC实现;Native模型 |
| `software_exit` | 两个SoC真实provider和ELF;范围/时钟/调试冻结语义;产品feed政策留外部;启用不可逆/已有状态/启动失败的remaining-effect正确记录 |
| `hardware_exit` | 真实超时复位/调试冻结/上电reset cause;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-ARC-01;RF-HIL-02;RF-HIL-03 |
| `implementation_paths` | soc/*/*/drivers/watchdog.c;soc/native/drivers/watchdog.c;io/include/nexus/io/watchdog.h |
| `evidence_paths` | tests/contracts/*_io_test.c;contract_gd32_watchdog-late-failure;contract_gd32_watchdog-hardware-enabled |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-031)=
## NG-031

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-031 |
| `batch` | B3 |
| `priority` | P1 |
| `area` | exti |
| `task` | 实现静态EXTI line与共享vector事件 |
| `depends_on` | NG-010;NG-011;NG-012 |
| `primary_role` | B1;B2 |
| `review_roles` | K1;Q1;Q2 |
| `deliverables` | reviewed line/edge;共享vector;有界事件;停止合同 |
| `software_exit` | 真实SoC provider/ELF;重复line拒绝;事件溢出/排序/stop排空;未实现模式拒绝 |
| `hardware_exit` | 真实edge/去抖策略边界/优先级/丢失与排空;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-HIL-02 |
| `implementation_paths` | soc/*/*/drivers/exti.c;io/include/nexus/io/exti.h;soc/native/drivers/exti.c |
| `evidence_paths` | tests/contracts/*_io_test.c;tests/contracts/*_assembly/*exti.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-032)=
## NG-032

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-032 |
| `batch` | B3 |
| `priority` | P1 |
| `area` | timer |
| `task` | 实现固定timer时基与PWM输出 |
| `depends_on` | NG-007;NG-010;NG-012 |
| `primary_role` | B1;B2 |
| `review_roles` | K1;I2;Q2 |
| `deliverables` | 选定timer/channel;period/duty;电气初值;后续capture/trigger资源边界 |
| `software_exit` | 首发固定timer/PWM真实provider/ELF;overflow/range/原子更新;timebase冲突拒绝;共享PSC/ARR/base mode与0/100pct/shadow更新明确;capture/advanced模式另立项不计首发退出 |
| `hardware_exit` | 真实周期/占空比/停止输出;首发模式未执行;capture/advanced另验收 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-HIL-02;RF-HIL-03 |
| `implementation_paths` | soc/*/*/drivers/timer.c;io/include/nexus/io/timer.h;soc/native/drivers/timer.c |
| `evidence_paths` | tests/contracts/*_io_test.c;tests/contracts/*_assembly/*pwm.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-033)=
## NG-033

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-033 |
| `batch` | B3 |
| `priority` | P1 |
| `area` | adc |
| `task` | 实现ADC单次与低速scan |
| `depends_on` | NG-010;NG-012;NG-032 |
| `primary_role` | B1;B2 |
| `review_roles` | K2;I2;Q2 |
| `deliverables` | reviewed ADC channel/sequence;选定采样模式;校准;buffer合同 |
| `software_exit` | 首发single-shot/低速scan真实provider/ELF;样本格式/range/overflow;未选模式不驻留;参考电压/计数/sample time/超时输出有效性明确;trigger/DMA/block合同另立项不计首发退出 |
| `hardware_exit` | 首发真实计数/精度/采样率/超时;未执行;高频trigger/DMA/block独立验收 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-HIL-02;RF-HIL-03 |
| `implementation_paths` | soc/*/*/drivers/adc.c;io/include/nexus/io/adc.h;soc/native/drivers/adc.c |
| `evidence_paths` | tests/contracts/*_io_test.c;tests/contracts/*_assembly/*adc*.json |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-019)=
## NG-019

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-019 |
| `batch` | B4 |
| `priority` | P0 |
| `area` | os |
| `task` | 实现薄wait/wake与逐对象静态OS storage |
| `depends_on` | NG-007;NG-012 |
| `primary_role` | K1 |
| `review_roles` | K2;Q1;R1 |
| `deliverables` | 裸机/FreeRTOS/Native能力;静态task/queue参数 |
| `software_exit` | 不强制driver依赖OS;无统一最大池;delete/join回收时点正确;ISR ceiling拒绝 |
| `hardware_exit` | 真实scheduler/IRQ wake/stack/FPU;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-OS-01;RF-OS-02 |
| `implementation_paths` | os/baremetal/notify.c;os/native/native.c;os/freertos/freertos.c;os/README.md |
| `evidence_paths` | tests/contracts/os_wait.c;tests/contracts/os_freertos_runtime;build/ng-agent-cmake/evidence/freertos-100-fresh-processes.log |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-020)=
## NG-020

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-020 |
| `batch` | B4 |
| `priority` | P1 |
| `area` | integration |
| `task` | 实现有界多producer适配与总线owner样例 |
| `depends_on` | NG-015;NG-019 |
| `primary_role` | I1 |
| `review_roles` | K2;Q1 |
| `deliverables` | 显式queue/service adapter;可选callback adapter |
| `software_exit` | 无隐藏worker;adapter已admit后start失败仍有归属;旧cancel不命中新epoch;SETTLED后通知不访问request;唯一owner等待仍drain;通知满/丢失无lost-wake;client等待不接管执行;producer等待SETTLED时stop不死锁;wake发布者退出后才回收通知storage |
| `hardware_exit` | 最长service间隔/负载队列/公平性按产品实测 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-02;RF-CMP-01 |
| `implementation_paths` | components/bus_owner/src/bus_owner.c;components/bus_owner/src/uart_owner.c;components/bus_owner/src/spi_owner.c |
| `evidence_paths` | tests/contracts/components_owner.c;tests/contracts/components_uart.c;tests/contracts/components_spi.c |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-021)=
## NG-021

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-021 |
| `batch` | B4 |
| `priority` | P1 |
| `area` | devices |
| `task` | 实现一个可复用外部器件driver垂直切片 |
| `depends_on` | NG-015;NG-020 |
| `primary_role` | I2 |
| `review_roles` | B1;B2;Q1 |
| `deliverables` | 一个SPI器件的core/transport;外部composition;I2C器件按实际需要另选 |
| `software_exit` | 无Board pin/默认任务;chip-ID/解析/失败/请求寿命正确;无未reviewed route假支持 |
| `hardware_exit` | 连接/波形/测量误差/长期负载;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-CMP-01 |
| `implementation_paths` | components/bmp280/src/bmp280.c;components/bmp280/src/spi_transport.c |
| `evidence_paths` | tests/contracts/components_bmp280.c;tests/contracts/components_spi.c |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-022)=
## NG-022

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-022 |
| `batch` | B4 |
| `priority` | P1 |
| `area` | components |
| `task` | 重建通用协议/存储/日志窄端口边界 |
| `depends_on` | NG-017;NG-019;NG-020 |
| `primary_role` | I1;I2 |
| `review_roles` | K2;Q1 |
| `deliverables` | 实际选入组件core和adapter;删除默认worker与全局发现 |
| `software_exit` | 协议与OS/Board解耦;格式/恢复正确;错误可释放;未用组件不驻留 |
| `hardware_exit` | wire timing/持久化掉电/负载按组件资格 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-02;RF-CMP-01;RF-FIX-04;RF-FIX-05 |
| `implementation_paths` | components/modbus;components/storage;components/log;components/README.md |
| `evidence_paths` | tests/contracts/components_modbus.c;tests/contracts/components_storage.c;tests/contracts/components_log.c |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-023)=
## NG-023

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-023 |
| `batch` | B4 |
| `priority` | P0 |
| `area` | examples |
| `task` | 迁移三Board裸机/FreeRTOS外部examples |
| `depends_on` | NG-006;NG-013;NG-014;NG-019 |
| `primary_role` | I1 |
| `review_roles` | I2;R1;Q1 |
| `deliverables` | 六基础firmware配置;SDK pin/callers;说明 |
| `software_exit` | 真实外部build/Native运行/六基础ARM ELF;功能仅按支持route扩展 |
| `hardware_exit` | 实物启动/UART及选择用例;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-EX-01;RF-BLD-02 |
| `implementation_paths` | X-Gen-Lab/nexus-examples:assemblies;X-Gen-Lab/nexus-examples:apps;X-Gen-Lab/nexus-examples:scripts |
| `evidence_paths` | external build/nextgen-development/native/ctest-results.xml;external build/nextgen-development/arm-development.json;clean pin pending |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-024)=
## NG-024

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-024 |
| `batch` | B5 |
| `priority` | P0 |
| `area` | engineering |
| `task` | 扩展本地/PR/nightly风险门禁与证据 |
| `depends_on` | NG-003;NG-004;NG-012 |
| `primary_role` | R1 |
| `review_roles` | TL;K1;Q1 |
| `deliverables` | 精选analyzer;现有Doxygen合同模板;完整CI风险矩阵;既有formatter/warning门禁扩展;格式目录和入口一致 |
| `software_exit` | owned warning阻断;public/private隔离;不以路径过滤漏高风险;失效报告拒绝;新旧owned目录均纳入相同格式范围;反斜杠Doxygen/文件头/header-source分工复核;第三方排除 |
| `hardware_exit` | 不适用 |
| `status` | implemented |
| `legacy_rf_refs` | RF-CI-01;RF-REL-02 |
| `implementation_paths` | scripts/ci/style_gate.py;scripts/ci/quality_tools.py;.github/workflows;Doxyfile;docs/sphinx |
| `evidence_paths` | whole-file format/comments;compile_commands analyzer;actual Doxygen/Sphinx;CI workflow tests |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-025)=
## NG-025

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-025 |
| `batch` | B5 |
| `priority` | P0 |
| `area` | verification |
| `task` | 建立新契约故障/并发/边界回归 |
| `depends_on` | NG-012;NG-013;NG-014;NG-015;NG-017;NG-019;NG-018;NG-031;NG-032;NG-033;NG-016;NG-021;NG-022 |
| `primary_role` | Q1 |
| `review_roles` | K2;B1;B2 |
| `deliverables` | host/model/fuzz;sanitizer;边界和late-event用例 |
| `software_exit` | 非零真实执行;partial start/abort失败/timeout/notification满/stop竞争;失败不释放借用 |
| `hardware_exit` | 模型不升格硬件;具体物理项在HIL |
| `status` | implemented |
| `legacy_rf_refs` | RF-BAS-01;RF-HIL-03 |
| `implementation_paths` | tests/contracts;tests/contracts/os_freertos_runtime;tests/contracts/stm32_model;tests/contracts/gd32_model |
| `evidence_paths` | actual Native/model CTest and sanitizer JUnit;tools unit suites |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-026)=
## NG-026

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-026 |
| `batch` | B5 |
| `priority` | P0 |
| `area` | performance |
| `task` | 实现ELF资源预算与受控benchmark工具 |
| `depends_on` | NG-002;NG-011;NG-023;NG-018;NG-031;NG-032;NG-033;NG-016;NG-021;NG-022 |
| `primary_role` | Q1;R1 |
| `review_roles` | K1;B1;B2 |
| `deliverables` | 资源schema报告;最小image delta;O2/Os/O3/LTO比较 |
| `software_exit` | 静态预算越界真实阻断;stack/instruction与cycles分开;同artifact身份 |
| `hardware_exit` | 专用station测cycles/latency/high-water/丢失/负载;未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HIL-03;RF-BLD-02 |
| `implementation_paths` | tools/measurement/resources.py;tools/measurement/compare.py;tools/schemas/resource-budget.schema.json |
| `evidence_paths` | 60 actual O2/Os/O3 with/without LTO images;18 GC checks;tests/contracts/test_measurement_*.py |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-027)=
## NG-027

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-027 |
| `batch` | B5 |
| `priority` | P0 |
| `area` | hil |
| `task` | 重建HIL准入与三Board工装准备 |
| `depends_on` | NG-010;NG-023;NG-026 |
| `primary_role` | Q2 |
| `review_roles` | B1;B2;R1 |
| `deliverables` | station/fixture/probe/serial工具;raw evidence schema |
| `software_exit` | 错Board/ELF/config/空station真实拒绝;软件工具准备完成;不执行未接入设备 |
| `hardware_exit` | 真实probe/刷写/读回/waveform/断电/负载需接入后验收 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HIL-01;RF-HIL-02;RF-HIL-03 |
| `implementation_paths` | tools/hil/admit.py;tools/hil/fixtures;tests/contracts/test_hil_admission.py |
| `evidence_paths` | tests/contracts/test_hil_admission.py;three fixtures;incomplete station rejection;physical not executed |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_executed |

(status-next-generation-ng-028)=
## NG-028

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-028 |
| `batch` | B5 |
| `priority` | P1 |
| `area` | reproducibility |
| `task` | 独立验收hermetic与可复现构建 |
| `depends_on` | NG-003;NG-006;NG-024 |
| `primary_role` | R1 |
| `review_roles` | Q1;TL |
| `deliverables` | 离线正式build;路径归一;独立clean rebuild比较 |
| `software_exit` | 网络关闭与声明输入边界;双clean ELF/BIN确定性比较;不把hash称可复现 |
| `hardware_exit` | 不适用 |
| `status` | implemented |
| `legacy_rf_refs` | RF-SDK-02;RF-REL-01 |
| `implementation_paths` | tools/evidence/container.py;tools/evidence/reproduce.py;tests/contracts/test_repro_execution.py |
| `evidence_paths` | tools/evidence/reproduce.py;delivery bundle/reproducibility*.json;two offline source SDK builds |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-029)=
## NG-029

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-029 |
| `batch` | B6 |
| `priority` | P0 |
| `area` | migration |
| `task` | 删除旧运行架构并原子迁移消费路径 |
| `depends_on` | NG-023;NG-024;NG-025;NG-026 |
| `primary_role` | TL;K1 |
| `review_roles` | I1;B1;B2;R1;Q1 |
| `deliverables` | 旧API/targets/scripts/docs清理;源码迁移说明 |
| `software_exit` | 无长期双注册/对象/OS队列/配置入口;外部pin一致;持久化迁移另立合同 |
| `hardware_exit` | 已部署产品需独立迁移/恢复与物理回归 |
| `status` | implemented |
| `legacy_rf_refs` | RF-HAL-01;RF-HAL-02;RF-ARC-01 |
| `implementation_paths` | docs/delivery/migration.md;CMakeLists.txt;tools/configure;docs/archive |
| `evidence_paths` | current target/include/script graph;docs/archive;external caller migration development builds |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |

(status-next-generation-ng-030)=
## NG-030

| 原字段 | 原值 |
| --- | --- |
| `task_id` | NG-030 |
| `batch` | B6 |
| `priority` | P0 |
| `area` | delivery |
| `task` | 封存新软件候选及精确支持证据 |
| `depends_on` | NG-006;NG-023;NG-025;NG-026;NG-027;NG-028;NG-029;NG-018;NG-031;NG-032;NG-033 |
| `primary_role` | R1;Q1 |
| `review_roles` | TL;Q2;I1 |
| `deliverables` | source/config/tool/dependency/ELF包;支持tuple;known limits |
| `software_exit` | 当前完整源码资格;源码SDK干净外部消费;同hash候选;软件/物理资格分列 |
| `hardware_exit` | 实板资格与产品release promotion另验收;当前未执行 |
| `status` | implemented |
| `legacy_rf_refs` | RF-REL-01;RF-REL-02 |
| `implementation_paths` | tools/evidence/candidate.py;tools/evidence/proof.py;tools/evidence/qualification.py;docs/delivery/qualification.md |
| `evidence_paths` | tools/evidence/candidate.py;delivery bundle/candidate/candidate.json;six actual qualification scopes |
| `software_status` | qualification_record_authority |
| `hardware_status` | not_applicable |
