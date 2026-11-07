# 有界 Settings／KV 快照

`Nexus::Settings` 在已打开的 `nx_storage_t` 上暂存一份有界键值快照。assembly
component 列表可选择 `settings`，CMake 管理 Storage 依赖；
[公共头](include/nexus/components/settings.h)定义操作与失败合同。

调用者提供 metadata、独立 image buffer、容量、非零条目上限和产品定义的
`uint32_t` schema。key 是产品分配的 `uint32_t` 标识，value 是复制的字节串，允许
空值。组件没有堆、worker、全局 key 注册表或指向输入 value 的保留指针；全部操作
在 task 中与底层 store 串行执行。

## 生命周期

1. 先打开 Storage，再以未使用或已关闭的 metadata 打开 Settings。没有 blob 时只
   暂存空 image，不提交。未知 schema／格式或损坏载入返回错误，不伪装为空数据库。
2. Get 复制 value；查询大小或容量不足时报告所需长度。Set／remove 只改变暂存。
   被拒绝的变更保持条目和 dirty 状态；赋相同值不增加写入责任。
3. Commit 显式通过 Storage 保存完整快照；干净暂存不写入。修改性 I/O 失败会关闭
   Settings，须重新打开 Storage 和 Settings，判断旧或新提交实际存活；不能把失败
   当成安全回滚后直接重试。
4. Close 丢弃未提交暂存，不做 I/O，Storage 继续保持打开。底层 workspace 生命周期
   仍按其自身退出合同管理。

默认值、迁移、授权、提交频率、Flash 保留区和恢复由产品负责。格式没有认证；CRC
和主机 NOR 掉电模型不能建立签名更新或物理 Flash 掉电资格。实际执行与剩余退出见
[原交付台账](../../docs/status/ledgers/platform-delivery-completion.md)。
