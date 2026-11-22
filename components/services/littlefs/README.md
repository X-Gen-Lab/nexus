# LittleFS 显式 Flash adapter

`nexus_add_littlefs()` 选择锁定的真实 LittleFS；未选择时不编译 vendor。公共 API
使用 Nexus metadata、`nx_fal_t` 和 caller workspace，所有 vendor 状态与 read/program/
lookahead/file cache 都在该 workspace 内，固定 cache 为 256 bytes，无 heap。

产品提供独占、均匀擦除的完整物理 region。一个逻辑块对应一个完整物理 erase sector，
program unit 必须整除 cache/block；不把混合 sector 伪装成更小的可独立擦除块。所有
回调通过同一 FAL/Flash owner，整次 mount/file/format 共用一个有限绝对 deadline。

init 不读写；mount 失败不会 format。format 是独立显式调用，仅对未挂载选区执行。
put/get 在调用内关闭文件，不保留业务数据指针。put 是单文件替换，不能作为多文件或
数据库事务。get 容量不足不复制前缀，报告完整所需长度。错误输出无效；修改失败须
完成物理恢复并重新挂载，不能把 timeout 当回滚。

产品负责目录策略、保留、迁移、认证与执行调度。本组件没有 VFS、后台线程或重复
分区管理；region 不能与 FlashDB、Storage 或 OTA 共同管理同一字节。Native 软件媒体
结果不建立实物 Flash 掉电/耐久资格。

## 固定缓冲分片访问

`nx_littlefs_read_at()` 从明确字节偏移读取至多一个 caller buffer；文件尾返回成功和
零字节，越过文件尾返回 `INVALID`，不会创建稀疏内容。`nx_littlefs_append()` 每次
追加一块并执行 sync/close，文件不存在时创建。两者只在调用内使用临时文件句柄，
复用现有 file cache，workspace 和实例大小不增加。读取大文件时，应用逐次推进偏移，
无需提供完整文件大小的 RAM。

`bytes` 输入容量／请求长度，接纳后输出 vendor 确认的读／写前缀；参数或准入拒绝
保持原值。读取发生错误时，只有该前缀经过 vendor 确认，其余输出无效。追加失败
可能已经改变介质，前缀不是持久提交证明，也不能据此直接重发剩余内容。每次完整
调用共享原始有限 deadline；产品安排跨调用的总预算、保留和恢复策略。关闭失败与
物理错误继续按原 owner/FAL 合同处理，不以函数返回代替恢复与卸载。
