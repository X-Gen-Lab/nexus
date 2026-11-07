# 有界 GPIO OneWire 总线

`Nexus::OneWire` 提供调用者持有的标准速率静态总线。Board／assembly 提供已经
配置的开漏 GPIO output、外部上拉、真实输入回读和有效单调 clock。写一表示释放
线路；组件不改变 mode、不创建 task／queue／allocator／设备注册表或全局 IRQ mask。

## 事务与时序

`nx_onewire_exchange` 每次从新 reset／presence 开始，按 LSB-first 发送 1～16 字节，
接收 0～128 位；最后接收字节未用 bit 补零。每次返回都结束 CPU buffer 访问，失败
可能改变 partial receive bytes。span 与已知 owner／config／face／ops 分离；完整
opaque GPIO／clock context 范围由装配保证。

每次等待同时受有限绝对 deadline 和有限 poll count 限制。下降沿前后都读 clock：
最小 pulse 从写后观察计时，最大 pulse 从写前观察计时。读取采样在严格的已观察
14 µs 内完成，slot／recovery 最小值也包含边沿后观察。GPIO／clock callback 延迟
或抢占导致错误，不报告虚假的时序成功。

500 µs reset-low target 与 960 µs 软件 ceiling 是保守参考；960 µs 不是外部 VDD
数据手册最大值。真实 pull-up RC、clock 不确定度、波形和 WCET 仍需测量，host mock
不建立物理时序或寄生供电能力。

## 失败和清理

同一串行 task 拥有调用与配置。owner 未关闭前，不改变 clock 域、pin 或 pull-up。
同步失败即使已超过 transfer deadline，仍尝试一次立即 release／readback；primary
和 cleanup 结果分开。withdraw 失败保留 wire binding、fault 和 GPIO 生命周期，但
不保留 CPU payload 指针。释放外层 GPIO／config 前，用新的有限预算调用
`nx_onewire_stop` 并验证结果。

faulted stop 从首次观察到物理高及随后 clock 起，保留现有开漏／clock binding
500 µs，再验证 high 和 deadline；中止 reset 可能引出迟到 presence pulse。预算
太短或 poll 不足继续保留 binding，clean stop 不增加此等待。

回退或非有限 clock 永久使该 epoch 故障。stop 可尝试立即电气释放，不能宣称该失效
epoch 已 clean close，需外部 reset 或单独审核的 handoff；新数值 deadline 不会修复。

## 组合边界

[DS18B20](../ds18b20/README.md)在此总线上实现器件协议，不跨无关事务使用旧
read-bit 转换上下文。未提供 overdrive、ROM search、多主机、strong pull-up 或
parasite supply。来源身份见 `references.json`，物理 HIL 为 `not_executed`。
