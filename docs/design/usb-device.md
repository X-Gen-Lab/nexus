# USB FS device 包所有权与接入边界

公共接口是 `nexus/io/usb_device.h`。当前纵切片提供 Native、STM32F407 和
GD32F470 的 internal-PHY full-speed device PIO 机制及实际 SDK 绑定。维护范围是
EP0 control、显式选择的 EP1–EP3 bulk IN/OUT、SETUP/reset/suspend/resume 事实、
STALL、SET_ADDRESS、取消与退出。没有 Host、HS、DMA、ISO、remote wake、USB class
或产品协议栈。一次 request 是一个不超过 MPS 的包；多包 control/class 事务、
终止 ZLP、描述符与工作者由外部应用负责。

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

开发证据在 `tools/testing/evidence/usb-device-iteration/manifest.json`。完整执行
Native 43、STM32 7、GD32 7 个 Google cases，ASan/UBSan 同 57 cases、零 skip；
两家实际 SDK 的六个 production ARM 单元通过 strict C11 编译。对应 clock owner 的
27 个独立 cases 保留在 `usb-clock-iteration`，不重复计入 DCD 57。历史模型的
错误 TXFE/EP0/PID 假设和失败日志保留，最新源与旧报告的身份不混用。

资料是锁定 ST CMSIS header 的 FS 寄存器事实，以及 GD32F4xx User Manual Rev3.3
§29.3（尤其 printed p986–995 的 EP0、W1C、TXFE）和 §4.3.24/26 的 PLLSAI/CK48。
GD 手册 SHA256 是 `c8e1d20d7cde338892d08dcb233345e03f7960256a60a36c755c576d9cd65c13`。
完整出处身份绑定在 manifest。

这不是 clean software candidate、产品 release 或物理资格。三块 PCB 的 USB role、
VBUS sensing/power/backfeed、pull-up/ESD、电气跳帽、pin lease 与共享引脚冲突尚需
独立 reviewed Board 装配；本批没有配置这些引脚。枚举、主机重试、reset/拔线、
传输错误、PHY 三周期 flush 时序和实际 ISR/WCET/HIL 均未执行，不继承旧例程测试。
