---
id: reference.contracts.sdk-format
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.sdk-format)=
# 源码 SDK、版本与生成格式

Nexus 交付可重新装配的源码 SDK。消费方用同一配置、编译器与 ABI 重编平台、provider、生成 bindings 和调用者。当前不承诺稳定二进制 ABI。

## 当前格式的唯一数据

`dependencies/platform-compatibility.json` 是语义版本、factory 代际和各 schema 的唯一策略数据。`tools/configure/metadata.py` 是公共严格 loader。正文只解释字段，不维护另一份数值表。

```{literalinclude} ../../../dependencies/platform-compatibility.json
:language: json
:caption: 当前接口与格式策略；值由实际生产入口共同检查。
```

| 字段 | 作用 |
| --- | --- |
| `semantic_version` | CMake、生成头与 SDK 元数据共用的完整版本 |
| `factory_abi_epoch` | 当前生成源码接口代际；不是二进制 ABI 承诺 |
| `schemas.assembly_authoring` | 唯一作者 TOML 输入格式 |
| `schemas.assembly_projection` | `resolved.json` 单向审计输出格式 |
| `schemas.typed_ir` | 私有 Python 类型化源码合同 |
| Board/SoC/routes 与 CPU runtime schemas | 各事实和装配入口的独立格式 |
| `schemas.source_sdk_manifest` | 已安装源码包的完整文件映射与导出声明 |

这些值分别负责自己的边界，不能互相替代。未知格式、未知字段、缺少策略、布尔伪整数、非规范版本和 symlink 输入均拒绝；校验不会选择替代硬件或静默修复输入。

factory 的紧凑 ID 只在该 bundle 内有意义。它返回已有静态接口，不初始化硬件；ID 不可持久化后跨 bundle 使用。所有 Platform bundle 都生成 strong factory，未选类别没有 getter/ID，普通调用检查 NULL 及生命周期状态。

## SDK 文件映射与导出

Manifest 的 `source_exports` 记录实际导出的平台成员和包身份，每个成员接受 `files_sha256` 字节校验。包声明生产导出范围；CMake 仍拥有唯一编译图。导出范围不是某个 ELF 的翻译单元闭包。

`profile` 明确记录 backend、components、host_support 与 packages。默认包来自策略的 `source_sdk.default_package_manifests`；新增家族不自动进入 SDK。验证使用导出时冻结的包选择，不把未选库存补入已安装包。

消费方使用该 SDK 自己的 verifier 和 resolver。严格验证拒绝缺失、额外、修改或 symlinked payload，以及 expected revision/版本不一致。源码 checkout 与安装 SDK 分别记录自身来源；消费者目录的 Git 身份不能代替 SDK 来源。

## 当前检查与升级

从完整仓库根准备所选工具后运行 `python tools/maintenance/compatibility.py`。`--generated` 指向真实生成的 `nexus_config.h`，`--sdk` 指向安装包的 `share/nexus/src`。工具观察生产 validator/emission、实际事实和真实 CMake 版本测试，结束前复验参与字节。

一致性只证明被检查元数据相符，不能替代软件执行、镜像资源、签名或物理验收。版本或格式行为改变先记录真实拒绝/接受回归，再修改唯一策略和实际入口。没有实现的格式不得被声明可接受。

更换 SDK 的操作见[使用源码 SDK](../../guides/use-source-sdk.md)：更新精确 pin/锁、验证选中内容、使用独立 builddir、重新生成并重编所有调用者。产品持久化格式、密钥和恢复政策不由平台自动升级。

依赖的组织 fork、gitlink、许可证和按任务准备规则见[依赖维护](../../development/dependencies.md)。
