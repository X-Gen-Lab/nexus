# F407 FreeRTOS STOP 冷态链接契约

本消费者使用真实 TOML、强类型 RTC factory、所选 FreeRTOS 时间基准、唯一 Idle
入口和 F4 STOP OBJECT 适配器，验证 API、编译及链接。它不启动 scheduler，不执行
实板 STOP，不预置日历或声称已存在保留 RTC。`main.c` 的零 Idle 窗口不能停 tick，
随后无有效 suspended RTC 的 cold arm 必须被拒绝。

Board 声明是 PC14 外部 32768 Hz LSE 软件参考；电源、频率误差和实板未知。
`drift_ppm = 100` 与 `max_elapsed_us = 200000` 是链接输入中的示范约束，产品必须
根据实际外部时钟、唤醒和恢复上界重新审核，不能直接当作物理事实。

运行链路和失败责任见 [System README](../../../../soc/stm32/stm32f4/system/README.md)。
产品必须先排空用户、借用与旧 deadline，暂停 Platform、关闭正确 ClockDomain，
配置唯一 125 ms RTC 事件，再明确 arm 一次。默认浅睡、Core epoch 与保守时间迟滞
不能混为同一连续计时合同。
