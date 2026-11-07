# 维护的源码 SDK 装配参考

这些 TOML 随 SDK 发布，每份明确选择一个 Board、后端和预算。产品在外部工程持有
装配，不把测量样例当默认产品配置。

| 文件族 | 精确选择 |
| --- | --- |
| `nucleo-h743-*` | STM32H743ZIT6／NUCLEO-H743ZI2，固定 HSI64、Cache off |
| `nucleo-u575-*` | STM32U575ZIT6Q／NUCLEO-U575ZI-Q，固定 MSIS4，外部已建立 TZEN=0、SWAP_BANK=0 |
| `empty` | 不选 I/O |
| `uart` | 已审 LED 与 IRQ-byte console：H743 USART3 PD8／PD9，U575 USART1 PA9／PA10 |

Baremetal／FreeRTOS 分文件。测量参考命名 `led0`、`uart3`／`uart1`，供 linkage
workload 使用；ID 属于装配，外部工程可以选其他名字。

它们是软件参考，实板未执行。PCB revision、电源、焊桥、option bytes、silicon
revision 需实际观察，不自动启用 DMA／Cache 一致性、Secure pair、变频或 STOP。
支持由装配和实际镜像决定，文件名不授予额外能力。

UART workload 例如设置
`NEXUS_ASSEMBLY_FILE=tools/configure/assemblies/nucleo-h743-baremetal-uart.toml`
和 `NEXUS_WORKLOAD=uart`。其他工作目录使用绝对装配路径，准备匹配 ARM 工具。
离线资格另需完整可发布 SDK 的两次全新断网构建，保留各自原始 ELF／BIN／map。
