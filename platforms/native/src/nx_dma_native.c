/**
 * \file            nx_dma_native.c
 * \brief           Native platform DMA manager simulation
 * \author          Nexus Team
 */

#include "hal/resource/nx_dma_manager.h"
#include <stdlib.h>
#include <string.h>

/* Maximum number of simulated DMA channels */
#define NX_DMA_MAX_CHANNELS 8

/**
 * \brief           DMA channel state enumeration
 */
typedef enum {
    NX_DMA_CH_STATE_FREE = 0,
    NX_DMA_CH_STATE_ALLOCATED,
    NX_DMA_CH_STATE_BUSY,
} nx_dma_ch_state_t;

/**
 * \brief           DMA channel structure (internal)
 */
struct nx_dma_channel_s {
    uint8_t index;              /**< Channel index */
    nx_dma_ch_state_t state;    /**< Channel state */
    uint32_t periph;            /**< Associated peripheral */
    nx_dma_callback_t callback; /**< Completion callback */
    void* user_data;            /**< User data for callback */
    uint32_t remaining;         /**< Remaining transfer count */
};

/**
 * \brief           DMA manager instance structure
 */
typedef struct {
    nx_dma_manager_t base;                          /**< Base interface */
    nx_dma_channel_t channels[NX_DMA_MAX_CHANNELS]; /**< Channel pool */
} nx_dma_manager_impl_t;

/* Forward declarations */
static nx_dma_channel_t* dma_alloc(nx_dma_manager_t* self, uint32_t periph);
static nx_status_t dma_free(nx_dma_manager_t* self, nx_dma_channel_t* ch);
static nx_status_t dma_start(nx_dma_channel_t* ch, const nx_dma_request_t* req);
static nx_status_t dma_stop(nx_dma_channel_t* ch);
static uint32_t dma_get_remaining(nx_dma_channel_t* ch);

/* Singleton instance */
static nx_dma_manager_impl_t g_dma_manager = {
    .base =
        {
            .alloc = dma_alloc,
            .free = dma_free,
            .start = dma_start,
            .stop = dma_stop,
            .get_remaining = dma_get_remaining,
        },
    .channels = {0},
};

/**
 * \brief           Allocate DMA channel for peripheral
 */
static nx_dma_channel_t* dma_alloc(nx_dma_manager_t* self, uint32_t periph) {
    nx_dma_manager_impl_t* impl = (nx_dma_manager_impl_t*)self;

    if (!impl) {
        return NULL;
    }

    /* Find free channel */
    for (uint8_t i = 0; i < NX_DMA_MAX_CHANNELS; i++) {
        if (impl->channels[i].state == NX_DMA_CH_STATE_FREE) {
            impl->channels[i].index = i;
            impl->channels[i].state = NX_DMA_CH_STATE_ALLOCATED;
            impl->channels[i].periph = periph;
            impl->channels[i].callback = NULL;
            impl->channels[i].user_data = NULL;
            impl->channels[i].remaining = 0;
            return &impl->channels[i];
        }
    }

    /* No free channel available */
    return NULL;
}

/**
 * \brief           Free DMA channel
 */
static nx_status_t dma_free(nx_dma_manager_t* self, nx_dma_channel_t* ch) {
    nx_dma_manager_impl_t* impl = (nx_dma_manager_impl_t*)self;

    if (!impl || !ch) {
        return NX_ERR_NULL_PTR;
    }

    /* Validate channel belongs to this manager */
    if (ch < impl->channels || ch >= impl->channels + NX_DMA_MAX_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Stop transfer if busy */
    if (ch->state == NX_DMA_CH_STATE_BUSY) {
        dma_stop(ch);
    }

    /* Mark as free */
    ch->state = NX_DMA_CH_STATE_FREE;
    ch->periph = 0;
    ch->callback = NULL;
    ch->user_data = NULL;
    ch->remaining = 0;

    return NX_OK;
}

/**
 * \brief           Start DMA transfer (simulated)
 */
static nx_status_t dma_start(nx_dma_channel_t* ch,
                             const nx_dma_request_t* req) {
    if (!ch || !req) {
        return NX_ERR_NULL_PTR;
    }

    if (ch->state != NX_DMA_CH_STATE_ALLOCATED) {
        return NX_ERR_INVALID_STATE;
    }

    /* Validate parameters */
    if (req->transfer_count == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (req->periph_width != 8 && req->periph_width != 16 &&
        req->periph_width != 32) {
        return NX_ERR_INVALID_PARAM;
    }

    if (req->memory_width != 8 && req->memory_width != 16 &&
        req->memory_width != 32) {
        return NX_ERR_INVALID_PARAM;
    }

    if (req->priority > 3) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Store callback */
    ch->callback = req->callback;
    ch->user_data = req->user_data;
    ch->remaining = req->transfer_count;

    /* Mark as busy */
    ch->state = NX_DMA_CH_STATE_BUSY;

    /* Simulate immediate transfer completion */
    if (!req->circular) {
        ch->remaining = 0;
        ch->state = NX_DMA_CH_STATE_ALLOCATED;

        /* Call completion callback */
        if (ch->callback) {
            ch->callback(ch->user_data, NX_OK);
        }
    }

    return NX_OK;
}

/**
 * \brief           Stop DMA transfer
 */
static nx_status_t dma_stop(nx_dma_channel_t* ch) {
    if (!ch) {
        return NX_ERR_NULL_PTR;
    }

    if (ch->state != NX_DMA_CH_STATE_BUSY) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as allocated (not busy) */
    ch->state = NX_DMA_CH_STATE_ALLOCATED;
    ch->remaining = 0;

    return NX_OK;
}

/**
 * \brief           Get remaining transfer count
 */
static uint32_t dma_get_remaining(nx_dma_channel_t* ch) {
    if (!ch) {
        return 0;
    }

    if (ch->state != NX_DMA_CH_STATE_BUSY) {
        return 0;
    }

    return ch->remaining;
}

/**
 * \brief           Get DMA manager singleton instance
 */
nx_dma_manager_t* nx_dma_manager_get(void) {
    return &g_dma_manager.base;
}
