/* Observable portable-bit contract, shared by all production adapters. */
#ifndef OSAL_EVENT_MASK_CONTRACT_H
#define OSAL_EVENT_MASK_CONTRACT_H
#include "osal/osal.h"
#include <assert.h>
#include <stdint.h>

static void test_event_advertised_bits(void) {
    osal_backend_info_t info;
    assert(osal_get_backend_info(&info) == OSAL_OK);
    osal_event_handle_t event;
    assert(osal_event_create(&event) == OSAL_OK);
    const osal_event_wait_options_t options = {OSAL_EVENT_WAIT_ALL, false, 0};
    for (unsigned position = 0; position < 32; ++position) {
        const uint32_t bit = UINT32_C(1) << position;
        uint32_t observed = UINT32_MAX;
        if (info.event_bits_mask & bit) {
            assert(osal_event_set(event, bit) == OSAL_OK);
            assert(osal_event_wait(event, bit, &options, &observed) == OSAL_OK);
            assert(observed == bit && osal_event_get(event) == bit);
            assert(osal_event_clear(event, bit) == OSAL_OK);
            assert(osal_event_get(event) == 0);
            /* POSIX FreeRTOS exercises the actual deferred daemon entry in
             * task context; physical ISR priority is a separate HIL check. */
            assert(osal_event_set_from_isr(event, bit) == OSAL_OK);
            const osal_event_wait_options_t deferred = {OSAL_EVENT_WAIT_ALL, false,
                info.backend == OSAL_BACKEND_FREERTOS ? 1000u : 0u};
            assert(osal_event_wait(event, bit, &deferred, &observed) == OSAL_OK && observed == bit);
            assert(osal_event_clear_from_isr(event, bit) == OSAL_OK);
            if (info.backend == OSAL_BACKEND_FREERTOS) {
                for (unsigned retry = 0; retry < 100 && osal_event_get(event); ++retry)
                    assert(osal_task_delay(1) == OSAL_OK);
            }
            assert(osal_event_get(event) == 0);
        } else {
            assert(osal_event_set(event, bit) == OSAL_ERROR_INVALID_PARAM);
            assert(osal_event_clear(event, bit) == OSAL_ERROR_INVALID_PARAM);
            assert(osal_event_wait(event, bit, &options, &observed) == OSAL_ERROR_INVALID_PARAM);
            assert(observed == 0);
            assert(osal_event_set_from_isr(event, bit) == OSAL_ERROR_INVALID_PARAM);
            assert(osal_event_clear_from_isr(event, bit) == OSAL_ERROR_INVALID_PARAM);
            const osal_event_wait_options_t barrier = {OSAL_EVENT_WAIT_ALL, true, 0};
            assert(osal_event_sync(event, bit, bit, &barrier, &observed) == OSAL_ERROR_INVALID_PARAM);
            assert(osal_event_get(event) == 0);
        }
    }
    assert(osal_event_delete(event) == OSAL_OK);
}
#endif
