/**
 * \file            nx_usb_power.c
 * \brief           USB power management implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements power management (enable, disable, is_enabled,
 *                  set_callback) for USB peripheral.
 */

#include "nx_usb_helpers.h"
#include "nx_usb_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Power Context Structure                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Power management context
 */
typedef struct {
    bool enabled;                 /**< Power enabled flag */
    nx_power_callback_t callback; /**< Power state callback */
    void* user_data;              /**< User data for callback */
} nx_usb_power_ctx_t;

/* Static storage for power contexts */
static nx_usb_power_ctx_t g_power_contexts[2];

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get power context from power interface
 */
static nx_usb_power_ctx_t* get_power_context(nx_power_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, power));
    nx_usb_state_t* state = impl->state;

    if (state == NULL || state->index >= 2) {
        return NULL;
    }

    return &g_power_contexts[state->index];
}

/*---------------------------------------------------------------------------*/
/* Power Implementation                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable USB power/clock
 */
static nx_status_t usb_power_enable(nx_power_t* self) {
    nx_usb_power_ctx_t* ctx = get_power_context(self);
    if (ctx == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (ctx->enabled) {
        return NX_OK; /* Already enabled */
    }

    /* Enable power */
    ctx->enabled = true;

    /* Invoke callback if registered */
    if (ctx->callback != NULL) {
        ctx->callback(ctx->user_data, true);
    }

    return NX_OK;
}

/**
 * \brief           Disable USB power/clock
 */
static nx_status_t usb_power_disable(nx_power_t* self) {
    nx_usb_power_ctx_t* ctx = get_power_context(self);
    if (ctx == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!ctx->enabled) {
        return NX_OK; /* Already disabled */
    }

    /* Disable power */
    ctx->enabled = false;

    /* Invoke callback if registered */
    if (ctx->callback != NULL) {
        ctx->callback(ctx->user_data, false);
    }

    return NX_OK;
}

/**
 * \brief           Check if USB power is enabled
 */
static bool usb_power_is_enabled(nx_power_t* self) {
    nx_usb_power_ctx_t* ctx = get_power_context(self);
    if (ctx == NULL) {
        return false;
    }

    return ctx->enabled;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t usb_power_set_callback(nx_power_t* self,
                                          nx_power_callback_t callback,
                                          void* user_data) {
    nx_usb_power_ctx_t* ctx = get_power_context(self);
    if (ctx == NULL) {
        return NX_ERR_NULL_PTR;
    }

    ctx->callback = callback;
    ctx->user_data = user_data;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Power Interface Initialization                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void usb_init_power(nx_power_t* power) {
    if (power == NULL) {
        return;
    }

    power->enable = usb_power_enable;
    power->disable = usb_power_disable;
    power->is_enabled = usb_power_is_enabled;
    power->set_callback = usb_power_set_callback;
}

/**
 * \brief           Reset power context for testing
 */
void usb_reset_power_context(uint8_t index) {
    if (index < 2) {
        memset(&g_power_contexts[index], 0, sizeof(nx_usb_power_ctx_t));
    }
}
