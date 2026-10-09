#include "hal/runtime/nx_completion.h"

static unsigned calls;
static void completed(void* context, nx_hal_completion_ticket_t ticket,
                      const nx_hal_completion_result_t* result) {
    (void)context;
    if (ticket.sequence && result->settled && result->status == NX_OK) ++calls;
}

int main(void) {
    nx_hal_completion_queue_t queue = NX_HAL_COMPLETION_QUEUE_INITIALIZER;
    nx_hal_completion_slot_t slots[1];
    uint32_t entries[1], dispatched = 0;
    nx_hal_completion_ticket_t ticket;
    if (nx_hal_completion_init(&queue, slots, 1, entries, 1) != NX_OK) return 1;
    if (nx_hal_completion_arm(&queue, completed, NULL, &ticket) != NX_OK) return 2;
    nx_hal_completion_result_t terminal = {NX_OK, true};
    if (nx_hal_completion_post(&queue, ticket, terminal) != NX_OK) return 3;
    if (calls != 0) return 4;
    if (nx_hal_completion_dispatch(&queue, 1, &dispatched) != NX_OK ||
        dispatched != 1 || calls != 1) return 5;
    if (nx_hal_completion_deinit(&queue) != NX_OK) return 6;
    return 0;
}
