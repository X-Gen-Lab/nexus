# 通用平台重构执行记录

本记录映射外部 [platform-refactor-plan.md](https://github.com/X-Gen-Lab/nexus-examples/blob/main/docs/platform-refactor-plan.md) 的 **38 个任务、19 个提交批次**。[机器清单](refactor-execution.csv)保留原任务 ID、优先级、依赖、角色、批次与验收条件，依赖图无循环。计划、源码、软件执行、物理资格和运营支持分别核验。

用户本阶段暂不接实板，交付范围为通用平台软件、外部应用消费、构建/发布工具与 HIL 工装准备。首发为 F407ZG 启明欣欣 V3.1、F407VE 天空星青春版与 F470ZG 梁山派，保留 F407VG Discovery 参考。**没有执行探针、刷写、串口、波形、掉电或长期负载资格。**

## 当前状态与身份权威

目录统一后，controller、clock/IRQ/system、芯片/虚拟资源、Flash/identity、private 头和 SDK 装配都位于 `soc/{stm32f407,gd32f470,native}/`；`platforms/`只拥有启动、平台生命周期与最终对象装配。家族目录不代替精确料号/密度；Native 不宣称物理芯片。三平台使用共同 OBJECT 转发 helper，保留实际 startup、注册与强 IRQ。

本轮统一源码的实际检查结果与被测身份以外部报告为准。当前软件状态见 [支持矩阵](../strategy/support-matrix.yaml) 的 `current_delivery`；确切被测双仓 source/tree、依赖、配置、工具链、命令、退出码、工件与原始报告由外部 [platform-refactor-validation.json](https://github.com/X-Gen-Lab/nexus-examples/blob/main/evidence/platform-refactor-validation.json) 绑定。平台文档不把自身最终 SHA 写回以产生身份自递归。外部 gitlink/lock 必须匹配被测 Nexus，报告记录验证的实现提交与其后文档提交区别。

历史干净软件交付 `5498b2b6988cc1cf28cf4255c5c281d56394d614` / examples tested `272200f40eca5b414e4aa97f5c5be67a0659bc78` 已完成 Native 1810/1810、8 平台 contract ELF、6 个 Native 应用检查、8 ARM/38 外部 ELF、严格 publishable source SDK 消费及在线门禁。原始报告保存在外部 `evidence/reports/5498b2b/`。该完整基线不自动资格后续目录/lifecycle/provider/配置变化。

CSV 的 `software_verification`、`commit_status`、`commit_ids`、`execution_evidence` 与 `observed_*` 保留原 f2d 定向快照身份；`code_evidence`跟随当前实现路径。新增 `current_change_verification` 与 `current_delivery_record`明确本轮重验，不把原来“final pending”误报为从未交付过软件，也不把历史通过当新源码通过。计数必须读取各自实际报告，重叠 scope 不累加。

## 状态口径与真实剩余条件

| 状态 | 含义 |
|---|---|
| `implemented` | 声明范围的生产实现存在；当前源码的软件验证仍由新报告单列 |
| `partial` | 已实现部分契约，未实现/未验证能力保留具体退出条件 |
| `tooling_ready` | 软件工装可用，station/物理资格未完成 |
| `documentation_in_progress` | 原快照中的文档/岗位状态；当前运营支持仍须真实成员与责任 |
| `planned` | 实板/长期资格未执行，部分 workload runner 尚未实现 |
| `see_current_delivery_record` | 当前源码结果读取外部精确 source-pair 报告；本库不回写 PASS 或自己的最终 SHA |

- **RF-HAL-03**：Native typed I2C 与 parent/child、deadline/cancel 已实施；STM32/GD32 typed hardware I2C 当前明确 unsupported。不得把 SDK 符号或 Native 模型当 MCU 接入。
- **RF-HAL-04**：Completion 是 caller-owned 有限 slot/entry FIFO；FULL 保留 ticket/context 等 producer 重试，只有真实 settled terminal 才能 post。它不证明硬件停止、不自动接线 provider、不创建 worker。
- **RF-BOOT-01/OS-01**：Runtime 只拥有串行 HAL/OSAL 基础设施；完整运行中 MCU kernel teardown/restart 不支持。设备/OSAL objects 必须先结清，失败保留所有权；READY 不是产品健康。
- **RF-BRD-01/02**：manifest 验证 reviewed GPIO/UART/SPI 路由、密度/HSE/pin/AF/IRQ/selected DMA 和 source containment；不是全芯片拓扑求解器。未实现 controller 不能通过填 manifest 获得支持。
- **RF-FLS-01/02/03**：Flash 暴露全物理几何；layout 外部选择 region，默认 whole-Flash image 且无 storage 区。StorageHAL 借用已打开的 uniform 完整 block region，不选择产品分区。image offset 仅 0，无 bootloader/VTOR/install/recovery 链。软件故障边界不代替物理掉电或维护时序。
- **RF-CMP-01/02/03**：core 与 adapter 明确；Shell 仍为单例并用动态 core 存储。Security 无自动 provider，OpenSSL 为显式 Native provider；MCU crypto/熵源/vault、真实 trust/install/health/security counter 仍须产品独立实现与资格。
- **RF-EX-01/02/CI-01**：应用源码在外部仓库。正式消费固定 gitlink/lock；candidate CI 若显式指定 commit 只是变更回归，不改正式 pin，不证明 core 的每个 PR 自动完成私有外部仓库回归。
- **RF-SDK-02**：交付可重定位 source SDK；完整 clean 包的 manifest/许可/依赖/export 和真实移动后消费需要匹配当前源码。development fixture 必须显式 opt-in、`publishable=false`。没有 binary SDK 或跨配置 ABI 承诺。
- **RF-HIL-01/02/03**：四板 fixture、board/probe/tty 租约、真实 ELF/BIN/Board/layout 准入与 UART challenge 工具已建立；station 未绑定。IRQ/DMA/掉电/长负载的完整 workload runner 与实测预算尚未建立；`null` 不是测量，`hardware_verified=false`。
- **RF-REL-01/02**：软件检查/SDK 通过不自动创建正式 release 或产品 promotion。真实 reviewer/backup、分支规则、支持窗口、持续回归资源、信任/签名与制造资格未建立，enterprise/LTS 未承诺。

## 19 批次的软件与后续验收

| 批次 | 已建立的软件范围 | 当前重验或外部退出条件 |
|---|---|---|
| C01 | 独立外部仓库、固定 SDK baseline、原始缺陷复现 | 新 source-pair 继续按实际记录绑定 |
| C02–C04 | IRQ/AF 修复、OSAL 能力/启动/mask/静态资源 | 统一 SoC/配置后的软件检查；物理 IRQ/AF/调度独立 |
| C05 | Runtime/Firmware、移除 Product/隐式 main | MCU 生命周期/失败回滚改进按生产路径重验 |
| C06 | HAL opaque consumer、provider、独立 facade/support | 所有 maintained typed registration/capability 与最小链接 |
| C07–C09 | Shell teardown、Log 锁/背压、静态 UART adapter | 当前 providers 下故障/lease 和实际 ARM consumers |
| C10 | 单外部 Board 路径、资源/密度/源码验证 | SoC 物理目录、private SDK 和 reviewed routes 保持一致 |
| C11–C12 | 全物理 Flash、region/StorageHAL、单 layout | current ELF/geometry/故障检查；物理 power-cut 未执行 |
| C13 | 通用组件/窄 ports/显式 crypto | 当前完整组件/external consumers；产品 trust/install 独立 |
| C14 | Native typed I2C、deadline/zero-ticket/Completion | 当前 provider/lifecycle 集成；MCU I2C 仍 unsupported |
| C15 | 八个外部应用与 SPI 示例、平台业务源删除 | 当前正式 pin 的 Native/8 ARM application matrix |
| C16 | 单有效配置、严格证据/CI/工具链与依赖校验 | 新 GCC/Clang/sanitizer/analysis/Python/8 ARM 在线结果 |
| C17 | HIL fixture、租约、准入与 challenge 工装 | 只做 read-only 软件准备；真实 station/预算/workloads 后续 |
| C18 | 严格可重定位源码 SDK 与实际消费者 | 新 clean 包 prepare/verify/relocate/Native+ARM consume |
| C19 | 权威架构、支持/风险/交付边界 | 当前冻结证据包；支持/产品 promotion 外部资格未完成 |

## 历史定向执行（f2d 快照，不是当前源码计数）

下表保留原模块检查与日志。它们可能重叠，不能累加成全平台测试总数；表内“最终待验证”是当时快照描述，后续 `5498b2b` 的完整软件矩阵另有上述源绑定报告。日志路径/摘要仍指原运行，当前源码应生成自己的记录。

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
