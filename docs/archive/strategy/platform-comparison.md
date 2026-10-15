# 主流嵌入式平台比较与 Nexus 底座选择

检索日期：2026-10-08。状态：选型分析，供架构评审；没有决定迁移，也没有新增平台支持承诺。

本文面向工业控制与设备联网、约 10 人、STM32/GD32、3–6 个月交付窗口。FreeRTOS 与裸机是当前建设目标；精确 GD32 型号、产品控制预算、板卡修订及成员既有技能仍需确认。

关联现有决策：ADR 002 分层、ADR 003 每构建配置、ADR 004 实时契约、ADR 005 按需组件、ADR 006 同工件发布。关联任务：BAS-004、HAL-001/004/005、OS-001、BSP-001/003/004、HIL-001/002、REL-003。

## 1. 证据口径与比较范围

- **文档事实**：下文引用官方文档或官方源码，说明平台公开的设计与工具机制。
- **Nexus 判断**：根据团队、目标硬件和交付窗口作出的工程判断，不代表官方推荐或已测量结果。
- 本轮仅检索与源码审查，没有安装这些平台、移植 Nexus、跑分或执行物理 HIL。
- 不用示例内存数字推算本产品成本；Flash/RAM、启动时间、jitter、取消上界均须按相同产品切片实测。
- 文档的 `latest`、`stable`、`main`、`master` 是检索入口，不是可重现构建的版本锁。实验与交付必须固定实际 release/commit、工具链及依赖。
- 官方列有芯片或开发板，不等于本产品 PCB、全部外设、驱动模式或工业验收已经获得支持。

比较五类问题：OS 依赖方向、控制器与设备模型、arch/SoC/board 分工、配置与组件构建、团队维护和扩展。

## 2. Zephyr：完整 RTOS 与硬件描述体系

**文档事实。** Zephyr 的设备模型属于其内核体系，驱动使用按外设类型定义的 API；设备将只读配置与运行态数据分开。初始化区分不能使用内核服务的 `PRE_KERNEL_1/2` 与可以使用内核服务的 `POST_KERNEL`，也支持显式延迟初始化。这表明初始化阶段是一项契约，不能仅靠链接顺序假定驱动已可用。[Device Driver Model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)

**文档事实。** 硬件模型区分 board、revision、SoC、可选 series/family、CPU cluster/core 与 architecture。板卡定义可包含元数据、DTS、Kconfig、构建与烧录配置；当前硬件模型从 Zephyr 3.7 起替换旧模型，迁移自定义板有真实维护成本。[Board Porting Guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html)

**文档事实。** Devicetree 主要描述硬件及其启动配置，Kconfig 主要选择进入镜像的软件功能。外设数量、地址、引脚及具体实例属于硬件描述；是否纳入网络或驱动代码属于软件选择。[Devicetree versus Kconfig](https://docs.zephyrproject.org/latest/build/dts/dt-vs-kconfig.html)

**文档事实。** 外部模块可通过 `zephyr/module.yml` 纳入构建；模块并不强制依赖 west。west manifest 管理多个仓库的版本关系，可以冻结解析后的版本。因此组件集成与多仓库版本管理是不同职责。[Modules](https://docs.zephyrproject.org/latest/develop/modules.html)、[West Manifests](https://docs.zephyrproject.org/latest/develop/west/manifest.html)

**文档事实。** 官方有 [STM32F4 Discovery](https://docs.zephyrproject.org/latest/boards/st/stm32f4_disco/doc/index.html) 与 [GD32F407V-START](https://docs.zephyrproject.org/latest/boards/gd/gd32f407v_start/doc/index.html) 的独立板卡页和外设表；应逐项检查实际所需接口及目标版本。

**Nexus 判断。** 适合借鉴设备描述与运行态分离、板卡修订、显式初始化阶段、硬件与软件配置分工、外部模块和固定依赖。采用 Zephyr 底座意味着接受其内核、驱动和构建模型；它不是在现有 FreeRTOS/裸机下直接替换一个头文件。

**Nexus 判断。** 当产品需要持续扩展网络、USB、蓝牙、文件系统、电源管理或多处理器功能时，上游集成可能减少长期自维护面积。十人团队不宜复制全部 Zephyr 机制再维护一套自有实现；若采用底座，应尽量直接使用其平台 API，把 Nexus 保留在产品领域、特定契约和交付证据层。

## 3. RT-Thread：内核对象、设备框架与组件生态

**文档事实。** RT-Thread 的传统 I/O 设备模型派生自内核对象，提供注册、查找、打开、读写、控制等操作。它依赖 RT-Thread 对象管理，不能据此声称其设备框架可以原样运行在无内核的裸机环境。[I/O Device Framework](https://www.rt-thread.io/document/site/programming-manual/device/device/)

**文档事实。** SPI 模型区分 bus 与 slave device，设备持有独立配置及片选信息。官方当前 API 文档说明传输接口调用 `rt_mutex_take()`，不能从 ISR 直接调用；上下文和同步规则需要显式传递到应用设计。[SPI device](https://rt-thread.github.io/rt-thread/page_device_spi.html)

**文档事实。** 当前官方 API 文档已经包含 `RT_USING_DM` 的设备树驱动模型：SPI 控制器注册后可扫描有 `compatible` 的子节点，芯片驱动可通过 `RT_SPI_DRIVER_EXPORT` 匹配；未启用 DM 的 BSP 仍可手动挂载。不能沿用“RT-Thread 没有设备树”的旧结论。[SPI device model (DM)](https://rt-thread.github.io/rt-thread/page_device_spi_dm.html)、[Devicetree Compiler](https://rt-thread.github.io/rt-thread/page_device_dtc.html)

**文档事实。** MCU 组织的主要入口是 `libcpu`、`bsp`、`components`、内核 `src/include`；Env/Kconfig 用于裁剪，SCons/SConscript 用于构建和生成 IDE 工程。官方 Changelog 也记录 CMake generator 维护，不能描述为“仅支持 SCons”。具体板卡仍需核对采用哪条受维护构建链。[Source catalogue](https://www.rt-thread.io/document/site/)、[SCons](https://rt-thread.github.io/rt-thread/page_scons.html)、[官方 Changelog](https://github.com/RT-Thread/rt-thread/blob/master/ChangeLog.md)

**Nexus 判断。** 适合借鉴 bus/device 分离、片选和设备配置归属、驱动上下文说明、组件裁剪及 BSP 模板。新的 DM/OFW 能力值得评估，但不能因为存在框架就假定首发 STM32/GD32 BSP 已完整适配。

**Nexus 判断。** 若团队已有 RT-Thread 项目经验、产品愿意统一内核且所需 BSP/软件包成熟，它可以是合理底座。选择后仍需管理包版本、许可、安全更新和驱动差异；同时维护传统挂载与新 DM 两套路径会扩大首期验证面积。

## 4. ESP-IDF：厂商平台的组件与依赖治理

**文档事实。** ESP-IDF 将组件编译为静态库，通过 CMake 注册源文件、公开包含目录和依赖。`REQUIRES` 表达公共头暴露的依赖，`PRIV_REQUIRES` 表达实现依赖；Kconfig 定义组件配置，Component Manager 产生 `dependencies.lock`。[Build System](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)

**文档事实。** 硬件抽象从寄存器与 LL、HAL 到 driver 分层，按 target 选择实现并公开外设能力。官方同时提醒部分 HAL/LL API 不属于稳定公共 API，因此直接使用底层接口需要承担版本变化风险。[Hardware Abstraction](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hardware-abstraction.html)

**文档事实。** ESP-IDF 集成自己的 FreeRTOS 实现；其 SMP 配置与 vanilla FreeRTOS 有内核行为和 API 差异。SPI 主驱动区分 host、bus、device、transaction；不同 device 可共享 bus，但多个任务访问同一 device 仍需满足其同步规则。[FreeRTOS (IDF)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html)、[SPI Master Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html)

**Nexus 判断。** 组件的公共/私有依赖、版本锁、能力描述及明确的总线事务边界非常值得借鉴。Nexus 可采用这些构建原则，而无需引入 ESP-IDF 本身。

**Nexus 判断。** ESP-IDF 面向 Espressif 芯片生态，不是 STM32/GD32 的现成底座。将其内核适配、驱动、启动、Flash/分区和工具链整体迁出会形成自有 fork；对当前团队与窗口不应默认立项。未来若产品使用 ESP 芯片，可独立建立 ESP-IDF 产品 backend，并复用领域协议和证据格式。

## 5. Apache NuttX：POSIX 风格运行环境与上下半部驱动

**文档事实。** NuttX 驱动通常分 upper half 与 hardware-specific lower half；上半部注册设备并提供 read/write/close 等高层接口，通过回调调用硬件实现。这是隔离共享策略与芯片实现的参考。[Device Drivers](https://nuttx.apache.org/docs/latest/components/drivers/index.html)

**文档事实。** `arch/<arch>` 包含架构及芯片逻辑，`boards/<arch>/<chip>/<board>` 负责 OS bring-up 和驱动初始化。官方明确不应将应用可调用的板级业务放入内部 boards 目录，产品相关逻辑可放到 `apps/platform`。[Boards Support](https://nuttx.apache.org/docs/latest/components/boards.html)

**文档事实。** NuttX 使用 Kconfig，当前有 CMake 的 board/config 选择与独立输出目录，也保留 Make 构建。官方 Device Tree 页说明支持 libfdt/FDT 属性解析，但该页尚未将它描述为整个内核框架统一采用的模型；不能说没有设备树，也不能等同 Zephyr 的配置链。[Configuring](https://nuttx.apache.org/docs/latest/quickstart/configuring.html)、[Compiling with CMake](https://nuttx.apache.org/docs/latest/quickstart/compiling_cmake.html)、[Device Tree](https://nuttx.apache.org/docs/latest/guides/devicetree.html)

**Nexus 判断。** upper/lower half、板级初始化与产品业务分离、每配置独立构建目录值得采用。若产品需要大量 POSIX 应用、文件描述符、文件系统和接近嵌入式 Linux 的开发模型，NuttX 应进入候选实验。

**Nexus 判断。** 当前产品尚未提出这些需求，同时保留 FreeRTOS/裸机会增加运行模型与 API 转换负担。因此不把 NuttX 作为默认首期替换方案；这不是对其性能或资源成本的未测量判断。

## 6. CMSIS-Driver / CMSIS-RTOS2：接口规范与内核实现分开

**文档事实。** CMSIS-Driver 指定 Cortex 设备的通用外设接口，明确独立于具体 RTOS。接口包含版本、能力、初始化、电源、非阻塞传输及事件回调；应用选择用 RTOS 等待完成或轮询进度。[CMSIS-Driver Overview](https://arm-software.github.io/CMSIS_6/latest/Driver/index.html)、[Theory of Operation](https://arm-software.github.io/CMSIS_6/latest/Driver/theoryOperation.html)

**文档事实。** CMSIS-RTOS2 是内核上方的标准 API，不是另一个独立内核。RTX 将其作为原生接口，CMSIS-FreeRTOS 提供 FreeRTOS 适配；标准还提供验证套件入口。接口一致性不能替代具体实现与硬件行为验证。[CMSIS-RTOS2 Overview](https://arm-software.github.io/CMSIS_6/main/RTOS2/index.html)

**Nexus 判断。** CMSIS-Driver 的非阻塞硬件接口与可选等待方式，是同时保留裸机与 RTOS 的直接参考。可复用维护中的驱动或建立窄适配，但仍需定义 Nexus 的总线排队、timeout/cancel、buffer ownership 和多设备生命周期。

**Nexus 判断。** 若第三方中间件要求 CMSIS-RTOS2，使用维护中的适配实现通常优于重新实现完整标准。不能默认堆叠 `Nexus OSAL → CMSIS-RTOS2 → FreeRTOS`；只在组件边界需要兼容时使用，并验证优先级、tick 舍入、ISR 与对象销毁语义。

**Nexus 判断。** 裸机 backend 不应模拟成功的线程创建或条件等待。缺失能力明确返回不支持，业务选用显式状态机与单调时间；接口能力查询与 profile 校验决定哪些服务可用。

## 7. 哪些机制进入 Nexus，哪些复杂度暂缓

以下均为 **Nexus 判断**，是待评审的采用范围，不表示代码已落实。

| 机制 | 建议 | 理由与验收方式 |
| --- | --- | --- |
| arch / SoC / board / product 分工 | 继续强化 | 更换 PCB 不修改领域逻辑；换芯片不改变公共 SDK 类型；由依赖与调用者检查证明 |
| controller / bus / device / transaction 分离 | 首期必需 | 双 slave 的 CS、速率、mode 与 generation 隔离；共享 bus 的占用与 deadline 可测 |
| 常量描述与运行态分离 | 首期必需 | board wiring、资源能力只读；状态、锁、队列和 DMA 所有权由实例维护 |
| 驱动 readiness 与初始化阶段 | 首期必需 | 未初始化、依赖失败、重复启动均可诊断；禁止 getter 隐式启动全部系统 |
| Kconfig 选择软件，硬件描述选择连接 | 采用分工原则 | 避免引脚、晶振、DMA 连接散入全局功能开关；生成结果要接受资源冲突校验 |
| 完整 Zephyr DTS 机制自研副本 | 暂缓 | 先评估现有工具或有限、版本化的 board schema；不自研通用 DTS parser/overlay/binding 生态 |
| MCU 动态 probe、热插拔和运行时 DT | 按产品需要 | 固定工业 PCB 可先使用静态目录；不能为了形式现代化引入难以验证的动态状态 |
| target 私有/公共依赖与头文件隔离 | 首期必需 | 借鉴 ESP-IDF；禁用可选服务后仍可链接，厂商与内核头不能泄漏到领域接口 |
| 组件版本锁与外部模块 | 首期必需 | 精确提交、许可、更新责任和配置纳入 BuildIdentity；不必立即拆成大量仓库 |
| 产品系统统一事件总线 | 暂缓 | 控制路径优先使用有界直接端口、快照和队列；跨组件调度需求出现后再决策 |
| POSIX 文件描述符或完整 VFS | 按产品需要 | 用于实际文件/网络生态，不强迫简单 GPIO、DMA 控制都经过通用 read/write |

低层驱动依赖架构原语与芯片 SDK；可选同步层依赖 OSAL；领域与协议依赖窄端口；产品装配选择板卡与 backend。
这是一种目标依赖关系。调度器无关不等于没有同步需求：task、ISR、DMA 完成与取消仍必须由端口提供正确的原子与所有权协议。

## 8. 三条底座路线的真实取舍

成本是团队职责的变化与相对投入判断，未估算未经验证的人月。现有岗位保持 1 名技术负责人、2 名内核接口、2 名 BSP、2 名工业应用、2 名 QA/HIL、1 名构建发布；不分配虚构人员账号。

### A. 采用 Zephyr 底座

- **获得**：复用其内核、设备/硬件配置、驱动、组件和工具机制；可把更多投入转向产品与上游适配。
- **付出**：BSP、初始化、驱动调用、OSAL 语义、配置与构建均需迁移；原来的两种运行模式不能自动保持。
- **内核接口 2 人**：转为 Zephyr 契约审查、组件适配与领域边界；避免包装整个 Zephyr 再暴露全部旧 API。
- **BSP 2 人**：维护板卡/SoC 定义与必要的上游驱动补丁；准确核查首发芯片、DMA 模式和外部器件。
- **QA/HIL 与发布**：更换测试与构建集成入口，但继续负责同工件、产品预算、故障恢复和实验室可信来源。
- **选择触发条件**：产品愿意统一到 Zephyr；必要硬件和协议已有合适实现；小切片移植的缺口小于继续自建共享平台。
- **拒绝或延后条件**：FreeRTOS/裸机是不可改变的产品约束；关键控制/DMA/存储路径需要长期大规模 fork；版本迁移无法覆盖交付窗口。

### B. 采用 RT-Thread 底座

- **获得**：复用内核、设备框架、BSP 模板和组件；团队既有技能与具体软件包可以缩短接入。
- **付出**：从 FreeRTOS/裸机转为 RT-Thread 运行契约，维护 SCons/IDE 或经过核验的 CMake 入口及包管理。
- **内核接口 2 人**：负责内核接口、对象与设备生命周期适配；不复制 RT-Thread 的内核对象系统。
- **BSP 2 人**：选择可维护 BSP，并明确采用手动挂载还是新 DM/OFW；首期限制双路径与多版本并存。
- **QA/HIL 与发布**：将组件锁、实际驱动模式与包来源纳入证据；IDE 工程可用不能替代自动构建和板级验证。
- **选择触发条件**：团队已有维护经验；首发板和所需包覆盖较好；产品接受统一内核且不需要同时交付裸机版。
- **拒绝或延后条件**：需维护多套设备模型或大量 BSP 私有修改；关键包责任不清；切换收益只来自生态数量而没有产品需求证据。

### C. 保留薄 Nexus 平台，使用维护中的 FreeRTOS 与厂商 SDK

- **获得**：保留 FreeRTOS/裸机产品通道，直接控制实时接口及 board/product 边界；复用当前工业与交付工作。
- **付出**：设备目录、总线事务、驱动适配、超时取消、配置与构建治理由团队长期维护；薄抽象不会自动变薄。
- **内核接口 2 人**：只维护必要公共契约、调度/同步适配和有界资源；不自研 RTOS、完整 VFS 或通用中间件生态。
- **BSP 2 人**：承担 STM32/GD32 启动、SDK、IRQ、DMA、Flash、clock、pinmux 的独立差异验证与勘误处理。
- **工业应用 2 人**：优先集成成熟协议、TLS、bootloader 等组件，领域状态机使用窄端口；避免重复实现第三方生态。
- **QA/HIL 与发布**：承担跨 backend 契约矩阵、真实板证据和依赖更新验证；这是该路线的必要持续成本。
- **选择触发条件**：产品功能范围受控，双运行模式仍有价值；平台薄层可用明确预算与组件规则保持边界。
- **退出触发条件**：平台通用基础设施和重复移植持续挤占产品交付；产品需要大量成熟 OS 子系统；跨 backend 最小公分母阻碍需求。

**当前判断。** 在 FreeRTOS/裸机仍是目标、GD32 输入未冻结及 3–6 个月窗口下，C 可以作为近期默认评审起点；这不是对已投入代码的保护承诺。
如果产品愿意统一 RTOS，应在 A/B 与 C 的相同产品切片上比较实际维护差异后重新决定，而非先完成全部自有框架再考虑迁移。

## 9. 决策触发与有限实验

以下是 **Nexus 判断**，建议技术负责人组织一次有时间边界的选型实验；不在本次文档中启动移植。

| 需求或观察 | 对路线的影响 | 必须收集的证据 |
| --- | --- | --- |
| 必须同时交付真正裸机与 FreeRTOS | 支持 C，降低 A/B 直接替换可行性 | 两种运行模式的产品原因、资源与维护价值 |
| 网络/USB/BLE/文件系统等子系统成为主工作量 | 提高 A/B 候选优先级 | 精确芯片、外设、组件版本及实际支持缺口 |
| 团队有持续维护 RT-Thread 或 Zephyr 的经验 | 降低对应学习成本 | 真实维护履历、可交接人员与版本升级责任 |
| 多个产品反复修改相同 board/BSP 代码 | 要求静态描述与组件边界改进 | 第二板/第二产品接入时的改动文件与共享缺陷 |
| 自有抽象需要持续暴露内核特殊能力 | 检查最小公分母是否合理 | 调度、零拷贝、PM、DMA 等确切需求及性能证据 |
| 支持芯片却缺关键 DMA/Flash/协议模式 | 阻断单凭支持表选底座 | 代码审查、相同硬件用例、缺口补丁的维护范围 |

建议实验窗口为 1–2 周，属于规划建议，不是交付保证。最多比较 C 与一个最有条件的成熟底座，避免十人团队同时试做所有平台。

1. 冻结一个 STM32 板卡修订、两只不同参数的 SPI slave、UART 与 Flash 分区；GD32 冻结后作为移植扩展检查。
2. 用同一个领域状态机验证双设备共享 bus、事务完整性、DMA timeout/cancel、一次终态与 buffer 回收。
3. 验证 reset 到 ready、watchdog、参数恢复与管理负载；采用产品给出的 deadline、内存和 Flash 暂停预算。
4. 记录应用与 BSP 改动范围、依赖升级补丁、构建入口数量、编译产物、有效配置、map 与实际执行日志。
5. 比较加入第二板卡与一个真实所需组件时的增量工作，而非只比较 hello world 或 API 形式。
6. 无硬件时可以完成配置、交叉编译和进程/驱动模型实验；物理 IRQ/DMA、电气时序及断电项保持未执行。

路线评审的退出条件是依赖方向、关键能力缺口、维护责任与产品验证计划清楚；不能用一个综合评分隐藏任何阻断项。

## 10. 采用底座后仍由 Nexus 团队承担的责任

- 需求到实现、测试和验收的映射；控制、通信、管理域的实际资源预算。
- PCB revision、实际 silicon UID、烧录读回与运行观察的关联；模型测试与物理 HIL 分开记录。
- 固件、有效配置、工具链与依赖的统一身份；从候选到正式复用同一工件。
- 包许可与安全更新、产品信任根、密钥生命周期、升级恢复与制造身份的审核责任。
- 现场支持窗口、补丁责任、回归板池与可重复故障证据。

成熟平台能减少一部分实现工作；这些产品与交付责任不会由平台名称自动满足。本文没有宣称跑分、功能安全认证、量产就绪或所有 STM32/GD32 型号支持。
