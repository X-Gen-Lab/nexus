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

`stm32-ve-encoders-baremetal.toml` 和 `stm32-zg-encoders-freertos.toml` 同时选择
TIM3／4／5 的原始正交计数，分别使用 x2 TI1／x2 TI2／x4 TI12。两个输入由 Board
显式授权，各 timer 与自己的 PWM／capture personality 互斥。平台只 prepare，
不隐式 start；原始回卷、filter code 和软件观察时间不替代产品的累计位置／速度策略，
不建立物理传感器资格。

`stm32-ve-adc-injected-baremetal.toml` 与 `stm32-zg-adc-injected-freertos.toml` 选择
三个独立 ADC 的软件注入组，覆盖内部输入、重复 rank 和非默认分辨率。重复采样不隐式
平均；ZG 的 TIM3 PWM 独立运行，不声称与注入采样相位对齐。全部数据为 raw count，
由已有静态 factory 与同一 ADC state／config 连接，不引入额外实例存储或任务。

`stm32-ve-i2c-tenbit-baremetal.toml` 与 `stm32-zg-i2c-tenbit-freertos.toml`
使用三个独立 I2C controller，交叉覆盖 polling／IRQ／DMA 和 7／10-bit endpoint。
7-bit 与 10-bit 的相同数值地址属于不同身份；10-bit 地址字节不计入 payload。
VE 的 I2C3 DMA 与 ZG 的 I2C1／2 DMA 使用不冲突的 stream，不隐式替换路线。
Standard 10-bit profile 限制在名义 88 kHz 以内；Fast profile 显式选择 400 kHz
上限，不据此授予上拉、电容、目标器件或重复 START 物理时序资格。

`stm32-ve-wwdg-baremetal.toml` 仅选择 WWDG，
`stm32-zg-watchdogs-freertos.toml` 同时选择 IWDG 与 WWDG。prepare 不启狗，产品显式
调用 enable。`window_ticks` 为必须经过的 WWDG decrement 数，不是微秒；62 的极限
只适用于仍保留合法喂狗计数的 timeout。两种看门狗使用同一公共接口与独立静态身份，
不自动喂狗、启动任务或授予物理复位／时间窗资格。

`stm32-ve-pvd-baremetal.toml` 选择无事件队列的 PVD level，
`stm32-zg-pvd-freertos.toml` 选择显式四项存储、双边沿与 priority6 的 PVD events。
两者均选择 PLS code5，仅作为软件参考；PVDO high 表示低电压，不能解释为 power-good。
不取得 GPIO／SYSCFG，不选择产品阈值政策、不证明实际电压、BOR 或低功耗唤醒。

`stm32-ve-advanced-pair-baremetal.toml` 和
`stm32-zg-advanced-pair-freertos.toml` 同时选择 TIM1／TIM8 的 CH1／CH1N／BKIN。
TIM1 使用 PA8／PB13／PB12 AF1，TIM8 使用 PC6／PA7／PA6 AF3；168 MHz timer clock
下显式选择 1 MHz tick、1000-tick period 与 128 个 timer-clock deadtime ticks。
两路分别占用 TIM9／TIM12 共享 break vector，不能同时选择其 peer。main 保留两个
真实 factory face，但不 start 输出；软件 Board 不授权具体 PCB 或功率级接线。
