# 后续迭代与团队执行

本轮选定软件交付固定公共 P2 `ab532250`，完整两轮159、SDK v15、8 个候选／
16 次 fresh 断网构建、P2 严格双语文档及无签名 outer 的公开实际恢复均已完成。
独立 QA 后续 `706ee80` 的普通完整索引门禁与完整 Python174 复扫通过：38 条
finding 为 30 note／6 warning／2 error，全部唯一对应原已审处置；七条旧 finding
消失且无新增。当前私有 E `f59debf1` 固定 P2 的 17 presets／
116 命令、source／installed 各五个真实 C broker 场景、三个 recipes 和公共 SDK walk
均实际通过；E 已正常 nonforce push 并精确读回 `f59debf1`，
仓库保持 private、private main 未更新；后续 D／部署及最终 FF 仍待实际结果。已发布
`v0.2.1` 的 `b1b903da` 预览与 `7590b2d4` 历史候选保留原身份。精确源码、执行及
取得边界见{ref}`当前 P2 无签名软件交付 <p2-unsigned-software-ab532250>`。

目标是外部团队独立接入 SoC/provider，应用团队完成工业控制与安全联网，维护者能定位失败并升级。静态 factory、只读 face、C11 硬件行为、显式执行与借用合同继续保留；可选能力不能变成默认线程、全局设备注册表或通用最大池。

下表保留原 71 个规划编号及其到 107 个权威任务的映射，不另建状态台账。
[同一交付台账](../design/platform-delivery-completion.csv)仍有 210 行，原 175 行
保持历史身份；35 个 `IT-` 行分别描述实现与当前开发检查。[派生视图](../status/ledgers/platform-delivery-completion.md)
由原 renderer 生成，原编号、顺序和退出条件不变。

外置 provider、Settings、wait-select、可选观察、TMP117、熵、TLS、MQTT 和开发
工具已有有界实现。新增 RX 负载借用、MCUboot 控制、TX 错误观察、PCAP、resolver／
CLI 与 SDK v15 属于当前组合；实现、定向 GREEN、完整门禁和安装版 SDK 分别验收。
原测试源码路径是审查入口，不能代替新鲜执行 receipt。

`IT-R04` 复用已有 H743 V／固定 HSI64／Cache off／裸机 SPI4 的生命周期
pause/resume，不规划新的通用 PM。它仍需当前集成执行与实板验收。`IT-S06` 的
已审调用方使用 Settings、显式 Flash region 和 MCUboot 字节存储，尚无文件语义
消费者；`IT-X04` 尚无新探针需求。因此暂不增加文件系统依赖或设备 runner，
真实需求出现后再按原退出条件进入。

F01 当前159检查与原成本已对账；F07 完成同 P2／工具／环境的 observational
比较，缓存与 RSS 限制保留，不能称为受控因果提速。N09 的 fresh build directory／
warm configure-build 只证明选定 Native trace，其他 MCU、CLI 与 F05 退出仍独立。
继续开放 Flash／ECC event-owner／sticky／恢复细节、七组 vendor-leaf 提案、GD303
引用审查、物理开发入口、后续 D／部署／FF、
服务端规则和物理 HIL。测试保留须有原合同或独立等价覆盖依据，不能为通过而减项。

## 工作包、依赖与权威任务

本轮共同前提是精确 part/Board/backend/controller/mode/route，以及内存、clock 和安全配置。CPU profile 不代替 SoC/Board 支持；未选组件不驻留，新增成本用实际 ELF 解释。外置 Board、资源图、能力查询、生命周期诊断、Storage、ClockDomain 和 HIL 直接扩展原机制。

### 基线与交付入口

优先级：`P0`。进入与顺序：无；先核对实际源码与已发布资产。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-B01` | 版本与资格导航 | `G04` |
| `IT-B02` | 精确支持矩阵 | `A01`, `V-A01` |
| `IT-B03` | 完整开发操作基线 | `V-K05` |
| `IT-B04` | 同条件成本基线 | `A03`, `F01`, `F07` |
| `IT-B05` | 服务端分支保护 | `V-X02` |

### 外置 SoC/provider 接入

优先级：`P0`。进入与顺序：IT-B02；先固定包契约，再实现解析、构建、SDK 与独立夹具。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-P01` | 外置 SoC/provider 包契约 | `IT-P01` |
| `IT-P02` | 不可变 package context | `IT-P02` |
| `IT-P03` | 外置包身份与路径准入 | `IT-P03` |
| `IT-P04` | 外置配置解析接入 | `IT-P04` |
| `IT-P05` | 外置 CMake 集成 | `IT-P05` |
| `IT-P06` | 安装版 SDK 消费外置包 | `IT-P06` |
| `IT-P07` | 外置包输入与证据闭包 | `IT-P07` |
| `IT-P08` | 独立接入验收夹具 | `IT-P08` |

### 配置与诊断

优先级：`P0–P1`。进入与顺序：复用 selected contracts 和 typed IR；外置包结果必须贯穿查询。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-C01` | 模式 list/explain | `N06`, `N08` |
| `IT-C02` | 使用与生命周期约束 | `V-A02`, `B02` |
| `IT-C03` | 用户配置中的生命周期诊断 | `V-C08`, `N08` |
| `IT-C04` | resolver 阶段局部化 | `D02`, `V-H01` |
| `IT-C05` | CMake 模式查询成本 | `F05`, `N09` |

### 驱动与资源成本

优先级：`P1`。进入与顺序：按完整模式审查官方实现；修改具体驱动前固定回归范围。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-D01` | 官方库与直接访问决策 | `V-A03`, `V-A04` |
| `IT-D02` | 共享 IP engine 归属 | `V-H04` |
| `IT-D03` | DMA 内存与所有权 | `D04`, `V-G06`, `V-G07` |
| `IT-D04` | 只读配置与可变状态成本 | `V-I01`, `V-I03` |
| `IT-D05` | 完整模式回归与成本比较 | `V-J02`, `V-J05`, `V-I06` |

### 运行时与关闭协调

优先级：`P1–P2`。进入与顺序：R01/R02 复用现有观察与等待合同；R04 核对已有精确 H743 路径的集成、保留与恢复，不从公共类型推导通用 PM。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-R01` | 只读运行状态观察 | `IT-R01` |
| `IT-R02` | 多源 wait-set 与显式 executor | `IT-R02` |
| `IT-R03` | 关闭与恢复协调 | `E02`, `E03`, `V-C06`, `V-C08` |
| `IT-R04` | 精确 runtime PM 路径 | `IT-R04` |
| `IT-R05` | 第二传感器与采样合同 | `IT-R05` |

### 网络与安全连接

优先级：`P0–P1`。进入与顺序：N01→N02→N03/N04→N05；N06→N07→N08，随后 N09。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-N01` | 首发联网范围与预算 | `IT-N01` |
| `IT-N02` | 成熟 C 网络栈接入 | `IT-N02` |
| `IT-N03` | Ethernet frames 网络栈适配 | `IT-N03` |
| `IT-N04` | 网络执行与有界内存 | `IT-N04` |
| `IT-N05` | 基础 IPv4 联网 | `IT-N05` |
| `IT-N06` | 熵接口与首个 provider | `IT-N06` |
| `IT-N07` | TLS 资源与信任端口 | `IT-N07` |
| `IT-N08` | 可选 MQTT 适配 | `IT-N08` |
| `IT-N09` | 联网端到端故障与资源验证 | `IT-N09` |

### Settings 与镜像维护

优先级：`P1–P2`。进入与顺序：复用 Storage；S03→S04→S05；S06 以真实文件需求进入。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-S01` | 有界 Settings/KV | `IT-S01` |
| `IT-S02` | Settings 故障回归 | `IT-S02` |
| `IT-S03` | 镜像维护窄端口 | `IT-S03` |
| `IT-S04` | 成熟 bootloader 外部集成 | `IT-S04` |
| `IT-S05` | 镜像故障与资源验证 | `IT-S05` |
| `IT-S06` | 文件系统准入与适配 | `IT-S06` |

### 开发使用体验

优先级：`P0–P1`。进入与顺序：复用现有 dev/provenance/station lock；按明确主机与探针扩展。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-X01` | 模板到固件的完整操作 | `E05`, `V-K05` |
| `IT-X02` | 可选 GDB 客户端流程 | `IT-X02` |
| `IT-X03` | 串口主机与波特率扩展 | `IT-X03` |
| `IT-X04` | 统一设备 runner 扩展 | `IT-X04` |
| `IT-X05` | 安装版 SDK 开发诊断 | `V-K01`, `V-K02` |
| `IT-X06` | 固件产物查询 | `IT-X06` |

### 测试质量与执行效率

优先级：`P0–P1`。进入与顺序：先记录同覆盖成本，保留完整提交门禁与独立编译上下文。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-T01` | 测试计划只读预览 | `IT-T01` |
| `IT-T02` | 真实 TDD 交接 | `V-J01`, `V-J02` |
| `IT-T03` | 同覆盖验证耗时 | `F07` |
| `IT-T04` | 有依据的测试清理 | `F04`, `V-J07` |
| `IT-T05` | 资源预算与早失败 | `F03`, `F06`, `V-J08` |
| `IT-T06` | 高风险输入与并发补充验证 | `IT-T06` |

### 外部参考系统与文档

优先级：`P0–P1`。进入与顺序：控制系统复用现有机制；网关依赖 N；同步双语和包外使用。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-E01` | 工业控制外部参考系统 | `IT-E01` |
| `IT-E02` | 联网网关外部参考系统 | `IT-E02` |
| `IT-E03` | 参考系统资源与故障说明 | `V-I05`, `V-K04` |
| `IT-E04` | 双语文档与架构图 | `E06`, `V-K07` |
| `IT-E05` | 从零使用验收 | `N14`, `V-K05` |

### 真实硬件验收

优先级：`设备接入后`。进入与顺序：先 H01/H02，再逐模式、联网、掉电、性能；工装不等于实测。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-H01` | 真实 station 接入 | `H02`, `H03`, `V-P01` |
| `IT-H02` | 启动与基础事实实测 | `H03`, `V-P02` |
| `IT-H03` | I/O 完成与关闭实测 | `H06`, `H07`, `H08`, `V-P05` |
| `IT-H04` | 完整联网实测 | `H06`, `H10` |
| `IT-H05` | 掉电与启动维护实测 | `H09`, `V-P06` |
| `IT-H06` | 性能与低功耗实测 | `H04`, `H05`, `H08`, `H10` |

### 候选与发布

优先级：`集成退出`。进入与顺序：冻结 clean source，实际完整计划、SDK、离线复现后再发布。

| 规划编号 | 本轮工作入口 | 权威任务编号 |
| --- | --- | --- |
| `IT-L01` | 可复现锁定环境 | `I01`, `I02`, `I03` |
| `IT-L02` | 新增依赖维护闭环 | `V-B01`, `V-B06` |
| `IT-L03` | 冻结源码的软件总验收 | `J02`, `J03`, `V-L03`, `V-L04` |
| `IT-L04` | 新精确软件候选 | `N13`, `V-L07` |
| `IT-L05` | 发布与独立恢复 | `V-L08`, `G05`, `G06` |

## 里程碑退出

| 里程碑 | 主要范围 | 可观察退出 |
| --- | --- | --- |
| M1 基线与操作 | `B、C01、T01、X01` | 用户能找到版本与支持范围，解释配置和测试计划，走通既有软件流程。 |
| M2 独立接入 | `P、C02–C04、D` | 包外维护者无需修改主库即可交付一个可验证集成；不虚构新实芯片资格。 |
| M3 工业控制 | `D、R01–R03、S01–S02、E01` | 精确外部系统可运行、持久化、维护、停止和恢复，并有故障回归。 |
| M4 安全联网 | `N、E02–E03、X` | 一个精确组合完成有界 TCP/TLS/MQTT、资源检查与故障退出。 |
| M5 维护与条件扩展 | `S03–S06、R04–R05` | 镜像维护与外部参考构建完成；PM、传感器、FS 按明确进入条件验收。 |
| M6 资格与发布 | `E04–E05、L、H` | 冻结源码的软件候选、SDK、双语文档可恢复；物理仅发布实际执行范围。 |

接入、控制、联网与交付可并行；共享合同先交接再改行为。每个功能增量同时交付实现、真实回归、当前文档和适用的完整门禁。软件、实板与发布分别关闭，不用提交数、计划生成或旧测试计数替代完成。

通用机制进入平台，任务、协议寄存器表、Flash 分区、密钥、授权和恢复策略留外部工程。私有应用源码和证据不进入公开平台产物；参考系统的软件通过也不代表实板通过。

通用 DVFS、全设备 PM、userspace I/O、通用 TrustZone 异步传输和多种文件系统仍需独立需求与合同。Zephyr 可借鉴外部模块、设备模型、测试计划、Settings 与 PM 分层；代码复用前固定具体版本或提交，不把滚动文档作为实现身份。
## 实板与后续测量的进入条件

实板接入后，先核对三块首发实际 Board、记录 PCB/晶振/供电/接线，建立 probe、serial
和 waveform/power-cut station。G474、H743、U575 来源参考配置另行核对实际板卡，
H743 还需观察供电模式，U575 需在执行镜像前确认 option bytes 与启动状态。
先复核安全初值与 clock，再验证 UART 真 TC、取消、
RX 丢失、watchdog reset、Flash 掉电与 EXTI/PWM/ADC。每种 mode 只在实际执行过的
target tuple 上资格化。试验输出保持脱离产品负载，具体现场操作由 station 合同约束。

以同一 candidate ELF 做 cycles、IRQ latency、最长 service 间隔、stack high-water、
丢失、长负载测量。源码／汇编推断只能帮助定位，不写成真实时序结果。需要调整
预算或 provider 时重新生成对应 source/config/artifact 证据。

## 按测量增加能力

F407/F470 已有 UART/SPI 有限 DMA、UART 连续 RX／双向 DMA、ADC 有限／连续
trigger blocks、I2C IRQ／DMA、Classic CAN、USB FS PIO，以及精确软件夹具的
Ethernet、capture 和 complementary PWM。先以实际吞吐、IRQ latency、块间
间隙、line drain 和 overload 测量收敛这些已存在模式；连续模式也不能承诺任意
IRQ 延迟或消费者速度下无丢失。I2C 保持 100 kHz、7-bit address，每条1–256 bytes、
最多8条消息和精确 route；10-bit 或更长事务仍是独立扩展。

F103/GD303 新增有限 UART channel-DMA、正常 RX block、I2C IRQ 消息及物理
Flash／单次 ADC／固定 PWM；GD303 另维护 EXTI。有限 RX 依赖 owner service
重新装填，必须测量间隙、背压和丢失；I2C priority0 不允许内核通知。GD303 的硬件
timer 级联还需实测跨域 carry 与 Flash 停顿，不能由主机模型推断传播延迟。

G474 新增 ADC1 bulb 单次／TIM6 正常 DMA block、I2C1 polling 和 ECC Flash；
必须实测首次转换丢弃、预热、2.5–50 kHz 精确触发范围与真实采样孔径。H743 已维护
有限 UART RX DMA-IDLE、可选已初始化 D2 SRAM1 和七类有限外设；Cache-on DMA
仍要求独占完整 lines，RX 有 service/rearm 间隙，Flash 仅开放 Cache off。U575
TZEN0 已有 SPI/I2C/ADC/PWM/EXTI/Flash 的有限夹具；VDDA/ASV、VDDIO2 与真实
总线电气须单独验收。独立 TZEN1 参考仅新增复制最多八个 PC7 命令的 cooperative
job，Secure 私有存储保留状态，NS 地址不跨调用保留；没有通用异步借用、RTOS、
双域 factory 或跨域 DMA。芯片事实不授权其他模式、路由或实板资格。
后续顺序与明确退出条件见
[SOC-28–36](../design/soc-refinement.md)：

| 方向 | 进入实现前必须固定的合同 |
|---|---|
| G474 新模式或更多 controller/route | 已有 bulb/有限 DMA/I2C/Flash 的实测边界；新控制器另固定 IRQ／DMA 冲突、Board 来源、实例隔离与预算 |
| H743 DMA 与 Cache | 已有 AXI/SRAM1 有限路径的实测一致性；其他 DMA1/2、BDMA、MDMA 分别审查可达域、cache-line 所有权、维护时点与失败借用 |
| U575 DMA 与安全域 | GPDMA/LPDMA descriptor 生命周期、内存和外设归属、GTZC/MPCBB、实际 TZEN 入口与错误处理 |
| 新固定时钟或运行时变频 | 供电／Flash／bus 顺序、consumer 重配置、回滚和 deadline domain；固定 profile 与动态转换分别实现 |
| bootloader 接管或 deep STOP | VTOR／mask／FP／DMA 活跃状态、RAM 所有权，RTC/LPTIM 连续域、wake 恢复与精度 |
| userspace I/O transport | 真实 principal、权限、copy/pin、异步借用、退出回收和 SVC/MPU/TZ gateway；现有 scalar broker 不代替 transport |

每次扩展先明确 owner、buffer、cancel/drain、ISR、deadline、对齐/cache 与停止
合同，再用 GoogleTest/GoogleMock 做真实 RED→GREEN 的生产 provider 纵切片，
随后验证实际生成 factory、ARM ELF、SDK 与物理边界。未选择模式不驻留资源，
不把不断扩大的通用对象成本摊到全部产品。

本轮源实现、定向开发执行与最终集成软件资格分开记录。最终验收须绑定冻结后的
精确 source/config/tool/artifact；新文档、源码合并或旧测试计数不替代实际执行。
ECC Flash 的 `program_flags`、fresh-erase 来源及逐脉冲完成证据也要进入产品调用
合同。实板未接入，物理项继续为 `not_executed`，不提升为安全认证或生产发布。

优先复用已有窄端口与静态装配；当现有合同无法表达硬件事实时允许明确版本化的
破坏性变更。Flash 认证、升级、制造密钥与工业安全政策属于外部产品／独立机制，
不能因已有 storage CRC 或 watchdog feed 就声称完成。

## 十人角色与企业工作流

| 角色 | 席位 | 负责的可复核产物 |
|---|---:|---|
| 技术负责人 | 1 | 契约、边界、预算与支持 scope，跨层设计审查 |
| Core/I/O/OS | 2 | 所有权、并发、时间、静态资源与公共 ABI |
| STM32/GD32 BSP | 2 | 精确 facts/routes、SDK/startup/provider、故障模型 |
| 通用器件／通信与外部应用 | 2 | 窄端口组件、器件 driver、外部 composition |
| QA/HIL | 2 | 软件故障、实际 station、时序／掉电／长期负载证据 |
| 构建发布 | 1 | 唯一配置／CMake图、锁与 SDK、CI、候选／复现证据 |

维护责任由 Codex 全部承担，`.github/maintainer-roles.json` 使用
`agent_managed` 策略。十人角色保留为领域与容量划分，不再要求实名账号；安全、
制造和现场接口也由同一维护责任覆盖，产品权限／流程留在外部应用仓库。

`python tools/maintenance/ownership.py --require-assigned` 验证负责 agent、
路径覆盖、备用领域和强制门禁。GoogleTest／GoogleMock、配置、ARM、预算、SDK
和证据身份检查继续执行。该策略不生成虚构 CODEOWNERS，不声明独立人工评审；
GitHub branch rules 与 required checks 的实际配置状态仍须远程核实。
具体命令见[维护工具](../../tools/maintenance/README.md)。

Issue 描述具体触发和可观察结果；PR 明确源／配置／依赖／工具与影响的合同。每次
commit 自动检查 staged 内容和 Conventional Commit，CI 用同一整文件规范。
新行为和缺陷修复必须 TDD，主机合同采用 GoogleTest／GoogleMock；Python 工具
保留 unittest。新增用例先实际 RED，再最小修复 GREEN；机械门禁不证明历史编辑
顺序，PR 记录命令、失败原因和原始日志。中断、生命周期、持久化和并发变更要有
真实 failure-path 回归。nightly/正式候选的测量、离线构建和 station 结果独立封存，
生产发布不由普通平台 CI 自动提升。
