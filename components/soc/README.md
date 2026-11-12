# 精确芯片扩展组件

本目录维护依赖特定芯片、硅片修订、时钟或安全状态的可选组件。它们复用通用 owner、
request、clock 与 typed I/O，但不能直接推广到整个家族。组件存在不等于新增完整
SoC／Board provider，也不建立实板资格。

[FIG-31 F4 源码归属](../../docs/assets/diagrams/driver-source-ownership.svg)说明
当前 F4 provider、公共接口与官方库边界。可选组件遵守各自的所有权合同，链接一项
组件不会授予其他型号、模式或物理资格。

## 架构与职责

通用算法放在 `components/<mechanism>/`，芯片适配放在这里；实际时钟、引脚、IRQ、
DMA、内存和启动仍由精确 SoC／外部 boot owner 持有。CMake 按实际 family、part、
backend 和选择条件创建目标，未显式链接的实现不进入镜像。公共头仅描述窄端口和
调用者状态；厂商头、寄存器和 transport 依赖保持私有。

从各目录 README／公共头进入，先核对精确范围，再审查 prepare、submit、service、
stop／recover、borrow 与外部 join。ClockDomain 只能证明已登记消费者，不会发现
未登记 masters；本核 IRQ mask 也不保护另一核或安全域。

## 当前 STM32 扩展导航

| 扩展 | 入口与范围 |
| --- | --- |
| F407 音频 | [标准 audio 合同](../../docs/design/f407-i2s2-slave-tx.md)；静态 factory、Platform 生命周期与有限帧借用 |
| F407 image handoff | [合同](stm32/stm32f407_handoff/README.md)；已停止裸机平台、掩码 MSP 交接 |

当前目录只保留上述 F407 组件。H743／H747／H563／U575 的历史扩展已退出当前生产
范围，原始实现与记录可从
[固定重构前源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/components/soc)
追溯。历史链接不成为当前 CMake 目标或支持声明。

## 验证与产品责任

Host 模型、ARM 链接、移位 SDK、完整软件候选和实板各自记录源码与实际范围。
不要从正常链接推导 physical DMA／Cache coherence、跨核 reset join、唤醒时延或
掉电恢复。完整接入／软件与硬件状态以[平台支持](../../docs/delivery/support.md)及
[交付记录](../../docs/delivery/README.md)为准，默认不授权破坏性硬件操作。
