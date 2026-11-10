# USB ISO 软件装配参考

三个独立 C11 链接配置分别覆盖 VE 的 FS core 最大 IN、ZG 的 ULPI HS DMA，以及
ZG 的 HS core 内置 PHY 最大 FS OUT。它们复用已审核的硅引脚软件 Board，
不授权未知 PCB、PHY、VBUS 或物理 USB 行为。

程序使用强类型 factory、显式静态 request/storage 和公共 packet face。
真实枚举、描述符、非零 endpoint activation、周期调度和 class 业务由外部 owner 提供。
当前协议、FIFO、时钟与退出范围见[USB 合同](../../../docs/design/usb-device.md)。
