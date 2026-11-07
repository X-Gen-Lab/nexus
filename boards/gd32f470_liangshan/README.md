# 梁山派 GD32F470ZGT6 板级资源包

本包通过 `board.json` 引用 [GD32F470](../../soc/gd32/gd32f470/README.md) 的精确
芯片事实和已审路线。默认声明资料已复核的 PD7 LED、USART0 PA9/PA10、SPI4
PF6/PF7/PF8/PF9、内部 Flash 和独立看门狗；不创建后台任务或外接器件实例。

## 时钟、内存与安全初值

25 MHz HXTAL／200 MHz 固定时钟采用名义 3.3 V 条件，APB1 为 50 MHz、APB2 为
100 MHz。TIMER1 专属 1 MHz 单调时间域，基本时钟启动不提供 USB 48 MHz。

物理 Flash 是完整 1 MiB，不预留产品分区。默认数据与栈只使用连续 192 KiB SRAM；
附加 SRAM／TCM 不默认分配，也不据此声明 DMA 可达。

GPIO 先预置初始电平再启用输出：PD7 初始低，PF6 CS 初始高。外部 NOR 的实际
型号未知，不会自动成为产品存储。

## 接线与验收边界

SoC 的 I2C0、TIMER2 PWM、ADC0、EXTI3 可由明确的外部资源包维护接线，不属于
当前梁山派默认绑定。RS485 不默认选择外部收发器。

实际 PCB 修订、晶振、电压、接线、探针和串口尚未实物观察；启动、时钟精度、电气、
IRQ、Flash 掉电与长期负载的实板验收均未执行。
