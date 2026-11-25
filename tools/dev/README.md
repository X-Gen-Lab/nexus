# 平台开发入口

`dev.py` 直接调用 CMake／CTest，保留真实退出码、日志与实际构建来源。
产品应用、调度策略和私有板级数据放在外部仓库。

`fault_decode.py` 在主机上校验并解码固定 160 字节的保留故障记录。应用为每次构建
分配唯一、非零的 16 字节标识，将 `const uint8_t nx_fault_build_id[16]` 保留在
未剥离 ELF 的只读 LOAD 中，并将相同字节传给捕获入口。保存完整 ELF 和该次构建
的来源记录。原始解码验证长度、提交字、CRC、版本、保留字段和标识。

```sh
python tools/dev/fault_decode.py fault.bin --elf firmware.elf \
    --provenance build/target/quality/build-provenance.json
```

`--elf` 核对镜像内标识后才用 `arm-none-eabi-addr2line` 解析有效核心帧的 PC／LR。
`--provenance` 进一步验证已保存的构建清单及原有文件，要求清单包含传入 ELF 的
准确字节和已解析配置，输出 ELF 与配置的 SHA-256。`dev.py build --record-provenance`
生成此格式。原始记录的 CRC 和构建标识用于损坏检查与匹配。

`--redact` 只输出 `version`、`sequence`、`flags` 和 `build_id`，可以同时要求
镜像及来源核对。它保留原始文件且不运行符号解析。原始记录、完整 JSON、ELF、
构建清单与诊断日志的访问和披露由产品管理。

```sh
python tools/dev/dev.py doctor --target native
python tools/dev/dev.py prepare --scope native --allow-network
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

`prepare` 显式允许网络后才下载依赖；配置和编译不隐式下载。
组织来源缺少保留 pin 时，错误指出依赖路径；应恢复该 fork 上选中提交的
`refs/tags/nexus/pins/<commit>`，保持来源与锁定提交不变。其他远端查询失败
报告依赖路径和 Git 退出码，不公开可能包含凭据的原始诊断。
`doctor --target arm` 检查锁定的 ARM 编译器；`contracts` 同时检查 Native 和 ARM。
CMake 的原生参数可以直接透传。并发受进程分配预算约束。

同一台主机并行运行多个独立入口时，由启动方划分 `NEXUS_BUILD_JOBS` 总预算。
交互构建、性能采样与完整 SDK 发布各自计时；性能采样在声明的固定资源条件下运行。
正式发布在最终源码冻结后执行一次完整资格，局部开发使用相关目标与提交检查范围。

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

普通 TDD 门禁在锁内识别已配置的 Native 目录。相同 preset、工具链与缓存复用
Ninja 图，源码、头和配置文件的变化由原生依赖处理。构建身份变化或第一次运行时
重新 configure。每次门禁仍重新执行合同并生成本次测试报告。

外部应用显式消费 Source SDK 时先验证整包。验证通过且配置身份相同，复用既有
Ninja 图；SDK、preset、工具链或缓存身份变化时重新配置。验证失败保留原缓存和
配置记录，便于修复后使用同一入口重试。

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

`matrix`、`docs`、`check` 和 `doctor --target docs|workflows` 需要完整开发仓库，
不属于 Source SDK 的构建工具闭包；SDK 在加载仓库专用模块前明确拒绝这些命令。
`doctor` 只诊断所选工具和环境，不扫描全平台能力。`check` 执行格式与完整开发门禁。
普通 commit hook 读取完整暂存区，运行全部 Native GoogleTest；普通固件实现改动不触发
整套辅助工具域，公共头、声明事实和工具改动选择完整的相关测试模块，未知输入保守执行完整域。
快速提交报告明确标记有限 scope，不能替代完整验收。合并前用 `check` 或 `--all` 执行
完整工具域，独立执行其他 Native 模式、
ARM 链接和独立 Source SDK 消费者。GoogleTest／GoogleMock 仅链接到主机测试。

`create` 创建外部工程；`timings` 直接汇总已有执行记录。普通 configure／build 不自动
生成完整来源归档；需用于设备操作的精确映像身份时显式传入 `--record-provenance`。
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
