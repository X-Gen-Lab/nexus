# 工业平台重构交付记录

本轮在 `codex/industrial-platform-modernization` 上实施 Arch / SoC / Board / HAL / OSAL / Product 分层。首发硬件输入为 STM32F407ZGT6 启明欣欣高配 V3.1、STM32F407VET6 天空星青春版、GD32F470ZGT6 梁山派；保留已验证构建的 F407VG Discovery 参考组合。用户明确将原 GD32F303 目标改为 F470，当前没有 F303 支持声明。

## 代码边界与消费

`arch/` 提供 Native 与 Cortex-M4 CPU saved interrupt state、异常查询和屏障；不依赖 HAL、OSAL 或厂商头。SoC 提供内部 Flash 几何、时钟、IRQ、UID 和单调时间端口。Board 描述晶振、引脚、LED 极性、外部器件与初始化电平。平台目标私有链接厂商 SDK，并显式保留 controller/Board 对象、实际 startup、注册段和强中断入口。

`hal/` 的设备核心使用设备类别、状态码、owner/generation 引用和显式 open/close/recover。发现不启动硬件，GPIO 操作不泄露实现指针；UART ticket 在硬件 settled 前持有 buffer lease，取消失败不会释放所有权，旧引用与旧 ticket 被拒绝。SPI façade 使用有界静态子设备池、父子 lease 和 generation/ticket；同步取消保留 running lease，异步取消只表示请求接受，须等 terminal callback 返回后才 settled。真实 Native 和 GD32 SPI4 production provider 的纵向测试覆盖这条链路。GPIO/UART 生产调用者与这些契约一同迁移。其余类别仍需按 capability 和实际端口审查，legacy factory 仅作为明确标注的构造迁移入口，不能绕开独占引用获得双重所有权。

`osal/` 提供后端能力、初始化状态和资源限制。FreeRTOS 的六类对象及 idle/daemon 使用静态内核对象、有界池和明确栈/payload 预算。Native 提供真实宿主同步语义；裸机仅暴露实际支持的主循环能力。调度前对象创建保存恢复 incoming port mask，运行操作与 FromISR 入口要求已经启动 scheduler。OSAL heap seal 只约束 OSAL 入口。已结束但仍持 mutex 的任务不能回收身份给新任务。

`products/` 提供常量配置身份、串行 HAL→OSAL 启动、部分失败回滚及可重试的所有权。它不假装重新启动 MCU scheduler 或完成全平台 teardown。普通产品使用 `Nexus::Product` 和 Nexus 类型；FreeRTOS 应用在真实 scheduled worker 内执行业务。服务按配置启用，minimal profile 不强制引入 Config/crypto/OpenSSL；源码 SDK 的外部 `add_subdirectory()` 消费与父工程隔离由实际 configure/build/link/run 检查。SDK 当前提供源码消费，尚未提供可安装的二进制或 CMake package。Typed I2C 尚未迁移；Native I2C 模型不代表四块板已提供生产 I2C 支持。

## 板卡、内存与驱动

| 组合 | Flash 应用 / 保留 | 默认主 RAM | 独立域 |
|---|---|---|---|
| F407VG Discovery / F407ZG 启明 V3.1 | 768 KiB / sector10–11 共256 KiB | 128 KiB SRAM | 64 KiB CCM，未自动分配/初始化 |
| F407VE 天空星青春版 | 256 KiB / sector6–7 共256 KiB | 128 KiB SRAM | 64 KiB CCM，未自动分配/初始化 |
| F470ZG 梁山派 | 1008 KiB / 末四个4 KiB page | 192 KiB 主 SRAM | 256 KiB ADDRAM、64 KiB TCM，默认不分配/初始化 |

STM32 读取实际 Flash 密度，并核对编译配置及 linker 分区。GD32 的4 KiB独立 page erase 来自 F470/F425/F427 专属功能与官方 SDK，不继承 STM32 sector/F303 page 假设。内部 Flash 操作必须在产品定义的维护窗口进行；同 Bank 执行暂停和真实掉电恢复仍需测量。

STM32 UART 使用正确的零起始 Nexus index→USART 映射、一字节 IRQ RX 环、错误/溢出事件、真实 wire TC 完成、延后 task poll 和有限总 deadline。DMA 未建立所有权时返回不支持。GD32 使用实际 USART0、SPI4 和 TIMER1 1 µs 时间端口；TIMER1 是平台保留资源，32-bit overflow 通过 IRQ 扩展。UART reset/cancel 的 RX discontinuity 形成可见事件。

官方启动向量的段名、SP symbol、Reset/IRQ 入口和 linker KEEP 是交付检查的一部分。GD32 `.vectors`/`_sp` 与 ST `.isr_vector`/`_estack` 不同；仅“链接成功”不足以判断镜像能够启动。Flash RX 与 SRAM RW 段通过实际 ELF 检查，不能用过滤 RWX warning 代替布局修复。

## 工具链与源码身份

ARM GNU 14.3.rel1 的官方 archive SHA-256 固定在 `dependencies/toolchains.lock.json`，CI 与本地以同一 installer 获取完整 GCC/newlib 工具链。STM32/CMSIS/FreeRTOS 使用既有 pinned Git 依赖。GD32F4xx 3.3.3 来源、双层 archive 摘要和157个原始文件身份在 `vendors/gigadevice/gd32f4xx/source.lock.json` 中记录；配置和候选打包拒绝缺失、额外、篡改、非规范路径和 symlink。138个 BSD、17个 Apache 和2个旧 Arm 特殊许可分别保留，不把完整导入重标为 BSD。

CI 增加启明、天空星和梁山派的裸机/FreeRTOS ARM 构建；所有组合使用独立有效配置和实际 ELF/map/bin/hex。发布工具显式审核配置、板卡、芯片、后端、工具链及 Git/import 依赖身份。没有创建正式 Release，也没有合并 main。

## 验证范围与产品验收

定向证据包括生产 STM32/GD32 驱动的 vendor fakes、真实 pinned FreeRTOS kernel、Native 并发与引用寿命、Product 失败回滚、外部源码 SDK 消费、配置冲突和依赖篡改。最终干净提交的全量 CTest、八份 ARM Release（含实际向量、强IRQ、编译 sizeof ABI、分区与损坏镜像拒绝）和线上门禁另行绑定提交、配置及产物身份；定向或 dirty-tree 构建不冒充该最终记录。

本环境未连接三块实板。HSE/PLL 波形、电源/安全电平、真实 IRQ/TC/DMA、Flash 断电、控制 jitter、栈峰值、RS485 接线和长期运行尚无物理报告。Native UART 新 ticket 能力未实现时明确不支持；MCU 全平台 shutdown、crypto/熵源、受保护 bootloader/vault、安全计数器和完整自动 pin/DMA 拓扑校验没有被声明为完成。CAN/Ethernet 是按产品需求实施的后续能力。软件平台候选与企业支持/LTS 的准入分别记录在支持矩阵和企业工作流。

岗位按用户确认的10人配置：技术负责人1、内核/HAL/OSAL2、STM32/GD32 BSP2、工业应用通信2、QA/HIL2、构建发布1。真实成员账号、实验室和产品预算尚未登记，岗位元数据不等于远程分支保护或实板验收已经生效。
