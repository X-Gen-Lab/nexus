# NUCLEO-F103RB 双 SPI 软件参考

| 控制器 | 时钟／输入／输出 | GPIO 片选 |
| --- | --- | --- |
| SPI1 | PA5／PA6／PA7（D13／D12／D11） | PA4（A2），低有效 |
| SPI2 | PB13／PB14／PB15（CN10） | PB12（CN10），低有效 |

两组控制器使用 HSI8 下的 short-poll。每组有独立片选、状态和事务端点。SPI1 使用 APB2，SPI2 使用 APB1。两者复用生产事务实现。固件验证静态实例、配置绑定和已过期请求的处理。Native 寄存器模型覆盖传输、两个片选和停机恢复。

板卡引脚依据 UM1724 Rev17 表 12、表 27。芯片默认复用见 DS5319 Rev20 表 5 和 RM0008 Rev21 表 56。PA5 同时接到板载 LD2，实板验证时检查 SB21、波形和负载。

## STM32F107VC SPI3 外接参考

`f107vc-spi3.toml` 选择 STM32F107VCT6 的 remap 路由。PC10、PC11、PC12 分别对应
SCK、MISO、MOSI，PC9 是低有效 GPIO 片选。LQFP100 的对应引脚为 78、79、80、66。
HSE25 PLL72 配置提供 36 MHz PCLK1，选定 SCK 为 4.5 MHz，支持 8 位与 16 位主机轮询事务。

启动仅取得 SPI3_REMAP 位和所需时钟，保留 SWJ 设置和其他 AFIO 路由。停止先排空并复位
SPI3，再恢复 GPIO、原重映射状态和时钟。载板需提供外接 SPI 从设备、公共地及 CS 上拉。
完整 JTAG 下的 STM32F103ZE 默认路由占用 PB3/PB4，且该器件没有 PC10～PC12 重映射。
F107 参考依照 DS6014 Rev10 表 5 和表 43、RM0008 Rev21 表 57，以及 ES022 Rev11
§2.10.1～2.10.2。CRC、DMA、从机和 I2S3 未进入该装配。实板状态为 `not_executed`。
