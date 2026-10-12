# 软件候选与物理资格

平台机制已经实现，当前 [开发验证记录](verification.md) 说明真实执行的范围。
**最终 clean-source SDK／离线双构建／候选封存与物理 HIL 是独立阶段**，不能因
代码完成、CI 定义或某个 hash 存在就记录为 passed。

| 阶段 | 必需的输入与执行 | 当前记录原则 |
|---|---|---|
| 开发验证 | 实际 compiler、生产 TU 模型、ARM ELF、资源报告、fresh tests | 已执行项记录 raw path；dirty workspace 明确是开发证据 |
| 源码 SDK | clean HEAD、locked dependencies、许可 manifest、移动路径独立 consumer | 必须用当前新包实际 configure/build/run/link |
| 正式软件资格 | source/config/dependency/tool/ELF 同身份；非零 fresh tests 与 budget | `tools/evidence/qualification.py` 不接受空报告、错 scope 或失效证据 |
| 离线复现 | 密封 OCI/toolchain，两个独立 fresh roots，正式 build network=none | `reproduce.py` 真实执行并比较 ELF/BIN；map 保留原始字节；recipe 或摘要不代替执行 |
| 候选封存 | 同一 source/config/artifact 的 qualification、reproduction 与 SDK | `candidate.py`/`proof.py` 验证后封存；不自动创建产品发布 |
| 物理 HIL | 实际 PCB/UID/probe/serial/power/station、同候选 image 与预算 | 当前硬件未接入，全部 MCU 目标 `not_executed` |

HIL fixture 和 station template 位于 `tools/hil/fixtures/`。空 station、错误
Board/PCB/芯片／probe／image/config、失效工具文件或不完整预算会被准入拒绝。
准备工装不等于已经刷写、串口测试、测量波形或完成掉电试验。

本轮新增 ThreadX、USB FS、浅睡眠 timebase、Ethernet、高级 timer、G474 和
user I/O broker，以及 H743／U575 GPIO/UART 参考装配的证据各自绑定独立 source
scope。ThreadX 的真实 Linux 内核
执行不同于 IRQ/libc 边界模型；三板 BASEPRI/PRIMASK ELF 和缺少 external Tick 的
精准负链接不同于 MCU 运行。安装 SDK 的 retention-only 永久任务程序只建立链接；
有限退出的真实 Linux kernel consumer 另外建立运行证据。不能从链接程序的存在
推断已运行 scheduler，也不能从两个 host case 推断完整 MCU backend 资格。

H743/U575 分别固定 HSI64／MSIS4 MHz 和 Cache off；U575 只接纳外部已建立的
TZEN=0／SWAP_BANK=0。模型和 ELF 不会观察实际供电、option bytes 或 silicon
revision。per-engine DMA 可达性 facts 也不建立 DMA provider、cache coherency
或安全域隔离。SDK 必须实际用当前完整 source closure 构建 H743/U575 的公共
consumer；只加入两个 CMSIS dependency 的旧包资格不能继承为新增平台资格。

G474 已有真实 typed factory 主机执行、system/provider 回归和四个显式 TOML
ARM 镜像（empty／selected × 裸机／FreeRTOS）。镜像预算、操作表保留、未选
GPIO／UART 裁剪及实际向量字节分别检查；SysTick 的 170 MHz／1 kHz／reload169999
是编译配置与链接证明，不是时间精度测量。主机 sanitizer 只检查被执行的软件
路径。参考配置的晶振／PCB／焊桥、硬件时序、同最终 clean HEAD 的 SDK／离线
复现与新候选有各自退出条件，不能据此把 G474 的其他外设或 ThreadX 记为支持。

USB 的 packet/clock/factory 模型与软件 Board ELF 不提供 VBUS、connector、PHY、
enumeration 或 flush 周期的物理证明。浅 Sleep 的 1 MHz timer 模型只建立被执行
的量化相位合同，真实 clock/PLL/wake/IRQ/catch-up/功耗要用同一镜像测量。Ethernet
descriptor OWN/loan、capture/break 与 caller memory pin 的软件模型同样不能
建立电气、吞吐、边沿孔径、功率级安全或 MPU/TZ userspace 隔离。公共 API、模式
配置、SoC provider、reviewed Board route、最终候选与物理执行是不同退出条件。

可返回任务只有真实 kernel detach 后才归还存储；prestart discard 只属于已证明
的启动前 scope。scheduler 启动尝试后返回是 retaining fail-stop，不能将内核
返回值解释为从未启动，或撤销可能已经运行的任务、DMA、时钟和外部效果。
停止 I/O 后仍运行的内核继续保留 clock/Tick；产品 reset、设备 safe outputs 和
完整 scheduler-stop port 是外部责任。平台不做自动故障恢复或安全认证声明。

Core 的 `nx_fault_observe` 是无默认状态的 best-effort 观察窄端口。FreeRTOS 和
ThreadX 在 retaining fail-stop 路径提供实际 domain/reason/静态源码位置，然后仍
保留全部借用。故障上下文可能已 masked、ISR、unprivileged 或 stack 损坏，观察者
不能假定工作中的 kernel/clock/transport，也不能释放借用、加 OS lock 或重放
请求。hook 返回仍继续停机；fault log、外部 safe output 与受控 reset 分别验收，
不能由 host death test 推断实板异常处理和安全输出已经运行。

最终候选包应保留准确原始命令、退出码、JUnit／model／预算与 binary 身份，支持
再次校验。给当前源码补一份介绍文档不会继承过去的测试资格；源码或产物变化需
重新执行适用检查。普通 CI 与 source SDK workflow 也不自动完成产品 promotion。

实板接入后优先复核安全输出、clock/reset、UART TC/cancel/RX loss、SPI/I2C、Flash
掉电、watchdog、EXTI/PWM/ADC，再测 cycles/latency/high-water 与长期负载。私有
产品策略、现场部署、密钥与制造许可由外部工程承接。

实施台账的 `implemented` 记录代码与工具已经落入源码；
`qualification_record_authority` 表示实际验收由该源码 HEAD 的外部交付证据决定。
它不在源码中自报 passed。正式执行写入 `nexus-delivery/<HEAD>/`，候选的
`candidate.json`、六类 qualification 和原始报告一起给出最终状态。
这样提交后的验收能绑定准确 HEAD，后续文档提交也不能继承旧资格。
