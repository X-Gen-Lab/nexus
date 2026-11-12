# STM32F4 SAI1 软件链接参考

该目录仅验证精确芯片的静态选编、公共 factory、存储布局查询和完整 Platform 生命周期链接。
它不声明 PCB、codec、外部音频时钟或实板能力，不启动音频 DMA，也不包含应用产品策略。

| 装配 | 精确芯片 | SAI1 路径 | 数据存储 | 时钟 |
| --- | --- | --- | --- | --- |
| `f446ze-master.toml` | STM32F446ZET6 | A TX、同步 B RX，PE2 MCLK | 24 bit / 32 bit 容器 | System PLLSAI Q12.288MHz，名义48kHz |
| `f446ze-slave.toml` | STM32F446ZET6 | B RX | 32 bit / 32 bit 容器 | 外部 SCK，声明上界3.072MHz；rate0/0 |
| `f479ni-master.toml` | STM32F479NIH6 | B TX | 10 bit / 16 bit 容器 | 共享 System M10、PLLSAI Q12.288MHz，名义48kHz |
| `f479ni-slave.toml` | STM32F479NIH6 | A TX | 16 bit / 16 bit 容器 | 外部 SCK，声明上界3.072MHz；rate0/0 |

四个配置都只装配一个 SAI1 owner，使用内部名义 HSI16、两路全部有效 slots、裸机和静态内存。
板级目录中的多个 binding 是互斥路线选择，并非同时启用两个独立 SAI1 owner。
主模式仅通过统一 ClockPlan 声明 PLLSAI；驱动与 consumer 不另行配置 RCC。

`consumer.c` 从强类型 factory 获取接口，通过 `nx_audio_port_layout` 得到真实存储顺序、
容器位宽与有效位位置，再交给既有 frame layout 规划器。它没有假设 I2S 的高半字优先顺序。
ARM ELF、方法保留和 Source SDK 消费分别验收；这些证据不等于物理执行或音质资格。
