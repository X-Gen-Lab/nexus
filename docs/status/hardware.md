---
id: status.hardware
type: status
audience: reviewer
source_language: zh_CN
---

(status-hardware)=
# HIL 与实板状态

当前没有接入可执行的 MCU 实板、调试探针或串口。F407ZG 启明、F407VE 天空星、
F470ZG 梁山派，以及 G474／H743／U575 来源参考切片的物理状态均为 `not_executed`。
准备工装、成功生成计划或通过 host validator，不是 MCU 上实际完成的实验。

| 待执行范围 | 已有入口 | 尚缺的真实证据 |
| --- | --- | --- |
| 三块首发板与芯片参考配置 | [支持记录](../delivery/support.md)、[通用 HIL 准备](../guides/performance-hil.md) | PCB、探针、供电、晶振、接线与 station admission |
| UART／SPI／ADC／Flash 等实际模式 | [资格合同](../delivery/qualification.md) | IRQ、DMA、线完成、取消排空、过载、掉电原始 traces |
| OS49–OS55 | [OS 原记录](../delivery/os.md)、[逐项 OS 台账](ledgers/os.md) | IRQ→task、寄存器保持、栈峰值、Tick、满载退出、睡眠与异常响应 |
| H743 Cache／DMA、U575 双域 | [SoC 扩展记录](../delivery/soc-extensions-verification.md) | 精确电源／option bytes 与真实硬件执行、隔离和失败观测 |
| cycles、latency、power、WCET | [后续迭代](roadmap.md) | 同一 candidate、仪器、负载与明确预算的实际测量 |

OS 计划保持 `prepared_unbound`，物理状态保持 `not_executed`。真实执行必须先满足
station admission，并提供准确 workload、instrument、raw trace、时间和目标身份。
禁止给 unbound 计划填写 `passed` 来代替 acquisition。

## 首次实板验收顺序

1. 核对具体 PCB、option bytes、电气条件、安全初值与 clock，不猜接线。
2. 将真实 probe、serial 与需要的 waveform／power-cut 仪器绑定到 station。
3. 固定源码、resolved 配置、工具和 candidate ELF；采用实际预算。
4. 执行对应模式的原始采集与失败路径，保留每次失败和重试。
5. 对同一 tuple 做资格判断。没有执行的 controller、mode 或板卡保持未执行。

工具源码入口见 [实现地图](../development/implementation-map.md)。物理 scope
仅在真实站点执行后升级，不能因文档发布、软件 CI 或片上单元存在自动升级。
