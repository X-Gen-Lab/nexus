# STM32F407 掩码状态下的镜像交接

本目录提供 `Nexus::STM32F407Handoff`，仅在精确 STM32F407VET6／ZGT6 裸机
Platform assembly 中存在。通过 `nexus_add_firmware(... LIBRARIES ...)` 显式
链接；产品只包含 `nexus/components/stm32f407_handoff.h`，vendor 寄存器和可变
准入存储保持私有。

## 责任域与交接流程

在准入用户前调用一次 `nx_stm32f407_handoff_initialize(capacity)`。稳定 getter
`nx_stm32f407_handoff_domain()` 不初始化硬件或分配。显式登记每个 Platform／产品
executor、IRQ、DMA、notification 和 memory owner。关闭准入后，以真实 API 取消、
排空并 join，停止 Platform，随后才释放每个精确 lease。外来空 domain、过期 epoch
或 retained lease 都不能授权交接。domain 不自动知道未登记用户、另一 CPU 或总线主设备。

产品从 privileged MSP Thread 保存／屏蔽中断，调用
`nx_stm32f407_handoff(closed, physical_flash_vector)`。机制只做受限 handoff，
不认证镜像、不选择 boot／recovery 策略，也不预留 Flash 分区。

## 精确进入条件

必须 PRIMASK=1、BASEPRI=FAULTMASK=0；没有 active FP／lazy preservation、MPU、
已启用／pending／active IRQ 或 SysTick。System 已停，使用 HSI16／reset buses，
没有 peripheral／DMA gates 或 busy Flash。普通 SystemInit 的完整 CP10／11
权限在没有活跃 FP owner 时可接纳；尾部以 DSB／ISB 禁用 CP10／11，随后替换 MSP
并跳转。

[FIG-23 所有权图](../../../../docs/assets/diagrams/io-extension-ownership.svg) 中的
镜像交接分支表示此类显式会合责任，不表示通用 boot 或已执行物理交接。

拒绝保留 masks 且不写 MMIO，但读取 SysTick CTRL 会消费 COUNTFLAG。off gate
只阻止执行，不能认证 controller 的潜在配置已清除；目标在开 gate 前 reset controllers。

完整 98 项 vector table 必须位于生成和物理观察一致的 512 KiB／1 MiB Flash 中，
并按 512 对齐。MSP 按 8 对齐且位于 `(0x20000000,0x20020000]`；Reset PC 为
Thumb，其首个 halfword 在物理 Flash 内。DEV_ID `0x413` 是共享器件组，F407／
封装身份仍来自精确 part／PCB 来源，不能仅凭 device ID 证明。

## 成功后的状态与限制

成功交接永不返回。VTOR 指向已准入 Flash table，CONTROL=0、PSP=0、CP10／11
关闭、PRIMASK=1。主 SRAM 保留到目标 startup 自行 copy／clear。目标须接受
masked entry，仅在自己的状态 ready 后 unmask，不能复用源 deadline。调用者全程
排除 NMI／HardFault、debugger 和其他 masters。

源码入口为公共头及 `src/`，构建条件在 `CMakeLists.txt`。本组件是保守 SOC-33
软件增量，不是完整 SOC-33 验收；不接管 live RTOS／kernel，不转移 active-FPU
context，不支持 RAM／ROM vector、H743／Cache-on transition、通用 Cortex-M 或
实测交接。SOC-34 DVFS 与 IO-30 runtime clock 仍开放，固定频率 provider 不因此
变为 mutable-clock 合同。
