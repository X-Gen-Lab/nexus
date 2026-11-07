# Timer 装配的软件验证

F407／GD470 fixture 显式选择已审 silicon pad。Board manifest 是软件合同，不证明
PCB 接线、功率级、inactive 电平或 break 安全资格。

调用者提供 capture ring，拒绝停机时仍保留存储。固件保留真实 factory、冷资源、
共享 IRQ 和 C11 provider；Google 模型只替换外部 clock／pin／硬件边界，使用实际
生成包。模型、ELF 或软件 reset 不资格化物理 timer 时序或安全。
