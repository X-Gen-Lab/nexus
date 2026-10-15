---
id: status.support
type: status
audience: consumer
source_language: zh_CN
---

(status-support)=
# 支持范围与选型

本页帮助确认一种精确装配是否处于维护范围。完整支持说明仍由
[原支持记录](../delivery/support.md)维护；本页只提供判断入口。
[事实派生矩阵](../delivery/capabilities.md)读取 SoC／Board／route、provider
和软件夹具，不能替代 PCB 或物理验收。不要按“芯片属于 Cortex-M”推导全部外设可用。

## 选型必须分别核对五个事实

| 核对项 | 权威来源 | 可得到的结论 |
| --- | --- | --- |
| 芯片能力 | 精确 SoC JSON 与来源 | 该芯片有什么资源；unknown 保持 unknown |
| provider 实现 | 生产代码、resolver 与派生矩阵 | 哪个 controller、route、mode 真正可选 |
| 板级授权 | 精确 Board facts 与来源 | 哪种 PCB、引脚、电气配置获准；不能猜接线 |
| 软件验证 | [精确软件结果](software-verification.md) | 特定 source/config/tool/artifact 范围实际执行 |
| 实板验证 | [物理状态](hardware.md) | 真实 station 对精确 tuple 的实际测量 |

[平台参考入口](../reference/platforms/index.md)连接各芯片的软件切片；
[选择指南](../reference/platforms/support.md)解释 F407、F470、G474、H743、U575
与 CPU-only Runtime 的差异。两个入口都引用原支持记录，不另建能力规则。

## 判定流程

1. 固定完整料号、Board 和 PCB 来源；检查尚未知的晶振、电源和接线事实。
2. 在派生矩阵中找到精确 mode、controller 和 endpoint；未列模式不自动支持。
3. 使用实际 TOML resolver；冲突、unknown 或不支持的配置应当拒绝。
4. 比对软件结果的源码、配置与产物，不能借用相邻板或相邻模式的执行身份。
5. 在声称物理能力前取得真实 station 记录；当前 MCU HIL 均未执行。

平台启动不启动调度器。任务、缓冲区、栈、owner 服务预算、停止及恢复策略
属于外部工程。连续流模式也不承诺任意中断延迟或消费者速度下无丢失。

![CPU Runtime、具体 SoC 和物理资格边界](../assets/diagrams/cortex-m-capabilities.svg)

FIG-13 是软件支持层次图；完整图示适用条件见 [图册](../assets/diagrams/README.md)。
