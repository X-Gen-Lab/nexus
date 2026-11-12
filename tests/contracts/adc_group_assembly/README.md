# F407 ADC 公共接口软件夹具

这些装配按 RM0090 Rev22 与 DS8626 Rev12 的片内资源建立，不声明真实 PCB 接线。
`main.c` 通过强类型 factory 获取唯一 ADC123 owner，显式调用公共 info、raw／paired
prepare、submit、blocks、observer、service、cancel、stop 与 Platform close。
同一主函数按公开 `injected_info.format.trigger == NX_ADC_TRIGGER_EVENT` 选择有限
JAUTO；仅在 blocks 准入纯返回 UNSUPPORTED 时回退到原有限接口。不维护另一份模式
配置表，接纳后不切换请求视图。镜像只用于实际 ARM 编译／链接，不自动烧录或执行。

| 配置 | 模式 |
| --- | --- |
| `ve-dual-regular.toml` | 双 ADC 两 rank 同步，TIM4 CH4、word DMA |
| `zg-triple-regular-freertos.toml` | 三 ADC 同步，TIM8 TRGO、halfword DMA |
| `ve-dual-injected.toml` | 双 ADC 单次硬件注入，TIM3 CH2、无 DMA |
| `zg-triple-injected.toml` | 三 ADC 软件注入，短 JSQR 及 JDR 顺序 |
| `ve-dual-interleaved.toml` | 双 ADC 同输入交错，软件启动有限采集 |
| `zg-triple-interleaved.toml` | 三 ADC 交错，每 frame 六个 halfword |
| `ve-dual-blocks.toml` | 双 ADC 同步 word DBM、三个调用者 slot 与 observer |
| `zg-triple-timer-injected-blocks.toml` | 三 ADC halfword DBM，共用 owner；规则 TIM8 TRGO／注入 TIM3 CH2、独立 APB 时钟 |
| `ve-dual-auto-injected.toml` | 一次 dual regular→JAUTO，显式双结果 descriptor、1 µs one-pulse 延迟 |
| `zg-triple-interleaved-blocks.toml` | 三 ADC 8 位 mode3 DBM，halfword DMA、完整六样本 frame |


三 ADC 交错的 frame 为两个 `1,2,3` 周期，以保持 CDR 三个传输单位的完整顺序。
12／10 位采用 mode2 的 word，8／6 位采用 mode3 的 halfword，后者每样本 1 字节；
未承诺交错帧时间或真实吞吐。DMA 只能写已授权的片内 SRAM。注入请求只有一个
frame；硬件触发以独占 timer 的 OPM 防止第二次覆盖。所有普通 ADC 与 ADC123
互斥，Core TIM2 保留，实际触发 timer 与外设共享冲突由同一资源图拒绝。

循环消费者显式提供三个 768 B 静态 bank 与三个 observer 记录；这是测试消费者的
存储，不是平台默认 buffer。所有 bank 都是完整 frame 的整数倍，所选服务预算由
真实 provider 校验。主函数显式推进到一个完整 consumer block 并核对其 observation；
combined 先提交一个真实注入 request，paired 模式联合借用两个独立输出 span。
成功必须观察 SETTLED／有效进度；失败不冒充完成，所有数据保持静态有效直到 stop
实际 join。请求等待与停止重试均使用有限 Core deadline。

Host 模型、ARM ELF、Source SDK 和实板验收分别记录。每份链接证据应核对生成的
配置、实际 CMake 编译条目、ELF 保留的方法表／provider 符号与资源；编译本目录
不等于已经执行 ARM 镜像，也不改动 SDK 消费计划。当前实板为 `not_executed`。
