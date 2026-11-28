# STM32F103RB 双 USART IRQ 软件参考

NUCLEO-F103RB 装配同时选择 USART1 和 USART2。USART1 使用 APB2、IRQ37、
PA9/D8 发送与 PA10/D2 接收，外接 3.3 V 串口。USART2 使用 APB1、
IRQ38 和 PA2/PA3，并可连接板上的 ST-LINK VCP。该参考固定 HSI8，两个
端口使用独立的接收存储、IRQ 和时钟门控。

两个端口复用同一字节 IRQ 引擎。USART1 采用 8N1，可配置字节或事件接收；
本次不提供它的 DMA、9 位和线路协议。USART1 默认路由只读取自身 AFIO
重映射位。停止一方保留另一方的请求与 GPIO 供电，平台关闭时按相反顺序归还
GPIO 配置与时钟。实板连线、速率误差和时序仍需验证。

## USART3 软件接线参考

`f103rb-usart3.toml` 使用同一测试程序运行 USART3 与 USART2。USART3 由 APB1
驱动，IRQ39，PB10 为 TX、PB11 为 RX，使用默认 AFIO 路由和外部 3.3 V 串口。
USART3 独立取得 GPIOB，停止后恢复原 pin 配置。USART2 继续持有独立的
GPIOA、IRQ38 和 APB1 门控。软件用例选择 HSI8 和 8N1。

选定的是异步字节 IRQ 模式。ES096 §2.3.5 中的 USART3 同步 CK 涉及 PB12 与
SPI2 NSS，本装配不配置该引脚和线路模式。外部串口电气连接、波特率误差及
实际接收仍待硬件验证。
