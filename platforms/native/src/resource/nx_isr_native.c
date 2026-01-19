/**
 * \file            nx_isr_native.c
 * \brief           Native platform ISR manager simulation
 * \author          Nexus Team
 */

#include "hal/resource/nx_isr_manager.h"
#include <string.h>

/* Maximum number of IRQs supported */
#define NX_ISR_MAX_IRQS 64

/* Maximum callbacks per IRQ */
#define NX_ISR_MAX_CALLBACKS_PER_IRQ 4

/**
 * \brief           ISR callback handle structure (internal)
 */
struct nx_isr_handle_s {
    uint32_t irq;                 /**< IRQ number */
    nx_isr_func_t func;           /**< Callback function */
    void* data;                   /**< User data */
    nx_isr_priority_t priority;   /**< Callback priority */
    bool active;                  /**< Active flag */
    struct nx_isr_handle_s* next; /**< Next callback in chain */
};

/**
 * \brief           ISR callback chain for one IRQ
 */
typedef struct {
    struct nx_isr_handle_s* head; /**< Head of callback chain */
    uint8_t count;                /**< Number of callbacks */
    bool enabled;                 /**< IRQ enabled flag */
} nx_isr_chain_t;

/**
 * \brief           ISR manager instance structure
 */
typedef struct {
    nx_isr_manager_t base;                  /**< Base interface */
    nx_isr_chain_t chains[NX_ISR_MAX_IRQS]; /**< Callback chains per IRQ */
    struct nx_isr_handle_s
        handle_pool[NX_ISR_MAX_IRQS *
                    NX_ISR_MAX_CALLBACKS_PER_IRQ]; /**< Handle pool */
} nx_isr_manager_impl_t;

/* Forward declarations */
static nx_isr_handle_t* isr_connect(nx_isr_manager_t* self, uint32_t irq,
                                    nx_isr_func_t func, void* data,
                                    nx_isr_priority_t priority);
static nx_status_t isr_disconnect(nx_isr_manager_t* self,
                                  nx_isr_handle_t* handle);
static nx_status_t isr_set_priority(nx_isr_manager_t* self, uint32_t irq,
                                    uint8_t priority);
static nx_status_t isr_enable(nx_isr_manager_t* self, uint32_t irq);
static nx_status_t isr_disable(nx_isr_manager_t* self, uint32_t irq);

/* Singleton instance */
static nx_isr_manager_impl_t g_isr_manager = {
    .base =
        {
            .connect = isr_connect,
            .disconnect = isr_disconnect,
            .set_priority = isr_set_priority,
            .enable = isr_enable,
            .disable = isr_disable,
        },
    .chains = {0},
    .handle_pool = {0},
};

/**
 * \brief           Allocate handle from pool
 */
static nx_isr_handle_t* alloc_handle(void) {
    for (size_t i = 0; i < sizeof(g_isr_manager.handle_pool) /
                               sizeof(g_isr_manager.handle_pool[0]);
         i++) {
        if (!g_isr_manager.handle_pool[i].active) {
            memset(&g_isr_manager.handle_pool[i], 0,
                   sizeof(struct nx_isr_handle_s));
            g_isr_manager.handle_pool[i].active = true;
            return &g_isr_manager.handle_pool[i];
        }
    }
    return NULL;
}

/**
 * \brief           Free handle back to pool
 */
static void free_handle(nx_isr_handle_t* handle) {
    if (handle) {
        handle->active = false;
    }
}

/**
 * \brief           Insert callback into chain sorted by priority
 */
static void insert_sorted(nx_isr_chain_t* chain, nx_isr_handle_t* handle) {
    if (!chain->head || handle->priority < chain->head->priority) {
        /* Insert at head */
        handle->next = chain->head;
        chain->head = handle;
    } else {
        /* Find insertion point */
        nx_isr_handle_t* curr = chain->head;
        while (curr->next && curr->next->priority <= handle->priority) {
            curr = curr->next;
        }
        handle->next = curr->next;
        curr->next = handle;
    }
    chain->count++;
}

/**
 * \brief           Remove callback from chain
 */
static bool remove_from_chain(nx_isr_chain_t* chain, nx_isr_handle_t* handle) {
    if (!chain->head) {
        return false;
    }

    if (chain->head == handle) {
        /* Remove from head */
        chain->head = handle->next;
        chain->count--;
        return true;
    }

    /* Find and remove */
    nx_isr_handle_t* curr = chain->head;
    while (curr->next) {
        if (curr->next == handle) {
            curr->next = handle->next;
            chain->count--;
            return true;
        }
        curr = curr->next;
    }

    return false;
}

/**
 * \brief           Connect ISR callback to interrupt
 */
static nx_isr_handle_t* isr_connect(nx_isr_manager_t* self, uint32_t irq,
                                    nx_isr_func_t func, void* data,
                                    nx_isr_priority_t priority) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    if (!impl || !func) {
        return NULL;
    }

    if (irq >= NX_ISR_MAX_IRQS) {
        return NULL;
    }

    if (impl->chains[irq].count >= NX_ISR_MAX_CALLBACKS_PER_IRQ) {
        return NULL;
    }

    /* Allocate handle */
    nx_isr_handle_t* handle = alloc_handle();
    if (!handle) {
        return NULL;
    }

    /* Initialize handle */
    handle->irq = irq;
    handle->func = func;
    handle->data = data;
    handle->priority = priority;
    handle->next = NULL;

    /* Insert into chain sorted by priority */
    insert_sorted(&impl->chains[irq], handle);

    return handle;
}

/**
 * \brief           Disconnect ISR callback
 */
static nx_status_t isr_disconnect(nx_isr_manager_t* self,
                                  nx_isr_handle_t* handle) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    if (!impl || !handle) {
        return NX_ERR_NULL_PTR;
    }

    if (!handle->active) {
        return NX_ERR_INVALID_PARAM;
    }

    uint32_t irq = handle->irq;
    if (irq >= NX_ISR_MAX_IRQS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Remove from chain */
    if (!remove_from_chain(&impl->chains[irq], handle)) {
        return NX_ERR_GENERIC;
    }

    /* Free handle */
    free_handle(handle);

    return NX_OK;
}

/**
 * \brief           Set interrupt priority (simulated)
 */
static nx_status_t isr_set_priority(nx_isr_manager_t* self, uint32_t irq,
                                    uint8_t priority) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    if (irq >= NX_ISR_MAX_IRQS) {
        return NX_ERR_INVALID_PARAM;
    }

    if (priority > 15) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Simulated - no actual hardware to configure */
    return NX_OK;
}

/**
 * \brief           Enable interrupt (simulated)
 */
static nx_status_t isr_enable(nx_isr_manager_t* self, uint32_t irq) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    if (irq >= NX_ISR_MAX_IRQS) {
        return NX_ERR_INVALID_PARAM;
    }

    impl->chains[irq].enabled = true;

    /* Simulated - no actual hardware to configure */
    return NX_OK;
}

/**
 * \brief           Disable interrupt (simulated)
 */
static nx_status_t isr_disable(nx_isr_manager_t* self, uint32_t irq) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    if (irq >= NX_ISR_MAX_IRQS) {
        return NX_ERR_INVALID_PARAM;
    }

    impl->chains[irq].enabled = false;

    /* Simulated - no actual hardware to configure */
    return NX_OK;
}

/**
 * \brief           Simulate ISR dispatch (for testing)
 * \note            This function can be called to simulate an interrupt
 */
void nx_isr_simulate(uint32_t irq) {
    if (irq >= NX_ISR_MAX_IRQS) {
        return;
    }

    nx_isr_chain_t* chain = &g_isr_manager.chains[irq];

    if (!chain->enabled) {
        return;
    }

    /* Call all callbacks in priority order */
    nx_isr_handle_t* curr = chain->head;
    while (curr) {
        if (curr->active && curr->func) {
            curr->func(curr->data);
        }
        curr = curr->next;
    }
}

/**
 * \brief           Get ISR manager singleton instance
 */
nx_isr_manager_t* nx_isr_manager_get(void) {
    return &g_isr_manager.base;
}
