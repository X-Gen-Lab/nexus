# NUCLEO-F103RB MCO 软件参考

此装配选择 MB1136 C-02 或后续修订的 8 MHz ST-LINK MCO。UM1724 Rev17
§7.9.1 要求 SB55 断开、SB54 接通、SB16 与 SB50 接通、R35 与 R37 缺件。
已供电的 ST-LINK 必须在 OSC_IN 提供有效时钟，系统才会切换到 PLL72。
C-01 默认没有 HSE。X3 晶体和 CN7 外部时钟属于各自的板级接线方案。

HSI 保持开启。PLL72 设置 HCLK 72 MHz、PCLK1 36 MHz、PCLK2 72 MHz、
TIM2 72 MHz、ADC 12 MHz 和 Flash 两个等待周期。72 MHz 是名义频率。
TIM2 在 1 MHz 下仍提供与 HSI8 相同的时间接口。ES096 §2.2.1 要求
两等待周期与预取并用时保留 Flash 接口时钟。系统启动前保持该接口
时钟开启，并在运行时保留它。

应用先启动 Platform，再启动 OS。Cortex-M3 SysTick 使用生成的核心频率。
启动失败会回到可证明的 HSI8 基线，切钟失败时保留相关硬件供重试。
时钟安全系统观察到外部输入故障时，结束当前时基并进入故障处理。

该参考覆盖软件装配。PCB 修订、焊桥、波形和实际频率待实板核对。

[ST UM1724](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
[ST RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-and-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)
[ST ES096](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
