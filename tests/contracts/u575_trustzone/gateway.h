/**
 * \file            gateway.h
 * \brief           Test-only fixed-principal scalar GPIO CMSE transport ABI
 * \author          Nexus Team
 */
#ifndef NX_U575_REFERENCE_GATEWAY_H
#define NX_U575_REFERENCE_GATEWAY_H
#include "nexus/components/user_io_broker.h"
#include "stm32u575_secure_batch.h"
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
#define NX_REFERENCE_GATEWAY __attribute__((cmse_nonsecure_entry))
#else
#define NX_REFERENCE_GATEWAY
#endif
/** \brief Synchronous NS RAM input/reply; caller retains both until return. */
NX_REFERENCE_GATEWAY uint32_t nx_reference_gpio(
    const nx_user_io_message_t* message, nx_user_io_reply_t* reply);
/** \brief Copy scalar batch input/reply; no NS address survives this call. */
NX_REFERENCE_GATEWAY uint32_t
nx_reference_gpio_batch(const nx_stm32u575_secure_batch_message_t* message,
                        nx_stm32u575_secure_batch_reply_t* reply);
#endif /* NX_U575_REFERENCE_GATEWAY_H */
