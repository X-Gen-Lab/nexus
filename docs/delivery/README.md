---
id: delivery.readme
type: reference
audience: consumer
source_language: zh_CN
---

# 当前平台交付边界

当前重构将生产 SoC 收敛为 STM32F4，现有 F407VE／ZG 及四个 Board 包迁入系列入口。
全部 vendor 来源保留；Native 移到显式测试支撑。应用任务、产品策略和私有 PCB
继续由外部工程维护。

本页说明当前阅读与验收入口，不把源码存在、文档构建或历史软件结果写成本次通过。
默认 F407 路径的启动、停止、失败回滚、共享资源及诊断已由手写 C 维护，生成器负责
静态连接。后续主线是精确 F4 型号扩展、所需运行期能力和真实测量，按实际回归与源码
身份验收。当前物理 HIL 为 `not_executed`。本轮已选软件范围的实际结果见下方固定
源码记录；文档更新不把这些结果转移到后续提交。

主机配置统一由 `tools/configure/` 维护；配置代码不进入 MCU 镜像。

| 入口 | 用途 |
|---|---|
| [当前架构](architecture.md) | 目录、依赖、运行路径和状态责任 |
| [本轮架构任务](architecture.md#本轮架构迭代任务) | WP01–WP12 与 IO01–IO20 的既有基础、实施增量和退出条件 |
| [精确支持](support.md) | 型号、板卡、模式与限制 |
| [能力矩阵](capabilities.md) | 事实、路线、装配与实际生产定义关联 |
| [源码定位](../explanation/source-map.md) | 各层权威入口与源码归属 |
| [资格边界](../reference/platforms/qualification.md) | 模型、ARM、SDK、候选和物理验收分开 |
| [本批变更与验证](https://github.com/X-Gen-Lab/nexus/pull/62) | 便携 I/O 重构、实际源码身份、普通门禁、SDK 与资源结果 |
| [前序变更](https://github.com/X-Gen-Lab/nexus/pull/61) | PR #61 保留自己的源码与执行范围 |
| [软件证据导航](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/status/software-verification.md) | 按原源码与配置读取执行记录 |

当前源码版本号保持 0.2.1；已发布版本仍以
[GitHub Releases](https://github.com/X-Gen-Lab/nexus/releases)和其固定 manifest 为准。
重构不移动旧标签，不重新标记旧产物。

本轮架构迭代以固定源码
[`a2b74d82`](https://github.com/X-Gen-Lab/nexus/tree/a2b74d826dc1545aa8f51b5fcbc05fcd1cc2c984)
为既有基础。该版本已有的流式传输、恢复和勘误修正，与本轮工作包新增内容分别记录。
本轮便携 I/O 的平台实现为
[`6b59f666`](https://github.com/X-Gen-Lab/nexus/tree/6b59f66628f0dbb415f6203ba917c1fe717bf3fd)，
其 Debug、Release、ASAN+UBSAN 和 TSAN 各执行完整 329 个 CTest 项、5982 个
GoogleTest 用例并通过。最终
[`4bf4b5a2`](https://github.com/X-Gen-Lab/nexus/tree/4bf4b5a258b3120c24f6d834545ac3f300ce801b)
仅继续迁移 TMP117 SDK 调用者；编译输入审计确认该文件不属于上述四种 Native
程序的输入，没有把前次运行写成该提交重新执行。

`4bf4b5a2` 的 clean-source SDK 已完成导出、验证、整体搬移、全部 34 个维护消费者
和消费后验证，165 条消费者子命令全部成功。SDK manifest、完整报告、失败尝试、
产物身份与本地交付包的校验值见 PR #62；它们不表示已上传 Release 附件。文档与
ARM 参考构建保留各自原始输入；[验收边界](qualification.md)说明不同结果的适用范围。
实板和远端 Actions 按用户要求暂缓，软件验收使用本地门禁。

退出平台的原始实现和记录可从
[重构前固定源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
追溯。原日志、来源和状态保持原身份，恢复或阅读它们不授予新源码资格。
