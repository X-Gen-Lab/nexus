# STM32F103RB TIM3 正交编码器软件参考

三份装配共用 HSI8 裸机配置和 TIM3 默认引脚。在 NUCLEO-F103RB 上，
PA6／TI1 接到 Arduino D12，PA7／TI2 接到 D11。两脚均为浮空输入，
需要外部驱动的兼容电平和公共地。TOML 的 `counting` 选择计数边沿。

| 装配 | counting | 计数边沿 |
| --- | --- | --- |
| `f103rb-tim3.toml` | `x4` | PA6、PA7 的上升沿和下降沿 |
| `f103rb-tim3-x2-a.toml` | `x2-a` | PA6 的上升沿和下降沿，PA7 参与方向判断 |
| `f103rb-tim3-x2-b.toml` | `x2-b` | PA7 的上升沿和下降沿，PA6 参与方向判断 |

三种模式独立设置每相极性和输入滤波，滤波时钟为标称 8 MHz。
`nx_encoder_read()` 返回 TIM3 的 16 位模计数和软件观测时间。
停止保留计数，关闭释放定时器并恢复引脚。装配独占 TIM3，无法同时选用
TIM3 PWM 或 TIM3 TRGO 的 ADC 触发。TIM1 CC1 触发仍可独立选用。

计数方向参照 RM0008 Rev21 表 85。RM0008 Rev21 第 15.3.12 节和 AN4013
Rev14 的 x2 选择编码说明存在互换，驱动采用 Nexus 锁定的 STM32CubeF1 LL
中的 TI1／TI2 符号。ES096 Rev15 第 2.3.8 节所述重映射路线在冷态被拒绝，
第 2.6.5 节涉及的捕获寄存器未参与计数读取。引脚路线见 UM1724 Rev17 表 12
和 DS5319 Rev20 表 5。[ST 对 TIMx_SMCR 编码说明的更正](https://community.st.com/stm32-mcus-products-25/has-there-been-any-changes-to-timx-smcr-in-stm32f407-series-mcus-122444)

寄存器模型验证实际 SMS、两相滤波配置、正反向回卷以及停止保留计数。
实际转向、输入噪声、滤波时延和可用边沿速率待实板验证。
