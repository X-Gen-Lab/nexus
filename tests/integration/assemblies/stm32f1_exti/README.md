# F103RB 按键 EXTI 软件装配

`f103rb-button.toml` 为 NUCLEO-F103RB MB1136 C-03、C-04、C-05 的 B1
按键连接建立独立验证。资料中 SB17 接通，R30 外部上拉，B1 按下时 PC13 为低。
实际板卡焊桥与电平需在上电后核对。

应用通过强类型 factory 取得 EXTI 接口，读取原始电平与边沿事件。
此装配使用 EXTI13 及 `EXTI15_10` 向量。启动时拒绝同向量上的外来活动，
停止时只撤回 EXTI13，按键消抖由应用负责。

示例在 PRIMASK 下先查事件，再使用公共 Arch 接口执行一次浅睡眠。已启用的
EXTI13 或 TIM2 溢出可以唤醒 CPU。解除屏蔽后应用才接收 IRQ 产生的事件。
这个软件路径保持 HSI8 和 TIM2 工作，实板唤醒时序及功耗需单独测量。

使用仓库的 `scripts/ci/provider_assemblies.py` 选择本目录 CMake 工程和
`f103rb-button.toml`，复验 ARM 链接、静态预算和接口保留结果。
