# 安全扫描与生命周期边界

CodeQL 在实际 Native 构建中分析维护中的生产实现、当前工具和行为测试。
Python 提取只排除 `docs/archive/` 内不再执行的历史文档工具；不排除当前测试、
配置器、证据工具或 SDK 打包代码。查询保持 `security-and-quality`，没有通过
关闭所有权或路径检查来消除诊断。扫描执行成功与诊断审查是不同结果。

## 本轮发现与修复

- 注释检查 CLI 明确终止无效输入，不能继续输出未建立的成功报告；回归验证
  错误退出码、标准错误和空标准输出。
- 存储故障测试使用自己创建的 `0700` 临时目录和固定文件名，完成后恢复 cwd。
  跨进程持久化固定重执行 Linux `/proc/self/exe`，命令行不能指定文件或程序。
  实际测试保留两组各 315 个写入／掉电边界及新进程读取，不用同进程内存代替。
- STM32 GPIO 寄存器夹具退出前恢复其借用的全局 RCC 指针，随后实际调用验证
  未绑定时拒绝访问；避免留下已经退出作用域的栈地址。
- GD32 watchdog 夹具使用静态存储。独立 watchdog 启用不可逆，`stop` 返回
  unsupported，owner 必须持续到进程／平台复位，不能用普通作用域结束来释放。
- 删除维护工具中的未使用导入，说明进程清理的预期竞争和轮询超时；保留
  失败时清理整个进程组并重新抛出异常的行为。

相关源码与回归位于 `scripts/ci/test_comment_style.py`、
`tests/contracts/components_storage.c`、`stm32_io_test.c`、`gd32_io_test.c`
以及工具合同测试。最终执行状态由当前提交的 CI 和候选证据记录决定。

## 保留的借用语义与审查责任

异步接口把调用者提供的地址存入 provider／owner 是必要的借用。栈地址逃逸类
诊断须核对真正的生命周期，不能改用隐式 heap、去掉 IRQ 表或删除屏障来绕过。
生成的 MCU provider 对象均为静态存储；私有模型中的局部存储遵守同一责任。

| 地址保存位置 | 必须满足的退出条件 |
|---|---|
| GD32 控制器 IRQ 表 | 正常 stop 先解绑 IRQ；失败恢复保留责任；不可逆 watchdog 持续到复位 |
| Native UART request | provider detach 后 release-publish SETTLED；调用者 acquire-observe 后才能释放 |
| FreeRTOS task context | 静态 stack/TCB/context 持续到外部成功 join；不能由当前任务销毁自己 |
| 多 producer owner slot | executor 结清请求、slot detach、发布者和观察者退出后才可回收 |

GD32 Flash 的 program／erase 在重新上锁读回失败时进入不可用状态并保留静态
provider；普通 stop 不能宣称已恢复。此情况要求受控复位，不能释放 owner 或继续
写入。当前模型没有专门的 relock-fault 注入，不能宣称该硬件失败已经资格化。

较长状态机的可读性、第三方头中的注释和借用诊断仍需按具体路径审查。供应商
导入保留原始字节与许可身份。普通静态扫描不证明没有漏洞，也不替代实际 IRQ、
电气、掉电、时序、长期负载或产品安全资格。
