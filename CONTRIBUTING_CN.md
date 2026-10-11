# Nexus 贡献指南

本仓库维护通用嵌入式机制，产品策略和应用属于外部仓库。
请阅读 [维护规则](AGENTS.md)、[架构合同](docs/design/README.md)
和[当前交付状态](docs/delivery/README.md)。

```sh
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

每个新 clone 都需安装 pre-commit 和 commit-msg。提交检查 staged 内容，
使用固定 clang-format 14.0.6 和当前根 `.clang-format`/`.editorconfig`，
不会自动暂存。保持现有四空格、80列和反斜杠 Doxygen 风格；声明说明上下文、
所有权、deadline 和失败语义，实现注释解释不变量及硬件访问顺序。
新建及修改文件按整文件严格检查，历史债务只允许减少。

生命周期、IRQ、并发和持久化修改需真实故障回归。Native 模型、ARM 链接资源
和物理 HIL 分别记录。没有实际设备执行时，不能把硬件资格标记为通过。
PR 描述最终行为、实际验证、适用 part/Board/backend/mode 及已知限制。
详细工具和评审流程见[英文指南](CONTRIBUTING.md)。
