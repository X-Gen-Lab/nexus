---
id: explanation.source-map
type: explanation
audience: maintainer
source_language: zh_CN
---

# 当前源码与维护边界

本页定位当前实现，不复制各目录的完整合同。生产 MCU 入口只有 STM32F4，已迁入
F407VE／ZG；其他 SDK 原位保留。目录、实现、执行记录和物理资格分别说明。

```text
core/                         结果、时间、请求与整平台冷态协调
arch/arm/cortex_m/            编译期 CPU 原语
arch/native/                  主机测试 CPU 端口
io/                           公共接口与硬件无关机制
soc/stm32/stm32f4/             F4 系列唯一生产 provider
  drivers/ip/                 仍被 F4 使用的共享硬件实现
  private/                    厂商类型、Handle 与私有状态
boards/                       四个现有 F407 板卡包
components/                   可选器件、协议和公共机制
tools/configure/              当前集中主机配置工具
tests/support/native/         显式主机测试模型与装配
vendors/                      全部锁定来源；按实际选择消费
```

| 维护问题 | 权威入口 |
|---|---|
| 请求、deadline 与借用 | [Core](../../core/README.md) |
| CPU 原语与上下文 | [Arch](../../arch/README.md) |
| 公共 face、ops 与通用机制 | [I/O](../../io/README.md) |
| F4 启动、时钟和外设实现 | [F4 支持包](../../soc/stm32/stm32f4/README.md) |
| PCB 接线与电气条件 | [Board](../../boards/README.md) |
| 等待、任务和内核责任 | [OS](../../os/README.md) |
| 外部器件与可选机制 | [组件](../../components/README.md) |
| 配置与构建 | [工具](../../tools/README.md)、[CMake](../../cmake/README.md) |
| 模型、链接与执行证据 | [测试](../../tests/README.md)、[交付边界](../delivery/README.md) |

顶层共享 MCU `drivers/` 已归入 F4，不增加运行时转发。ILI9341、XPT2046、W25
继续通过公共 I/O 访问设备，留在组件目录，不随 MCU 支持包迁移。

Board 和 SoC 不再加载本地 Python 插件。默认 Python 工具集中校验事实并生成静态连接；
Core `platform_lifecycle.c` 维护整平台启动、停止、回滚与诊断，F4
`drivers/platform_coordinator.c` 维护 System 与共享资源的 C 协调。Host C11
工具是可选主机试点，不是固件 C 化或交付前置。实际生成 factory、模型与 ARM 产物
各自验证，不能把源码移动视为整条链路已经验收。
Native 必须显式启用测试 scope，不进入生产系列目录或能力聚合表。

精确支持见[支持参考](../reference/platforms/support.md)。重构前的目录与执行范围可从
[固定旧源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
追溯；旧记录保留原始身份，不建立本次重构的软件候选或实板资格。
