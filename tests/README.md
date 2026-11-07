# 平台测试架构

测试验证当前保留合同；固件保持 C11／必要汇编，GoogleTest／GoogleMock 仅在主机
使用 C++17。生产 MCU 支持当前只有 STM32F4，不再要求退出平台的历史夹具存在。

| 位置 | 证明范围 |
|---|---|
| `google/` | 真实生产 C 行为；替换外部依赖和寄存器边界 |
| `contracts/` | CPU／ABI、F4 装配、链接预算、OS 与 SDK 消费 |
| `support/native/` | 显式主机模型、测试 Board 和装配 |
| `support/f4_cpu/` | F4 模型所需 CPU 边界 |
| `evidence/` | 原始固定源码记录，不是新源码资格 |

Native 需要 `test_support=True`、CLI `--test-support` 或 CMake
`NEXUS_TEST_SUPPORT=ON`。四个 Native preset 继续执行主机合同，不成为第二个生产
SoC。模型与 production C 共享实际实现，不能另写一份成功算法代替被测代码。

保留拒绝、部分失败、取消、迟到 IRQ、排空失败、隔离和实例独立性回归；退出平台
专属用例与失去用途的旧路径断言删除。删除测试需说明旧合同已退出或已有等价独立
覆盖，不以测试数量减少作为验收。

普通入口为 `tools/dev/dev.py test --preset native-debug`，行为提交按
`python scripts/ci/tdd_gate.py --all --preset native-debug` 执行既定完整门禁。RED
必须是实际预期行为失败；GREEN、新报告和完整执行身份分别保存。空、跳过或旧报告
不能替代通过。规则见[工程手册](../docs/design/engineering-handbook.md)。

模型检验软件行为，ARM 检查实际向量与链接，SDK 检查独立消费；实板另绑 PCB、
station 和镜像。源码迁移不继承旧结果，当前物理 HIL 仍为 `not_executed`。
