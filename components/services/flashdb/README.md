# 显式工作区 FlashDB

选中 `Nexus::FlashDB` 才准入并编译锁定的真实上游算法；公共头仅含 Nexus 类型。
产品提供 `nx_fal_t`、对齐 workspace、metadata 和完整调用期间稳定的
`nx_flashdb_operation_t`。实现 `nx_flashdb_operation_get()` 返回本次传入的精确
scope，在同一个物理 owner 的外部串行域中调用。库没有线程、锁、heap 或设备注册表。
所有实例的首次 open 在发布给其他执行者之前串行完成；上游初始化仍有共享的
`log_is_show` 标记。公开包装只使用 blob API，不开放上游静态 string-result buffer。

## 固定存储 profile

当前 profile 为 `FDB_WRITE_GRAN=32`、64-bit timestamp、KVDB 和 TSDB、关闭 KV/sector
cache 与 transient read cache。它接纳能够执行 4-byte 对齐完整写的选定非 ECC provider，
包括独立验证的 x32 片内软件模型和 byte-program NOR 软件模型。FAL region 必须完整、
均匀可擦除，至少两个逻辑 sector；真实器件、供电与 physical HIL 不由模型建立。

写粒度是全库编译配置，不能给不同 DB 动态改变。更改粒度、timestamp 位宽、固定 blob
配置或上游格式均须独立评估旧数据兼容；不能混链不同配置但相同全局符号的两份库。
workspace 大小和对齐由查询函数给出，调用者选择静态存储。不要使用 README 示例跑分
或“RAM 几乎为零”估计预算；实例、workspace、调用栈和最坏扫描/GC 应分别核算。

## 打开、格式化与失败

init 只连接 metadata。existing-only 对全擦除介质返回 EMPTY；create-if-erased 只有
在整个选区均为 FF 时才允许初建。已有未知/损坏格式不会被当作空库覆盖。format 是
独立的显式破坏操作，只允许已关闭实例；默认值、schema migration 和授权由产品负责。

修改性 I/O 失败关闭逻辑准入。保留 workspace 和同一硬件 owner，先处理物理恢复，
然后重新 open 判断存活数据。写入失败可能已提交新值；不承诺回滚，也不直接重试原
状态写。read 失败也不伪装成空库。上游部分接口使用长度/指针而非详细错误，桥保留
首个底层错误，防止其后继续修改介质。

## KV 与 Settings 的合同

KV set/remove 立即持久化独立 key，可能同步触发 GC；不承诺多个 key 的整体事务。
既有 `Nexus::Settings` 保持 uint32 key、有界 RAM staging、显式 commit 完整快照和
schema 拒绝合同。需要多 key 一起提交时继续使用 Settings/Storage；本组件没有悄悄
替换它们的后端或格式。产品迁移须读旧格式、验证 schema、在独立新 region 写入并
验证，最后由产品切换使用，不能原位覆盖唯一旧副本。

## TSDB 与持久业务记录

TS append 要求非负且严格递增的 timestamp；同 tick 高频事件和 RTC 回拨由产品的
时间/序号策略处理。rollover 关闭，满后显式返回容量错误，避免默默覆盖未处理数据。
visitor 借用 scratch，只在回调期间有效，不允许重入。状态只能保持或向前进展；重复
设置已经确认的同一状态不再 program，同一失败状态不可绕过恢复后盲目重放。

`persistent_record.h` 提供明确 LE/version/schema/record-ID/CRC32 封装。CRC 检测损坏，
不认证；TSDB 自身不被宣称拥有 KVDB 的 payload CRC。记录 ID 和 USER 状态不是可靠
outbox：需要可靠上传的产品仍负责远端幂等、ACK 持久化、重试、满载背压和未确认记录
保留。append 成功只证明本地库调用完成，不证明远端收到。

## 现有映射与 OTA 客户端

MappedStorage 的 CPU/XIP/DMA loan 必须先按原合同释放并会合，才能由共享 owner 进入
FlashDB writer；数据库不会撤销映射 loan 或创建第二硬件 owner。停止顺序为关闭新
业务、退出 visitor/调用、释放 loan、完成物理清理，再退出 workspace/provider 生命周期。
原 MappedStorage API 和退役/epoch 规则保持不变。

OTA 继续使用现有 MCUboot flash map 与产品唯一 layout；boot/app 的 slot/trailer
和数据库 region 一起校验，无重叠或隐含保留。数据库和文件系统不修改 bootloader、
不获得候选槽授权，也不把普通 CRC 升级为签名或防回滚机制。
