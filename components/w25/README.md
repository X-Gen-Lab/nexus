# W25Q128JV 单线 SPI 存储组件

`Nexus::W25` 是可选、无动态分配 C11 组件，使用已有 polling
`nx_spi_endpoint_t`。它不创建 bus、worker、task、lock、设备注册表或产品分区；
vendor controller 和 pin 所有权留在所选 SPI endpoint 后面。

## 精确 profile 与来源

必须显式选择 **W25Q128JV**：16 MiB、JEDEC `EF4018`，使用单线 03h read、02h
page program 和 20h 4 KiB sector erase。EF4018 必要但不能唯一证明全部订货后缀、
FV／JV revision 或焊接型号。不接纳 Q64、EF7018 DTR、SFDP、四字节寻址、quad／QPI、
status／protection 写入、erase suspend／resume、power-down 或 reset，也不自动回退。

来源为 Winbond **W25Q128JV Revision M, 24 December 2024**，Industrial／Industrial
Plus grades：

- [官方数据手册](https://www.winbond.com/resource-files/W25Q128JV%20RevM%2012242024%20Plus.pdf)
- SHA-256：`b51bb4303e17a17750c9cbcafc9a776dea842a8971d7cdd5edc70ff738efb1fe`
- 已审核 power-up／write protection、status 1／2／3、03h／02h／20h／06h／04h／9Fh、
  read-status，以及 page／sector 几何。

下载原 PDF 保存在执行证据中，不放源码 Git。

## Transport 与借用

调用者提供稳定、awake、mode 0 或 3 的单线 SPI endpoint，频率不超过 03h read 的
50 MHz。endpoint 负责完整 CS、硬件排空及自己的保留恢复故障；组件不调用共享
bus reset 或 stop。

polling 合同在每次返回结束全部 CPU buffer 访问，包括失败；CS 撤回失败仍可能
保留 controller／pin 恢复责任。因此同步 read／program 返回后不保留 payload 指针，
但 Flash program／erase pulse 不同：state、endpoint 和 clock 保留到后续 status
进度证明静止。transport 错误或 timeout 不清除此责任。

操作 task-only 串行，调用者排除 foreign Flash command、pin／clock／power 改变和
并发 bus 使用。endpoint、ops、opaque context 保持存活且与可变状态和 payload
分离。实现检查已知对象和每个非空 context 首字节；完整 opaque context 范围仍由
调用者保证，不是任意指针的内存沙箱。

## 有界操作

| 操作 | 当前边界与完成含义 |
| --- | --- |
| `nx_w25_init` | 对明确 profile、region、power／clock 事实做只读 probe，不 reset／wake／unlock |
| `nx_w25_read` | 1～252 字节，一个 03h CS 区间；失败／short read 保持输出 |
| `nx_w25_program_start` | 单个 256 字节物理页内的 1～252 字节；SUCCESS 只表示已发命令 |
| `nx_w25_erase_4k_start` | 产品区域内恰好一个对齐 4 KiB sector |
| `nx_w25_service` | 至多 4 次短 status／disarm 交换，不忙等 |
| `nx_w25_get_status` | 不做 I/O，观察 pending 责任和最后修改结果 |
| `nx_w25_stop` | pending／uncertain 清除后才无 I/O 解绑 |

4 字节 header 加 252 字节 payload 正好满足 endpoint 的 256 字节上限，不偷偷发送
260 字节整页事务。较大工作由调用者按 region／page 边界拆分。offset 相对产品提供
的非空、erase-aligned region，平台不预留或选择分区。

read 局部数组为 512 字节，program 为 256 字节，另加实际调用栈；这是源码边界，
不是实测 task stack peak。不增加最大对象池、heap buffer 或保留 payload copy。

只有实际 BP、CMP、WPS 均为零才允许写入，不修改 protection 或 QE。array command
前必须观察 WREN 已锁存。发命令后 idle WEL 报错，并用 WRDI 加 status readback
解除武装。WIP0／SUS1 不算完成，也不自动 resume。

## deadline、失败与最终结果

调用者建立真实上电／写入等待并提供绝对 `ready_at`，此前 init 返回 BUSY 且不做
SPI。clock 已有效是前提，零值不证明 readiness，domain／epoch 标签也不是运行时
证明。修改 deadline 包含后续 Flash busy 时间；service 使用独立有限 cleanup
deadline，延长 probe 不延长原 operation。BUSY 保留责任，status 可已显示锁存
TIMEOUT；新 cleanup 预算可证明稍后静止并返回原错误。pending 期间 clock 回退或
非有限，存储继续保留给外部系统恢复。

WREN 失败也可能留下 armed 状态，program／erase 传输失败可能已改变阵列。
`may_have_modified` 表达这种可能，不是字节计数证明。意外观察到活动也关闭新准入，
直到 status service 证明静止。timeout 不释放 state，不取消不可逆 pulse。

命令完成不是数据验证、原子替换或掉电持久性。产品遵守 NOR 1→0 规则，提供已擦除
目标或单独审核更新策略；回读／CRC、擦除验证、崩溃恢复、磨损和布局在外部。恢复
idle 不能重建中断写入结果。

## Board 与证据边界

核心板图把 W25Q128 标为未焊，高配板 W25Q128 标题又与 64 Mbit 注释矛盾；两者
都不自动选择 JV profile，实际器件、焊接和电压待确认。host 协议模型和 ARM／SDK
链接分别证明软件属性，不证明物理时序、寿命、电气或掉电行为；HIL 未执行。
