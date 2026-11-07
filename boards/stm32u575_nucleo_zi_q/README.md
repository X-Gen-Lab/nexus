# NUCLEO-U575ZI-Q 板级软件参考

本包在资料已审的 MB1549 上选择 STM32U575ZIT6Q。内部 MSIS4MHz 配置使 `clocks`
保持空，不虚构 Board HSE 依赖。默认使用 SRAM1 192KiB；时钟、安全、DMA、Cache
限制见 [U575 SoC 合同](../../soc/stm32/stm32u575/README.md)。

## 已审路线

PC7/LD1 为高有效，资料要求 SB21ON／SB23OFF。USART1 PA9/PA10 AF7 使用 STLINK
VCP 路线：SB32/SB34ON，SB24/SB26/SB31/SB33OFF。PA13/PA14 保留给 SWD。
以上是官方来源默认值，不是某块实板的观察结果。

## 启动与验收边界

PCB／硅片修订、实际焊桥、SMPS／供电实装、振荡器精度及 TZEN/SWAP_BANK option
bytes 未知。外部 boot owner 必须确认 TZEN0/SWAP_BANK0，不能依赖平台修改
option bytes。物理 HIL 尚未执行。

USART 噪声毛刺勘误可能需要外部线路或滤波设计，寄存器模型通过不建立抗扰资格。
LED 行为、console 协议、任务编排和产品恢复策略均属于外部工程。
