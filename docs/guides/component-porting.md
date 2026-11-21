---
id: guides.component-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 选择、装配和接入通用组件

本指南用于在已有外部 CMake 工程中选择组件、提供静态存储和执行进度，并按其合同
停机或保留恢复责任。工程先按[SDK 消费指南](cmake-consumption.md)取得选定平台，
再连接组件；客户私有流程、寄存器策略、产品 worker、分区和恢复决策留在外部工程。

## 1. 选择职责与实际 profile

先从[组件 API](../reference/api/components/index.md)的选型表确定公开 target，再读
其叶 README 和公共头。六分类分别是 algorithms、runtime、buses、devices、services
与 protocols，不表示应用必须启用所有类别。

| 需求 | 选择前必须确定 |
| --- | --- |
| 算法／控制 | 样本单位、实际间隔、系数约定、数值范围和每实例历史 |
| FSM／周期／timer | 唯一执行者、精确容量、事件借用和时钟 epoch 切换 |
| 软件总线／器件 | 精确 part、供电与 GPIO 电气事实、同步／异步端口合同 |
| 数据库／文件系统 | region／volume、擦除／program 几何、profile、workspace 和格式策略 |
| 日志／诊断 | 普通 task 或异常上下文、复制来源、sink 容量和排空条件 |
| 协议中间件 | 被选依赖及完整软件闭包、外部业务政策和硬件支持范围 |

例如平面 FSM 不等于层次状态机，packet PEC 不等于完整 SMBus 电气控制器；
W25／SFUD 选择明确 W25Q128JV，JEDEC 相同不证明任意后缀适用。未有精确需求的
CANopen profile 与新增器件保持 `not_selected`，不通过空壳 API 暗示支持。

## 2. 静态选择软件与端口

普通组件直接经固件 `LIBRARIES` 或已有 target 的 `target_link_libraries()` 消费，
不为每个辅助函数建立 TOML 类别。需要供应商源码的 CMSIS-DSP、数据库、文件系统、
Shell／RTT 等 profile 必须显式选择，随后才验证锁定来源并编译其叶目标。
库存、gitlink 或头文件存在不允许初始化未选依赖。

TOML 选择 transport 控制器与 endpoint，应用选择器件和业务角色。factory 只返回
已有静态 face，不启动 GPIO／总线；组件不能把第一个 endpoint 当成整个控制器。
复用[FIG-04 的接口结构](../explanation/factory.md)，不建立第二份设备注册表。

## 3. 提供存储、布局与生命周期

按实际 profile 的大小／对齐查询提供 workspace，按实际容量提供 slot、滤波历史、
timer、sink 和消息存储。只读系数／表可以共享，可变历史和控制状态各自独立；
初始化、reset、回调和 teardown 由同一外部串行域管理。

workspace 查询是运行期接口，不能直接作为文件作用域静态数组的大小。需要静态
分配时，应用按已验证的选定 profile 提供明确尺寸和对齐预算的 arena，并在启动时
用查询结果核验；不足时拒绝准入，不转为堆分配。这个产品预算不代表所有 backend
共用的最大对象大小。

普通调用方只包含 Nexus 公共类型。`shell_storage.h` 等明确的 supplier-layout 头
只在静态装配点使用其独立 storage target；不能把它们的第三方字段传给业务代码。
保留配置、face、context 和 workspace，直到该组件的关闭及全部使用者 join 条件成立。

存储应用显式选择互不重叠的 region／partition，连同 MCUboot slot／trailer 一起
验证布局。均匀 sector adapter 不能假装支持混合 sector 或不兼容 ECC program。
open／mount 失败保持原介质，不自动 format；创建、格式化和迁移需独立产品动作。

## 4. 推进一次有界工作

由已有 executor 显式调用 step／service／dispatch；Periodic 只计算释放，Timers
只返回 due event，EventFanout 只同步尝试 sink。离散异步消息复用现有 queue，
连续借用复用 stream；不要为组件再创建隐藏 worker、全局 tick 或一套取消协议。

事务准入前建立同一单调 clock 域的有限绝对 deadline，排队、传输和底层 callback
共用它。周期到期或一次 BUSY 不能刷新既有请求的预算。算法的 `dt_s` 是显式秒数，
不等于 deadline；采样丢期、时钟换源和输出限幅由应用决定。

先检查操作结果，再消费数值和 metadata。器件的 software observation 保留其来源、
domain／epoch 与未知物理不确定度；近期读回不自动证明转换新鲜。Health 的未知
事实不能当健康，Log UART 的写入成功是副本准入，不是线路或持久化完成。

## 5. 关闭准入、排空并恢复

1. 停止新业务和上游事件发布，加入所有能访问该组件的调用者。
2. 对已接受请求继续 service／cancel／drain，直到 acquire 观察 `SETTLED`；
   `QUARANTINED`、deadline 或取消失败都保留请求和缓冲区。
3. 按组件合同关闭或卸载；失败的 GPIO 撤回、器件写周期或 Flash cleanup 保留
   相同绑定与外部资源，不以数值更大的 deadline 消除 clock 故障。
4. 完成 observer／executor／映射 loan 的退出会合，再归还外层 bus、GPIO、clock
   和 workspace。纯算法也要等所有调用退出后才重置历史或回收系数。

[请求生命周期 FIG-06](../explanation/ownership.md)和
[等待／退出 FIG-08](../explanation/os-portability.md)说明借用结束与使用者退出的区别；
具体执行路径见[I/O 执行指南](io-execution.md)。失败的持久化操作可能已经提交，
先完成物理恢复，再按原合同 reopen／remount 判断实际存活状态；不盲目 replay。

异常路径只捕获固定 raw fault record，不调用普通 Log、Shell、RTT、Flash 或 kernel。
正常启动／task 在验证原始记录和 Storage 就绪后再持久化；物理 retention 与 reset
域由产品证明。

## 6. 需要新增可复用行为时

叶目录维护实现与 CMake 依赖，定义窄端口，不依赖 Board pin、SDK Handle 或产品
worker。确需静态绑定／供应商选择时才在 `components/catalog.json` 登记 ID、叶入口
和公开 target；不在中央调度器加名称分支。公开生成头的 API 要求与 bindings 私有
编译要求分别声明，避免向应用泄露私有目录。

按仓库 TDD 合同先执行真实失败，再实现最小行为；覆盖无效输入、容量、超时／恢复
和独立实例，而不是只检查 mock 调用次数。用独立外部消费者验证源 SDK 与安装 SDK
的公开头、精确选择、ABI 和链接范围。Native 模型、ARM 链接和实板测量是不同证据；
本文不建立硬件资格，未执行的物理电气／时序／掉电项保持 `not_executed`。
