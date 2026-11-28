# STM32F103RB TIM3 PWM 软件参考

本装配选择 HSI8、TIM3 CH1 和 PA6。PA6 在 NUCLEO-F103RB 的
Morpho CN10-13 与 Arduino D12 可访问。PSC 为 7，ARR 为 999，
初始 CCR1 为 250，对应 1 kHz 名义周期和 25% 初始占空比。
停止后 PA6 保持主动低电平，归还时恢复该引脚进入装配前的状态。

消费者用强类型 factory 获取 PWM，并链接启动、占空比预装载、状态查询和
停止。模型检查正常更新、0%／100%、有界更新确认及失败后的寄存器组归还。
硬件输出、负载和实际电气时序保留 `not_executed`。

## STM32F100RB Value Line

`f100rb-pwm.toml` 同时选择 TIM15 CH1／PA2、TIM16 CH1／PB8、
TIM17 CH1／PB9。采用 RM0041 表 39–41 的默认 MAPR2 路线和 HSI8／APB2
时钟。各实例拥有独立时基、预装载和 MOE。停止时引脚回到低电平，
随后关闭输出使能并停止计数器。关闭装配时按逆序恢复资源。

此装配使用三个普通单路 PWM。互补输出、硬件刹车、同步、捕获、DMA
和 HDMI-CEC 需独立配置。PA2 与 USART2 TX 冲突。示例与测试均按软件
参考交付，实板波形和负载尚未验证。

[ST RM0041 Rev6](https://www.st.com/resource/en/reference_manual/rm0041-stm32f100xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
