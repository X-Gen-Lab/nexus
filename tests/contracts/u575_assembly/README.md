# U575 实际装配夹具

empty／io 精确选 STM32U575ZIT6Q、TZEN0、MSIS4 MHz、SRAM1、来源复核 MB1549；
FreeRTOS 变体另保留真实 M33 NTZ port。CMake 用生产图、官方 startup 与自有
SystemInit，fixture 不替代 provider。

ELF 保留启动、单调时基、factory、只读表、真实方法，供资源／retention 检查；
它们不是已运行固件或产品。每次记录 source／config／tool 和原始产物，HIL 保持
`not_executed`，不继承旧 build count，CPU-only M33 不证明 U575 芯片／Board。

peripherals 两变体选 SPI1 PA5／6／7、PA4 CS、I2C1 PB8／9 与独立 HSI16 kernel、
ADC4 PC0／channel1、EXTI1 PA1、全物理 Flash。I2C 保持 SYSCLK／TIM2，stop 先
join 真实 V2 port，再恢复 selector／HSION。PWM 两变体另选 TIM3 CH1 PA6，避免
SPI1 MISO 冲突。ADC 要外部已准备 VDDA 且观察 PWR.SVMCR.ASV，链接不证明电源。
main 只保留 face／ops，不启动 Flash erase／program 或任意产品业务。
