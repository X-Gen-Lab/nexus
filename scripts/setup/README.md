# 开发环境安装

从仓库根准备 Python 3.11+、CMake 3.31.6、Ninja 1.13.2 与 C11 编译器。
每个 clone 安装工具、pre-commit 和 commit-msg hook：

```sh
python tools/dev/dev.py prepare --scope native --allow-network
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
python tools/dev/dev.py doctor
```

prepare 审计源码拥有的精确图，按父先于子的层次初始化。去掉 `--allow-network`
只离线检查已有源码；未知声明、缺失依赖和变化 pin 会拒绝。固件 configure 不下载。

提交图含共享 `vendors/arm/CMSIS_6` 与所选 ST／GigaDevice Device、driver。
CMSIS-Core 固定官方 v6.4.0，release／头／pack 身份见
[供应商版本](../../vendors/VERSIONS.md)。它私有于实现，与公共 `Nexus::Core` 不同，
初始化不表示上游全部组件／芯片支持。

安装器通过 pre-commit migration 保留旧 hook，拒绝显式 `core.hooksPath`。
CI 用 `--skip-hooks` 安装再独立执行相同检查。普通安装使用 `install --install-hooks`
准备隔离环境，即使外层工具已匹配。TDD 环境精确固定 pre-commit、clang-format、
PyYAML，并与开发／workflow 锁交叉检查。

离线可设 `PIP_NO_INDEX=1`，`PIP_FIND_LINKS` 指向验证后的 wheel 导出，同样用于
hook 隔离环境。导出含运行工具与 `dependencies/hook-build-tools.txt` 的构建后端。
根 `.clang-format`、`.editorconfig` 为格式权威，版本以开发／环境工具锁为准。

ARM 使用摘要验证的 GNU 14.3.rel1：

```sh
python scripts/ci/install_arm_toolchain.py --install-dir /your/toolchains/arm
```

将其 `bin` 加 PATH，通过维护 preset 构建。正式离线复现使用实际封存 OCI 环境，
可变宿主工具检查另有范围。
