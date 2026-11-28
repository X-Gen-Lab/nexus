# STM32F103RB TIM3 单脉冲软件参考

`f103rb-tim3.toml` 使用 HSI8 和 TIM3 默认复用的 PA6／CH1。1 MHz 的定时器
时基产生一个周期，输出在前沿延迟后为高，计数器回卷时由 PWM2 自动回到低电平。
宽度 0 使用强制无效输出，完整周期宽度拒绝。示例选 1000 个时基 tick 和 250 个
高电平 tick，通过 typed factory 取得 `nx_timer_pulse_port_t`。依次调用
`nx_request_prepare`、`submit`、`service`，待请求结清后读取结果并关闭 Platform。

软件完成要求观察回卷及计数停止，再撤回 IRQ 并将 PA6 停放在低电平 GPIO。
超时、取消和失败停止均通过原请求报告，只有撤回完成才释放请求和宽度数组。
依据为 DS5319 Rev20 表 5、RM0008 Rev21 §15.3.10、ES096 Rev15 §2.6.3／§2.6.4，
以及 UM1724 Rev17 的 MB1136 接线。实板、负载、波形和时序待硬件验证。
