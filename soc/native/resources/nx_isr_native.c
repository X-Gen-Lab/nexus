/** Native ISR contract: one owner per IRQ, connect enables, disconnect drains.
 * Dispatch is a host simulation, not a hardware interrupt/priority model. */
#include "hal/resource/nx_isr_manager.h"
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
    lock();
    native_irq_t* entry=&entries[irq];
    nx_status_t r=entry->connected || entry->active ? NX_ERR_BUSY : NX_OK;
    if (r==NX_OK) {
        entry->function=function; entry->context=context;
        entry->priority=priority; entry->connected=true;
    }
    unlock();
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
