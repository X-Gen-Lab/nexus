# GD32F470 可选内存合同

本 consumer 使用生产 CMake、梁山派 SoC facts，每镜像一份装配，不在硅片执行。
GD32F470ZGT6 主 SRAM 为 `0x20000000` 的 192 KiB，DMA 可达 ADDSRAM 为
`0x20030000` 的 256 KiB，CPU data-only TCM 为 `0x10000000` 的 64 KiB。
MSP 和普通对象始终留在主 SRAM。

default 不选额外区域，另外三份各选 ADDSRAM、TCM、二者，独立预算 128 B。
每区保留一个初始化 word、16 B BSS、16 B NOINIT；各 placement 类未用对象必须被
GC，不能通过 KEEP 强留。NOINIT 先由调用者初始化，不承诺复位／断电保留。

GoogleTest 用寄存器／地址模型执行真实边界 initializer 和 SystemInit；ARM ELF
检查 placement、Flash LOAD、section type、startup reference、预算拒绝和 GC。
不证明内存时序、电源、DMA 访问或实际板启动，HIL 为 `not_executed`。

手册与 hash 见 GD32 provider README。家族内存图只是最大值，实际 ZG 控制额外
256 KiB；TCM 永远不是 instruction／ramfunc 或 DMA buffer 域。
