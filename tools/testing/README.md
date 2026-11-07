# 宿主 TDD 与合同执行

新增公共 C API／provider 行为使用 GoogleTest／GoogleMock，固件保持 C11。
框架来自 `ext/googletest`，由 `dependencies/googletest.lock.json` 和 Gitlink 锁定。
先执行 `python tools/dev/dev.py prepare --scope native --allow-network` 或
`git submodule update --init --recursive ext/googletest`。配置只读本地框架，拒绝
缺失、修改、错误 pin，不下载依赖。固件和 SDK 消费不要求宿主框架。

所选组织 fork 基于上游 1.16.0（`6910c9d9165801d8827d628cb72eb7ea9dd538c5`），
修复 `RE` 所有权：禁用 copy-assignment、保留深拷贝构造，避免 POSIX 正则状态浅拷贝。
lock／Gitlink 选补丁提交，`upstream_commit`／官方 URL 记来源，`.gitmodules`
选已审组织检索路径。该补丁仅作用宿主测试。

从仓库根执行与 CI 相同门禁：

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

门禁执行 `testing`／`configure`／`ci`／`workflows` 四个完整工具域，构建
`nexus_google_contracts`，发现各程序全部用例后无 filter／shard 执行。新 XML 必须
是已完成通过、且身份与数量匹配 discovery；空、陈旧、跳过、失败、部分执行均不能
验收。原始命令结果位于 `build/<preset>/tdd/`，`execution.json` 是开发记录。

行为变更的 commit hook 调相同 gate，包括适用 tools／scripts 输入。较长 SDK／合同
和 FreeRTOS 工具测试有自己的 CI 域。gate 前后检查 staged 与未暂存／未跟踪行为，
要求 index tree 不变；迟到源码或暂存修改拒绝本次摘要。pre-commit 临时 stash 已
跟踪的未暂存编辑，读取当前 `.pre-commit-config.yaml`，新 gate 定义无需重装 hook；
CI 独立执行。子 configure／build／test 清除 Git 仓库局部环境，子模块使用自己的
index；gate 自身保留实际提交 index 做快照检查。

`evidence/` 记录选定 RED／GREEN。有些初期 RED 是新接口编译／链接失败；UART
owner、IRQ wake、stream storage、staged execution 另有真实行为失败。早期 owner
回归在实现后添加，仅有 GREEN。记录不能证明每次历史编辑都先 RED，也不建立 clean
candidate 或物理资格。

GoogleMock 替换依赖端口并调用真实 API，检查存储、准入、取消、隔离、实例独立、
通知和 DMA 排空。Python 工具使用 stdlib unittest，同样要求非空执行，无 skipped
或 expected-failure 用例。
