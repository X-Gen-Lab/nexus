#include "osal/osal.h"
#include "osal/osal_baremetal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* A board model supplies interrupt masks and a real, independently advancing
 * tick. Hardware timing and IRQ instruction validation remain HIL work. */
static bool in_isr, irq_masked, saved_mask;
static unsigned enters, exits;
static uint32_t clock_ms;
static bool inject_token;
static osal_sem_handle_t injected_sem;
void osal_platform_enter_critical(void) { ++enters; saved_mask = irq_masked; irq_masked = true; }
void osal_platform_exit_critical(void) { ++exits; irq_masked = saved_mask; }
bool osal_platform_is_isr(void) { return in_isr; }
void osal_platform_delay_us(uint32_t us) {
    (void)us;
    ++clock_ms;
    if (inject_token) {
        inject_token = false;
        in_isr = true;
        assert(osal_sem_give(injected_sem) == OSAL_ERROR_ISR);
        assert(osal_sem_give_from_isr(injected_sem) == OSAL_OK);
        in_isr = false;
    }
}
static uint32_t board_clock(void) { return clock_ms; }
static uint32_t other_clock(void) { return 0; }
static void dummy_task(void* arg) { (void)arg; }
int main(void) {
    assert(osal_init() == OSAL_OK);
    irq_masked = true;
    enters = exits = 0;
    osal_enter_critical(); osal_enter_critical();
    assert(enters == 1 && exits == 0);
    osal_exit_critical(); assert(irq_masked && exits == 0);
    osal_exit_critical(); assert(irq_masked && exits == 1);
    irq_masked = false;
    puts("Baremetal nested region preserves initial interrupt state passed");
    osal_sem_handle_t sem;
    assert(osal_sem_create(0, 1, &sem) == OSAL_OK);
    assert(osal_sem_take(sem, 3) == OSAL_ERROR_NOT_SUPPORTED);
    assert(osal_sem_take(sem, 0) == OSAL_ERROR_TIMEOUT);
    assert(osal_baremetal_set_clock(board_clock) == OSAL_OK);
    assert(osal_baremetal_set_clock(other_clock) == OSAL_ERROR_BUSY);
    clock_ms = UINT32_MAX - 1;
    assert(osal_sem_take(sem, 3) == OSAL_ERROR_TIMEOUT);
    assert(clock_ms == 1);
    puts("Baremetal rejects missing clock and handles tick wrap passed");
    injected_sem = sem;
    inject_token = true;
    assert(osal_sem_take(sem, 5) == OSAL_OK);
    assert(osal_sem_delete(sem) == OSAL_OK);
    for (unsigned i = 0; i < 1000; ++i) {
        osal_sem_handle_t old, next;
        assert(osal_sem_create(0, 1, &old) == OSAL_OK);
        assert(osal_sem_delete(old) == OSAL_OK);
        assert(osal_sem_create(1, 1, &next) == OSAL_OK && old != next);
        assert(osal_sem_take(old, 0) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_take((void*)(uintptr_t)1, 0) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_give(next) == OSAL_ERROR_FULL);
        assert(osal_sem_delete(next) == OSAL_OK);
    }
    puts("Baremetal ISR boundary, capacity and generation passed");
    osal_mutex_handle_t mutex;
    assert(osal_mutex_create(&mutex) == OSAL_OK);
    assert(osal_mutex_lock(mutex, 0) == OSAL_OK);
    assert(osal_mutex_lock(mutex, 0) == OSAL_OK);
    assert(osal_mutex_delete(mutex) == OSAL_ERROR_BUSY);
    assert(osal_mutex_unlock(mutex) == OSAL_OK && osal_mutex_is_locked(mutex));
    assert(osal_mutex_unlock(mutex) == OSAL_OK && !osal_mutex_is_locked(mutex));
    assert(osal_mutex_delete(mutex) == OSAL_OK);
    puts("Baremetal recursive mutex lifetime passed");
    osal_queue_handle_t queue;
    assert(osal_queue_create(SIZE_MAX / 2 + 1, 2, &queue) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_queue_create(sizeof(int), 2, &queue) == OSAL_OK);
    int value = 1;
    assert(osal_queue_send(queue, &value, 0) == OSAL_OK);
    value = 2; assert(osal_queue_send(queue, &value, 0) == OSAL_OK);
    value = 3; assert(osal_queue_send(queue, &value, 0) == OSAL_ERROR_FULL);
    assert(osal_queue_set_mode(queue, OSAL_QUEUE_MODE_OVERWRITE) == OSAL_OK);
    assert(osal_queue_send(queue, &value, 0) == OSAL_OK);
    assert(osal_queue_receive(queue, &value, 0) == OSAL_OK && value == 2);
    assert(osal_queue_receive(queue, &value, 0) == OSAL_OK && value == 3);
    in_isr = true;
    assert(osal_queue_receive(queue, &value, 0) == OSAL_ERROR_ISR);
    assert(osal_queue_receive_from_isr(queue, &value) == OSAL_ERROR_EMPTY);
    in_isr = false;
    assert(osal_queue_delete(queue) == OSAL_OK);
    puts("Baremetal queue bounds, overwrite and ISR polling passed");
    osal_event_handle_t event;
    assert(osal_event_create(&event) == OSAL_OK);
    assert(osal_event_set_from_isr(event, 3) == OSAL_OK);
    osal_event_wait_options_t options = {OSAL_EVENT_WAIT_ALL, true, 0};
    uint32_t bits;
    assert(osal_event_wait(event, 3, &options, &bits) == OSAL_OK && bits == 3);
    assert(osal_event_get(event) == 0);
    assert(osal_event_delete(event) == OSAL_OK);
    puts("Baremetal event consumption passed");
    osal_task_config_t cfg = {.name="task",.func=dummy_task,.arg=NULL,.stack_size=1024,.priority=1};
    osal_task_handle_t task;
    assert(osal_task_create(&cfg, &task) == OSAL_ERROR_NOT_SUPPORTED && task == NULL);
    assert(osal_task_join(task, 0) == OSAL_ERROR_NOT_SUPPORTED);
    assert(osal_mem_alloc(1) == NULL);
    assert(osal_timer_start((void*)(uintptr_t)1) == OSAL_ERROR_NOT_SUPPORTED);
    assert(!osal_is_running());
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK);
    assert(!stats.task_count && !stats.mutex_count && !stats.sem_count &&
           !stats.queue_count && !stats.event_count && !stats.timer_count);
    puts("Baremetal capability failures are explicit passed");
    puts("7 baremetal board-model contract groups passed");
    return 0;
}
