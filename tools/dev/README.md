# 平台开发入口

`dev.py` 直接调用 CMake／CTest，保留真实退出码、日志与实际构建来源。
产品应用、调度策略和私有板级数据放在外部仓库。

```sh
python tools/dev/dev.py doctor --target native
python tools/dev/dev.py prepare --scope native --allow-network
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

`prepare` 显式允许网络后才下载依赖；配置和编译不隐式下载。
`doctor --target arm` 检查锁定的 ARM 编译器；`contracts` 同时检查 Native 和 ARM。
CMake 的原生参数可以直接透传。并发受进程分配预算约束。

```sh
python tools/dev/dev.py check --preset native-debug
python tools/dev/dev.py support --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/dev/dev.py docs check
```

文档命令和仓库检查需要完整开发仓库，不属于 Source SDK 的构建工具闭包。
`check` 执行格式、维护约束和完整普通开发门禁。普通 commit hook 使用同一门禁，
读取完整暂存区，运行全部 Native GoogleTest 与受影响的完整工具域。
一次配置和构建足够；不重复执行已运行的工具域。完整 CI 还覆盖其他 Native 模式、
ARM 链接和独立 Source SDK 消费者。GoogleTest／GoogleMock 仅链接到主机测试。

`create` 创建外部工程；`artifacts` 读取实际构建来源；`timings` 汇总原始执行耗时。
`flash`、`debug`、`monitor` 默认准备可审查的操作，通过各自显式执行选项接入设备。
设备身份、探针、串口和映像必须匹配；规划或链接不能建立物理 HIL 通过状态。

构建输出保存在 `build/`，成功报告不作为缓存。独立构建树可以复用已验证的工具和
依赖准备。更多配置、设备操作及 SDK 合同见 [工具参考](../../docs/reference/configuration/tools.md)。
