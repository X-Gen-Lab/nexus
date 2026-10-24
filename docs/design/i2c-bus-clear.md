# 显式 I2C 总线恢复

`nx_i2c_bus_clear_t` 是调用方持有的可选 GPIO 恢复机制。它不扫描设备，不重放
失败的 transaction，也不自动替换 SoC 的 `nx_i2c_port_recover`。既有 controller
恢复与本机制分开：前者证明控制器停止并回收硬件引用；后者需要板卡明确授权，
才允许切换双引脚并驱动实际电气总线。

公共接口在 `io/include/nexus/io/i2c_bus_clear.h`，算法在
`io/src/mechanisms/i2c_bus_clear.c`。生产代码仍为 C11，没有 heap、默认 worker、动态注册表
或隐藏 OS 锁。调用方保存 session、只读 ops 表、adapter context 和 clock，按
单 executor 合同调用；这些对象必须活到租约成功归还。

## 板级授权与资源交接

调用方显式提供 `acquire/drive/sample/wait_until/release` 五个方法和
`nx_clock_t`。`acquire` 的成功合同包括：controller 已停用，DMA 与 IRQ 发布者
已排空，其他总线 master 与引脚用户静止，SCL/SDA 独占，双引脚已安全释放并
采用开漏模式。失败不能留下引用、租约或未报告硬件效果。

`drive(scl_low, sda_low)` 中 `true` 只允许拉低，`false` 只允许释放为高阻。
公共 `nx_gpio_port_write` 不能证明开漏、电压、上拉或引脚复用，因此不能直接
替代这个授权合同。`sample` 必须读取实际输入电平。adapter 必须先核对 PCB
修订、两引脚权限、上拉、电压与所有权交接，不能从芯片 capability 猜测接线。
这批交付没有自动接入 F407/F470 物理引脚，也没有对未审板卡进行操作。

当 `scl_low=true`，adapter 必须先建立 SCL LOW，再修改 SDA。两引脚可能在
不同 GPIO port，不能假设双引脚寄存器写原子；尤其从失败保留状态重试 STOP
时，SDA 的先前输出状态可能未知。

`release` 恢复已保存的 peripheral/pin 所有权。返回失败必须保留租约与保存的
状态；调用方不能把 controller 再次启用或复用 context。所有方法都是有界的，
`acquire/release/drive/sample` 不等待其他 owner；只有显式 `wait_until` 按绝对
时间等待。wait adapter 必须证明有限返回界，即使 clock 停止也必须能报错并
返回，不能在 callback 内无限等待冻结的 timer；helper 不能抢占或修复一个
违反这个合同的 callback。helper 返回后重读 clock，晚到则报 deadline 失败。
调用方不能在 callback 内递归调用本 session。

## 执行与停止

调用方显式选择非零 `low_us/high_us/stop_setup_us/stretch_poll_us`，依据实际
器件和电气模式给出时间。没有默认“安全”数值。`run` 接收同一 monotonic
clock 域的有限 primary deadline 和独立 cleanup deadline，后者不早于前者。
`NX_DEADLINE_NEVER`、无效 timing 与缺失方法均在硬件访问前拒绝。

取得租约后，机制先观察实际 SCL/SDA。SCL 被拉低时，以有界 poll interval 等待
clock stretching；每次 `wait_until` 返回成功后重读 clock，证明它已推进至
target。冻结、倒退或耗尽的 clock 报 IO 并退出。释放 SCL 后，只有观察到实际
HIGH 才开始计算高电平保持时间。SDA 已释放时不生成恢复脉冲；否则最多九个
完整低/高周期，SDA 释放后立即结束，不再继续脉冲。

拉低阶段结束后还会 sample 确认实际 SCL LOW；释放后的高周期结束也确认 SCL
仍 HIGH。STOP 先验证双线 LOW，并在 setup 结束后再次确认 SCL HIGH、SDA LOW，
才释放 SDA。间隔中未经采集的瞬态无法用主机模型证明，仍需真实波形验收。

任何取得租约的执行都在 cleanup budget 内尝试 STOP：先拉低 SCL，再在 SCL
低时拉低 SDA；保持 low，释放 SCL，等到实际 HIGH；保持 STOP setup 时间，
释放 SDA，再保持 high 并观察双线 HIGH。STOP 准备边沿不计入最多九个恢复
脉冲，未完成的高周期也不计为已完成。STOP 失败不会被误报为 bus free。

STOP 与恢复状态不是 transaction 完成状态。即使总线恢复成功，原失败数据
操作仍失败，是否发起新 transaction 是外部产品策略。

## 独立错误与保留状态

输出包含 `primary/cleanup/pulses/stop_observed/lease_held`。`run` 返回 primary
失败，否则返回 cleanup；cleanup 永远不覆盖主错误。未取得租约时不会调用
release。取得租约后 STOP 失败或 release 方法失败，均保留 session 和完整
租约，停止新 transaction 与 recovery pulse。

`finish` 仅重试未证明的 STOP 或所有权归还，不重新 acquire、不生成恢复
脉冲。如果 STOP 已证明而 release 失败，下一次只重试 release；已成功步骤
不重复。`finish` 返回最新 cleanup，输出中的原 primary 与 pulse count 不变。
新的 `run` 和重新 init 在仍有租约时拒绝，不用“重初始化”掩盖残余状态。

如果 release 方法实际成功，但返还时间已经超出 deadline，cleanup 报 TIMEOUT，
同时 `lease_held=false` 如实报告已归还的资源。deadline 错误不能虚造租约。
其他保留的失败不能释放、丢弃或移走 session/context；产品必须隔离故障设备，
由明确 recovery/reset 策略处理。机制不自动复位或自称安全输出。

## 验证边界

GoogleTest/GoogleMock 用真实生产算法及外部引脚/clock/lease mock 验证九周期
上限、SCL stretching、冻结 clock、primary/cleanup、STOP 失败保留、仅清理
重试与实例独立性。这证明软件合同，不证明实际开漏或波形。

物理资格要求实际 station 绑定与原始电平采集，测量 SCL 的 low/high、SDA 的
STOP setup、stretching 与失败后的引脚/外设所有权。上拉、rise time、多 master
排他、设备 stuck-low 与 SDA 释放时机必须用实际装备执行；数值预算由板卡和
器件确定。当前所有相关 physical status 保持 `not_executed`。
