/** Faulted worker ownership is quarantined; a fresh task cannot inherit it. */
#ifdef OSAL_TEST_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "osal/osal.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

static osal_mutex_handle_t owned_mutex;
static osal_status_t fresh_unlock, fresh_lock;
static atomic_bool completed;

static void faulty_owner(void* arg) {
    (void)arg;
    assert(osal_mutex_lock(owned_mutex, 0) == OSAL_OK);
    /* Deliberate application fault: returns while owning a mutex. */
}
static void fresh_worker(void* arg) {
    (void)arg;
    fresh_unlock = osal_mutex_unlock(owned_mutex);
    fresh_lock = osal_mutex_lock(owned_mutex, 0);
}
static osal_task_handle_t create_worker(osal_task_func_t func) {
    osal_task_config_t config = {.name = "ownership", .func = func,
        .stack_size = 32768, .priority = 20};
    osal_task_handle_t task;
    assert(osal_task_create(&config, &task) == OSAL_OK);
    assert(osal_task_join(task, 1000) == OSAL_OK);
    return task;
}
static void run_contract(void* arg) {
    (void)arg;
    assert(osal_init() == OSAL_OK);
    assert(osal_mutex_create(&owned_mutex) == OSAL_OK);
    osal_task_handle_t faulted = create_worker(faulty_owner);
    assert(osal_mutex_get_owner(owned_mutex) == faulted);
    assert(osal_task_delete(faulted) == OSAL_ERROR_BUSY);
    osal_task_handle_t fresh = create_worker(fresh_worker);
    assert(fresh != faulted);
    assert(fresh_unlock == OSAL_ERROR_INVALID_PARAM && fresh_lock == OSAL_ERROR_TIMEOUT);
    assert(osal_task_delete(fresh) == OSAL_OK);
    assert(osal_mutex_get_owner(owned_mutex) == faulted);
    assert(osal_mutex_delete(owned_mutex) == OSAL_ERROR_BUSY);
    assert(osal_deinit() == OSAL_ERROR_BUSY);
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK && stats.task_count == 1 && stats.mutex_count == 1);
    osal_resource_usage_t usage;
    assert(osal_get_resource_usage(&usage) == OSAL_OK &&
        usage.tasks.reserved == 1 && usage.mutexes.reserved == 1);
    puts("Faulted mutex owner quarantined; fresh task has no ownership; reset required");
    atomic_store(&completed, true);
#ifdef OSAL_TEST_FREERTOS
    vTaskEndScheduler();
#endif
}
int main(void) {
#ifdef OSAL_TEST_FREERTOS
    assert(xTaskCreate(run_contract, "quarantine", 8192, NULL, 5, NULL) == pdPASS);
    vTaskStartScheduler();
#else
    run_contract(NULL);
#endif
    assert(atomic_load(&completed));
    return 0;
}
