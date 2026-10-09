#include "hal/runtime/nx_deadline.h"
#include "osal/osal.h"
static nx_status_t status(osal_status_t value) {
    switch (value) {
        case OSAL_OK: return NX_OK;
        case OSAL_ERROR_INVALID_PARAM: return NX_ERR_INVALID_PARAM;
        case OSAL_ERROR_NULL_POINTER: return NX_ERR_NULL_PTR;
        case OSAL_ERROR_NO_MEMORY: return NX_ERR_NO_MEMORY;
        case OSAL_ERROR_TIMEOUT: return NX_ERR_TIMEOUT;
        case OSAL_ERROR_NOT_INIT: return NX_ERR_NOT_INIT;
        case OSAL_ERROR_BUSY: return NX_ERR_BUSY;
        case OSAL_ERROR_NOT_FOUND: return NX_ERR_NOT_FOUND;
        case OSAL_ERROR_FULL: return NX_ERR_FULL;
        case OSAL_ERROR_EMPTY: return NX_ERR_NO_DATA;
        case OSAL_ERROR_ISR: return NX_ERR_CONTEXT;
        case OSAL_ERROR_CANCELLED: return NX_ERR_CANCELLED;
        case OSAL_ERROR_NOT_SUPPORTED: return NX_ERR_NOT_SUPPORTED;
        default: return NX_ERR_IO;
    }
}
static nx_status_t now(void* context, uint32_t* out) {
    (void)context;
    return status(osal_get_time_ms(out));
}
static nx_status_t wait(void* context, uint32_t ms) {
    (void)context;
    return status(osal_task_delay(ms));
}
const nx_hal_wait_port_t* nx_hal_osal_wait_port(void) {
    static const nx_hal_wait_port_t port = {now, wait, NULL, 1};
    return &port;
}
