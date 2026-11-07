# SoC 芯片集成与私有实现

本层负责精确芯片的启动、时钟、时间域、内存、IRQ／DMA，以及公共 I/O 的具体实现。
MCU 源码按厂家和已维护系列组织；`stm32f407`、`gd32f470` 等公共系列标识保持独立，
不可变 family descriptor 给出唯一源码目录。目录层级不新增配置权威或运行时注册表。

## 目录与阅读入口

```text
soc/
  common/                     跨厂家 SoC 协作合同
  native/
    configure/                Native 能力策略与静态装配
    drivers/                  独立主机设备模型
  stm32/
    common/                   STM32 身份读取辅助
    stm32f103/  stm32f407/  stm32g474/  stm32h743/  stm32u575/
  gd32/
    gd32f303/  gd32f470/
```

| 实现 | 精确范围入口 |
| --- | --- |
| [STM32F103](stm32/stm32f103/README.md) | F103RBT6 的所选模式 |
| [STM32F407](stm32/stm32f407/README.md) | F407VET6／ZGT6；密度与封装分别验证 |
| [STM32G474](stm32/stm32g474/README.md) | G474RET6 参考与独立模式 |
| [STM32H743](stm32/stm32h743/README.md) | H743ZIT6 固定 clock／Cache／revision 切片 |
| [STM32U575](stm32/stm32u575/README.md) | U575ZIT6Q 单状态及独立双域参考 |
| [GD32F303](gd32/gd32f303/README.md) | GD32F303RCT6 精确封装参考 |
| [GD32F470](gd32/gd32f470/README.md) | GD32F470ZGT6 精确实现 |
| [Native](native/README.md) | 主机行为模型，不建立 MCU 资格 |

## 事实、装配与硬件行为

各 MCU family 拥有 `soc.json`、`routes.json`、私有 controller 状态、选定驱动源码
和明确的 CMake target。其 `configure/` 负责系列能力、准入、静态构造描述及生命周期
调用顺序。配置测试仍位于 `tools/configure/tests/providers/<family>/`；共享解析、
不可变 IR、资源仲裁与 bundle 事务由 `tools/configure/` 维护。

实际硬件算法直接由本 family 的 C 代码持有。启动、clock、timebase、memory 和
power 也留在本层。可复用 IP 算法进入 [drivers](../drivers/README.md)，但每个 wrapper
仍须检查真实厂商布局、bit、clock、IRQ／DMA 和 errata；外设同名不证明硬件等价。

![FIG-31：共享 IP 与精确 SoC 源码归属](../docs/assets/diagrams/driver-source-ownership.svg)

图表示源码和维护责任，不增加运行时转发层。`common/uart_control.h` 是跨厂家协作
合同；`stm32/common/stm32_identity.h` 是 STM32 身份辅助，两者都不是通用外设算法。

## 接入和验证边界

[Board 包](../boards/README.md)授权已审 PCB 绑定。新增 Board 不会自动开放新时钟树；
须选择精确已维护 clock profile，或实现并验证新的 profile。未维护的振荡源、domain、
mode 和 silicon revision 必须明确拒绝。

接入顺序见 [provider 指南](../docs/guides/provider-porting.md)，可选路线和模式见
[派生能力矩阵](../docs/delivery/capabilities.md)，实际证据见
[资格说明](../docs/reference/platforms/qualification.md)。CPU 支持、SDK 宏或目录存在
均不建立整系列支持；MCU 实板 HIL 仍为 `not_executed`。
