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
