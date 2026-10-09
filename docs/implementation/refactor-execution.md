# 通用平台重构执行记录

本记录逐项映射外部仓库 [platform-refactor-plan.md](https://github.com/X-Gen-Lab/nexus-examples/blob/main/docs/platform-refactor-plan.md) 的 **38 个任务、19 个提交批次**。机器可读状态与原计划依赖保存在 [refactor-execution.csv](refactor-execution.csv)，原计划 ID、优先级、责任角色和验收条件均保留，依赖图无循环。

这是 2026-10-09 的提交前软件执行快照：平台观察 HEAD 为 `f2d2cf89f437ad69bf73a8d9eb068d600d53081f`，软件实现已分批提交，当前架构/证据文档仍在集成；examples 观察 HEAD 为 `1d517fe231305f4c3dc3b49fe74448aa96dee4ff`，其新 Runtime 调用者和八个应用已迁移，新的固定 SDK pin 与最终双仓矩阵仍在验证。本记录的实施快照截至代码提交 `f2d2cf8`；后续最终 clean SHA pair、完整构建和在线结果由外部 `nexus-examples/evidence/platform-refactor-validation.json` 绑定，不回写平台源码以制造 source revision 自递归。这里的“已实现”描述明确的软件范围，并不表示原计划所有软件、实板或发布验收已完成。

用户本阶段暂不接实板，当前交付范围为通用平台软件、独立应用消费、构建/发布工具和 HIL 工装。三首发板为 F407ZG 启明欣欣 V3.1、F407VE 天空星青春版与 F470ZG 梁山派，另保留 Discovery 参考。**没有执行任何物理刷写、探针、串口、波形、掉电或长期负载资格。**

## 状态口径

CSV 分别保存实现状态、软件验证、物理验证和提交状态；没有把它们合并成“全部完成”。

| 实现状态 | 含义 |
|---|---|
| `implemented` | 表中声明范围的生产实现存在，已有真实定向软件检查；最终 clean 集成矩阵仍单列 |
| `partial` | 已实现部分契约；remaining_acceptance 明确尚未实现或验证的能力 |
| `in_progress` | 仍在集成生产路径/调用者，不依据新文件或 target 名称计完成 |
| `tooling_ready` | 工装、准入或模型可用，物理 station 和资格未完成 |
| `documentation_in_progress` | 当前软件/维护范围的权威文档仍在对齐，角色元数据不等于实际团队已指派 |
| `planned` | 实板/长期资格阶段未执行，部分 workload runner 也尚未实现 |

`code_evidence` 为实际实现路径，`execution_evidence` 为确实存在的日志/机器记录，`executed_scope` 写明该次检查实际覆盖内容。`examples:` 前缀表示独立 examples 仓库；`build/` 和 `/tmp/` 日志属于本次工作空间，交付时须收集到同一证据包。不能仅凭路径存在或提交消息通过验收。CSV 内的提交信息是观察快照，不是未来提交清单。

## 已提交的小批修复

| 提交 | 生产行为与执行范围 |
|---|---|
| `4ae7e3c` | STM32 ISR manager 采用正确外部 IRQ 编号空间；高 IRQ/非法输入/注册派发注销回归 |
| `74b175e` | GPIO effective AF 传入实际初始化；AF5/AF7/普通 GPIO 回归 |
| `acebc1d` | OSAL event capability、启动/ISR 契约和有限资源诊断；Native、裸机及真实 FreeRTOS POSIX 定向验证 |
| `f122668` | STM32 必须消费 generated config；VE/VG/ZG 容量一致性、物理 SRAM 与 main SRAM 上界检查；3 有效运行 + 7 编译拒绝 |
| `1956184` | HAL opaque consumer、provider contract、分层 facade 与支持目标，typed lifecycle/租约/最小链接及 Native I2C/runtime/零 ticket 回归 |
| `c5aa06d` | Log/Shell、Config 与显式 Security provider 拆分；实际 committed index tree 全量编译后执行 735 相关 CTest，0 skip |

后续软件批次已提交：`6153094` Runtime/Board/layout/fullFlash，`4742069` StorageHAL，`7af16f9` Completion，`2dc54c3` source SDK，`629cc5a` CI，`93ff0cb` HIL tooling，`f2d2cf8` maintained dependencies。Runtime/Board 精确提交树完成完整编译并执行 187 scoped CTest，0 failure/error/skip。当前文档与最终 clean 源码/双仓完整矩阵尚未闭环。最新六 Python gates 的日志、actual commands 和 source/artifact scope 另见 [platform-workflow-evidence.json](/workspace/nexus/build/platform-workflow-evidence.json)。已提交的 P0 修复不代表所有 C02/C03/C04 批次所需最终镜像和实板条件自动通过。

## 实际运行证据

下列计数属于各自源码工作树和选择范围，彼此可能重叠，**不累计成一个新平台全量测试数字**。每项列出的定向检查均记录零失败与零 skip；最终集成可能继续发现 ABI、配置或组件连接问题。例如组件 archive 曾通过，外部 LogUART 真正链接后才发现 LogCore 缺失 ARM build options，已修复，两 Discovery LogUART image 重新链接；组件精确提交树另执行 735 项。

| 范围 | 已执行内容 | 记录 |
|---|---|---|
| OSAL | 278 定向 CTest；3 ARM adapter 编译；真实 FreeRTOS POSIX 有限 token 耗尽，均未代表 ARM 运行 | [docs/implementation/osal-refactor-validation.json](../implementation/osal-refactor-validation.json) |
| HAL | 84 CTest，含 81 HAL 与 3 typed Log/Shell；62 core models 中包含 Flash/runtime/零 ticket；并非 MCU I2C 实现 | [build/hal-refactor-check/hal-refactor-results.xml](/workspace/nexus/build/hal-refactor-check/hal-refactor-results.xml) |
| HALCompletion | 18 实际 Native CTest，含仅 Completion/Arch 的纯 C 最小链接；Cortex Release archive 编译。没有自动 device callback 接线或 HIL | [completion-results.xml](/workspace/nexus/build/completion-refactor-native/completion-results.xml) |
| Exact Runtime / Board tree | 实际 index tree 完整编译后执行 187 scoped CTest，0 failure/error/skip；仍待最终完整平台和8ARM matrix | [junit.xml](/workspace/nexus/build/refactor-commit-checks/runtime-board/junit.xml) |
| Runtime / Init / SPI | 53 CTest：2 Runtime、6 Native SPI、45 Init；无完整 MCU 热重启 | [build/runtime-refactor-native/runtime-init-results.xml](/workspace/nexus/build/runtime-refactor-native/runtime-init-results.xml) |
| Log / Shell | 444 CTest：Log 140、Shell 304；四组 ARM archive 另有记录，archive 不等于 image | [build/components-refactor/native/components-results.xml](/workspace/nexus/build/components-refactor/native/components-results.xml) |
| External components / SPI | 实际 Shell/Config2 Native checks、Qiming/GD32四组件ELF、SPI1 Native CTest（内部3场景）与Discovery2ELF；显式开发SDK override，非final pin | [component-validation.json](/workspace/nexus-examples/build/component-check/component-validation.json) / [spi-validation.json](/workspace/nexus-examples/build/spi-check/spi-validation.json) |
| Exact committed components | 735 个组件/密码/integration CTest，实际 index tree 全量编译并运行；仍需最终完整平台矩阵 | [junit.xml](/workspace/nexus/build/refactor-commit-checks/components/junit.xml) |
| Security / persistence / Update | 77 选定 CTest；另 52 integration CTest；默认无 crypto provider 的 core-only consumer 明确 unsupported | [build/security-refactor/native/security-results.xml](/workspace/nexus/build/security-refactor/native/security-results.xml) |
| Board / layout | 18 实际 unittest；涵盖输入、资源与布局拒绝；不是完整自动拓扑求解 | [/tmp/nexus-board-layout-tests.log](/tmp/nexus-board-layout-tests.log) |
| Effective configuration | 27 实际 unittest；唯一生成、约束与资源配置回归 | [/tmp/nexus-effective-build-tests.log](/tmp/nexus-effective-build-tests.log) |
| StorageHAL / typed regions | 14 scoped CTest、8 StorageHAL 场景与 563 个逐字节故障边界；另 65 typed model CTest，无物理掉电 | [build/hal-storage-adapter-check/storage-hal-results.xml](/workspace/nexus/build/hal-storage-adapter-check/storage-hal-results.xml) |
| BSP IRQ / AF / Flash | 7 scoped CTest；真实生产路径 + 硬件替身；chip config 另外 10 个真实 compile/run/reject case | [build/rf-bsp/build/rf-bsp/junit.xml](/workspace/nexus/build/rf-bsp/build/rf-bsp/junit.xml) |
| External Board + Flash layout | 路径含空格的实际外部 STM32 Board、768 KiB image 和两个 region 的真实 ELF 检查 | [build/external-arm-layout-check.json](/workspace/nexus/build/external-arm-layout-check.json) |
| CI / release helpers | 历史 scoped 168 unittest；最新六 Python gates 共 281 项，其中 CI170/package83。实际 Qiming ELF 路径已运行，Native 新 bundle 分支待最终产物 | [build/package-refactor-helper-evidence.json](/workspace/nexus/build/package-refactor-helper-evidence.json) |
| Historical entry / strict JUnit | 25 unittest；拒绝空、旧、零测试、全部 skip 和异常 counts/report | [build/script-validation-checks/results.json](/workspace/nexus/build/script-validation-checks/results.json) |
| Installed source SDK | 12 unittest，真实 relocated Native C/C++ 运行与 STM32 C/C++ 两个 ELF；新增 canonical output alias 防绕过另 1/1 通过 | [/tmp/nexus-source-sdk-tests-final.log](/tmp/nexus-source-sdk-tests-final.log) |
| Installed source SDK path guard | 1 unittest；与前 12 次运行分别记录，最新 suite 共 13 项尚待一次 clean 全量 | [/tmp/nexus-source-sdk-path-test.log](/tmp/nexus-source-sdk-path-test.log) |
| HIL static artifact admission | 当前契约 ELF/BIN 准入 1 成功 + 6 篡改拒绝；hardware_verified=false；HIL 模型为新增 23 + 既有 20 | [build/rf-hil-artifact-admission.json](/workspace/nexus/build/rf-hil-artifact-admission.json) |

Runtime JUnit 当前摘要为 `6ffa1a317efbc904746085a8aec8fd7a94dd6d2e9175aa134a5276f7e84eca46`。SDK 12 项日志摘要为 `03f9a4bfca77d2ae2928f557bd98da26f256d2e85b324c27785bf7a5cf53a305`，新路径拒绝 1 项日志摘要为 `5dd06965a6109f867210ddfa8f16578789e4252ab99b45af14feacf5d4ecead0`。CI helper 的输入、源码与 actual ARM ELF 摘要见对应 JSON。发生源码或产物变化后需要重新绑定，不能沿用这些摘要代表新版本。

历史 [platform-validation.json](platform-validation.json) 中 `3129550` 的 Native 1781 项、8 ARM 配置/15 ELF 及在线结果，只用于重构前基线。独立 examples 的 `24ef09c` 绑定旧平台 `069dcf14` 的 3 Native 运行和 8 ARM/20 ELF，属于旧调用者的干净交付；新 API、应用和 SDK pin 必须重新构建验证。

## 实现边界与尚未关闭的能力

- **RF-HAL-03：MCU typed I2C 未实现。** 普通 consumer 的 typed parent/child/generation、pool、地址、deadline/callback/cancel/recover 已有生产 Native provider 回归；STM32/GD32 modern hardware provider 保持明确不支持。Native I2C 成功和 SDK 编译不能证明 MCU I2C 已接入。
- **RF-HAL-04：有界 Completion 软件 adapter 已完成。** finite-deadline UART wait 配合 caller-owned slot/entry FIFO，arm/post/dispatch 保持有限容量与显式 FULL 重试、terminal 单派发和 callback 生命周期；18 实际 Native CTest 与 Cortex Release archive 编译通过。它不自动接线现有设备 callback、不认证硬件 settlement 或创建 worker；最终 clean 集成/ARM image 和真实 IRQ/HIL 仍待执行。
- **RF-FLS-03：新 StorageHAL 已完成软件故障回归。** 实际生产 StorageCore→StorageHAL→typedFlash 8 场景与 563 个逐字节 program/erase/sync 故障边界通过；region loan、旧 token 不重定向、uniform block 和整次 open/load/save 单 budget 均有回归。14 scoped CTest 与 65 typed model CTest 无 skip，两 MCU adapter 实际交叉编译通过；最终 clean 集成、物理掉电与维护窗口测量尚未执行。
- **RF-BRD-01/02：资源校验有明确支持范围。** 已检查 reviewed UART/SPI/GPIO 路由、物理密度、HSE、pin/AF、IRQ、选定 DMA 完整绑定和外部 source containment。它不是完整外围资源拓扑求解器，未实现的 controller 不因填 manifest 自动可用。
- **RF-FLS-02：只有 Flash 基址 image 可启动。** 单输入 layout 驱动 linker、区域和 SHA 绑定，默认 image 使用整块物理 Flash且不自动创建 storage 区。非零 image offset 明确拒绝，尚无 bootloader、VTOR relocation、Boot/A-B 安装与恢复链。
- **RF-CMP-01/02/03：组件边界已收敛，能力不扩大。** Log formatter core 与 runtime/UART adapter、Shell core/adapter 和 test fake 分开；Config RAM/Flash、Storage 和协议通过窄端口装配。Shell 尚为单例并使用动态 core 存储。OpenSSL 是显式 Native provider；MCU crypto/熵源/vault、secure counter、安全认证与实际 Update boot/install/health 策略不在本次证明范围。
- **RF-EX-02：八个应用已迁往外部仓库，最终 SHA pair 待验证。** 新组件和最小 SPI 示例已存在；Native SPI 实际有限运行以及 Discovery 裸机/FreeRTOS 两个 SPI ELF 已通过。平台内 applications/examples 业务源已删除；外部完整矩阵、new pin/lock 和 clean examples commit 尚待绑定。
- **RF-SDK-02：交付的是可重定位源码 SDK。** 已真实移动源码快照、运行 C/C++ Native 消费者并链接检查两个 STM32 ELF；开发快照需要 opt-in，`publishable=false`，不能用于 promotion。最终 clean 默认严格包、verify 与 consumer 矩阵仍等待源码提交；无 installed binary SDK 或跨配置二进制 ABI 承诺。
- **RF-HIL-01/02/03：工装与资格分开。** 四板 manifest/schema、board/probe/tty 租约和真实 ELF/BIN 准入已可用，station 身份/命令为空。UART challenge runner 已实现但未碰串口。IRQ/DMA/掉电/长负载只有待测接口和 null 预算，没有完整 workload runner；物理资格均未执行。
- **RF-CI-01/REL-01/REL-02：最终集成与维护合同未闭环。** 新 Native GCC/Clang、sanitizer/analysis、8 ARM、独立 examples 同 SHA pair、clean source package 与在线 job 需绑定当前源码。候选工具通过不代表已创建正式候选；物理 HIL 依赖未通过，更没有 promotion。10 人岗位只定义责任模型，实际账号/reviewer/分支规则、支持窗口与持续回归资金尚未落实，LTS 未承诺。

## 19 批次的当前核验

| 批次 | 当前软件证据与剩余条件 |
|---|---|
| C01 | 外部仓库旧 API 的固定 SDK baseline 已提交并运行；新接口 SHA pair 待更新 |
| C02–C04 | IRQ、AF、OSAL 修复已提交并真实定向回归；新源码最终 ARM 及实板条件分开保留 |
| C05 | Runtime、Product 移除和显式 startup 已实现，53 scoped CTest；最终 clean 外部消费者待绑定 |
| C06 | HAL 分层已提交，84 scoped CTest 与最小链接；最终 sanitizer/整体 source matrix 待执行 |
| C07–C09 | Shell teardown、Log锁/背压、静态 UART生产回归已通过；最终 Release 与 ARM image 连接待绑定 |
| C10 | external Board/manifest 与实际独立 Board ELF 已验证；final eight images 与拓扑范围审查待收敛 |
| C11–C12 | 全物理 Flash、typed region、StorageHAL 软件故障边界与单 layout 的模型/实际 ELF 已验证；最终 clean 集成与实板掉电尚未通过 |
| C13 | core/adapter、explicit crypto、选定 persistence/Update 77 与 integration 52 通过；全外部组件 matrix 待绑定 |
| C14 | typed Native I2C、deadline、零 ticket 和 Completion18真实软件检查已验证；MCU I2C未实现，最终集成/物理派发尚未验证 |
| C15 | 八个应用与SPI示例已迁外部，平台业务源已删；final pin/完整外部source pair matrix尚未闭环 |
| C16 | 六Python gates281（CI170/package83）、入口/JUnit等已真实运行；当前clean全量/线上pair仍待执行 |
| C17 | HIL工装/23+20模型/actual static admission ready；物理刷写、时序、掉电与长期资格均未执行 |
| C18 | relocated source fixture12+1真实检查已通过；strict clean publishable source package+consume待执行 |
| C19 | 本记录按实际证据映射38项；候选、最终支持声明与运营维护合同尚未完成 |

下一步按可验证的软件边界提交并重建最终矩阵，更新 examples pin 后提交双仓记录，准备严格 clean 源码 SDK，把日志、源码/依赖/配置和同一批 ELF/BIN 摘要归档。当前软件交付保持 `hardware_verified=false`；实板阶段依用户后续启用，不以模型、station 模板或预留预算宣称资格。
