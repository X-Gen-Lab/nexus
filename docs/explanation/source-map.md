---
id: explanation.source-map
type: explanation
audience: maintainer
source_language: zh_CN
---

# 源码旁的架构与维护合同

各层 README 与实现放在一起，作为该目录的架构和维护入口。本页直接包含这些原文，
不复制第二份合同。站点内的源码链接绑定文档对应的 revision；当前实现、软件执行、
候选与实板资格分别记录，不能通过页面发布自动转移。

Arm CPU 实现按 ISA 分组，MCU 集成按厂家分组。family 标识、公共 include 路径和
CMake target 是各自明确的接口；目录分组表达源码归属，不增加运行时转发层。

历史开发资料保存在[证据归档](../archive/evidence/README.md)，使用原始成员身份和
恢复方法。恢复归档不建立当前源码资格；其中的原始执行范围与失败记录保持不变。

```text
arch/
  include/nexus/arch/         公共 CPU 合同
  arm/cortex_m/              编译期选定的 Arm Cortex-M 实现
  native/                    宿主 CPU 行为模型
drivers/
  <kind>/<ip>/               经逐芯片复核的共享寄存器协议算法
soc/
  common/                    跨厂家协作合同
  native/configure/          Native 配置与静态装配
  native/drivers/            独立宿主设备模型
  stm32/common/              STM32 芯片身份辅助
  stm32/<family>/            精确 STM32 provider
    configure/               family 策略、静态构造与装配
  gd32/<family>/             精确 GD32 provider
    configure/               family 策略、静态构造与装配
tools/configure/
  providers/                 显式 provider map 与公共叶合同
  tests/providers/<family>/  宿主配置测试
io/
  include/nexus/io/           公共 I/O 类型与行为合同
  src/api/                   检查接口并分派
  src/mechanisms/            有界通用 I/O 机制
  private/                   硬件无关校验与内部辅助
components/
  <component>/               通用器件、owner、协议与存储机制
  soc/stm32/<extension>/     显式选入的精确 STM32 扩展
```

## Core

```{include} ../../core/README.md
:relative-docs: ../../core/
:relative-images:
```

## Arch

```{include} ../../arch/README.md
:relative-docs: ../../arch/
:relative-images:
```

## 类型化 I/O

```{include} ../../io/README.md
:relative-docs: ../../io/
:relative-images:
```

## OS 适配

```{include} ../../os/README.md
:relative-docs: ../../os/
:relative-images:
```

## 通用组件

```{include} ../../components/README.md
:relative-docs: ../../components/
:relative-images:
```

## 选定网络与启动合同

[网络组件合同](../../components/network/README.md)维护适配器执行、复制 TCP 字节、
TX 结果观察和退出责任。{ref}`选定 RX payload 借用合同 <lwip-loaned-rx-contract>`
维护可选 copied／loaned profile，以及公共接口与 Storage target 的分界。最后一次
pbuf free 只把 loan 标记为可归还，串行 owner 随后释放精确 MAC loan；存在引用或
释放失败时继续保留存储。这些机制不建立 PHY 接线、DMA／Cache 或实板资格。

[MCUboot 组件合同](../../components/mcuboot/README.md)维护选定 overwrite-only
profile、显式 Flash 组合、boot／application 所有权图及产品恢复责任。
{ref}`公共控制 API <mcuboot-control-contract>`只展示通用 C11 头，具体 Flash port
类型和私有 boot／profile 头不属于该接口。模型与链接结果不建立掉电或实物 Flash 资格。

## 精确 SoC 集成

```{include} ../../soc/README.md
:relative-docs: ../../soc/
:relative-images:
```

## 可复用 IP 算法

```{include} ../../drivers/README.md
:relative-docs: ../../drivers/
:relative-images:
```

精确实现边界与参考配置分别见
[STM32F103](../../soc/stm32/stm32f103/README.md)、
[GD32F303](../../soc/gd32/gd32f303/README.md)、
[STM32F407](../../soc/stm32/stm32f407/README.md)、
[GD32F470](../../soc/gd32/gd32f470/README.md)、
[STM32G474](../../soc/stm32/stm32g474/README.md)、
[STM32H743](../../soc/stm32/stm32h743/README.md)和
[STM32U575](../../soc/stm32/stm32u575/README.md)。站点链接绑定当前文档 revision，
每个 family 的完整退出条件和实际证据仍以对应合同为准。

## Board 包

```{include} ../../boards/README.md
:relative-docs: ../../boards/
:relative-images:
```
