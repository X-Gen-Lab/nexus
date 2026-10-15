# 源码版本、factory 代际与依赖维护

Nexus 交付源码 SDK。消费方在同一配置、编译器和 ABI 选择下重新编译 Core、
provider、生成 bindings 和调用者；当前不交付稳定二进制 ABI。预编译库、不同 enum
选项、不同 FP ABI 或不同生成 bundle 的对象不能因为版本号相同而混合链接。

## 版本各自负责什么

`dependencies/platform-compatibility.json` 是源码接口代际的唯一策略数据。
`tools/configure/metadata.py` 是生产装配器和维护工具共用的严格纯 stdlib loader。
它拒绝重复键、未知字段、缺失策略、布尔伪整数、非规范版本和 symlink 输入；不会
迁移 TOML、选择硬件、执行构建或替换任何未知值。

| 项目 | 当前值 | 合同 |
|---|---|---|
| 平台 semantic version | `0.2.0` | 唯一人工维护值来自兼容性 JSON；CMake、生成头文件和包元数据派生并保持一致 |
| `factory_abi_epoch` | `1` | 当前唯一认可的生成源码接口代际；不是 binary ABI 保证 |
| source SDK | `source_only` / `exact` | CMake package 只接受相同完整版本，拒绝 ranges |
| authored assembly | `2` | 简化的 TOML 选择面；不改变 Board/SoC 事实 |
| projection / typed IR | `1` / `1` | 严格 resolver 与实际 emitter 的源码格式 |
| Board / SoC / routes | `1` / `1` / `1` | 物理事实文件及生产 validator 必须一致 |
| CPU runtime assembly / facts / resolved | `1` / `1` / `1` | CPU 软件配置，不能升格 SoC/Board 资格 |
| installed source SDK manifest | `1` | 既有完整 file-map/源依赖/package-export 校验 |

这些数字不能互相替代。修改文档或新增可选机制通常不改变配置 schema；改变 schema
意味着生产 validator/emitter 明确维护了新的格式，并更新策略断言与负测。策略不能
使尚未实现的 schema 获得支持。SDK manifest 增加一个被 file-map 覆盖的源码文件，
不等于修改 manifest 格式。

当前 `0.2.0` 用于开发与软件候选准备，不表示已公开发行或已通过候选验收。
本次 semantic version 变化保留 epoch `1`、exact 匹配和 authored schema `2`；
版本值不把旧 SDK／候选的资格转移到新源码。新候选仍须独立绑定实际 clean source、
配置、依赖、工具与执行证据。

`0.1.0` 是 2026-10-15（Asia/Shanghai）首个公开源码 SDK 开发者预览版。
之前源码快照中的 `1.0.0` 是公开发行前的版本值；2026-01-12 第一阶段的旧
`0.1.0` 文字保留为历史里程碑。该已发布版本的范围和原始证据见
[0.1.0 历史发行状态](../status/release-0.1.0.md)，准确 source／tree 以其发行
manifest 为准。既有 tag、发行附件和证据保持原始身份，不随当前开发版本改写。

factory 的 compact ID 只在该生成 bundle 内有意义。它返回既有静态 readonly face，
不分配、不初始化硬件。重新排序实例或改变配置可能改变 ID；不能把 ID 持久化后
拿到另一 bundle 使用。`factory_abi_epoch` 命名保留 ABI 字样，但目前只标记生成
源码接口代际：消费者、factory header、公共 face 和实现必须重新编译。当前 epoch
为 1；指定 0、未来值、非整数或找不到策略时必须拒绝，不能当成兼容 fallback。

生成配置须把策略原字节加入既有 inputs/snapshots/hash 闭包，并从唯一 loader
生成 `NEXUS_PLATFORM_VERSION` 和 `NEXUS_FACTORY_ABI_EPOCH`。这些是编译期常量，
不会增加运行时 registry、实例数量或 RAM。生产 emitter 与 SDK 打包注册完成后，
生成 header 校验和真实 SDK 消费才构成这条链路的完整验收；仅有维护工具不能代替
它们。历史 SDK 缺少该新策略，不被新工具自动认定为已维护代际。

## 自动检查

```sh
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py --expected-factory-epoch 1 \
    --generated build/native-debug/generated/nexus_factory.h
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py --sdk /abs/prefix/share/nexus/src
```

`--generated` 使用实际生成 header 的路径；不同构建根的路径可能不同。工具从真实
Python validators/emission 的 AST 读取 schema，读取实际事实文件，并运行现有
`NexusConfigVersion.cmake.in` 的相等版本、不同版本与 range 拒绝用例。它不复制
resolver 选择规则；如果 authority 的表达形式改变，检查会明确失败，维护者需
更新观察器。所有参与字节有 SHA/size，结束前复验，观察过程中变化不能报告一致。

`--sdk` 复用既有 `package_source_sdk.verify` 校验整个源文件映射、依赖、厂商 import
和 package exports，再检查策略及 schema/version 一致。`consistent` 只表示这些
元数据一致，不表示 fresh 全量测试、软件候选、签名、物理 HIL 或产品发布通过。

版本变更需先写拒绝/兼容契约测试，再修改唯一策略及实际 authority。source breaking
变更需明确 major/epoch 的变化和外部迁移；可选源码能力用 minor，保持公开合同的
缺陷修复用 patch。没有实际迁移实现时，禁止接受旧格式后静默选择新行为。

## Codex 的每周依赖流程

`tools/maintenance/dependency_proposals.py` 默认只观察当前实际 Gitlink、锁、归档和
许可证字节，网络读取为零。它覆盖 FreeRTOS、CMSIS、STM32 CMSIS、GD32 import、
GoogleTest、ARM 工具链、开发环境/Python 工具锁和 GitHub Actions 的确切 pins。
某项发行物还需审查许可证时，会明确记录 required，不从版本名猜许可。

```sh
.venv/bin/python tools/maintenance/dependency_proposals.py
.venv/bin/python tools/maintenance/dependency_proposals.py \
    --observe-upstream --upstream-output build/maintenance/upstream-fresh
.venv/bin/python tools/maintenance/dependency_proposals.py \
    --plan /abs/candidate-proposal.json
```

显式 upstream 模式只查询锁所对应的公共 GitHub origin 的 heads/tags。使用无 shell、
禁止 credential helper/交互/global config 的 `git ls-remote`；每个请求最长 10 秒，
stdout/stderr 总捕获上限 1 MiB，refs 最多 8192 条。复用已有有界 subprocess
capture/kill/reap 机制，不执行 HIL admission 或设备动作。输出目录必须新建且无
symlink；源码内只允许 build 目录。原始 stream、argv、实际退出码、时间和工具/
响应 SHA 均保留。网络、超时、输出或格式失败为 `not_observed`；没有 Git publication
的 GD32/工具链/环境项需单独审查发行来源，不能伪造版本观察成功。观察 refs 不是
选择最新可靠版本或批准升级。周度 workflow 的实际运行状态需另看 GitHub 证据。

candidate plan 使用严格 JSON：`schema_version=1`、已观察的 `dependency`、明确
`proposed_revision`、不带凭据/query 的公共 HTTPS `source_url`、实际非空 archive
identity、声明的 license expression 和 notices identities，以及可选的 `evidence`
映射。每个 identity 都是绝对 `path`、完整 `sha256` 和真实 `size`。Git 来源必须是
完整 40 位 commit，不能用 main/latest；license 内容必须独立审查，声明不是许可
批准。archive 是否来自该 commit、是否完整包含声明 notice 和所有变更也在独立
来源/许可证 review 中验证，工具不会从任意用户提供的 notice 推断这些事实。

| 必需 gate | 验收范围 |
|---|---|
| license_review | 来源/archive/commit、全部再分发文件、许可/notice 变更，由独立 Codex review 复核 |
| configuration | 全配置/内核边界、单一输入闭包、schema/epoch 与生成一致性 |
| host_models | pinned GoogleTest/GoogleMock 全发现执行、工具/CTest、维护的 Native sanitizer profiles |
| cortex_runtime | 当前 CPU/ISA/FP/MVE/ABI runtime、受维护 MPU 与 Secure/NS 软件矩阵 |
| board_arm | 三块精确 Board × baremetal/FreeRTOS，受影响的外部 examples |
| resource_budgets | 每个新 ELF 独立预算、同 fixture 前后成本、各 RAM 域和 reservation |
| installed_sdk | clean exact source SDK、版本/epoch 拒绝、实际独立消费者 |
| offline_reproduction | 绑定环境内两次 fresh network-disabled 构建及同一 ELF/BIN/map bytes |
| static_analysis | owned ClangTidy/Cppcheck、格式/注释、原始失败与明确 vendor 审查边界 |

工具只形成 proposal。缺证据为 `not_executed`；提供真实 file identities 也只变成
`referenced_unreviewed`。即使引用 JSON 自称 passed，工具也不升级资格，不执行其
命令、不修改 Gitlink/lock、不创建自动升级 commit。现有正式 validators 必须对
升级的 exact clean 源、完整 scope 和同一 artifact 独立执行。随后通过正常 staged
review、hooks 和 CI 原子更新源码 Gitlink/厂商 import、锁、许可和外部 SDK pin。

没有实板时，受升级影响的电气、IRQ/DMA timing、低功耗和长期负载仍是物理 pending。
一次 upstream request、一个模型 pass 或之前版本的软件候选都不能代替本次验收。
