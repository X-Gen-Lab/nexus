# USB FS device 包所有权与接入边界

公共接口是 `nexus/io/usb_device.h`。当前纵切片提供 Native、STM32F407 和
GD32F470 的 internal-PHY full-speed device PIO 机制及实际 SDK 绑定。维护范围是
EP0 control、显式选择的 EP1–EP3 bulk IN/OUT、SETUP/reset/suspend/resume 事实、
STALL、SET_ADDRESS、取消与退出。DCD 的一次 request 仍是一个不超过 MPS 的包。
可选 `Nexus::USBControl` 组合 EP0 多包 DATA、必要 ZLP 与 STATUS；独立的
`Nexus::USBDescriptors` 校验外部描述符结构。两者不创建任务或分配内存。
具体 class 分派、描述符字节与查找、VID/PID、字符串、配置及恢复策略由外部工程提供；
没有具体 CDC/HID 类栈，也不增加 Host、HS、DMA、ISO 或 remote wake 支持。

## 对象、资源与时钟

controller 和 endpoint 保持独立 identity；公共 face 只有 readonly ops 与 context。
装配拥有一个 controller、精确 endpoint 数组、精确事件环和一个显式 48 MHz clock
owner。实例共享只读方法表，没有全局可变 registry、endpoint 最大池、隐藏 worker
或必选 OS。未选实例不产生状态，MCU 热路径用 direct volatile MMIO。模型编译只
替换外部 MMIO/NVIC 叶端口，MCU 不携带模型函数表或运行时模拟分支。

两款 SoC 的 USBFS 基址都是 `0x50000000`，四个 IN / 四个 OUT 方向、320-word FIFO，
单一 IRQ 67；绑定用锁定 SDK 验证实际 offset、stride、容量和 W1C/只读位。
EP0 固定 64-byte MPS；bulk MPS 只能是 8/16/32/64。RX FIFO 至少 64 words；每个
被选 IN FIFO 至少 16 words，分区先验证无重叠且总计不超过 320 words。

| SoC | 实际 48 MHz 来源 | 接入行为 |
|---|---|---|
| STM32F407 | HSE 8 MHz / M8 × N336 / Q7 | 验证现有 PLL/ready，只管理 USBFS gate |
| GD32F470 | HXTAL 25 MHz / M25 × PLLSAI N192 / P4 | 独占闲置 PLLSAI 与 CK48 mux 后有界启动 |

GD32 原 200 MHz 主 PLL 的 Q9 输出不是 48 MHz，不能直接用于 USB。GD32 clock owner
拒绝新活跃 USBHS/TRNG/SDIO/TLI 消费者，保留旁邻字段并从未修改 SYSCLK。clock owner
有 IDLE/CLEANUP/READY 与精确 consumer count；失败回滚缺少读回证明时仍保留 CLEANUP
责任。DCD 先 validate/retain，退出完成后 release consumer，最后由外部 owner stop
时钟。pin/electrical/VBUS 和所有 clock-gated 隐藏消费者必须由外部资源 owner 先
授权并静止，不能仅凭 gate 为零推断系统独占。

ARM GCC 14.3 的实际 sizeof probe 为：controller 144 B、每个 endpoint 40 B、
每个 event 40 B、request 56 B，STM/GD clock state 分别 8/16 B。生产单元的 ARM
objects 无全局 `.data`/`.bss`；完整 ELF 的未选 GC/资源变化由实际装配构建另行证明。
这些既有 DCD 尺寸不包含可选 ROM profile、EP0 adapter 或调用者的 outer request；
新增组件的 Flash／RAM 成本需以同源 ARM ELF 与实际 ABI 分别确认。

## 选定 ROM profile 与描述符

`nexus/io/usb_profile.h` 的只读 profile 绑定既有 controller／endpoint face，以及
选定的方向、类型、MPS、FIFO、48 MHz、internal PHY、FS device 与 PIO 事实。
只有 USB 被选入且 `factory.capabilities = true` 时，生成器才产生 profile、精确
endpoint 数组与 `nx_factory_usb_profile` getter。设为 `false` 时不生成这些对象
或 getter；普通 USB face、方法表、provider context 与既有 generic capability
类型不因此扩大。getter 不初始化硬件，也不发现新 endpoint。

`nx_usb_profile_validate` 只读检查选定事实，不能证明实际时钟、PCB、VBUS 或枚举。
外部 descriptor 通过 `nx_usb_descriptor_view_t` 传给
`nx_usb_descriptor_validate`：device、configuration、string 必须完整；校验
length／total、EP0 MPS、唯一 interface、alternate 0，以及 bulk endpoint 的
方向／编号／MPS 与选定 profile 的对应关系。class-specific 子结构只检查边界，
不解码其类语义；其他传输类型、alternate setting 等扩展不由本参考接入。

描述符校验按给定完整长度扫描，不读取硬件、不复制或保留 descriptor；通过只表示
结构合法。选择哪段描述符、处理何种请求及 class 内容仍属外部策略。发送后，
descriptor／reply 字节必须保持有效且不变，直到 outer request 的 acquire SETTLED。

profile、endpoint 数组、face／ops、adapter、request 和 view／snapshot 元数据
须按其 C 类型自然对齐，地址范围不能回绕；非法对齐在 typed 访问前拒绝。
descriptor bytes 只需 byte alignment。数值检查不证明任意地址或 opaque 私有
context 可访问；调用者仍须提供合法且保持有效的可读／可写存储。

## 包完成与控制 epoch

prepare 不访问 payload；拒绝 admission 不保留新引用。ACCEPTED 借用 request 与
payload，直到 acquire-observed SETTLED。所有 request、payload、输出、count、
immutable clock ops 和 wake target 均检查与 controller/endpoint/ring/clock state
及已有 loan 的重叠。拒绝 state/read_events 输出别名时输出保持原值。

IN FIFO 写入不是完成，只有 XFRC、EP enable 已清、FIFO 空闲容量恢复并且观察到的
W1C completion 清除读回成功才发布 SUCCESS。OUT 必须同时读到包数据和 XFRC，
short packet 正常完成；越界包的所有 FIFO words 都被消费，最多复制 caller capacity，
之后报告 OVERFLOW 并排空，不越界写 caller buffer。

IN TXFE 是只读状态，不参与 W1C 清除/完成的零值证明。ITTXFE 是主机 IN token 在
FIFO 尚空时到达的可重试信息，OUT STUP 是正常 SETUP phase 事实；两者不错误地终止
包。IN control timeout 和 OUT FIFO overrun 使用对应方向的真实事件位。EP0 的
DATA PID 由 SETUP/core 管理：bits 29:28/16 对 EP0 是 reserved，软件仅对 bulk
endpoint 使用 DATA0 命令。

SETUP 严格解码八个 little-endian bytes，并产生单调递增 epoch。新 SETUP/reset
使旧 control loan 进入排空，不能仅改 epoch 就撤销借用。reset 与 queued SETUP
在同一 snapshot 中没有先后证明，先前 queued SETUP 被丢弃；复位排空完成后才允许
新 admission。SET_ADDRESS 验证当前标准 SETUP 的各字段，先 staged，只有对应
status-IN 实际 XFRC 后写 DCFG 地址。地址读回失败关闭 controller。

事件环保存 value facts，满时不覆盖旧事件；精确软件 loss marker 排在原队列后。
最新 SETUP/epoch/closing 状态不依赖事件环空间。时间戳是 IRQ acquisition time，
不是 USB 总线采样时刻；reset 批次中的无序旧 SETUP 不计作新的已接收 control。

## 可选 EP0 packetizer 与外层借用

`nexus/components/usb_control.h` 使用调用者零初始化的 `nx_usb_control_t`，内部
只有两个固定的 packet request 槽；outer `nx_usb_control_request_t` 与 payload
也由调用者提供。profile／face 的生命周期覆盖整个绑定的 adapter。应用独占 EP0
packet 使用权；event ring 消费、class 分派及 controller 的恢复／停止仍由外部
owner 管理。现有 [FIG-16 I/O 装配与执行图](../assets/diagrams/README.md) 的
face→provider 是包层调用；EP0 组件由应用执行者显式推进，不增加生命周期 owner。

prepare 同样仅允许 Task 调用；其余 adapter API 还要求同一个序列化特权 Task
执行，incoming IRQ masks 须 clear；不允许第二 Task、ISR 或 callback 重入。
initialize 不启动硬件；prepare 不借用 payload，`nx_usb_control_begin` 按当前
权威 SETUP epoch 准入，成功才建立 outer 借用，且本身不提交包。IN 发送
min(length, wLength)，OUT 容量必须覆盖 wLength；请求、控制存储与 payload
须满足公共头文件的对齐和不重叠合同。
`nx_usb_control_step` 每次最多调用一次 DCD service、读取一次权威 state，并执行
一次 packet 提交／取消或 STALL 动作。它只检查两个固定 packet 槽，不在一步内循环
完成多包事务，也不创建 worker；调用者在 suspend、cancel 或停机期间仍负责推进。

| 事实 | 允许的结论与后续责任 |
| --- | --- |
| 一个 inner packet acquire SETTLED | 仅该包的 provider 已结束访问；DATA ACK 不完成整个 control 事务，也不返还 outer payload |
| DATA 全部结束 | IN 按 MPS 分包，短于 wLength 且恰好落在 MPS 边界时发送必要的 DATA ZLP；OUT 进入 DATA_READY，尚不发送 STATUS IN |
| 对应 STATUS 包真实 ACK | 可完成该 outer 事务的线上结果；已 acquire 的 STATUS ACK 先于随后观察到的新 epoch、closing 或超时判定 |
| SET_ADDRESS staging 成功 | 仅暂存地址；STATUS IN 的实际 ACK 与 DCFG 地址写入／读回分别成立，线上成功不保证地址或外部 configuration 已提交 |

`nx_usb_control_out_view` 只在同 epoch 的 DATA_READY 且没有 provider packet 引用时，
提供 const 检查视图；outer 借用继续，视图在下一次同 owner adapter 调用时失效，
不能修改、释放或跨 Task 传递。外部策略检查内容后调用 `nx_usb_control_approve`：
true 才允许下一步 STATUS IN，false 请求对旧事务 STALL。审批包含在原绝对
monotonic deadline 内；已被软件观察到的新 epoch 不接受旧事务的拒绝决定。

拒绝路径使用选定 ROM profile 的 `setup_decision`：它是一个
`nx_usb_setup_decision_port_t`，仍由共享只读 ops 与既有实例 context 构成两指针
face。`nx_usb_setup_reject` 在同一 IRQ 临界区内检查预期的已发布软件 SETUP
epoch、pending／closing 及两个 EP0 方向没有 active provider packet，再执行 STALL
命令并核对读回；旧 generic USB 方法表和 provider context 不增加字段。旧 epoch 的决定不对已观测的新软件事务产生
STALL 效果。这是已观测软件 epoch 的序列化合同，不是 USB 总线的物理原子事务。IRQ 尚未采集的 SETUP、主机重试和真实
wire 时序仍需实板验证；outer request 的借用不因拒绝、epoch 更新或临界区退出而撤销。

地址策略显式调用 `nx_usb_device_address`，并在 step 刷新后检查权威 device state；
`nx_usb_control_snapshot` 只复制最近一次已获取的 state，不替代刷新，也不返还借用。
设备地址、closing 与外部 configuration readiness 不能从 outer SUCCESS 推断。

新 SETUP/reset 的 epoch、cancel、timeout 或 device stop 都不撤销旧借用。adapter
先 acquire/fold 已完成 packet，再观察新 epoch；旧 packet 必须实际排空，才能最终
发布 outer SETTLED。描述符字节、outer request、adapter context、profile 和其
endpoint face 在此期间保持有效；一个 outer 结束也不替调用者 join 独立 observer。

如果 OUT0 packet 进入 QUARANTINED，adapter 暴露 `NEEDS_DEVICE_STOP` 并继续保留
outer 借用。只有外部 controller owner 能授权 `nx_usb_device_stop`；adapter
不自动停止共享 controller，不用 reset 或重初始化冒充排空。owner 执行实际完整停止
后仍须继续 step，以 acquire packet 终态并结束旧 outer；缺少证明时继续保留存储。

## 取消、失败与完整停止

任务入口要求单一序列化特权执行者、Handler 外与 incoming IRQ masks clear。IRQ
保存/恢复 mask；一次最多消费八个 RX status、128 payload words 和八个选中方向。
冷热路径分开：cold reset/flush 有有限 poll budget；IRQ flush 只发一次命令并读回，
从不在 IRQ 使用 cold poll loop。超时使用独立 monotonic deadline，USB suspend 或
拔线后仍须由外部执行者调用 service，不依赖 SOF 或隐藏 timer。

IN drain 先请求 SNAK、读到实际 NAK 状态，再请求 disable；EN clear、EPDISD、FIFO
flush 和 W1C clear 必须全部成立。OUT 非零方向先确认 global OUT NAK，再 disable
及排空 RX。drain 超时保留 QUARANTINED，不能撤销 buffer loan。

Control OUT0 的 EPD 位固定为零，SETUP 也可绕过 NAK。不能模拟一个不存在的 disable
ack 来完成取消。OUT0 individual cancel 在缺少真实完成证明时保持 DRAINING，随后
QUARANTINED；用户可选择完整 device stop。完整停止先读回 soft disconnect、global
OUT NAK 与 DMAEN=0，消费 RX FIFO；随后关闭 global/endpoint/NVIC sources 并 join
所有通知发布者，flush FIFO，再次验证 disconnect/NAK/FIFO/IRQ 状态，才解除 OUT0
PIO loan、最后发布 SETTLED。OUT0 EP enable 此时可以仍为一，因为硬件只拥有内部
FIFO，已撤回的 PIO 执行者不再拥有 caller buffer；下一次 cold core reset 重新
建立硬件状态。任一步证据缺失就返回 BUSY，并保留 caller storage 与 clock consumer。

捕获 wake target 后 publisher count 仍保留该借用；stop/detach 在 notify 未返回时
拒绝 BUSY。wake 必须同时支持 service/cancel 的 task 上下文和真正 IRQ 上下文，
绑定读取实际 priority/group 验证 syscall ceiling，不修改全局 grouping。

## 证据与尚未建立的资格

既有 DCD 开发证据在 `tools/testing/evidence/usb-device-iteration/manifest.json`，
绑定原 Native 43、STM32 7、GD32 7 个 Google cases 以及 ASan/UBSan 同 57 cases、
零 skip 的执行；
两家实际 SDK 的六个 production ARM 单元通过 strict C11 编译。对应 clock owner 的
27 个独立 cases 保留在 `usb-clock-iteration`，不重复计入 DCD 57。历史模型的
错误 TXFE/EP0/PID 假设和失败日志保留，最新源与旧报告的身份不混用。
这些历史计数不覆盖新增 profile、descriptor helper 或 EP0 packetizer。新增量的
开发证据分别记录 ROM profile、descriptor、control 与软件 epoch 的真实回归、
对齐反例 RED→修正后的 focused GREEN，以及精确 ARM 装配的编译与链接。
每份结果只描述对应源码与命令；用例明细及失败尝试见该批原始回执。
focused GREEN 与 ARM 链接不建立 clean SDK 或完整 Native
候选资格；SDK consumer、资源、双语文档与普通完整提交门禁仍须分别验证。

资料是锁定 ST CMSIS header 的 FS 寄存器事实，以及 GD32F4xx User Manual Rev3.3
§29.3（尤其 printed p986–995 的 EP0、W1C、TXFE）和 §4.3.24/26 的 PLLSAI/CK48。
GD 手册 SHA256 是 `c8e1d20d7cde338892d08dcb233345e03f7960256a60a36c755c576d9cd65c13`。
完整出处身份绑定在 manifest。

这不是 clean software candidate、产品 release 或物理资格。三块 PCB 的 USB role、
VBUS sensing/power/backfeed、pull-up/ESD、电气跳帽、pin lease 与共享引脚冲突尚需
独立 reviewed Board 装配；本批没有配置这些引脚。枚举、主机重试、reset/拔线、
传输错误、PHY 三周期 flush 时序和实际 ISR/WCET/HIL 均未执行，不继承旧例程测试。
