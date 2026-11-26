# 平台工具回归

保留配置、构建、依赖来源、SDK、资源测量、进程清理和 HIL 工装的真实故障回归。
候选晋升、历史归档恢复和重复任务台账已退出当前维护范围。

| 目录 | 维护内容 |
| --- | --- |
| `build/` | CMake 图、平台消费、来源闭包和实际编译选择 |
| `dependencies/` | 来源锁、gitlink、供应商配对及依赖准备 |
| `execution/` | 命令、构建锁、进程撤回、测试发现和报告准入 |
| `measurement/` | ELF、资源预算、HIL 工装和测量身份 |
| `sdk/` | 导出选择、包完整性、接口消费和精确来源 |
| `support/` | 执行、依赖和编译探针使用的共享夹具 |

```sh
python tools/testing/run_tool_tests.py --suite testing --report build/quality/tools-testing.json
```

普通固件实现提交执行完整 Native CTest 与 GoogleTest 身份核对。公共头、声明事实
和工具改动按 `run_tool_tests.py` 选择完整相关测试模块；未知或缺少已知覆盖则执行完整域。
板级电气合同的完整测试模块在 `tools/configure/tests/__init__.py` 的
`BOARD_FACT_CONTRACTS` 声明所用板级事实文件。新增专属合同只需在测试侧登记。
提交范围标记 `commit-tools`、`qualification=false`；完整 `--all` 验收在合并前独立执行。
常规完整门禁运行 `testing`、`configure`、`ci` 三个工具域。工作流合同归入
`scripts/ci/test_platform_ci.py`，由 `ci` 域执行。完整 CI 另行运行 `integration`
和 `freertos`，文档用例及严格中英文构建由文档 job 执行。工具测试不按 Native
编译模式重复。
每次要求非空发现、完整执行、零跳过及真实返回码，旧报告不能建立本次通过。
新增工具行为先记录 RED 再实现 GREEN。纯粹删除废弃功能不新增镜像式测试。
testing 域集中执行一次全局能力和派生文档检查；局部外设测试不重复扫描整个平台。
报告包含最慢用例摘要，用于定位实际成本，不以测试数量作为优化目标。
`duration_seconds` 记录整域执行时间。并行模块的 `module_durations_seconds`
包含解释器启动、导入、类夹具和清理。并行模块时长可重叠，整域耗时以整体字段为准。

完整模块默认在独立进程中执行，worker 数继承调用方的 `NEXUS_BUILD_JOBS` 或
`CMAKE_BUILD_PARALLEL_LEVEL`；独立调用未提供预算时使用 2 个槽。
`--jobs N` 或 `NEXUS_TOOL_JOBS=N` 可限制本次工具并发，执行器按总预算分配各
worker 的编译槽。`--jobs 1` 保留单 worker 对照入口。
每个模块使用独立 Python 进程；报告的 `modules` 含各模块实际用例标识的
SHA-256，可用 `--jobs 1` 与并行结果对照。超时、信号或模块失败会撤回已启动的
子进程并使本次执行失败。共享构建目录的写入必须由所属构建入口取得目录锁。

已知工具实现与测试名称不对应时，在现有 `COMMIT_ALIASES` 登记完整模块。
普通固件提交按实际依赖选择检查；工具自身或未知覆盖的变更扩大到适用完整域。
