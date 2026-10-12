# 当前设计与实施台账

当前代码采用统一物理目录、TOML 静态装配、typed factory 和共享多态方法表。
公共 C11 face 只保存 readonly ops 与私有 context；每个实例拥有独立状态，request 和
block storage 由调用者提供。SDK 和 provider storage 私有，应用产品工程独立。
这里的设计合同与 [当前交付](../delivery/README.md) 一起使用，历史文档不建立当前资格。

| 文档 | 用途 |
|---|---|
| [架构蓝图](next-generation-platform.md) | 原理、执行、所有权、性能/空间与支持边界 |
| [工程手册](engineering-handbook.md) | 既有格式/注释、工具、自动化、证据与发布 |
| [接入契约](integration-contracts.md) | SoC/Board/器件/组件/外部工程的严格资源合同 |
| [OS 执行与生命周期合同](os-contracts.md) | deadline、IRQ policy、任务/队列、低功耗与扩展边界 |
| [后端接入与执行探针](os-backend-integration.md) | 新 RTOS 的能力合同、实际执行和不得自动扩大支持 |
| [ThreadX 配置与接入](threadx.md) | 官方真实 Linux/M4 内核、独立 typed policy、bootstrap、静态对象与精确支持范围 |
| [浅睡眠 timebase](timebase-lowpower.md) | 显式 FreeRTOS selector、固定 clock/compare alarm、量化 phase、失败保留与物理边界 |
| [Classic CAN](classic-can.md) | 三邮箱、共享 filter domain、controller-local abort/recovery 与 Board 电气 handoff |
| [USB FS device](usb-device.md) | PIO packet/endpoint 所有权、独立 48 MHz owner、control epoch 与完整停止 |
| [Raw Ethernet / MDIO](ethernet.md) | caller descriptor/RX loan、RMII、可选 DP83848；协议与 PHY/Board 政策外置 |
| [高级 timer](timer-advanced.md) | capture 事件、complementary PWM/deadtime/break；共享资源与实际安全边界 |
| [用户 I/O broker](user-io-broker.md) | kernel-invoked scalar GPIO、可信 principal/pin/copy；未接 userspace SVC/MPU/TZ |
| [选定装配资格门禁](provider-assemblies.md) | 逐个 authored fixture 实际 ARM 构建、资源预算、链接保留和不可变输入 |
| [多控制器](multiple-controllers.md) | 独立 provider/IRQ/DMA 所有权；配置开放与硅片实现分别记录 |
| [STM32G474 初始接入](stm32g474.md) | 精确型号、MB1367 来源、clock/startup/time/GPIO/LPUART 纵切片与未维护功能 |
| [I2C 总线恢复](i2c-bus-clear.md) | 调用者独占 GPIO、有限时序与失败后继续清理的合同 |
| [UART DMA 与 RS485](uart-dma-rs485.md) | 有限/连续接收、双向 DMA、流控和真实 TC/DE 时序 |
| [OS01–OS55 台账](os-execution.csv) | 每项实际机制、证据及软件/物理状态 |
| [Arch 与 CPU runtime 契约](arch-contracts.md) | 九核软件、原子操作、cache/MPU/SAU 和安全边界 |
| [33项实施台账](next-generation-execution.csv) | 实现文件、证据入口、软件与物理状态分列 |
| [本次 P0–P5 台账](factory-platform-execution.csv) | factory、TOML、TDD、多实例与 IRQ/DMA/block 实现 |
| [故障观察与停机](fault-observation.md) | 无状态观察窄端口、保持中断屏蔽、外部安全输出与实板边界 |
| [I/O 生命周期检查](io-lifecycle-matrix.md) | 203 项有界性质、真实生产编译、同一测试执行和来源校验 |
| [60项完善台账](platform-refinement-execution.csv) | 本轮实现、软件执行、实板与外部阻塞分别记录；全部维护责任由Codex承担 |
| [实际架构](../delivery/architecture.md) | 当前目录、公共 factory 和多态接口、依赖方向 |
| [当前资格边界](../delivery/qualification.md) | 开发执行、clean软件候选、实板pending的区别 |

台账保留B0–B6和原始退出条件，新增implementation_paths/evidence_paths、software_status、
hardware_status。implemented表示机制已经存在，不表示所有退出条件已passed；开发验证
不能替代clean-source SDK、正式离线双构建和候选封存。NG028/030的正式执行以新生成
证据更新，硬件未接入，全部适用物理项not_executed。Glob表示同一机制的明确目录族，
不代表递归发现或缺省构建；X-Gen-Lab/nexus-examples路径是独立外部仓库。

CPU 软件层覆盖 M0/M0+/M3/M4/M7/M23/M33/M55/M85，通过显式 CPU/IRQ facts
选择原子后端、ABI、可选 FPU/MVE/cache/MPU/SAU 与裸机/FreeRTOS port。外部
CPU runtime 不生成 SoC/Board/factory；ThreadX 另有明确的 Native/M4 子集，不能继承
九核 FreeRTOS port 范围。配置示例与剩余条件见
[九核支持说明](../delivery/cortex-m.md)。这不扩大完整平台的芯片/板卡范围。

首发为启明STM32F407ZGT6、天空星STM32F407VET6和梁山派GD32F470ZGT6，裸机/
FreeRTOS、显式 ThreadX 及Native模型。新增模式和 G474 纵切片按各文档的配置、
完整镜像与 SDK 退出条件验收，不从公共类型推断全芯片接入。
三Board未审实物PCB事实仍标unknown；软件测试夹具不升格真实
Board路线。角色沿用十人配置：TL1、Core/I/O/OS2、BSP2、通用应用/通信2、QA/HIL2、
构建发布1；维护责任由Codex全部承担，无需实名人员。远程branch protection状态
须通过GitHub独立核实，不能从本地角色文件推断。

行为变更采用 RED → GREEN → REFACTOR；主机 GoogleTest/GoogleMock 测试真实生产
provider、factory 与 owner，配置工具用 Python unittest。commit hook 和 CI 执行同一
门禁，生产 MCU target 不链接测试框架。构建期资源验证、实际寄存器模型、ARM ELF
预算与物理 HIL 分别建立证据，方法表开销由实际 linked image 测量。

- [Source compatibility and SDK versions](source-compatibility.md)

- [SoC refinement and 55-item ledger](soc-refinement.md)
