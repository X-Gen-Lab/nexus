# STM32F103RB Flash 选项只读观察

NUCLEO-F103RB 裸机软件参考使用 `poll-halfword` Flash 的既有唯一
owner，以 `maintenance = true` 生成可选观察面。业务从 typed factory 取得
与普通 Flash 同 owner 的端口，调用 `nx_flash_maintenance_snapshot()`。
它读取启动时装载的 RDP、WRP、硬件看门狗和 Stop／Standby 复位选项。

F103RB 的 128 个 1 KiB 擦除页由 32 个低有效 WRP 位控制，每位保护连续四页。
RDP 同时保护前四页。当前公共结构用 32 位逐页位图。当第 32 页或以上受到写保护，
读取返回 `NX_ERROR_UNSUPPORTED` 并保留调用者的原输出。若选项装载比较报错，
读取返回 `NX_ERROR_IO`。固定 POR/PDR 不对应可编程 BOR，`brownout` 报为
`UNKNOWN`。快照反映已加载选项，`power_on_reset_required` 仅描述本会话写入，
该观察面没有 OTP、选项编程、擦除或复位能力。

调用应在特权、未屏蔽中断的 Thread 上与其他 Flash 操作串行，输出存于主 SRAM。
F103C8 及其他器件维持各自普通 Flash 支持；本软件装配只验收 F103RBT6，不操作实板。

依据 [ST PM0075 Rev 2 §2.4、§2.5 Table 9、§3.7 和 §3.8](https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf)和 [DS5319 Rev 20](https://www.st.com/resource/en/datasheet/stm32f103rb.pdf)。
