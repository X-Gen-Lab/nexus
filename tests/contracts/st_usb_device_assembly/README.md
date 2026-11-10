# Device Core ARM 消费夹具

此处只建立软件编译、静态接线、链接和 ELF 预算证据，不执行真实设备。
三个显式 TOML 分别选择 VE FS PIO、ZG HS-core/internal-FS DMA，以及 ZG ULPI HS DMA。
复用已有软件 Board facts，不声称有实际 VBUS、PHY reset、参考时钟或 PCB 接线资格。
私有回调和占位描述符只为保留真实官方 Core 源码及唯一 DCD owner；没有产品 VID/PID、
class 业务协议、堆、隐藏线程或自动角色策略。全部 state/config/slot/staging 使用静态
存储，关闭失败仍保持有效。
