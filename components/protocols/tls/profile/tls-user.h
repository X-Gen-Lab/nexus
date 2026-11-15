/**
 * \file            tls-user.h
 * \brief           Fixed no-network-backend TLS 1.2 P256 client profile
 * \author          Nexus Team
 */
#undef MBEDTLS_NET_C
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_RENEGOTIATION
#undef MBEDTLS_SSL_CIPHERSUITES
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
