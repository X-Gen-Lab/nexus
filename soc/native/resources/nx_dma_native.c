/**
 * \file            nx_dma_native.c
 * \brief           Native platform DMA manager simulation
 * \author          Nexus Team
 */

#include "hal/resource/nx_dma_manager.h"
#include <stdlib.h>
#include <string.h>

/* Maximum number of simulated DMA controllers */
#define NX_DMA_MAX_CONTROLLERS 2

/* Maximum number of channels per controller */
#define NX_DMA_MAX_CHANNELS_PER_CTRL 8

/**
 * \brief           DMA channel state enumeration
 */
typedef enum {
    NX_DMA_CH_STATE_FREE = 0,
    NX_DMA_CH_STATE_ALLOCATED,
    NX_DMA_CH_STATE_BUSY,
} nx_dma_ch_state_t;

/**
 * \brief           DMA channel implementation structure
 */
typedef struct {
    nx_dma_channel_t base;      /**< Base interface */
    uint8_t dma_index;          /**< DMA controller index */
    uint8_t channel_num;        /**< Channel number */
    nx_dma_ch_state_t state;    /**< Channel state */
    nx_dma_config_t config;     /**< Current configuration */
    nx_dma_callback_t callback; /**< Completion callback */
    void* user_data;            /**< User data for callback */
    size_t remaining;           /**< Remaining transfer count */
} nx_dma_channel_impl_t;

/* Forward declarations */
static nx_status_t dma_configure(nx_dma_channel_t* self,
                                 const nx_dma_config_t* cfg);
static nx_status_t dma_start(nx_dma_channel_t* self);
static nx_status_t dma_stop(nx_dma_channel_t* self);
static size_t dma_get_remaining(nx_dma_channel_t* self);
static nx_status_t dma_set_callback(nx_dma_channel_t* self,
                                    nx_dma_callback_t callback,
                                    void* user_data);

/* Channel pool for all DMA controllers */
static nx_dma_channel_impl_t g_dma_channels[NX_DMA_MAX_CONTROLLERS]
                                           [NX_DMA_MAX_CHANNELS_PER_CTRL] = {0};

/**
 * \brief           Configure DMA transfer parameters
 */
static nx_status_t dma_configure(nx_dma_channel_t* self,
                                 const nx_dma_config_t* cfg) {
    if (!self || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)self;

    if (impl->state == NX_DMA_CH_STATE_BUSY) {
        return NX_ERR_BUSY;
    }

    /* Validate configuration */
    if (cfg->size == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (cfg->data_width != 1 && cfg->data_width != 2 && cfg->data_width != 4) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Store configuration */
    impl->config = *cfg;

    return NX_OK;
}

/**
 * \brief           Start DMA transfer
 */
static nx_status_t dma_start(nx_dma_channel_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)self;

    if (impl->state != NX_DMA_CH_STATE_ALLOCATED) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as busy */
    impl->state = NX_DMA_CH_STATE_BUSY;
    impl->remaining = impl->config.size;

    /* Simulate immediate transfer completion for non-circular mode */
    if (!impl->config.circular) {
        impl->remaining = 0;
        impl->state = NX_DMA_CH_STATE_ALLOCATED;

        /* Call completion callback */
        if (impl->callback) {
            impl->callback(impl->user_data);
        }
    }

    return NX_OK;
}

/**
 * \brief           Stop DMA transfer
 */
static nx_status_t dma_stop(nx_dma_channel_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)self;

    if (impl->state != NX_DMA_CH_STATE_BUSY) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as allocated (not busy) */
    impl->state = NX_DMA_CH_STATE_ALLOCATED;
    impl->remaining = 0;

    return NX_OK;
}

/**
 * \brief           Get remaining transfer count
 */
static size_t dma_get_remaining(nx_dma_channel_t* self) {
    if (!self) {
        return 0;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)self;

    return impl->remaining;
}

/**
 * \brief           Set transfer complete callback
 */
static nx_status_t dma_set_callback(nx_dma_channel_t* self,
                                    nx_dma_callback_t callback,
                                    void* user_data) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)self;

    impl->callback = callback;
    impl->user_data = user_data;

    return NX_OK;
}

/**
 * \brief           Allocate a DMA channel
 */
nx_dma_channel_t* nx_dma_allocate_channel(uint8_t dma_index, uint8_t channel) {
    /* Validate parameters */
    if (dma_index >= NX_DMA_MAX_CONTROLLERS ||
        channel >= NX_DMA_MAX_CHANNELS_PER_CTRL) {
        return NULL;
    }

    nx_dma_channel_impl_t* impl = &g_dma_channels[dma_index][channel];

    /* Check if channel is already allocated */
    if (impl->state != NX_DMA_CH_STATE_FREE) {
        return NULL;
    }

    /* Initialize channel */
    impl->base.configure = dma_configure;
    impl->base.start = dma_start;
    impl->base.stop = dma_stop;
    impl->base.get_remaining = dma_get_remaining;
    impl->base.set_callback = dma_set_callback;

    impl->dma_index = dma_index;
    impl->channel_num = channel;
    impl->state = NX_DMA_CH_STATE_ALLOCATED;
    impl->callback = NULL;
    impl->user_data = NULL;
    impl->remaining = 0;
    memset(&impl->config, 0, sizeof(impl->config));

    return &impl->base;
}

/**
 * \brief           Release a DMA channel
 */
nx_status_t nx_dma_release_channel(nx_dma_channel_t* channel) {
    if (!channel) {
        return NX_ERR_NULL_PTR;
    }

    nx_dma_channel_impl_t* impl = (nx_dma_channel_impl_t*)channel;

    /* Validate channel belongs to our pool */
    bool valid = false;
    for (uint8_t i = 0; i < NX_DMA_MAX_CONTROLLERS; i++) {
        for (uint8_t j = 0; j < NX_DMA_MAX_CHANNELS_PER_CTRL; j++) {
            if (impl == &g_dma_channels[i][j]) {
                valid = true;
                break;
            }
        }
        if (valid) {
            break;
        }
    }

    if (!valid) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Stop transfer if busy */
    if (impl->state == NX_DMA_CH_STATE_BUSY) {
        dma_stop(channel);
    }

    /* Mark as free */
    impl->state = NX_DMA_CH_STATE_FREE;
    impl->callback = NULL;
    impl->user_data = NULL;
    impl->remaining = 0;
    memset(&impl->config, 0, sizeof(impl->config));

    return NX_OK;
}
