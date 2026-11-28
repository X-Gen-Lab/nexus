# STM32F105/F107 USB OTG FS Device

两个软件参考装配选择裸机 Device、EP0 IN/OUT 和已审核的 25 MHz 晶体时钟。
应用通过 typed factory 获得配置过的 Device 和双向控制端点。适配复用
`soc/stm32/common/dwc_otg_device` 的生产 DWC 机制，以同一套设备、
端点和请求合同解释排队、SETUP、地址生效及关闭。

| 接线 | 接入要求 |
| --- | --- |
| PA9 | 固定 B-device 板的实际 VBUS 感知，GPIO 浮空输入 |
| PA11、PA12 | 分别为 DM、DP，由嵌入式 PHY 取得，不设置数字 AF |
| RCC | 25 MHz 参考、HCLK72、PLL48，OTG FS 取得 AHB gate bit 12 |
| NVIC | 只选择 OTG_FS 全局 IRQ 67 |

装配需要显式确认板端接线。驱动检查 live RCC 和设备态，先确认软断开，
再让共享 DWC 初始化。VBUS 无效时停止在软断开，返回状态错误并由 Platform
收回已取得的资源。有效时启动控制端点。停止先收回端点、IRQ 和 FIFO，再关闭
PHY、AHB gate 和已取得的 GPIO gate。ES022 的 DCFG 读延迟由共享机制处理。

本批范围为 PIO Device 与 EP0。非零端点、远程唤醒、DMA、Host 和 OTG 角色
需要各自验收。电气连接、VBUS 门限及实板枚举尚无物理验证结果。

[RM0008 Rev21 表 30 和第 28 章](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) ·
[ES022 Rev11 §§2.13.4–2.13.5](https://www.st.com/resource/en/errata_sheet/es022-stm32f105xx-stm32f107xx-device-errata-stmicroelectronics.pdf)
