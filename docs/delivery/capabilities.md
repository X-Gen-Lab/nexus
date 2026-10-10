# 派生能力与接入矩阵

本表由 `tools/maintenance/capabilities.py` 读取维护中的 `soc.json`、
`routes.json`、参考 Board `board.json`，并运行现有软件 assembly 的生产 resolver
和 binding emitter 生成。表格不是另一份配置输入；修改能力从原始事实、实现和
回归开始，随后执行 `--write` 更新本派生文本，`--check` 拒绝任何过时内容。

- **Mode** 是 SoC 声明的有限模式；无 route 的模式不能据此选择连接。
- **Reviewed SoC routes** 描述已维护的芯片路线，不是 PCB 连续性或电气资格。
- **Declared reference Boards** 表示 Board 中存在对应 binding，不表示默认 assembly
  已启用它，也不表示实板验收通过。
- **Resolver fixtures** 是当前实际通过解析的 TOML 软件夹具。
- **Selected production source** 从实际生成的 face 读取完整 ops 标识，匹配生产 C
  中已初始化的导出定义；注释、字符串、声明和相似前缀不算定义。这是有限源码
  一致性证据，不替代编译、执行或物理测量。空白列明确没有该类证据。

表格不从旧候选继承新 HEAD 的软件资格，也不推断物理结果。三板实物未连接，HIL
保持 `not_executed`。精确模式限制及所有权合同见[支持范围](support.md)。

```sh
python tools/maintenance/capabilities.py --check
python tools/maintenance/capabilities.py --write
```

<!-- nexus-capabilities:start -->
| SoC | Controller | Kind | Mode | Reviewed SoC routes | Declared reference Boards | Resolver fixtures | Selected production source |
| --- | --- | --- | --- | --- | --- | --- | --- |
| gd32f470 | ADC0 | adc | low-rate-scan | `adc0-pa0-pa1` | — | `tests/contracts/gd32_assembly/adc-scan.toml` | `soc/gd32f470/drivers/adc.c` |
| gd32f470 | ADC0 | adc | single-shot | `adc0-pa0` | — | `tests/contracts/gd32_assembly/adc.toml` | `soc/gd32f470/drivers/adc.c` |
| gd32f470 | ADC0 | adc | trigger-dma | `adc0-pa0`, `adc0-pa0-pa1` | — | `tests/contracts/gd32_assembly/blocks.toml` | `soc/gd32f470/drivers/adc_stream.c` |
| gd32f470 | EXTI3 | exti | edge-event | `exti3-pe3` | — | `tests/contracts/gd32_assembly/exti.toml` | `soc/gd32f470/drivers/exti.c` |
| gd32f470 | FMC | flash | poll-word | `physical-flash` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/flash.toml` | `soc/gd32f470/drivers/flash.c` |
| gd32f470 | FWDGT | watchdog | independent | `independent-watchdog` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/watchdog.toml` | `soc/gd32f470/drivers/watchdog.c` |
| gd32f470 | GPIOB | gpio | input | — | — | — | — |
| gd32f470 | GPIOB | gpio | output | `gpiob0`, `gpiob1` | — | — | — |
| gd32f470 | GPIOD | gpio | input | `gpiod7` | `gd32f470zg-liangshan` | — | — |
| gd32f470 | GPIOD | gpio | output | `gpiod7` | `gd32f470zg-liangshan` | `tools/configure/assemblies/liangshan-baremetal-gpio.toml`, `tools/configure/assemblies/liangshan-baremetal.toml`, `tools/configure/assemblies/liangshan-freertos-gpio.toml`, `tools/configure/assemblies/liangshan-freertos.toml` | `soc/gd32f470/drivers/gpio.c` |
| gd32f470 | I2C0 | i2c | poll-messages | `i2c0-pb6-pb7` | — | `tests/contracts/gd32_assembly/blocks.toml`, `tests/contracts/gd32_assembly/dma.toml`, `tests/contracts/gd32_assembly/i2c.toml`, `tests/contracts/gd32_assembly/multi.toml` | `soc/gd32f470/drivers/i2c.c` |
| gd32f470 | I2C1 | i2c | poll-messages | `i2c1-pb10-pb11` | — | `tests/contracts/gd32_assembly/blocks.toml`, `tests/contracts/gd32_assembly/dma.toml`, `tests/contracts/gd32_assembly/multi.toml` | `soc/gd32f470/drivers/i2c.c` |
| gd32f470 | SPI0 | spi | short-poll | `spi0-pa5-pa7` | — | `tests/contracts/gd32_assembly/blocks.toml`, `tests/contracts/gd32_assembly/dma.toml`, `tests/contracts/gd32_assembly/multi.toml` | `soc/gd32f470/drivers/spi.c` |
| gd32f470 | SPI4 | spi | dma-full-duplex | `spi4-pf6-pf9` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/blocks.toml`, `tests/contracts/gd32_assembly/dma.toml` | `soc/gd32f470/drivers/spi_dma.c` |
| gd32f470 | SPI4 | spi | short-poll | `spi4-pf6-pf9` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/multi.toml`, `tests/contracts/gd32_assembly/spi.toml`, `tools/configure/assemblies/liangshan-baremetal-spi.toml`, `tools/configure/assemblies/liangshan-freertos-spi.toml` | `soc/gd32f470/drivers/spi.c` |
| gd32f470 | TIMER2 | pwm | fixed-pwm | `timer2-pa6` | — | `tests/contracts/gd32_assembly/pwm.toml` | `soc/gd32f470/drivers/timer.c` |
| gd32f470 | USART0 | uart | dma-tx | `usart0-pa9-pa10` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/blocks.toml`, `tests/contracts/gd32_assembly/dma.toml` | `soc/gd32f470/drivers/uart_dma.c` |
| gd32f470 | USART0 | uart | irq-blocks | `usart0-pa9-pa10` | `gd32f470zg-liangshan` | — | — |
| gd32f470 | USART0 | uart | irq-byte-event | `usart0-pa9-pa10` | `gd32f470zg-liangshan` | `tests/contracts/gd32_assembly/multi.toml`, `tools/configure/assemblies/liangshan-baremetal-uart.toml`, `tools/configure/assemblies/liangshan-baremetal.toml`, `tools/configure/assemblies/liangshan-freertos-uart.toml`, `tools/configure/assemblies/liangshan-freertos.toml` | `soc/gd32f470/drivers/uart.c` |
| gd32f470 | USART1 | uart | irq-blocks | `usart1-pa2-pa3` | — | `tests/contracts/gd32_assembly/blocks.toml` | `soc/gd32f470/drivers/uart_stream.c` |
| gd32f470 | USART1 | uart | irq-byte-event | `usart1-pa2-pa3` | — | `tests/contracts/gd32_assembly/dma.toml`, `tests/contracts/gd32_assembly/multi.toml` | `soc/gd32f470/drivers/uart.c` |
| stm32f407 | ADC1 | adc | low-rate-scan | `adc1-ch0-pa0`, `adc1-scan-pa0-pa1` | — | `tests/contracts/stm32_assembly/ve-adc-scan.toml`, `tests/contracts/stm32_assembly/zg-adc-scan.toml` | `soc/stm32f407/drivers/adc.c` |
| stm32f407 | ADC1 | adc | single-shot | `adc1-ch0-pa0` | — | `tests/contracts/stm32_assembly/ve-adc.toml`, `tests/contracts/stm32_assembly/zg-adc.toml` | `soc/stm32f407/drivers/adc.c` |
| stm32f407 | ADC1 | adc | trigger-dma | `adc1-ch0-pa0`, `adc1-scan-pa0-pa1` | — | `tests/contracts/stm32_assembly/ve-blocks.toml`, `tests/contracts/stm32_assembly/zg-blocks.toml` | `soc/stm32f407/drivers/adc_stream.c` |
| stm32f407 | EXTI0 | exti | edge-event | `exti0-pa0` | — | — | — |
| stm32f407 | EXTI5 | exti | edge-event | `exti5-pa5` | — | `tests/contracts/stm32_assembly/ve-exti.toml`, `tests/contracts/stm32_assembly/zg-exti.toml` | `soc/stm32f407/drivers/exti.c` |
| stm32f407 | EXTI6 | exti | edge-event | `exti6-pa6` | — | `tests/contracts/stm32_assembly/ve-exti.toml`, `tests/contracts/stm32_assembly/zg-exti.toml` | `soc/stm32f407/drivers/exti.c` |
| stm32f407 | FLASH | flash | poll-word | `flash-internal` | `stm32f407ve-sky-qingchun`, `stm32f407zg-qiming-v31` | `tests/contracts/stm32_assembly/ve-flash.toml`, `tests/contracts/stm32_assembly/zg-flash.toml` | `soc/stm32f407/drivers/flash.c` |
| stm32f407 | GPIOA | gpio | input | — | — | — | — |
| stm32f407 | GPIOA | gpio | output | — | — | — | — |
| stm32f407 | GPIOB | gpio | input | — | — | — | — |
| stm32f407 | GPIOB | gpio | output | `gpio-pb2` | `stm32f407ve-sky-qingchun` | `tools/configure/assemblies/sky-baremetal-gpio.toml`, `tools/configure/assemblies/sky-baremetal.toml`, `tools/configure/assemblies/sky-freertos-gpio.toml`, `tools/configure/assemblies/sky-freertos.toml` | `soc/stm32f407/drivers/gpio.c` |
| stm32f407 | GPIOE | gpio | input | — | — | — | — |
| stm32f407 | GPIOE | gpio | output | `gpio-pe3`, `gpio-pe4` | `stm32f407zg-qiming-v31` | `tools/configure/assemblies/qiming-baremetal-gpio.toml`, `tools/configure/assemblies/qiming-baremetal.toml`, `tools/configure/assemblies/qiming-freertos-gpio.toml`, `tools/configure/assemblies/qiming-freertos.toml` | `soc/stm32f407/drivers/gpio.c` |
| stm32f407 | GPIOG | gpio | input | — | — | — | — |
| stm32f407 | GPIOG | gpio | output | `gpio-pg9` | `stm32f407zg-qiming-v31` | — | — |
| stm32f407 | I2C1 | i2c | poll-messages | `i2c1-pb6-pb7` | — | `tests/contracts/stm32_assembly/ve-i2c.toml`, `tests/contracts/stm32_assembly/zg-i2c.toml` | `soc/stm32f407/drivers/i2c.c` |
| stm32f407 | IWDG | watchdog | independent | `iwdg` | `stm32f407ve-sky-qingchun`, `stm32f407zg-qiming-v31` | `tests/contracts/stm32_assembly/ve-watchdog.toml`, `tests/contracts/stm32_assembly/zg-watchdog.toml` | `soc/stm32f407/drivers/watchdog.c` |
| stm32f407 | SPI1 | spi | dma | `spi1-pa5-pa6-pa7` | — | `tests/contracts/stm32_assembly/ve-blocks.toml`, `tests/contracts/stm32_assembly/ve-dma.toml`, `tests/contracts/stm32_assembly/zg-blocks.toml`, `tests/contracts/stm32_assembly/zg-dma.toml` | `soc/stm32f407/drivers/spi_dma.c` |
| stm32f407 | SPI1 | spi | short-poll | `spi1-pa5-pa6-pa7` | — | `tests/contracts/stm32_assembly/ve-spi.toml`, `tests/contracts/stm32_assembly/zg-spi.toml` | `soc/stm32f407/drivers/spi.c` |
| stm32f407 | TIM3 | pwm | fixed-pwm | `tim3-ch1-pa6` | — | `tests/contracts/stm32_assembly/ve-pwm.toml`, `tests/contracts/stm32_assembly/zg-pwm.toml` | `soc/stm32f407/drivers/timer.c` |
| stm32f407 | USART1 | uart | dma-tx | `usart1-pa9-pa10` | `stm32f407ve-sky-qingchun`, `stm32f407zg-qiming-v31` | `tests/contracts/stm32_assembly/ve-dma.toml`, `tests/contracts/stm32_assembly/zg-dma.toml` | `soc/stm32f407/drivers/uart_dma.c` |
| stm32f407 | USART1 | uart | irq-blocks | `usart1-pa9-pa10` | `stm32f407ve-sky-qingchun`, `stm32f407zg-qiming-v31` | `tests/contracts/stm32_assembly/ve-blocks.toml`, `tests/contracts/stm32_assembly/zg-blocks.toml` | `soc/stm32f407/drivers/uart_stream.c` |
| stm32f407 | USART1 | uart | irq-byte-event | `usart1-pa9-pa10` | `stm32f407ve-sky-qingchun`, `stm32f407zg-qiming-v31` | `tools/configure/assemblies/qiming-baremetal-uart.toml`, `tools/configure/assemblies/qiming-baremetal.toml`, `tools/configure/assemblies/qiming-freertos-uart.toml`, `tools/configure/assemblies/qiming-freertos.toml`, `tools/configure/assemblies/sky-baremetal-uart.toml`, `tools/configure/assemblies/sky-baremetal.toml`, `tools/configure/assemblies/sky-freertos-uart.toml`, `tools/configure/assemblies/sky-freertos.toml` | `soc/stm32f407/drivers/uart.c` |
<!-- nexus-capabilities:end -->
