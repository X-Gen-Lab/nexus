# STM32F4 SAI 软件链接参考

该目录仅验证精确芯片的静态选编、公共 factory、存储布局查询和完整 Platform 生命周期链接。
它不声明 PCB、codec、外部音频时钟或实板能力，也不包含应用产品策略。有限消费者只查询
布局；连续消费者显式提交并停止自己的 session。ARM 链接不执行这些操作。

| 装配 | 精确芯片 | SAI1 路径 | 数据存储 | 时钟 |
| --- | --- | --- | --- | --- |
| `f446ze-master.toml` | STM32F446ZET6 | A TX、同步 B RX，PE2 MCLK | 24 bit / 32 bit 容器 | System PLLSAI Q12.288MHz，名义48kHz |
| `f446ze-slave.toml` | STM32F446ZET6 | B RX | 32 bit / 32 bit 容器 | 外部 SCK，声明上界3.072MHz；rate0/0 |
| `f479ni-master.toml` | STM32F479NIH6 | B TX | 10 bit / 16 bit 容器 | 共享 System M10、PLLSAI Q12.288MHz，名义48kHz |
| `f479ni-slave.toml` | STM32F479NIH6 | A TX | 16 bit / 16 bit 容器 | 外部 SCK，声明上界3.072MHz；rate0/0 |

原四个配置都只装配一个 SAI1 owner，使用内部名义 HSI16、两路全部有效 slots、裸机和静态内存。
板级目录中的多个 binding 是互斥路线选择，并非同时启用两个独立 SAI1 owner。
主模式仅通过统一 ClockPlan 声明 PLLSAI；驱动与 consumer 不另行配置 RCC。

`consumer.c` 从强类型 factory 获取接口，通过 `nx_audio_port_layout` 得到真实存储顺序、
容器位宽与有效位位置，再交给既有 frame layout 规划器。它没有假设 I2S 的高半字优先顺序。
ARM ELF、方法保留和 Source SDK 消费分别验收；这些证据不等于物理执行或音质资格。

四个连续配置使用独立的 `blocks_consumer.c` 和相同的公共接口：

| 装配 | 路径 | wire／存储 | 调用者资源 |
| --- | --- | --- | --- |
| `f446ze-sai2-master-blocks.toml` | SAI2 A TX／B RX，PE0 MCLK | 2个32-bit slots，mask3，24-bit样本 | TX／RX各3个256B slot |
| `f446ze-sai2-slave-blocks.toml` | SAI2 A TX／B RX | 2个32-bit slots，mask3，32-bit样本 | 同上 |
| `f479ni-sparse-master-blocks.toml` | SAI1 B TX／A RX | 8个16-bit wire slots，mask0x81，仅存2个16-bit容器（10-bit样本） | 同上 |
| `f479ni-sparse-slave-blocks.toml` | SAI1 A TX／B RX | 8个16-bit wire slots，mask0x81，仅存2个16-bit容器 | 同上 |

连续参考的 `max_service_gap_us=100` 是应用必须满足的调度前提。consumer 使用完整
`nx_audio_port_layout`→`nx_frame_layout_audio` 链计算自然存储宽度，配置 stream、
准备绝对 deadline、提交、service 和显式 stop。失败 stop 保留静态 session／buffer
及 Platform；不以一次返回值伪造释放，也不为这个链接夹具增加无限重试或隐藏任务。
正常退出后只回收已经发布的 RX 前缀，再关闭 Platform。

SAI2 使用自己的 IRQ91、APB2 gate、DMA2 stream4/channel3 与 stream6/channel3。
SAI1 保持 IRQ87 及既有请求映射；不能以同属于 DMA2 就共享一个仍活动的 stream。
Board JSON 的新增 binding 只说明已审软件路线，未推断实装模块或 PCB 接线。
