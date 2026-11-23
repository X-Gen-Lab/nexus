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

## 固定缓冲分片访问

`nx_fatfs_read_at()` 使用明确字节偏移读取一个有限缓冲；短成功表示到达 EOF，
EOF 返回成功和零字节，越过 EOF 返回 `INVALID`。`nx_fatfs_append()` 创建或追加
一个有限块，并在原 operation deadline 内完成 sync/close。临时文件句柄在本次
调用内结束，不增加 workspace、动态句柄池或跨调用 buffer 借用。

`bytes` 先传容量／请求长度，接纳后报告 vendor 确认的前缀；参数或准入拒绝保持
原值。失败时的写入前缀不能证明持久提交，介质效果也可能超出该前缀。sync/close
失败可能需要恢复整个 volume，组件保留 fault、原 Block owner 与 workspace，
拒绝新的文件操作；应用完成使用者退出及显式卸载，再按介质合同恢复，不能盲重试。
每次调用都保留原有限 deadline，跨多块传输的总预算由应用提供。
