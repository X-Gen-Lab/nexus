# 多控制器软件装配

软件 Board 明确选择 F407ZGT6、F407VET6 硅片 pad，PCB、外部线、pull、
电压与物理资格未知。以下手写 TOML 保留各自模式和后端，不运行时改写装配：

| 后缀 | 控制器图 | 后端 |
| --- | --- | --- |
| `.toml` | 两 UART TX-DMA、两 SPI DMA、一个 secondary I2C DMA | baremetal |
| `-freertos.toml` | 同上五个独立 controller | FreeRTOS |
| `-i2c-pair.toml` | 两 I2C 有限 message DMA | baremetal |
| `-i2c-pair-freertos.toml` | 同一 I2C DMA pair | FreeRTOS |
| `-i2c-pair-irq.toml` | 两 I2C 有限 message IRQ | baremetal |
| `-i2c-pair-irq-freertos.toml` | 同一 I2C IRQ pair | FreeRTOS |
| `stm32-ve-tertiary.toml` | SPI3 DMA、I2C3 DMA、ADC2 有限 DMA | baremetal |
| `stm32-ve-tertiary-freertos.toml` | SPI3 DMA、I2C3 IRQ、ADC2 连续 DMA | FreeRTOS |
| `stm32-zg-tertiary.toml` | SPI3 polling、I2C3 polling、ADC3 连续 DMA | baremetal |
| `stm32-zg-tertiary-freertos.toml` | SPI3 DMA、I2C3 DMA、ADC3 有限 DMA | FreeRTOS |
| `stm32-ve-uart6.toml` | 六个 UART 有限 TX DMA 与独立 IRQ RX events | baremetal |
| `stm32-zg-uart6-freertos.toml` | 同上六个独立 UART controller | FreeRTOS |

secondary UART TX 与 primary I2C TX 共用 DMA1 的物理 stream6，
即使 selector 不同也冲突，resolver 拒绝同时选择；五路图改选 secondary I2C。
不同 bus 的同地址 endpoint 保持独立身份／context。

SPI3 在 VE 使用 PC10／11／12，在 ZG 使用 PB3／4／5，均为 AF6；I2C3 使用
PA8／PC9 AF4。SPI3 占 DMA1 stream5 TX／stream0 RX，I2C3 占 DMA1 stream4 TX／
stream2 RX。ADC2 独占 DMA2 stream2 channel1，ADC3 独占 stream1 channel2；它们的
TIM3 触发源不能与其他 ADC stream 或 PWM owner 共享。ZG 的 ADC3 参考输入是
PF6，VE 是 PC0；这些选择不授权任何真实 PCB 接线。

CMake 读 `NEXUS_SOURCE_ROOT` 和一份 `NEXUS_ASSEMBLY_FILE`，调用生产
`nexus_add_firmware`。main 通过 typed factory 保留全部 controller／endpoint，
不发传输、不证明器件。post-link 检查实际 context／只读 ops，provider runner
另查预算和 source／config。

生成冷装配持有 clock／pin／NVIC lease，失败构造也参与逆序清理。实际撤源与 NVIC
quiescence 后才恢复 pad；每个 active-low CS 在同组任一引脚恢复前先观察 inactive。
撤回拒绝保留 lease、阻止 restart，直到显式清理成功。日常 I/O 仍直接调用共享表。
