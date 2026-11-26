/**
 * \file            probe.c
 * \brief           Linked allocation model for explicit owner and caller state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/owner_observer.h"
#include "nexus/io/uart.h"

#if (COST_SLOTS != 1 && COST_SLOTS != 4) ||                                    \
    (COST_OBSERVER != 0 && COST_OBSERVER != 1)
#error "Select one reviewed storage allocation model"
#endif

_Static_assert(sizeof(void*) == 4U, "This allocation model requires ARM32");

nx_bus_owner_t cost_owner;
nx_owner_slot_t cost_slots[COST_SLOTS];
nx_uart_tx_request_t cost_requests[COST_SLOTS];
uint8_t cost_payloads[COST_SLOTS][32];
#if COST_OBSERVER
nx_owner_observer_t cost_observer;
#endif
volatile uintptr_t cost_roots[5];

/**
 * \brief           Keep every selected object in the actual linked image.
 * \note            No hardware, ownership operation or runtime observation is
 *                  executed. The constant anchor overhead remains explicit.
 */
void _start(void) {
    cost_roots[0] = (uintptr_t)&cost_owner;
    cost_roots[1] = (uintptr_t)&cost_slots;
    cost_roots[2] = (uintptr_t)&cost_requests;
    cost_roots[3] = (uintptr_t)&cost_payloads;
#if COST_OBSERVER
    cost_roots[4] = (uintptr_t)&cost_observer;
#endif
    for (;;) {
    }
}
