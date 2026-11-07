# 跨厂家 SoC 协作合同

本私有目录保存精确 SoC provider 共用的 `uart_control.h` 控制合同。它不拥有公共
I/O API、芯片事实或外设算法；公共请求借用合同仍由 `io/` 定义。

经审核的共享寄存器算法位于 [drivers](../../drivers/README.md)。STM32 身份访问位于
`soc/stm32/common/stm32_identity.h`，不属于跨厂家合同。每个 SoC wrapper 继续负责
芯片专属校验、clock 和 IRQ／DMA 所有权。
