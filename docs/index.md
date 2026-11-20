---
id: index
type: reference
audience: consumer
source_language: zh_CN
---

(index)=
# Nexus 嵌入式平台

Nexus 提供可复用的 C11 平台机制：Core、Arch、类型化 I/O、静态 factory、可选 OS 和组件。产品任务、私有 PCB、Flash 分区与恢复策略由外部工程拥有。

选择一条任务路径：

| 你要完成什么 | 从哪里开始 |
| --- | --- |
| 第一次运行程序 | [Native 首次运行](tutorials/first-native.md) |
| 为已有板卡构建固件 | [F407 首次构建](tutorials/first-f407.md) |
| 编写 TOML、连接设备与组件 | [应用配置](guides/app-configuration.md)、[I/O 执行](guides/io-execution.md) |
| 为新 PCB 或芯片接入实现 | [Board 接入](guides/board-porting.md)、[Provider 接入](guides/provider-porting.md) |
| 修改平台代码或文档 | [维护流程](development/workflow.md)、[文档维护](development/documentation.md) |

当前生产 SoC 为 STM32F4，板卡、精确料号、模式与限制由[支持参考](reference/platforms/support.md)维护。Native 是显式主机测试模型。软件行为、ARM 链接、SDK 消费和物理 HIL 分别记录；当前物理 HIL 为 `not_executed`。

```{toctree}
:maxdepth: 2

tutorials/index
guides/index
explanation/index
reference/index
development/index
```
