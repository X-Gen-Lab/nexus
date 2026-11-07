# STM32G474RE HSE24 板级软件参考

本包依据 NUCLEO-G474RE／MB1367 C04/C05 资料描述 STM32G474RET6 参考配置。
名称标识软件 profile；实际 PCB 修订、器件实装、焊桥及全部 HIL 均未确认。

## 固定时钟与资料路线

`hse24-pll170` 使用 X3 24 MHz、PLLM6/N85/R2，HCLK、APB1/APB2 均为 170 MHz。
[UM2505 Rev7 §7.5.1 p22](https://www.st.com/resource/en/user_manual/um2505-stm32g4-nucleo64-boards-mb1367-stmicroelectronics.pdf)
规定默认 X3 路线为 SB25/SB26 ON、SB24/SB28/SB27 OFF、C56/C59 为 6.8 pF。
本配置不维护另一条 8 MHz ST-LINK MCO 焊桥时钟。

`led0` 授权 PA5／LD2，高有效、非活动值零，要求 SB6 ON。`uart1` 是 LPUART1
PA2 TX／PA3 RX AF12；已审 VCP 条件为 SB17/SB23 ON，SB18/SB22/SB13/SB19 OFF。
这些是手册事实，不是已观察实板，投运前产品须核对实际 PCB 与电气路径。

## 明确标注的芯片路线夹具

原 LED／VCP profile 保留。额外绑定提供 GPIOB0/GPIOC0、USART1 PA9/PA10 AF7、
USART2 PA2/PA3 AF7、USART3 PB10/PB11 AF7、UART4 PC10/PC11 AF5、UART5 PC12/PD2
AF5。其连接器、焊桥、负载和电压未检查，不能把资料里的 LPUART1 VCP 改称 USART2。
UART 使用显式生成的字节 ring 和 priority5，不声明 event/block RX 或 DMA。
TIM2/IRQ28、PF0/PF1 HSE 引脚保留；条件 PG10/NRST 在有已审 option 策略前也保留。

SPI1 PA5/PA6/PA7 AF5＋PA4 CS、SPI2 PB13/PB14/PB15 AF5＋PB12 CS 都是未连接器件
的软件夹具。低有效 CS 初始高，不据此推断传感器或板级电气连接。只维护有限字节
polling，固定配置上限为 42.5 MHz，实际频率由 APB 分频选择。`watchdog0` 提供 IWDG
但启动时不自动启用／喂狗；启用后的不可逆责任阻止完整 Platform 关闭。

TIM3 CH1 PA6 AF2、TIM4 CH1 PB6 AF2 是未接负载的固定 base PWM 夹具，各自独占
整个 timer，明确 GPIO 非活动电平，维护高有效向上计数和 staged duty0..period。
不推断连接器、advanced break、互补输出、capture 或 DMA。SPI／PWM 选择 OSPEEDR11；
逻辑计数频率不是物理引脚频率限制。投运前实际 VDD、电容负载和脉宽须满足
DS12288 Table56 与对应外设条件。

## 所有权与验收

生成的冷态装配只租用选定 GPIO 字段：先取得对应 bank gate，再在修改模式前保存
字段；全部 provider 退出后才恢复，只撤销本次新取得的 gate，并检查回读。恢复失败
保留 Platform 生命周期，允许显式重试，factory 查询不取得或初始化硬件。

[G474 设计](../../docs/design/stm32g474.md)定义 CPU／Flash／RAM 和精确模式。
本 Board 未列的 ADC、I2C、Flash programming、FDCAN 及其他模式没有由此包授权；
SoC 的其他独立软件夹具不自动成为该 PCB 的接线或资格。所有物理验证仍未执行。
