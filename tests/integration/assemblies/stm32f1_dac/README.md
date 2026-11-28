# STM32F1 单通道 DAC 直接输出

各装配以 HSI8 运行。通过应用 TOML 的 `binding` 与 `channels` 选择一条路线。

| 装配 | 绑定 | 通道和引脚 |
| --- | --- | --- |
| `f100rb-dac.toml` | `dac1` | STM32F100RBT6B 通道 1，PA4 |
| `f103ze-dac.toml` | `dac1` | STM32F103ZET6 通道 1，PA4 |
| `f103ze-dac2.toml` | `dac2` | STM32F103ZET6 通道 2，PA5 |
| `f103zg-dac.toml` | `dac1` | STM32F103ZGT6 通道 1，PA4 |

应用从强类型 factory 获取 DAC 接口，显式启动后写入原始码。
`nx_dac_port_stop()` 恢复选定通道的数字空闲码，平台关闭时停用 DAC、恢复引脚。
每个装配独占 DAC 外设和时钟。通道 1 与通道 2 的装配配置互斥。
驱动支持 8 位和两种 12 位对齐格式。当前装配使用 12 位右对齐。
寄存器模型覆盖通道权限、数字码、空闲恢复和关闭失败重试。

F103ZE PA5 为 LQFP144 第 41 脚，对应 DAC_OUT2。输出电压、负载和模拟建立时间
需要实板验证。资料见 RM0041 Rev6 §11.3、RM0008 Rev21 §12.2、§12.3 和
§12.5、DS6329 Rev9 表 46、DS5792 Rev13 表 5 和表 63、DS6558 Rev4 表 66，
以及 F103 高密度器件的 ES0340 Rev17。
