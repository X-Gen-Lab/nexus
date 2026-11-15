# 双 bank 整体数据存储

`Nexus::Storage` 在显式窄 Flash port 上实现双 bank、commit-last 的完整 blob。
`Nexus::FlashStorage` 连接公共物理 Flash region，Native 图中的 `Nexus::FileFlash`
提供真实 host file 与受控故障模型。它们不选择产品分区、迁移、认证、写入频率或
恢复策略。[storage.h](include/nexus/components/storage.h)与各 adapter 头是调用合同。

## 布局、端口和资源

调用者提供互相独立可擦除的两 bank、erase-aligned offset／bank size、metadata 与
workspace。workspace 至少为 `max(32, port.program_size)`，与实例／payload 分离，
从 open 到后续所有 load／save 持续借用。端口同步并在返回结束 buffer 访问；program
仅按完整对齐单位把 1 改为 0，erase 按完整 uniform unit，sync 建立其端口所承诺的
先前成功写入持久性。失败可能已改变部分 unit。

wire format v1 为明确 little-endian、CRC 保护，不认证。容量由实际 bank 几何扣除
header／对齐后得到，没有 heap、最大 pool 或隐式保留区域。

## 打开、读取和提交

open 在单 task 串行域内有界扫描两 bank 的 payload，恢复有效代次或空 store。
只有无效 committed generation 或歧义时返回 CORRUPT，不能当成空数据库覆盖。
load 可先查询所需容量；成功才保证输出有效，失败可能部分写输出，但不保留输出指针。

save 向 inactive bank 写完整 blob，sync 后最后写 commit marker。修改性 I/O 失败使
opened 失效；重新 open 判断哪个 commit 实际存活。marker 失败可能已提交新 blob，
不能直接假设只有旧提交生效并重放。完整 workspace／port context 在所有同步访问者退出前
保持有效；没有通用强制取消或自动释放底层 adapter 的操作。

物理 Flash adapter 必须遵守精确 geometry／program_flags，特别是同一 unit 的擦写
责任；它不隐藏 program-pulse 历史。FileFlash 在会合全部 store 用户后才可关闭。

[Settings](../settings/README.md)在此层暂存键值快照。host 掉电故障模型和 CRC 不
证明实物 Flash 掉电、电气耐久、认证更新或产品安全资格。
