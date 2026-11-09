# 平台工具回归

保留配置、构建、依赖来源、SDK、资源测量、进程清理和 HIL 工装的真实故障回归。
候选晋升、历史归档恢复和重复任务台账已退出当前维护范围。

```sh
python tools/testing/run_tool_tests.py --suite testing --report build/quality/tools-testing.json
```

普通提交运行受影响的完整工具域；完整 CI 分别运行 testing、configure、ci、
workflows、contracts、freertos。工具测试不按 Native 编译模式重复。
每次要求非空发现、完整执行、零跳过及真实返回码，旧报告不能建立本次通过。
新增工具行为先记录 RED 再实现 GREEN。纯粹删除废弃功能不新增镜像式测试。
