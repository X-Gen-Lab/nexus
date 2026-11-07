# Native 主机实现与模型边界

本目录在主机上实现与 MCU 相同的公共 I/O 接口，供应用消费、生命周期回归及独立
实例验证使用。它不描述 PCB、电气连接、真实 NVIC、DMA 总线或 MCU 时序。

## 源码结构与调用

`configure/` 拥有 Native 能力、模式和静态构造策略；`drivers/` 保存独立主机设备
模型，`CMakeLists.txt` 选择实际编译目标。公共 face 使用共享只读 ops 和独立 context；
factory 返回已有身份，不分配或启动设备。选择同一类别的多个实例仍分别持有状态。

测试和应用通过公共 I/O 进入模型；模型遵守请求接纳、借用、排空、隔离和最终
SETTLED 的合同。应用仍拥有执行者、service 进度、缓冲区、等待和关闭顺序。
Native OS／Arch 的宿主线程同步与 libc 内部行为不能作为 MCU 无堆、延迟或 RAM
成本的证明。

## 使用与证据

使用根目录维护的 `native-debug` 等 preset 和 [开发工具](../../tools/dev/README.md)。
配置／模型结果、ARM 链接与物理 HIL 分别建立证据；Native 不会自动获得硬件资格。
具体公共操作、controller／endpoint 与存储责任见 [I/O](../../io/README.md)，芯片
集成入口见 [SoC](../README.md)。
