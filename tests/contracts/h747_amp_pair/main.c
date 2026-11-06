/**
 * \file            main.c
 * \brief           Actual independent ARM consumer of the H747 AMP attachment
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/amp_ipc_storage.h"
#include "nexus/components/stm32h747_amp.h"
extern unsigned char __shared_begin[];
static nx_h747_amp_t port;
static nx_amp_ipc_t endpoint;
static nx_amp_local_slot_t slots[2];
/** \brief No persistent epoch authority is supplied by a link fixture. */
static nx_result_t unknown_epoch(void* context, uint32_t epoch) {
    (void)context;
    (void)epoch;
    return NX_ERROR_UNSUPPORTED;
}
/** \brief Link real port, LL hardware and common lane without a boot claim. */
int main(void) {
    nx_amp_wire_slot_t* wire = (nx_amp_wire_slot_t*)__shared_begin;
#ifdef CORE_CM7
    const nx_amp_role_t role = NX_AMP_SENDER;
    const uint8_t claim = 1U;
#else
    const nx_amp_role_t role = NX_AMP_RECEIVER;
    const uint8_t claim = 2U;
#endif
    const nx_h747_amp_config_t config = {__shared_begin, 4096U, wire, 2U,
                                         unknown_epoch,  NULL,  1U,   role,
                                         claim,          3U,    1U,   5U};
    nx_result_t result = nx_h747_amp_attach(&port, &config);
    if (result != NX_SUCCESS)
        return (int)result;
    const nx_amp_config_t lane = {NULL,
                                  NULL,
                                  wire,
                                  slots,
                                  2U,
                                  __shared_begin + 640U,
                                  128U,
                                  (nx_amp_word_t*)(__shared_begin + 512U),
                                  (nx_amp_word_t*)(__shared_begin + 576U),
                                  1U,
                                  1U,
                                  32U,
                                  role};
    result = nx_h747_amp_construct(&endpoint, &port, &lane);
    /* An unexpected live owner keeps these automatic configurations alive.
     * The fixture's rejecting authority must never create that owner. */
    if (result == NX_SUCCESS || nx_h747_amp_detach(&port) != NX_SUCCESS)
        for (;;) {
        }
    return (int)result;
}
