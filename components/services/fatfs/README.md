# FatFs 已有 Block owner adapter

`nexus_add_fatfs()` 选择锁定 ST 官方 FatFs 源。profile 为单 volume、512-byte sector、
short name、无动态分配、无隐藏 OS lock。产品提供已激活的真实 `nx_block_port_t`、
明确 partition sector 范围、caller workspace 和 operation getter；不会自动激活或
替换 socket 上的介质。

`nx_fatfs_operation_get()` 在 vendor disk callback 期间返回本次精确 scope。该 scope
包含同一个完整调用的有限绝对 deadline；read/write 检查真实 completed-block prefix，
只有完整完成才返回成功。bounce sector 放在 caller workspace 中，满足 provider buffer
alignment；sync 通过原 Block owner 证明其协议层 readiness，不承诺额外掉电原子性。

mount 失败不 format；format 由产品显式授权。put/get 使用已有 volume，在调用内结束
全部 file/buffer 借用。写失败可能已经改变介质，不能直接重试或假设文件/FAT 回滚。
先按已有 Block 合同处理 uncertain owner，再由产品重建/恢复 volume。卸载不停止或
移除 Block owner，也不替代所有使用者退出会合。

本 adapter 只用于已经提供 sector 语义的介质软件/硬件 profile；不把原始 NOR 擦写
直接冒充磁盘，不新增 FTL 或 VFS。FAT 互操作需求不等于 journal/database 事务。
