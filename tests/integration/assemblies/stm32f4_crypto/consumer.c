/**
 * \file            consumer.c
 * \brief           Exact-device crypto factory and linked lifecycle reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Linking this reference does not execute cryptographic hardware. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS)
        return 1;
    const nx_cipher_port_t* cipher =
        nx_factory_cipher_ref(NX_CIPHER_REF_ENCRYPT);
    const nx_hash_port_t* hash = nx_factory_hash_ref(NX_HASH_REF_DIGEST);
    nx_cipher_constraints_t cipher_limits;
    nx_hash_constraints_t hash_limits;
    nx_result_t cipher_result =
        nx_cipher_port_query(cipher, NX_CIPHER_AES_CBC, &cipher_limits);
    nx_result_t hash_result =
        nx_hash_port_query(hash, NX_HASH_SHA1, &hash_limits);
    nx_result_t stopped = nx_platform_stop();
    return cipher_result == NX_SUCCESS && hash_result == NX_SUCCESS &&
                   stopped == NX_SUCCESS
               ? 0
               : 2;
}
