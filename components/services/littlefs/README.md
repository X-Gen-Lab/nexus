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
