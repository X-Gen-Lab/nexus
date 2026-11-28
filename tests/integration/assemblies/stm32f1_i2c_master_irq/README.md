# STM32F103RB I2C1 中断主机写入

F103RB 的 PB6/PB7 默认路线使用 HSI8 和标称 80 kHz。装配固定
`irq-write-messages`，只接受 7 位地址与写消息。每个消息开始时发送
START 或重复 START。字节经 BTF 确认后计入完成前缀。

请求到期或 START 失败后，由前台确认本地 SWRST、外设中断源及两个 NVIC
向量撤回。即使对端仍拉低总线，已确认撤回的本地请求也可归还缓冲。
重新提交前须显式调用 `recover`，由应用确认独占主机、对端状态和总线电平。
驱动无需改动引脚模式或生成 GPIO 清除脉冲。此装配面向支持时钟拉伸的合作对端。

夹具检查强类型 endpoint、请求、取消和停机的 ARM 链接。寄存器模型覆盖重复
START、NACK、仲裁丢失、START/STOP 故障和隔离。实际合格的主机时序和实板
线路状态待测量。读消息、10 位地址和完整 SMBus 设备访问需要其他已审核模式。

[RM0008 Rev21 §26](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) · [ES096 Rev15 §2.8](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-errata-stmicroelectronics.pdf)
