# Nexus 嵌入式通用平台

Nexus 提供工业 MCU 的通用机制：静态配置与类型化 I/O、明确的请求生命周期、
按对象分配的可选 OS 适配、通用器件／日志／协议／存储组件，以及构建与验收工具。
产品任务、寄存器授权、资源预算、私有 PCB、恢复和升级策略属于外部应用仓库。
公开示例位于 [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)。

首发软件平台是启明 STM32F407ZGT6、天空星 STM32F407VET6、梁山派 GD32F470ZGT6，
每块板提供裸机与 FreeRTOS 配置。板级默认只选择已有来源的 LED 和串口；更多外设
通过明确声明接线的 Board 包接入。两款芯片的能力、软件模型和实板资格分别记录。
当前实板未接入，物理测试未执行。

新架构移除了旧 HAL/OSAL factory、注册表、统一对象池、Runtime 转发层和 Kconfig
主构建链。默认数据路径直接访问固定类型端口，不创建隐藏任务或分配堆内存。
取消和超时不释放借用；调用者必须观察 SETTLED，不能回收 QUARANTINED 请求。
SPI/I²C/Flash/ADC 首发采用同步轮询，DMA 等模式另行设计与验收。

安装、配置、构建和测试命令见 [主 README](README.md)。格式保持根 `.clang-format`、
`.editorconfig` 和反斜杠 Doxygen 约定，commit 和 CI 自动执行同一门禁。

完整资料：

- [架构蓝图、工程手册、平台接入契约与 33 项实施清单](docs/design/README.md)
- [当前软件交付与证据入口](docs/delivery/README.md)
- [源码 SDK 独立消费](cmake/package/README.md)
- [开发贡献流程](CONTRIBUTING_CN.md)
- [历史方案与历史结果](docs/archive/README.md)，不用于本版资格证明。
