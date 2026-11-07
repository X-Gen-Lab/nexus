# 精确料号 I/O 正向选择夹具

这些开发夹具把已审正向路线纳入维护能力表与既有
`providers-multi_controller_assembly` 软件阶段。每份平坦 TOML 是明确 ARM 输入，
共享 C11 main 启动 Platform，通过 volatile 指针读各 face，使 GC 后仍保留真实方法表
和初始化方法。provider checker 核对 compiler 输入、选择、CPU ABI、Flash／RAM
预算和 post-link retention；链接不执行操作、不证明物理行为。

最初十一组保留 F103RB、F407ZG、G474RE、GD303RC、GD470ZG、H743ZI、U575ZI 的
29 项历史正向选择，软件 Board 来源仍绑定原 revision。当前 TOML 使用本地软件
Board，`f407-output.toml` 另选已审 ZG GPIOB PB2。历史收条不资格化当前源码。

五组 F407VE 另覆盖十四项正向选择：

| 装配 | 选择 |
| --- | --- |
| `ve-word-adc-can.toml` | USART1 字传输 IRQ、ADC1 连续触发 DMA、CAN1／CAN2 内部回环 |
| `ve-rx-idle.toml` | USART1 空闲检测 RX DMA、EXTI0、GPIOB／GPIOE 输入 |
| `ve-rx-continuous.toml` | USART1 连续 RX DMA、GPIOA 输入、GPIOE 输出 |
| `ve-duplex.toml` | USART1 有限双工 DMA、GPIOA 输出 |
| `ve-duplex-continuous.toml` | USART1 continuous duplex DMA |

USART1 模式互斥。ADC PA0／PA1 与 EXTI0／GPIOA 用户分开，DMA 和 CAN pad 由
生产资源图准入；G474 保留 GPIOF／G 模式继续拒绝，F407VE 无已审 GPIOG／Ethernet
精确路线也继续拒绝。

全部选 baremetal、显式 clock、Os、2 KiB MSP、libc heap 0；F103 static RAM
限 16 KiB，其他 32 KiB，Flash 全部 64 KiB，均对实际 ELF 检查。软件 Board 不声明
PCB、焊接器件、电压、pull、晶振实测或外接线，不是物理 Board，不能建立 HIL、全
系列、吞吐、IRQ latency 或产品资格。固件 C11，host 选择回归用 stdlib unittest。

用明确源码、ARM 工具和 TOML 运行普通 provider checker；正式计划在既有阶段枚举
所有维护 fixture，不另建 discovery 或 Source SDK 消费者清单。
