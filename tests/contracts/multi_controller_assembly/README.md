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

四份 `*uart6-*-baremetal.toml`／`*uart6-*-freertos.toml` 交叉覆盖两种精确芯片、
两个后端和有限／连续 RX、有限／连续 duplex。每份六路同时选择，TIM／其他外设
未授权使用同一 DMA lane。所有 RX blocks 由消费者显式提供，不隐式建立池或任务。

`stm32-ve-buses-poll.toml` 同时选择三个 SPI 的增强 polling endpoint 和三个 I2C
polling controller；`stm32-zg-buses-dma-freertos.toml` 改选三个 SPI 的增强 DMA
事务并保留三个 I2C polling。三个 SPI 的 CS 独立，factory 保留所有增强面及其真实
端口；不使用未选中的普通 endpoint 代替，ELF 检查实际保留情况。
这两份装配的 SPI1 为默认 MSB-first，SPI2／3 endpoint 显式选择 `lsb_first=true`；
位序选择属于端点，不改变 controller／DMA lane、word 宽度或 buffer 内容。

`stm32-ve-uart6-irq-words-baremetal.toml` 选择六路独立 9N1 word face；
`stm32-zg-uart6-irq-blocks-freertos.toml` 选择六路 IRQ block face。这两种模式均不
取得 DMA，word face 不替代 byte face。block 的存储由消费者在 `rx_start` 显式提供。

`stm32-ve-i2c3-fast-poll-baremetal.toml` 选择三个 polling I2C，名义最大速率分别为
400000／350000／100001 Hz，覆盖向上取整分频；
`stm32-zg-i2c3-fast-async-freertos.toml` 选择 I2C1／2 IRQ 与 I2C3 DMA，均为
400000 Hz。它们不证明上拉、电容或实际 SCL 波形，旧参考装配保持各自配置。

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

`stm32-ve-pwm35-baremetal.toml` 和 `stm32-zg-pwm35-freertos.toml` 同时选择独立的
TIM3 与 TIM5 PWM。软件 Board 分别授权 CH3／CH4 和 CH4／CH1；VE 的 TIM5 使用
UINT32_MAX 周期，ZG 使用 70000。每个控制器只选择一路，保留 factory 真正的 PWM
face／ops；不支持两个 timer 的隐式同步或一个 timer 的多 owner 共享。

`stm32-ve-adc-internal-baremetal.toml` 和
`stm32-zg-adc-internal-freertos.toml` 分别选择 ADC1 的内部 raw scan 和连续 DMA。
通道 16／17／18 使用 480-cycle acquisition，不取得 GPIO；连续模式仍独占 TIM3
及 DMA2 stream4 channel0，存储由调用方提供。factory 保留真实接口，换算、校准和
物理温度／电压资格不属于这些链接夹具。

`stm32-ve-pwm-family-baremetal.toml` 和 `stm32-zg-pwm-family-freertos.toml` 同时选择
九个独立的 TIM3／4／5／9／10／11／12／13／14 PWM controller，每个仅选一路。
TIM9 使用 168 MHz tick、PSC=0；其他实例保留各自整数分频。TIM4 不同时取得 capture，
也不选择占用 TIM9 的 TIM1 advanced PWM。它们链接真实静态 factory 与共享 ops，
不代表硬件波形、时钟精度或电气输出资格。
