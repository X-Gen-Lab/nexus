---
id: reference.contracts.io.usb-lifecycle
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.io.usb-lifecycle)=
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
其合同不建立完整 class 栈；Device ISO 使用下述有限 packet 合同，远程唤醒由独立的
调用者请求和显式 service 推进。
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

## 实现位置与验收边界

F4 绑定在 `soc/stm32/stm32f4/drivers/usb/usb_device.c`；PIO 控制器机制在
`soc/stm32/common/dwc_otg_device/`。厂商寄存器布局与位定义来自锁定的
`vendors/st/cmsis_device_f4/`，芯片资料身份由该家族的 `references.json` 固定。
Native 外部 MMIO 模型位于 `tests/support/native/drivers/usb_device*`，只在测试
配置中启用，不作为生产 SoC。公共组件与 F4 绑定分别由 `tests/behavior/usb_*`
测试和精确 ARM 装配验证；用例数与资源结论以当前提交的真实执行报告为准。

当前精确芯片范围为 F407VE／ZG，板卡范围是启明 V3.1、ZG 核心板、V4.1 和
天空星青春版；这不表示四板均已取得每项 USB 电气资格。板级必须逐项确认
USB role、VBUS sensing/power/backfeed、pull-up/ESD、跳帽及共享引脚约束。
软件装配可以选择 PA11／PA12，pin claim 不证明实际接线或角色正确。

可选官方 Device／Host Core 通过私有桥接接入，见上述独立 owner。产品 class、
描述符身份和外部配置事务仍由应用提供。物理枚举、主机重试、reset／拔线、
PHY flush 时序、IRQ／WCET 与 HIL 必须另行验收；本环境没有连接实板，均未执行。
