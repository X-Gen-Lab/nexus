# Native 主机测试支撑

本目录提供公共 I/O 的主机模型、测试硬件事实和装配。它不属于生产 MCU 支持目录，
不声明 PCB、电气连接、真实 NVIC、DMA 总线或 MCU 时序。

`configure/` 维护测试能力与静态连接；`drivers/` 保存执行真实公共合同的主机模型。
`board/` 和 `assemblies/` 只供显式测试装配使用。公共 face 共享只读 ops，实例分别持有
状态；factory 返回已有身份，不分配存储或启动设备。

配置工具仅在传入 `--test-support` 时接纳 `native_reference`，Python API 对应
`test_support=True`。CMake 测试工程必须显式设置 `NEXUS_TEST_SUPPORT=ON`；生产工程
默认关闭。维护中的 Native preset 显式声明测试用途；GoogleTest 保留
Debug、Release、ASan 和 TSan 四种模式。

```sh
python tools/configure/configure.py check --test-support \
  --assembly tests/support/native/assemblies/native.toml
```

模型继续验证请求接纳、借用、取消、排空、隔离和最终 SETTLED。调用者拥有执行者、
service 进度、缓冲区、等待与关闭顺序；宿主 libc 和线程行为不证明 MCU 无堆或时序。
CAN、USB、Ethernet 模型执行选中的 F4 私有 IP 源码，不另写一份成功路径代替它。

维护入口见 [开发工具](../../../tools/dev/README.md)和 [I/O](../../../io/README.md)。
Native 结果、ARM 链接和实板 HIL 分别建立软件与硬件证据。
