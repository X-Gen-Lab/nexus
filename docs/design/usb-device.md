# USB Device、Host 与显式角色所有权

公共接口是 `nexus/io/usb_device.h`。生产实现归属 `soc/stm32/stm32f4/`，
当前精确适配 STM32F407VET6／ZGT6 的 OTG FS 与 HS core：内置 PHY 的全速、
外部 ULPI 的高速／全速回退，以及 PIO 和 HS AHB DMA。Host 使用锁定官方 HCD／LL，
Device 复用已有控制阶段与 errata IP owner。
目录按 STM32F4 家族组织，不表示该系列全部器件已完成接入。Native 位于
`tests/support/native/`，仅作为显式启用的主机测试模型。维护范围是
EP0 control、显式选择的非零 bulk／interrupt／ISO IN/OUT、
SETUP/reset/suspend/resume 事实、合法类型的 STALL、SET_ADDRESS、取消与退出。
DCD 的一次 request 仍是一个不超过 MPS 的包；非零 endpoint 的 active／mask 由
独立可选 activation face 显式控制。SET_CONFIGURATION、配置就绪及其回滚由外部
Chapter 9 owner 负责，packet 机制不能单独证明完整枚举／物理 reset 恢复。
可选 `Nexus::USBControl` 组合 EP0 多包 DATA、必要 ZLP 与 STATUS；独立的
`Nexus::USBDescriptors` 校验外部描述符结构。两者不创建任务或分配内存。
具体 class 分派、描述符字节与查找、VID/PID、字符串、配置及恢复策略由外部工程提供。
可选 `Nexus::USBACM` 只组合下述 CDC ACM 控制子集；没有数据泵或完整 CDC/HID 类栈，
其合同不建立完整 class 栈；Device ISO 由下述有限 packet 合同提供，remote wake 未接入。
独立的 `nexus/io/usb_host.h` 提供 Host packet transport，
`nexus/io/usb_otg.h` 提供按需显式 managed role；自动 ID、SRP、HNP 策略不在范围内。

## 对象、资源与时钟

controller 和 endpoint 保持独立 identity；公共 face 只有 readonly ops 与 context。
装配拥有一个 controller、精确 endpoint 数组、精确事件环和所选 PHY／controller
clock owner。实例共享只读方法表，没有全局可变 registry、endpoint 最大池、隐藏 worker
或必选 OS。未选实例不产生状态，MCU 热路径用 direct volatile MMIO。模型编译只
替换外部 MMIO/NVIC 叶端口，MCU 不携带模型函数表或运行时模拟分支。

STM32F407 的 OTG_FS 基址是 `0x50000000`，四个 IN / 四个 OUT 方向、320-word FIFO，
单一 IRQ 67；绑定用锁定 SDK 验证实际 offset、stride、容量和 W1C/只读位。
OTG_HS 基址是 `0x40040000`，六个 IN／六个 OUT、1024-word FIFO、IRQ 77。
内置 FS PHY 使用 PB14／PB15 AF12；ULPI 使用十二根 AF10 pad 和外部 60 MHz。
EP0 固定 64-byte MPS；全速 bulk 为 8/16/32/64，高速 bulk 为 512。
全速 interrupt IN/OUT 为 1–64 B，高速为 1–1024 B。ISO 的协议 MPS 上界为
全速 1023 B、高速 1024 B，每次机会只支持一个 transaction，实际选择还须通过 FIFO 准入。
RX FIFO 采用审核下限 48 words，不自动改变既有默认；FS core 的 RXFD 上限为 256 words，
HS core 为 1024 words；
总 FIFO 容量不能当作 RX 上限。每个选中及低编号未选 IN FIFO 至少留 16 words，
HS AHB DMA 另留 32 words。完整 TX 分区由 controller 验证无重叠且不超总容量，
非法方向、类型、MPS、FIFO 和重复 identity 在 clock validate／retain 与 MMIO 之前拒绝。

RX 还须覆盖 SETUP/status 与最大 OUT packet；多个 ISO OUT 至少保留两份最大 OUT
packet 及其状态字空间。配置器、公开 profile 的 RX 检查和 C 准入使用同一需求。
FS core 的最大 ISO IN 参考为 48 RX＋16 EP0 TX＋256 ISO TX＝320 words。
whole-packet FS core OUT 1023 B 需要 270 RX words，超过 RXFD 上限而拒绝；
HS core 的内置 PHY 全速模式可用足够 RX 接纳该 MPS。这是精确资源组合，不是所有
MPS／endpoint 组合均可装配的承诺。

| PHY 选择 | 所选 PHY 时钟条件 | 接入行为 |
|---|---|---|
| 内置 FS PHY | 选中 ClockPlan 的 HSE／PLL48，精确 48 MHz | 验证实际 PLL/ready 与 SYSCLK/APB 计划；管理对应 FS／HS gate |
| 外部 ULPI PHY | 外部交接的 60 MHz | 管理 HS 与 ULPI gate；不隐式要求 PLL48 |

clock owner 管理精确 consumer count；失败回滚缺少读回证明时仍保留清理责任。
DCD 先 validate/retain，退出完成后 release consumer，最后由外部 owner stop
时钟。pin/electrical/VBUS 和所有共享时钟消费者必须由外部资源 owner 先授权并
静止，不能仅凭 gate 为零推断系统独占。FS core AHB 必须大于 14.2 MHz，
HS core 必须大于 30 MHz，TRDT 按实际 AHB 选择。内置 PHY 不接受仅有 nominal
48 MHz 的 HSI 精度假设；实际晶振精度和 PCB 仍需外部电气证据。

controller、endpoint、事件环与 request 按所选实例静态分配；可选 ROM profile、
EP0 adapter 和 outer request 分别计入其 owner 的资源预算。私有 endpoint 保存类型
与本次 ISO 的目标帧；目标帧复用已验证 ARM ABI 的原 padding。
`bInterval` 仍属外部 descriptor，不新增调度队列或 worker。结构尺寸、
未选 section GC 与完整 Flash／RAM 成本须由同源配置的 ARM ELF／map 建立，
不以 host sizeof 或旧版本 probe 代替当前镜像测量。

## 选定 ROM profile 与描述符

`nexus/io/usb_profile.h` 的只读 profile 绑定既有 controller／endpoint face，以及
选定的方向、类型、MPS、FIFO、PHY 时钟、速度和 PIO／AHB DMA 事实。
只有 USB 被选入且 `factory.capabilities = true` 时，生成器才产生 profile、精确
endpoint 数组与 `nx_factory_usb_profile` getter。设为 `false` 时不生成这些对象
或 getter；普通 USB face、方法表、provider context 与既有 generic capability
类型不因此扩大。getter 不初始化硬件，也不发现新 endpoint。
ULPI 使用独立的高速与全速回退 profile；`nx_factory_usb_profile_for_speed`
只返回选定速度对应的 ROM 记录，不把高速 512 B MPS 用于全速。
公共 `NX_USB_PROFILE_INTERRUPT` 的枚举值是 2，USB wire／寄存器 EPTYP 值是 3；
`NX_USB_PROFILE_ISOCHRONOUS` 的枚举值是 3，wire EPTYP 为 1。provider 显式映射类型，
不把两者直接强转混用。

`nx_usb_profile_validate` 只读检查选定事实，不能证明实际时钟、PCB、VBUS 或枚举。
外部 descriptor 通过 `nx_usb_descriptor_view_t` 传给
`nx_usb_descriptor_validate`：device、configuration、string 必须完整；校验
length／total、EP0 MPS、唯一 interface、alternate 0，以及 bulk／interrupt／ISO IN/OUT
endpoint 的方向／编号／类型／MPS 与选定 profile 的对应关系。interrupt 描述符的
`bInterval` 全速为 1–255 个 1 ms frame，高速为 1–16 的指数编码；ISO 在两种速度下
均为 1–16 指数编码，单位分别是 1 ms frame 和 125 µs microframe。interval 由外部
descriptor 独占，不复制到 TOML、IR、profile 或 provider state。DCD 不调度 host poll，
也不承诺实际间隔；bulk 的既有 interval 处理保持不变。ISO 同步／用途字段校验标准
编码，不推导音频反馈或 class 拓扑；保留 usage／位非法时拒绝。HS 高带宽 multiplier
仍不支持，不静默裁剪成单 transaction。

选定类型不符、描述的 endpoint 未选入 profile 或 MPS 不符返回 INVALID；非零
control 与 alternate setting 仍为 UNSUPPORTED，保留字段非法仍为 INVALID。
class-specific 子结构只检查边界，不解码其类语义。

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

非 ISO IN 的每次有限 data／ZLP 长度寄存器写入 `(1U << 19) | length`（PKTCNT=1），
MCNT／保留高位保持零；ISO IN 同样 PKTCNT=1，但 MCNT=1，按下一节选择帧奇偶。
interrupt 不增加 odd/even-frame 命令。
每个选定 IN direction 使用独立 Tx FIFO，
PIO 一次同步装入整个不超过所选 MPS 的包，没有剩余软件 refill 工作；TX-empty mask 保持零，
不创建 TXFE refill worker。IN FIFO 写入不是完成，只有 XFRC、EP enable 已清、
FIFO 空闲容量恢复并且观察到的 W1C completion 清除读回成功才发布 SUCCESS。
OUT 必须同时读到包数据和 XFRC，
short packet 正常完成；越界包的所有 FIFO words 都被消费，最多复制 caller capacity，
之后报告 OVERFLOW 并排空，不越界写 caller buffer。

IN TXFE 是只读状态，不参与 W1C 清除/完成的零值证明。ITTXFE 是主机 IN token 在
FIFO 尚空时到达的可重试信息，OUT STUP 是正常 SETUP phase 事实；两者不错误地终止
包。IN control timeout 和 OUT FIFO overrun 使用对应方向的真实事件位。EP0 的
DATA PID 由 SETUP/core 管理：bits 29:28/16 对 EP0 是 reserved，软件只对非零
bulk／interrupt endpoint 使用 DATA0 命令。对这些 endpoint，reset／clear-halt 须核对 DATA0
读回；失败保留 reset admission 屏障或 halt，不提前允许新包借用。

SETUP 严格解码八个 little-endian bytes，并产生单调递增 epoch。新 SETUP/reset
使旧 control loan 进入排空，不能仅改 epoch 就撤销借用。reset 与 queued SETUP
在同一 snapshot 中没有先后证明，先前 queued SETUP 被丢弃；reset 在旧 loan 排空和 FIFO flush 后，
还须显式屏蔽并清除选定非零方向的 active，读回成功才清 `reset_pending`。
失败保留 reset 屏障和 clock，由 service 或完整 stop 继续恢复。新 admission 仍须
满足 endpoint 的实际启用条件。SET_ADDRESS 验证
当前标准 SETUP 的各字段，先 staged，只有对应 status-IN 实际 XFRC 后写 DCFG
地址。地址读回失败关闭 controller。

STM32F407 总线 reset 清除非零 endpoint 的 USBAEP，模型保留这个硬件事实。
生产路径仍在 loan 排空后显式清除非零 active／mask 并核对读回，不仅依赖 reset
事件来证明平台后置条件。完成 reset 后仍需以新当前 epoch
显式 enable；reset／clear-halt／submit 均不自动激活。对 inactive 非零 endpoint，
submit 返回 STATE，prepared request 保持 READY，没有新 request／buffer 借用或
FIFO 写入。EP0 control 保留既有合同；DATA0 恢复、reset 事件或 controller 未
closing 均不能证明外部 configuration 已提交。

事件环保存 value facts，满时不覆盖旧事件；精确软件 loss marker 排在原队列后。
最新 SETUP/epoch/closing 状态不依赖事件环空间。时间戳是 IRQ acquisition time，
不是 USB 总线采样时刻；reset 批次中的无序旧 SETUP 不计作新的已接收 control。

## ISO 有限单包与帧边界

`transfer="isochronous"` 沿用现有 packet request、调用者 payload 与唯一 controller
owner。每次 submit 从实际 DSTS 选择下一 frame／microframe 的奇偶；FS 使用 11-bit
帧计数，HS 使用 14-bit 计数。ULPI 必须先观察到 ENUMERATED speed，才能提交 ISO。
每次提交的剩余 Core deadline 至多一秒，小于两种速度帧半环的 1.024 秒。收集
incomplete 事实时先检查 Core 超时，避免长停顿把旧目标误判成未来；Core 与 SOF
之间不建立时间换算。

IN 以单个 transaction 提交，OUT 的硬件接收计划为一个 MPS packet。ISO 的控制位 28／29
表示 even／odd frame，不表示 DATA toggle，不执行 bulk 的 DATA0 重置证明。
ISO endpoint 的 STALL 操作返回 UNSUPPORTED，不写端点寄存器。ISO 没有 ACK／NAK
握手；SUCCESS 只证明控制器完成且必要源、FIFO、AHB 和 IRQ 引用已撤回，不能证明
host 或其应用已收到数据。周期调度、同步、用途和 class 策略由外部 owner 明确提供，
不自动重试错过的机会，也不按 `bInterval` 创建数据泵。

已有未清的 global incomplete／drop 事实会在新 admission 前拒绝。ISOODRP（bit14）、
incomplete IN／OUT（bit20／21）只使已到机会的 active ISO borrow 进入 IO 排空，
不取消仍属未来帧的请求，也不影响其它 bulk borrow；已观察到的真实完成优先于并发旧
incomplete。取消、超时、reset 和 stop 都继续实际 drain，失败仍保留 request／payload
并可进入 QUARANTINED，不能强制释放借用。

软件参考位于 `tests/contracts/usb_iso_assembly/`，分别覆盖 FS 最大 IN、HS core
内置 PHY 的全速 OUT 和 ULPI HS DMA。它们不提供完整 USB 音视频 class、remote wake
或物理吞吐／时序资格。

## 非零 endpoint 显式 activation

独立的 `nexus/io/usb_activation.h` 提供 `nx_usb_activation_port_t`：face 是共享
只读 ops 与既有 controller context 两指针，只提供一个同步
`nx_usb_activation_set(port, epoch, endpoint_address, active)` 操作。生成的
`nx_factory_usb_activation(nx_usb_device_id_t)` 返回选定 controller 的静态借用
face，非法或不支持的 identity 返回 NULL；查找不初始化硬件、不 retain clock。
生成独立于 `factory.capabilities`，不要求启用 ROM profile，不改变既有 USB ops、
profile、face 布局或 factory ABI epoch 1。该机制不新增可变 controller／endpoint
状态、队列、worker、实例池、动态分配、TOML 字段或 IR 类型。两指针布局说明 face
形状；实际只读保留成本、未引用 GC 与完整镜像成本须分别以对应 ARM ELF 确认。

操作只面向已经选定的非零 endpoint 某一个方向：FS engine 为 EP1–EP3，HS engine
为 EP1–EP5；拒绝两个 EP0 地址及保留／未选地址。OUT 可为 bulk、interrupt 或通过
FIFO 准入的 ISO，各速度 MPS 按选中 profile 限定。调用者须是同一个
序列化特权 Task，进入时 incoming IRQ
masks clear。controller 须 started、持有 clock、没有 closing／reset 屏障，参数
`epoch` 等于最新权威软件 control/reset epoch；冷态 epoch 0 合法。无效 identity
或已知旧软件 epoch 在 MMIO 前拒绝。软件检查通过后，已观察到的 raw USBRST
也使本次调用在写入前返回 STATE。尚未采集的物理 SETUP/reset 仍可能随后发生，
IRQ 临界区不提供总线原子性；stop/restart 后外部须丢弃旧缓存的 epoch。

目标方向必须没有 request loan、disable 工作、EPENA、待确认的定义事件或尚未完成的
SETUP 清理。IN 还要求专用 FIFO 全空且没有 flush；OUT 要求共享 RX status FIFO 空。
否则返回 BUSY，不暗中取消请求、消费其他方向的 RX 或释放其存储。前置检查返回
INVALID／CONTEXT／STATE／BUSY 时没有硬件写入，不建立新 loan；激活本身从不借用
request 或 payload，不能代替已有 packet 的 cancel／drain／acquire SETTLED。

冷启动仅使两个 EP0 方向 active 并解除其 endpoint mask；其余选定方向保留
immutable 几何但 inactive／SNAK。使能一个 inactive 方向时，先按选定 MPS、wire
类型和仅 IN 的 FIFO selector 配置目标，清 STALL、置 active 与 SNAK；bulk／interrupt
同时建立 DATA0，ISO 不使用 DATA toggle。保持 EPENA clear，不写 TSIZ 或 payload FIFO。
核对稳定控制字段、适用的 DATA0 与 NAK 状态，
仅处理命令后新出现的目标 IN NAK-effective／token-empty 提示（0x50），并证明其
W1C 清除；其他新事件使本次操作失败，进入 partial IO／closing。调用前已有的
定义事件仍返回 BUSY，不在前置拒绝中清除。随后解除目标 EP_MASK 位并读回完整
预期 mask。OUT 不写 reserved TxFIFO 字段。active、传输使能与 IRQ mask
是三个独立条件；mask 或 active 单独成立均不代表已经接受一个包。

反激活之前，外部必须实际 cancel／drain 并 acquire 目标 packet 的 SETTLED。
操作先屏蔽目标方向且核对其他 mask 位未变，再设置 SNAK、清 active 并核对。
每次仅修改对应方向，保留其他 endpoint 和两个 EP0 位，物理 vector／priority
route 不变；不 flush 共享 RX、不重置 controller、不停止 clock，也不开始传输。
Task 显式操作在既有有限 poll budget 内等待控制／W1C 读回，每项命令只发一次；
reset progress 新增的反激活步骤只单次观察，由后续 service 继续，不在 IRQ 引入
此 Task 等待循环。

幂等调用必须先满足同样的无 loan／无 pending 前置条件。已经 active 的 enable
先核对 immutable 几何，不重放控制寄存器配置，只按需修正目标 mask；重复 enable
不得重置 bulk／interrupt 的 DATA1、清除真实 HALT 或发送包。已经 inactive 的 disable
不重新编程几何，只证明目标 mask 关闭及事件清除。只有显式 disable 后重新 enable
才为 bulk／interrupt 建立新 DATA0 并在读回成功后清 halt；disable 保留其 PID。
已 active 但 immutable
几何损坏时返回 IO／closing，不通过重写控制寄存器掩盖错误。

控制或 mask 写入后的读回失败返回 IO，并保留既有 `closing` 屏障；IO 可以已有
部分硬件效果，不能作为无操作拒绝。只对目标 best-effort mask／inactive 回滚，
若观察到 EPENA 意外为一，不能假装写零已完成 disable。失败不能通过重试 activation
清 closing；外部须继续推进其他 loan 并执行显式完整 stop。stop 仍可能 BUSY，
在既有 disable／flush／IRQ join 证明完成前保留 clock 与 caller storage。

外部 Chapter 9/configuration owner 根据配置或 alternate 选择方向，先 cancel／
drain／disable 旧方向，再 stage 新方向，随后提交该请求的 STATUS IN。这与 ST
手册及参考 Zephyr 的 before-status 顺序一致，不强制解释为 ACK 后才可激活。
外部仅在对应实际 STATUS 完成且仍满足当前事务条件后发布 configured readiness，
此前自行阻止普通非零 packet admission。多次 setter 不是配置事务：后续失败、
新 SETUP supersede 或 status 失败时，外部须撤销已经成功的 staging 或显式 stop；
不能用平台 rollback 撤销主机已观察到的 ACK。去配置也须先实际排空／反激活，
再按外部 deadline 与拒绝策略决定是否接受；DCD 不解码 configuration value。

activation 补充既有 FIG-16 的显式控制调用，不新增生命周期 owner 或执行线程。
它不提供 Chapter 9／class stack、CDC notification、多配置／alternate 策略、
连续传输或电气接入；ISO packet、HS、DMA、Host 和角色由各自独立选入路径提供。
activation 机制不证明
物理枚举／reset 时序、完整 IO-35 退出、clean candidate 或 HIL。

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
`nx_usb_control_step` 每步至多一次 poll／权威 state 读取，再执行至多两次 submit
或一次 cancel；为 DATA IN 先取得同 epoch 的零长 STATUS OUT，再提交 IN。
只检查两个固定 packet 槽，不在一步内循环完成多包事务，不创建 worker。
调用者在 suspend、cancel 或停机期间仍负责推进。

| 事实 | 允许的结论与后续责任 |
| --- | --- |
| 一个 inner packet acquire SETTLED | 仅该包的 provider 已结束访问；DATA ACK 不完成整个 control 事务，也不返还 outer payload |
| DATA 全部结束 | IN 按 MPS 分包，短于 wLength 且恰好落在 MPS 边界时发送必要的 DATA ZLP；OUT 进入 DATA_READY，尚不发送 STATUS IN |
| 对应 STATUS 包真实 ACK | 确定该 outer 事务的线上结果；已 acquire 的 STATUS ACK 先于后来观察到的 epoch、closing 或超时，但两个 packet 方向及其 DMA／FIFO／IRQ 借用都退出后才发布 outer SETTLED |
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
先 acquire/fold 已完成 packet，再观察新 epoch。零长 STATUS OUT 的有效 XFRC
关闭线上 control admission，但不是尚未停止的 IN／OUT DMA、FIFO 或 IRQ 退出证明；
AHB busy、地址拒清或另一方向未结清时继续保留两个槽及 outer payload。已经取得
STATUS ACK 后不再发下一 DATA IN；只 join 旧方向再发布 outer SETTLED，不伪造
额外 ACK 或重放 payload。描述符、outer request、adapter、profile 和 endpoint face
全程保持有效，outer 结束仍不替调用者 join 独立 observer。

如果 OUT0 packet 进入 QUARANTINED，adapter 暴露 `NEEDS_DEVICE_STOP` 并继续保留
outer 借用。只有外部 controller owner 能授权 `nx_usb_device_stop`；adapter
不自动停止共享 controller，不用 reset 或重初始化冒充排空。owner 执行实际完整停止
后仍须继续 step，以 acquire packet 终态并结束旧 outer；缺少证明时继续保留存储。

## 可选 CDC ACM 控制子集

`nexus/components/usb_acm.h` 使用调用者零初始化、自然对齐的 `nx_usb_acm_t`，
其中包含一个既有 EP0 packetizer、一个 outer request 和七个 wire bytes。
调用者显式提供 communication interface number、初始 line coding 和控制意图；
不创建实例池、任务、数据泵、UART 绑定或新的 controller/endpoint face。provider
仍共享只读 ops；未引用 `Nexus::USBACM` 不保留类实例或 archive 代码。
外部 dispatcher 独占安排 EP0 使用，并先处理标准请求和其他 interface/class。
class-specific descriptor、capability 宣告、configuration readiness 和应用授权
继续由外部工程负责；通用 descriptor helper 不验证 ACM 类语义。外部若宣告相关
class capability，必须补足其全部必需的请求／notification 合同，不能仅凭这个
控制子集声称完整 CDC 合规。

仅支持 `SET_LINE_CODING`、`GET_LINE_CODING`、`SET_CONTROL_LINE_STATE` 和
`SEND_BREAK`。`bmRequestType` 必须分别是 OUT `0x21` 或 GET 的 IN `0xa1`，
`wIndex` 必须完整等于选定 interface number；line coding 的 `wValue` 为零且
`wLength` 恰为 `7`，control-line 与 break 的 `wLength` 为零，control-line
只允许 DTR/RTS 两位。未知或字段不符的请求通过已有 software-epoch guarded
reject 明确 STALL；如果 guarded reject 返回 BUSY/STATE/IO 等错误，不能声称
拒绝已经执行。外部不应将其他合法类请求误派给这个窄子集。

`SET_LINE_CODING` 先接收七字节暂存到 DATA_READY，
`nx_usb_acm_pending` 仅复制值供外部检查。线编码的共享格式检查只接受非零 rate、
stop bits `0..2`、parity `0..4` 与 data bits `5/6/7/8/16`；实际 UART 能力与
产品策略可更窄。short DATA 或非法字段不能获批，但仍可显式拒绝。
control-line/break 请求在获批前不准入 STATUS。所有 mutation 都必须由同一
序列化特权 Task 显式 `nx_usb_acm_approve`；true 仅准许后续 STATUS，不提前
修改 committed 状态或应用物理线路。审批包含在原绝对 monotonic deadline 内。

只有对应 outer acquire SUCCESS、即真实 STATUS ACK 后，组件才提交批准的
软件意图并更新 `committed_epoch`。新 SETUP/reset/cancel 按既有 packetizer
规则排空，不撤销借用；未获得 outer SUCCESS 的终态不提交。cancel 成功只请求
排空：若撤回 endpoint 前，对应 STATUS 仍获真实 ACK，outer acquire SUCCESS
仍提交已批准的原 epoch 意图。已经 acquire 的 STATUS ACK 不被后来的 SETUP/reset、closing
或 state 读取失败撤销，紧接的 GET 返回已提交的新 line coding。reset 后是否恢复
初始类状态是外部 owner 的独立策略。每次 `nx_usb_acm_step` 最多推进一次既有
packetizer，沿用本节 packetizer 的一次 poll／state 与有界 submit／cancel 预算；
外部仍须在取消、超时、suspend 和停机期间继续推进并保留所有 storage。
OUT0 quarantine 仍须外部 controller owner 授权真实完整停止，组件不自动 stop。

外部在每次 step 后、派发下一请求前读取 snapshot，按新的 `committed_epoch`
应用 UART 参数、DTR/RTS 或 break，并独立处理应用失败与恢复；snapshot 不是事件队列，
软件提交不证明外设已应用。`SEND_BREAK` 仅记录 `0`（停止）、`0xffff`（无限期）或
有限毫秒意图，不建计时器、不自动清除，也不证明物理 break。控制子集不提供 bulk
数据搬运、serial-state notification、完整 CDC 枚举、其他类或物理资格。

## 取消、失败与完整停止

任务入口要求单一序列化特权执行者、Handler 外与 incoming IRQ masks clear。IRQ
保存/恢复 mask；一次最多消费八个 RX status、128 payload words 和八个选中方向。
冷热路径分开：cold reset/flush 有有限 poll budget；IRQ flush 只发一次命令并读回，
从不在 IRQ 使用 cold poll loop。超时使用独立 monotonic deadline，USB suspend 或
拔线后仍须由外部执行者调用 service，不依赖 SOF 或隐藏 timer。

非 ISO IN drain 先请求 SNAK、读到实际 NAK 状态，再请求 disable。ISO IN 不等待
不适用的 NAK-effective 证明，仍执行 SNAK／EPDIS；EN clear、EPDISD、FIFO flush、
AHB 与 W1C clear 仍须成立。OUT 非零方向（包括 ISO）先确认 global OUT NAK，
再 disable 及排空 RX。drain 超时保留 QUARANTINED，不能撤销 buffer loan。

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

## HS AHB DMA、Host 与 managed role

HS Device DMA 使用每方向静态 SRAM staging 和独立 24 B SETUP staging；CPU 复制
公共 payload，不要求它额外声明 DMA word padding。AHB master 不占 DMA2 stream，
CCM 不可选。完成还必须证明 endpoint disabled、AHB idle、DMA 地址撤回读回和 IRQ
会合；失败保持 QUARANTINED，不能只看 XFRC 或 timeout 释放调用者存储。

Host 每个所选 channel 同时借用一个 packet；buffer capacity 覆盖 HAL 的完整 word
访问，IN 还覆盖 MPS。NAK／NYET 返回 NACK，由协议 owner 明确重试。锁定 HCD 的
回调只锁存事实；取消／超时保守排空整个共享 FIFO controller。旧 channel、DMA
地址、FIFO 和 IRQ 全部退出才 SETTLED。Host 没有自动 hub split 策略。

Host control pipe 的 MPS 按实际速度准入：low-speed 为 8，full-speed 为
8／16／32／64，high-speed 为 64；非法组合在 HAL 和 channel 副作用前拒绝。
reset／resume 在任何 HPRT 写入前分别要求严格多于 10 ms／20 ms 加 1 µs 的剩余
deadline；额外一 tick 覆盖整数微秒采样隐藏的相位。不足、已到期或恰好耗尽预算
时返回 TIMEOUT，不发出短复位／恢复脉冲。

已经开始的 reset／resume 必须保持最低信号时间。抢占越过 deadline 时，先完成
最低保持与撤信号读回，再返回 TIMEOUT；实际已恢复时仍发布 resumed 状态。
撤信号失败进入 closing，保留原 controller／clock 责任，stop 首先重试信号撤回，
成功前不调用 HAL stop 或释放 gate。已开始信号的时钟倒退、无效或持续无进展，
以有限观察预算进入 fault observer 和不返回的 park，保留信号而不伪造最低保持。
不增加实例字段、后台任务或另一套时钟；这些软件检查不构成物理脉宽测量。

`Nexus::USBRole` 是通用供电和排空屏障，F407 managed OTG 将它接入真实 cold
Device／Host。一个 composite 独占 core、PHY、pins、clock 和 IRQ；OFF prepare
不启动 USB。显式 generation-bound 请求先真实 drain，再 source-off 读回和独立
VBUS 放电确认，才进行新角色的供电／cold prepare。切回 Device 等待至少 50 ms
并确认实际 CMOD；inactive face 纯拒绝，旧 SETUP epoch 不跨角色复用。过流通过
显式 service 进入同一撤回屏障；硬件限流与实际放电路径由外部电源 owner 提供。
详见 `tests/contracts/usb_otg_assembly/README.md` 的软件信号合同。

## 按需官方 Core middleware

`st-usb-device` 与 `st-usb-host` 只准备精确锁定的相应 middleware Core。
`components/st_usb_device` 和 `components/st_usb` 使用调用者静态 state、slots、
staging、有限 deadline 和显式 Thread step。官方 Handle／class／descriptor 回调表
留在私有接线边界，公共 I/O 不泄漏厂商类型；不选时不编译、不建立堆或后台任务。

Device Core 复用现有 epoch、activation 和真实 packet settlement。多包 DATA IN
在同 epoch 提前接纳零长 STATUS OUT；线上 STATUS 已结束但 DMA 内存尚未 join 时，
bridge 根据权威 `control_pending=false` 暂停 EP0 Core 续包派发，先会合双方向。
只转交真实已结清的 IN ACK 和 STATUS，不补造 `EP0_TxSent`，不因这段合法间隔
锁存 INVALID，也不重放下一 IN payload。新 SETUP、deadline 与停止仍须完整排空。
Host Core 在断开后先排空，再允许 Core reset／class 退出及显式 cold restart。
两者均锁存官方 Core 忽略的 LL 错误，不建立 ISO 周期调度或 remote wake。

私有 Device Core bridge 把 profile ISO 显式映射到 `USBD_EP_TYPE_ISOC`，不把
Nexus 枚举值当作官方 wire 类型。Core／class 仍由特权 Thread 显式 step，packet
失败沿既有收条传播，不增加自动 class 恢复或完整音视频协议栈。

选入 HCD 的 `HAL_Delay` 与官方 Host Core 的 `USBH_Delay` 使用既有 Core timer，
不安装 SysTick。除了实际 deadline，
它对计数冻结保留有限无进展预算；invalid、后退或持续冻结时进入不返回的 park，
不把 void delay 早退当作成功。原 cold receipt 和所有借用仍保留，故障／reset 策略
由外部 owner 负责。System STOP 前必须完成所有 delay、request 和角色事务的撤回。

## 实现位置与验收边界

F4 绑定在 `soc/stm32/stm32f4/drivers/usb_device.c`；PIO 控制器机制在
`soc/stm32/stm32f4/drivers/ip/fs_device_pio/`。厂商寄存器布局与位定义来自锁定的
`vendors/st/cmsis_device_f4/`，芯片资料身份由该家族的 `references.json` 固定。
Native 外部 MMIO 模型位于 `tests/support/native/drivers/usb_device*`，只在测试
配置中启用，不作为生产 SoC。公共组件与 F4 绑定分别由 `tests/google/usb_*`
测试和精确 ARM 装配验证；用例数与资源结论以当前提交的真实执行报告为准。

历史跨系列开发报告只描述其记录的固定源码、配置和工具身份，不代表本轮保留的
平台范围，也不覆盖后续 profile、descriptor、EP0 或 activation 修改。原始报告
保持原字节；旧计数不能作为当前候选的完成证明。当前提交仍须分别通过公共合同、
F4 寄存器模型、ARM 链接、资源预算、SDK consumer 与完整本地门禁。

当前精确芯片范围为 F407VE／ZG，板卡范围是启明 V3.1、ZG 核心板、V4.1 和
天空星青春版；这不表示四板均已取得每项 USB 电气资格。板级必须逐项确认
USB role、VBUS sensing/power/backfeed、pull-up/ESD、跳帽及共享引脚约束。
软件装配可以选择 PA11／PA12，pin claim 不证明实际接线或角色正确。

可选官方 Device／Host Core 通过私有桥接接入，见上述独立 owner。产品 class、
描述符身份和外部配置事务仍由应用提供。物理枚举、主机重试、reset／拔线、
PHY flush 时序、IRQ／WCET 与 HIL 必须另行验收；本环境没有连接实板，均未执行。
