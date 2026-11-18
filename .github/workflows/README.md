# 平台 CI

`ci.yml` 汇总 PR、main／develop push、nightly 和手动检查。远端 Actions 不可用时，
在准备好锁定工具的本地主机执行相同入口，保留真实返回码和新鲜报告。
工作流定义本身不代表执行成功。

| 工作流 | 责任 |
|---|---|
| build-matrix.yml | 四种 Native CTest／GoogleTest 模式、Arch／OS 编译检查、F4 板卡与外设装配 |
| enterprise-tools.yml | 六个完整工具域，覆盖配置、构建、依赖、SDK、预算和 HIL 工装 |
| quality-checks.yml | 与 hook 共用格式和注释规则；在真实编译数据库上运行静态分析 |
| docs-build.yml | 锁定工具、文档测试、Doxygen 和严格中英文站点构建 |
| security.yml | 依赖审计、CodeQL 和秘密扫描 |

Native 的 Debug、Release、ASan／UBSan、TSan 各执行完整 CTest 计划一次。
GoogleTest 报告必须与未过滤的发现结果逐例匹配，缺失、陈旧、跳过和失败均拒绝。
与编译模式无关的 Python 工具测试只由独立工具 job 执行一次。

普通 commit hook 读取完整暂存区，先检查所需工具，配置和构建一次，再执行完整
Native GoogleTest。普通固件实现不触发整套辅助工具域；公共头、声明事实和工具改动
选择完整相关测试模块，未知或缺少已知覆盖时保守执行完整域。提交报告不代表完整资格；
合并前通过 `--all` 独立执行完整工具矩阵、相关 ARM 与 SDK 验证。
CI 的完整 Native runtime 另由 `scripts/ci/native_contracts.py` 校验。
工具 job 只准备实际编译和生成输入，不重复完整 Native 执行；全局能力与文档一致性
检查只在 testing 域执行一次。依赖升级走普通 PR 和现有来源锁、许可证、消费者验证，
不再维护自动建议报告和虚构人员席位配置。

ARM 装配通过 `provider_assemblies.py` 检查实际来源、链接和预算。
`declared_io_assembly` 与 I/O evolution 的精确配置保留真实编译入口。
SDK 的文件闭包和独立消费由 contracts 工具域验证，不在各 Native 模式重复。
四个 Board 的 HIL prepare 只准备未绑定工装，
不能建立物理通过状态。

保留原始失败日志、JUnit、配置、ELF／BIN／map 和 SDK 验证结果。
历史候选晋升、归档运输、恢复和重复任务台账已退出当前平台维护范围。
版本发布和真实 HIL 仍需各自明确的范围及实际执行，不由普通 CI 隐式完成。
