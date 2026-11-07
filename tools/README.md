# 平台开发、测量与交付工具

这些宿主工具配置和检查固件，不成为 MCU 运行时。硬件行为由 C11 和必要 Arch 汇编
维护，不通过运行时寄存器脚本解释器执行。

| 目录 | 输入、输出与责任 |
| --- | --- |
| `dev/` | preset configure／build／test 薄入口和显式准备 |
| `configure/` | 一份装配与 SoC／Board 事实生成原子包 |
| `measurement/` | 实际 ARM ELF、物理内存／配置／预算、最小镜像比较 |
| `hil/` | 工位和真实物理证据准入，不操作未接入设备 |
| `evidence/` | 干净源码、文件、日志、SDK、环境与候选字节 |
| `maintenance/` | 能力矩阵、依赖和 agent-managed 所有权策略 |
| `release/` | SDK 预览发布与完整消费者证据运输 |
| `testing/` | 宿主 TDD 与工具负路径合同 |

Codex 承担维护，不要求虚构实名账号；独立评审和自动门禁需实际执行。
本地角色检查不配置 branch protection，也不建立硬件资格。

## 日常操作

工具前提见[安装说明](../scripts/setup/README.md)，从仓库根执行：

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

## SDK、复现与封存

SDK 要求干净源码与固定子模块，完整 prefix 检查文件和 CMake 导出。不可发布开发
fixture 不建立候选。以下命令展示输入关系，`/your/` 由执行工程提供：

```sh
python cmake/package/package_source_sdk.py --source . --output /your/source-sdk
python cmake/package/verify_consumers.py --prefix /your/source-sdk --output /your/sdk-checks
python tools/evidence/reproduce.py --prepare-spec --spec /your/repro-spec.json \
  --source . --assembly tools/configure/assemblies/liangshan-baremetal-empty.toml \
  --source-sdk /your/source-sdk/share/nexus/src/.nexus-source-sdk.json \
  --environment /your/observed-environment.json --toolchain /your/arm-toolchain
python tools/evidence/reproduce.py --spec /your/repro-spec.json --report /your/repro-report.json
```

正式执行前用 `tools/dev/Dockerfile`、`evidence/container.py seal` 准备并验证实际
image／OCI archive。配方固定 base、记录实际依赖，不承诺可变准备源重新生成同一
镜像。固件执行断网，SDK／toolchain 只读、输出独立、不挂载凭据。

候选需全部适用软件资格实际执行，递归保留原始日志和引用产物。报告放在源码树外
避免自引用；硬件、产品晋级与签名分别记录。版本、workflow 和历史分支职责见
[发布维护](release/README.md)。

[历史归档](../docs/archive/evidence/README.md)保留原始集合、成员和恢复说明。
当前导航采用紧凑引用；恢复旧集合不为当前源码建立资格。
