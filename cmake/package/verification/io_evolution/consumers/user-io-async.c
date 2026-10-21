/**
 * \file            user-io-async.c
 * \brief           Public-only copied UART mediation ARM link scenario
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/user_io_async_storage.h"

/**
 * \brief           Link the public lifecycle with explicit trusted assembly.
 * \note            This is a link entry, not a reset handler or runtime test.
 *                  The integrating kernel supplies real authenticated identity,
 *                  fault-safe copies, persistent exclusive session authority
 * and gateway join proof. No leaf stubs claim those protections.
 */
nx_result_t nx_user_io_async_public_consumer(
    nx_user_io_async_t* mediation, nx_user_io_async_slot_t* cells, size_t count,
    const nx_user_io_async_config_t* config,
    const nx_user_io_async_message_t* user_message, const uint8_t* user_payload,
    nx_user_io_async_reply_t* user_reply);

nx_result_t nx_user_io_async_public_consumer(
    nx_user_io_async_t* mediation, nx_user_io_async_slot_t* cells, size_t count,
    const nx_user_io_async_config_t* config,
    const nx_user_io_async_message_t* user_message, const uint8_t* user_payload,
    nx_user_io_async_reply_t* user_reply) {
    nx_result_t result =
        nx_user_io_async_construct(mediation, cells, count, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_user_io_async_dispatch(mediation, user_message, user_payload,
                                       user_reply);
    /* A failed user reply may still retain an accepted copied cell. Trusted
     * shutdown always closes, then separately drives the real existing owner.
     */
    (void)result;
    result = nx_user_io_async_close(mediation);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_bus_owner_service(config->owner);
    if (result != NX_SUCCESS) {
        return result; /* Including quarantine: keep every binding live. */
    }
    result = nx_user_io_async_reap(mediation);
    if (result != NX_SUCCESS) {
        return result; /* Kernel continues explicit owner/reap recovery. */
    }
    return nx_user_io_async_join(mediation);
}
