# 显式时间的固定相位周期释放

`Nexus::Periodic` 只计算到期释放，不执行回调、不创建 worker、队列或 scheduler。
唯一执行者提供 Core `nx_time_us_t`，可以来自注入 `nx_clock_t`；不强制依赖
ClockDomain。时钟 stop、restart 或换源之后，owner 等待退出旧使用者并显式 reset。
数值相同的时间戳不能证明仍处于同一个 epoch。

skip 模式报告最近一次应到期的释放及被跳过数量；catch-up 模式最多报告调用者的
本次预算，将未处理历史保留到下一次调用。相位始终从上一计划截止时间推进整周期，
不把执行完成时间作为新的相位。绝对时间不能表示下一次有限截止时间时拒绝，不回绕。

## 复用已有 owner 和预算

周期释放后由已有唯一 executor 调用一次 `nx_owner_observer_service` 或既有 service。
不要再包一个总线 worker、请求队列或取消协议。请求准入前使用
`nx_deadline_finite_after` 建立一次绝对事务预算；排队和硬件执行共享它，周期 tick
不能为排队请求刷新预算。周期策略也不自动取消或释放已接受请求。

观察接入已有 OwnerObserver：它复制调用、BUSY、错误、隔离和最大 service 间隔；
诊断错误不能阻止必须继续的 drain。连续数据预算复用 `nx_stream_budget_calculate`
及原有 queue／stream 存储，不在周期组件复制容量规划。阈值、丢期处置和告警属于调用方。

`common_runtime_test.cpp` 验证相位与时域；`common_owner_integration_test.cpp`
将实际周期、BusOwner 和 OwnerObserver 串联，检查排队已用尽的原预算在接触硬件前
结清。测试不是实际线程、MCU 时延或吞吐测量。
