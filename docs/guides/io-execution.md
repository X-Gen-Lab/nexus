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

## 查看诊断

`nx_io_diagnose` 复制当前请求和流事实，不执行恢复，不归还借用。`unknown_facts`
保留未知信息；`SERVICE_OVERDUE` 需要同时间域内实际时间戳和明确 service 预算。
`nx_dma_storage_validate` 校验完整显式存储预留，不只校验首地址对齐。

用[异步请求教程](../tutorials/async-io.md)练习成功与失败路径，公共签名见
[I/O API](../reference/api/io/index.md)，实际延迟与吞吐测量见
[性能与 HIL](performance-hil.md)。
