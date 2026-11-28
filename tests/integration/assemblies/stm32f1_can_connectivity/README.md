# STM32F105/F107 双 CAN 软件参考

这两个裸机装配在 F105VCT6 和 F107VCT6 上分别选择 CAN1 与 CAN2 内部静默回环。
装配使用 HSI8，CAN PCLK 为 8 MHz，名义位率 500 kbit/s。CAN2 通过 CAN1
访问 28 个共享过滤器，示例在 bank 14 分割。CAN1 将 bank 0 分到 FIFO0，
CAN2 将 bank 14 分到 FIFO1。应用由 factory 获得两路端口，停掉 CAN1 后，
CAN2 仍保持其时钟、独立中断和过滤器。Platform 在两路停止后才释放共享主域。

一套 bxCAN C 机制服务 STM32F1 Connectivity 与 STM32F4，精确启动向量、
时钟和板级准入由各自的 SoC adapter 提供。当前支持边界为任务收发与 IRQ
观察时间戳。ES022 §2.12.1 要求关闭 TTCM。两个装配都没有外部 pad、
收发器或总线时钟容差资格，物理验证待执行。

[RM0008 Rev21 §§24.2、24.3、24.6、24.9.4](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) ·
[ES022 Rev11 §2.12.1](https://www.st.com/resource/en/errata_sheet/es022-stm32f105xx-stm32f107xx-device-errata-stmicroelectronics.pdf)
