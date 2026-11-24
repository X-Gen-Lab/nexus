# F407 电驱静态装配软件参考

本消费者验证 `injected-power-stage` 的真实 TOML、不可变选项、强类型 factory、
静态构造和 ARM 链接。`main.c` 只取得只读接口并查询、关闭 cold 资源，不启动电机。

软件参考使用 STM32F407ZGT6 的 TIM8 三对互补输出、PA6 BKIN、ADC1 四个顺序
injected ranks，以及 TIM4 PD12/PD13 编码器。前三个 ADC rank 表示 raw 相电流，
第四个表示 raw 母线输入；没有默认零偏、增益、电机或功率板。

当前模式是边沿对齐。四个 ADC rank 不同时采样，TIM4 计数也是 IRQ 中的软件观察，
不能据此声明同步采样孔径或精确转子角。完整输出必须落在声明的 duty 范围；deadtime
换算、模拟稳定窗口、转换、IRQ 与计算预算都需要符合所选模式。

`board/board.json` 明确标记为软件参考，没有已审 PCB、功率级接线或电气安全声明。
模型、配置与链接分别建立软件事实；物理采样、输出、保护、转矩和时延仍未执行。
