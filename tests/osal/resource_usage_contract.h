#ifndef OSAL_RESOURCE_USAGE_CONTRACT_H
#define OSAL_RESOURCE_USAGE_CONTRACT_H
#include "osal/osal.h"
#include <assert.h>

static void test_resource_usage_pool(void) {
    osal_resource_usage_t before, full, after;
    assert(osal_get_resource_usage(NULL) == OSAL_ERROR_NULL_POINTER);
    assert(osal_get_resource_usage(&before) == OSAL_OK);
    assert(before.semaphores.capacity == OSAL_MAX_SEMS && !before.semaphores.reserved);
    assert(before.lifetime_tokens_remaining > OSAL_MAX_SEMS);
    osal_sem_handle_t handles[OSAL_MAX_SEMS], extra = NULL;
    for (unsigned i = 0; i < OSAL_MAX_SEMS; ++i)
        assert(osal_sem_create(0, 1, &handles[i]) == OSAL_OK);
    assert(osal_get_resource_usage(&full) == OSAL_OK);
    assert(full.semaphores.reserved == full.semaphores.capacity);
    assert(full.lifetime_tokens_issued == before.lifetime_tokens_issued + OSAL_MAX_SEMS);
    assert(osal_sem_create(0, 1, &extra) == OSAL_ERROR_NO_MEMORY && !extra);
    assert(osal_get_resource_usage(&after) == OSAL_OK);
    assert(after.lifetime_tokens_issued == full.lifetime_tokens_issued);
    for (unsigned i = 0; i < OSAL_MAX_SEMS; ++i)
        assert(osal_sem_delete(handles[i]) == OSAL_OK);
    assert(osal_get_resource_usage(&after) == OSAL_OK && !after.semaphores.reserved);
    assert(after.lifetime_tokens_issued == full.lifetime_tokens_issued);
    assert(osal_sem_create(0, 1, &extra) == OSAL_OK && extra != handles[0]);
    assert(osal_sem_delete(handles[0]) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_sem_delete(extra) == OSAL_OK);
    assert(osal_reset_stats() == OSAL_OK);
    assert(osal_get_resource_usage(&after) == OSAL_OK);
    assert(after.lifetime_tokens_issued == full.lifetime_tokens_issued + 1);
    assert(after.lifetime_tokens_capacity ==
           after.lifetime_tokens_issued + after.lifetime_tokens_remaining);
}
#endif
