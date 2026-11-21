# 显式 Flash region 存储桥

`Nexus::FAL` 提供无分配 C11 的 `nx_fal_t`。产品提供已存在的物理 Flash
face、完整 sector region、同一时间域的 clock 和每次事务的一个有限绝对 deadline。
它不创建硬件 owner、分区注册表、worker、锁或默认产品保留区。

`nx_fal_layout_validate()` 校验产品给定的 region 数组和同 context provider alias
之间的重叠；它不保存数组或建立另一份布局。bind 要求完整、均匀的物理擦除 sector。
混合 sector 和 `NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE` profile 明确拒绝。

begin/end 包住一次上层完整操作。所有 read/program/erase 共用该绝对 deadline，
回调只返回完整字节数或负数，拒绝溢出和向外扩大擦除。修改失败锁存错误、关闭新准入，
保留同一个物理 owner；recover 不重放数据，也不把原失败升级成成功。产品仍须重新
打开逻辑数据库，判断哪些记录实际存活。deadline 不能抢占已开始的物理 pulse。

## 与上游 FAL 的范围

FlashDB adapter 私下复用其 bundled portable FAL 头的选定 ABI，返回每实例稳定的
partition/context，直接调用本组件；没有编译上游全局动态分区管理实现，也没有宣称
替换其 shell、RT device、动态表或全部公共 API。FlashDB 初始化时由产品的显式
operation getter 提供上下文，后续访问由数据库所持有的稳定 partition 找回原实例。
没有 library current-device/current-deadline 变量。

静态检查只能证明布局和软件行为。实际供电、Flash stall、watchdog、擦除耐久和
物理掉电资格仍属于产品。外部 SPI Flash 的同步 read 也需要预算：SFUD 使用产品显式
设定的同一 read deadline；不要为每个底层 callback 延长预算。
