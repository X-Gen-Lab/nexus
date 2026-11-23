---
id: guides.io-execution
type: how-to
audience: consumer
source_language: zh_CN
---

# 为 I/O 提供执行与存储

取得 factory 接口以后，日常调用直接进入其操作表。Platform 管所选基础设施的
启动与停止；应用负责任务、调度、队列、service 频率、缓冲与恢复决策。

## 一个接口，一个明确执行者

轮询模式在所选预算内完成操作；IRQ／DMA 模式需要按接口合同提供通知、service 或
等待适配。多个 producer 共享 UART／SPI 时，选择显式 owner，避免不同任务同时
推进同一控制器状态。平台不会隐式创建 worker，也不会启动 scheduler。

普通接口不为未选异步模式分配最大状态或缓冲。先从实际任务选择有限请求或连续流，
再确定容量和执行方式，具体模式以支持矩阵和完整配置校验为准。

## 有限请求的最小闭环

1. 应用准备请求、payload 和同一单调时间域的绝对 deadline。
2. `REJECTED` 不保留调用者引用；处理错误后可按合同重试。
3. `ACCEPTED` 开始借用，持续推进其执行者，观察请求状态。
4. acquire 观察 `SETTLED` 后读取最终结果，确认控制与观察者也已排空。
5. 再归还或复用相应存储。

通知是提示，请求谓词是完成依据。DMA 搬运完成、线上完成和引用排空是不同事件。
取消、超时和 `QUARANTINED` 都不授权提前释放；失败后保持存储，继续既定排空或
进入产品恢复流程。[所有权合同](../explanation/ownership.md)定义完整状态规则。

## 连续流与停止

按流合同取得并归还 loan，借用期间不改数据。容量耗尽时处理 loss／backpressure；
`BUSY` stop 仍可能保留已发布的借用。归还、排空并 join 相关 owner、producer 和
观察者之后，才重新使用其状态。

停止应用时先关闭新请求，再请求取消并继续 service，等待真正排空。最后按后端
允许的顺序停止 Platform。FreeRTOS scheduler 已尝试启动后不能按“任务从未运行”
逆序丢弃状态；内核仍使用的 Tick／时钟必须保留，见
[OS 停止](os-shutdown.md)。

## 五条最小使用路径

配套 [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples) 维护实际应用 C，
平台不复制另一份应用实现。阅读时核对例库 gitlink 与 SDK lock；主工程的 UART IRQ
或 SPI polling 装配不能代替独立 DMA 装配。以下每条都需明确配置、接口、存储、
执行者、正常顺序、失败退出和实际验证范围。

### 同步 GPIO

`apps/platform_reference/` 与起始模板使用所选 Board 的已审极性。取得 strong
logical signal 后调用 `nx_gpio_signal_write` 或 `nx_gpio_signal_read`；业务不重复
pin mask 和极性。
未知极性不从初始电平猜测，逻辑 inactive 也不表示高阻或电气断能。

这条路径只需同步调用和明确写入者，不承担 request、队列或 worker。停止写入者后
再停 Platform；启动或停止失败保留其 primary、cleanup 和 remaining effects。

### 有限 UART DMA

选择真实 `dma-tx` 装配，调用者持有 request、payload 和同一时钟域的有限绝对
deadline。先用 `nx_deadline_finite_after` 检查有限预算，失败时拒绝本次准入。
请求接受后由同一执行者持续推进；DMA TC、UART TC 和 acquire 观察 SETTLED
分别证明内存搬运、线上完成及借用结束。该模式原有 RX 职责仍由同一 owner 持有。

取消或超时仅请求终止。每次推进返回进行中或保留责任时，原 request、payload、
回调和硬件资源继续存活；QUARANTINED 不允许直接退出存储作用域。仅使用该模式
实际提供的恢复操作，无恢复能力时由外部产品决定停机或复位。

### 有限 SPI DMA

从 controller 取得已配置 endpoint，起始路径只用一个执行者。调用者持有 request、
段表与 TX/RX span，并明确帧宽、数量单位、CS 时序及允许的重叠关系。需要多个
producer 时才接入既有 SPI owner，不为单次 DMA 强制增加队列和线程。

双向 DMA 的 memory TC 不代替 wire idle 或 CS 合同完成。取消、错误和排空期间仍
保留全部被接受的 span；acquire 观察 SETTLED 后读取最终结果和实际传输前缀，
失败结果也不表示零效果。

### 连续 ADC blocks

`apps/stream_recovery/` 维护采集和退出组合；`recipes/control/` 另演示数据处理。
配置明确 scan 格式、硬件触发、rate、service 上界和 DMA 可达存储。容量包含硬件
目标、替换块和消费者保留；CCM 等 CPU-only 内存不能作为 DMA 目标。

按 service、acquire 完整 block、处理、release 原 token 推进。EMPTY 不复用旧样本，
LOSS 和错误由应用明确决定是否停止。调用所选停止操作，由 provider 先撤新触发；
调用者继续排空 filling、READY 和 BORROWED。release 或 stop 失败保留原 token、
状态和 executor，不清零重启。

### 连续音频 blocks

按精确 I2S/SAI 装配选择 layout、主从时钟、rate 与调用者 block 存储；frame、
sample 和 DMA container 的单位分别读取公共合同。实际提交或取得完整 block 后，
按对应方向归还借用；TX 与 RX 不共用一份已回收证明，duplex 停止须两侧真实退出。

underrun、overrun、缺少替换块或外部时钟停止走显式错误与退出路径。平台不自动
填静音或丢弃借用数据。有限运行窗口结束只停止新增工作；每次 step 的工作量上限
不构成物理停机时限，失败排空仍保留原 owner 与数据。

上述用例的软件行为、真实 provider 的 ARM 链接和物理时序分别验收。Native mock
只替代外部端口，不代替应用退出逻辑，也不建立真实 DMA、电气或采样时序资格。

## 查看诊断

`nx_io_diagnose` 复制当前请求和流事实，不执行恢复，不归还借用。`unknown_facts`
保留未知信息；`SERVICE_OVERDUE` 需要同时间域内实际时间戳和明确 service 预算。
`nx_dma_storage_validate` 校验完整显式存储预留，不只校验首地址对齐。

用[异步请求教程](../tutorials/async-io.md)练习成功与失败路径，公共签名见
[I/O API](../reference/api/io/index.md)，实际延迟与吞吐测量见
[性能与 HIL](performance-hil.md)。
