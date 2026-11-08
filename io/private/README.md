# I/O 私有机制边界

本目录保存类型化 I/O 机制与选定 SoC 实现间的私有合同，不进入产品公共头文件
搜索路径。公共只读接口保存操作表和实例上下文，可变状态与厂商寄存器类型留在
具体实现内部。公共入口见[上级 README](../README.md)。

| 目录 | 责任 |
| --- | --- |
| `io/src/api/` | 公共检查与分派 |
| `io/src/mechanisms/` | 可复用 I/O 执行规则 |
| `io/private/` | 无硬件依赖的验证、分段传输序列的所有权身份和有界日历运算 |
| `soc/stm32/stm32f4/drivers/ip/` | F4 私有 CAN、USB FS、Ethernet、定时器和 I2C 算法 |
| `tests/support/native/drivers/` | 显式测试 scope 的独立宿主模型 |
| `soc/stm32/stm32f4/drivers/` | 当前 F407VE／ZG 具体模式和资源生命周期 |

SoC 适配层提供真实寄存器和时钟事实、IRQ／DMA 绑定及受审硅片语义。私有构建期
头文件依赖不选择源码，不增加运行时分派；具体 SoC 的 CMake 目标仍拥有每个编入
的引擎。相同名称不证明相同硬件语义。

固件保持 C11，SDK 头文件依赖保持私有。共享私有引擎不能隐式分配存储、发现设备、
启动工作线程或取得强制 OS 锁。队列、端点、描述符和流块容量来自用户装配或明确
的调用者存储合同，成本须观察真实 ELF。

IRQ 记录有界硬件事实，任务侧推进服务继续处理截止时间、线上完成和撤源。最终
引用撤销后才发布 SETTLED，QUARANTINED 保留全部未结束借用。具体限额和物理范围
见[执行预算](../../docs/design/soc-execution-budgets.md)、公共操作合同与各控制器
设计说明。[FIG-31：驱动源码归属](../../docs/assets/diagrams/driver-source-ownership.svg)
描述构建归属，不表示额外运行时转发层。
