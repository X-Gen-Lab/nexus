# 精确芯片扩展组件

本目录维护依赖特定芯片、硅片修订、时钟或安全状态的可选组件。它们复用通用 owner、
request、clock 与 typed I/O，但不能直接推广到整个家族。组件存在不等于新增完整
SoC／Board provider，也不建立实板资格。

[FIG-23 有界软件机制的所有权](../../docs/assets/diagrams/io-extension-ownership.svg)
展示其中映射存储、AMP、镜像交接等机制的责任边界；图中所选范围不包含本目录全部
后续扩展，也不表示任意模式已经接入或完成实板验证。

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
| F407 I2S2 slave TX | [合同](stm32/f407_i2s2_slave_tx/README.md)；有限 stereo16、外部 CK／WS、DMA 排空 |
| F407 image handoff | [合同](stm32/stm32f407_handoff/README.md)；已停止裸机平台、掩码 MSP 交接 |
| H743 W25Q128JV | [合同](stm32/stm32h743_w25q128/README.md)；单线 Flash、CPU read mapping、显式 MPU 防护 |
| H743 DMA2D frame | [公共头](stm32/stm32h743_frame_dma2d/include/nexus/components/frame_dma2d.h)；精确内存加速，不能当显示控制器 |
| H747 AMP | [合同](stm32/stm32h747_amp/README.md)；外部 paired boot、HSEM、显式共享 SRAM4 |
| H563 I3C port | [私有端口](stm32/stm32h563_i3c/private/stm32h563_i3c_port.h) 与 [CMake](stm32/stm32h563_i3c/CMakeLists.txt)；有限轮询软件参考，不是内置 H563 Board |
| U575 retained time | [公共头](stm32/stm32u575_retained_time/include/nexus/components/stm32u575_retained_time.h)；独立 RTC 时间域，外部 boot 预备 |
| U575 LPDMA copy | [公共头](stm32/stm32u575_lpdma_copy/include/nexus/components/stm32u575_lpdma_copy.h)；精确 SRAM4 receiver，与 Stop2 复合退出 |
| U575 LPTIM1 | [合同](stm32/stm32u575_lpdma_lptim1/README.md)；一组固定 tuple 与有限 write-OK 证明 |
| U575 Stop2 | [合同](stm32/stm32u575_stop2/README.md)；WUT、copy、LPTIM1 变体明确互斥 |

## 验证与产品责任

Host 模型、ARM 链接、移位 SDK、完整软件候选和实板各自记录源码与实际范围。
不要从正常链接推导 physical DMA／Cache coherence、跨核 reset join、唤醒时延或
掉电恢复。完整接入／软件与硬件状态以[平台支持](../../docs/delivery/support.md)及
[交付记录](../../docs/delivery/README.md)为准，默认不授权破坏性硬件操作。
