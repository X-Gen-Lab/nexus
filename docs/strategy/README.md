# Nexus 工业嵌入式平台建设与验收

Nexus 的首期定位是工业控制与设备联网 MCU 平台，面向约 10 人团队、STM32/GD32 产品和 3–6 个月交付周期。用户已授权删除不良旧设计并重构；维护目标是可重复构建、明确接口边界和可核验的设备交付。

原始审查基线为 `7a203266082ad1f655b6686713b2b7950ba31ec6`。2026-10-08 的维护已实施有效配置生成、HAL/OSAL 生命周期、SPI/I2C 交易、原子持久化、维护中的密码 provider、更新策略、工业服务及企业交付工具。源码已进入后续集成，不再把这些工作全部标为“待实施”。最终提交的完整构建、运行与工件身份以实际验证记录为准；历史 [首批迭代](first-iteration.md) 只记录当时的快照条件和检查范围。

## 阅读路径

| 文件 | 内容 |
|---|---|
| [维护任务清单](backlog.csv) | 38 项工作的状态、已执行证据和剩余验收条件 |
| [本轮平台交付](../implementation/platform-delivery.md) | Arch、类型化设备、静态 OSAL、Product 与三首发板的代码和验收边界 |
| [实施记录](../implementation/) | 已落地的构建、驱动、存储、安全、工业服务和交付工具 |
| [支持矩阵](support-matrix.yaml) | 每个组合的验证范围与阻塞；没有企业支持或 LTS 宣称 |
| [目标架构](target-architecture.md) / [架构决策](architecture-decisions.md) | 依赖方向、资源所有权、配置与版本格式边界 |
| [平台重构详细设计](platform-architecture-v2.md) | HAL/OSAL、Arch/SoC/Board/Product、源码 SDK 与构建目标的具体边界 |
| [主流平台比较](platform-comparison.md) / [HAL/OSAL 设计](hal-osal-design.md) | 官方架构对照、底座选择、设备与异步操作契约及迁移条件 |
| [企业研发工作流](enterprise-workflow.md) | 需求、开发、验证、发布、制造和现场维护的责任与证据 |
| [质量与安全](quality-security.md) / [路线图](roadmap.md) | 产品预算、风险以及 3–6 个月阶段退出条件 |
| [独立边界审查](../implementation/independent-review.md) | 定向发现、修复、回归和仍未关闭的审查项 |

## 怎样读取进度

`backlog.csv` 保留原有任务 ID、优先级、阶段、责任角色和依赖，新增 `evidence` 与 `remaining_acceptance`。软件实现、模型验证、物理验证和产品支持是不同的完成条件：

- “软件已实现并定向验证”表示对应实现和真实软件回归已经执行，最终源码集成与全部目标矩阵仍由验证包绑定。
- “实现并模型验证”表示 runner 或故障模型的行为通过，不能计作真实板卡、签名服务、工装或产线验收。
- “待最终集成/审查”表示已知生产路径或生命周期问题仍需关闭，不能将其列为完成。
- “输入受阻”“按产品选择”“未承诺”分别表示缺少硬件输入、可选需求和尚未建立支持责任。

这些状态是按每项验收边界报告进度，不以文件数量、接口数量、测试数量或自动检查定义代替交付证据。具体成员账号尚未指派；岗位配置已落到 `.github/maintainer-roles.json`，为 1 名技术负责人、2 名内核接口、2 名 BSP、2 名工业应用、2 名 QA/HIL 和 1 名构建发布，安全、制造、现场接口挂到相应角色。

## 当前实现与支持范围

重构审查基线 `affaa86f` 的线上 Native GCC、Clang 和 coverage 配置各执行 1711 项 CMake/CTest，均通过；线上选定 ASan/UBSan 23 项使用 LeakSanitizer 执行通过。本地 LeakSanitizer 受 ptrace 限制，不计通过。指定生产源码范围的行覆盖率为 80.8%，不能解释为完整 MCU 行为、分支覆盖或硬件验收。pinned FreeRTOS kernel 的 POSIX 端口用于软件行为检查，不能代表 ARM 中断端口已上板。后续重构须重新验证，不能继承这些历史结果。

本轮独立 Arch、类型化 GPIO/UART、六类静态 FreeRTOS 对象、Product 启动/回滚和源码 SDK 消费已落地。SPI/UART Board 通过窄资源端口连接，SDK 留在实现目标内部，服务真正按配置启用。启动前 BASEPRI 遗留、TCB 与 mutex 身份复用、GPIO 假成功、UART 索引与缓冲取消等问题有生产路径回归。最终完整测试和线上门禁以新提交的实际记录为准。

首发硬件由用户确认：STM32F407ZGT6 启明欣欣高配 V3.1、STM32F407VET6 天空星青春版、GD32F470ZGT6 梁山派；原 F303 目标被 F470 替换。保留 F407VG Discovery 参考。8个裸机/FreeRTOS ARM Release 配置使用固定 ARM GNU 14.3.rel1；GD32 官方3.3.3 SDK有下载、archive和逐文件身份检查。STM32根据实际密度选择sector10/11或6/7；GD32采用F470专属4KiB页擦除。官方 startup 的真实向量/SP/Reset/强IRQ和内存段均作为静态工件验证条件。

没有连接实板。PCB revision、供电、电气时序、IRQ/DMA/TC、Flash断电和最坏负载预算仍需独立 HIL。RS485服务仍需要产品transceiver/DE/RE连接与测量。CAN/CANopen、Ethernet/MQTT 是 COM-002/003 的产品选项；完整自动资源拓扑校验、MCU全平台shutdown和完整installed binary SDK也没有被声明为完成。

Native 使用 OpenSSL 3 的 CSPRNG、AES-GCM、SHA-256 与 Ed25519。MCU provider 未绑定时明确 UNSUPPORTED；产品熵源、key vault、防回放、bootloader、镜像安装器和安全计数器仍需接入。旧 CBC/NXCFG、binary v1 以及旧配置入口没有静默兼容迁移；已有现场数据须有独立、可回滚且经过验证的迁移方案。

## 下一阶段的退出条件

本轮发现的 Native 生产注册、宿主退出及 timer reclaim 软件边界问题已修复并回归。当前软件迭代在干净源码提交上重新构建并统一验证矩阵，执行在线工作流，绑定 source、有效配置、工具链、ELF/map 和测试报告。

产品阶段需要确定首发功能、实物 PCB revision，冻结控制周期、jitter、通信、栈池、Flash 暂停和恢复预算，再接入受控实验室 HIL、RS485、电源故障工装及真实密钥/更新端口。约第 3 个月以一个组合的受控试点为目标，第 6 个月以双平台企业候选为目标；日期不能替代阶段验收。

企业交付工具已实现同工件摘要、map-linked archives SBOM、物理 HIL 准入、公钥签名验证和制造身份预留。正式发布还需要真实 HIL、完整组件许可/安全审核、企业信任与签名服务、量产工装和团队主备责任；现有软件设施没有自动满足这些条件。企业支持和 LTS 必须在持续回归资源、支持窗口、补丁责任和现场证据落实后单独决策。
