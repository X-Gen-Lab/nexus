# GD32F470ZGT6 梁山派资源包

下一代输入为 `board.json`，只引用 `soc/gd32f470` 的精确芯片事实与
reviewed routes。默认只声明资料已复核的 PD7 LED、USART0 PA9/PA10、
SPI4 PF6/PF7/PF8/PF9、内部 Flash 和独立看门狗，不创建 worker 或器件实例。

25 MHz HXTAL / 200 MHz clock profile 使用名义 3.3 V 条件，APB1 50 MHz、
APB2 100 MHz。TIMER1 专属 1 MHz monotonic 时间，不提供 USB 48 MHz 时钟。
物理 Flash 为完整 1 MiB，没有平台预留产品分区；默认数据和栈只使用连续
192 KiB SRAM。附加 SRAM 和 TCM 不默认分配，也不声称 DMA 可达。

GPIO 输出先预置安全电平，再启用模式：PD7 初始低；PF6 CS 初始高。
SPI 外部 NOR 的实际型号未知；此包不把其自动配置为产品存储。

SoC I2C0、TIMER2 PWM、ADC0、EXTI3 可以分别通过明确外部资源包维护接线，
它们不属于当前梁山派默认 Board bindings。RS485 也不默认配置外部收发器。

实际 PCB revision、晶振、电压、接线、探针和串口尚未实物观察；所有启动、
时钟精度、电气、IRQ、Flash 掉电和长期负载资格均为未执行。
