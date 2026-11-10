# F407 I2C target 与 SMBus device

![驱动源码与资源所有权](../../../docs/assets/diagrams/driver-source-ownership.svg)

本页定义 F407VET6/ZGT6 的有限 target 交换和可选 SMBus 机制。
`i2c_target` 与 `i2c` master 共用同一个物理 I2C controller，资源图拒绝同时选择
两个 owner。I2C1/2/3 各有一个静态 C state、冷启动 receipt 和固定 EV/ER 向量；
没有 listener worker、运行期注册、分配或应用寄存器表。

RX 数据读取和 GPIO 使用官方 LL，NVIC 使用官方 CMSIS。TX DR 单次 store 避免
该 SDK LL transmitter 的 DR read-modify-write 引入额外破坏性读。复合 CR1 命令、
已观察 SR1 的 ADDR/STOPF 序列、完整冷态快照恢复及 bounded reset/withdraw
由 C 显式维护：通用 HAL listener callback 的完成点不包含 Nexus 的借用退出
证明，单独的 LL setter 也不能替代 PEC 命令单次 CR1 写入后、pending 期间
不再写 CR1 的约束。

依据为 RM0090 Rev22 §27.3.2、§27.3.7、§27.3.9、§27.6，DS8626 Rev12
AF4 表及 ES0182 Rev19 §2.10.2/§2.10.5。GoogleTest/Mock 建立软件行为，
ARM ELF 建立构建和资源证据。PCB、电气、外部 controller 和真实 SCL 时序的
物理状态仍为 `not_executed`。

## 静态选择与生命周期

TOML `target-irq` 选择普通 I2C target，`smbus-device` 选择 SMBus device。
`own_address`、`address_bits`、`second_address`、`max_hz`、
`irq_latency_us`、`pec`、`alert` 和 IRQ policy 均显式提供。
`max_hz` 是外部 SCL 的集成上界；本 target 不生成总线频率。
实际 APB1 频率来自唯一 ClockPlan，C 使用相同的 `NEXUS_APB1_HZ` 核对
CR2.FREQ。输入必须为整 MHz，标准模式至少 2 MHz，Fast 模式至少 4 MHz。

普通 target 接受 7 位或 10 位自身地址。双地址只适用于两个不同的普通
7 位地址，第二地址为零表示不启用比较器。SMBus device 使用 7 位地址和
至多 100 kHz 的外部 SCL。`alert=true` 必须选择 Board 已声明的第三条 SMBA
路线；不得从两条 SCL/SDA 线推断实际 PCB 有 SMBA。

`nx_factory_i2c_target()` 只返回既有不可变 face。Platform 的只读 admission
先于 System clock effects，拒绝已供钟的活动 I2C、NVIC 引用和被占用的 pad。
System RUN 建立后，C cold prepare 才取得 gate、配置 AF4 open-drain、OAR1/2、
SMBus/PEC 和 IRQ priority，并逐项读回。未有请求时 ACK 关闭。
失败取得的资源保留在同一个 exact receipt；close 成功后才可回收静态存储。
关闭后保留 generation 计数，不能通过重启静态 state 重新接受旧 READY。

## 有限交换与借用

caller 先初始化 request，再通过对应 port prepare 一个有限 Core deadline：

| direction | caller spans | wire 结束证据 |
| --- | --- | --- |
| `RECEIVE` | 1..256 字节可写 RX capacity | 自身地址后的 STOPF |
| `TRANSMIT` | 1..256 字节不可变 TX | 最后一个 payload 字节后的 AF/NACK；启用 PEC 时须在 PEC 后 |
| `WRITE_READ` | 两个互不重叠的有界 span，预计算响应 | RX 后一次 Sr，再完成 TX |
| `ALERT_RESPONSE` | 无 caller payload | ARA 自身地址字节，及可选 PEC 后的 AF |

prepare 只绑定 owner、generation、System time epoch 和 span，不写 I2C/pad。
已有 System 时钟的读操作仍可维护其共享计数器，不会取得新的 clock owner。
submit 先建立 ACCEPTED borrow，再开放地址 ACK 或 ARA ALERT。
10 位读支持硬件要求的空 receiver 前导地址及随后的读 header。
DUALF 决定实际报告的自身地址，不用静态首地址冒充匹配结果。
计数使用 `size_t`，256 字节边界不会回绕；计数不含地址或 PEC。

IRQ 在已观察到 ADDR 后才允许 SR1→SR2 的破坏性清除序列。
它先读取旧 receiver 管线，再处理 Sr 的新方向；不在普通轮询中读取 SR2，
避免清除恰好晚到的 ADDR。STOPF 前先排空最多两个实际 RX 管线字节，随后
fresh SR1→CR1 清除。TX 在 ADDR 后先装首字节，后续只在 BTF 时装下一字节。
这个有意的逐字节 clock stretching 不会预取另一个 caller 字节。
IRQ 只搬字节和锁存结果，不能发布 SETTLED。

Thread service 处理 deadline、cancel 和退出。C 关闭 publisher 后，通过本
controller 的实际 reset、PE/ACK/ALERT 读回及两个 NVIC 向量的 disable、pending
clear、active=0 证明退出，再断开 active 指针并 release-publish SETTLED。
失败排空保持 QUARANTINED 和同一 borrow；调用者继续保存 request、span、state
和 receipt，待真实退出后重试。reset 只释放自身输出，不证明共享总线已经恢复，
也不会自动 GPIO clock-pulse 或重试应用数据。

每次准备、提交、IRQ payload 访问前后均核对实际 RUN/时钟和同一 epoch。
失域后不再调用旧 Core 时间；cleanup 仍能撤回自身资源。Platform suspend 使用
同一个 C close；恢复只重新建立冷态 owner 和新 epoch，不重放请求。

## SMBus 的明确边界

PEC 实际使用 CR1.ENPEC/PEC。RX-only 交换接收恰好 capacity 个 payload 字节，
随后接收独立 PEC。WRITE_READ 的 CRC 连续覆盖 W 地址、RX payload、Sr R 地址
和 TX payload，只在最终 TX 末尾追加 PEC。软件 CRC-8 独立核对接收数据，
不靠读取 SR2 取得 CRC。PEC 命令的拒绝、硬件 PECERR 和过晚的 RX 管线均不能
报告成功。

`irq_latency_us` 包含 IRQ entry 和 PEC 定位前全部非等待工作。选择 PEC 要求
该声明严格小于六个外部最大 SCL bit 周期。clock stretching 在 ADDR/BTF 点
提供背压，但不能倒退已经经过的 PEC ACK 窗口。模型和软件 CRC 不证明物理 ACK
时序，也不能保证运行期发现每一种被违反的集成预算。

TIMEOUT、PECERR、BERR、ARLO、OVR 保留独立 diagnostics。仅 TIMEOUT 的硬件失败
报告 TIMEOUT；同时出现其他损坏标志时报告 IO 并保留全部 observed faults。
ALERT_RESPONSE 是一个显式、有限、被借用的请求，才会拉低 SMBA 并响应 ARA。
内部发送自身 7 位地址左移一位，低位为零。ARA 仲裁丢失可在原 deadline 内等待
下一次 ARA；若启用 PEC，按 RM0090 的 ARLO 限制重建本 controller 的硬件 CRC
状态。它不延长 deadline、不重试 caller 数据，也不产生后台告警策略。

ES0182 §2.10.2 指出硬件无法按任意无效 byte/command 返回 NACK，因此此接口不
声称实现完整 SMBus v2.0 标准。应用命令验证、寄存器含义、报警条件、host-notify、
ARP、业务重试和错误恢复策略由外部产品提供。NOSTRETCH 始终为零，遵守
§2.10.5 对 target transmitter 的 clock stretching 工作区。
