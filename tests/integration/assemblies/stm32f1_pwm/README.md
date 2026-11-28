# STM32F103RB TIM3 PWM 软件参考

本装配选择 HSI8、TIM3 CH1 和 PA6。PA6 在 NUCLEO-F103RB 的
Morpho CN10-13 与 Arduino D12 可访问。PSC 为 7，ARR 为 999，
初始 CCR1 为 250，对应 1 kHz 名义周期和 25% 初始占空比。
停止后 PA6 保持主动低电平，归还时恢复该引脚进入装配前的状态。

消费者用强类型 factory 获取 PWM，并链接启动、占空比预装载、状态查询和
停止。模型检查正常更新、0%／100%、有界更新确认及失败后的寄存器组归还。
硬件输出、负载和实际电气时序保留 `not_executed`。
