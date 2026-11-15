# GPIO I2C 与 SCCB

`Nexus::SoftwareI2C` 是调用者持有的同步两线控制器；`Nexus::SoftwareSCCB` 复用
同一库，但提供独立协议 API。组件没有 task、heap、运行时发现或长全局关中断区。

## 端口与协议边界

只读配置提供已设为开漏输出的两个 GPIO、外部上拉、实际输入回读和同一单调 clock。
写一是释放线路，绝不能推挽驱动高电平。显式最小 half-period 与有限 polling count
约束每次等待，不代表实测频率、电压或电气资格。

严格 I2C 使用独立固定地址的 `nx_i2c_endpoint_t` child，接纳七位地址
`0x08..0x77`，至多 8 条消息、每条至多 256 字节；消息间 repeated START，最后一次
STOP。payload count 不包括地址阶段，NACK 如实报告，不忽略；异步方法不支持。

SCCB 使用不同 child 和 register API，write identifier 是偶数 8 位值，显式执行
register8／data8。读取按文档以 STOP 分隔 sub-address 与 read cycle，再发送 master
NA；master 释放并忽略 target 第九个 Don't-Care bit。成功完成波形不证明 sensor
存在，SCCB 不削弱严格 I2C face，也不是通用 ignore-NACK 回退。

## 撤回与恢复

每个同步返回结束全部 CPU payload 访问；失败 STOP 另外保留 wire owner、配置与
引脚责任。同一 task 串行管理调用和 clock。仲裁丢失只释放线路，不强制 STOP。
恢复前调用者重新建立独占并会合 foreign writer；恢复仅尝试一次有界 STOP，不做
九时钟 bus-clear、reset 或事务重放。没有 deadline 的 controller recover 方法不支持。

释放外层 GPIO lease／配置前，调用带预算的 stop 并确认成功。后来的数值 deadline
不能修复故障或不连续 clock domain；不承诺多 controller 协作。

V4.1 camera 的 PD6／PD7 与 audio 的 PC0／PC13 是 GPIO 对，不是硬件 I2C 路由。
本机制仅供明确选择的器件 adapter 使用，不选择或初始化 codec／camera。来源身份见
`references.json`，物理 HIL 保持 `not_executed`。
