# 平台测试架构

测试验证当前保留合同；固件保持 C11／必要汇编，GoogleTest／GoogleMock 仅在主机
使用 C++17。生产 MCU 支持当前只有 STM32F4，不再要求退出平台的历史夹具存在。

| 位置 | 证明范围 |
|---|---|
| `google/` | 按 Core／Arch／I/O／OS／SoC／组件归类的真实 C 行为 |
| `contracts/` | CPU／ABI、F4 装配、链接预算、OS 与 SDK 消费 |
| `support/native/` | 显式主机模型、测试 Board 和装配 |
| `support/f4_cpu/` | F4 模型所需 CPU 边界 |

Native 需要 `test_support=True`、CLI `--test-support` 或 CMake
`NEXUS_TEST_SUPPORT=ON`。四个 Native preset 继续执行主机合同，不成为第二个生产
SoC。模型与 production C 共享实际实现，不能另写一份成功算法代替被测代码。

保留拒绝、部分失败、取消、迟到 IRQ、排空失败、隔离和实例独立性回归；退出平台
专属用例与失去用途的旧路径断言删除。删除测试需说明旧合同已退出或已有等价独立
覆盖，不以测试数量减少作为验收。

测试源码目录调整后，已有 ExternalProject 子构建可能仍缓存旧源码路径。使用新的
构建目录，或在保留原始报告后清理对应子构建缓存，再由维护命令重新配置；不要为
失效缓存增加旧源码目录或兼容转发层。

普通入口为 `tools/dev/dev.py test --preset native-debug`，行为提交按
`python scripts/ci/tdd_gate.py --all --preset native-debug` 执行既定完整门禁。RED
必须是实际预期行为失败；GREEN、新报告和完整执行身份分别保存。空、跳过或旧报告
不能替代通过。规则见[工程手册](../docs/reference/contracts/engineering.md)。

模型检验软件行为，ARM 检查实际向量与链接，SDK 检查独立消费；实板另绑 PCB、
station 和镜像。源码迁移不继承旧结果，当前物理 HIL 仍为 `not_executed`。

`contracts/` 按独立集成合同保留现有入口；不为目录整齐移动全部软件 Board。
已退出维护的 H563 手动 I3C 头参考、TMP117 临时双系列布局测量和孤立 lwIP
链接探针不再保留；通用组件、CPU Runtime、有效故障回归与 SDK 消费继续维护。
旧夹具可从固定 Git 历史追溯，删除不表示它们的独特旧测量已经重新执行。
