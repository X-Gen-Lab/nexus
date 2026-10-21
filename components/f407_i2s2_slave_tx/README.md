# Finite F407 I2S2 slave transmission

This draft targets one exact chip reference: STM32F407VE/ZG, baremetal,
HSE8/PLL168 with the live Platform TIM2 clock, SPI2 in Philips I2S slave TX,
two 16-bit channels, and finite normal DMA1 Stream4 channel0. It does not select
PLLI2S or a continuous double-buffer engine. Development execution has passed
35 external-MMIO GoogleTest/GoogleMock cases and focused independent source
review. Normal Cortex-M4 public-consumer and installed-SDK validation remain
pending; no Board audio wiring, codec or physical execution is qualified.

The authored `[i2s2]` hardware role selects only
`profile="slave-tx-stereo16-pb12-pb13-pb15"`. It contributes the physical SPI2,
pad, DMA-engine and vector claims to the same resource plan as other controllers.
The optional `Nexus::F407I2S2SlaveTx` CMake target owns its software dependencies
and exists only for that validated role. These are DS8626 chip AF5 signals,
not a new Board/PCB route.

Supply one caller-owned immutable face, zero storage and a complete contiguous
stereo16 frame loan. `submit` SUCCESS means **accepted**. The source is borrowed
before DMA can prefetch. Only acquire-observed request ACTIVE means **armed**:
one DR halfword, exact `NDTR=N-1`, `TXE=0`, enabled I2S and at least three real
1 MHz TIM2 ticks after enable readback have been observed. A poll count never
establishes elapsed time. The caller holds the external master CK idle and WS
HIGH throughout setup; IDR reads only sample instants. The master then supplies
the declared complete Philips frames, including the one-clock WS lead, and stops
before an absent word. Actual clock rate, electrical timing and DMA service
margin remain external responsibilities.

TC and zero NDTR are memory-count observations. Software stream withdrawal can
also set TC. Every SR read preserves UDR/FRE in the first-error latch before
those read-clear flags disappear. Submission first accepts the loan before its
first destructive SR read; an already-set error becomes an accepted failure,
never a rejection that consumes a flag. Only observed DMA EN0, TXE1 then a fresh BSY0,
I2SE withdrawal and vector/address detachment permit the original frame release
and request SETTLED. Success reports the complete block; errors report zero
serialized bytes. Cancellation after prefill with CK idle leaves a DR word:
retain QUARANTINED storage until coordinated drain clocks allow final access.
That error drain may underrun or produce partial/invalid audio; it does not
promise a complete stereo frame. The absolute deadline covers wire completion
and final address/vector withdrawal, and remains active while draining.
There is no timeout or reset-looking-register release shortcut.

Selected normal Platform startup holds the shared GPIOB/DMA1 gates under its
cold-reset, no-foreign-running-master contract. Initialization requires those
gates already on; this provider neither opens nor withdraws them and never
resets the whole DMA controller. Clock-gating alone does not prove that an
unknown retained stream is reset. Stop and join all shared-domain consumers,
including external HAL users, before Platform stop withdraws its added gates.

The caller joins this owner before Platform stop/restart and excludes concurrent
reset, clock, pin, DMA, IRQ and debugger owners. Observed reset or clock-history
loss during a live loan permanently retains that loan. Request settlement and
provider pad/gate cleanup are separate: provider storage lasts through successful
`stop`. See the public header for context, bounds and initialization cleanup.

## 中文合同

本草案仅覆盖 STM32F407VE/ZG、裸机、HSE8/PLL168 与存活的 Platform TIM2 时间域：
SPI2 作为 Philips I2S 双声道 16 位从发送器，使用 DMA1 Stream4 channel0 的有限普通
传输。不选择 PLLI2S 或连续双缓冲。开发验证已通过 35 个外部 MMIO 模型
GoogleTest/GoogleMock 用例和针对性独立源码审查。正常 Cortex-M4 公开消费者与
已安装 SDK 验证仍待执行；未建立板卡音频接线、编解码器或实物执行资格。

`[i2s2]` 只允许固定 `profile="slave-tx-stereo16-pb12-pb13-pb15"`，将物理 SPI2、
引脚、DMA 引擎和向量加入同一资源计划；CMake 可选目标拥有软件依赖。这是芯片 AF5
能力参考，不代表新增 PCB 路线。

`submit` 成功仅表示已接纳，DMA 可能预取前即借用完整 frame。只有 acquire 读取到
ACTIVE 才表示已就绪：实际观察到一份 DR 半字、`NDTR=N-1`、`TXE=0`、I2S 使能及
使能回读后三个真实 TIM2 tick。轮询次数不能充当时间。调用者须在此之前持续保持外部
CK 静止且 WS 为高；IDR 只能采样瞬时状态。外部 master 随后发送声明的完整 Philips
帧并保留 WS 提前一个时钟的相位，不能进入缺少数据的下一字。实际频率、电气时序和
DMA 服务余量由外部负责。

TC/NDTR0 不代表线上完成；软件停流也可产生 TC。每次 SR 读取必须先保留 UDR/FRE，
不能因读清除而丢失首个故障。submit 先接纳贷款再做第一次会读清标志的 SR 观察；
已置位的故障属于接纳后的失败，不能返回普通拒绝却消费标志。只有 DMA EN0、TXE1 后新观察到 BSY0、I2SE 关闭及
向量/地址解绑全部成立，才归还原 frame 并发布 SETTLED。成功报告完整块；错误报告
零个已串行发送字节。预填后取消且 CK 静止时，DR 中仍有数据，必须保留 QUARANTINED
贷款，直到协调的排空时钟允许最终访问结束。超时或看似复位的寄存器都不能替代证明。
错误排空可能欠载或产生不完整、无效音频，不承诺完整双声道帧。绝对截止时间覆盖
线端完成及最终地址、向量撤回，在排空期间仍然有效。

选中的正常 Platform 启动依据冷复位、无其它运行中总线主设备的合同，持有共享
GPIOB/DMA1 门控。初始化要求这些门控已开启；本 provider 不开关共享门控，也不
复位整个 DMA 控制器。仅凭关门不能证明未知保留 stream 已复位。Platform 撤回
其新增门控前，须停止并会合所有共享域消费者，包括外部 HAL 用户。

调用者在 Platform 停止或重启前完成本 owner 的停止和会合，并排除其它复位、变频、
引脚、DMA、IRQ 与调试写入者。贷款存活时观察到复位或时钟历史丢失，将永久保留贷款。
请求结清与 provider 的引脚/门控归还各有生命周期；后者持续到 `stop` 成功。
