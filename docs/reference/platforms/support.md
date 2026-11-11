---
id: docs-reference-platforms-support
type: reference
audience: consumer
source_language: zh_CN
---

(docs-reference-platforms-support)=
# 读取精确平台支持

当前生产系列入口是 STM32F4，精确器件与模式统一见[详细支持](../../delivery/support.md)。
五个已审料号的事实、四个实体 F407 Board 包及新增料号的内部 CRC 软件参考分别维护。
同系列名称、官方 SDK 文件或器件宏存在，不代表全部 F4、外设组合或实体 PCB 已验证。

| 维度 | 当前含义 |
|---|---|
| 芯片事实 | 精确料号、封装、Flash／RAM 和允许路线 |
| 实现与模式 | F4 provider 明确维护的有限模式 |
| Board | 四个独立 F407 包中已审查的连接与条件 |
| 软件验证 | 精确源码、配置、工具和本次实际执行 |
| 物理验证 | 同一镜像、PCB、station 和真实观测 |

模式限制见[详细支持](../../delivery/support.md)，路线、夹具和生产源码关联见
[派生矩阵](../../delivery/capabilities.md)，目录见[源码图](../../explanation/source-map.md)。

## 当前可选择范围

- 启明 F407ZG V3.1、独立核心板和 V4.1 保留不同来源与装配身份。
- 天空星 F407VE 青春版使用其独立封装和 PCB 事实。
- 裸机、FreeRTOS 和明确维护的 ThreadX M4 子集分别选择；启动不进入内核。
- Native 只属于 `tests/support/native/`，需要 `test_support=True`、CLI
  `--test-support` 或声明了 `NEXUS_TEST_SUPPORT=ON` 的测试 preset。
- Core／Arch 的 CPU 软件能力与具体 SoC 集成分开，不从 CPU 名称推导芯片支持。

所有 `vendors/` 保留，未使用 SDK 的存在不扩大生产支持。旧 F103、G474、H743、
U575 和 GD32 等接入退出当前生产选择；原证据按
[固定旧源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
读取，不将旧通过值转移到重构源码。

## 使用前检查

1. 固定完整料号、Board、振荡源和必要跳线条件。
2. 查看对应控制器、route、mode 和资源冲突。
3. 解析实际 TOML；未知字段、未维护模式与冲突必须拒绝。
4. 核对同一源码与配置的模型、ARM、SDK 或候选记录。
5. 需要物理行为结论时，核对实际 station 原件。

异步 ACCEPTED 后持续借用到 acquire 观察 SETTLED；timeout、cancel 与排空失败
不授权复用存储。物理 HIL 当前为 `not_executed`，未知 PCB 修订仍保持未知。
完整资格边界见[资格参考](qualification.md)。
