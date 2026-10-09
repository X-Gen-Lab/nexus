/**
 * \file            test_main_common.cpp
 * \brief           Common Test Framework Main Entry Point
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Generic test entry point for non-HAL tests
 */

#include <gtest/gtest.h>
#include "nexus_config.h"
#if defined(NX_CONFIG_CRYPTO_PROVIDER_OPENSSL) && NX_CONFIG_CRYPTO_PROVIDER_OPENSSL
#include "security/crypto_openssl.h"
#endif


/**
 * \brief           Main entry point for tests
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument values
 * \return          Test result
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
#if defined(NX_CONFIG_CRYPTO_PROVIDER_OPENSSL) && NX_CONFIG_CRYPTO_PROVIDER_OPENSSL
    /* Explicit fixture startup, not implicit selection inside the SDK core. */
    if (nx_crypto_set_provider(nx_crypto_openssl_provider()) != NX_CRYPTO_OK) return 1;
#endif
    return RUN_ALL_TESTS();
}
