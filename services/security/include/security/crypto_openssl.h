/** Optional Native OpenSSL 3 provider. SPDX-License-Identifier: MIT */
#ifndef NX_SECURITY_CRYPTO_OPENSSL_H
#define NX_SECURITY_CRYPTO_OPENSSL_H
#include "crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/** This is a borrowed immutable provider with static lifetime. Link the
 * explicit CryptoOpenSSL target and call nx_crypto_set_provider() while all
 * callers are quiescent. Merely obtaining it does not activate the core. */
const nx_crypto_provider_t* nx_crypto_openssl_provider(void);
#ifdef __cplusplus
}
#endif
#endif
