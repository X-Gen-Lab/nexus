/* Run the real pinned FreeRTOS kernel with its POSIX simulation port. This
 * validates kernel behavior, not Cortex-M IRQ priority or board timing. */
#include "osal/osal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static atomic_bool completed;
static osal_sem_handle_t waiter_sem;
static osal_status_t waited;
static void sem_waiter(void* arg) {
    (void)arg;
    waited = osal_sem_take(waiter_sem, 1000);
}
static void cooperative(void* arg) {
    (void)arg;
    while (!osal_task_should_stop()) osal_task_delay(1);
}
static atomic_uint callback_count;
static osal_timer_handle_t timer_handle;
static osal_status_t callback_delete;
static void timer_callback(void* arg) {
    assert(arg == &callback_count);
    atomic_fetch_add(&callback_count, 1);
    callback_delete = osal_timer_delete(timer_handle);
}
static osal_task_handle_t make_task(osal_task_func_t func) {
    osal_task_config_t cfg = {.name = "contract", .func = func, .arg = NULL,
                             .stack_size = 32768, .priority = 20};
    osal_task_handle_t task;
    assert(osal_task_create(&cfg, &task) == OSAL_OK);
    return task;
}
static osal_event_handle_t barrier;
static void barrier_task(void* arg) {
    uint32_t set = (uint32_t)(uintptr_t)arg;
    osal_event_wait_options_t opts = {OSAL_EVENT_WAIT_ALL, true, 1000};
    uint32_t bits;
    assert(osal_event_sync(barrier,set,3,&opts,&bits)==OSAL_OK && bits==3);
}
static osal_sem_handle_t delete_start;
static osal_timer_handle_t deleting_timer;
static osal_status_t delete_results[2];
static void delete_racer(void* arg) {
    unsigned index = (unsigned)(uintptr_t)arg;
    assert(osal_sem_take(delete_start, 1000) == OSAL_OK);
    for (unsigned retry = 0; retry < 100; ++retry) {
        osal_status_t result = osal_timer_delete(deleting_timer);
        if (result == OSAL_OK || result == OSAL_ERROR_INVALID_PARAM) {
            delete_results[index] = result;
            return;
        }
        assert(result == OSAL_ERROR_BUSY || result == OSAL_ERROR_TIMEOUT);
        vTaskDelay(1);
    }
    assert(!"timer deletion did not settle");
}
static void unused_timer_callback(void* arg) { (void)arg; }
static void test_concurrent_timer_reclamation(void) {
    assert(osal_sem_create(0, 2, &delete_start) == OSAL_OK);
    /* FreeRTOS reclaims self-deleted TCB/stack storage in the idle task. */
    vTaskDelay(1);
    size_t heap_before = xPortGetFreeHeapSize();
    osal_timer_config_t timer = {.name = "delete-race", .period_ms = 1000,
        .mode = OSAL_TIMER_ONE_SHOT, .callback = unused_timer_callback};
    for (unsigned lifetime = 0; lifetime < 30; ++lifetime) {
        assert(osal_timer_create(&timer, &deleting_timer) == OSAL_OK);
        osal_task_config_t config = {.name = "delete-racer", .func = delete_racer,
            .stack_size = 32768, .priority = 20};
        osal_task_handle_t tasks[2];
        config.arg = (void*)(uintptr_t)0;
        assert(osal_task_create(&config, &tasks[0]) == OSAL_OK);
        config.arg = (void*)(uintptr_t)1;
        assert(osal_task_create(&config, &tasks[1]) == OSAL_OK);
        assert(osal_sem_give(delete_start) == OSAL_OK);
        assert(osal_sem_give(delete_start) == OSAL_OK);
        for (unsigned i = 0; i < 2; ++i) {
            assert(osal_task_join(tasks[i], 1000) == OSAL_OK);
            assert(osal_task_delete(tasks[i]) == OSAL_OK);
        }
        assert((delete_results[0] == OSAL_OK) + (delete_results[1] == OSAL_OK) == 1);
        osal_stats_t stats;
        assert(osal_get_stats(&stats) == OSAL_OK && stats.timer_count == 0);
        osal_timer_handle_t fresh;
        assert(osal_timer_create(&timer, &fresh) == OSAL_OK && fresh != deleting_timer);
        assert(osal_timer_delete(deleting_timer) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_timer_start(fresh) == OSAL_OK);
        assert(osal_timer_delete(fresh) == OSAL_OK);
        assert(osal_get_stats(&stats) == OSAL_OK && stats.timer_count == 0);
        vTaskDelay(1);
        assert(xPortGetFreeHeapSize() == heap_before);
    }
    assert(osal_sem_delete(delete_start) == OSAL_OK);
}
static void test_entry(void* arg) {
    (void)arg;
    assert(osal_init() == OSAL_OK);
    osal_sem_handle_t old, fresh;
    for (unsigned i = 0; i < 1000; ++i) {
        assert(osal_sem_create(1, 1, &old) == OSAL_OK);
        assert(osal_sem_give(old) == OSAL_ERROR_FULL);
        assert(osal_sem_delete(old) == OSAL_OK);
        assert(osal_sem_create(1, 1, &fresh) == OSAL_OK && old != fresh);
        assert(osal_sem_take(old, 0) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_take((void*)(uintptr_t)1, 0) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_delete(fresh) == OSAL_OK);
    }
    puts("FreeRTOS stale handles and semaphore capacity passed");
    assert(osal_sem_create(0, 1, &waiter_sem) == OSAL_OK);
    osal_task_handle_t task = make_task(sem_waiter);
    vTaskDelay(1);
    assert(osal_sem_delete(waiter_sem) == OSAL_ERROR_BUSY);
    assert(osal_sem_give(waiter_sem) == OSAL_OK);
    assert(osal_task_join(task, 1000) == OSAL_OK && waited == OSAL_OK);
    assert(osal_task_delete(task) == OSAL_OK);
    assert(osal_sem_delete(waiter_sem) == OSAL_OK);
    puts("FreeRTOS wait pin prevents object destruction passed");
    task = make_task(cooperative);
    assert(osal_task_join(task, 1) == OSAL_ERROR_TIMEOUT);
    assert(osal_task_delete(task) == OSAL_ERROR_BUSY);
    assert(osal_task_request_stop(task) == OSAL_OK);
    assert(osal_task_join(task, 1000) == OSAL_OK);
    assert(osal_task_delete(task) == OSAL_OK);
    puts("FreeRTOS cooperative stop, return trampoline and join passed");
    osal_queue_handle_t q;
    assert(osal_queue_create(SIZE_MAX / 2 + 1, 2, &q) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_queue_create(sizeof(int), 1, &q) == OSAL_OK);
    assert(osal_queue_set_mode(q, OSAL_QUEUE_MODE_OVERWRITE) == OSAL_OK);
    int value = 1;
    assert(osal_queue_send(q, &value, 0) == OSAL_OK);
    value = 2;
    assert(osal_queue_send(q, &value, 0) == OSAL_OK);
    value = 0;
    assert(osal_queue_receive(q, &value, 0) == OSAL_OK && value == 2);
    assert(osal_queue_delete(q) == OSAL_OK);
    assert(osal_queue_create(sizeof(int),3,&q)==OSAL_OK);
    assert(osal_queue_set_mode(q,OSAL_QUEUE_MODE_OVERWRITE)==OSAL_OK);
    for(int i=1;i<=4;++i)assert(osal_queue_send(q,&i,0)==OSAL_OK);
    for(int i=2;i<=4;++i){assert(osal_queue_receive(q,&value,0)==OSAL_OK && value==i);}
    assert(osal_queue_delete(q)==OSAL_OK);
    osal_mutex_handle_t mutex;
    assert(osal_mutex_create(&mutex) == OSAL_OK);
    assert(osal_mutex_lock(mutex, 0) == OSAL_OK);
    assert(osal_mutex_lock(mutex, 0) == OSAL_OK);
    assert(osal_mutex_delete(mutex) == OSAL_ERROR_BUSY);
    assert(osal_mutex_unlock(mutex) == OSAL_OK && osal_mutex_is_locked(mutex));
    assert(osal_mutex_unlock(mutex) == OSAL_OK && !osal_mutex_is_locked(mutex));
    assert(osal_mutex_delete(mutex) == OSAL_OK);
    puts("FreeRTOS bounded queue and recursive mutex passed");
    osal_sem_handle_t empty;
    assert(osal_task_delay(0) == OSAL_OK);
    assert(osal_sem_create(0, 1, &empty) == OSAL_OK);
    TickType_t before = xTaskGetTickCount();
    assert(osal_sem_take(empty, 1) == OSAL_ERROR_TIMEOUT);
    assert((TickType_t)(xTaskGetTickCount() - before) >= 1);
    assert(osal_sem_delete(empty) == OSAL_OK);
    puts("FreeRTOS positive sub-tick timeout rounds up passed");
    atomic_store(&callback_count, 0);
    osal_timer_config_t timer = {.name = "timer", .period_ms = 1,
                                .mode = OSAL_TIMER_PERIODIC,
                                .callback = timer_callback, .arg = &callback_count};
    assert(osal_timer_create(&timer, &timer_handle) == OSAL_OK);
    assert(osal_timer_start(timer_handle) == OSAL_OK);
    vTaskDelay(5);
    assert(atomic_load(&callback_count) > 0);
    assert(callback_delete == OSAL_ERROR_BUSY);
    assert(osal_timer_stop(timer_handle) == OSAL_OK);
    unsigned stable = atomic_load(&callback_count);
    vTaskDelay(3);
    assert(atomic_load(&callback_count) == stable);
    assert(osal_timer_delete(timer_handle) == OSAL_OK);
    assert(osal_timer_start(timer_handle) == OSAL_ERROR_INVALID_PARAM);
    puts("FreeRTOS timer daemon barriers and callback ownership passed");
    unsigned char* bytes = osal_mem_alloc(16);
    assert(bytes);
    memset(bytes, 0x5a, 16);
    bytes = osal_mem_realloc(bytes, 4096);
    assert(bytes);
    for (unsigned i = 0; i < 16; ++i) assert(bytes[i] == 0x5a);
    osal_mem_free(bytes);
    assert(osal_mem_alloc(SIZE_MAX) == NULL);
    assert(osal_mem_calloc(SIZE_MAX / 2 + 1, 2) == NULL);
    assert(osal_mem_alloc_aligned((size_t)1 << (sizeof(size_t) * 8 - 1), SIZE_MAX / 2 + 1) == NULL);
    assert(osal_mem_get_allocation_count() == 0);
    puts("FreeRTOS size-aware realloc and overflow rejection passed");
    osal_event_handle_t event;
    assert(osal_event_create(&event) == OSAL_OK);
    assert(osal_event_set(event, 3) == OSAL_OK);
    osal_event_wait_options_t options = {OSAL_EVENT_WAIT_ALL, true, 1};
    osal_event_bits_t result;
    assert(osal_event_wait(event, 3, &options, &result) == OSAL_OK && result == 3);
    assert(osal_event_get(event) == 0);
    assert(osal_event_delete(event) == OSAL_OK);
    assert(osal_event_set(event, 1) == OSAL_ERROR_INVALID_PARAM);
    puts("FreeRTOS event consumption and lifecycle passed");
    assert(osal_event_create(&barrier)==OSAL_OK);
    osal_task_config_t barrier_cfg={.name="barrier",.func=barrier_task,.arg=(void*)(uintptr_t)1,
        .stack_size=32768,.priority=20};
    osal_task_handle_t first,second;
    assert(osal_task_create(&barrier_cfg,&first)==OSAL_OK);
    barrier_cfg.arg=(void*)(uintptr_t)2;
    assert(osal_task_create(&barrier_cfg,&second)==OSAL_OK);
    assert(osal_task_join(first,1000)==OSAL_OK);
    assert(osal_task_join(second,1000)==OSAL_OK);
    assert(osal_task_delete(first)==OSAL_OK && osal_task_delete(second)==OSAL_OK);
    assert(osal_event_get(barrier)==0);
    assert(osal_event_delete(barrier)==OSAL_OK);
    puts("FreeRTOS event barrier snapshot passed");
    test_concurrent_timer_reclamation();
    puts("FreeRTOS concurrent delete retry and slot reuse passed");
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK);
    assert(!stats.task_count && !stats.mutex_count && !stats.sem_count &&
           !stats.queue_count && !stats.event_count && !stats.timer_count);
    puts("10 real FreeRTOS kernel contract groups passed");
    atomic_store(&completed, true);
    vTaskEndScheduler();
}
int main(void) {
    assert(xTaskCreate(test_entry, "contracts", 8192, NULL, 5, NULL) == pdPASS);
    vTaskStartScheduler();
    assert(atomic_load(&completed));
    return 0;
}
