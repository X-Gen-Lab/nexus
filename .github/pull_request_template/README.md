# 平台 PR 模板

普通平台变更使用默认模板，SoC／Board／provider 接入使用 `platform.md`，小型可逆
变更使用 `simple.md`，具体回归修复使用 `hotfix.md`。标题采用 Conventional Commits，
正文先说明问题与变更后的行为，例如 `feat(io)`、`fix(os)`、`build(config)`、
`docs(delivery)`。

评审核对[当前交付](../../docs/delivery/README.md)中的合同与证据，并遵守
[代码规范](../../docs/sphinx/development/coding_standards.rst)。本地 commit hook
与 CI 使用相同格式、注释规则；Native／模型执行、实际 ARM 链接、物理 HIL
分别建立证据。

平台由 Codex 维护，高风险变更保留独立评审。实际 PR 评审记录与服务器分支规则
分别核实，角色文件不建立评审通过或 branch protection。产品策略、私有 PCB 数据
与应用工作任务留在外部仓库。
