# UTC 质量与 SNTP 测量

`Nexus::WallClock` 用显式 UTC 样本、单调采样时间、有效期和 oscillator drift 上界，
计算当前 UTC 与 uncertainty。它不改变 Core 单调时钟、deadline 或 scheduler 时间。
所有状态由调用者静态持有，更新和查询共享一个串行执行者。

质量区分 UNKNOWN、MEASURED、TRUSTED。没有有效样本、clock rollback、到期和整数
溢出都报告失败，不返回伪造的可信时间。产品审核来源后才能标为 TRUSTED；普通 SNTP
解析结果永远只是 MEASURED，不能直接用它绕过 TLS 对可信 UTC 的要求。

`nx_sntp_parse` 选择 48-byte、NTP v3/v4 server reply profile。它检查 mode、leap、
stratum、原始 request timestamp 与非零 transmit timestamp，明确由调用者选 era 0/1。
调用者还须核对源 IP、端口与唯一未完成请求，提供不可预测 request token 和同域收发
时间。扩展/认证包、NTS、多服务器选举与自动调时不在此解析器中。

返回 UTC 使用 transmit time 加一半本地 RTT；uncertainty 包含 RTT/2、server root
时延/离散度和换算取整，属于单次测量的保守估算。网络非对称、恶意服务器与本地捕获
精度仍由产品的信任和测量模型处理，不是物理精度或安全认证。
