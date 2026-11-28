# F103RB 首故障记录

裸机应用显式链接 `Nexus::STM32F103RBFault`，在已知可执行的特权路径调用
`nx_stm32f103rb_fault_capture`。应用提供对齐的独立内部 SRAM，决定异常入口、
故障后操作及复位保留。该接口只读 SCB 并提交记录，传入的 EXC_RETURN 原样记录。
记录格式与 CRC 复用 `Nexus::Fault`。

复位后，由应用在 Platform 启动完成后调用
`nx_reset_record_read(record, nx_reset_cause(), &output)`，分别获取本次启动的
RCC 原因与保留记录。应用负责在 SRAM 初始化排除区中保留记录，首次使用前清零，
完整读取后才按序号确认。故障地址仅在 CFSR 中有相应有效位时提供。

参考使用同一裸机装配链接选中和未选中的两个映像。GoogleTest 覆盖真实采样和
记录代码。物理复位保留、真实异常入口、NMI 抢占时序和实际故障栈容量待实板测量。

[PM0056 Rev 7 §4.4.14](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf)
