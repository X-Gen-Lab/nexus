/**
 * \file            nx_option_bytes_power.c
 * \brief           Option Bytes power management implementation for Native
 * platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements power management (enable, disable, is_enabled,
 *                  set_callback) for Option Bytes peripheral.
 */

#include "nx_option_bytes_helpers.h"
#include "nx_option_bytes_types.h"

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
} nx_option_bytes_power_ctx_t;

/* Static storage for power contexts */
static nx_option_bytes_power_ctx_t g_power_contexts[4];

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get power context from power interface
 */
static nx_option_bytes_power_ctx_t* get_power_context(nx_power_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, power));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL || state->index >= 4) {
        return NULL;
    }

    return &g_power_contexts[state->index];
}

/*---------------------------------------------------------------------------*/
/* Power Implementation                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable Option Bytes power/clock
 */
static nx_status_t option_bytes_power_enable(nx_power_t* self) {
    nx_option_bytes_power_ctx_t* ctx = get_power_context(self);
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
 * \brief           Disable Option Bytes power/clock
 */
static nx_status_t option_bytes_power_disable(nx_power_t* self) {
    nx_option_bytes_power_ctx_t* ctx = get_power_context(self);
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
 * \brief           Check if Option Bytes power is enabled
 */
static bool option_bytes_power_is_enabled(nx_power_t* self) {
    nx_option_bytes_power_ctx_t* ctx = get_power_context(self);
    if (ctx == NULL) {
        return false;
    }

    return ctx->enabled;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t option_bytes_power_set_callback(nx_power_t* self,
                                                   nx_power_callback_t callback,
                                                   void* user_data) {
    nx_option_bytes_power_ctx_t* ctx = get_power_context(self);
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
void option_bytes_init_power(nx_power_t* power) {
    if (power == NULL) {
        return;
    }

    power->enable = option_bytes_power_enable;
    power->disable = option_bytes_power_disable;
    power->is_enabled = option_bytes_power_is_enabled;
    power->set_callback = option_bytes_power_set_callback;
}
