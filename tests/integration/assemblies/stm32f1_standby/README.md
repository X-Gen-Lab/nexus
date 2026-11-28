# F103RB PA0 WKUP Standby

`f103rb-pa0.toml` 选择 F103RBT6、HSI8 和裸机，并显式填写
`[hardware] standby_wakeup = "pa0-wkup"`。板级声明将 LQFP64 的 PA0
（14 脚）独占保留给 Standby。PA0 为有效高，外部接线必须保证空闲低及合法输入电压。
本软件参考没有实板证据。

应用在启动成功后可通过 `nexus/soc/stm32f103_standby.h` 读取独立的 RCC 复位原因
和 PWR SBF／WUF。SBF 表示进入过 Standby，WUF 可能因复位清除。这些标志无法单独
识别 PA0 唤醒。示例默认只取证并停止平台。进入验证由工装设置 `s_nx_standby_requested`，
实际验证时须关闭调试器的 Standby 保持选项。

应用先撤回所有使用者、旧 deadline 和设备借用，再关闭自己的 ClockDomain。
`nx_stm32f103_standby_enter()` 验证无租约，完成 Platform 停止后才独占打开
PWR 和 GPIOA 时钟。入口确认 PA0 浮空输入且低电平、RTC 关闭、IWDG 由软件启动
且当前未运行、无预存系统中断。进入前禁用 EWUP，只按显式授权清除已观察到的 WUF，
恢复 EWUP 后重新检查 PA0、WUF 和 pending。SBF 与新的唤醒证据保留。

真 Standby 从复位重新启动，原 SRAM 内容、时基与请求不可沿用。WFI 返回报告错误。
只有硬件撤回成功才释放临时 GPIOA 和 PWR 时钟。若返回的 `platform_stopped` 为真，
由调用者依据 `cleanup` 选择显式重启或恢复。未选择板级能力时不编译 Standby。

[RM0008 Rev 21 §5.3.5、表 15、§5.4.1／5.4.2](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) · [ES096 Rev 15 §2.2.5](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf) · [DS5319 Rev 20 表 5](https://www.st.com/resource/en/datasheet/stm32f103rb.pdf)
