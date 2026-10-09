# Nexus 下一代平台：从第一性原理设计

状态：**下一代架构合同；实现与验收映射见[当前交付](../delivery/README.md)。** 新代码采用本设计的静态装配、窄端口和显式所有权边界，旧运行架构已删除。本文的伪代码解释合同；精确可编译接口以当前公共头文件及测试为准。软件执行、离线可复现构建和实板资格分别记录，历史[架构](../archive/strategy/target-architecture.md)仅用于迁移对照。

本次迭代将设计、工程规范、接入契约与33项实施清单落实为新的生产代码和构建链。新源码与产物建立独立资格；历史通过结果不能继承。

用户明确要求代码格式和注释风格保持当前仓库规范。架构可以重构，排版、命名和Doxygen形式沿用现有配置及贡献/注释指南；通过完善合同内容提高可维护性。

配套资料：[工程手册](engineering-handbook.md)、[接入契约](integration-contracts.md)、[执行清单](next-generation-execution.csv)。实施阶段的决策可据实验结果修订，不能用架构提案代替实验。

## 1. 平台要解决的问题

MCU 平台的本质是把固定硬件、并发执行和有限资源连接起来，使多个产品复用可信实现。首先确定事实和约束，然后决定哪些抽象值得付费。

| 第一性事实 | 设计推导 |
|---|---|
| 一个固件通常只运行在一种精确芯片和 PCB 上 | CPU、寄存器、引脚、时钟、DMA 路由在构建期绑定 |
| RAM、Flash、带宽与执行时间都有限 | 每个对象有来源、容量、寿命和资源预算；启用什么才分配什么 |
| 外设和总线是真实的共享资源 | 静态写入权与单一总线执行者明确；共享请求经过有界仲裁 |
| IRQ、DMA 和 deferred 工作可能晚于调用函数返回 | 请求和 buffer 借用必须延续到所有使用者结清 |
| CPU 时间、RAM、Flash、功耗之间有取舍 | 在约束满足后比较 Pareto 候选；不以一个编译选项宣称同时绝对最优 |
| 企业项目需要多人并行维护和可重复交付 | 边界窄、事实单一、工具固定、错误可诊断、产物与证据一致 |
| C 类型与静态资源规划不能隔离恶意本机代码 | 编程契约与硬件隔离分别承诺；MPU/安全域需要独立实现 |
| 产品任务、升级、制造和健康策略各不相同 | 平台交付机制与端口，外部产品拥有政策和资格责任 |

优先顺序：满足正确性和产品硬约束；减少必需运行成本；减少预留资源；降低接入与维护成本。不能用提前交还 DMA buffer、遗漏错误或未检查的越界换取更小的 benchmark 数值。

第一阶段范围：Cortex-M4F、STM32F407ZG 启明欣欣 V3.1、STM32F407VE 天空星青春版、GD32F470ZG 梁山派；裸机和 FreeRTOS；Native 行为模型。Discovery 可保留为开发参考，不强制新产品依赖它。芯片精确型号、路由、执行模式和证据分别管理。

现有 controller 支持、引脚资料和故障测试仅用于识别需求。新实现可以直接替换不良结构。暂无实板接入，首阶段交付软件和 HIL 工装准备。

## 1.1 现有体系与选择依据

以下比较常见架构机制与适用条件。功能裁剪、编译器、配置和工作负载都会改变成本，性能排名需要同条件实验。

| 体系 | 适用机制 | 对本平台的设计启示 |
|---|---|---|
| 厂商SDK＋手工BSP | 单芯片直接控制、少量手写静态初始化 | 保留接近硬件的表达；跨芯片公共合同、资源规划和失败语义由平台补足 |
| Zephyr | Devicetree硬件描述、配置与统一device driver model | 借鉴构建期资源描述和能力检查；是否需要统一device model取决于运行时设备集合与产品需求 |
| FreeRTOS | 内核机制、应用提供静态task/object storage | 使用真实内核与逐对象容量；HAL、硬件描述和产品调度分别归属，不把内核变成设备中心 |
| RT-Thread | I/O设备注册、查找、open/close及统一访问框架 | 动态设备访问有明确价值时才支付相应身份和生命周期成本；固定物理端口采用更窄合同 |
| Embassy/Rust async | 编译期大小的静态task与显式async执行 | 借鉴按实际状态分配和显式执行责任；首版保持C生态，buffer借用与DMA结清通过合同和故障验证落实 |

参考官方机制说明：[Zephyr Devicetree](https://docs.zephyrproject.org/latest/build/dts/index.html)、[Zephyr device model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)、[FreeRTOS静态任务API](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/include/task.h)、[RT-Thread I/O框架](https://www.rt-thread.io/document/site/programming-manual/device/device/)、[Embassy executor](https://docs.embassy.dev/embassy-executor/)。这些参考用于架构取舍；正式依赖仍须锁定精确revision并另行验收。

选择依据是两SoC、三板、固定BOM、十人维护和C产品接入：构建期描述硬件，链接期选择实现，运行期只维护实际执行与借用状态。工业控制中的高频固定输出与企业设备管理中的运行时发现不是相同问题，需要不同的成本模型。

## 2. 默认运行模型：静态装配、固定绑定、最小驱动

默认产品在构建期选定硬件资源、组件、执行模式和容量。启动建立硬件配置，RUN 阶段保持端口身份、路由及写入权稳定。外设实际运行态和一次请求的借用状态由驱动维护。

默认不建设通用设备注册中心、runtime probe、每 pin 的 open/close/ref、全局对象继承树或隐藏线程。资源固定不表示硬件永不故障；驱动仍管理 BUSY、FAULT、取消、排空和受控恢复。

三个路径采用不同成本：

| 路径 | 执行内容 | 成本约束 |
|---|---|---|
| 构建期 | 选择、冲突检查、能力检查、静态绑定、容量与链接布局 | 可以充分检查，不进入固件高频路径 |
| 控制路径 | 初始化、校准、start/stop/recovery、配置变更 | 有界、明确失败与剩余资源；可使用诊断信息 |
| 数据路径 | GPIO 操作、请求启动、IRQ 数据搬运、完成状态发布 | 无名称查找、无分配、无无界扫描、无默认应用回调 |

默认固定端口使用 typed opaque context 和普通 C 函数；每个镜像选择一种对应的 SoC 实现。多实例共享实现。生产代码与 Native 模型实现同一合同，分别链接，不要求 runtime vtable。

同一镜像确有异构 provider，或组件需要注入 transport/time/storage 端口时，使用一个共享 const 方法表和 context。方法表用于真实变化点，不能按每个实例在 RAM 复制。不能把“希望未来可能支持”作为强制多态的依据。

不承诺所有调用都内联或 LTO 都能去虚化。普通函数、共享方法表、生成的直接绑定分别使用 ELF/反汇编及实板测量判断。默认先选择简单的可维护表达。

## 3. 责任边界与建议源码组织

责任边界不等于逐层调用链。GPIO 数据操作可以直接进入 SoC 实现；纯协议不经过 OS、Board 或系统生命周期。

| 部分 | 拥有的内容 | 公共依赖边界 |
|---|---|---|
| core | 少量状态码、基础类型、时间/请求的共同规则 | 标准 C；不提供通用可变设备树 |
| arch | 异常上下文、原子/屏障、CPU-local 临界区、必要的 cycle 能力 | 不依赖 vendor、Board 或 OS |
| soc | 精确芯片能力、clock/IRQ/DMA、controllers、Flash/内存、startup、私有 SDK | 实现 io 合同，使用 arch |
| board package | PCB 接线、晶振、外接器件事实、安全初值、特殊电气动作 | 只读资源与窄 hook；可在外部仓库 |
| io | GPIO、UART、SPI、I2C、Flash、timer/ADC 等各自的 typed 合同 | 公共头不暴露 SDK/内核类型 |
| os | 实际需要的时间、通知、锁与可选任务/storage 适配 | 裸机/FreeRTOS/Native 分别实现能力 |
| components | 器件驱动、协议、存储/日志等可复用 core 和明确 adapter | 窄 io/time/transport/storage 端口 |
| tools | 配置、构建编排、资源检查、证据、打包、HIL | 标准工具；不生成业务执行算法 |
| 外部工程 | main、任务、roles、私有器件/协议、布局、恢复/升级/制造策略 | 消费源码 SDK，不被平台反向链接 |

建议目标目录如下；实际命名在首个垂直切片中一次确定。目录迁移与接口实现一起完成，不维护永久双路径。

~~~text
core/
arch/<cpu>/
soc/<vendor>/<family>/
io/include/nexus/io/
os/<backend>/
components/<component>/{include,src,adapters}/
boards/<reference-board>/
tools/{configure,dev,evidence,hil}/
tests/{contracts,models,integration}/
docs/{design,contracts,integration}/
~~~

startup/linker/clock 属于所选芯片实现，最终 image target 装配一次。没有必须存在的 platforms 转发层。系统启动与逆序撤销可用少量普通 C 函数；不为目录对称性增加 runtime 模块。

组件定义在标准 CMake target 中，显式 sources、PUBLIC/PRIVATE 依赖和 include；vendor/生成实现头与测试 fake 不进入普通 consumer 的 include 路径。工具不得另维护一套软件依赖图。

## 4. 配置体系：三类输入，一份解析结果

首版采用 schema 校验的 JSON 和一个窄 Python 配置器。Kconfig 不进入首版新链路；未来需要交互式配置时，作为受控前端输出同一解析模型，不引入第二份有效配置。

| 输入 | 事实拥有者 |
|---|---|
| SoC package | 精确 part/package/density、内存域、reviewed route、实现模式、clock profile、SDK 来源 |
| Board package | 实际 HSE、PCB/source revision、连接、装配器件、跳线、电气、安全初值 |
| 外部 assembly.json | 所选 Board/backend、组件和实例、路由、执行模式、容量、外部 layout |

拒绝未知字段、未实现模式、矛盾和资源冲突。首版不支持 overlay/inherit 合并语言、自动拓扑求解或从 SDK 宏推导所有路由。使用者显式选择已复核资源，配置器给出字段路径、冲突资源及声明来源。

同一解析模型生成 resolved.json、配置头、只读 binding C、linker 输入和资源报告。生成器只做确定性校验和静态装配，不生成业务 main、worker、调度算法、器件状态机或驱动算法。

CMake Presets 选择 assembly、正式工具环境、优化 profile 和独立 build root。CMake 是源与依赖权威；它读取解析结果并检查组件所需能力，不重写选择结果。不允许 cache、手工 CONFIG 宏和生成头共同决定同一事实。

console/control/sensor等应用role由外部C composition映射到生成的typed物理端口；平台schema和resolved不生成role政策。硬件资源声明者的标识用于冲突诊断，不自动成为C调用权限或业务owner证明。

解析失败阻断构建；生成使用原子提交，旧输出不能作为成功回退。跨 Board、backend、ABI 或 instrumentation 使用不同 build root。发布记录所有输入和派生输出的身份。

资源计划检查 pin/AF、电气、controller、共享总线 endpoint、DMA stream/request、IRQ/共享 vector、EXTI line、timer/channel、timebase、clock、RTOS 保留异常、内存域、layout 和 ABI。详见 [接入契约](integration-contracts.md)。

## 5. 设备抽象：controller、endpoint、request、器件

四种实体采用不同模型：

| 实体 | 必需状态 |
|---|---|
| controller | 实际硬件/clock/IRQ/DMA 状态，active request、必要的仲裁和故障恢复 |
| endpoint | 不可变 CS/address、速率/mode、电气动作与连接参数 |
| request | 一次操作的 buffer、长度、绝对 deadline、结果、借用状态、必要 epoch |
| 器件 driver | 寄存器协议、校准、转换、初始化/重试与该器件状态 |

controller 拥有线上独占和执行；endpoint 不再复制 controller 锁和硬件生命周期。sensor driver 使用 endpoint 或窄 transport，不选择 PCB pin，不创建采样任务。

GPIO/PWM/ADC stream 不强行转换成字节事务。SPI 的 CS/全双工、I2C 的 repeated START/ACK/仲裁、Flash 的 program/erase 几何各自保持具体合同。共用 request 辅助规则，不建设万能事务执行器。

factory 可表现为生成的 typed 符号/访问器或低频诊断入口。固定实例的便利获取不承担动态构造、隐式启动和生命周期。名称与 strings 可在关闭诊断后从产物删除。

默认不为每 GPIO 保存 owner/generation。构建期证明资源规划不冲突；外部应用明确单一 writer/协调者。普通 C 代码遵守该组织约束，配置器不能证明所有用户源码没有越权调用。

需要runtime owner更换、资源回收、动态重绑定，或可撤销的observer/独立loan时，才引入checked session/epoch。固定输入被多个组件只读不需要此成本。此类managed模式使用同一controller，且排除未受管理的直接调用；不能给共享context增加一个token后继续允许旧静态调用绕过撤销。第一阶段不实现全平台动态设备管理。

GPIO batch set/reset要求两mask互斥且均为授权mask子集；动态输入越界明确拒绝，构建可证明的常量允许编译器消除重复检查。read是所选port的单次输入快照，不承诺跨port同时采样。toggle只在独占串行writer或明确原子实现下提供，不隐含与另一个ISR写入兼容。紧急输出控制与普通控制共享引脚时，使用产品明确的仲裁/硬件interlock，不能把两种写入上下文统称一个owner来消除竞态。

## 6. 默认并发模型

每个固定硬件 controller 有一个明确执行 owner。GPIO 输出由一个组件控制；共享总线由一个执行者串行执行请求。多个应用 client 可以通过显式 adapter 的有界队列请求它。裸机 owner 可以是 loop；FreeRTOS owner 可以是应用声明的 task。

运行时不自动创建总线 worker、日志 worker、completion worker。一个请求不在 HAL queue、OS queue 和 completion queue 中重复保存相同生命周期元数据。应用业务队列有独立意义时，由应用预算。

单 owner 路径避免内核锁；明确的多 producer adapter 才增加必要同步。API 声明调用上下文和串行要求；公共异步操作仍检查真实 BUSY/FAULT、长度、request 状态和容量。

默认 prepare/submit/cancel/result-consume/reuse由同一 request owner串行执行。其他任务通过显式adapter发送取消命令，不直接用旧 request 指针取消。跨任务控制使用 adapter-owned稳定身份和 expected epoch，并持有 storage到 owner确认；owner在命中仍有效的记录后再访问 request，不能对可能已释放的指针先读epoch。

直接提交路径中request owner与controller execution owner重合。共享adapter路径中，client负责prepare、提交控制命令、消费结果和最终回收；adapter执行者独占controller start/cancel/service/drain。client不在ACCEPTED期间直接修改request，也不通过wait调用controller service。跨任务命令的storage/身份保护由adapter承担。

短临界区保护 admission、active 指针、取消标记和 IRQ 发布。大 buffer 复制、wait、vendor 阻塞操作和用户代码在区外。DMA 在途不持续持有 task mutex；多producer适配必须能接受控制命令并使owner继续推进取消/drain。

SPSC 仅在恰好一个不可重入 producer 和一个 consumer 时使用。IRQ、error IRQ、task recovery 同时入队会改变这个条件。队列索引使用明确的 atomic/release-acquire 或短临界区；volatile 不充当并发协议。环回绕先用加一/比较，避免强制 power-of-two 容量造成不必要 RAM。

IRQ 只做规定的数据动作、状态发布和受支持的唤醒。不得分配、格式化日志、无界查找、阻塞或调用任意应用 callback。使用 FreeRTOS FromISR 时遵守 priority ceiling；更高优先级 handler 只能走可证明的 deferred 路径。

CPU metadata PRIMASK、FreeRTOS BASEPRI、总线 owner 和 DMA 生命周期是不同机制。架构审查后确定最小必要屏障，不能凭 benchmark 直接删除硬件顺序保障。内核计时与 HAL 计时独立规划；降低 RTOS tick 必须先实现真实可用的 HAL 时间源。

## 7. 请求、deadline、取消与完成

异步默认使用 caller-owned request 和 payload；排队仅借用指针或小 descriptor。以下 API 为合同示意，当前可编译接口见 `core/include/nexus/core/request.h`：

~~~c
nx_spi_request_prepare(&request, tx, rx, length, transfer_deadline);
status = nx_spi_submit(endpoint, &request);
/* status == NX_OK means admitted, not transfer success. */
nx_spi_cancel(endpoint, &request);
wait_status = nx_spi_wait_settled(endpoint, &request, wait_deadline);
/* A wait timeout retains request and payload lifetime. */
~~~

该示意采用直接单执行者路径；wait便利函数仅在所选适配具有等待和service能力时提供。共享adapter使用自身的client提交/取消/等待入口，不把公共请求辅助规则实现成强制万能controller调度器。

submit 明确 admission 结果：

- REJECTED：没有借用 request/buffer，没有遗留需要结清的硬件动作。
- ACCEPTED：借用成立，后续 start failure、IRQ error、timeout 和 abort failure通过 request 报告；不把已经借用的请求作为普通拒绝返回。

共享adapter的队列接受是这条链唯一admission点，立即建立借用和总deadline。后续controller暂时BUSY保留QUEUED，实际start失败成为request结果；不能向原caller倒退为REJECTED。QUEUED取消先移除全部排队引用再结清，满队列拒绝不得留下借用。controller直接提交和adapter提交复用同一request状态规则。

默认一个request owner/结果consumer。准备和复用要求上一次已结清且consumer/adapter不再持有。只有存在可保留的延迟身份记录/控制命令时才分配序列/epoch；该模式的身份不重复、耗尽明确失败。完全串行、无逃逸引用且旧源已drain的request不强制此字段，更不给固定pin制造额外生命周期。

| 请求状态 | backend是否借用 request/buffer | 含义 |
|---|---|---|
| READY | 否 | 可准备和提交 |
| QUEUED | 是 | 已 admission，等待 controller |
| ACTIVE | 是 | 硬件或 handler 可访问 |
| DRAINING | 是 | 正在停止源和排空使用者 |
| SETTLED | 否 | 硬件、IRQ、deferred 使用均结束 |
| QUARANTINED | 是 | 无法证明排空，保留责任直到受控恢复/reset |

transfer deadline 使用明确 monotonic clock domain 的绝对截止时间，包含排队。wait deadline只决定等待者停留多久。cancel accepted、传输超时、等待超时都不代表 SETTLED。

每个 execution profile 明示 deadline 观察者、service 调度要求、取消能力和 drain 上界。polling 不能宣称低 CPU 异步；硬件 Flash pulse 和异常 DMA不能被 C deadline 自动抢占。超时后的残余硬件访问必须有归属。

完成默认是 request 中的 level state 加单 owner 通知。通知是唤醒提示，通知丢失/满队列不会丢失唯一结果。用户 callback 由显式 task adapter执行，它自己持有并归还必要借用。默认驱动不保存应用 callback。

controller执行者的可选wait适配必须同时保证进展：先有界service，再检查request和控制命令；arm静态通知后重查完成/待service谓词，再等待最早的wait deadline、transfer deadline或最大service间隔。等待不得让唯一执行者阻塞在需要它自己完成的drain上。共享adapter的client只观察自身结果并防lost-wake；adapter执行者继续承担service/deadline/drain，不允许多个client接管controller。纯裸机使用显式pump；没有wait能力就不提供伪阻塞成功。

通知port提供level-latched或sequence语义，arm/重查/休眠之间不存在lost-wake窗口。无法保证不丢通知的port采用有界重查，不能无限睡眠。IRQ完成标记、取消控制和deadline到期都能使owner重新执行必要service；这些规则不要求通用executor或隐藏worker。

SETTLED 发布前，必须关闭/停止所有可能访问该request的硬件源、证明 DMA 不再访问、排空相关IRQ/deferred使用者，并完成必要 memory ordering。持续RX等独立存储路径不必因此停止。epoch不能替代drain：无硬件tag的旧IRQ可能读取新active指针。

默认IRQ记录硬件完成/错误标记并唤醒owner，需要task上下文的drain由owner完成。结果字段、从controller detach及必要通知对象先准备，release发布SETTLED是backend对request的最后一次访问；caller用acquire观察。发布后的通知只能使用提前取得且仍有效的静态owner通知对象。IRQ直接发布只有在旧源已经结清、发布后不再访问request且尾部不影响下一笔时才作为明确profile允许；Native并发模型也须证明相同借用终点。

同步便利 API 也遵守这些规则。如果它使用 stack request，返回前必须结清；如果等待预算到期而无法结清，不能返回并使该 stack 失效。此类操作选择 caller-owned 长寿命 request，或进入明确的 fail-stop/reset 政策。

SETTLED 表示 backend 的借用结束；caller 复用还必须满足结果已消费、observer/显式 callback adapter 已退出。adapter 自己持有的借用不能被一个 driver 状态自动解除。默认单 owner 模式不增加独立 completion latch；request 是唯一结果权威。

## 8. 启动、故障与停止

静态装配产生可读的初始化依赖顺序；用普通串行 C 调用建立 CPU/clock/timebase、安全初值、内核必要静态对象和所选驱动。外部应用显式启动业务组件和 scheduler。平台不生成业务 main 或自动开启所有设备。

失败逆序撤销，只撤销实际取得的资源；同时记录原错误、cleanup 错误和剩余所有权。发生残余 DMA/IRQ 时保留 context/request，不伪造回到 OFFLINE。

初始化动作声明可撤销与不可撤销效果。已启用IWDG、Flash option bytes或硬件锁不能假装由逆序cleanup关闭。默认watchdog启用由产品显式决定，已有启用状态也必须识别；后续启动失败记录不可撤销效果，产品选择继续喂狗的受控失败状态或复位。平台不在rollback中谎报watchdog已停止。

RUN 阶段固定端口身份和写入权。controller 内部受控 recovery 可以恢复同一端口，不借机换 owner/route。stop先关闭新admission并请求producer停止新增操作；controller执行者仍处理控制命令、deadline和drain，直到请求及adapter借用结清。完成通知发布者也须退出，随后外部应用join调用者/执行者并回收OS storage，最后关闭clock/引脚。不能先停掉唯一执行者，再join仍在等待SETTLED的producer。

无admission的直接GPIO writer和相关ISR必须先静止，才能改变mode/clock；通用stop flag不能撤销无检查的直接调用。通知对象即使静态分配，也不能在仍可能被wake访问时删除、复用或解除内核注册；request已SETTLED不足以单独证明通知对象可回收。

固定 hot path 的低成本依赖这个 quiescence 前提。默认没有任意时刻由另一个 task teardown/rebind 的承诺。需要该能力的产品使用单独评审的 managed binding 合同，并承担对应检查与状态成本。

安全输出的物理初值由 Board描述；产品在错误时选择停止、降级或复位。reset/启动电平、外部器件时序及输出连续性需要实板证明。默认不承诺 MCU 内核运行中的全平台热重启。

Board初值表达电气约束和已知inactive polarity；阀门、加热器等过程安全状态属于产品。两者不能因同称“安全”而互相替代。

## 9. 内存与资源策略

默认 core/io 没有 heap、隐藏 OS 对象或全量资源池。controller、选中的 mode 状态、endpoint、必要队列和 buffer按实际实例分配。固定配置/绑定/方法表放 Flash，可变 context零初始化放 BSS。

| 资源 | 默认策略 |
|---|---|
| 请求 payload | caller-owned，借用到 SETTLED；copy API 另选入 |
| queue | 实际 depth 与 item size；只存需要的数据；overflow 策略明确 |
| task | 外部声明的 stack/TCB storage；预算为各任务之和 |
| RX | 精确选择 event/byte/block 语义及容量；不默认最大事件池 |
| DMA | 仅 DMA mode 保留状态；地址域、alignment、alias 和借用检查 |
| trace/stats | 单独编译选择；关闭后无常驻格式化或每操作统计分支 |
| managed heap | 明确 opt-in、域/容量/失败/封存；libc 与 kernel heap分别记账 |

UART逐 byte时间戳、byte stream、DMA block loan是不同合同。需要精确帧间时间的应用选择保真 event profile；选择 block profile的应用接受它实际提供的时间信息。不能根据 DMA completion倒推出带间隙数据的每 byte到达时间。

circular DMA loan必须保证未释放窗口不会覆盖；做不到时使用有界复制、流控、停机或明确丢弃策略。buffer非 NULL不等于 DMA 可达。F407 CCM等 CPU-only memory域必须与 DMA域分开；GD按照精确芯片文档另列。

静态 RAM 以实际 ELF分配区域和独立保留区记账，避免把 BSS内的 stack/heap/buffer再重复加一次。高水位反映使用，不缩小已经占用的静态区域。Flash包括vector、代码、rodata、注册/绑定和 RAM初始化装载；不能只统计 text。

大而统一的 generic metadata先删除。具体状态仍保留：busy/fault、active request、IRQ/drain标记、实际 callback/loan借用和必要epoch。不能只凭字段大小删除一个安全不变量。

## 9.1 工业控制类别与首发实施范围

首先定义各类别具体合同，再按已复核连接和实际需求实现。公共类型的存在不构成 provider 支持；未实现的 mode 在配置阶段拒绝。

| 类别 | 必需抽象 | 必需验收 |
|---|---|---|
| GPIO/EXTI | 固定授权mask；输入/输出；真实line归属、边沿与事件 | 初值/原子输出；共享EXTI vector；事件溢出；停机排空 |
| UART/RS485 | byte/event/block接收语义；TX memory settlement与wire-idle；明确DE/RE电气动作 | error/loss/TC/取消；帧间时间；实际收发器与接线 |
| SPI | controller、静态CS endpoint、mode/speed、完整transaction | CS保持/释放；同步全双工；DMA/取消及deadline |
| I2C | 地址endpoint；消息序列；repeated START/STOP与错误 | NACK/仲裁/挂死；恢复动作与终态；不把bus recovery伪装成功 |
| timer/PWM/capture | 时钟域、timer/channel、周期/占空比、硬件trigger、固定安全值 | 时间精度/边界；原子更新；ADC触发资源；停机输出 |
| ADC | 已审channel/scan/trigger、样本格式、校准、block借用/溢出 | 采样率/精度；DMA可达；sequence/trigger；丢失和buffer回收 |
| watchdog/reset | 硬件配置范围、feed与reset cause | 实际超时复位；调试冻结行为；上电/复位差异 |
| 内部Flash | 物理几何、region、program/erase、不可中断脉冲 | 越界/对齐；硬件终态；掉电与暂停预算 |

第一个软件垂直切片完成GPIO、时钟与请求模型；UART、SPI、内部Flash是基础通讯/持久化切片；I2C、watchdog及timer/PWM/ADC/EXTI按工业需求逐模式进入后续批次。NG-018、NG-031、NG-032、NG-033分别管理watchdog、EXTI、固定timer/PWM、ADC单次/低速scan，进入对应故障/资源验收和候选依赖，不能用一个类别总状态关闭所有模式。

CAN/CANopen、Ethernet、USB、低功耗、cache/MPU、安全启动和远程更新需要明确产品需求、维护依赖及独立预算。架构保留窄端口接入位置，首个候选不要求全芯片外设覆盖；也不公开仅有SDK壳的能力。

RTOS tick、HAL时间、UART时序、ADC/PWM触发分别占用实际timebase资源。选型和配置不能让多个用途无意占用同一timer/channel。未确认三板的外设接线不从芯片具有该外设自动推导。

首发模式目标在B0冻结为：EXTI指定边沿的有界事件；timer固定时基与PWM输出；ADC单次/低速scan；IWDG feed/reset-cause。capture、advanced timer break/dead-time及高频ADC DMA stream是独立扩展模式，未实现前配置拒绝。高频工业控制必须选择并验证相应DMA/trigger模式，不能用single-shot基础支持覆盖它。

- timer说明频率、tick单位、wrap扩展、最长维护间隔和clock/stop行为；PWM同时检查共享PSC/ARR/base mode，明确0%/100%、shadow更新、误差及启动/停止电平。
- ADC说明分辨率、sample time、参考电压/原始计数含义、sequence/trigger、overrun、时间信息和超时输出有效性；DMA stream另定义窗口覆盖和借用。
- EXTI说明line/port mux、共享vector、边沿合并/丢失、时间精度和pending清理；debounce策略由产品决定。
- IWDG说明启用不可逆性、已有状态、含LSI误差的超时范围、固定feed责任、reset cause及debug freeze；健康判断不进入平台。

## 10. 性能策略与成本验收

默认优化对象是总成本：CPU cycles、最大观测屏蔽时间、Flash加载字节、RAM保留、stack峰值、吞吐/丢失率、能耗，以及新增实例边际成本。

先测最小垂直切片：空 image、GPIO、UART、SPI请求，分别为裸机和 FreeRTOS。新板 profile只选切片必需资源，不能把完整板配置当作最小平台成本。

| 操作 | 设计成本目标 | 验证方式 |
|---|---|---|
| 固定 GPIO set/reset | 无注册查找/通用ref/OS锁，使用授权 mask，支持同port batch | 反汇编＋DWT/logic analyzer |
| request admission | O(1)或明确固定队列上界；无payload复制/分配/名称查找 | 模型故障＋边界输入＋cycles |
| IRQ数据发布 | 有界动作，无应用callback/无界扫描；index发布正确 | 竞态模型＋优先级负载＋实板 |
| completion观察 | 从request读取已有状态；通知不构成结果权威 | 通知满/丢失＋取消/late IRQ |
| 新实例 | 只增加该实例必需context/storage与只读绑定 | image delta/map＋配置说明 |
| 禁用功能 | 不保留相应RAM和不可达方法引用 | 符号/section/实际ELF差分 |

正式优化 profile从 O2/Os/O3与 LTO比较中选少量组合，不沿用 CMake默认或发布器硬编码。按单个热点确有收益时局部优化；不全局启用 fast-math或删掉必要边界检查。

测量记录clock/waitstate、ABI/FPU、compiler、执行模式、负载、IRQ干扰、测量开销和ELF身份。不能把指令数当cycles、Native wall time当MCU deadline、观测最大值当严格WCET。DWT使用快照差值，profiler不能重置驱动使用的共同monotonic时基。

绝对预算由参考benchmark与产品共同确定。平台先保证结构性约束和软件资源门禁；只有实测满足指定环境和负载，才承诺相应性能。架构不能无条件证明在所有负载、编译器和硬件上全局最优。

## 11. 工程工具、风格与管理

默认 C11 production，公共 ABI保持 C；C++可用于 host tests，不作为 driver对象系统。正式工具用锁定 ARM/Native compiler、CMake/Ninja/CTest、Python、clang-format/精选clang-tidy和一个host测试框架。其他工具只有覆盖明确缺口时引入。

正式环境锁定版本与 OCI digest，源依赖另外锁定内容和许可。一个薄 dev入口编排原生命令；本地与CI复用，不建立第二build/test/workflow engine。

风格沿用根目录.clang-format/.editorconfig：80列、4空格、K&R attached braces和类型侧pointer alignment。公共symbol延续模块前缀、type的_t后缀和既有命名；private实现static，header自足，整数/长度/序列化规则明确。vendor代码独立target，不通过全局关闭warnings掩盖own代码。

注释沿用反斜杠Doxygen、现有文件头与section/inline形式。public header写完整参数和返回合同，source避免重复；调用context、阻塞和deadline起点、所有权、输出有效性、取消/结清及失败剩余状态用现有标签说明。共享规则放类型/模块contract，函数仅写差异。内部注释解释竞态、屏障、勘误和取舍。文件头保留作者/版本/日期等现有约定，Git补充历史追踪。具体规则和现有落实缺口见[工程手册第12节](engineering-handbook.md#12-代码风格注释与静态检查)。

状态码服务于caller可以执行的恢复动作；raw vendor诊断为可选detail。API不隐式格式化日志、不写全局last error、不在IRQ调用全局用户handler。普通错误与invariant违约分别处理。

十人配置沿用职责投入，不绑定旧代码边界：TL1、core/io/os2、STM/GD BSP2、器件/通信2、QA/HIL2、build/release1。每个维护域落实primary/backup。普通变更一个独立domain reviewer；公共合同、IRQ/DMA、同步、持久化和发布链增加消费者/跨层review。

状态分为设计、实现、软件资格、硬件资格、发布资格。任务不以代码行、PR数、文档数量或测试数量完成。详见 [工程手册](engineering-handbook.md)。

## 12. 验证、交付与平台接入

验证层次：fast local → PR集成 → nightly广度/负载 →专用HIL →同工件晋升。检查选择依据契约与风险，配置、同步、公共API、linker/toolchain变化触发跨层检查。

核心不变量：REJECTED无借用；ACCEPTED的错误仍有归属；SETTLED后backend/DMA/IRQ/deferred不再访问request/buffer；caller回收还需consumer/adapter退出；request复用前排空旧源；一个bus仅一个wire-active事务；stop先禁admission再排空；overflow/通知满不伪造成功；wait仍能推进drain且无lost-wake；资源计划与实现一致。

模型测试覆盖partial start、late IRQ、epoch复用、queue满、取消和deadline竞争、abort失败及stop重试。ARM真实链接覆盖startup/vector/strongIRQ、layout、ABI和资源。实板覆盖TC、DMA drain、电气、屏蔽时间、高水位和长期负载，当前明确未执行。

provenance证明输入/命令/输出绑定，hermetic证明只读声明输入，可复现证明独立clean build字节相同。三者单独验收。正式产物从完整锁定输入构建，封存ELF/BIN/map/config/manifest；晋升同一hash，不在最后发布步骤重建另一个image。

支持单元由 exact part、Board source/PCB revision、route、execution mode、backend与resolved配置确定。source资料、编译/链接、model、physical、product负载分别记录。一个Board名称不代表全部外设已支持。

新Board交付少量硬件事实和必要电气hook；新SoC交付真实provider/startup/clock/IRQ/memory/SDK；新器件交付窄transport与器件逻辑；新协议交付纯core与adapter；外部工程交付assembly/main/tasks/policy。典型流程和源码SDK升级见 [接入契约](integration-contracts.md)。

## 13. 实施顺序与退出条件

每批按新公共合同原子更新实现、caller、测试和文档；外部examples更新独立SDK pin。既有接口允许直接删除；持久化数据若已部署，仍需独立迁移与回滚政策。

| 批次 | 内容 | 软件退出条件 |
|---|---|---|
| B0 | 设计冻结、需求/风险、基准workload与预算schema | 一套可评审合同，未实现/未测量状态准确 |
| B1 | 正式工具、源码包、schema配置、最小image/target | 两SoC真实最小ELF；错误输入真实拒绝；一个有效配置 |
| B2 | CPU/clock/memory/启动与固定GPIO | 三Board基础组合；直接/batch路径；安全初值及输入身份一致 |
| B3 | 请求、UART/SPI/I2C/Flash及EXTI/PWM/ADC/IWDG首发模式 | cancel/deadline/drain与不可逆效果不变量；按实际支持模式链接和故障回归 |
| B4 | 按对象OS storage、通用器件/组件、外部examples | 真实外部consumer；业务不进入平台；未用资源不驻留 |
| B5 | 静态分析、资源/性能工具、hermetic/reproducible、HIL准备 | 软件门禁、独立重建、真实工件准入；物理结果仍未执行 |
| B6 | 新候选与迁移收束 | 新源码资格与交付证据完整；旧入口删除；支持范围真实 |

具体依赖与主责见 [执行清单](next-generation-execution.csv)。十人3–6个月用于范围受控的迭代，不能在此窗口默认完成所有外设、自动拓扑、动态设备系统、低功耗、MPU、安全更新和企业LTS。

以下是十人投入下的排期假设，任务依赖和退出证据优先于日历。各provider首个切片即带故障验证、格式/warning和最小资源预算，B5负责扩大与收束，不能把检查推迟到最后。

| 时间窗 | 主线及并行工作 | 阶段结果 |
|---|---|---|
| 第1–2周 | B0；工具与QA同时准备最小workload和锁定环境 | 首发模式、资源口径、失败/借用合同明确 |
| 第3–6周 | B1；两BSP准备精确startup/memory，接口组建立请求模型 | 外部消费到两SoC最小ELF的完整切片 |
| 第7–12周 | B2及B3基础路径；两BSP并行GPIO/UART，QA同步故障模型 | 三板基础通讯软件基线与资源报告 |
| 第13–18周 | B3工业模式及B4；器件/通信组迁移组件和六个基础examples | 首发EXTI/PWM/ADC/IWDG及明确I2C范围，裸机/FreeRTOS消费闭合 |
| 第19–24周 | B5/B6；QA与构建组收束门禁、离线重建、HIL准备和旧路径删除 | 范围受控的新软件候选、精确支持记录和可用工装准备 |

第3个月基线与第6个月候选承担不同范围。实板接入后按实际产物继续物理验收；硬件依赖未关闭时保留未执行状态，不用缩小测试范围来维持日历承诺。

首个验证切片是固定GPIO及caller-owned请求状态模型，再实现一个必要的真实UART或SPI执行路径。DMA列为独立模式与资格项，不能因为SDK提供函数而计完成。软件候选与产品实板资格分开交付。

## 14. 会删除或不引入的默认复杂度

- 逐GPIO的通用注册/owner/ref/generation及热路径字符串查询。
- 每实例复制的可变OOP接口、raw getter与managed引用并存的长期兼容。
- 未选模式的DMA handle、最大legacy copy buffer、全类型最大对象池。
- 必须经过统一OSAL的所有kernel功能、通用executor、隐藏worker和默认completion队列。
- 重复配置事实、多重overlay、生成业务流程的DSL、所有引脚自动求解器。
- 重复构建/发布入口、只看test count的资格、与实际产物不一致的PASS报告。
- 为名称现代化而拆仓、增加目录转发层或一次引入多个构建/包管理体系。

新方案保留的是实际问题所需的状态和证据：固定资源规划、真实请求借用、故障与drain、明确context、实际内存/时序、可重复工程与外部产品边界。
