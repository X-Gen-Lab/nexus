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

各预设必须使用互不包含的独立构建目录。`configure`、`build`、`test`、`check`、
TDD、Native runtime 和矩阵使用相同的 `.nexus-build.lock` 内核锁；组合命令持锁至
全部阶段和报告发布结束。同目录的其他托管执行立即拒绝，不同目录可以并行。
配置的 `-B` 和测试的 `--test-dir` 覆盖也按实际目录加锁。
`configure` 的 `-S` 只能选择当前平台源码；外部工程使用创建命令给出的 CMake 入口，
避免从另一份源码的同名预设取得不同构建目录。

POSIX 上，组合命令仅向自己启动的子进程传递真实锁描述符，子进程核对规范路径与
文件身份后复用锁；环境变量本身不能跳过加锁。同线程调用可以重入，其他线程不
自动继承所有权。异常或取消完成排空后释放锁。Windows 独立命令使用文件锁；
嵌套托管命令明确拒绝，需要 POSIX 描述符继承，不能据此声明 Windows 矩阵通过。
直接运行 CMake／Ninja／CTest 不参与这项协调，调用者仍须保证目录独占。
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

## 最小公开消费工程

```sh
python tools/dev/dev.py create --public --board stm32f407_qiming_v31 \
    --clock hse8-pll168 --gpio led0 --source /absolute/path/to/nexus \
    --output /tmp/my-nexus-gpio
```

使用所选 Board 已有的 GPIO binding；clock 与 binding 由所选平台自己的 resolver
验证。该固定 baremetal 工程使用强类型 factory 读取授权 GPIO snapshot，再停止平台，
不需要私有模板、Native 模型、RTOS 或可选中间件。它是最小公共接口消费资料，不是
产品闪灯应用，也不建立实板资格。`--output` 必须是平台外的新目录。

可用 `--sdk-prefix /installed/prefix` 替代 `--source`，只能选择已验证的可发布 SDK。
来源冲突、配置拒绝或输入变化时拒绝创建，并只清理本次独占创建的目录。按终端输出
先运行 configure，再运行 build；`--json` 的命令数组按名称选择，不依赖 JSON 键顺序。
所选版本有 commit 身份时，configure 命令固定 `NEXUS_EXPECTED_SOURCE_REVISION`。
`.nexus-template.json` 保留复制文件摘要、选择来源及本次验证，不表示已经构建或运行。

构建输出保存在 `build/`，成功报告不作为缓存。独立构建树可以复用已验证的工具和
依赖准备。更多配置、设备操作及 SDK 合同见 [工具参考](../../docs/reference/configuration/tools.md)。
