/* Production adapters compiled with a small, real identity budget. No private
 * counter injection, wraparound or resetting lifetime identities. */
#include "osal/osal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#if OSAL_LIFETIME_TOKEN_LIMIT != 6
#error "This test requires a six-identity adapter build"
#endif
int main(void) {
    assert(osal_init() == OSAL_OK);
    osal_resource_usage_t usage;
    assert(osal_get_resource_usage(&usage) == OSAL_OK);
    assert(usage.lifetime_tokens_capacity == 6 && !usage.lifetime_tokens_issued);
    osal_sem_handle_t sem, sem2, extra;
    osal_event_handle_t event, event2;
    osal_queue_handle_t queue;
    osal_mutex_handle_t mutex;
    assert(osal_sem_create(0, 1, &sem) == OSAL_OK);
    assert(osal_event_create(&event) == OSAL_OK);
    assert(osal_queue_create(1, 1, &queue) == OSAL_OK);
    assert(osal_mutex_create(&mutex) == OSAL_OK);
    assert(osal_sem_create(0, 1, &sem2) == OSAL_OK);
    assert(osal_event_create(&event2) == OSAL_OK);
    assert(osal_get_resource_usage(&usage) == OSAL_OK);
    assert(usage.semaphores.reserved == 2 && usage.events.reserved == 2 &&
           usage.queues.reserved == 1 && usage.mutexes.reserved == 1);
    assert(usage.lifetime_tokens_issued == 6 && !usage.lifetime_tokens_remaining);
    extra = (void*)(uintptr_t)1;
    assert(osal_sem_create(0, 1, &extra) == OSAL_ERROR_NO_MEMORY && !extra);
    assert(osal_sem_delete(sem) == OSAL_OK);
    assert(osal_event_delete(event) == OSAL_OK);
    assert(osal_queue_delete(queue) == OSAL_OK);
    assert(osal_mutex_delete(mutex) == OSAL_OK);
    assert(osal_sem_delete(sem2) == OSAL_OK);
    assert(osal_event_delete(event2) == OSAL_OK);
    assert(osal_get_resource_usage(&usage) == OSAL_OK);
    assert(!usage.semaphores.reserved && !usage.events.reserved &&
           !usage.queues.reserved && !usage.mutexes.reserved);
    assert(usage.lifetime_tokens_issued == 6 && !usage.lifetime_tokens_remaining);
    assert(osal_sem_delete(sem) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_reset_stats() == OSAL_OK);
    assert(osal_deinit() == OSAL_OK && osal_init() == OSAL_OK);
    assert(osal_get_resource_usage(&usage) == OSAL_OK && usage.lifetime_tokens_issued == 6);
    assert(osal_event_create(&event) == OSAL_ERROR_NO_MEMORY && !event);
    assert(osal_sem_create(0, 1, &extra) == OSAL_ERROR_NO_MEMORY && !extra);
    assert(osal_deinit() == OSAL_OK);
    puts("Six identities exhausted across four classes; freed slots and reinit cannot reuse old identities");
    return 0;
}
