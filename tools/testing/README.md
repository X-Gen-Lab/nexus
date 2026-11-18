# 平台工具回归

保留配置、构建、依赖来源、SDK、资源测量、进程清理和 HIL 工装的真实故障回归。
候选晋升、历史归档恢复和重复任务台账已退出当前维护范围。

```sh
python tools/testing/run_tool_tests.py --suite testing --report build/quality/tools-testing.json
```

普通固件实现提交执行完整 Native GoogleTest，不触发整套辅助工具域。公共头、声明事实
和工具改动按 `run_tool_tests.py` 选择完整相关测试模块；未知或缺少已知覆盖则执行完整域。
提交范围标记 `commit-tools`、`qualification=false`；完整 `--all` 验收在合并前独立执行。
完整 CI 分别运行 testing、configure、ci、
workflows、contracts、freertos。工具测试不按 Native 编译模式重复。
每次要求非空发现、完整执行、零跳过及真实返回码，旧报告不能建立本次通过。
新增工具行为先记录 RED 再实现 GREEN。纯粹删除废弃功能不新增镜像式测试。
testing 域集中执行一次全局能力和派生文档检查；局部外设测试不重复扫描整个平台。
报告包含最慢用例摘要，用于定位实际成本，不以测试数量作为优化目标。
