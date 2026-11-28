# F103RB I2C1 目标接收软件参考

HSI8 下的 I2C1 使用 PB6／PB7 默认开漏路线。需要外部上拉和支持时钟拉伸的
标准模式主机，参考上限为 80 kHz。装配选择一个 7 位本机地址和优先级 0 的
事件、错误中断。`irq_latency_us` 是外部时序输入，实板延迟仍需测量。

业务通过强类型 factory 获取 `nx_i2c_target_port_t`，显式准备 RX 缓冲并提交
`NX_I2C_TARGET_RECEIVE`。每次请求有一个 1～256 字节的接收缓冲和有限截止时间。
`service` 在两路 IRQ 与控制器确认撤回后发布 SETTLED。读取缓冲、归还请求或关闭
Platform 前必须等到该状态。

该模式遇到对端读方向、重复地址、溢出和已观测到的 EV2 丢失时结束当前请求，并
保留已确认的前缀。传输格式为单地址接收，使用 STOP 结束。硬件拉伸保持开启。
软件验证覆盖实际 C 路径和链接，ES096 §2.8.1 的中断时序、实际线路和波形仍待硬件验收。

[RM0008 Rev21 §26](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) · [ES096 Rev15 §2.8](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-errata-stmicroelectronics.pdf)
