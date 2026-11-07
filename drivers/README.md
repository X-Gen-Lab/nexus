# Drivers：可复用硬件 IP 实现

本目录按控制器类别和受审 IP 协议组织私有算法。精确 SoC 目标用自身 ABI
和私有 include 路径直接编译所需源码。公共接口对象／请求所有权归 I/O，
芯片时钟、路线、启动、内存和具体实现状态归 SoC。这是源码归属边界，
不增加运行时层、注册表、分配、队列或工作线程。

![FIG-31：共享 IP 与精确 SoC 源码归属](../docs/assets/diagrams/driver-source-ownership.svg)

图说明当前维护判定和编译责任，不表示每次 I/O 都经过额外转发。
图源与范围见[图册](../docs/assets/diagrams/README.md)。

## 已维护引擎

| 目录 | 算法 |
| --- | --- |
| `uart/uart_v1_channel_dma/` | 旧式 USART 与固定通道 DMA |
| `i2c/i2c_v1_async/` | 受审 V1 IRQ／DMA 消息顺序 |
| `i2c/i2c_v2/` | 受审 V2 轮询消息顺序 |
| `adc/legacy_adc/` | 旧式单次转换 ADC |
| `timer/legacy_pwm/` | 旧式固定 PWM |
| `flash/legacy_flash/` | 旧式半字 Flash 编程 |
| `can/classic_can/` | 经典邮箱与共享过滤器域 |
| `usb/fs_device_pio/` | FS 设备端点／FIFO PIO |
| `ethernet/legacy_mac/` | 普通和选定增强链式描述符 |
| `timer/advanced_timer/` | 捕获、互补 PWM／刹车模板 |
| `uart/usart_fifo/` | 有界字节 FIFO 与选定 RTS／CTS |
| `gpio/owned_pin_state/` | 自有位掩码下的两态 GPIO 迁移 |

`Nexus::DriverPrivateHeaders` 只提供私有构建期 include 路径，消费者私有
链接；不包含固件源码、对象或状态。精确 SoC 是唯一源码选择器。
公共检查分派、数据帧验证、序列所有权、日历运算
仍在 I/O。CAN 的 `cortex_m_port.h` 私有维护字节优先级／字访问 NVIC 访问；
Native 在包含前替换同一宏，不增加公共 Arch API、不改中断排序。
H743／U575 共用四字节不可变 `uart_fifo_profile.h` 布局，SoC 别名保留精确
入口类型，不增加运行时链接。

## 共享条件与实例隔离

共享依据是硬件协议和受审硅片语义，不是厂家或寄存器名字相同。SoC
适配层对照实际 SDK 寄存器偏移与位掩码，提供时钟、DMA／IRQ、可见内存，
并绑定选定静态状态。它们不因此继承另一芯片的路由、引脚接线、时钟或
勘误策略。

I2C V1 由 F407、GD470、F103、GD303 共用。前两者保留受审 100 kHz，8 MHz 低端
两款因 ES096 2.8.1／2.8.5 和 GD32F30x 限制 2.8.1，使用名义 80 kHz、
优先级为零且不调用内核的 IRQ，无 I2C DMA。F4 流式 DMA 布局不复用于固定通道
控制器。已捕获唤醒发布者等待退出后才停止并归还通知存储；
向量撤销拒绝时保留已初始化责任者，待显式重试。

UART V1 通道 DMA 为精确 F103／GD303 共享，适配层独立断言旧式 USART 和
固定通道 DMA 位掩码与布局。`dma-tx` 占一 TX 通道和显式 IRQ 字节环；
`dma-duplex` 占两通道与调用者提供有限 RX 块。线上 TC 与 DMA
内存完成分开；引擎撤每个请求源、观察稳定剩余计数、确认标志后
才归还缓冲区。无法证明撤销时保留请求／生产者借用。普通 RX 由
责任者推进服务重启，具有明确间隙／背压，不覆盖消费方块。有限
边界丢弃 DATA 字节时记录丢失，不据此提供循环 RX 或 Cache 合同。

仅硬件协议一致时共享逐字节操作；冷态验证、NVIC 撤销和芯片
资源仍归适配层。协议／勘误不同须新受审实现或精确窄能力，不能在
热路径加入推测家族分支。STM32 身份访问位于
`soc/stm32/common/stm32_identity.h`，跨厂商 SoC 控制合同位于
`soc/common/uart_control.h`。

## 接入与验证

CMake 是唯一源码图，未选算法不能因文件存在而编译。检查选定源码的真实
链接保留情况和 Flash／RAM 成本。寄存器边界模型执行真实引擎与适配层，
覆盖拒绝、错误、取消、失败排空、实例隔离；每个选定
ARM 实现分别编译。Native 模型与 ARM 链接建立不同软件属性，
时序／电气必须物理执行。

维护入口见[执行预算](../docs/design/soc-execution-budgets.md)与
[provider 接入指南](../docs/guides/provider-porting.md)。
