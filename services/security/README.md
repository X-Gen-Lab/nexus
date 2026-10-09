# Security core 与显式 provider

`Nexus::SecurityCore`（`Nexus::Security`）只提供参数校验、能力派发、错误处理和认证后提交输出，没有厂商、板卡、OSAL 或 OpenSSL 链接依赖。所有平台启动时 provider 都为 NULL，调用有效的 random/AEAD/hash/signature 请求返回 `NX_CRYPTO_UNSUPPORTED`。只编译或链接一个 provider 不会启用密码能力。

有效 Kconfig 的 `CRYPTO_PROVIDER_OPENSSL=y` 仅在 Native 创建可选 `Nexus::CryptoOpenSSL`。外部应用在启动或所有调用停止的维护阶段明确绑定：

```c
#include "security/crypto_openssl.h"
nx_crypto_status_t status = nx_crypto_set_provider(nx_crypto_openssl_provider());
/* 必须检查 status。平台没有默认 provider 注册或 lazy constructor。 */
```

`nx_crypto_openssl_provider()` 返回静态 immutable 的维护中 OpenSSL 3 adapter；没有 Nexus 自制密码原语。Native full test profile 显式选择该 target，测试程序 startup 明确绑定，core-only 或 MCU profile 不选择它。MCU 可在外部集成受维护的软件/硬件 provider：通过 `nx_crypto_provider_t` 显式注册，缺失 callback 按单项能力返回 UNSUPPORTED。

provider/context/callback 在所有调用结束前必须保持有效；替换不具备线程同步，必须由外部应用 quiesce 后进行。API 是任务上下文、同步调用，provider 可以分配和阻塞；应用管理等待预算。AEAD open 使用最多 `NX_CRYPTO_MAX_AEAD_SIZE` 的临时分配，仅认证成功才更新调用者输出，所有临时明文随后擦除；不宣称全静态 crypto。

熵源必须来自合格 CSPRNG/硬件与维护库，失败传播；UID、tick 或 key bytes 不能作种子。core 不选择产品密钥、trust store、身份、nonce 生命周期、counter、health 或 rollback 政策。加密记录格式/nonce budget 由记录组件及外部应用承担，产品真实熵与 vault/保护边界需要独立验证。

本轮验证包括已编译 provider 在显式绑定前依然 disabled；OpenSSL NIST/RFC 向量和篡改失败；partial callback 缺失；entropy failure；Config 加密/世代轮换及 Update 状态故障模型。当前真实范围见 [实施记录](../../docs/implementation/security-provider-boundary.md)，软件模型不表示 MCU entropy、HIL、现场 trust/bootloader 或产品安全资格完成。
