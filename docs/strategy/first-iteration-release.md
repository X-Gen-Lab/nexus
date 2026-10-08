# 首批迭代：发布候选与产物完整性

本次修复范围为 `.github/workflows/release.yml`、`scripts/ci/package_release.py` 和对应测试。未向 GitHub 写入、创建 Release、推送分支或标签。

## 行为变化

原工作流先创建正式 Release，再执行矩阵构建；产物复制使用 `|| true`，构建目录缺失时仍可能形成仅含文档的包。现在按以下顺序执行：

1. 校验版本格式、已有 Git 标签与 checkout commit 一致。手动输入经环境变量传递，并由 Python 使用参数数组调用 Git。`-rc.1` 等预发布标签将候选草稿标为 prerelease，且不标为 Latest。
2. Windows MSVC、Linux GCC、macOS Clang 和 ARM GCC 分别构建同一个已验证 commit。ARM 使用存在的 `linux-stm32-armgcc-release` preset；现有 setup-build 的 `*arm*` 检测可以安装该工具链。
3. 三个主机构建运行 CTest，并显式指定 `--no-tests=error`。ARM 只交叉编译；本流程不宣称完成板级验证。
4. 从 `bin/`、`lib/` 递归收集产物，包含 MSVC 等多配置生成器的 `Release/` 子目录，排除其他配置。至少存在一个可识别的 ELF、PE、Mach-O 或静态库；只有 README、裸 `.bin` 或 `.hex` 不满足发布前置条件。
5. 各包附实际 CMake cache 配置、Kconfig 输入、生成头文件、preset、工具链文件（如配置了）、源 commit、子模块 commit 和文件 SHA-256。打包与汇总阶段均要求元数据及 cache 的配置为 Release，并用四个明确的 target profile 绑定 artifact/preset、实际平台和编译器；ARM 额外绑定 CPU、FPU、浮点 ABI、工具链名称与路径。路径逃逸、符号链接、缺失配置、子模块未初始化/漂移、源文件未提交修改、配置或身份不匹配均使打包失败。
6. 全部构建成功后，下载并校验完整矩阵的同一套产物，再生成总 `SHA256SUMS` 和候选说明，最后通过 `gh release create --draft --verify-tag` 创建草稿。不会自动正式发布，也不会覆盖已有 Release。

工作流默认仅有 `contents: read`；创建候选的单独 job 才有 `contents: write`。各 job 不保留 checkout 凭据。

## 本地验证

`python -m unittest discover -s scripts/ci -p test_package_release.py -v`：34 个测试通过。测试使用临时本地 Git 仓库及二进制格式夹具，覆盖 CLI、标签/commit 匹配、预发布标记、输入注入格式、空包与缺失配置、Debug 误打包、Native 冒充 ARM、编译器/CPU/FPU/ABI/工具链不匹配、嵌套多配置产物、文件/目录符号链接、跨平台路径逃逸与校验清单注入、dirty source、子模块状态、完整产物集合、传输校验及包内文件被篡改。

另使用本环境已有的 PyYAML 校验工作流语法，检查四个 configure/build preset 存在、三个 host test preset 存在，以及候选 job 依赖和最小权限设置。

## 验证边界与后续

本环境是源码快照，不是原仓库 Git checkout；缺少 CMake，未执行真实 Native/ARM 构建、GitHub Actions 或硬件测试。二进制夹具测试不代表项目固件已编译通过。首个在线候选流程仍需在真实仓库验证四个平台。

目前 `.config` 可将 ARM Release/GCC preset 的构建类型或工具链声明覆盖为 Debug/armclang。打包器要求实际 cache 中的 `CMAKE_BUILD_TYPE` 与 `Release` 一致，否则明确失败，不能将 Debug 产物冒充 Release。下一阶段须统一 preset、Kconfig 与实际工具链的优先级并校验最终生效配置。

默认生成头文件仍可能写回已跟踪的源目录 `nexus_config.h`；当前 dirty-source 检查会拒绝这种状态。应将生成配置与头文件迁移到每个构建目录，并核对所有 target 的 include 路径。该阻断是已知待办，不能据此声称发布构建已恢复。

本次 SHA-256 与 provenance 用于完整性核对和追溯，没有签名，不等于真实性证明、可复现构建证明或固件签名。下一阶段补充受保护的发布环境、固件签名/密钥隔离、SBOM、工具链和依赖锁定、Actions 完整 commit SHA 固定、板级准入与候选转正式发布流程。
