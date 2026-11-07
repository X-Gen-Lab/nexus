# U575 保留 RTC 的 Stop2 所有者

`Nexus::U575Stop2` 是精确 STM32U575ZIT6Q、TZEN0、baremetal 可选组件，拥有 RTC
WUT、direct RTC IRQ2、Stop2 power selection、RTC sleep／autonomous gates 和
深等待边界。它使用独立 `Nexus::U575RetainedTime` 日历时钟，不读取 Core TIM2、
不启动 OS、不恢复旧 TIM2 deadlines；应用与产品策略始终外置。

## 身份、资源与 RUN 域

以 `nx_stm32u575_stop2_identity()` 的稳定值作为 `wake_owner` 初始化 retained-time
owner，再用该 clock storage 和显式 RUN consumer capacity 初始化 Stop2 owner。
clock storage 活到两 owner 和全部复制 clock faces 退出。可信静态装配绑定身份，
这不是硬件认证或访问控制。不能同时有 factory RTC／AlarmA controller、其他
calendar／WUT／RTC IRQ writer，调用者串行化两个 owner。

全部 Platform／产品 producer、active devices、IRQ、DMA、observers 和 Core deadline
consumers 显式加入本 owner 的 ClockDomain。关闭 RUN admission，通过真实 API
stop／join，在实际 drain 后逐一 release lease；无 consumers 时传精确 closed
barrier。外来空 domain 不授权进入。独立 RTC clock 活在已停 RUN 域外，没有自动
Platform enrollment。失败 transition／recover 期间 admission 保持关闭；barrier
只能证明已经登记的 owners。

## 进入条件与外部 boot 责任

进入要求 privileged MSP Thread、CONTROL=0、PRIMASK=1、BASEPRI=FAULTMASK=0。
System／Platform 已 stop 且无 residual effects，scheduler 从未启动。检查 TZEN0、
未 swap 的 2 MiB Flash、有效 MSIS4 与相等 buses；其他 high-speed clocks 除可选
MSIK 外均关闭，kernel mux 为 reset HCLK。

排除 active caches、MPU、SysTick、debugger、其他 NVIC／EXTI wake sources、
peripheral／DMA gates 和 SRAM power-down。所有 SRAM 保留，包括 caller stack、
owner 与 retained-clock state。读取 SysTick CTRL 消费 COUNTFLAG。零寄存器不能
证明 watchdog 未运行、NMI／HardFault／debug agents 不存在、peripheral analog
state、其他 masters 或外部 configuration writers；这些由外部装配保证。

boot owner 提供 retained LSE32768 calendar、RTC／PWR access gates、有效 provenance
和 DBP=1。本组件不初始化或更改 calendar、LSE、backup reset、option bytes 或
security attribution。只在自有 WUT 写入周围 unlock WPR，返回前总写 lock key。
要求 SRAM power-down bits 全清、Flash prefetch 关闭，配置 RTC SMEN／AMEN，
通过 direct RTC IRQ2 唤醒，不经过 EXTI19。固定 ARM CMSIS `Ref_cmInstr.txt`
明确允许 PRIMASK-masked IRQ 变 pending 时唤醒 WFI；owner 不 unmask 或调用 RTC handler。

## 计时、WFI 与撤回

调用者申请 nominal 2048 Hz 的 2..65536 个 RTC／16 ticks。观察 WUTWF 后设置
WUTR=ticks-1、WUTOCLR=0。首次 WUTF 在 enable 后 WUTR 到 WUTR+2 counter ticks
之间置位；它是申请区间，不是实测 elapsed time 或物理响应保证。clock 分辨率
256 Hz／3906.25 us，短请求可观察到零 elapsed microseconds。

最大 sample gap 必须覆盖申请睡眠、arm／drain 和真实 wake latency；超过会使 clock
epoch 失效，不伪造连续性。实际 timing／wake latency 需精确设备测量。

WFI 后先清 SLEEPDEEP、验证 MSIS4／Range4 wake profile，随后才访问 RTC。无效
RUN profile 只允许 clear core deep bit 和撤 CPU vector，WUT 与相关效果继续保留，
直到外部已审 owner 恢复 MSIS4；recover 也先验证再访问 RTC。有效 profile 下恢复
进入前 MSIK enable，排空 WUTE／WUTIE、WUTWF、WUTF 和 NVIC，再恢复 WUTR、power、
新增 sleep／autonomous gates。前后时间戳必须属于同一 live generation。

WFI 不能证明物理 Stop2 residency，pending wake 可立即返回；unexpected wake
会报告并 drain。每阶段采用调用者正数有限 observation budget，循环次数不是时间。

## 失败恢复与证据

withdrawal 失败保留 static hardware owner 和 residual-effects flag，禁止 restart
Platform、重开 RUN admission、回收 loan 或 stop retained clock。在同一 masked
boundary 重试 `nx_stm32u575_stop2_recover`，它不再 WFI。RTC sample 已无效时仍能
完成硬件隔离；tainted clock 需独立 joined stop／reopen，不授权复用旧 deadline。
所有效果归零后才重开 RUN 并 restart Platform，建立新 TIM2 epoch。

硬件序列依据 RM0456 Rev7 的 10.7.8／Table104、11.8.1／11.8.4／11.8.44／
11.8.45，以及 63.3.10／63.3.11／63.6.4／63.6.6／63.6.7／63.6.23。
ES0499 Rev12 要求拒绝 active PLL2／PLL3／HSI48／SHSI（2.2.5）、inactive ADC／DAC／ADF
kernel 用 HCLK（2.2.8）、caches off（2.2.11）、无 debug Stop（2.2.19）、全部 SRAM
retained（2.2.22）、Flash prefetch off 并排除其他 Flash masters（2.2.26）。
retained-time admission 另处理 LSE drive／calendar bypass errata。精确 U575 头
只有 EXTI bank1，多器件 RM 中的 bank2 描述不创造不存在的寄存器。

公共头为 `include/nexus/components/stm32u575_stop2.h`；
[完整合同](../../../../docs/design/u575-retained-time.md) 和
[FIG-24](../../../../docs/assets/diagrams/u575-retained-stop2.svg) 标明 ownership 与状态。
本 WUT 组件是 SOC-35 有界增量，不关闭原 SOC-35／IO-30，不实现 DVFS、RTOS tick
补偿、LPDMA／LPBAM、SRAM4-only、通用 TrustZone sleep、Standby 或物理资格。
SRAM4-copy／LPTIM1 是另外显式选择的同 owner 变体，不能混合成更广泛能力承诺。
