/** Real kernel with a test-only model of Cortex-M's pre-start BASEPRI sentinel.
 * Link wrapping leaves the pinned POSIX implementation intact. This is a mask
 * contract model, not execution of ARM instructions or target timing. */
#include "FreeRTOS.h"
#include "task.h"
#include "osal/osal.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <stdio.h>

static UBaseType_t model_mask;
static unsigned sentinel_depth = 0xaaaa;
static bool model_isr;
static unsigned isr_tick_reads;
void __real_vPortEnterCritical(void);
void __real_vPortExitCritical(void);
UBaseType_t __real_xPortSetInterruptMask(void);
void __real_vPortClearInterruptMask(UBaseType_t mask);
bool __real_nx_arch_in_isr(void);
TickType_t __real_xTaskGetTickCountFromISR(void);

bool __wrap_nx_arch_in_isr(void) {
    return model_isr || __real_nx_arch_in_isr();
}
TickType_t __wrap_xTaskGetTickCountFromISR(void) {
    ++isr_tick_reads;
    return __real_xTaskGetTickCountFromISR();
}

static bool boot(void) { return xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED; }
void __wrap_vPortEnterCritical(void) {
    if (boot()) { model_mask = 0x50; ++sentinel_depth; }
    __real_vPortEnterCritical();
}
void __wrap_vPortExitCritical(void) {
    if (boot()) {
        assert(sentinel_depth != 0);
        if (--sentinel_depth == 0) model_mask = 0;
    }
    __real_vPortExitCritical();
}
UBaseType_t __wrap_xPortSetInterruptMask(void) {
    if (!boot()) return __real_xPortSetInterruptMask();
    UBaseType_t previous = model_mask;
    model_mask = 0x50;
    return previous;
}
void __wrap_vPortClearInterruptMask(UBaseType_t mask) {
    if (boot()) model_mask = mask;
    else __real_vPortClearInterruptMask(mask);
}
static void unused(void* arg) { (void)arg; }
int main(void) {
    assert(osal_init() == OSAL_OK && model_mask == 0);
    uint32_t milliseconds = UINT32_MAX;
    assert(osal_get_time_ms(&milliseconds) == OSAL_OK && milliseconds == 0);
    assert(model_mask == 0 && !nx_arch_irq_is_masked());
    model_isr = true;
    milliseconds = UINT32_MAX;
    assert(osal_get_time_ms(&milliseconds) == OSAL_ERROR_NOT_INIT && milliseconds == 0);
    assert(isr_tick_reads == 0 && model_mask == 0 && !nx_arch_irq_is_masked());
    model_isr = false;
    osal_mutex_handle_t mutex;
    osal_sem_handle_t sem;
    osal_queue_handle_t queue;
    osal_event_handle_t event;
    assert(osal_mutex_create(&mutex) == OSAL_OK && model_mask == 0);
    assert(osal_sem_create(1, 1, &sem) == OSAL_OK && model_mask == 0);
    assert(osal_queue_create(4, 4, &queue) == OSAL_OK && model_mask == 0);
    assert(osal_event_create(&event) == OSAL_OK && model_mask == 0);
    assert(!osal_mutex_is_locked(mutex) && osal_mutex_get_owner(mutex) == NULL && model_mask == 0);
    assert(osal_sem_get_count(sem) == 1 && model_mask == 0);
    assert(osal_queue_get_count(queue) == 0 && osal_queue_get_available_space(queue) == 4 && model_mask == 0);
    assert(osal_event_get(event) == 0 && model_mask == 0);
    int item = 0;
    assert(osal_mutex_lock(mutex, 0) == OSAL_ERROR_NOT_INIT);
    assert(osal_sem_take(sem, 0) == OSAL_ERROR_NOT_INIT);
    assert(osal_sem_give_from_isr(sem) == OSAL_ERROR_NOT_INIT);
    assert(osal_queue_send(queue, &item, 0) == OSAL_ERROR_NOT_INIT);
    assert(osal_queue_receive_from_isr(queue, &item) == OSAL_ERROR_NOT_INIT);
    assert(osal_event_set_from_isr(event, 1) == OSAL_ERROR_NOT_INIT);
    assert(osal_task_delay(0) == OSAL_ERROR_NOT_INIT && model_mask == 0);
    assert(osal_mutex_delete(mutex) == OSAL_OK && model_mask == 0);
    assert(osal_sem_delete(sem) == OSAL_OK && model_mask == 0);
    assert(osal_queue_delete(queue) == OSAL_OK && model_mask == 0);
    assert(osal_event_delete(event) == OSAL_OK && model_mask == 0);

    unsigned char* memory = osal_mem_alloc(16);
    assert(memory && model_mask == 0);
    assert(osal_mem_check_integrity() == OSAL_OK && model_mask == 0);
    osal_mem_free(memory);
    assert(model_mask == 0 && osal_deinit() == OSAL_OK);

    model_mask = 0x50;
    nx_arch_irq_state_t irq = nx_arch_irq_save();
    milliseconds = UINT32_MAX;
    assert(osal_get_time_ms(&milliseconds) == OSAL_OK && milliseconds == 0);
    model_isr = true;
    milliseconds = UINT32_MAX;
    assert(osal_get_time_ms(&milliseconds) == OSAL_ERROR_NOT_INIT && milliseconds == 0);
    assert(isr_tick_reads == 0 && nx_arch_irq_is_masked() && model_mask == 0x50);
    model_isr = false;
    assert(osal_mutex_create(&mutex) == OSAL_OK && model_mask == 0x50);
    assert(nx_arch_irq_is_masked());
    assert(osal_mutex_delete(mutex) == OSAL_OK && model_mask == 0x50);
    nx_arch_irq_restore(irq);
    model_mask = 0;
    osal_timer_config_t timer_config = {.name = "boot", .period_ms = 1,
        .mode = OSAL_TIMER_ONE_SHOT, .callback = unused};
    osal_timer_handle_t timer;
    assert(osal_timer_create(&timer_config, &timer) == OSAL_OK && model_mask == 0);
    assert(!osal_timer_is_active(timer) && osal_timer_get_remaining(timer) == 0 && model_mask == 0);
    assert(osal_timer_get_period(timer) == 1);
    assert(osal_timer_start(timer) == OSAL_ERROR_NOT_INIT && model_mask == 0);
    osal_task_config_t task_config = {.name = "boot", .func = unused,
        .stack_size = 32768, .priority = 20};
    osal_task_handle_t task;
    assert(osal_task_create(&task_config, &task) == OSAL_OK && model_mask == 0);
    assert(osal_task_get_state(task) == OSAL_TASK_STATE_READY);
    assert(osal_task_get_name(task) != NULL && osal_task_get_priority(task) == 20 && model_mask == 0);
    assert(osal_task_get_stack_watermark(task) <= task_config.stack_size && model_mask == 0);
    assert(osal_task_get_current() == NULL);
    assert(osal_task_join(task, 0) == OSAL_ERROR_NOT_INIT);
    assert(osal_deinit() == OSAL_ERROR_BUSY);
    puts("Boot constructors preserve model BASEPRI/Arch state; operations require scheduler");
    return 0;
}
