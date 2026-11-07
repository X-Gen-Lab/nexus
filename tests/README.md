# 平台测试架构

测试按所证明的边界组织，不把 host、ARM 链接与实板混为一次“通过”。固件始终
C11，GoogleTest／GoogleMock 只在 host 使用 C++17。

| 目录 | 责任 |
| --- | --- |
| `google/` | 真实生产算法的行为合同，mock 外部依赖／寄存器边界 |
| `contracts/` | 公共构建、CPU／ABI、SDK消费、资源、真实kernel和选定ARM装配合同 |
| `evidence/` | 固定开发阶段原件与紧凑索引，不是当前源的自动资格 |
| `cmsis_host/` | host模型所需的CPU／CMSIS边界 |
| `cmake/` | 合同目标的测试构建辅助 |

普通Native入口见[工程手册](../docs/design/engineering-handbook.md)；完整行为门禁为
`python scripts/ci/tdd_gate.py --all --preset native-debug`。工具先准备，不隐式下载。
必须保留真实discovery、无过滤执行、新报告、命令／exit和精确source／config。
空、陈旧、跳过或失败不能替代验收；RED必须是真实预期失败而非环境缺工具。

模型检验软件关系，ARM检查实际向量、对象和链接预算，SDK检查移动与边界，物理
HIL另绑PCB／station／image实测。旧结果不传递到新HEAD，详细入口见
[合同夹具](contracts/README.md)和[Google测试](google/README.md)。
