# 启明欣欣 STM32F407ZG V3.1 接线参考

精确型号为 **STM32F407ZGT6**，LQFP144、内部 Flash 1 MiB。HSE 为 8 MHz，维护的
固定时钟为 168 MHz。`V3.1` 标识用户提供的手册，不是已验证的实物 PCB 修订；
检查前 `physical_pcb_revision` 保持 `null`。

## 已审接线

只读 `board.json` 声明 PE3、PE4、PG9 为低有效 LED，各绑定先写入高电平非活动位，
再设置输出模式。USART1 为 PA9/PA10 AF7，MCU UART 是 3.3 V 逻辑。

板载 RS232 连接器经过 MAX232/P14 跳线路径，须使用匹配的 RS232 适配器，不能把它
当作 TTL 串口。实际连接器与跳线选择属于工装记录。PA13/PA14 保留给 SWD。

来源包含原 V3.1 手册，以及 LED、HSE、UART 示例仓库的精确 Git tree/blob 身份。
[芯片实现](../../soc/stm32/stm32f4/README.md)与后来独立的核心板／V4.1 包分别维护，
本包不因新包存在而改变接线。

## 未选择与未验证范围

SPI／I2C／RS485、外部 Flash 实装、PWM、ADC 和 EXTI 外部接线没有由本包选择。
不承诺工业执行器输出或产品存储安全。实板、启动、UART 波形、复位电平及负载行为
均未执行测试；软件链接和模型证据只建立对应软件范围。

## 软件装配

裸机／FreeRTOS 与空／GPIO／UART 参考见[中央参考目录](../../tools/configure/assemblies/README.md)。
装配、晶振和 backend 都显式选择；参考不默认启动应用或建立物理资格。

`board.json` 另保留 CAN1 PA11/PA12、CAN2 PB12/PB13 软件 pad 路线。
收发器实装、终端和跳线尚未确认，不能据此声明物理 CAN 支持。
