/** Bounded host DMA ownership model. No hardware/DMA timing is simulated. */
#include "hal/resource/nx_dma_manager.h"
#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "native_platform.h"
#include <string.h>

#define NX_DMA_MAX_CONTROLLERS       2u
#define NX_DMA_MAX_CHANNELS_PER_CTRL 8u
typedef enum { FREE, ALLOCATED, BUSY } channel_state_t;
typedef struct {
    nx_dma_channel_t base;
    channel_state_t state;
    nx_dma_config_t config;
    nx_dma_callback_t callback;
    void* context;
    size_t remaining;
    unsigned callbacks;
} channel_t;
static channel_t channels[NX_DMA_MAX_CONTROLLERS][NX_DMA_MAX_CHANNELS_PER_CTRL];
static channel_t* resolve(nx_dma_channel_t* self) {
    for (unsigned i = 0; i < NX_DMA_MAX_CONTROLLERS; ++i)
        for (unsigned j = 0; j < NX_DMA_MAX_CHANNELS_PER_CTRL; ++j)
            if (self == &channels[i][j].base)
                return &channels[i][j];
    return NULL;
}
static nx_status_t configure(nx_dma_channel_t* self,
                             const nx_dma_config_t* cfg) {
    if (!self || !cfg)
        return NX_ERR_NULL_PTR;
    if (!cfg->size ||
        (cfg->data_width != 1 && cfg->data_width != 2 && cfg->data_width != 4))
        return NX_ERR_INVALID_PARAM;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    nx_status_t r = !c                                 ? NX_ERR_INVALID_PARAM
                    : c->state == FREE                 ? NX_ERR_INVALID_STATE
                    : c->state == BUSY || c->callbacks ? NX_ERR_BUSY
                                                       : NX_OK;
    if (r == NX_OK)
        c->config = *cfg;
    nx_arch_irq_restore(saved);
    return r;
}
static nx_status_t start(nx_dma_channel_t* self) {
    if (!self)
        return NX_ERR_NULL_PTR;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    nx_status_t r = !c                      ? NX_ERR_INVALID_PARAM
                    : c->state != ALLOCATED ? NX_ERR_INVALID_STATE
                    : c->callbacks          ? NX_ERR_BUSY
                    : !c->config.size       ? NX_ERR_INVALID_PARAM
                                            : NX_OK;
    nx_dma_callback_t callback = NULL;
    void* context = NULL;
    if (r == NX_OK) {
        c->state = BUSY;
        c->remaining = c->config.size;
        if (!c->config.circular) {
            c->state = ALLOCATED;
            c->remaining = 0;
            callback = c->callback;
            context = c->context;
            if (callback)
                ++c->callbacks;
        }
    }
    nx_arch_irq_restore(saved);
    if (callback) {
        callback(context);
        saved = nx_arch_irq_save();
        --c->callbacks;
        nx_arch_irq_restore(saved);
    }
    return r;
}
static nx_status_t stop(nx_dma_channel_t* self) {
    if (!self)
        return NX_ERR_NULL_PTR;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    nx_status_t r = !c                 ? NX_ERR_INVALID_PARAM
                    : c->callbacks     ? NX_ERR_BUSY
                    : c->state != BUSY ? NX_ERR_INVALID_STATE
                                       : NX_OK;
    if (r == NX_OK) {
        c->state = ALLOCATED;
        c->remaining = 0;
    }
    nx_arch_irq_restore(saved);
    return r;
}
static size_t remaining(nx_dma_channel_t* self) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    size_t value = c && c->state != FREE ? c->remaining : 0;
    nx_arch_irq_restore(saved);
    return value;
}
static nx_status_t set_callback(nx_dma_channel_t* self,
                                nx_dma_callback_t callback, void* context) {
    if (!self)
        return NX_ERR_NULL_PTR;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    nx_status_t r = !c                                 ? NX_ERR_INVALID_PARAM
                    : c->state == FREE                 ? NX_ERR_INVALID_STATE
                    : c->state == BUSY || c->callbacks ? NX_ERR_BUSY
                                                       : NX_OK;
    if (r == NX_OK) {
        c->callback = callback;
        c->context = context;
    }
    nx_arch_irq_restore(saved);
    return r;
}
nx_dma_channel_t* nx_dma_allocate_channel(uint8_t controller, uint8_t channel) {
    if (controller >= NX_DMA_MAX_CONTROLLERS ||
        channel >= NX_DMA_MAX_CHANNELS_PER_CTRL)
        return NULL;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = &channels[controller][channel];
    if (nx_device_shutdown_is_active() || c->state != FREE) {
        nx_arch_irq_restore(saved);
        return NULL;
    }
    memset(c, 0, sizeof(*c));
    c->base = (nx_dma_channel_t){.configure = configure,
                                 .start = start,
                                 .stop = stop,
                                 .get_remaining = remaining,
                                 .set_callback = set_callback};
    c->state = ALLOCATED;
    nx_arch_irq_restore(saved);
    return &c->base;
}
nx_status_t nx_dma_release_channel(nx_dma_channel_t* self) {
    if (!self)
        return NX_ERR_NULL_PTR;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    channel_t* c = resolve(self);
    nx_status_t r = !c                 ? NX_ERR_INVALID_PARAM
                    : c->state == FREE ? NX_ERR_INVALID_STATE
                    : c->callbacks     ? NX_ERR_BUSY
                                       : NX_OK;
    if (r == NX_OK) {
        c->state = FREE;
        c->callback = NULL;
        c->context = NULL;
        c->remaining = 0;
        memset(&c->config, 0, sizeof(c->config));
    }
    nx_arch_irq_restore(saved);
    return r;
}
nx_status_t nx_native_dma_idle(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_status_t r = NX_OK;
    for (unsigned i = 0; i < NX_DMA_MAX_CONTROLLERS; ++i)
        for (unsigned j = 0; j < NX_DMA_MAX_CHANNELS_PER_CTRL; ++j)
            if (channels[i][j].state != FREE || channels[i][j].callbacks)
                r = NX_ERR_BUSY;
    nx_arch_irq_restore(saved);
    return r;
}
