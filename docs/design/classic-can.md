# Classic CAN 的平台机制与接入边界

当前公共接口是 `nexus/io/can.h`，生产实现归属 `soc/stm32/stm32f4/`，
精确适配 STM32F407VET6／ZGT6 的 classic CAN。家族目录不表示所有 STM32F4
器件已完成适配。Native 位于 `tests/support/native/`，仅保留为显式启用的
主机测试模型。当前硬件模式为 CAN 2.0A/B：11/29-bit ID、DATA/RTR、0–8-byte
DLC；接口拒绝 CAN FD、大于 8 的发送 DLC 和越界 ID。

## 静态对象与执行

公共 `nx_can_port_t` 只含 readonly ops 与私有 context。三个真实硬件邮箱对应
三个静态 slot，没有软件 TX 队列、分配器、全局可变 registry、worker 或必选 OS。
接收事件环由装配提供精确容量。未选实例不生成状态；当前生产单元的 ARM objects
没有 `.data`/`.bss`，最终未选 ELF 的 GC/资源验证由实际装配构建建立。

一个特权任务执行者序列化 submit、cancel、service、read、recover 和 stop；入口
拒绝 Handler、非特权或已有 PRIMASK/BASEPRI/FAULTMASK 屏蔽状态。四条 IRQ 执行
有界邮箱/接收处理：最多三个邮箱、两个 FIFO 各三个初始帧。MCU 的寄存器访问
是 direct volatile MMIO；Native 只在模型编译时替换外部寄存器/NVIC 叶端口，
生产 MCU 不携带模型访问表或运行时模型分支。

IRQ wake 是提示，不替代 request predicate。绑定时读取四条实际 IRQ priority
和 grouping，逐条验证 policy/syscall ceiling；不修改全局 grouping。sink 必须
同时支持任务 service/cancel 与 IRQ 通知。捕获 sink 后执行者计数继续保留借用；
stop、detach 或 recover 在通知尚未返回时报告 BUSY。

## 共享过滤器域与硬件事实

每个装配显式生成一个 `nx_can_filter_domain_t`，拥有主时钟、次时钟、28 个共享
filter banks、split 与 immutable filter plan。无其他 controller 持有 active bit
时才允许 domain stop 释放时钟。STM32 CAN2 依赖 CAN1 的时钟/过滤器域。
停止一个实例不 reset 主 controller，也不覆盖 peer filters。

| SoC | 控制器 | CAN 输入时钟 | TX/RX0/RX1/error IRQ |
|---|---|---:|---|
| STM32F407 | CAN1 / CAN2 | APB1 42 MHz | 19/20/21/22；63/64/65/66 |

私有绑定用锁定 SDK 的真实寄存器 offset、bit masks、mailbox/FIFO/filter stride、
CPU IRQ 数和 NVIC priority bits 做编译期/启动期检查。CAN 输入没有 APB timer 的
倍频规则。prescaler/BS1/BS2/SJW 使用物理计数，必须精确满足
`clock_hz = bitrate × prescaler × (1 + BS1 + BS2)`；不搜索或替换参数。
第一版采用单次发送 NART，关闭 ABOM，RX 使用 locked FIFO；过滤器只实现 32-bit
mask，显式匹配 IDE，可选择匹配 RTR。过滤器归属与 bank/split 范围先验证再写。

安全装配顺序是：reviewed Board electrical/pin handoff → domain initialize →
各 controller initialize → 外部执行者启动。cold initialize 在 acknowledged
init 中证明没有未知 TXRQ，并 drain retained RX FIFO；IER、四条 IRQ 使能与
priority 读回不符会保留 closing/retained 责任。退出顺序是各 controller stop
成功 → domain stop 成功 → Board pin/electrical lease release。失败时不得回收
provider、ring、filter plan 或 sink。

## 完成、取消与恢复

prepare 在 READY/已消费 SETTLED storage 中复制 value frame；ACCEPTED 后借用整个
request 到 acquire-observed SETTLED。第四个请求、非法 frame/context/deadline、
以及 request 与 controller/ring/domain/filter plan 的重叠都拒绝且不产生借用。

线上完成必须同时观察 RQCP、TME 和 TXRQ clear；只有 TXOK 能证明发送成功。
清 W1C completion 后还要读回 RQCP clear，随后解除 slot，最后 release-publish
SETTLED；这之后 provider 不再访问 request。cancel/timeout/bus-off/stop 请求
abort 并等待真实 acknowledgement；deadline 不能撤销借用。drain 超时或 W1C
清除失败保留 QUARANTINED。TXOK 与 cancel 竞争时保留真实 SUCCESS，不保证帧
从未上线。RTR 成功的 transferred 为零，DATA 为 DLC。

bus-off/TEC/REC/last-error 的 authoritative snapshot 不依赖 RX ring 是否有空位。
公共 `last_error` 是单字节 `nx_can_protocol_error_t`：NONE、ACK、STUFF、FORM、
BIT_DOMINANT、BIT_RECESSIVE、CRC 或 UNKNOWN；provider 转换硅片编码，应用不解释
LEC 位。BIT 名称表示发送者期望的电平，分类不标识某个 frame，也不证明当前总线状态。
ABOM 关闭；recover 是显式 controller-local init，在所有 TX loan 与 publisher
退出后执行，不重发原帧，不 reset 活跃 peer。未完成硬件/IRQ/FIFO drain 的 stop
返回 BUSY，保留原 storage。成功 stop 后 read_events 拒绝 STATE，不接触已释放
接收环。

## 接收与丢失事实

接收保存 value frame、逻辑 `filter_tag`、raw DLC 与 IRQ acquisition timestamp。
raw DLC 9–15 被规范为 8 并加标志，最多复制八字节；不解释为 CAN FD。时间不是
物理 SOF timestamp。RFOM 必须读回释放完成后才能发布一次事件；卡住时停止 admission
并保留责任，不能重复发布同一个硬件邮箱。

每条 filter 可显式设置 1–65535 的 `tag`，同一 controller 内不得重复；省略时按该
controller 的 filter 声明顺序从 1 编号，与物理 bank 无关。需要跨配置调整保持规则身份
时应显式填写。无法识别的硬件 match 保留 frame，返回 `filter_tag=0` 和
`NX_CAN_FILTER_UNKNOWN`，不猜测匹配规则。应用按 tag 执行策略前须检查该标志。

F4 私有配置按 RM0090 Rev22 第 1082 页 Figure343 计算 `match_index`：两个 FIFO
独立编号，未启用的 bank 也占位置；本实现选中 bank 为 32-bit mask，其余为未启用的
16-bit mask/FIFO0。生成器只输出不可变对应关系，C 冷态预检再次核验，RX 用同一
controller/FIFO 的已审对应关系转换 tag；没有新增运行期注册表或可变映射数组。
RX 查找为所选 filter 数量的有界线性工作，最多 28 条；每次 IRQ 至多处理六帧。

事件环不覆盖未消费事实。软件丢失计数是精确值；硬件 FIFO overrun 只报告下界。
locked FIFO 中先前保存的帧先于 overrun loss marker；pending loss 排在旧 buffered
事件后、未来新帧前。饱和计数有显式标记。错误状态单独锁存，不借事件队列容量
决定 completion 或 bus-off 是否存在。

## Board 来源与软件/物理资格

启明 V3.1 原始 tree `08c5f513f1677388fcacaa519dca0c9ef00b6d2e` 的 CAN1
示例 blob `8731616cf0d5bf02917c76463c49e722b39eb415` 给出 PA11 RX/PA12 TX AF9；
CAN2 blob `ef8da679c5e7c7163fe2a48a80b6ce8f9d6ecfaa` 给出 PB12 RX/PB13 TX AF9。
[用户手册](https://github.com/petercwq/STM32407/blob/08c5f513f1677388fcacaa519dca0c9ef00b6d2e/启明欣欣407开发板(高配版)V3.1例程使用手册.pdf)
printed18/physical20 给出 TJA1050 U9/U11、5 V supply、RS 接地、120 Ω R35/R49。
P21/P15 是人工复用跳帽；不能虚构软件 standby GPIO，也不能把软件 pin claim
当作实物跳帽证明。CAN1 与 USART1 RTS/CTS、USB FS 的 PA11/PA12 必须拒绝并选。
天空星当前没有审阅的外接 CAN transceiver 路线，不猜 Board binding。

F4 绑定位于 `soc/stm32/stm32f4/drivers/can.c`，控制器机制位于
`soc/stm32/stm32f4/drivers/ip/classic_can/`；真实硬件定义来自锁定的
`vendors/st/cmsis_device_f4/`。Native 叶端口位于
`tests/support/native/drivers/can*`。当前 GoogleTest/GoogleMock、配置／factory／
向量集成与精确 ARM ELF 分别验证其合同，不由一个旧开发 harness 代替。

[原始跨系列开发证据（历史）](https://github.com/X-Gen-Lab/nexus/blob/83a27dfbee4e8fbf4d35875b1e9db85af7516eec/tools/testing/evidence/can-iteration/manifest.json)
保留当时的固定源码哈希、工具身份、RED／GREEN 与执行范围，原始字节不修改。
其中已退役系列和旧接口的计数不属于当前 F4 候选验收；当前用例数、资源与完整
门禁结论必须绑定本次提交，不能继承历史通过状态。

物理 CAN 波形、晶振/传播/采样点容差、termination、跳帽/电压、transceiver、
仲裁/ACK/error 注入、bus-off 恢复时间与最大 IRQ 屏蔽/WCET 均未执行 HIL。
有限 poll_limit 是软件工作预算，不证明实物 init 最长应答时间；寄存器模型也不
证明总线故障或 store corruption。外部 CANopen/产品重试/总线管理策略归产品工程。
