/** Native ISR contract: one owner per IRQ, connect enables, disconnect drains.
 * Dispatch is a host simulation, not a hardware interrupt/priority model. */
#include "hal/resource/nx_isr_manager.h"
#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "native_platform.h"
#include <stdatomic.h>

#define NX_ISR_MAX_IRQS 64U
typedef struct {
    nx_isr_func_t function;
    void* context;
    uint8_t priority;
    bool connected;
    unsigned active;
} native_irq_t;
static native_irq_t entries[NX_ISR_MAX_IRQS];
static atomic_flag guard = ATOMIC_FLAG_INIT;
static void lock(void) { while (atomic_flag_test_and_set_explicit(&guard,memory_order_acquire)) {} }
static void unlock(void) { atomic_flag_clear_explicit(&guard,memory_order_release); }
static nx_status_t connect_irq(nx_isr_manager_t* self, uint32_t irq,
                                nx_isr_func_t function, void* context, uint8_t priority);
static nx_status_t disconnect_irq(nx_isr_manager_t* self, uint32_t irq);
static nx_isr_manager_t manager={.connect=connect_irq,.disconnect=disconnect_irq};
static nx_status_t connect_irq(nx_isr_manager_t* self, uint32_t irq,
                                nx_isr_func_t function, void* context, uint8_t priority) {
    if (!self || !function) return NX_ERR_NULL_PTR;
    if (self != &manager || irq >= NX_ISR_MAX_IRQS || priority > 15) return NX_ERR_INVALID_PARAM;
    nx_arch_irq_state_t saved=nx_arch_irq_save();
    lock();
    native_irq_t* entry=&entries[irq];
    nx_status_t r=nx_device_shutdown_is_active() || entry->connected || entry->active ? NX_ERR_BUSY : NX_OK;
    if (r==NX_OK) {
        entry->function=function; entry->context=context;
        entry->priority=priority; entry->connected=true;
    }
    unlock();
    nx_arch_irq_restore(saved);
    return r;
}
static nx_status_t disconnect_irq(nx_isr_manager_t* self, uint32_t irq) {
    if (!self) return NX_ERR_NULL_PTR;
    if (self != &manager || irq >= NX_ISR_MAX_IRQS) return NX_ERR_INVALID_PARAM;
    lock();
    native_irq_t* entry=&entries[irq];
    nx_status_t r=!entry->connected ? NX_ERR_NOT_FOUND : entry->active ? NX_ERR_BUSY : NX_OK;
    if (r==NX_OK) { entry->connected=false; entry->function=NULL; entry->context=NULL; }
    unlock();
    /* Success means no callback still holds context. BUSY requires retry. */
    return r;
}
void nx_isr_simulate(uint32_t irq) {
    if (irq>=NX_ISR_MAX_IRQS) return;
    lock();
    native_irq_t* entry=&entries[irq];
    if (!entry->connected || !entry->function || entry->active) { unlock(); return; }
    ++entry->active;
    nx_isr_func_t function=entry->function;
    void* context=entry->context;
    unlock();
    function(context);
    lock(); --entry->active; unlock();
}
nx_isr_manager_t* nx_isr_manager_get(void) { return &manager; }

/** HAL holds the registry admission fence while it checks all resources. */
nx_status_t nx_native_resources_idle(void) {
    lock();
    nx_status_t r=NX_OK;
    for (unsigned i=0;i<NX_ISR_MAX_IRQS;++i)
        if (entries[i].connected || entries[i].active) r=NX_ERR_BUSY;
    unlock();
    return r==NX_OK ? nx_native_dma_idle() : r;
}
