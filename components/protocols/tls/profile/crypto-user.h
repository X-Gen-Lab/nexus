/**
 * \file            crypto-user.h
 * \brief           Caller arena, explicit entropy driver and trusted UTC hook
 * \author          Nexus Team
 */
#include <stddef.h>
/* Upstream defaults to 4, which misaligns allocator metadata on 64-bit hosts.
 */
#if defined(__cplusplus)
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE alignof(max_align_t)
#else
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE _Alignof(max_align_t)
#endif

#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_SNPRINTF_ALT
#define MBEDTLS_PLATFORM_VSNPRINTF_ALT
#define MBEDTLS_PLATFORM_EXIT_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_PLATFORM_GMTIME_R_ALT
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_TIME_ALT
#undef MBEDTLS_PSA_BUILTIN_GET_ENTROPY
#define MBEDTLS_PSA_DRIVER_GET_ENTROPY
#undef MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
#undef PSA_WANT_ECC_SECP_R1_384
#undef PSA_WANT_ALG_SHA_384
#undef PSA_WANT_ALG_SHA_512
#undef MBEDTLS_PEM_PARSE_C
#undef MBEDTLS_BASE64_C
