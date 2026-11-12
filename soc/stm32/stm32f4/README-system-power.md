# F407 System、时钟与监督

本页描述 F407VE／ZG 的静态 C 机制。产品喂狗、复位、任务重启、低压保护和
VBAT 策略由外部工程提供。主机模型与 ARM 构建分别建立软件和链接证据；本轮
physical HIL 为 `not_executed`，不声明晶振精度、功耗或复位后的物理保留资格。

静态配置、资源责任与公共调用边界参见共享图示。

[F4 源码归属与接口边界](README.md)

图表示当前源码和依赖关系；具体模式、关闭证明和时间域规则以下文为准。

## 配置与真实频率

既有 `clock = "hse8-pll168"` 保持 8 MHz HSE、M8/N336/P2/Q7、AHB168、
APB1 42、APB2 84 MHz。新增 `clock = "custom"` 必须完整提供唯一参数表：

```toml
clock = "custom"

[clock_parameters]
source = "hsi"
input_hz = 16000000
pll_m = 16
pll_n = 336
pll_p = 4
pll_q = 7
ahb_div = 1
apb1_div = 4
apb2_div = 2
adc_div = 4
supply_mv = 3300
css = false
```

`sysclk_source` 可显式选 `"input"`，令 SYSCLK 直接采用已声明的 HSE/HSI，
默认 `"pll"` 保持兼容。直接输入且没有 PLL 消费者时，C 从未置 PLLON，计划的
`pll48`/`pll_sysclk` 域为 0（不可用）。共享 PLLM/source 仍静态写入以供独立
PLLI2S owner 使用。需要 USB 内置 FS PHY 或显式 RNG/SDIO 48 MHz 预留时，直接
输入配置另外声明 `pll48 = true`；它要求精确 48 MHz 并建立实际独立 PLL。
USB FS 的 HCLK 必须大于 14.2 MHz，HS engine 必须大于 30 MHz；HS ULPI 使用
显式外部 60 MHz PHY clock，不以主 PLLQ 作为时钟消费条件。
内置 PHY 的 USB48 仅接纳外部 HSE 晶振源；RM0090 §34.3.2 的 ±0.25%
要求不能由 DS8626 Table34 中 HSI 的名义值与 25°C 下 ±1% 误差界替代。

主机工具只计算并验证静态连接和常量，启动、回读、等待及回滚全部留在 C。
输入为 HSI16 或已审 Board 的 HSE4–26 MHz；HSE 必须精确匹配 Board 晶振事实。
不提供外部 bypass 或运行期改频。PLL 输入限 1–2 MHz、VCO 限 192–432 MHz，
SYSCLK/AHB 不超过 168 MHz，APB1/2 不超过 42/84 MHz，ADC 不超过 36 MHz。
所有已使用域必须产生精确整数 Hz，TIM2 必须能整数分频到 1 MHz。AHB 与 APB
分频器按 RM0090 编码；APB 除 1 时 timer 不倍频，其他分频时 timer 为两倍 PCLK。

启动失败或停止时，确认切回 HSI 后的 `SystemCoreClock`／`core_hz` 按实际
`CFGR.HPRE` 计算；配置中的目标 AHB 分频尚未建立时不会假定它已经生效。
源关闭或后续恢复失败仍保留观察到的频率与原清理责任，重试不重放旧时间域。

`ClockPlanIR` 是唯一已选时钟计划。`nexus_config.h` 与
`private/stm32f407_clock.h` 的 `NEXUS_*_HZ`、PLL/CFGR、ADC divider 和 Flash
wait-state 常量由该计划产生；UART/SPI/I2C/CAN、timer、ADC/DAC 与监督机制使用
同一实际频率合同。PLLI2S 与主 PLL 共享 source/M，音频 owner 独占自己的 N/R。
USB/SDIO 的已选 48 MHz 使用者必须具有精确名义 PLL48；具体尚未审核 custom
profile 的 provider 继续显式拒绝。HSI 的名义值不建立 USB 精度或外部同步资格。

Flash 暂只接纳声明 2.7–3.6 V 的供电范围，按每 30 MHz 一档计算 0–5 WS，并先
建立 VOS/Flash 再切换 PLL。`nx_stm32_clock_profile_matches` 只读核对实际
source ready、PLL、总线、CSS、SSCG、所需 Flash latency 和 HSI reset trim。
这些读回不能测量实际电压、晶振误差或温漂。

已选 ADC 另外要求至少 0.6 MHz；DS8626 Table67 的模拟供电条件与最高频率仍须
由具体模拟供电/Board 验收，不把名义时钟算术当作 VDDA 测量。

## 冷启动与内存域

`SystemInit` 在普通 DATA/BSS 之前启用并读回 CP10/CP11，拒绝继承的有效 MPU，
安装 512-byte 对齐的真实 Flash vector，并保持既有 lazy-FP ABI。它不清除外来
RCC 状态来掩盖 warm handoff；普通启动的只读 preflight 仍要求可证明的冷时钟、
独占 TIM2、正确 FPU/MPU/vector。RAM 尚不可用的失败路径仅使用 reset stack
与 Flash 常量并 park，不依赖日志、scheduler 或 libc。

普通 Platform 在取得引脚与时钟前读取实际 Flash 密度与完整 `DBGMCU_IDCODE`。
DEV_ID 必须为 `0x413`，REV_ID 只接纳已审的 `0x1001`、`0x1003`、`0x1007`、
`0x100F`、`0x101F`；未知、缺失及旧 A 标识不按“更新芯片”自动准入。
DEV／密度不符返回 IO，未审 cut 返回 UNSUPPORTED，均不建立平台资源责任。
同一纯只读 C 规则也用于 pre-C 外部 SRAM bootstrap 和受控镜像 handoff：
前者在不可撤销内存／FSMC 设置前拒绝并 park，后者在最终转移前拒绝。
订货号、封装与 PCB 仍由 Board／实物核对；资料白名单和主机模型不表示开发环境
已经观察到某块实板，也不建立物理资格。

| 域 | F407VE／ZG 事实与使用边界 |
| --- | --- |
| 主 SRAM | 128 KiB；普通栈、DATA/BSS，已审 DMA 可以访问 |
| CCM | 64 KiB CPU-only；显式 linker/startup 计划，DMA 始终排除 |
| Backup SRAM | 4 KiB；RTC/backup owner 显式获得 gate、DBP/BRE 和借用 |
| 外部 SRAM | 独立精确 FSMC startup plan；不自动获得 STOP 改时钟资格 |
| Flash | VE 512 KiB／ZG 1 MiB；不隐含产品分区 |

所选 CCM 在第一次写入前先确认 gate 和完整 copy/clear 计划；默认不增加 CCM
初始化。`.noinit` 仅表示不复制和清零。STOP 保持主 SRAM/CCM 内容，Standby
复位并丢失这两域；backup 的有效性、供电和物理保留由显式 RTC/产品策略证明。

## STOP、Standby 与时间域

浅 Sleep 保留运行时钟、TIM2 和现有 tickless 合同。深 STOP 通过可选
`nexus_add_stm32f407_sleep()` 的 `nx_stm32f407_power_enter` 显式执行。
调用者先关闭自己的 ClockDomain，加入所有设备、DMA、deadline 和 OS borrowers，
实际停止周期 SysTick 并处理其旧 pending。接口验证 closed/empty barrier、全 DMA
stream、外设 gate、无 active handler 和精确已使能 wake IRQ 集合后才发生效果。

实际装配先调用 `nx_platform_suspend()` 排空 controller 并释放共享 pins/gates，
保留 System；失败停在原有 prefix，必须继续 service/drain 后重试。成功后再关闭
调用者的 ClockDomain barrier、执行 power enter，并在 RUN 恢复后调用
`nx_platform_resume()` 重建同一静态装配。resume 不调用 system_start、不启动
任务、不重放旧请求；失败反向排空，保留 System 和未关闭 owner。尚在挂起状态时
普通 platform stop 拒绝，须先显式恢复。RTC 使用专用 suspend/resume 保存自主
calendar、aux wake route 与持久 backup effects；活动 AlarmA/内存 loan 会阻止挂起。
不可撤回的 watchdog 或 startup SRAM owner 仍可以明确阻止 STOP。

必须先让完整 Platform suspend 成功，再进入原始 Power 入口；只读到外设 gate 为零
不能证明逻辑借用已经退出。例如 OTG 正在 OFF 状态等待 VBUS 或角色转换时，可能尚未
取得 USB gate，却仍保留旧域 deadline，必须先 close/cancel 并完成排空。ClockDomain
只证明调用者实际登记并排空的借用，不发现遗漏 owner，也不替调用者暂停任务或 OS。

Standby 的 WKUP／RTC 序列遵循 ES0182 Rev19 §2.2.6。`wake_pin = true`
显式选入 PA0 WKUP，入口在所有源禁用时清 WUF，再重开所选 EWUP；返回时恢复原
EWUP。已有 WUF 默认 BUSY，只有 `discard_wakeup = true` 才授权确认它。WKUP
不要求额外 NVIC IRQ；引脚、电气与 VBAT 条件由产品证明。

RTCEN 已开启的 Standby 使用 `nx_stm32f407_standby_enter()`。应用在
Platform.suspend 后先调用同一 RTC owner 的 `standby_prepare`，显式提供允许
丢弃的 RTC 事件 mask。RTC 独占前端禁源、必要的 flags/EXTI PR 确认及 receipt；
Power 只读 validate，在 CWUF 后调用 commit，再执行 WFI，不直接写 RTC/EXTI。
真正 Standby 复位不返回。虚假返回先重建 System epoch，再 restore RTC；新到的
WUTF／EXTI PR 保持可消费。RTC restore 失败保留原 receipt，调用者重试后才能
Platform.resume。PROVISION 的自主备份责任仍须显式 handoff，不能靠普通 close
或一次睡眠删除。

本 drained profile 只允许 EXTI0–15 与独立 RTC 的 IRQ 唤醒。RTC 的
EXTI17/21/22 EMR+RTSR 可通过 `wake_events` 选择一次 WFE；不使用会合并并丢弃
真实边沿的 SEV/WFE/WFE 清空序列。已有 event latch 可以造成立即返回，调用者仍
须重检业务谓词。RTC 必须有仍运行的 LSE/LSI，HSE 派生 RTC 不能充当此 STOP 唤醒。
不清 RTC/EXTI pending，不自动配置 Board wake pin。

进入前记录旧 TIM2 时间，停止 timer/vector 并结束旧 epoch；WFI/WFE 返回后在
PRIMASK 保护下恢复已选 PLL/总线/Flash，重新建立 1 MHz TIM2 并递增
`time_epoch`。收据分开提供 `old_epoch/before_us` 与 `new_epoch/after_us`。
Core 暂停的时间始终未知，不把 timer 归零包装成连续单调时间。恢复失败不返回到
旧 IRQ/deadline 消费者：保留 mask、资源与 storage，观测平台故障并 park。

`nx_stm32f407_deadline_bridge` 只接受匹配的旧 epoch，以及由独立 RTC 提供的
elapsed min/max，输出新域中最早／最晚 deadline 包络。采样窗口、晶振误差、RTC
失效和量化误差必须计入外部界限。它不自动补 kernel tick、恢复请求或重新启动任务。
Standby 必须已有明确 wake source；真实进入后通过复位重建整个 boot。若 WFI
立即返回，机制恢复新 RUN epoch 并报告 IO，不假称已经发生复位。

## EWI、CSS、故障与 trace

WWDG 的基本 owner 仍为原有 6-byte 状态。可选 caller-owned
`nx_stm32f407_wwdg_ewi_t` 在显式激活看门狗之后 arm，产品提供唯一 WWDG vector。
EWI 是 sticky 效果，失败读回或 NVIC enable 不能撤回 storage；IRQ 精确清 EWIF，
累计饱和次数并报告 overflow，不喂狗、不复位、不调用 worker。早／迟喂狗窗口继续
由原 owner 核对真实 counter；EWI 开启后 feed 保留 EWI 配置。

`css = true` 只用于 HSE，显式选择 NMI ownership。CSS/NMI 在确认任何 elapsed
时间之前标记 TIM2 域失败，确认 CSSC 时保留其他 RCC interrupt enable，再保留
全部效果、观测并 park。它不采用降频后继续使用旧 deadline 的恢复策略。

`nx_stm32f407_fault_capture` 使用调用者静态记录及 lock-free 首次发布；嵌套报告
不能覆盖首故障，完整 acquire/read 与 checksum 拒绝未完成或损坏记录。接口不沿
不可信 SP 走栈；fault entry 先根据 EXC_RETURN、FPU extended frame 与 stacking
错误证明 basic frame 可读。默认不预留 RAM、不装载产品 Fault vector、不写 Flash。

复位原因仍是启动捕获的原始 flags 映射。显式 clear 只执行硬件支持的完整 RMVF
确认，不篡改已捕获 cause；显式 reset request 保留 PRIGROUP、提供 AIRCR key，
保存 mask 并等待真实复位。DWT trace 由显式调用开启／关闭，关闭不清共享 TRCENA；
周期计数在 STOP 停止，速率随 HCLK，既不是时间域，也不是一个永久 deadline。

## RNG 的 typed 配置与时钟依赖

通过 `[entropy.random]`、`binding = "rng"`、`mode = "rng-words"` 选择唯一
RNG 内部路线。四个生产 Board 包都声明该无引脚 binding；它不建立外部电路或
熵质量资格。配置期只消费已解析计划的 `pll48.nominal_hz`，缺少精确 48 MHz 时
拒绝生成。`nx_factory_entropy(NX_ENTROPY_ID_RANDOM)` 只返回既有 readonly
face，不启动硬件。生成的 Platform 生命周期直接使用原有私有 C owner 与 ops，
没有第二份实例或时钟 authority；未选择时不编译 RNG driver。

Platform 在 System 成功启动后 prepare RNG，并在关闭或挂起时撤回 RNG enable
与本 owner 取得的 gate；恢复仍使用同一静态 owner，并重新绑定当前 time epoch。
在任何 System 时钟效果前先只读拒绝已有 gate 下的外部 RNGEN／IE；prepare
再次检查 System 运行状态、RCC/Flash 完整计划与实际 PLL48，不修改共享 PLL。
HSE26、HSI 与直接 SYSCLK 共用同一事实；直接输入必须显式预留 `pll48 = true`。

每次读保留官方 RNG 的首字丢弃、连续相同值与 seed/clock 错误检查；失去时钟或
epoch 后不再发布数据，不建立熵池或质量认证。应用在 Platform suspend／stop
之前 join 全部同步 RNG／TLS／网络消费者。关闭失败保留原 owner、时钟依赖与可
重试责任；时钟失败后仍可清理，成功才撤回本 owner 取得的位。外部活动 RNGEN／IRQ
仍会令 System.stop 在任何写入之前拒绝撤时钟。
