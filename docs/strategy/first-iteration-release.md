# 发布候选：有效配置与产物完整性

对应 `REL-001`、`BAS-002`、`BAS-003` 和 ADR 006。`.github/workflows/release.yml` 先完成同一 source commit 的构建、测试和产物校验，再创建 draft candidate。工作流定义与打包夹具测试不代表 GitHub Actions 或硬件验证已经执行。

## 维护的候选矩阵

| preset | artifact | 验证范围 |
| --- | --- | --- |
| `linux-gcc-release` | `nexus-linux-gcc` | Linux x86_64、Native OSAL、主机测试 |
| `stm32-armgcc-release` | `nexus-stm32f407-baremetal` | STM32F407、STM32F4DISCOVERY MB997、裸机交叉编译 |
| `stm32-armgcc-freertos-release` | `nexus-stm32f407-freertos` | 同一参考板、FreeRTOS 交叉编译 |

源码 `4a283ebf6c2fa44162014d6bc7e263c7525775f4` 的实际 GitHub 构建已通过两种 STM32 ARM 配置的编译与完整链接，以及三个 Native 编译/构建组合各 1711 项测试。本地同一干净源码通过 Native Debug 1711 项、Release 5 个有限应用及 ASan/UBSan 37 项；本地未获得 LeakSanitizer 通过，线上所选 23 项已开启 leak 检查并通过。结果及身份记录见 [集成验证](../implementation/integration-validation.md)。这不是带匹配标签的三候选打包、实际 draft Release 或正式发布执行证据。

Windows/macOS 的 preset 保留供后续验证，当前发布包不承诺对应持久化适配。STM32 包仍是实现候选：实物 PCB revision、时钟、IRQ 优先级、电气行为、DMA 取消、断电恢复和实时预算需要独立 HIL 证据。GD32 尚未进入发布矩阵。

## 配置与身份

每个构建只使用 `build/<preset>/generated/` 中的 `effective.config`、`nexus_config.h` 和 `config.cmake`。打包器逐项核对三者，确认有效 Release 模式、平台、工具链、OSAL、板卡、芯片和 ARM CPU/FPU/浮点 ABI；不再读取已经移除的 `NEXUS_CONFIG_HEADER`、`NEXUS_CPU_ARCH` 等旧缓存别名。`NEXUS_CONFIG_FILE` 仅记录输入片段，不能替代最终有效配置。

构建包保存这三份生成文件、输入片段、实际 CMake cache、编译命令、preset、工具链文件、source commit、实际使用的固定依赖提交、目标身份、文件大小和 SHA-256。生产编译命令必须使用本构建的生成目录，ARM 编译参数必须符合目标；至少存在一个架构相符的 ELF 参考应用。只有文档、静态库、裸 `.bin` 或 `.hex` 不满足候选门禁。

Native 还必须提供非零、无失败的 `ctest-results.xml`。包记录实际执行与跳过数量，并保存可用的测试日志；不会将跳过的测试计入成功执行。ARM 包记录 `cross-compile-only` 和 `hardware_verified: false`，板版本仍为未知值。

打包与汇总都拒绝标签/commit 不一致、非 Release 或开启覆盖率或 Sanitizer 插桩的构建、配置分歧、目标冒充、源代码未提交修改、未初始化或漂移的必需依赖、符号链接、路径逃逸、缺失产物以及被修改的校验清单。未使用的其他芯片 SDK 不构成依赖证据，也不阻断当前 profile。汇总阶段再次核对依赖身份与 source commit 的 Git gitlink，并验证完整三候选集合。

## 执行顺序与权限

1. 校验 `vMAJOR.MINOR.PATCH[-prerelease]`、已有标签与选定 commit；输入通过参数数组传给 Git。
2. 三个 job checkout 同一 commit，初始化维护依赖，配置并构建。Native 执行 CTest，拒绝零测试并输出 JUnit。
3. 各 job 打包已校验产物，保留配置和测试证据。
4. 全部成功后校验完整候选集合，生成总 `SHA256SUMS` 与含实际执行数量的说明，再通过 `gh release create --draft --verify-tag` 创建草稿。预发布标签同时使用 `--prerelease --latest=false`。

默认权限为 `contents: read`，只有创建草稿的 job 使用 `contents: write`；checkout 不保留凭据。正式发布应复用审核后的相同工件，并完成产品门禁，不重新编译另一个镜像。

## 本地验证边界

`python -m unittest discover -s scripts/ci -p test_package_release.py -v` 验证临时本地 Git 仓库、真实本地子模块、生成配置束与 ELF 格式夹具。覆盖完整三候选、配置与编译参数冲突、源/依赖身份、非零测试、传输与包内校验、以及重算摘要后仍不允许错误配置或虚假硬件/测试声明。

这组测试不运行固件。真实 Native/ARM 构建与合同测试结果记录于 `docs/implementation/`，实际在线候选需在已提交、标签匹配的干净 checkout 中执行。本次迁移还直接核对了实际 Native 构建生成束与编译命令，没有绕过 dirty-source 或标签门禁生成发布包。覆盖率修复 `e15df5c` 和最终文档 HEAD 的线上状态按 [PR 实际检查](https://github.com/X-Gen-Lab/nexus/pull/3/checks) 判断，不沿用之前提交的通过结果。

provenance 和 SHA-256 没有签名，提供完整性核对与追溯，不是真实性或可复现构建证明。HIL、固件签名、SBOM、供应链锁定和企业正式发布准入仍需各自独立完成；MCU 默认未配置密码提供者，不能据此发布要求加密配置或固件认证的产品。
