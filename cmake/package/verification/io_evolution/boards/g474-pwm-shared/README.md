# G474 共享 TIM3 软件参考

本参考为 `shared-fixed-pwm` 声明 STM32G474RET6 PA6／TIM3_CH1 与
PA7／TIM3_CH2 AF2。两个 PWM 接口共用一个物理 TIM3 owner 和固定时基。
第二通道加入当前相位，不承诺同时起动或完整首脉冲。

Board 声明 24 MHz HSE、PA6 inactive-low、PA7 inactive-high，仅为软件测试输入，
不建立 PCB、connector、负载、电气安全、时钟测量或 HIL 结果。

唯一普通输入为 [`g474-pwm-shared.toml`](../../g474-pwm-shared.toml)。静态路线、
provider 行为、链接成本和物理资格各有独立证据边界。
