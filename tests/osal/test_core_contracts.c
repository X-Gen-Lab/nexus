/* Executable contract regressions: observable outcomes, not source matching. */
#define _POSIX_C_SOURCE 200809L
#include "hal/provider/nx_device_provider.h"
#include "osal/osal.h"
#include "event_mask_contract.h"
#include "resource_usage_contract.h"
#ifndef OSAL_CONTRACT_NO_HAL
#include "hal/system/nx_mutex.h"
#include "hal/system/nx_mem.h"
#include "hal/base/nx_device.h"
#include "hal/system/nx_power_manager.h"
#endif
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void delay_ms(unsigned ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
}
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static void wait_flag(atomic_bool* flag) {
    uint64_t limit = now_ms() + 2000;
    while (!atomic_load(flag) && now_ms() < limit) delay_ms(1);
    assert(atomic_load(flag));
}
static unsigned critical_counter;
static void* nested_critical_worker(void* arg) {
    (void)arg;
    for (unsigned i = 0; i < 10000; ++i) {
        osal_enter_critical();
        osal_enter_critical();
        ++critical_counter;
        osal_exit_critical();
        osal_exit_critical();
    }
    return NULL;
}
static void test_nested_critical(void) {
    pthread_t threads[8];
    for (unsigned i = 0; i < 8; ++i)
        assert(pthread_create(&threads[i], NULL, nested_critical_worker, NULL) == 0);
    for (unsigned i = 0; i < 8; ++i) assert(pthread_join(threads[i], NULL) == 0);
    assert(critical_counter == 80000);
}
#ifndef OSAL_CONTRACT_NO_HAL
static nx_atomic_t atomic_counter;
static void* atomic_worker(void* arg) {
    (void)arg;
    for (unsigned i = 0; i < 10000; ++i)
        nx_atomic_fetch_add(&atomic_counter, 1);
    return NULL;
}
static void test_hal_atomic(void) {
    pthread_t threads[8];
    nx_atomic_store(&atomic_counter, 0);
    for (unsigned i = 0; i < 8; ++i)
        assert(pthread_create(&threads[i], NULL, atomic_worker, NULL) == 0);
    for (unsigned i = 0; i < 8; ++i) assert(pthread_join(threads[i], NULL) == 0);
    assert(nx_atomic_load(&atomic_counter) == 80000);
    uint32_t expected = 1;
    assert(!nx_atomic_compare_exchange(&atomic_counter, &expected, 0));
    assert(expected == 80000);
    assert(nx_atomic_compare_exchange(&atomic_counter, &expected, 10));
}
NX_MEM_POOL_DEFINE(test_pool, 3, 64);
static atomic_bool pool_owners[64];
static void* pool_worker(void* arg) {
    (void)arg;
    for (unsigned i=0;i<10000;++i) {
        void* ptr = nx_mem_alloc_from_pool(&test_pool);
        assert(ptr);
        assert((uintptr_t)ptr % _Alignof(max_align_t) == 0);
        size_t index = ((uint8_t*)ptr - (uint8_t*)test_pool.buffer) / test_pool.block_size;
        assert(!atomic_exchange(&pool_owners[index], true));
        *(uint8_t*)ptr=0x5a;
        atomic_store(&pool_owners[index], false);
        assert(nx_mem_free_to_pool(&test_pool, ptr) == NX_OK);
    }
    return NULL;
}
static void test_hal_memory_pool(void) {
    assert(nx_mem_init(NX_MEM_MODE_STATIC, NULL) == NX_OK);
    void* held = nx_mem_alloc_from_pool(&test_pool);
    assert(held);
    assert(nx_mem_init(NX_MEM_MODE_DYNAMIC, NULL) == NX_ERR_BUSY);
    assert(nx_mem_free_to_pool(&test_pool, (uint8_t*)held+1) == NX_ERR_INVALID_PARAM);
    assert(nx_mem_free_to_pool(&test_pool, held) == NX_OK);
    assert(nx_mem_free_to_pool(&test_pool, held) == NX_ERR_INVALID_STATE);
    pthread_t threads[8];
    for(unsigned i=0;i<8;++i) assert(pthread_create(&threads[i],NULL,pool_worker,NULL)==0);
    for(unsigned i=0;i<8;++i) assert(pthread_join(threads[i],NULL)==0);
    assert(test_pool.allocated==0 && test_pool.peak<=8);
    assert(nx_mem_init(NX_MEM_MODE_DYNAMIC,NULL)==NX_OK);
    held=nx_mem_alloc(1);
    assert(held && (uintptr_t)held % _Alignof(max_align_t)==0);
    assert(nx_mem_alloc(SIZE_MAX)==NULL);
    nx_mem_stats_t stats;
    assert(nx_mem_get_stats(&stats)==NX_OK && stats.allocated_bytes==1);
    nx_mem_free(held);
    assert(nx_mem_get_stats(&stats)==NX_OK && stats.allocated_bytes==0);
}
static atomic_uint device_init_count;
static nx_device_config_state_t test_device_state;
static int test_device_api;
static void* initialize_test_device(const nx_device_t* dev) {
    (void)dev;
    atomic_fetch_add(&device_init_count, 1);
    delay_ms(5);
    return &test_device_api;
}
static const nx_device_t test_device = {
    .name="contract-device", .state=&test_device_state, .device_init=initialize_test_device,
};
static void* device_worker(void* arg) {
    (void)arg;
    uint64_t limit=now_ms()+1000;
    void*api;
    while(!(api=nx_device_init(&test_device)) && now_ms()<limit) delay_ms(1);
    assert(api==&test_device_api);
    return NULL;
}
static void test_device_initialization_race(void) {
    pthread_t threads[8];
    for(unsigned i=0;i<8;++i) assert(pthread_create(&threads[i],NULL,device_worker,NULL)==0);
    for(unsigned i=0;i<8;++i) assert(pthread_join(threads[i],NULL)==0);
    assert(atomic_load(&device_init_count)==1);
}
static void test_power_manager_capability_and_invalid_instances(void) {
    nx_power_manager_t* manager = nx_get_power_manager();
    assert(manager && manager->enter_mode && manager->get_mode);
    assert(nx_get_power_manager() == manager);
    assert(manager->enter_mode(NULL, NX_POWER_RUN) == NX_ERR_NULL_PTR);
    assert(manager->get_mode(NULL) == NX_POWER_UNKNOWN);
    nx_power_manager_t foreign = *manager;
    assert(manager->enter_mode(&foreign, NX_POWER_RUN) == NX_ERR_INVALID_PARAM);
    assert(manager->get_mode(&foreign) == NX_POWER_UNKNOWN);
    assert(manager->enter_mode(manager, (nx_power_mode_t)-1) == NX_ERR_INVALID_PARAM);
    assert(manager->enter_mode(manager, NX_POWER_UNKNOWN) == NX_ERR_INVALID_PARAM);
    assert(manager->get_mode(manager) == NX_POWER_RUN);
    assert(manager->enter_mode(manager, NX_POWER_RUN) == NX_OK);
    assert(manager->enter_mode(manager, NX_POWER_SLEEP) == NX_ERR_NOT_SUPPORTED);
    assert(manager->get_mode(manager) == NX_POWER_RUN);
    assert(manager->enter_mode(manager, NX_POWER_STOP) == NX_ERR_NOT_SUPPORTED);
    assert(manager->get_mode(manager) == NX_POWER_RUN);
}
#endif
static void test_generation_and_validation(void) {
    for (unsigned i = 0; i < 1000; ++i) {
        osal_sem_handle_t old, fresh;
        assert(osal_sem_create(0, 1, &old) == OSAL_OK);
        assert(osal_sem_delete(old) == OSAL_OK);
        assert(osal_sem_create(1, 1, &fresh) == OSAL_OK);
        assert(fresh != old);
        assert(osal_sem_take(old, OSAL_NO_WAIT) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_give((void*)(uintptr_t)1) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_mutex_lock(fresh, 0) == OSAL_ERROR_INVALID_PARAM);
        assert(osal_sem_take(fresh, 0) == OSAL_OK);
        assert(osal_sem_delete(fresh) == OSAL_OK);
    }
}
typedef struct {
    void* handle;
    unsigned kind;
    atomic_bool started;
    osal_status_t result;
} wait_context_t;
static void* cancelled_waiter(void* arg) {
    wait_context_t* c = arg;
    atomic_store(&c->started, true);
    int data = 9;
    if (c->kind == 0) c->result = osal_sem_take(c->handle, OSAL_WAIT_FOREVER);
    if (c->kind == 1) c->result = osal_queue_receive(c->handle, &data, OSAL_WAIT_FOREVER);
    if (c->kind == 2) c->result = osal_queue_send(c->handle, &data, OSAL_WAIT_FOREVER);
    if (c->kind == 3) {
        osal_event_wait_options_t options = {OSAL_EVENT_WAIT_ALL, true, OSAL_WAIT_FOREVER};
        c->result = osal_event_wait(c->handle, 1, &options, NULL);
    }
    return NULL;
}
static void test_delete_cancels_and_reuse(void) {
    for (unsigned kind = 0; kind < 4; ++kind) {
        for (unsigned iteration = 0; iteration < 20; ++iteration) {
            wait_context_t c = {.kind = kind, .started = false};
            if (kind == 0) assert(osal_sem_create(0, 1, &c.handle) == OSAL_OK);
            if (kind == 1 || kind == 2) {
                assert(osal_queue_create(sizeof(int), 1, &c.handle) == OSAL_OK);
                int value = 1;
                if (kind == 2) assert(osal_queue_send(c.handle, &value, 0) == OSAL_OK);
            }
            if (kind == 3) assert(osal_event_create(&c.handle) == OSAL_OK);
            pthread_t waiter;
            assert(pthread_create(&waiter, NULL, cancelled_waiter, &c) == 0);
            wait_flag(&c.started);
            delay_ms(2);
            void* fresh = NULL;
            if (kind == 0) {
                assert(osal_sem_delete(c.handle) == OSAL_OK);
                assert(osal_sem_create(0, 1, &fresh) == OSAL_OK);
            } else if (kind == 3) {
                assert(osal_event_delete(c.handle) == OSAL_OK);
                assert(osal_event_create(&fresh) == OSAL_OK);
            } else {
                assert(osal_queue_delete(c.handle) == OSAL_OK);
                assert(osal_queue_create(sizeof(int), 1, &fresh) == OSAL_OK);
            }
            assert(pthread_join(waiter, NULL) == 0);
            /* Deletion before the call linearizes is also a valid rejection. */
            assert(c.result == OSAL_ERROR_CANCELLED || c.result == OSAL_ERROR_INVALID_PARAM);
            assert(fresh != c.handle);
            if (kind == 0) assert(osal_sem_delete(fresh) == OSAL_OK);
            else if (kind == 3) assert(osal_event_delete(fresh) == OSAL_OK);
            else assert(osal_queue_delete(fresh) == OSAL_OK);
        }
    }
}
static atomic_bool noise_stop;
static void* notify_without_token(void* handle) {
    while (!atomic_load(&noise_stop)) {
        assert(osal_sem_reset(handle, 0) == OSAL_OK);
        delay_ms(1);
    }
    return NULL;
}
static void test_deadline_survives_notifications(void) {
    osal_sem_handle_t s;
    assert(osal_sem_create(0, 1, &s) == OSAL_OK);
    atomic_store(&noise_stop, false);
    pthread_t noise;
    assert(pthread_create(&noise, NULL, notify_without_token, s) == 0);
    uint64_t begin = now_ms();
    assert(osal_sem_take(s, 40) == OSAL_ERROR_TIMEOUT);
    uint64_t elapsed = now_ms() - begin;
    assert(elapsed >= 35 && elapsed < 1000);
    atomic_store(&noise_stop, true);
    assert(pthread_join(noise, NULL) == 0);
    assert(osal_sem_delete(s) == OSAL_OK);
}
static osal_mutex_handle_t owner_mutex;
static void* wrong_owner(void* arg) {
    (void)arg;
    assert(osal_mutex_unlock(owner_mutex) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_mutex_lock(owner_mutex, 5) == OSAL_ERROR_TIMEOUT);
    return NULL;
}
static void test_mutex_ownership_and_reentrant_release(void) {
    assert(osal_mutex_create(&owner_mutex) == OSAL_OK);
    assert(osal_mutex_lock(owner_mutex, 0) == OSAL_OK);
    assert(osal_mutex_lock(owner_mutex, 0) == OSAL_OK);
    assert(osal_mutex_delete(owner_mutex) == OSAL_ERROR_BUSY);
    pthread_t wrong;
    assert(pthread_create(&wrong, NULL, wrong_owner, NULL) == 0);
    assert(pthread_join(wrong, NULL) == 0);
    assert(osal_mutex_unlock(owner_mutex) == OSAL_OK);
    assert(osal_mutex_is_locked(owner_mutex));
    assert(osal_mutex_unlock(owner_mutex) == OSAL_OK);
    assert(!osal_mutex_is_locked(owner_mutex));
    assert(osal_mutex_delete(owner_mutex) == OSAL_OK);
    assert(osal_mutex_lock(owner_mutex, 0) == OSAL_ERROR_INVALID_PARAM);
}
static void test_queue_bounds_modes_and_order(void) {
    osal_queue_handle_t q = (void*)(uintptr_t)1;
    assert(osal_queue_create(SIZE_MAX / 2 + 1, 2, &q) == OSAL_ERROR_INVALID_PARAM);
    assert(q == NULL);
    assert(osal_queue_create(sizeof(int), 3, &q) == OSAL_OK);
    for (int i = 1; i <= 3; ++i) assert(osal_queue_send(q, &i, 0) == OSAL_OK);
    int value = 4;
    assert(osal_queue_send(q, &value, 0) == OSAL_ERROR_FULL);
    assert(osal_queue_set_mode(q, OSAL_QUEUE_MODE_OVERWRITE) == OSAL_OK);
    assert(osal_queue_send(q, &value, 0) == OSAL_OK);
    for (int expected = 2; expected <= 4; ++expected) {
        assert(osal_queue_receive(q, &value, 0) == OSAL_OK);
        assert(value == expected);
    }
    value = 1;
    assert(osal_queue_send(q, &value, 0) == OSAL_OK);
    value = 0;
    assert(osal_queue_send_front(q, &value, 0) == OSAL_OK);
    assert(osal_queue_receive(q, &value, 0) == OSAL_OK && value == 0);
    assert(osal_queue_delete(q) == OSAL_OK);
    assert(osal_queue_create(sizeof(int), 1, &q) == OSAL_OK);
    value = 2;
    assert(osal_queue_send(q, &value, 0) == OSAL_OK);
    assert(osal_queue_send(q, &value, 0) == OSAL_ERROR_FULL); /* mode reset */
    assert(osal_queue_delete(q) == OSAL_OK);
}
static atomic_bool task_started;
static void cooperative_task(void* arg) {
    (void)arg;
    atomic_store(&task_started, true);
    while (!osal_task_should_stop()) osal_task_delay(10);
}
static void test_task_stop_join_and_reuse(void) {
    osal_task_config_t config = {.name = "contract-task", .func = cooperative_task,
                                .stack_size = 0, .priority = 1, .arg = NULL};
    osal_task_handle_t old, next;
    atomic_store(&task_started, false);
    assert(osal_task_create(&config, &old) == OSAL_OK);
    wait_flag(&task_started);
    assert(osal_task_join(old, 1) == OSAL_ERROR_TIMEOUT);
    assert(osal_task_delete(old) == OSAL_ERROR_BUSY);
    assert(osal_task_request_stop(old) == OSAL_OK);
    assert(osal_task_join(old, 1000) == OSAL_OK);
    assert(osal_task_delete(old) == OSAL_OK);
    atomic_store(&task_started, false);
    assert(osal_task_create(&config, &next) == OSAL_OK && old != next);
    assert(osal_task_request_stop(old) == OSAL_ERROR_INVALID_PARAM);
    assert(osal_task_request_stop(next) == OSAL_OK);
    assert(osal_task_join(next, 1000) == OSAL_OK);
    assert(osal_task_delete(next) == OSAL_OK);
}
static atomic_bool callback_entered, callback_release, callback_finished;
static osal_timer_handle_t callback_timer;
static osal_status_t self_delete_result;
static void owned_callback(void* arg) {
    (void)arg;
    atomic_store(&callback_entered, true);
    while (!atomic_load(&callback_release)) delay_ms(1);
    self_delete_result = osal_timer_delete(callback_timer);
    atomic_store(&callback_finished, true);
}
static void test_timer_callback_ownership(void) {
    atomic_store(&callback_entered, false);
    atomic_store(&callback_release, false);
    atomic_store(&callback_finished, false);
    osal_timer_config_t cfg = {.name = "ownership", .period_ms = 1,
                              .mode = OSAL_TIMER_ONE_SHOT,
                              .callback = owned_callback, .arg = NULL};
    assert(osal_timer_create(&cfg, &callback_timer) == OSAL_OK);
    assert(osal_timer_start(callback_timer) == OSAL_OK);
    wait_flag(&callback_entered);
    assert(osal_timer_delete(callback_timer) == OSAL_ERROR_BUSY);
    assert(osal_timer_stop(callback_timer) == OSAL_ERROR_BUSY);
    assert(osal_timer_set_callback(callback_timer, owned_callback, NULL) == OSAL_ERROR_BUSY);
    atomic_store(&callback_release, true);
    wait_flag(&callback_finished);
    assert(self_delete_result == OSAL_ERROR_BUSY);
    osal_status_t result;
    do { result = osal_timer_delete(callback_timer); if (result == OSAL_ERROR_BUSY) delay_ms(1); }
    while (result == OSAL_ERROR_BUSY);
    assert(result == OSAL_OK);
    assert(osal_timer_start(callback_timer) == OSAL_ERROR_INVALID_PARAM);
}
static osal_event_handle_t barrier_event;
static void* event_barrier_worker(void* arg) {
    uint32_t set = (uint32_t)(uintptr_t)arg;
    osal_event_wait_options_t options = {OSAL_EVENT_WAIT_ALL, true, 1000};
    uint32_t out = 0;
    assert(osal_event_sync(barrier_event, set, 3, &options, &out) == OSAL_OK);
    assert(out == 3);
    return NULL;
}
static void test_event_barrier_snapshot(void) {
    for (unsigned i=0;i<100;++i) {
        assert(osal_event_create(&barrier_event) == OSAL_OK);
        pthread_t a,b;
        assert(pthread_create(&a,NULL,event_barrier_worker,(void*)(uintptr_t)1)==0);
        assert(pthread_create(&b,NULL,event_barrier_worker,(void*)(uintptr_t)2)==0);
        assert(pthread_join(a,NULL)==0);
        assert(pthread_join(b,NULL)==0);
        assert(osal_event_get(barrier_event)==0);
        assert(osal_event_set(barrier_event,0x80000000u)==OSAL_ERROR_INVALID_PARAM);
        assert(osal_event_delete(barrier_event)==OSAL_OK);
    }
}
static void test_allocation_boundaries_and_concurrency(void) {
    void* aligned = osal_mem_alloc(1);
    assert(aligned && (uintptr_t)aligned % _Alignof(max_align_t) == 0);
    osal_mem_free(aligned);
    assert(osal_mem_alloc(SIZE_MAX) == NULL);
    assert(osal_mem_calloc(SIZE_MAX / 2 + 1, 2) == NULL);
    assert(osal_mem_alloc_aligned((size_t)1 << (sizeof(size_t) * 8 - 1),
                                 SIZE_MAX / 2 + 1) == NULL);
    osal_mem_stats_t memory;
    assert(osal_mem_get_stats(&memory) == OSAL_OK && memory.total_size < SIZE_MAX);
    void* above_budget = osal_mem_alloc(memory.total_size + 1);
    assert(above_budget);
    assert(osal_mem_get_stats(&memory) == OSAL_OK);
    assert(memory.free_size == 0 && memory.min_free_size == 0);
    assert(osal_mem_get_free_size() == 0 && osal_mem_get_min_free_size() == 0);
    assert(osal_mem_check_integrity() == OSAL_OK);
    osal_mem_free(above_budget);
    assert(osal_reset_stats() == OSAL_OK);
    assert(osal_mem_check_integrity() == OSAL_OK);
    osal_sem_handle_t s;
    assert(osal_sem_create(1, 1, &s) == OSAL_OK);
    assert(osal_sem_give(s) == OSAL_ERROR_FULL);
    assert(osal_sem_give_from_isr(s) == OSAL_ERROR_FULL);
    assert(osal_sem_delete(s) == OSAL_OK);
}
int main(void) {
    assert(osal_init() == OSAL_OK);
    assert(osal_is_initialized());
    osal_backend_info_t info;
    assert(osal_get_backend_info(NULL) == OSAL_ERROR_NULL_POINTER);
    assert(osal_get_backend_info(&info) == OSAL_OK && info.backend == OSAL_BACKEND_NATIVE);
    assert((info.capabilities & (OSAL_CAP_TASKS | OSAL_CAP_DYNAMIC_MEMORY |
        OSAL_CAP_SOFTWARE_TIMERS)) == (OSAL_CAP_TASKS | OSAL_CAP_DYNAMIC_MEMORY |
        OSAL_CAP_SOFTWARE_TIMERS));
    assert(!(info.capabilities & (OSAL_CAP_STATIC_OBJECTS | OSAL_CAP_HARDWARE_ISR |
        OSAL_CAP_PRIORITY_SCHEDULER | OSAL_CAP_MEMORY_SEAL)));
    assert(info.delete_policy == OSAL_DELETE_CANCELS_WAITERS);
    osal_execution_info_t execution;
    assert(osal_get_execution_info(NULL) == OSAL_ERROR_NULL_POINTER);
    assert(osal_get_execution_info(&execution) == OSAL_OK);
    assert(execution.backend == OSAL_BACKEND_NATIVE && execution.initialized &&
        !execution.in_isr && execution.scheduler_state == OSAL_SCHEDULER_NONE);
    assert(osal_mem_seal() == OSAL_ERROR_NOT_SUPPORTED && !osal_mem_is_sealed());
    osal_sem_handle_t lifecycle;
    assert(osal_sem_create(0, 1, &lifecycle) == OSAL_OK);
    assert(osal_deinit() == OSAL_ERROR_BUSY && osal_is_initialized());
    assert(osal_sem_delete(lifecycle) == OSAL_OK);
    assert(osal_deinit() == OSAL_OK && !osal_is_initialized());
    assert(osal_init() == OSAL_OK && osal_is_initialized());
unsigned groups = 0;
#define RUN(test) do { test(); ++groups; puts(#test " passed"); } while (0)
    RUN(test_event_advertised_bits);
    RUN(test_resource_usage_pool);
    RUN(test_nested_critical);
#ifndef OSAL_CONTRACT_NO_HAL
    RUN(test_hal_atomic);
    RUN(test_hal_memory_pool);
    RUN(test_device_initialization_race);
    RUN(test_power_manager_capability_and_invalid_instances);
#endif
    RUN(test_generation_and_validation);
    RUN(test_delete_cancels_and_reuse);
    RUN(test_deadline_survives_notifications);
    RUN(test_mutex_ownership_and_reentrant_release);
    RUN(test_queue_bounds_modes_and_order);
    RUN(test_task_stop_join_and_reuse);
    RUN(test_timer_callback_ownership);
    RUN(test_event_barrier_snapshot);
    RUN(test_allocation_boundaries_and_concurrency);
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK);
    assert(stats.mutex_count == 0 && stats.sem_count == 0 && stats.queue_count == 0);
    assert(stats.task_count == 0 && stats.timer_count == 0 && stats.event_count == 0);
    printf("%u core contract groups passed; no live resources\n", groups);
    assert(osal_deinit() == OSAL_OK && !osal_is_initialized());
    return 0;
}
