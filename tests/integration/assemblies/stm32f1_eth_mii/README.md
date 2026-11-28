# STM32F107 MB784 C.1 以太网软件参考

装配固定为 STM32F107VCT6、外部 25 MHz HSE、72 MHz HCLK 和裸机。DP83848CVV
使用独立 25 MHz X1，JP2 开路、JP3 接 2–3、JP4 接 1–2。JP11–13 接 2–3，
JP14 接 1–2。MII 使用 PD8–PD12 的已核对重映射路径，PC1 和 PA2 供 PHY 管理。

RMII 选择 `f107vc-rmii.toml`。装配要求实板安装默认未装的 50 MHz U2
（SM7745HEV-50.0M），JP2 闭合、JP3 接 1–2、JP4 开路，JP11–13 接 2–3。
U2 为 DP83848CVV X1 和 MCU PA1 提供公共参考，MCU 仍以独立的 X3 25 MHz
建立 72 MHz HCLK。PD8 接 PHY pin 39 的同步 RX_DV，PD9／PD10 接 RXD0／1。
MCU 的 RMII_CRS_DV 在该接线下只按全双工接收数据有效信号使用。

应用从强类型 factory 取得同一个控制器的 Ethernet 和 MDIO 端口。以太网使用
普通四字描述符、全双工 10/100 Mb/s 和 TX 存储转发。应用负责 PHY 地址、外部接线
和协商结果。链路模式变化必须等当前会话关闭后重新启动。

停止时等待 IRQ、TX 和 RX 借用。控制器以复位后的描述符表地址确认撤回，失败时
保留原有存储。驱动持有 DMA2 时钟直至 MAC 彻底关闭。这落实了 ES022 的
TX FIFO 与睡眠限制。软件参考只构建，板上走线、实际帧与停止时序未执行。

[UM0600 Rev 5，第 17、35 页](https://www.tme.eu/Document/aece8c709b198489a4f26cf104594d1a/CD00212441.pdf) ·
[RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) ·
[ES022](https://www.st.com/resource/en/errata_sheet/es022-stm32f105xx-and-stm32f107xx-device-errata-stmicroelectronics.pdf) ·
[DP83848C 数据手册，§4.4、§4.5 和 §5.9](https://www.ti.com/lit/ds/symlink/dp83848c.pdf)
