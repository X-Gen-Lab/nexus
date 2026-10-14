# Nexus 嵌入式通用平台

Nexus 是面向工业 MCU 的静态装配 C11 平台，提供 CPU、控制器、类型化 I/O、
可选 OS 与通用组件机制。应用、workers、产品角色、私有 PCB、Flash 分区和
恢复策略由 [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)
或私有产品仓库维护。[English](README.md)。

一份 TOML 选择经过复核的 SoC／Board 事实和精确实例。当前保留 **typed factory
与 C 多态接口**：生成器创建已有静态对象，factory 只返回只读 face，不分配、
初始化、加锁或探测设备。日常 I/O 通过共享只读 ops 访问独立私有 context。
CMake 负责源码依赖图；Kconfig 与 cache 宏不构成平行配置权威。

| 层 | 责任 |
| --- | --- |
| Core | 结果、单调 deadline、请求借用和稳定 epoch |
| Arch | 编译期 CPU 原语、掩码、屏障及可选 Cache／MPU／SAU |
| SoC | 精确芯片内存、时钟、启动、IRQ／DMA 与私有控制器实现 |
| Board | 复核接线、晶振、初始电平和明确未知项 |
| I/O | 公共类型化操作合同，不暴露 provider／SDK 布局 |
| 可选 OS／组件 | 调用方显式提供存储、窄端口和执行进度 |
| 工具／测试 | 实际配置、构建、资源、模型、ARM 链接、HIL 准备和证据 |

默认没有 heap、运行时注册表、隐藏驱动任务或最大对象池。已接受的异步请求
保持借用，直到 acquire 观察 SETTLED；cancel、timeout、失败排空不能撤销借用。
IRQ／DMA 的有限与连续模式是明确选择，具有各自的缓冲贷款和执行合同，
不承诺任意 IRQ 延迟和负载下无损。

## 从可验证任务开始

- [独立搬移与消费 Source SDK](docs/tutorials/source-sdk.md)。
- [执行拒绝、取消、排空与隔离实例](docs/tutorials/async-io.md)。
- [完整装配格式与拒绝规则](docs/reference/configuration/assembly.md)。
- [平台分层、factory 与所有权](docs/explanation/platform-architecture.md)。
- [精确支持与资格范围](docs/reference/platforms/support.md)。
- [TDD 和正常 commit 门禁](docs/development/workflow.md)。

网站通过 gettext／PO 提供关键路径中文翻译，并显示真实原文、缺译或过期状态。
GitHub 正文采用英语 canonical source；[文档入口](docs/index.md)提供统一任务导航。
原始[设计合同](docs/design/README.md)和[交付记录](docs/delivery/README.md)保留权威。
安装、构建与测试命令见[英文 README](README.md)，命令从仓库根执行。

## 支持与证据

启明 F407ZGT6、天空星 F407VET6、梁山派 GD32F470ZGT6 维护首发精确装配；
G474、H743、U575 各有独立窄范围参考。CPU 软件 runtime 覆盖
M0／M0+／M3／M4／M7／M23／M33／M55／M85，不自动提供 SoC／Board／factory。
ThreadX 维护 Native／M4 子集，不继承 FreeRTOS 的 CPU、隔离或低功耗范围。

[派生能力矩阵](docs/delivery/capabilities.md)区分模式、路由、板级声明、软件夹具和
实现源码。[资格说明](docs/reference/platforms/qualification.md)区分文档、代码、
实际测试和产物身份。**当前 MCU 实板 HIL 均未执行**；ARM 链接、模型通过或
文档发布不能建立物理时序、电气安全、签名产品发布或 LTS 承诺。

代码格式和反斜杠 Doxygen 约定保持根策略；commit／CI 保留真实失败，
不会自动暂存或悄悄修复源码。GoogleTest／GoogleMock 仅用于 host C++17 测试，
生产固件保持 C11。[历史资料](docs/archive/README.md)不建立当前支持或资格。
