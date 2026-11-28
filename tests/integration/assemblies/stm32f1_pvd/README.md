# NUCLEO-F103RB 供电比较器轮询

该装配验证工厂取得、平台启动、读取和关闭。`threshold_code` 是 STM32F103 的原始 PLS 编码，取值 0～7。`nx_exti_port_level` 返回 PVDO。`true` 表示当前硬件比较器报告 VDD／VDDA 处于较低状态。翻转电压有方向相关迟滞和器件容差，查阅 DS5319 Rev20 表 11。[STM32F103RB 数据手册](https://www.st.com/resource/en/datasheet/stm32f103rb.pdf)

驱动只取得 PWR 时钟和比较器。已有 EXTI16 或 NVIC 状态会拒绝取得。关闭恢复原 PLS 并释放时钟。没有配置 GPIO、IRQ、事件队列或 WFE 唤醒。读取验证原始数字状态，翻转电压和响应时间尚未实测。[RM0008 §5.2.2](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) · [ES096 §2.2.8](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
