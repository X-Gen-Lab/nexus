# 平台测试架构

测试验证当前保留合同；固件保持 C11／必要汇编，GoogleTest／GoogleMock 仅在主机
使用 C++17。生产 MCU 支持当前只有 STM32F4，不再要求退出平台的历史夹具存在。

| 位置 | 证明范围 |
|---|---|
| [behavior/](behavior/README.md) | 按 Core／Arch／I/O／OS／SoC／组件归类的真实 C 行为 |
| [integration/](integration/README.md) | C11 消费、CPU／ABI、F4 装配、真实内核、链接预算和安装 SDK |
| `cmake/` | 共享测试目标、生成配置、精确 GoogleTest 计划与 CTest 注册 |
| `support/native/` | 显式主机模型、测试 Board 和装配 |
| `support/arm/` | F4 CPU、CMSIS 宿主边界与指令探针支持 |
| `support/network/` | TLS／MQTT 对端夹具及测试凭据说明 |

Native 需要 `test_support=True`、CLI `--test-support` 或 CMake
`NEXUS_TEST_SUPPORT=ON`。四个 Native preset 继续执行主机合同，不成为第二个生产
SoC。模型与 production C 共享实际实现，不能另写一份成功算法代替被测代码。

保留拒绝、部分失败、取消、迟到 IRQ、排空失败、隔离和实例独立性回归；退出平台
专属用例与失去用途的旧路径断言删除。删除测试需说明旧合同已退出或已有等价独立
覆盖，不以测试数量减少作为验收。

测试源码目录调整后，已有 ExternalProject 子构建可能仍缓存旧源码路径。使用新的
构建目录，或在保留原始报告后清理对应子构建缓存，再由维护命令重新配置；不要为
失效缓存增加旧源码目录或兼容转发层。

`behavior/` 依被测模块和实现类别分层，`integration/` 依跨模块消费场景分层。
共享模型只保存硬件或外部依赖边界，测试断言与所属行为或集成场景一起维护。

普通入口为 `tools/dev/dev.py test --preset native-debug`，行为提交按
`python scripts/ci/tdd_gate.py --all --preset native-debug` 执行既定完整门禁。
hook 与 Native CI 共享同一完整 CTest 计划，随后逐例核对未过滤的 GoogleTest
发现结果和 XML 身份。C11 消费程序与 GoogleTest 程序均通过实际入口执行。RED
必须是实际预期行为失败；GREEN、新报告和完整执行身份分别保存。空、跳过或旧报告
不能替代通过。规则见[工程手册](../docs/reference/contracts/engineering.md)。

模型检验软件行为，ARM 检查实际向量与链接，SDK 检查独立消费；实板另绑 PCB、
station 和镜像。源码迁移不继承旧结果，当前物理 HIL 仍为 `not_executed`。

Python 的配置与工程工具回归分别就近放在 `tools/configure/tests/`、
`tools/testing/` 和 `scripts/ci/`。独立集成工具用例与其场景位于
`integration/`，显式通过 `--suite integration` 执行。FreeRTOS overlay 与文档
分别由所属 CI 入口验证。具体范围和预算见[测试工具](../tools/testing/README.md)。
