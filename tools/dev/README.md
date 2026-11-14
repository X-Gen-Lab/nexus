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

每条实际子命令的日志保存在独立的
`build/quality/developer-commands/<执行标识>/command.txt`，终端显示其路径。
`build/quality/developer-command.command.json` 是原子更新的最近一次完成结果入口，
其中 `log` 和 `attempt_receipt` 指向同一次执行的固定文件；读取该 JSON 后再按其中
路径查看日志。旧的公共 `developer-command.txt` 不再更新，不能作为当前执行入口。
失败、超时和取消也保留各自的原始输出与实际退出码。

## 本地 Native 矩阵

```sh
python tools/dev/dev.py matrix --jobs 4 --parallel-modes 2
```

默认执行 Debug、Release、ASAN+UBSAN 和 TSAN 四个完整 Native 模式；需要单独检查
时，可以显式传入 `--presets native-debug native-release`。每个模式调用现有 CMake
预设配置一次、构建一次，再执行完整的 `native_contracts.py`，不创建第二套源码构建图，
不缓存成功的测试报告。模式无关的完整工具域仍由普通门禁和独立工具 CI 执行。

`--jobs` 是整个矩阵的编译 CPU 预算，不是每个模式的预算，不能超过当前进程的
CPU affinity／Linux cgroup 限额。并行模式平分预算，例如 4 个 job 和 2 个模式时，
每个模式分配 2 个 job。默认每个模式预留 4096 MiB 的准入预算，可通过
`--memory-per-mode-mib` 调整；该值与当前可用内存共同限制同时运行的模式数量，
属于并发准入估计，不是操作系统施加的内存上限。可用内存无法确定时须选择
`--parallel-modes 1`。

各预设必须使用互不包含的独立构建目录。矩阵在运行期间持有各目录的
`.nexus-build.lock` 内核锁，重复矩阵会立即拒绝。该锁只协调矩阵；普通 `dev.py`、
TDD、CMake／Ninja／CTest 不自动参与，调用者必须保证这些构建目录在本轮独占。
任一模式失败即取消本次矩阵的其他在途命令；SIGINT／SIGTERM 会排空本次拥有的子进程并返回 130，不影响其他构建目录的进程。

每轮矩阵使用新的 `build/quality/native-matrix/<执行标识>/`，保留阶段输出、
命令回执、各模式完整执行报告副本和 `execution.json`。每个模式成功后立即保存
本次 contracts JSON、CTest 和对应 Google JUnit，以原执行报告的 SHA-256 核对副本；
最终汇总只使用这些独立副本，不在其他模式完成后重新读取共享报告。可以用
`--output <新目录>` 指定位置，已有目录会拒绝复用。一次工具诊断覆盖本轮矩阵；
它不替代实际配置、编译、测试或物理 HIL。

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
