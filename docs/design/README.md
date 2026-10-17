# 当前设计与实施台账

当前代码采用统一物理目录、TOML 静态装配、typed factory 和共享多态方法表。
公共 C11 face 只保存 readonly ops 与私有 context；每个实例拥有独立状态，request 和
block storage 由调用者提供。SDK 和 provider storage 私有，应用产品工程独立。
这里的设计合同与 [当前交付](../delivery/README.md) 一起使用，历史文档不建立当前资格。

| 文档 | 用途 |
|---|---|
| [文档系统架构与交付规划](documentation-system.md) | 读者任务、单一正文、MyST/API、双语、门禁、版本与离线交付的目标方案 |
| [文档系统58项台账](documentation-system-execution.csv) | 保留DOC-01至40并补内容/图示任务；依赖、退出条件与实施/验证分别记录 |
| [完整文档优化清单](documentation-improvements.md) | 从同一台账派生的读者清单，不建立第二份状态权威 |
| [内容与图示标准](documentation-standards.md) | 清晰表达、规范用语、事实/资格、示例、架构图和独立审查约束 |
| [页面模板](documentation-templates.md) | tutorial/how-to/reference/explanation/status必需问题 |
| [当前与目标参考图](../assets/diagrams/README.md) | 18 对本地 SVG/DOT：16 类主图与 2 张补充图；身份与范围由图册清单维护 |
| [文档迁移映射](documentation-migration.csv) | 当前正文、工具和相邻 README 的去向；保留原资格与台账身份 |
| [架构蓝图](next-generation-platform.md) | 原理、执行、所有权、性能/空间与支持边界 |
| [I/O 增量架构](io-evolution.md) | 资源/触发/DMA 计划、可选 traits、硅片准入、数据单位与当前/待实现边界；状态由 IO-01–42 台账维护 |
| [工程手册](engineering-handbook.md) | 既有格式/注释、工具、自动化、证据与发布 |
| [构建系统合同](build-system.md) | 配置事务、增量依赖、目标策略、开发入口与交付边界 |
| [构建系统48项台账](build-system-execution.csv) | 原始退出条件、实现路径与本轮执行证据 |
| [官方驱动与 provider 迭代](official-stm32-provider.md) | 私有 HAL/LL 复用、typed Construction、工作包退出条件及资格边界 |
| [主流 MCU 接入与反馈](mainstream-mcu-adaptation.md) | 八个精确料号、官方与 Zephyr 来源、有限外设边界及跨系列平台反馈 |
| [接入契约](integration-contracts.md) | SoC/Board/器件/组件/外部工程的严格资源合同 |
| [OS 执行与生命周期合同](os-contracts.md) | deadline、IRQ policy、任务/队列、低功耗与扩展边界 |
| [后端接入与执行探针](os-backend-integration.md) | 新 RTOS 的能力合同、实际执行和不得自动扩大支持 |
| [ThreadX 配置与接入](threadx.md) | 官方真实 Linux/M4 内核、独立 typed policy、bootstrap、静态对象与精确支持范围 |
| [浅睡眠 timebase](timebase-lowpower.md) | 显式 FreeRTOS selector、固定 clock/compare alarm、量化 phase、失败保留与物理边界 |
| [Classic CAN](classic-can.md) | 三邮箱、共享 filter domain、controller-local abort/recovery 与 Board 电气 handoff |
| [USB FS device](usb-device.md) | PIO packet/endpoint 所有权、独立 48 MHz owner、control epoch 与完整停止 |
| [Raw Ethernet / MDIO](ethernet.md) | caller descriptor/RX loan、RMII、可选 DP83848；协议与 PHY/Board 政策外置 |
| [高级 timer](timer-advanced.md) | capture 事件、complementary PWM/deadtime/break；共享资源与实际安全边界 |
| [用户 I/O broker](user-io-broker.md) | kernel-invoked scalar GPIO、可信 principal/pin/copy；U575 同步参考 transport 另见双域合同 |
| [选定装配资格门禁](provider-assemblies.md) | 逐个 authored fixture 实际 ARM 构建、资源预算、链接保留和不可变输入 |
| [多控制器](multiple-controllers.md) | 独立 provider/IRQ/DMA 所有权；配置开放与硅片实现分别记录 |
| [STM32G474 接入](stm32g474.md) | 精确型号、MB1367、六 UART、封装 GPIO、有限 SPI、TIM3/4 CH1 PWM、IWDG 与 PA0/PA1 EXTI |
| [STM32H743 接入](../../soc/stm32h743/README.md) | 四种固定 HSI/cache 配置、冷态 cache 所有权、USART3 DMA TX 与有限 RX DMA block；不代表连续 RX |
| [U575 双域参考合同](u575-trustzone-contract.md) | 独立 Secure/NS Runtime、冷态归属封存、固定 NS world 与同步 GPIO；无 RTOS/factory/async/HIL 扩大承诺 |
| [SoC 完善与 55 项台账](soc-refinement.md) | 实现范围、原始退出条件、软件机制与实板验收分列 |
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
FreeRTOS、显式 ThreadX 及 Native 模型。新增 F103RBT6／GD32F303RCT6 参考及
G474／H743／U575 增量按各文档的精确配置、完整镜像与 SDK 退出条件验收，不从公共
类型推断完整系列或全部外设。F103/GD303 增加有限 UART channel-DMA、I2C IRQ、
物理 Flash、单次 ADC 与固定 PWM；GD303 另维护 EXTI 和硬件级联 timebase。
G474 的既有 GPIO/UART/SPI/PWM/IWDG/EXTI 之外，选入 bulb ADC 单次／有限 TIM6
DMA、I2C1 和 ECC Flash。H743 的固定 HSI/cache 支持有限 TX DMA 或有 rearm
间隙的 RX block，可显式初始化 D2 SRAM1；另有七类有限外设的软件夹具。U575
TZEN0 增加 SPI/I2C/ADC/PWM/EXTI/Flash，外部电源与安全状态准入仍适用。
独立无 OS 的 TZEN1 GPIO 参考可复制最多八条命令到 Secure slot，再由调用者逐步
推进；不保留 NS 指针，不代表通用双域 factory、NS RTOS 或异步借用。
ECC `program_flags`、fresh-erase 来源与逐脉冲完成责任见公共 Flash 合同；当前源
实现和最终集成资格分别记录，不继承历史执行计数。
SOC-33–35 仍未实现；SOC-36 保留 partial，SOC-37–55 的物理验收仍为 not_executed。
三Board未审实物PCB事实仍标unknown；软件测试夹具不升格真实
Board路线。角色沿用十人配置：TL1、Core/I/O/OS2、BSP2、通用应用/通信2、QA/HIL2、
构建发布1；维护责任由Codex全部承担，无需实名人员。远程branch protection状态
须通过GitHub独立核实，不能从本地角色文件推断。

行为变更采用 RED → GREEN → REFACTOR；主机 GoogleTest/GoogleMock 测试真实生产
provider、factory 与 owner，配置工具用 Python unittest。commit hook 和 CI 执行同一
门禁，生产 MCU target 不链接测试框架。构建期资源验证、实际寄存器模型、ARM ELF
预算与物理 HIL 分别建立证据，方法表开销由实际 linked image 测量。

- [Source compatibility and SDK versions](source-compatibility.md)
