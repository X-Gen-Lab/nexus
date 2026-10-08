# Nexus scripts

构建与测试共用 CMakePresets.json 和 `scripts/ci/ci_build.py`，必须显式选择
`--preset`。不同产品、板卡和构建模式使用各自的 build 目录与有效配置。

```sh
python scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
python scripts/ci/ci_build.py --preset linux-gcc-release --stage build --jobs 4
python scripts/ci/ci_build.py --preset linux-gcc-debug --stage test --jobs 4
python scripts/ci/ci_build.py --preset stm32-armgcc-release --stage build --jobs 4
```

`build.sh`、`build.bat`、`building/build.py`、`building/build.sh` 和
`building/build.bat` 只委托上述 CLI；shell/batch 入口默认 `--stage build`。
`test/test.py`、`test/test.sh` 与 `test/test.bat` 默认委托 `--stage test`。
所有入口传递失败退出码。它们不自动探测根 .config、不推断工具链、不删除
构建目录。过滤已配置的测试使用 `ctest --preset linux-gcc-debug -R <regex>`，
并保留 `--no-tests=error`。

配置工具操作显式输入片段和专用输出目录：

```sh
python scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
python scripts/nexus_config.py generate --build-dir build/inspect --config platforms/native/defconfig
python scripts/nexus_config.py info --build-dir build/linux-gcc-debug
python scripts/kconfig/test_effective_config.py
```

生产构建由 CMake 再次生成单份 effective.config/header/CMake bundle。
配置 CLI 的输出用于检查，不能替代预设构建的有效配置。

企业流程脚本位于 `ci/`、`evidence/`、`hil/` 和 `manufacturing/`；其证据与
支持范围见 `docs/implementation/`。格式化和文档工具位于 `tools/`，依赖
工具缺失时不能把检查列为通过。

Linux Native 构建需要 Python 3.9+、Kconfiglib、CMake 3.21+、Ninja、GCC/G++
以及 OpenSSL 3 开发包。GoogleTest/FreeRTOS/CMSIS/ST HAL 使用仓库固定的
子模块提交；配置阶段不下载依赖。ARM 构建另外需要 arm-none-eabi 工具链，
板上执行需要真实 HIL。Windows/macOS 预设保留，但本轮未执行这些平台。
