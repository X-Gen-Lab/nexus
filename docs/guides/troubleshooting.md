---
id: guides.troubleshooting
type: how-to
audience: consumer
source_language: zh_CN
---

# 按失败阶段定位问题

先保留完整命令、第一条错误和实际日志路径，再按失败阶段检查。修改作者输入后
重跑对应命令；生成文件和旧通过报告不用于修复输入。

| 阶段与现象 | 检查与处理 | 成功判据 |
| --- | --- | --- |
| 找不到 Nexus package | 源码选 `<nexus>/cmake`，安装包选 `<prefix>/lib/cmake/Nexus`；核对 `Nexus_DIR` | 同一工程找到所选来源 |
| 工具链或 ABI 冲突 | 工具链在 `project()` 前选定；换工具链、装配或 SDK 时使用新 builddir | 实际编译器、CPU／FPU／ABI 与所选事实一致 |
| 缺少锁定依赖 | 源码执行所选 preset／assembly 的 prepare；只有显式 `--allow-network` 允许获取 | 选中闭包的 pin、来源、许可和工作区检查通过 |
| Native 被拒绝 | 模型解析和创建明确加 `--test-support`，CMake 明确 `NEXUS_TEST_SUPPORT=ON` | 模型仅在该测试支撑范围启用 |
| TOML 未知字段或 binding | 用同一平台的 schema、`list-bindings` 和当前 Board 查询；修正作者字段 | `check` 接受完整装配 |
| pin／DMA／IRQ／时钟冲突 | 根据诊断的资源和两个 owner 调整真实配置，不静默换路线 | `explain` 展示预期资源及容量 |
| SDK identity／epoch 不匹配 | 核对 SDK 完整提交、manifest、接口代际和应用锁；重新取得正确包 | 完整性校验及期望提交接纳 |
| 构建目录 busy | 等待该目录的真实持有者结束，或选择独立目录；不删除运行中的锁文件 | 一个构建目录只有一个写入者 |
| 请求超时或 stop BUSY | 保持被借用存储，继续既定 service／排空并检查 owner 状态 | 真正结清、归还及 join 完成 |

## 定位配置字段

以自己的 `assembly.toml` 为输入，路径相对当前工作目录：

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" check \
  --assembly assembly.toml --diagnostics-json
python "$NEXUS_SOURCE/tools/configure/configure.py" explain \
  --assembly assembly.toml --json
```

Native 输入同时加 `--test-support`。结构化拒绝写到标准错误，退出码为 1；命令行
语法错误使用 argparse 诊断和退出码 2。记录中的文件、键、资源、owner、mode 和
source 用来定位作者输入；`explain` 只在有效配置上提供结果，不自动修复拒绝项。

## 区分问题的责任

平台维护者处理公共机制、SoC／provider、配置合同和 SDK 导出。产品处理板级事实、
任务、缓冲、分区、授权与恢复；设备现场处理探针、供电、晶振、接线和观测。
Native 模型不接主机串口，软件 Board 的硅片路线也不代替实际 PCB 连通性。

提交问题时附精确源码或 SDK 提交、Board／part／backend、最小 TOML、实际命令、
错误和相关产物身份。移除凭据与私有 PCB 数据；不要用另一份 SDK 的成功结果替代
当前失败。所有权细节见[I/O 执行](io-execution.md)，物理检查见
[性能与 HIL](performance-hil.md)。
