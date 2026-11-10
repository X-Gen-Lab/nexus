# F407 ADC 公共接口软件夹具

这些装配按 RM0090 Rev22 与 DS8626 Rev12 的片内资源建立，不声明真实 PCB 接线。
`main.c` 通过强类型 factory 获取唯一 ADC123 owner，保留实际 prepare、submit、
service、cancel、stop 与 Platform close 的 ARM 链接路径；镜像不自动烧录或执行。

| 配置 | 模式 |
| --- | --- |
| `ve-dual-regular.toml` | 双 ADC 两 rank 同步，TIM4 CH4、word DMA |
| `zg-triple-regular-freertos.toml` | 三 ADC 同步，TIM8 TRGO、halfword DMA |
| `ve-dual-injected.toml` | 双 ADC 单次硬件注入，TIM3 CH2、无 DMA |
| `zg-triple-injected.toml` | 三 ADC 软件注入，短 JSQR 及 JDR 顺序 |
| `ve-dual-interleaved.toml` | 双 ADC 同输入交错，软件启动有限采集 |
| `zg-triple-interleaved.toml` | 三 ADC 交错，每 frame 六个 halfword |

三 ADC 交错的 frame 为两个 `1,2,3` 周期，以保持 CDR 三个传输单位的完整顺序。
12／10 位采用 mode2 的 word，8／6 位采用 mode3 的 halfword，后者每样本 1 字节；
未承诺交错帧时间或真实吞吐。DMA 只能写已授权的片内 SRAM。注入请求只有一个
frame；硬件触发以独占 timer 的 OPM 防止第二次覆盖。所有普通 ADC 与 ADC123
互斥，Core TIM2 保留，实际触发 timer 与外设共享冲突由同一资源图拒绝。

Host 模型、ARM ELF、Source SDK 和实板验收分别记录。当前实板为 `not_executed`。
