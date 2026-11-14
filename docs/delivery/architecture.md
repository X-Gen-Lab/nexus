---
id: delivery.architecture
type: explanation
audience: maintainer
source_language: zh_CN
---

# 当前系统架构

Nexus 提供静态 C11 通用机制。当前生产 SoC 入口只有 `soc/stm32/stm32f4/`；
[精确支持](support.md)区分八个精确料号、七个器件视图、四个 F407 Board 和显式软件参考。
系列源码复用不等于每个模式都已执行。厂商依赖保留，只有实际选中闭包进入准备、
校验、编译与 SDK。

本轮便携 I/O 重构以 [PR #61](https://github.com/X-Gen-Lab/nexus/pull/61) 的合并提交
`6eac971a07a5021d25ba57c3b6885e6e789f2910` 为基线。平台实现固定在
[`6b59f666`](https://github.com/X-Gen-Lab/nexus/tree/6b59f66628f0dbb415f6203ba917c1fe717bf3fd)，
最终 Source SDK 固定在
[`4bf4b5a2`](https://github.com/X-Gen-Lab/nexus/tree/4bf4b5a258b3120c24f6d834545ac3f300ce801b)；
后一个提交仅迁移 TMP117 参考消费者的 I2C 字段。已选软件增量的执行结果见
[PR #62](https://github.com/X-Gen-Lab/nexus/pull/62)。本页的后续文档修正不改变这些
原始执行身份，也不将已选模式的通过扩大为整个 F4 家族或实板资格。

![FIG-01：平台职责与外部产品边界](../assets/diagrams/platform-architecture.svg)

图表达通用职责和构建／调用关系，本轮 MCU 入口为 F4。可选 OS 与组件不是每次 I/O
的强制中间层；物理验收另行取得。

## 目录、状态与资源

| 位置 | 责任与所有者 |
|---|---|
| `core/` | 结果、单调 deadline、请求与借用规则、整平台启动、停止、暂停与恢复协调 |
| `arch/` | 编译期 CPU 原语、中断状态保存恢复和明确能力 |
| `io/` | 公共类型、检查分派与硬件无关机制 |
| `soc/stm32/stm32f4/` | 精确芯片事实、启动、时钟、IRQ／DMA、私有控制器状态与 HAL／LL 适配 |
| `soc/stm32/stm32f4/drivers/ip/` | 仍被 F4 使用的私有硬件算法，无额外运行时注册 |
| `boards/` | 四个 F407 板卡的实际接线、晶振、安全电平及未知项 |
| `os/` | 可选任务、等待、通知及明确后端合同 |
| `components/` | 器件协议、owner、存储、网络等显式可选机制 |
| `tests/support/native/` | 显式测试模型、Board 与装配，不是生产 SoC |
| `tools/`、`cmake/` | 配置、唯一源码图、测试、SDK 与交付 |
| 外部产品 | 任务、队列、预算、私有分区、恢复与安全政策 |

完整导航见[源码定位](../explanation/source-map.md)。顶层共享 MCU `drivers/` 已归入
F4，目录移动本身不改变指令数或资源成本。

## 构建与运行

一份 schema-2 TOML 和只读 SoC／Board JSON 经严格校验、资源计划和不可变 typed IR
生成原子 bundle。CMake 选择已声明的实际源码与链接依赖；生成器不维护平行源码图。

本轮 F4 输入先由 `variants.<part>.device` 选择唯一 device profile，再组合不重叠
controller groups；不可变 `DeviceView` 分开表达物理 `ControllerFacts` 和软件
`ControllerModes`。配置与查询共用所选事实，家族展示与 Host-only 审核索引不反向
授权单颗器件。具名资料 profile 只在中央主机模块校验受审身份与适用性，不增加
MCU registry，详见[接入指南](../guides/provider-porting.md)。

根 CMake 只消费 provider 声明的 `NEXUS_SYSTEM_PRIVATE_INCLUDES`，不再硬编码 F4
IP 私有目录；芯片、模式和供应方源码仍由唯一目标图选择，外置 provider 不自动继承
无关 F4 头。构建连接没有增加运行时层，详见[构建合同](../design/build-system.md)。

每个固件选择精确器件宏和启动文件。厂商头、Handle 和可变 provider 状态私有；
生成器按选中实例定容配置、状态、buffer、只读 face 与强类型 factory。

运行中 factory 只返回已有接口。日常 I/O 通过 `ops(context, ...)` 直接进入 F4
实现，硬件操作优先复用官方 HAL／LL。不会因共享源码引入最大池、动态注册表、
默认堆或隐藏 worker。应用负责串行执行者、服务进度和 scheduler 启动。

## 领域接口与原生能力边界

[FIG-16](../design/io-evolution.md)区分构建期装配与运行期调用。公共接口表达逻辑
设备、操作、数据格式、结果和借用；SoC／Board 配置负责寄存器编码、引脚、DMA、
IRQ 与时钟映射。组件不需理解这些映射，但仍需遵守长度、对齐、时限及归还条件。

本轮源码代际为 3，详见[迁移](migration.md)与[兼容性合同](../design/source-compatibility.md)。
公共领域 traits 与 factory 生成值分开；UART 的通用格式合法性和 F4 支持性分别校验。
SDIO function 请求按 function 地址表达，不再嵌套块设备 LBA 请求；逻辑 GPIO
绑定、编码器 A／B 语义及网络结果也沿各自领域合同收敛。本轮已选软件增量完成了
下述验证；各组回归、ARM、资源与 SDK 保留各自实际输入身份和差异核查范围。

普通同步调用不承担 request、stream、队列或 OS 状态；有限异步、连续流与跨任务
owner 按所选能力增加存储。原生波形与数据布局可作为显式扩展，仍经同一硬件
owner，准备／编码使用显式存储，提交不隐藏分配或整缓冲重排。同名公共类型不因
精简 profile 宏改变布局；未使用能力由实例和构建闭包裁剪，具体成本按 ELF 测量。

SOLID 在此约束实际依赖：每个 owner 只推进自己的硬件责任；provider 通过领域
合同扩展；可替换实现必须保持单位、原子性、完成与借用；接口按独立消费者划分；
组件通过静态注入取得窄端口。它不引入设备基类、运行时名称注册或默认线程。

### 本轮重构退出范围

R0–R13 是本轮方案分组，与下方 WP／IO 的既有能力范围交叉核对。它们不是新的
配置源或验收引擎；只有实现、消费者与对应执行证据齐全才关闭具体子项。

| 分组 | 本轮退出条件与剩余范围 |
|---|---|
| R0–R2 公共合同与原型 | 领域 traits、格式校验、独立 SDIO 请求和 epoch 3 消费者已完成迁移，并通过本轮普通提交门禁与完整 SDK 消费。 |
| R3–R7 数字、串行、控制与采集 | 逻辑绑定、事务与 native 计划边界、薄包装和生产者／消费者退出分别可用；尚未实现的连续 SAI、完整无缝帧及精确电脉冲仍按支持矩阵保留。 |
| R8–R11 存储、图形、网络与系统 | 厂商编码留实现，公共结果可解释；CPU-only 内存不自动授权图形访问。显式 `graphics-owned` SDRAM 由所选 LTDC／DMA2D 持有同一私有 bank lease，全部冷退出后归还；不是 MPU 隔离，新增组合仍须独立验证。 |
| R12 依赖与成本 | 对相同工具链、装配比较实际编译闭包、静态对象及 ELF；不以方法表、inline 或 GC 的存在推断零成本。 |
| R13 集成交付 | 最终 clean-source SDK 的完整维护消费者、搬移前后校验及恢复校验通过；双语文档和 ARM 记录保留原实际输入身份。物理 HIL 与远端 Actions 按用户要求暂缓。 |

平台实现的 Debug、Release、ASAN+UBSAN、TSAN 各通过完整 329 个 CTest 项和
5982 个 GoogleTest 用例。两个提交均通过普通提交门禁；最终消费者修正不属于
四种 Native 模式的编译输入，依赖核查保留原执行身份。静态分析覆盖 827 个不同
编译上下文：Cppcheck 完整通过；Clang-Tidy 的 826 个上下文通过，唯一 I2C
发现修复后按原配置重验受影响上下文通过，原始失败记录保留。

最终 clean-source SDK 的导出、校验、整体搬移、34 个消费者的 165 条命令和
消费后校验均通过；压缩包解压后也通过官方校验。文档工具测试与严格中英文构建、
四个 F407 ARM 参考配置均有通过记录；它们保留当时的源码快照与配置，不重标为
最终 SDK 提交的重新执行。具体命令、输入摘要、差异与原始结果由 PR #62 关联。

## 生命周期与失败

F4 C 实现维护资源准入、取得、收条、硬件撤回和 readback。部分取得失败必须处理
当前步骤已经产生的效果，再逆序撤回前序步骤；失败释放保留实际责任。

默认静态装配通过 Core 私有 `platform_lifecycle_impl.h` 使用手写 C 协调器。
公共 `nx_platform_lifecycle_start/stop/suspend/resume` 以运行期参数调用同一实现。固定装配的配置、
可选 owner 和诊断在同一编译单元可见，编译器可以裁剪恒定分支和不再需要的回调表；
产品仅包含公共接口，不增加运行时转发层或第二份算法。
全部 controller 准入先于硬件效果；随后依次建立 System、共享资源与 controller。
失败按逆序撤回，在首个释放拒绝处保留剩余责任，后续 stop 只重试未完成部分。
primary、cleanup、remaining effects 和调用期诊断独立保留，不以清理成功覆盖原始失败。
F4 的 System／DMA／SYSCFG／pins／CAN／Ethernet 共享责任由其私有 C 协调器处理。

暂停先由调用者 join 借用者，再逆序排空 controller 和共享资源；失败停在
SUSPENDING 并保留前缀，不能直接 resume。只有完整 SUSPENDED 才按原 plan 重取
资源和 controller；恢复失败逆序回滚，System 与原配置责任仍保留。自主 RTC 等
域通过同一 owner 的显式 suspend/resume hook 保留，不由通用协调器直接写硬件。
暂停本身不进入低功耗；STOP 的独立电源机制恢复后建立新 Core epoch，应用与 OS
重新建立 deadline，不能把暂停代次当作时间转换。详见
[System 合同](../design/f407-system-power.md)。

ACCEPTED 后借用请求与 buffer，直到 acquire 观察 SETTLED。DMA 完成、线上完成、
IRQ／callback 撤回和最终发布分别证明；timeout／cancel 不提前释放借用。
QUARANTINED 保留存储与恢复责任，观察者和 adapter 还需独立 join。

启动外部 SRAM、已启 watchdog 和保留 RTC 状态不能套用普通关闭。HAL 与自写
实现也不能各自独立推进同一个 live controller 行为域。

## 当前实现与后续边界

Board／SoC 不再保留 Python 插件。默认主机工具仍使用 Python 校验与生成静态对象、
只读 plan、按索引选择的固定 typed call 和薄 IRQ 入口；启动、停止、回滚与诊断控制流
由手写 C 维护。既有 `nx_lifecycle_*` 可撤销资源事务与整平台协调器职责不同，不要求
为统一外观把全部驱动改接同一组 ops。

协调状态保留身份、16-bit 前缀、重入保护和暂停阶段，当前 M4 ELF 为 12 字节；超过 65535 个物理
controller 的 plan 在任何 owner 回调前拒绝。它不改变日常 I/O 热路径；实际资源、
模型行为、ARM 链接和
物理测量各自验收，不能从源码迁移推导整平台通过。

其他 F4 型号需要精确内存、封装、时钟、DMA、IRQ 与硬件 profile；官方 HAL 覆盖
不建立 Nexus 全系列资格。当前软件验证绑定本次执行，旧记录不继承；物理 HIL 为
`not_executed`。精确边界见[支持](support.md)与[资格](../reference/platforms/qualification.md)。

## 本轮架构迭代任务

以下保留已接受的 WP01–WP12 和 IO01–IO20 原始任务编号，替代用旧批次编号推断
当前进度。它是本架构页的人工审查表，不建立另一套配置输入、台账引擎或发布流程。
每个工作包分别说明既有基础、本轮增量和退出条件；只关闭有对应源码与实际执行
证据的子任务，不用一个 GREEN 或公共 API 存在关闭整包。

本轮保持静态 C、共享只读 ops、单一硬件执行者及 ACCEPTED→SETTLED 借用合同。
全部 `vendors/` 保留。应用、产品策略和私有 PCB 继续在外部；实板与远端 Actions
按用户要求暂缓，本地软件门禁仍是必需条件。源码、模型、ARM、SDK、文档和物理
资格继续分开，见[验收边界](qualification.md)。

### 后续增量的实现边界

以下增量继续使用上面的同一条静态装配和运行链路。factory 不取得硬件，逻辑
signal 不增加 owner，SAI2 不复制一套驱动，DMA 完成也不替代外设及设备退出。
生成源码接口与 typed IR 进入 epoch 5；TOML schema 2、resolved projection 1 和
SDK manifest 2 各自保持原格式，迁移见[兼容合同](../design/source-compatibility.md)。
这里记录代码责任及尚未开放的范围；完整测试、ARM、SDK 与实板结果分别绑定实际
输入，前序 PR 的结果不转移到本增量。

| 领域／任务 | 本增量 | 保留的明确边界 |
|---|---|---|
| 工具并发 E01–E02 | 每次执行保留独立日志和回执；单一原子 latest JSON 指向完整执行。`dev.py matrix` 复用 CMake 预设、独立目录／锁及总 jobs／内存预算；取消只撤回所属进程 | 不缓存成功测试，不把并发 wall time 相加，不为降低计数删除有效故障回归；外部手工 Ninja 不自动受 CLI 锁保护 |
| 装配 E05–E06、D01–D03 | 可选 `active_high` 只为授权单 bit 生成只读 signal，强类型 factory 返回同一静态绑定；RS485 消费该 signal。CMake 按实际 controller 查询模式，避免混合 SAI／I2S 时选错源码 | 初始安全电平独立于逻辑极性；软件总线保留物理电平／开漏合同；手工绑定由集成方保证授权及存储生命周期 |
| 串行 D05–D11 | 保留已审 UART／SPI 模式及无副作用 preflight；可选 SMBus packet PEC 适配器复用异步 I2C endpoint、controller 与地址，静态 companion 沿用 endpoint strong ID。独立 payload／wire-storage、outer loan 与实际 I2C drain 一起结清 | PEC 的 32 B 限额包含 command／count，不声明完整 32-data-byte block、硬件 SMBTYPE、ALERT／ARA、ARP 或电气符合性；普通 I2C 不承担组件状态 |
| 音频 C07–C09 | F446 SAI1／2、F479 SAI1 共用同一 C 实现；有限与连续路径分别定容。wire slots 和 active mask 保留物理位置，内存只存有效 FIFO 容器；frame helper 和静态消费者使用同一单位 | 不重排、填充或隐藏复制音频。持续流依赖显式 SCK／service 预算；源关闭、目标替换及实际 DMA 排空后才归还贷款 |
| System M01–M03 | F411 HSI96、F446／F479 HSI168；F479 M10 可复用已审音频／图形 PLLSAI。System 唯一管理电压、Flash latency／prefetch、PLL／VOS 等待、选择、回滚和时间发布 | HSI 名义频率不是物理精度证明；180 MHz／overdrive、DVFS 不受支持。F411／F446／F479 的低功耗由 N07 独立校验精确高频 profile、RTC 与唤醒组合，不从 RUN restore 推导 |
| 存储 M04–M06 | F479 精确双 bank Flash、ES0321 缓存取得／恢复；QSPI page-program DMA 与 W25Q128JVSIQ 四线读取独立选择。Quad read 使用六个已审 pads、6Bh／8 dummy，接管前要求 QE=1、SUS=0，不写状态；原 PIO owner 与 ops 复用 | DMA TC、QSPI idle、NOR WIP 与 SETTLED 不合并；quad 不授予 DDR、映射、XIP、外来 suspend 接管或 F479 option 维护；未选 quad 不承担其 pad snapshots |
| 映射与高速存储 M07–M08 | F479NI＋W25Q128JVSIQ 可选独立 CPU_READ mapped endpoint，确切 reader slots 与通用 owner 静态组合；F407 可选 HSE8／HCLK150／PLLQ75、四位 SD／MMC 37.5 MHz SDR，速度与 poll／IRQ／DMA 分离 | 映射只授权已借用 CPU 范围，排除 DMA／XIP／外来 master；失败释放保留清理、不授予继续读。高速模式须完成卡片协商，不授予 USB／RNG、低功耗、DDR／HS200 或其他时钟组合；HIL 未执行 |
| 采集与图形 C10／M11 | 公共有限 frame-session 消费者保留已发布首帧直至 producer join；持续图形消费者严格按 image request SETTLED、display submit、旧 framebuffer SETTLED 的次序复用静态双缓冲 | 消费者组合不把 CM1 帧间重启变成无缝采集，不声明传感器时序或物理 scanout 无撕裂；停止失败保留原 loan／lease |
| 低功耗 N07 | F407 既有配置及 F411／F446／F479 精确高频软件参考复用家族 C 实现；显式 traits 处理完整 NVIC、CCM、备份 SRAM 和 WKUP 差异。28 B options、真实 System 恢复后新 epoch，以及 W1C／debug／外来模式准入分别验证 | 仅所列 baremetal、晶体 LSE 与唤醒组合；F479 sticky 状态需要产品授权的 reset-prepare＋实际复位，不自动清 RTC 或复位，不扩展到 RTOS／180 MHz／DVFS |
| 密码 N08 | F415／F417 的 CRYP／HASH 各自可选 DMA，复用同一官方 HAL transport owner；每 IP 独立源码／宏和静态扩展，任务 service 在所选 DMA 的 NVIC 中断项保持禁用时观察其状态 | HASH 输入／HMAC key 需要正长度、word 对齐与长度倍数 4；IRQ 保留原字节合同。CRYP 不承诺 DMA 原地重叠，不增加 AEAD 或未审器件能力 |
| 通信 N01–N04 | CAN／Ethernet 增加持续环形资源耗尽和恢复组合回归；ST Host 桥接保留唯一 HCD，使用调用者可选存储执行明确 descriptor cadence 与保守带宽准入 | cadence 不自动派发；SOF 回绕必须在声明观察预算内。无 split／自动 OTG／相位打包或物理周期保证；PTP 计数分辨率不是同步精度 |
| 成本与交付 Q01–Q08 | 使用现有 ELF、资源、保留、反汇编和 SDK 消费工具；新 SAI／QSPI 模式加入同一维护计划，不新增证据引擎 | 同 ABI／优化及实际选中配置比较；共享事实来源变化单独披露。静态预算、保留空间与实测峰值不同；HIL 仍为 `not_executed` |
| 配置成本 E03 | 官方输入在同一次 CMake configure 首次完整验收、保存不可替换快照，末尾完整复核；逐 target 的 device／ABI／私有 include 检查保持。精确依赖与只读 Git 观察避免锁容器时间戳触发重复配置 | 不跨 configure 缓存成功，不缓存测试结果；中途 source／lock／Git／SDK 变动、同字节重写和缺失 authority 都拒绝；实际 wall time 由必跑配置记录 |
| 服务端 E07 | 同时读取 main、rulesets 和适用规则；保留实际规则写入请求及 GitHub 403 | 本地 hook 不代替服务端保护，不伪造 required check；连接缺少 Administration 写权限时不声明规则已生效 |

后续条件任务仍按精确硬件证据推进：连续 JAUTO、交错 ADC＋注入、有限外部 DAC
尾部和无缝 DCMI 不能用有限重启路径替代；QSPI 映射和 SD/MMC 高速仅按 M07／M08
所列精确参考开放，不授予 XIP、其他速度或器件组合。NAND ECC 和实装 SDRAM 仍需
固定器件与存储合同；USB split 和增强密码／AEAD 仍有独立
所有权、错误和恢复责任。新料号 STOP／Standby 只按精确已审时钟与唤醒组合开放。bxCAN 不具备 CAN FD；F4 SDIO
不因模式名或头文件存在取得 DDR／HS200 资格。条件审查完成与运行模式实现完成
分别记录，应用策略和真实硬件验收继续由原边界约束。

### 工作包、当前状态与退出条件

下表说明本轮的实现范围和验证要求。实现范围限定为所列精确器件与已声明模式；
F407 模式回归不自动扩展到新增料号。源码接入、独立审查与软件验收分别记录；
资源、普通门禁、ARM、双语文档及 SDK 结果绑定各自实际源码、配置和工具，后续
差异按受影响输入核查。上述固定提交的结果由 PR #62 关联；新增变更另行验收。
未选器件与模式保留明确边界，不从厂商库覆盖推导
整个 F4 全功能支持；实板暂缓也不等于已有软件机制未实现。

| 工作包 | 保留的原始子任务 | 当前状态与退出条件 |
|---|---|---|
| WP01 基线 | BL01 固定源码、支持和消费者；BL02 空／单／多实例／DMA 代表配置；BL03 资源、ABI、身份基线；BL04 合同与任务映射 | BL01、BL02、BL04 已实现；BL03 的代表装配资源、ABI 和输入身份已记录。固定源码、唯一 Source SDK 消费者策略与空／单／多实例／DMA 代表装配均已有；SAI、图形、加密、SDRAM 和 QSPI 沿同一维护计划接入。新装配仍需自己的成本与资格记录，不把历史测量重标为新执行。 |
| WP02 精确事实 | FT01 统一宏、头和 startup；FT02 控制器／IRQ／DMA 事实去重；FT03 精确器件与封装；FT04 内存与 DMA 域；FT05 具名勘误 profile；FT06 独立手册／SDK 核验 | 实现已有：FT01、FT02、FT03、FT04、FT05、FT06（当前八料号、七器件视图的已审范围）。精确宏／startup、封装／内存／DMA 域、IRQ enum／number／handler、控制器与路线交集、独立资料及勘误 profile 已贯通。经典 F405／407／415／417 复用 67 项共同事实，F411／446／479 按各自 IP 与时钟审查；厂商 SDK 选择器不是运行期器件支持，未知封装事实不补猜。 |
| WP03 配置 | CF01 统一器件视图；CF02 模式合同；CF03 去首／次 controller 特判；CF04 严格诊断与原子 bundle；CF05 工程模板与接入说明 | CF01、CF02、CF03、CF04、CF05 已实现并通过本轮完整工具域与 SDK 消费。DeviceView、模式合同、严格诊断／原子 bundle 与模板已有。SPI 删除 peer 选择、UART 删除 preload 特判；新增模式沿原 typed IR、静态 emitter、资源校验和唯一 System ClockPlan 接入，新增实例不引入行为生成器。 |
| WP04 接口与成本 | IF01 强类型 factory 默认；IF02 迁移重复数字入口；IF03 公共接口／可选扩展；IF04 只读资源配置；IF05 配置／状态测量；IF06 编译期特化成本 | IF01、IF02、IF03、IF04 已实现；IF05、IF06 已取得代表性编译闭包、对象、ARM 布局与 ELF 比较。默认强类型 factory、SDK face／metadata 查询、公共 Nexus 接口及只读配置已有；底层 scalar switch 保留为唯一实现。新模式只扩展所选可选 ops 和自己的状态，不给所有实例增加最大对象。实际成本只适用于所测工具链、ABI 与装配，不据此声称全平台零成本或最优。 |
| WP05 共享资源 | RS01 共享 IRQ 优先级；RS02 TIM／ADC／CAN／USB 共享域；RS03 DMA 范围、单位、宽度与 FIFO；RS04 取消、迟到 IRQ、失败排空；RS05 部分收条逆序释放；RS06 连续流预算与背压 | 实现已有：RS01、RS02、RS03、RS04、RS05、RS06（已声明模式）。共享 IRQ／硬件域、DMA 范围与单位、借用排空、部分收条和有界流已有。ADC／DCMI 会话、SAI、图形、加密与存储由单 owner 承担源、FIFO、IRQ、地址和终止发布；wire、内存和器件 busy 分别证明。DCMI RAW／JPEG storage blocks 共用 44 B 在途预留和两个 1 µs tick 的观察相位余量，同时收紧旧 RAW 准入。各模式须覆盖冲突、迟到事件、拒撤与耗尽，失败不提前归还存储。 |
| WP06 Core／OS | OS01 有限 deadline；OS02 启动、停机与换域；OS03 临界区与屏障；OS04 默认 Platform 入口；OS05 裸机推进／RTOS 静态对象；OS06 布局与尺寸 | 实现已有：OS01、OS02、OS03、OS04、OS05、OS06。有限预算、停机换域、mask／屏障、同一 Platform 协调器及静态 RTOS 已有。本轮继续闭合串行／HAL／Ethernet 的有限排空、RS485 最低 hold 故障保留与 UART 配置归还后幂等 stop；实际布局与整程序有证据。新增候选仍须统一门禁，不授予全部 CPU／后端组合。 |
| WP07 系统 | SY01 时钟、供电与 Flash latency；SY02 回退实际频率；SY03 启动、CCM、外部 SRAM；SY04 STOP／Standby；SY05 恢复时间桥接；SY06 故障记录与恢复 | 实现已有：SY01、SY02、SY03、SY04、SY05、SY06，限已声明器件和模式。既有 F407 静态 ClockPlan、实际 HCLK 回退、pre-C／CCM／外部 SRAM、受约束 STOP／Standby、显式 deadline bridge 与首故障记录保留。新增 F411 HSI96、F446／F479 HSI168 的已审静态 RUN 配置；F446 独立 M、F479 共用 M 的 PLLSAI Q／R 只由唯一 System owner 校验与撤回，不由 SAI／LTDC 启动第二份 PLL。F411／F446／F479 的受约束 STOP、WKUP／RTC Standby 另有独立 traits、完整 NVIC、W1C／debug／外来电源状态拒绝及恢复测试，仅在精确 high ClockPlan、baremetal 和已审唤醒组合开放。180 MHz、overdrive、DVFS 不从 RUN 或 STOP 实现推导。 |
| WP08 驱动 | DR01 完整模式 HAL／LL；DR02 拆无关外设依赖；DR03 同 IP 行为复用；DR04 I/O 全清单；DR05 删除已有可靠替代的旧实现 | DR02、DR03、DR05 已有实现；DR01、DR04 按下表已声明路径验收。驱动复用官方 HAL／LL 的完整适用 transport；已核实的等待、FIFO、勘误或借用缺口留在有界 C owner，不生成寄存器算法或无理由逐寄存器重写。六类新 IP 和 CM0 JPEG 无帧界存储流均已接入现有静态路径。重复 START、已知控制元数据别名及私有 UART RTS 重配置的修正属于已有合同的质量闭环，不扩大为全部硬件模式支持；未选范围继续明确保留。 |
| WP09 Board／组件 | BD01 原理图与安全电平；BD02 未知 PCB／跳线显式；BD03 组件使用公共 endpoint；BD04 中间件按需 | 实现已有：BD01、BD02、BD03、BD04。 四份独立 Board 事实与安全电平、显式未知 PCB／跳线、公共 endpoint 器件组件及按需中间件已有。当前不扩大接线或实装声明，不猜测未知事实。未选择的中间件不进入产物，组件不含厂商寄存器；实板暂缓不阻塞这些软件机制。 |
| WP10 构建／SDK | BU01 根 CMake 去 F4 私有目录；BU02 模式／IP 选编；BU03 私有 SDK 配置／callback ABI；BU04 SDK 精确器件闭包；BU05 外部消费者 | BU01、BU02、BU03、BU04 已实现；BU05 的 PR #62 固定基线完成 34 个 clean-source SDK 消费者；当前增量继续按唯一维护计划重新导出与验收，旧结果不转移到新源码。provider 私有 include、模式／IP 选源、私有 SDK／callback ABI 和精确器件头／startup 选择已有；新增 IP 按选中目标进入同一源码闭包。首次 SDK 消费暴露的 TMP117 旧 I2C 字段已迁移，完整重新导出与消费通过，原失败记录保留。 |
| WP11 家族扩展 | FM01 F405／F407／F415／F417 差异复用；FM02 缺失外设／封装负例；FM03 其余 F4 按 IP 扩展；FM04 新 IP | 实现已有：FM01、FM02。本轮已实现 FM03／FM04 在当前八料号及六类新 IP 的所列软件增量，尚有明确后续范围。F411／446／479 按独立资料、封装和时钟政策接入；新 SAI／图形／QuadSPI／基础 CRYP／HASH 分别绑定精确器件与模式，不能由 HAL 覆盖扩大到整个 F4。其余料号、增强 IP 与下表未列模式仍需实际实现、差异负例和相应消费者。 |
| WP12 验证／交付 | QA01 合同与差异测试精简；QA02 真实 TDD；QA03 ARM、资源与编译上下文；QA04 完整正常门禁和审查；QA05 文档、图、示例与矩阵；QA06 软件候选与版本 | QA01 本轮合同保留审查与 QA02 实际 RED／GREEN 已记录；QA03、QA04、QA05、QA06 的已选软件增量具有上述 ARM／资源、分析、普通门禁、审查、双语文档与 clean-source SDK 结果。原始接口编译 RED 与运行期行为 RED 分别保留。按生产合同与编译差异保留回归，不用测试数量、filters 或重复小套件追求完成率。源码版本保持 0.2.1 开发快照；这些结果不建立新发行或实板资格，后续变更保留自己的输入与执行身份。 |

### I/O 覆盖与未完成边界

这里按原任务覆盖全部二十类 I/O；具体可选择模式仍以[精确支持](support.md)、
[派生能力](capabilities.md)及各领域合同为准。本表不增加第二份能力事实，不把一次
协议或 DMA 修正解释成该类所有功能已经完成。

| 编号 | 类别 | 既有基础与本轮边界 |
|---|---|---|
| IO01 | GPIO／EXTI | 已有输入、输出、IRQ 事件与 EMR 硬件事件；本轮逻辑信号绑定与按接口归属的 EXTI 来源已实现并通过模型回归。封装、电气、PC13／LSE 和 Board 授权继续受约束，不建立全类物理资格。 |
| IO02 | UART／USART | 已有选定格式、线模式及轮询／IRQ／DMA。UART idle cold、RX／duplex 不可变资源与别名准入、有限排空和归还配置后幂等 stop 保持同一合同；RS485 无法证明最低 hold 时保留 DE／outer loan。私有 live-format helper 无 RTS pad owner，旧／新格式任一启用 RTS 时在副作用前拒绝；静态 wire RTS 支持保持。控制器／格式组合仍按已声明模式验收。 |
| IO03 | SPI | 已有主／从、帧事务与 DMA。SPI1／2／3 DMA 统一 cold prepare／close，不再根据 peer 存在选择算法；优先级、失败 CR 恢复、CS／pin owner 有实际回归。有限 framed drain／tail 已修正，新增模式仍需自身尾部与取消证据。 |
| IO04 | I2C | I2C1／2／3 已有选定 master 轮询／IRQ／DMA、target 和 SMBus DEVICE（≤100 kHz／PEC／ARA）。异步 message 尾部请求 repeated START 后，下一 message 不重复置 START；请求、message 表和 payload 准入排除已知 owner、face、DMA／region 事实及控制寄存器范围。这些是原合同的质量修复，须保留异常停止、恢复和无副作用拒绝回归。可选 SMBus packet PEC 组件在已选异步 master endpoint 上提供有限 write／read／write-read；不授予硬件 SMBus HOST、ARP、HostNotify 或完整 SMBus 电气符合性。 |
| IO05 | TIM | 已有计数、PWM、捕获、编码器及有限波形；共享时基、触发、break 与 DMA burst 不能由单一实例的通过值代替。 |
| IO06 | ADC | 已有单次、scan、有限注入与多 ADC；regular blocks 覆盖双／三 ADC 同步或交错，同步 blocks 可同 owner 组合软件单次注入或独占 Table69 timer 单脉冲。新增有限 regular-auto-injected：一个 regular 序列接 JAUTO，TC、全部 JEOC 和真实硬件 join 后共同结清 descriptor／两个结果 span。交错 blocks 以最快吞吐和显式 service gap 约束，不承诺均匀 aperture。连续 JAUTO 逐次 JDR、交错与注入并行及未知外来 trigger 未建立当前软件合同。 |
| IO07 | DAC | 已有 CPU／timer／DMA 与波形路径；新增 EXTI9 event-only 外部触发的 CPU／循环 DMA，外部 rate 返回 unknown，有限外部尾部未获证明而拒绝。ES0182 pending／underrun 恢复只用声明的 timer 产生内部 idle 转换并完整重启，不计 payload，不承诺模拟无毛刺或请求间持续保持电压。 |
| IO08 | I2S／音频 | I2S2／3 有限与连续 blocks 保留；F446 SAI1／2、F479 SAI1 共用六种有限及六种连续 master／slave TX／RX／同步 duplex 实现。layout 明确容器位宽、有效位与实际 FIFO 存储顺序；稀疏 active mask 保留 wire 位置，存储只含有效 slots。SAI 消费唯一 System PLLSAI Q，连续模式使用独立定容状态、显式 service 预算和背压；不转换或重排音频，不默认分配 buffer，不授予其他音频协议。 |
| IO09 | CAN | 已有 CAN1／2 classic、邮箱和共享 filter domain；共享依赖、bus-off 与恢复仍按具体模式验收，不授予 CAN FD。 |
| IO10 | SDIO | 已有 SD／MMC 与 SDIO functions。本轮完成 RX PFCTRL／TX 精确计数与 FIFO／IRQ join、memory CMD18／25＋CMD12／13 及静态 ClockPlan 的 RM／勘误联合分频；默认与低 PCLK2 实际程序及 ARM 对象通过。M08 另选精确 F407 HSE8／HCLK150／PLLQ75 的四位 SD／MMC 37.5 MHz SDR，完成卡片协商后提交高速主机状态。最终 SDK 消费只证明实际维护装配，不授予其他高速组合、DDR／HS200 或物理资格。 |
| IO11 | FSMC／FMC | 保留 F407 FSMC 的 SRAM／PSRAM／NOR／NAND／parallel16 与启动 SRAM。F479NI FMC Bank1、8 MiB、16-bit SDRAM 默认提供 CPU 数据窗口；显式 `graphics-owned` 与所选 engine 的 `memory_window` 引用连接 LTDC 只读／DMA2D 读写区域，保留同一 bank lease 直至冷退出。两种模式均检查时序、原 System epoch 与真实自刷新退出；不授予 XIP、启动 RAM、自动 MPU 或实装资格，新几何与协议仍需独立实现。 |
| IO12 | USB Device | 已有 FS／HS Device PIO／DMA；提前 STATUS、SETUP、wire ACK 与双方向 AHB／地址 join 分开。固定 Device 与 managed Device 阶段均按需支持 remote wake：STATUS ACK 提交 host 许可，真实 suspend 新收条至少 5001 µs，调用者证明有限 service 延迟 B。cancel／stop 不提前缩短最低保持，拒撤保留 request／clock／pins，角色切换不能越过同一 DCD join。无高带宽 ISO、HNP／SRP 或物理脉宽资格。 |
| IO13 | USB Host／角色 | 已有显式 host／角色及 reset／resume 最低保持；新增实际 HCD 的单 transaction ISO 切片按精确速度、最大包和 DMA 对齐约束接入。未提供自动 OTG 协商、Host split 或周期调度；模型不替代 VBUS／PHY／枚举实测。 |
| IO14 | Ethernet | 基线已有 raw／增强帧、MII／RMII 与计划驱动的 MDC／PTP；计数分辨率不是物理精度或网络同步资格。 |
| IO15 | DCMI | 已有 RAW／JPEG snapshot、CM0 `raw-blocks`／`jpeg-blocks` 和有限 `frames-raw`／`frames-jpeg`。JPEG blocks 使用 8-bit hardware sync、无 crop，原样发布可跨帧且含 padding 的 32-bit 存储字；block length 为四字节对齐的字节数，不是完整图像、精确帧数或 JPEG 码流长度。两种 blocks 按最大 PIXCLK、44 B 在途预留和两个 1 µs tick 余量共同校验预算。CM1 每帧完整 join 后发布并重启，帧间存在停顿；缺 FREE 时 source-off 等待且 deadline 继续。仅 CM1 会话的 transferred 表示已发布完整帧前缀，不承诺 CM0 无缝完整帧；摄像头物理验收暂缓。 |
| IO16 | RTC | 基线 F407 日历、备份、晶体 PC13 静默及 INIT／tamper 勘误保留。新增 F411／F446／F479 晶体 LSE primary retained／provision 精确高频参考；F411 有 20 个 backup registers、无 backup SRAM，F446／F479 提供 4 KiB CPU lease。新增料号不由非 NULL aux 表推导全部模式资格；timestamp／tamper 仍需明确 bypass 事实，periodic／RTOS 未新增。F479 SBF／sticky WUF 阻止 RTC Standby 重入；产品授权清 SBF 后必须实际复位，平台不自动复位或清 RTC。 |
| IO17 | Flash 维护 | 已有精确几何、不可逆维护和显式 recover；物理几何使用完整 erase spans 和 checked 查询，产品 region 独立。F479 2 MiB 的普通读／program／erase 支持两个物理 bank，bank2 使用 SNB16..27；bank swap／BFB2／PCROP 不受支持。按 ES0321 在写脉冲前关闭 D-cache，完成后校验 cache reset／恢复，失败保留 owner；执行及供电前置条件仍有效。不授权旧 OPTCR 单 bank 维护接口操作 OPTCR1，不自动回滚、重放或替代 POR。 |
| IO18 | 监督／故障 | 已有 IWDG／WWDG、CSS、复位与故障观察机制；安全输出、真实复位、掉电及产品恢复仍分别验收。 |
| IO19 | RNG／CRC／身份 | 已有 RNG、固定 CRC 字序列与实际身份查询；新具名勘误 profile 不扩大器件范围，UID 不作为秘密，REV_ID 准入不证明 PCB 或封装。 |
| IO20 | 其他 F4 IP | F415／F417 基础 CRYP／HASH；F446 SAI1／2、F479 SAI1 的有限／连续／稀疏帧；F479 LTDC、DMA2D 与 W25Q128JV 间接 QuadSPI。QuadSPI 保留 SDR 1-1-1 PIO，另选 page-program DMA；读取／status／verify 仍 PIO。图形仍为并行 scanout 一／二 layer 和 fill／copy／convert／blend。F415／F417 基础 CRYP／HASH 可选 DMA；W25Q128JVSIQ 可选 1-1-4 quad read，与 page-program DMA 正交。M07 另选 F479NI＋W25Q128JVSIQ CPU_READ mapped endpoint，静态组合通用 owner；不与 program-DMA 或旁路 Flash face 共存。增强加密／AEAD、DSI／CLUT、QSPI XIP／DDR／dual 保留各自未实现边界。新模式须完成同一[维护计划](../../tools/maintenance/consumer_profiles.py)的实际消费者，不继承前序 SDK 结果。 |

本表的未开放范围描述当前软件合同，不从缺少软件证据推断芯片不具备硬件能力。
CM0 JPEG 的无帧界存储字流由 `jpeg-blocks` 提供；精确完整帧仍需要额外的帧边界
归属／观测证明。有限 EXTI9 DAC 需要最后转换的可计数握手，连续 JAUTO 需要逐次
JDR 结果不被覆盖的联合生命周期。对应替代是有限 CM1 完整帧、循环外部 DAC 或
独占 timer 有限 DMA、有限 paired JAUTO，见各领域合同。不能用延时、TC、单次
状态读回或模型外观代替这些证明；CM1 的帧间停顿不能解释为 CM0 无缝采集。

任务结果只在该子项的退出条件满足后更新。未完成项保留原编号和边界，后续改动
继续进入现有源码、合同、测试和 CMake／SDK 路径；不以新增目录、空壳测试或另一套
通用注册机制代替实现。
