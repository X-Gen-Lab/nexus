# Kernel profile 链接合同

CPU-only consumer 经一份 runtime TOML `[os]` 与 `nexus_add_runtime` 保留生产
scheduler、exception port、可选 Tick／counter，不另建 source graph。

`NEXUS_TEST_EXTERNAL_TICK_PROVIDER`、`NEXUS_TEST_RUNTIME_COUNTER_PROVIDER`
只选择明确合成符号，是 fixture control，不是平台覆盖项。启用 external Tick
却省 provider 必须链接失败，不能悄用 port weak SysTick；runtime stats 同样要求
两个 counter 符号。成功 ELF 保留调用者，external Tick 的
`vPortSetupTimerInterrupt` 必须 strong；关闭特性不应要求符号。

linker、clock 和 provider body 均为软件夹具，不证明真实 timer 频率、IRQ 优先级、
低功耗或 scheduler 运行。
