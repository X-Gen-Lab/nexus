# NUCLEO-F103RB 板级软件参考

本包选择精确 STM32F103RBT6 和名义复位 HSI8。依据 UM1724 Rev17，绑定 PA5/LD2、
USART2 PA2/PA3，以及模拟排针上的 PA0/PA1 原始边沿输入。

可选 SPI1 使用 PA5/PA6/PA7 和 PA4 软件 CS。PA5 共用 LED 负载，因此 SPI1 与 LD2
不能同时选择；外接 SPI 的布线、电压、负载和时序仍须验证。PA13/PA14 保留给 SWD。

本包不假定 HSE 已焊接，也未检查实际 PCB、焊桥或电气状态。VCP 路线要求匹配
实际板卡修订并确认 SB13/SB14 ON、SB62/SB63 OFF。

`board.json` 分开记录来源和未知项。[F103 实现](../../soc/stm32/stm32f103/README.md)
说明芯片模式及资源责任。HIL 仍为 `not_executed`；执行前须检查实板、探针、供电
和串口路径。产品策略、任务与协议由外部工程维护。
