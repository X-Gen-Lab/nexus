---
id: design.spi_frame_modes
type: reference
audience: maintainer
source_language: zh_CN
---

# F407 SPI 帧、线制与退出

`irq-frames` 为 STM32F407VET6／ZGT6 的 SPI1／2／3 提供显式 master／slave、
双线全双工、双线 receive-only、单线双向、Motorola 与 TI 帧收发。
公共面为 `nx_spi_frames_port_t` 与 `nx_spi_frames_request_t`，固定 8／16-bit
数值字宽；保留既有 polling、分段 transaction 和 DMA 面。

`[spi.<id>.frames]` 明确 `role`、`lines`、`word_bits` 与
`irq_latency_us`；还可选择 `format`、`mode`、`lsb_first`、`dummy` 和奇数
`crc_polynomial`。Motorola 支持 mode 0–3 与 MSB／LSB；TI 派生其相位与极性，
只接受 mode 0 与 MSB。CRC 目前用于双线全双工和 receive-only；单线加 CRC
在准入时拒绝，receive-only CRC 至少需要两个数据字。

## 静态实例与真实引脚

不可变 `SpiFramesOptions` 生成只读 C 资源，factory 使用独立的
`nx_factory_spi_frames` 和 `nx_spi_frames_id_t`。同类实例共享只读 ops，
状态、IRQ、请求和冷态收条分别保留；lookup 不执行初始化。
此面直接拥有硬件 NSS，不创建 CS endpoint、应用协议、worker 或运行期 registry。

主路线分别为 SPI1 PA5／PA6／PA7／PA4 AF5、SPI2
PB13／PB14／PB15／PB12 AF5、SPI3 PC10／PC11／PC12／PA15 AF6，
顺序为 SCK／MISO／MOSI／NSS。receive-only 的数据脚在 master 为 MISO、
slave 为 MOSI；单线的数据脚在 master 为 MOSI、slave 为 MISO。
只取得所选线制需要的三个或四个 pad；单线每请求在 SPE 关闭时选定方向。

`spi_frames.c` 与 `spi_frames_cold.c` 只在选择 `irq-frames` 时选编。
两个软件资源参考为 `tests/contracts/spi_frames_assembly/three-frames-ve.toml`
与 `three-frames-zg.toml`。这些 Board package 只声明硅片 pad，不建立 PCB 接线、
外部电平、负载或设备资格。

冷态准入先检查完整线制、引脚、IRQ、频率和存储不重叠合同；实际启动还检查选定
RCC profile。收条保存原 CR1／CR2／CRCPR、APB gate、GPIO gate、全部 pad 的
latch／mode／type／speed／pull／AF，以及 IRQ 优先级。关闭失败保留同一收条与
时钟，只有真实回读证明恢复后才清零 owner。

## 请求、帧数与时序预算

一个未屏蔽的 privileged Thread executor 独占 submit、cancel、service、stop
与 recover。描述符和跨度从成功受理借用到 acquire 观察 SETTLED；所有拒绝不
保留新引用或产生硬件效果。每请求 1–65535 个数据字，16-bit 跨度必须对齐，
TX、RX、描述符和全部 provider／冷态元数据必须互不重叠。

`nx_spi_frames_prepare` 显式接收 port，经该实例的 prepare 方法绑定 owner、
单调 generation 与当前 System time epoch。完整 cold close 不清除 generation，
下一次启动推进代际；旧 READY 不可跨重启或 Core epoch。重新 prepare 必须使用
新时间域的 deadline，不以数值仍未到期来继承旧时间语义。

Thread 准入、IRQ 入口、DR 读取后写 RX 之前和 TX payload 读取前，均核对 System
started／core_hz／timer_error／time_epoch 与实际 RCC profile。失域停止 payload
访问并进入隔离，cleanup 不读取已失效的 Core 时间。recover 不收养新 epoch，
需要完整 cold close 后重新启动。

双线全双工可省略 TX 来发送固定 dummy，或省略 RX 来丢弃接收，但不能同时省略。
receive-only 只接受 RX，单线每请求只接受一个方向。CRC 使用额外硬件 CRC 帧，
不计入数据字数；最后一个 RXNE 及 CRC 错误观察仍是完成条件。
CRCERR 返回 IO，收到的数据不能据 transferred 数值推断为校验通过。

master 的 SCK 为所选 APB 的 2–256 分频，实际速率不超过 `max_hz`。
slave 的 `max_hz` 是外部主机的最高 SCK，不受 master 的 APB／256 最低速率限制。
TI slave 的 BR 还决定 MISO 释放时间；低速外部时钟采用最长可用内部 BR。
产品必须保证外部 SCK、NSS setup／hold、占空比和共享线行为符合实际配置。

`irq_latency_us` 声明 IRQ 延迟与全部非等待软件开销的上界，并不是实测资格。
master receive-only／单线 RX 使用 RM0090 的连续时钟停止序列：总帧数为一时，
SPE 开启后等待至少一个 SCK 周期再关闭；更多帧在倒数第二个 RXNE 后等待该周期
再关闭 SPE，让正在进行的末帧完成。CRC 的最后一帧是独立 CRC 帧。
准入把整微秒计时量化与尾部裕量计入窗口；SUCCESS 的精确 N 个数据帧承诺以该
预算和 peer 合同成立为条件，不能把它解释为无条件的物理时序证明。

定时等待最多读取 10000 次时钟；冻结、倒退、已观察到的过晚时钟或 SPE 撤销拒绝
使请求进入 IO／QUARANTINED。继续 service，直到硬件 reset 与请求源撤销有真实
证据，才能归还借用。声明预算的所有违约不一定可由软件检出；host MMIO 模型
不会自主生成 SCK，不能据其通过宣称现场精确帧数已经验证。

slave peer 必须在 submit 成功后才驱动 NSS／SCK；CRC 还要求启动时 SCK 稳定，
并遵守数据与 CRC 阶段之间的 NSS 要求。Motorola slave 接收使用最后一个 RXNE
和实际 NSS high 证明结束，不以可能卡高的 BSY 判断失败；TI slave 接收使用最后
RXNE。slave 单线 TX 在最后 DR 后保留末 TXE 观察，在末帧仍进行时关闭 SPE，再等
真实 BSY low；Motorola 还要实际 NSS high。这依赖声明的最坏 IRQ 时限。

## 错误、取消与完整归还

IRQ 每次最多移动一个 RX 字和一个 TX 字，不结清描述符。MODF、OVR、FRE 与
CRCERR 保留第一个错误结果并停止后续加载；取消和 deadline 同样只请求退出，
不会撤销借用。源撤销拒绝时也不能继续读取 payload，持续错误不能反复推迟
第一次确定的排空期限。

service 在角色与线制的末帧证据、SPE／中断源真实撤销、pending／active IRQ
静止后发布 SETTLED。无法正常排空时进入隔离并尝试只复位本实例；复位或撤销
拒绝继续保留请求。RX transferred 表示已复制的数据字；单线 TX 在末帧证据
成立前异常退出时保守报告零，末帧与退出证据成立后才报告全部 TX 字。
transferred 数值不能覆盖错误结果。

stop 关闭新准入并请求活动项取消，BUSY 要求原 owner 继续 service 后重试。
recover 只复位空闲故障实例，不重放请求；完整 cold close 才恢复 pad、寄存器、
优先级和时钟。生产者、观察者和所有未完成请求必须在平台归还前加入停止过程。

## 官方依据与验证边界

实现依据 DS8626 Rev12 AF 与 Table55、RM0090 Rev22 §28.3，以及 ES0182 Rev19
§2.12。每次请求先执行实例硬件 reset，避免 slave RX 转 master 的 §2.12.2 问题；
拒绝偶数 CRC polynomial，覆盖 §2.12.3。master 接收关闭不依赖 BSY，master 发送
则先等 TXE 与 BSY；slave TX 使用 §2.12.5 的提前关闭序列。

pad 设置为 high speed code 2，对应 §2.12.4 所列适用软件设置；外部负载、电压、
布线、时钟反馈、实际 IRQ 延迟和芯片 revision 仍需独立实测。GoogleTest／Mock
验证真实 C、外部 MMIO／NVIC 边界和生成工厂，typed IR 验证精确选择，VE／ZG
ARM ELF 验证链接与预算。这些结果不替代电气与 SCK 波形；physical HIL 和远端
Actions 当前均为 `not_executed`。
