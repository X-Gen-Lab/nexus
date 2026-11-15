# 主机工具

固件保持 C11；这些工具在开发主机运行，不进入 MCU 镜像。

| 目录 | 责任 |
|---|---|
| configure | TOML／JSON 校验、typed IR、静态配置与 factory 生成 |
| dev | CMake／CTest 入口、锁定依赖准备、外部工程创建、日常设备操作 |
| maintenance | 依赖来源锁、能力事实与接口版本检查 |
| measurement | ELF、接口保留和 Flash／RAM 预算测量 |
| hil | 实板工装准备、串口协议、采集与结果校验 |
| documentation | 文档源检查、双语构建、图示与工具环境 |
| evidence | 构建和 HIL 复用的文件身份、严格 JSON、路径与原子写入 |
| testing | 开发、依赖、SDK、测量与 HIL 工具回归，按现有 suite 执行 |
| schemas | HIL 和资源记录的编辑／参考 schema；实际准入由对应工具校验 |

从 [开发入口](dev/README.md) 和 [工程手册](../docs/design/engineering-handbook.md) 开始。
平台不维护另一套候选晋升、归档恢复和任务台账引擎。

配置工具回归位于 `configure/tests/`；CI 回归位于 `scripts/ci/`；文档回归由独立锁定
环境运行。`testing/run_tool_tests.py` 保持各完整工具域的统一执行和结果准入。
