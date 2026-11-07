# 多控制器软件装配

软件 Board 明确选择 F407ZGT6、F407VET6、GD470ZGT6 硅片 pad，PCB、外部线、pull、
电压与物理资格未知。每料号六份手写 TOML，runner 不改 backend 或临时编装配：

| 后缀 | 控制器图 | 后端 |
| --- | --- | --- |
| `.toml` | 两 UART TX-DMA、两 SPI DMA、一个 secondary I2C DMA | baremetal |
| `-freertos.toml` | 同上五个独立 controller | FreeRTOS |
| `-i2c-pair.toml` | 两 I2C 有限 message DMA | baremetal |
| `-i2c-pair-freertos.toml` | 同一 I2C DMA pair | FreeRTOS |
| `-i2c-pair-irq.toml` | 两 I2C 有限 message IRQ | baremetal |
| `-i2c-pair-irq-freertos.toml` | 同一 I2C IRQ pair | FreeRTOS |

secondary UART TX 与 primary I2C TX 共用 STM DMA1／GD DMA0 的物理 stream6，
即使 selector 不同也冲突，resolver 拒绝同时选择；五路图改选 secondary I2C。
不同 bus 的同地址 endpoint 保持独立身份／context。

CMake 读 `NEXUS_SOURCE_ROOT` 和一份 `NEXUS_ASSEMBLY_FILE`，调用生产
`nexus_add_firmware`。main 通过 typed factory 保留全部 controller／endpoint，
不发传输、不证明器件。post-link 检查实际 context／只读 ops，provider runner
另查预算和 source／config。

生成冷装配持有 clock／pin／NVIC lease，失败构造也参与逆序清理。实际撤源与 NVIC
quiescence 后才恢复 pad；每个 active-low CS 在同组任一引脚恢复前先观察 inactive。
撤回拒绝保留 lease、阻止 restart，直到显式清理成功。日常 I/O 仍直接调用共享表。
