# STM32F103RBT6 保留 RTC 与备份字的软件参考

`rb.toml` 选择 baremetal、HSI8 和 `rtc:backup-adopt`。需要已运行的
32.768 kHz LSE 晶体及已配置的 RTC。该参考声明名义晶体频率，
实际 PCB、晶体、VBAT、分频、计数率和保持行为未测量。

平台启动占用 RTC、BKP、PWR 控制和 PC14/PC15 晶体路由。PVD 也占用
PWR 控制，配置同时选择两者会在生成前失败。初始化只重新同步 APB，
RTC 分频、计数、闹钟配置与十个 16 位数据字保持原样。

应用通过 `nx_factory_rtc_ref(NX_RTC_REF_BACKUP)` 取公共接口，
用 `nx_rtc_port_backup_read` 从索引 0–9 读取单个字并零扩展至 `uint32_t`，用
`nx_rtc_port_stop` 或平台停止释放 APB 门。计数保持 provider 私有，
日历、写入、闹钟和唤醒返回 `NX_ERROR_UNSUPPORTED`。

[ST 参考手册 RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)

## 私有原始计数闹钟

在 GNU Arm 构建中选 `-DNEXUS_F1_RAW_ALARM_PROBE=ON` 编译并链接
`nexus_f103_rtc_alarm`。此固定测试工程独占 RTC、BKP、PWR 和 EXTI17，
根据 RM0008 §18.3.5 在观测到计数推进后更新比较值。它使用专用
`RTC_Alarm_IRQHandler`，并保持 RTC 全局中断禁用。

闹钟目标用原始 32 位计数表达。设置时要求至少四个计数的提前量、
距离小于半个计数范围，并排查编程期间已越过目标的情况。触发后分两步
回收。专用 ISR 屏蔽自身，前台核对 RTC 和 EXTI17 均已挂起，再按顺序
确认事件并撤除路由。

比较寄存器只写。成功更新后的值会留在 RTC 中，停止只解除 IRQ 交付。
测试工程保留分频、计数、备份数据和其他 EXTI 线。它要求已配置的 LSE
RTC、空闲闹钟路径和运行中的 TIM2 单调时钟。固件运行预算按该测试
工程设定，平台接口仍暴露 `backup-adopt`，物理触发时序和保持未测量。
