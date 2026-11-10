# ST Host Core 软件链接参考

三个 TOML 分别选中 FS 内置 PHY、HS 内置 FS PHY、HS 外部 ULPI/DMA。
`components=["st-usb-host"]` 只接入已锁定的官方 Host Core。私有集成 TU
提供静态 slots、DMA 可见 staging、有限期限、VBUS owner 和精确 restart 回调；
USBH Handle 与 ST class 记录不进入公共 Nexus I/O 接口。

本参考仅作 C11 ARM 链接、依赖与预算证据。`source_power` 有意拒绝启用
没有 PCB 依据的供电开关，退出前完整排空 HCD 和 Platform。实际产品必须替换为
经其板级事实授权、可读回的 VBUS/过流实现，并提供外部 PHY 电源/60 MHz
handover。USB 描述符、类策略、连接测试和物理 HIL 均不由此 ELF 证明。

Host Core 由调用者显式 `step`，不启动任务、分配内存或安装 SysTick。
未经选择的 Device middleware 与全部类驱动不进入该参考的编译路径。
