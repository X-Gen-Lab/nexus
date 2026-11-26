# 工程检查与安装脚本

构建使用 `python tools/dev/dev.py configure|build|test --preset <name>` 与一份
schema-2 TOML，SoC／Board 事实仍是 JSON，原生 CMake 参数传给维护 preset。
配置、SDK、ELF 与 HIL 工装入口在 `tools/`、`cmake/package/`，见
[交付入口](../docs/reference/platforms/index.md)。

| 目录 | 责任 |
| --- | --- |
| `ci/` | 稳定 CI／hook 入口及其测试：格式、Actions、ARM、OS、供应商身份 |
| `ci/qualification/` | ARM 资格检查内部共享的预算验证，不提供独立命令 |
| `setup/` | 每个 clone 的工具和 Git hook 安装 |
| `validation/` | 真实、最新、非空测试报告准入 |
| `tools/` | Python 格式化命令与共享开发工具锁读取 |

缺工具、零范围会失败。`scripts/tools/format.py` 显式调用才应用根格式；hook 只检查，
不悄悄修改或暂存文件。在仓库根安装并检查 Native：

```sh
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python tools/dev/dev.py configure --preset native-debug
.venv/bin/python tools/dev/dev.py build --preset native-debug --parallel 4
.venv/bin/python tools/dev/dev.py test --preset native-debug
```

依赖以 Gitlink、锁文件和[兼容性策略](../dependencies/platform-compatibility.json)
为准，不在这里重复维护子模块数量。GD32F30x／F4xx 原导入另有逐文件锁。目录存在
不启用平台或模式；ARM 链接和 Native 模型不建立实板资格。
