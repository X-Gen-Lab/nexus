# F407 USB Host 软件参考

这些装配是独立的软件 pad 合同。它们不指定实际 PCB、USB 连接器、VBUS 电源开关、
过流输入、PHY reset 或晶振精度；物理 HIL 为 `not_executed`。

| 输入 | 控制器与 PHY | 软件路径 |
|---|---|---|
| `stm32-ve-fs.toml` | OTG_FS，PA11/PA12 AF10 | 官方 HCD，PIO |
| `stm32-zg-hs-fs.toml` | OTG_HS，PB14/PB15 AF12 | 官方 HCD，内置 PHY 全速 PIO |
| `stm32-zg-ulpi-dma.toml` | OTG_HS，十二根 AF10 ULPI pad | 官方 HCD，外部 60 MHz PHY，AHB DMA |

`electrical_handover="external-owner"` 是调用者的明确交接责任：必须在
`nx_platform_start()` 前建立相应 PHY 供电、reset、时钟、VBUS 开关和故障检测，
并保持它们直到 Host stop 和 Platform stop 成功。它不是电气验证或自动供电功能。
实际开关的授权、读回及 VBUS 放电观察可以接到 `Nexus::USBRole` 的显式后端。

公共接口是 `nexus/io/usb_host.h`；厂商 Handle、回调和寄存器布局都保持私有。
factory 只返回固定的 controller identity，channel 是显式硬件资源索引，目标设备地址和
endpoint 地址通过 open 复制。一个 channel 同时借用一个 packet request；没有隐藏
重试、枚举任务、class worker 或运行期 registry。Host 周期轮询和 Chapter 9／class
策略由调用方或显式选入的协议 owner 推进。

IN buffer capacity 至少覆盖完整 MPS 向上取整到四字节，OUT buffer capacity 覆盖
payload 向上取整到四字节。DMA 的零长 OUT 也显式提供四字节可读存储，DMA buffer
必须四字节对齐并完整位于声明的 DMA 可达域；CCM 不可用。请求仍借用到真实通道、
FIFO、DMA 地址和 IRQ 引用全部退出后的 SETTLED。取消和 timeout 走显式全控制器排空，
因此其他已接受 channel 也必须退出；这是共享 PIO FIFO 的保守所有权边界。

HAL 的完成回调只锁存事实。NAK／NYET 以 `NX_ERROR_NACK` 结算，由调用者明确重试。
失败启动、拒绝撤回、迟到 IRQ 与 QUARANTINED 都保留原存储和清理责任。通过
`service`／`stop` 重试观察实际硬件进度；不能通过重置 request 或释放 buffer 撤销借用。

ES0182 Rev19 §2.14.3／§2.14.4 的 Host FIFO 限制按所选 MPS 在 open 时验证。
通道停止不只依赖 CHH 回调，避免 §2.14.5 的早停缺中断造成提前回收。没有接入 hub
split transaction；§2.14.2／§2.15.2 的低速 hub 挂起问题没有软件修复，也不因此宣称
经 hub 的低速连接支持。

`host_main.c` 只验证实际 C11 factory、选编和 ARM 链接；它不会作为上电示例执行，
也不建立 USB 总线、供电、线缆、PHY 或目标设备的物理资格。
