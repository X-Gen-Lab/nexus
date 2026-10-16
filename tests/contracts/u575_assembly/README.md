# Actual U575 assembly fixtures

`empty.toml` and `io.toml` select exact STM32U575ZIT6Q, TZEN0 MSIS4MHz, SRAM1 and
the source-reviewed MB1549 Board. Their explicit FreeRTOS variants additionally
retain the real selected M33 NTZ port. CMake uses the maintained platform graph,
official U575 startup and owned SystemInit; fixture code never replaces providers.

These images retain startup, monotonic time, factory faces, readonly method tables
and real provider methods for actual ELF resource/retention checks. They are
link fixtures, not executed firmware or products. All physical statuses remain
`not_executed`; source/configuration/tool identities and raw compiler artifacts
must be recorded for each execution. No prior CPU-only M33 fixture establishes
U575 chip/Board support, and no archived build count is inherited.

`peripherals.toml` and `peripherals-freertos.toml` select exact SPI1 PA5/6/7,
PA4 CS, I2C1 PB8/9 with an independently leased HSI16 kernel, ADC4 PC0/channel1,
EXTI1 PA1 and full physical Flash. I2C clock acquisition preserves SYSCLK/TIM2;
stop joins the actual common V2 port before restoring selector and HSION.
`pwm.toml` and `pwm-freertos.toml` select TIM3 CH1 PA6 separately because that
pad is also SPI1 MISO. ADC startup requires an external prepared VDDA domain
and observed PWR.SVMCR.ASV; these link fixtures never assert electrical power.
The main retains public typed faces and readonly operations without starting
Flash erase/program or arbitrary product work. Physical HIL is not executed.

中文：外设综合夹具分别覆盖裸机与 FreeRTOS 的 SPI/I2C/ADC/EXTI/Flash 静态装配；
I2C 单独取得 HSI16，保持系统和 TIM2 时钟不变，停止外设后再恢复选择器并撤销时钟。
PWM 使用独立夹具避免 PA6 资源冲突。ADC 的 VDDA/ASV 必须由外部电源所有者预先
建立，夹具只证明软件构建和链接，不能证明板卡供电、接线或外设物理行为。
