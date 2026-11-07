# Provider 多控制器隔离模型

GoogleTest 编译真实 SoC provider 和共享算法，只在外部 CPU／NVIC、DATA 副作用、
DMA W1C／disable 边界建模。STM32 走正常 NVIC，不定义 `NEXUS_STM32_MODEL`；
两家族共用既有 M4 host shim，旧 GD32 变量名不选择 SoC。

私有 `nexus_config.h` 提供 host-only 四位 BASEPRI 和芯片实际 IRQ count，不选择
Board、设备、factory 或固件装配；产品与真实 ARM 继续消费权威 TOML 投影。
模型证明 request／vector／memory 所有权隔离，不证明总线时序、DMA 吞吐或电气。
