# NUCLEO-H743ZI2 板级软件参考

本包选择 STM32H743ZIT6，采用 UM2407 Rev6（2026 年 4 月）的 MB1364 已审资料
路线。实际 PCB 修订、稳压器实装和焊桥未知，物理 HIL 未执行。

## 已审参考配置

初始配置使用内部 HSI 64 MHz、LDO 供电、Cache off，并保留独立 TIM2 时间域。
LED1 使用高有效 PB0，资料路线为 SB39 ON／SB47 OFF；USART3 PD8/PD9 AF7 使用
ST-LINK VCP 的 SB19/SB12 路径。改用 PA5 LED 实装必须建立独立已审 Board 路线。

外部产品须检查实际 PCB，并负责 LED、console、供电、安全与恢复策略。包中的
PC2_C/PC3_C 是条件模拟焊盘，不是直接 PC2/PC3 GPIO 路线；本包不配置模拟开关，
也不建立其电气资格。

## 架构与限制

[H743 SoC 合同](../../soc/stm32/stm32h743/README.md)说明软件所有权和模式限制。
DMA 引擎的物理可达事实不能自动开放 DMA 模式、Cache 所有权或 TCM 放置。
主机模型和 ARM 链接不证明实际时钟精度、供电入口延迟、串口可靠性或安全输出。
