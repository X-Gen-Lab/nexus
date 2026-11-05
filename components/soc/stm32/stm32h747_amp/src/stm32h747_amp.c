/**
 * \file            stm32h747_amp.c
 * \brief           Static H747 HSEM owner and coherent noncacheable AMP port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/stm32h747_amp.h"
#include "hardware.h"
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"

static const nx_amp_port_ops_t operations;

/** \brief Local metadata and MPU selectors are cold privileged Thread work. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
/** \brief Subtraction keeps the reserved end from wrapping. */
static bool inside(const nx_h747_amp_config_t* c, const void* address,
                   size_t bytes) {
    return address != NULL && bytes != 0U && bytes <= c->shared_bytes &&
           (uintptr_t)address >= (uintptr_t)c->shared &&
           (uintptr_t)address - (uintptr_t)c->shared <= c->shared_bytes - bytes;
}
/** \brief A refused infallible unclaim cannot falsely publish owner release. */
static _Noreturn void park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
nx_result_t nx_h747_amp_attach(nx_h747_amp_t* port,
                               const nx_h747_amp_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->shared == NULL ||
        config->shared_bytes < 64U || config->shared_bytes > 65536U ||
        (config->shared_bytes & (config->shared_bytes - 1U)) != 0U ||
        ((uintptr_t)config->shared & (config->shared_bytes - 1U)) != 0U ||
        (uintptr_t)config->shared > UINTPTR_MAX - config->shared_bytes ||
        config->admit_epoch == NULL || config->boot_epoch == 0U ||
        config->slots == 0U ||
        config->slots > SIZE_MAX / sizeof(nx_amp_wire_slot_t) ||
        (config->role != NX_AMP_SENDER && config->role != NX_AMP_RECEIVER) ||
        config->claim_semaphore > 31U || config->notify_semaphore > 31U ||
        config->claim_semaphore == config->notify_semaphore ||
        config->process_id == 0U || config->mpu_region > 15U ||
        (uintptr_t)config->wire % NX_AMP_IPC_UNIT != 0U ||
        !inside(config, config->wire,
                config->slots * sizeof(nx_amp_wire_slot_t)) ||
        overlap(port, sizeof(*port), config, sizeof(*config)) ||
        overlap(port, sizeof(*port), config->shared, config->shared_bytes) ||
        overlap(config, sizeof(*config), config->shared, config->shared_bytes))
        return NX_ERROR_INVALID;
    if (port->initialized || port->owner != NULL || port->notify_pending)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool ready = nx_h747_amp_hw_ready(config->shared, config->shared_bytes,
                                      config->mpu_region);
    nx_arch_irq_restore(saved);
    if (!ready)
        return NX_ERROR_STATE;
    port->config = config;
    port->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_h747_amp_detach(nx_h747_amp_t* port) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->owner != NULL || port->notify_pending || port->constructing)
        return NX_ERROR_BUSY;
    port->initialized = false;
    port->config = NULL;
    return NX_SUCCESS;
}
nx_result_t nx_h747_amp_construct(nx_amp_ipc_t* endpoint, nx_h747_amp_t* port,
                                  const nx_amp_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || !port->initialized || endpoint == NULL ||
        config == NULL)
        return NX_ERROR_INVALID;
    const nx_h747_amp_config_t* c = port->config;
    if (port->owner != NULL || port->notify_pending || port->constructing)
        return NX_ERROR_BUSY;
    if (config->ops != NULL || config->context != NULL ||
        config->wire != c->wire || config->capacity != c->slots ||
        config->role != c->role || config->local_epoch != c->boot_epoch ||
        config->granule != 32U ||
        (uintptr_t)config->local_boot % NX_AMP_IPC_UNIT != 0U ||
        (uintptr_t)config->peer_boot % NX_AMP_IPC_UNIT != 0U ||
        !inside(c, config->local_boot, sizeof(nx_amp_word_t)) ||
        !inside(c, config->peer_boot, sizeof(nx_amp_word_t)) ||
        ((uintptr_t)config->payload & 31U) != 0U ||
        (config->payload_bytes & 31U) != 0U ||
        !inside(c, config->payload, config->payload_bytes) ||
        overlap(endpoint, sizeof(*endpoint), port, sizeof(*port)) ||
        overlap(endpoint, sizeof(*endpoint), config, sizeof(*config)) ||
        overlap(endpoint, sizeof(*endpoint), c, sizeof(*c)) ||
        overlap(endpoint, sizeof(*endpoint), c->shared, c->shared_bytes) ||
        overlap(config->slots, config->capacity * sizeof(*config->slots),
                c->shared, c->shared_bytes) ||
        overlap(config->slots, config->capacity * sizeof(*config->slots), port,
                sizeof(*port)) ||
        overlap(config->slots, config->capacity * sizeof(*config->slots), c,
                sizeof(*c)) ||
        overlap(config->slots, config->capacity * sizeof(*config->slots),
                config, sizeof(*config)))
        return NX_ERROR_INVALID;
    nx_amp_config_t bound = *config;
    bound.ops = &operations;
    bound.context = port;
    port->constructing = true;
    nx_result_t result = nx_amp_ipc_construct(endpoint, &bound);
    port->constructing = false;
    return result;
}
/** \brief HSEM is acquired before committing persistent role identity. */
static nx_result_t claim(void* context, const void* owner,
                         const nx_amp_wire_slot_t* wire, nx_amp_role_t role,
                         uint32_t epoch) {
    nx_h747_amp_t* port = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || !port->initialized || owner == NULL)
        return NX_ERROR_INVALID;
    const nx_h747_amp_config_t* c = port->config;
    if (wire != c->wire || role != c->role || epoch != c->boot_epoch)
        return NX_ERROR_INVALID;
    if (!port->constructing)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (port->owner != NULL || port->notify_pending ||
        !nx_h747_amp_hw_take(c->claim_semaphore, c->process_id)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = c->admit_epoch(c->authority, epoch);
    if (result != NX_SUCCESS) {
        if (!nx_h747_amp_hw_release(c->claim_semaphore, c->process_id))
            park();
    } else {
        port->owner = owner;
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Final join may release local HSEM claims, never peer resources. */
static void unclaim(void* context, const void* owner) {
    nx_h747_amp_t* port = context;
    if (port == NULL || !port->initialized || port->owner != owner)
        return;
    if (!task())
        park();
    const nx_h747_amp_config_t* c = port->config;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (port->notify_pending) {
        if (!nx_h747_amp_hw_release(c->notify_semaphore, c->process_id))
            park();
        port->notify_pending = false;
    }
    if (!nx_h747_amp_hw_release(c->claim_semaphore, c->process_id))
        park();
    port->owner = NULL;
    nx_arch_irq_restore(saved);
}
/** \brief This exact mode has no dirty cache lines in its admitted window. */
static nx_result_t coherent(void* context, const void* address, size_t bytes) {
    const nx_h747_amp_t* port = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || !port->initialized ||
        ((uintptr_t)address & 31U) != 0U || (bytes & 31U) != 0U ||
        !inside(port->config, address, bytes))
        return NX_ERROR_INVALID;
    nx_arch_dsb();
    return NX_SUCCESS;
}
/** \brief Natural aligned32-bit Normal-memory loads are single-copy atomic. */
static uint32_t acquire(void* context, const uint32_t* word) {
    const nx_h747_amp_t* port = context;
    if (port == NULL || !port->initialized ||
        (!port->constructing && port->owner == NULL) ||
        ((uintptr_t)word & 3U) != 0U ||
        !inside(port->config, word, sizeof(*word)))
        return 0U;
    uint32_t value = *(volatile const uint32_t*)word;
    nx_arch_dmb();
    return value;
}
/** \brief Complete descriptor/payload stores before coherent publication. */
static void release(void* context, uint32_t* word, uint32_t value) {
    const nx_h747_amp_t* port = context;
    if (port == NULL || !port->initialized || port->owner == NULL ||
        ((uintptr_t)word & 3U) != 0U ||
        !inside(port->config, word, sizeof(*word)))
        park();
    nx_arch_dmb();
    *(volatile uint32_t*)word = value;
    nx_arch_dsb();
}
/** \brief New epochs, HSEM releases and timeouts cannot prove DMA/peer stop. */
static nx_result_t stopped(void* context, uint32_t epoch) {
    (void)context;
    (void)epoch;
    return NX_ERROR_UNSUPPORTED;
}
static const nx_amp_port_ops_t operations = {
    claim, unclaim, coherent, coherent, acquire, release, stopped};

nx_result_t nx_h747_amp_notify(nx_h747_amp_t* port) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (port == NULL || !port->initialized || port->owner == NULL)
        return NX_ERROR_STATE;
    const nx_h747_amp_config_t* c = port->config;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->notify_pending) {
        if (!nx_h747_amp_hw_take(c->notify_semaphore, c->process_id)) {
            nx_arch_irq_restore(saved);
            return NX_ERROR_BUSY;
        }
        port->notify_pending = true;
    }
    bool released = nx_h747_amp_hw_release(c->notify_semaphore, c->process_id);
    if (released)
        port->notify_pending = false;
    nx_arch_irq_restore(saved);
    return released ? NX_SUCCESS : NX_ERROR_IO;
}
