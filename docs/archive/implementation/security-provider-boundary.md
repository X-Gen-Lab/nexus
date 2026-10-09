# RF-CMP-03：密码 facade 与 provider 的明确装配

本批将通用 Security core 与 Native OpenSSL provider 拆成真实独立 targets。`nexus_security`（`Nexus::SecurityCore` / `Nexus::Security`）只编译 `src/crypto.c`；`nexus_security_openssl`（`Nexus::CryptoOpenSSL`）由有效 `CONFIG_CRYPTO_PROVIDER_OPENSSL` 创建并私有链接维护中的 OpenSSL 3。符号默认 n，只在 Native 可选择；full Native test defconfig 显式 y。

所有平台 core 的初始 provider 均为 NULL。删除 `nx_crypto_use_default_provider()` 及静态隐式 OpenSSL default，公开 `security/crypto_openssl.h` 的 immutable provider getter；应用/测试 startup 直接 `nx_crypto_set_provider(nx_crypto_openssl_provider())`。link 或 getter 调用不能自动激活 crypto。原显式 default 调用、Config 加密 fixture、跨进程 Config snapshot/rotation、Update 模型全部更新。

通用 core 不拥有产品熵配置、key vault、trust store、health、bootloader 或 rollback 决策。provider 替换仍为 quiescent 外部管理操作；任务、blocking/分配与 AEAD 临时存储限制见 [Security 公开说明](../../services/security/README.md)。本批不改持久化格式、恢复不变量或 key 生命周期。

## 真实验证（2026-10-09，Asia/Shanghai）

1. full Native Debug 真实 configure/build，使用 OpenSSL 3.5.7；定向 CTest 执行 **77 项，0 failure、0 skip**，覆盖 ConfigCrypto 单元和 property、NIST AES-GCM/SHA256、RFC8032 Ed25519、partial capabilities、无 provider、Config durable snapshot/key rotation 和 Update state/storage 联合故障模型。provider getter 及 library 已链接，但首次显式 bind 前仍 disabled 的行为有 assertion。
2. Native core-only 独立配置 `CRYPTO_PROVIDER_OPENSSL=n`、`NEXUS_BUILD_TESTS=OFF`，额外 `CMAKE_DISABLE_FIND_PACKAGE_OpenSSL=TRUE`，configure/build `nexus_security` 成功；直接消费该实际 archive 的 disabled test 执行成功，所有有效必需操作明确 UNSUPPORTED。没有借用旧 raw crypto.c 假装验证生产目标。
3. 已删除全部自有源码的 use_default_provider / NX_SECURITY_OPENSSL fallback 入口。依赖选择来自 generated config；通用测试 startup 使用生成头的 `NX_CONFIG_CRYPTO_PROVIDER_OPENSSL`，CMake 使用对应 `CONFIG_*` 变量。

本地报告是 `build/security-refactor/native/security-results.xml`、`security-tests.log`、`core-provider-build.log`、`core-only-configure.log`、`core-only-build.log`。这是提交前工作树定向执行，不代替之后的干净完整平台矩阵/工件身份。同期 Config fake 的 integration target 装配完成后，integration_tests 实际重新构建，定向 Integration CTest **52 项，0 failure、0 skip**，包括加密调用者；报告为同目录 integration-results.xml / integration-build.log / integration-tests.log。上述是定向 77+52 项，完整平台全量仍单独绑定。

未执行 MCU 密码 provider、真实熵健康/nonce 生命周期、key vault/受保护counter、物理Flash掉电、安全boot、企业signing或产品密码认证。这些仍为外部应用/产品资格工作；不能把本批 core/provider 解耦说成完整产品安全支持。
