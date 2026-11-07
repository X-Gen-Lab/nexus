---
id: assets.diagrams
type: reference
audience: maintainer
source_language: zh_CN
---

# 当前架构图册

图源为可编辑 DOT，输出为本地 SVG；`labels.json` 维护同一拓扑的中英文标签。
实线和虚线以各边标签为准，不能把源码依赖理解为每次 I/O 的转发，也不能由图推导
软件候选、吞吐、低功耗或物理 HIL。当前 MCU 接入只有 F4，精确实现限定 F407VE／ZG。

纯退役芯片图已退出当前图册，原图可从
[固定旧源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/assets/diagrams)
追溯。机器身份与范围见 [diagram-index.csv](diagram-index.csv)，原始执行证据不重标。

修改图源或标签后，使用已准备锁定工具重新生成并验证；渲染检查不代替独立语义审查。

```sh
python -m tools.documentation.diagram_localization --write \
  --tool-prefix /absolute/path/to/prepared-doc-tools
python tools/dev/dev.py docs check --full \
  --tool-prefix /absolute/path/to/prepared-doc-tools
```

## FIG-01 全平台分层与外部边界

平台职责与产品外置；OS 和组件可选，逐边区分构建事实与日常调用。

![FIG-01：全平台分层与外部边界](platform-architecture.svg)

[可编辑 DOT](platform-architecture.dot)。

## FIG-02 选定编译目标依赖

F407ZG 裸机 IRQ UART 的构建依赖；虚线为私有 include 或 OBJECT 注入。

![FIG-02：选定编译目标依赖](build-targets.svg)

[可编辑 DOT](build-targets.dot)。

## FIG-03 配置与原子生成

TOML、硬件事实、typed IR 和原子 bundle；失败不发布可用半成品。

![FIG-03：配置与原子生成](configuration-generation.svg)

[可编辑 DOT](configuration-generation.dot)。

## FIG-04 factory与静态实例

按需静态实例、强类型 factory、共享 ops 与独立 context；查询不启动硬件。

![FIG-04：factory与静态实例](factory-instances.svg)

[可编辑 DOT](factory-instances.dot)。

## FIG-05 显式启动停机与失败撤回

图示显式冷态生命周期与具体 C owner 的清理责任；实际入口以当前 Core／F4 实现为准。

![FIG-05：显式启动停机与失败撤回](platform-lifecycle.svg)

[可编辑 DOT](platform-lifecycle.dot)。

## FIG-06 Core请求状态与借用

请求接纳、借用、排空和最终 SETTLED；取消不直接释放存储。

![FIG-06：Core请求状态与借用](request-lifecycle.svg)

[可编辑 DOT](request-lifecycle.dot)。

## FIG-07 UART与SPI有限DMA时序

选定 F407 有限 DMA 的内存完成、线上完成、撤回与发布顺序。

![FIG-07：UART与SPI有限DMA时序](dma-completion.svg)

[可编辑 DOT](dma-completion.dot)。

## FIG-08 wait wake与外部退出

等待、唤醒提示、执行进度与外部 join；通知不替代真实状态。

![FIG-08：wait wake与外部退出](wait-wake-shutdown.svg)

[可编辑 DOT](wait-wake-shutdown.dot)。

## FIG-10 Source SDK独立消费

所选 Source SDK 的导出、移动与独立消费；主机运行和 ARM 链接分别验证。

![FIG-10：Source SDK独立消费](source-sdk-consumer.svg)

[可编辑 DOT](source-sdk-consumer.dot)。

## FIG-11 软件与HIL证据关系

模型、链接、SDK、软件候选和实板证据分别取得，不自动晋级。

![FIG-11：软件与HIL证据关系](qualification-evidence.svg)

[可编辑 DOT](qualification-evidence.dot)。

## FIG-12 文档SDK与证据发布恢复

交付、验证与恢复流程参考；实际发布和下载需要独立执行记录。

![FIG-12：文档SDK与证据发布恢复](release-recovery.svg)

[可编辑 DOT](release-recovery.dot)。

## FIG-13 Cortex-M能力分层

通用 CPU 原语与 Runtime 独立于当前 F4 SoC；CPU 名称不授予可选能力。

![FIG-13：Cortex-M能力分层](cortex-m-capabilities.svg)

[可编辑 DOT](cortex-m-capabilities.dot)。

## FIG-15 连续RX与可见丢块

选定 F407 连续接收的 producer、块借用、替换和 loss／停机责任。

![FIG-15：连续RX与可见丢块](continuous-stream.svg)

[可编辑 DOT](continuous-stream.dot)。

## FIG-16 当前 I/O 装配与执行

构建期资源计划与运行时操作分开；不引入隐藏调度器或设备注册表。

![FIG-16：当前 I/O 装配与执行](io-evolution.svg)

[可编辑 DOT](io-evolution.dot)。

## FIG-21 Native RTC/LPTIM 装配、借用与重新开放

显式 Native RTC／LPTIM 主机测试；物理 provider 和低功耗不由模型推导。

![FIG-21：Native RTC/LPTIM装配与显式生命周期](rtc-lptim-lifecycle.svg)

[可编辑 DOT](rtc-lptim-lifecycle.dot)。

## FIG-25 UART TX 复制代理的 kernel 所有权与退出

公共复制 UART TX 代理的 owner、真实排空与退出；不授予硬件隔离。

![FIG-25：UART TX 复制代理的 kernel 所有权与退出](user-io-copied-ownership.svg)

[可编辑 DOT](user-io-copied-ownership.dot)。

## FIG-26 I3C 核心借用与整总线证明

公共有界 I3C 核心与借用合同；本轮没有生产 MCU I3C adapter。

![FIG-26：I3C 核心借用与整总线证明](i3c-core-lifecycle.svg)

[可编辑 DOT](i3c-core-lifecycle.dot)。

## FIG-30 F407 I2S2 从机有限发送与整块借用退出

选定 F407 I2S2 slave TX；有限 stereo16 DMA 与外部 CK／WS 前提。

![FIG-30：F407 I2S2 从机有限发送与整块借用退出](f407-i2s2-slave-tx.svg)

[可编辑 DOT](f407-i2s2-slave-tx.dot)。

(fig-31-driver-source-ownership)=

## FIG-31 F4 源码归属与接口边界

F4 私有实现、官方库、公共 I/O 与测试支撑的源码及调用边界。

![FIG-31：F4 源码归属与接口边界](driver-source-ownership.svg)

[可编辑 DOT](driver-source-ownership.dot)。

## FIG-32 lwIP/TCP/TLS/MQTT 软件主机互通参考

主机 TCP／TLS／MQTT 互操作；不证明 MCU、SDK 或物理联网能力。

![FIG-32：lwIP/TCP/TLS/MQTT 软件主机互通参考](lwip-tcp-tls-mqtt-host.svg)

[可编辑 DOT](lwip-tcp-tls-mqtt-host.dot)。

未注册的补充图保留自己的图内范围，仍参加成对输出和双语检查。当前结构的正文
以[架构](../../delivery/architecture.md)和[源码定位](../../explanation/source-map.md)为准。
