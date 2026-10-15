# Nexus 通用嵌入式平台维护

Nexus 仓库负责可复用的 MCU 平台：Arch、SoC/controller、Board、HAL、OSAL、通用 Runtime、组件 core/adapter、配置构建和验证工具。产品 main、任务、领域控制、客户协议、产品分区、升级/健康/制造策略由外部应用仓库拥有。公开实例放在 [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)，不向平台反向引入业务源码。

用户授权移除不良设计与破坏性重构，已将 GD32 首发改为 GD32F470ZGT6 梁山派。首发还包括 STM32F407ZGT6 启明欣欣 V3.1、STM32F407VET6 天空星青春版，保留 Discovery 参考。约 10 人团队、3–6 个月是投入与阶段规划；具体成员账号和支持责任尚须落实。

[下一代设计](../design/README.md) 从第一性原理重新定义架构、工程与接入，状态为 PROPOSED。它采用编译期静态装配、固定 typed 端口和 caller-owned 请求，不将现有 API、Kconfig、factory、OSAL 或工作流视为必须保留的约束。配套工程手册、接入契约与 33 项执行清单均是待实施设计；本节以下继续描述当前实现和资格范围。

## 阅读顺序

| 文档 | 作用 |
|---|---|
| [38 项执行清单](../implementation/refactor-execution.csv) / [执行说明](../implementation/refactor-execution.md) | 原 RF 任务依赖、实际源码/日志、软件范围、剩余验收；不把硬件或发布计为软件通过 |
| [完整重构计划](https://github.com/X-Gen-Lab/nexus-examples/blob/main/docs/platform-refactor-plan.md) | 38 项任务、19 个提交批次、主责和原始退出条件 |
| [目标架构](target-architecture.md) | 仓库边界、责任分层、依赖与演进条件 |
| [架构决策](architecture-decisions.md) | 当前已采用的软件合同与尚未实现能力 |
| [架构实现设计](platform-architecture-v2.md) | target、Runtime、Board/layout、源码 SDK 与真实消费路径 |
| [HAL/OSAL](hal-osal-design.md) | owner、generation、lease、deadline、ISR、关闭/恢复与后端能力 |
| [主流平台比较](platform-comparison.md) | Zephyr、FreeRTOS、厂商 SDK 等比较依据；不以比较稿代替实际性能测量 |
| [支持矩阵](support-matrix.yaml) | 精确板卡/配置、已运行的软件范围、HIL 与功能缺口 |
| [企业工作流](enterprise-workflow.md) | 双仓 PR、分批提交、风险评审、CI/证据、发布与维护责任 |

原 [backlog.csv](backlog.csv) 保留早期 BAS/HAL/BSP 等建设任务，不能把其旧 Product/固定分区状态视为 RF 重构完成结论。本轮以 RF 执行清单为准；历史实现记录保留各自 source SHA 和检查范围。

## 当前实现

`Nexus::Runtime` 提供 `nx_runtime_bootstrap()`、`nx_runtime_shutdown()`、`nx_runtime_get_state()`；`nx_platform_get_info()` 是无副作用的当前配置查询。`Nexus::Firmware` 显式保留平台对象、startup/注册/强 IRQ 与类型化 HAL。平台不存在产品身份、自动业务 main、固定任务或必选业务 application choice。

HAL core、各 GPIO/UART/SPI/I2C/Flash facade、provider/support/runtime 分成独立目标。普通 consumer 只获得 opaque descriptor 与 owner/generation 值引用，provider 内部状态通过显式实现头使用。Log formatter core、OSAL runtime、UART adapter，Shell core/adapter 和测试 fake 分开。Config、Storage、Modbus 与 Update 使用窄端口；Security core 默认无 provider，OpenSSL 由 Native 配置与应用显式绑定。

每个构建只有一份 effective configuration。一个 `NEXUS_BOARD_DIR` 接入外部 Board，manifest 验证 source containment、密度/HSE、已维护 GPIO/UART/SPI 的 pin/AF/clock/IRQ/DMA 绑定。一个外部 layout 同时驱动 linker、region header 和 ELF identity；默认 image 使用整块物理 Flash 且没有 storage 区，非零 image offset 拒绝。

可重定位源码 SDK 已在历史 `5498b2b` 干净版本完成严格 publishable 包、移动后 C/C++ Native 运行与 STM32 ELF 消费。新源码须重新 prepare/verify/consume，不继承旧包身份。它不是 binary SDK；开发 fixture 明确 `publishable=false`。[源码包说明](../../cmake/package/README.md) 给出真实准备、verify 与 find_package 命令。

## 证据与当前边界

维护目录已统一为 `arch/`、`soc/{native,stm32f407,gd32f470}/`、`boards/`、`platforms/`。controller、clock/IRQ/system、Flash/identity、私有头与 SDK 属于 SoC，platforms 仅拥有 startup/lifecycle/对象装配；Native 是虚拟主机资源模型。家族目录与精确料号分开，F407VE/VG/ZG 密度不会合并。三平台使用共同 OBJECT 转发 helper，厂商 SDK 编译面保持私有。

当前交付状态由 [支持矩阵](support-matrix.yaml) 的 `current_delivery` 与外部 [双仓验证记录](https://github.com/X-Gen-Lab/nexus-examples/blob/main/evidence/platform-refactor-validation.json) 绑定；外部 Gitlink/lock 与被测 source/tree 一致后才有当前源码的软件通过结论。平台文档不回写自身最终 SHA，避免身份自递归。当前执行结果不能用过去计数代替，也不在本库回写自身最终SHA或PASS。

历史 `5498b2b` / examples tested `272200f` 完成 Native 1810/1810、8 个平台 contract ELF、Native 6 应用检查与 38 个外部 ARM ELF、严格源码 SDK 和在线门禁。历史 `3129550` 的 Native 1781、8 ARM/15 ELF 留在 [platform-validation.json](../implementation/platform-validation.json)，更早 `4a283eb`/`affaa86f` 的检查也保留各自源码边界。各历史 scope 不能累加或继承到新重构。

Native typed I2C 已实现；STM32/GD32 modern hardware I2C provider 当前明确不支持。HALCompletion caller-owned 队列已有 18 项真实 Native 软件检查与 Cortex 编译；它不自动接线设备 callback 或启动 worker。MCU crypto/熵源/vault、全 MCU kernel teardown/restart、完整内存域/cache/MPU、bootloader/非零 VTOR relocation、完整自动拓扑求解和 installed binary SDK 未完成。

本阶段按用户选择先软件与 HIL 工装，不连接实板。四板 fixture、board/probe/tty 租约、真实 ELF/BIN 准入和公开 UART challenge 工具已建立；没有执行探针、刷写/读回、串口、波形、物理掉电与长期负载。IRQ/DMA/掉电/长期 workload 的完整 runner 与实测预算也未建立，null 预算不是测量结果。

企业支持、LTS 和产品 promotion 未承诺。真实成员/reviewer、远程分支规则、持续回归资源、产品 trust/signing、制造和现场资格由对应团队落实；角色与 workflow 文件不能替代这些操作。
