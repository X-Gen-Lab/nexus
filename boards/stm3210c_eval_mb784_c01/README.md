# STM3210C-EVAL MB784 C.1

本参考选用 STM32F107VCT6 和板上的 DP83848CVV，提供 Clause 22 PHY 管理与可选 CN6 RS-232。
PC1 为 MDC，PA2 为 MDIO。25 MHz 系统晶振使用 X3。PHY 在 MII 档使用独立
25 MHz 晶振 X1，软件装配无需在 PA8 输出 MCO。

| 板级选择 | 装配要求 |
| --- | --- |
| JP2、JP3 | JP2 开路，JP3 接 2–3 |
| JP4 | 接 1–2，选用已安装的 X1 |
| JP11、JP12、JP13 | 接 2–3，保留 PHY 的重映射接线 |
| JP14 | 接 1–2，PA0 连接 MII CRS |

CN6 使用 USART2 重映射，PD5 发送、PD6 接收。将 JP16 置于 RS-232 档，JP18 和 JP19 保持开路。
TOML 通过 `usart2_wiring = "cn6-rs232"` 确认此接线，PA2 继续用于 MDIO。

独立装配使用固定的裸机 HSE25/PLL72 时钟，并由强类型 factory 取得 PHY 管理端口。
管理模式保留已接到 PHY 的其他 MII 引脚，不配置 DMA 和数据端口。实际板卡修订、跳线、
PHY 地址、MDC/MDIO 波形和串口时序需要实测。

[STM3210C-EVAL 产品页](https://www.st.com/en/evaluation-tools/stm3210c-eval.html) ·
[UM0600 Rev 5，第 11、15、17、34–35 页](https://www.tme.eu/Document/aece8c709b198489a4f26cf104594d1a/CD00212441.pdf) ·
[Clause 22 软件装配](../../tests/integration/assemblies/stm32f1_mdio/mb784-c01.toml) ·
[CN6 和 Clause 22 联合装配](../../tests/integration/assemblies/stm32f1_uart_remap/mb784-c01.toml)
