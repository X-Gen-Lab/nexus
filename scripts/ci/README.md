# 工程门禁与真实执行入口

这些脚本把声明的工程合同连接到实际工具／构建／产物，不制造通过记录，也不自动
暂存或修复源码。日常入口仍是 `tools/dev/dev.py`，每次执行保留输入身份和原始失败。

| 入口 | 责任 |
| --- | --- |
| `style_gate.py`、`check_commit_message.py` | 完整staged格式、注释、文本与Conventional Commit |
| `tdd_gate.py`、`native_contracts.py` | 精确index、完整工具域、真实Google／CTest发现与无过滤执行 |
| `arch_compile.py`、`cortex_runtime.py` | 实际ARM CPU／ABI／指令与生产runtime闭包 |
| `os_resources.py`、`os_wait_cost.py` | 明确ABI／布局的真实链接成本，不推断cycles |
| SoC／provider runner | 手写精确装配、startup／资源／保留检查 |
| vendor／toolchain检查 | 固定来源、许可、输入与实际工具身份 |

缺工具、空范围、skip-only、陈旧报告都失败。本地hook与CI执行相同规则；存在本地
政策不表示远端branch protection已开启。外部工具按已有lock预备，固件configure
不下载缺失依赖。

每个runner的参数和输出所有权以其 `--help` 与对应fixture合同为准。纯文档整理
运行相关文档检查；生产／工具行为变更先执行真实RED再实现GREEN。Native、ARM、
Source SDK、离线复现、候选与HIL的范围分别记录，不继承旧执行counts。
