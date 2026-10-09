/* Actual HAL, Runtime, device registry and selected platform hooks. Vendor and
 * OSAL ports below are observed fault models; no MCU registers or HIL execute.
 * Independent register-model executables test production SoC helper bodies. */
#include "runtime/nx_runtime.h"
#include "hal/nx_hal.h"
#include "hal/provider/nx_device_provider.h"
#include "osal/osal.h"
#include "osal/osal_baremetal.h"
#include "arch/nx_arch.h"
#include "vendor_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

model_systick_t model_systick;
model_scb_t model_scb;
uint16_t model_flash_kib = 1024;
uint32_t SystemCoreClock = 16000000u;
static bool isr, osal_ready;
static uint32_t primask, basepri, faultmask;
static unsigned init_calls, cleanup_calls, clock_releases, board_calls;
static unsigned osal_init_calls, osal_deinit_calls;
static bool fail_init, fail_cleanup, fail_clock, fail_release, resource_busy;
static osal_status_t osal_init_status = OSAL_OK;
static osal_scheduler_state_t scheduler = OSAL_SCHEDULER_NOT_STARTED;
static osal_baremetal_clock_t bound_clock;
static nx_device_state_t life_state;

bool nx_arch_in_isr(void) {
    return isr;
}
bool nx_arch_irq_is_masked(void) {
    return primask || basepri || faultmask;
}
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved = {primask};
    primask = 1u;
    return saved;
}
void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    primask = saved.value;
}
void nx_arch_dmb(void) {
}
void nx_arch_dsb(void) {
}
void nx_arch_isb(void) {
}
bool osal_is_initialized(void) {
    return osal_ready;
}
osal_status_t osal_get_execution_info(osal_execution_info_t* execution) {
    *execution = (osal_execution_info_t){.initialized = osal_ready,
                                         .backend = OSAL_BACKEND_FREERTOS,
                                         .scheduler_state = scheduler};
    return OSAL_OK;
}
osal_status_t osal_init(void) {
    ++osal_init_calls;
    if (osal_init_status == OSAL_OK)
        osal_ready = true;
    return osal_init_status;
}
osal_status_t osal_deinit(void) {
    ++osal_deinit_calls;
#if defined(NX_CONFIG_OSAL_FREERTOS)
    if (scheduler != OSAL_SCHEDULER_NOT_STARTED)
        return OSAL_ERROR_BUSY;
#endif
    osal_ready = false;
    return OSAL_OK;
}
osal_status_t osal_baremetal_set_clock(osal_baremetal_clock_t clock) {
    if (bound_clock && bound_clock != clock)
        return OSAL_ERROR_BUSY;
    bound_clock = clock;
    return OSAL_OK;
}
osal_status_t osal_baremetal_clear_clock(osal_baremetal_clock_t clock) {
    if (osal_ready || (bound_clock && bound_clock != clock))
        return OSAL_ERROR_BUSY;
    bound_clock = NULL;
    return OSAL_OK;
}
HAL_StatusTypeDef HAL_Init(void) {
    ++init_calls;
    model_systick.CTRL = 7u;
    return fail_init ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_DeInit(void) {
    ++cleanup_calls;
    assert(nx_device_shutdown_is_active() && !osal_ready);
    assert(model_systick.CTRL == 0u);
    return fail_cleanup ? HAL_ERROR : HAL_OK;
}
uint32_t HAL_GetTick(void) {
    return 0u;
}
int SystemClock_Config(void) {
    return fail_clock ? -1 : 0;
}
int stm32_perf_init(void) {
    return 0;
}
void stm32_perf_deinit(void) {
}
void stm32_boot_time_mark(uint8_t mark) {
    assert(mark == 3);
}
void HAL_NVIC_SetPriorityGrouping(uint32_t group) {
    assert(group == 3);
}
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t sub) {
    (void)irq;
    assert(priority == 15 && sub == 0);
}
nx_status_t nx_board_prepare_safe_outputs(void) {
    ++board_calls;
    return NX_OK;
}
nx_status_t nx_stm32f407_clock_release(void) {
    ++clock_releases;
    assert(!osal_ready && nx_device_shutdown_is_active());
    SystemCoreClock = 16000000u;
    return fail_release ? NX_ERR_TIMEOUT : NX_OK;
}
nx_status_t nx_stm32f407_resources_idle(void) {
    return resource_busy ? NX_ERR_BUSY : NX_OK;
}
nx_status_t nx_gd32_board_safe_init(void) {
    ++board_calls;
    return NX_OK;
}
int nx_gd32f470_clock_validate(void) {
    return fail_clock ? -1 : 0;
}
int nx_gd32f470_timebase_init(void) {
    ++init_calls;
    return fail_init ? -1 : 0;
}
int nx_gd32f470_timebase_deinit(void) {
    return 0;
}
int nx_gd32f470_clock_release(void) {
    ++clock_releases;
    ++cleanup_calls;
    assert(!osal_ready && nx_device_shutdown_is_active());
    assert(model_systick.CTRL == 0u);
    SystemCoreClock = 16000000u;
    return fail_cleanup || fail_release ? -1 : 0;
}
nx_status_t nx_gd32f470_resources_idle(void) {
    return resource_busy ? NX_ERR_BUSY : NX_OK;
}
uint32_t nx_gd32f470_millis(void) {
    return 0u;
}
void NVIC_SetPriorityGrouping(uint32_t group) {
    assert(group == 3);
}
void NVIC_SetPriority(int irq, uint32_t priority) {
    (void)irq;
    assert(priority == 15);
}
uint32_t SysTick_Config(uint32_t count) {
    model_systick.CTRL = 7;
    model_systick.LOAD = count;
    return 0;
}

static nx_status_t device_init(nx_lifecycle_t* self) {
    (void)self;
    life_state = NX_DEV_STATE_RUNNING;
    return NX_OK;
}
static nx_status_t device_deinit(nx_lifecycle_t* self) {
    (void)self;
    life_state = NX_DEV_STATE_UNINITIALIZED;
    return NX_OK;
}
static nx_device_state_t device_state(nx_lifecycle_t* self) {
    (void)self;
    return life_state;
}
static nx_lifecycle_t lifecycle = {
    .init = device_init, .deinit = device_deinit, .get_state = device_state};
static nx_lifecycle_t* get_lifecycle(void* api) {
    assert(api == &lifecycle);
    return &lifecycle;
}
static nx_status_t construct(const nx_device_t* dev, void** api) {
    (void)dev;
    *api = &lifecycle;
    return NX_OK;
}
static nx_device_config_state_t config_state;
static const nx_device_t descriptor = {.name = "GPIO-model",
                                       .state = &config_state,
                                       .device_class = NX_DEVICE_CLASS_GPIO,
                                       .construct = construct,
                                       .get_lifecycle = get_lifecycle};
static nx_status_t open(nx_device_ref_t* ref) {
    return nx_device_open("GPIO-model", NX_DEVICE_CLASS_GPIO, 1u, ref);
}
static void boot(void) {
    assert(nx_runtime_bootstrap(NULL) == NX_OK);
    assert(nx_runtime_get_state() == NX_RUNTIME_READY &&
           nx_hal_get_state() == NX_HAL_READY);
}
static void offline(void) {
    assert(nx_runtime_shutdown(NULL) == NX_OK);
    assert(nx_runtime_get_state() == NX_RUNTIME_OFFLINE &&
           nx_hal_get_state() == NX_HAL_OFFLINE);
    assert(!nx_device_shutdown_is_active() && !osal_ready &&
           model_systick.CTRL == 0);
#if defined(NX_CONFIG_OSAL_BAREMETAL)
    assert(!bound_clock);
#endif
}
static void assert_quarantine(void) {
    nx_device_ref_t ref;
    assert(nx_hal_get_state() == NX_HAL_PARTIAL &&
           nx_runtime_get_state() == NX_RUNTIME_PARTIAL);
    assert(!osal_ready && nx_device_shutdown_is_active());
    assert(open(&ref) == NX_ERR_BUSY);
    nx_device_shutdown_end(); /* Migration API cannot remove private owner. */
    assert(nx_device_shutdown_is_active() && open(&ref) == NX_ERR_BUSY);
    assert(nx_hal_init() == NX_ERR_INVALID_STATE);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    assert(nx_device_register(&descriptor) == NX_OK);
    const char* scenario = argv[1];
    nx_boot_report_t report;
    if (!strcmp(scenario, "initial-resource-busy")) {
        resource_busy = true;
        assert(nx_runtime_bootstrap(&report) == NX_ERR_BUSY);
        assert(report.state == NX_RUNTIME_OFFLINE && !report.hal_owned);
        assert(report.rollback_status == NX_OK &&
               nx_hal_get_last_cleanup_status() == NX_OK);
        assert(!init_calls && !board_calls && !cleanup_calls &&
               !osal_init_calls);
        assert(!nx_device_shutdown_is_active());
        resource_busy = false;
        nx_device_ref_t ref;
        assert(open(&ref) == NX_OK);
        assert(nx_hal_init() == NX_ERR_BUSY &&
               nx_hal_get_state() == NX_HAL_OFFLINE);
        assert(!init_calls && !cleanup_calls && config_state.owner == 1u);
        assert(nx_device_close(ref) == NX_OK);
        boot();
        offline();
    } else if (!strcmp(scenario, "idle-reinit")) {
        boot();
        offline();
        boot();
        offline();
        assert(init_calls == 2 && cleanup_calls == 2);
    } else if (!strcmp(scenario, "owner-busy")) {
        boot();
        nx_device_ref_t ref;
        assert(open(&ref) == NX_OK);
        unsigned before = osal_deinit_calls;
        assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
        assert(nx_runtime_get_state() == NX_RUNTIME_READY && osal_ready &&
               !cleanup_calls);
        assert(osal_deinit_calls == before && config_state.owner == 1u);
        assert(nx_device_close(ref) == NX_OK);
        offline();
    } else if (!strcmp(scenario, "osal-rollback")) {
        osal_init_status = OSAL_ERROR_NO_MEMORY;
        assert(nx_runtime_bootstrap(&report) == NX_ERR_NO_MEMORY);
        assert(report.osal_status == OSAL_ERROR_NO_MEMORY &&
               report.rollback_status == NX_OK);
        assert(report.state == NX_RUNTIME_OFFLINE && !report.hal_owned &&
               !report.osal_owned);
        assert(cleanup_calls == 1u && !nx_device_shutdown_is_active());
        osal_init_status = OSAL_OK;
        boot();
        offline();
    } else if (!strcmp(scenario, "cleanup-quarantine")) {
        boot();
        fail_cleanup = true;
        assert(nx_runtime_shutdown(&report) != NX_OK);
        assert(report.hal_owned && !report.osal_owned);
        assert_quarantine();
        fail_cleanup = false;
        offline();
        boot();
        offline();
        nx_device_ref_t ref;
        boot();
        assert(open(&ref) == NX_OK);
        assert(nx_device_close(ref) == NX_OK);
        offline();
    } else if (!strcmp(scenario, "clock-cleanup-quarantine")) {
        boot();
        fail_release = true;
        assert(nx_runtime_shutdown(&report) == NX_ERR_TIMEOUT);
        assert(report.hal_status == NX_ERR_TIMEOUT && report.hal_owned &&
               !report.osal_owned);
        assert_quarantine();
        fail_release = false;
        offline();
        boot();
        offline();
    } else if (!strcmp(scenario, "init-quarantine")) {
        fail_clock = fail_cleanup = true;
        assert(nx_runtime_bootstrap(&report) == NX_ERR_IO);
        assert(report.hal_status == NX_ERR_IO &&
               report.rollback_status != NX_OK);
        assert(report.hal_owned && !report.osal_owned);
        assert_quarantine();
        fail_clock = fail_cleanup = false;
        offline();
        boot();
        offline();
    } else if (!strcmp(scenario, "preflight-busy")) {
        boot();
        resource_busy = true;
        unsigned before = board_calls;
        uint32_t tick = model_systick.CTRL;
        assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
        assert(report.state == NX_RUNTIME_READY && osal_ready &&
               nx_hal_get_state() == NX_HAL_READY);
        assert(!cleanup_calls && !clock_releases && board_calls == before &&
               model_systick.CTRL == tick);
        assert(!nx_device_shutdown_is_active());
        resource_busy = false;
        offline();
    } else if (!strcmp(scenario, "legacy-active")) {
        boot();
        assert(nx_device_get("GPIO-model") == &lifecycle);
        assert(device_init(&lifecycle) == NX_OK);
        assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
        assert(report.state == NX_RUNTIME_READY && !cleanup_calls);
        assert(device_deinit(&lifecycle) == NX_OK);
        offline();
    } else if (!strcmp(scenario, "context")) {
        boot();
        for (unsigned mask = 0; mask < 3; ++mask) {
            primask = mask == 0 ? 1u : 0u;
            basepri = mask == 1 ? 0x80u : 0u;
            faultmask = mask == 2 ? 1u : 0u;
            assert(nx_runtime_shutdown(&report) == NX_ERR_INVALID_STATE);
            assert(nx_hal_deinit() == NX_ERR_INVALID_STATE);
            assert(primask == (mask == 0 ? 1u : 0u) &&
                   basepri == (mask == 1 ? 0x80u : 0u) &&
                   faultmask == (mask == 2 ? 1u : 0u));
            assert(!cleanup_calls && osal_ready);
            primask = basepri = faultmask = 0u;
        }
        isr = true;
        assert(nx_runtime_shutdown(NULL) == NX_ERR_INVALID_STATE);
        assert(nx_hal_deinit() == NX_ERR_CONTEXT && !cleanup_calls);
        isr = false;
        offline();
    } else if (!strcmp(scenario, "direct-hal-busy")) {
        boot();
        assert(nx_hal_deinit() == NX_ERR_BUSY);
        assert(nx_hal_get_state() == NX_HAL_READY && !cleanup_calls &&
               osal_ready);
        offline();
    } else if (!strcmp(scenario, "kernel-running")) {
#if defined(NX_CONFIG_OSAL_FREERTOS)
        boot();
        scheduler = OSAL_SCHEDULER_RUNNING;
        assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
        assert(report.state == NX_RUNTIME_READY && !cleanup_calls &&
               osal_ready);
        scheduler = OSAL_SCHEDULER_SUSPENDED;
        assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
        assert(report.state == NX_RUNTIME_READY && !cleanup_calls &&
               osal_ready);
        scheduler = OSAL_SCHEDULER_NOT_STARTED;
        offline();
        /* Direct kernel startup may happen without OSAL initialized. Platform
         * preflight must still refuse clocks until the kernel is not started.
         */
        assert(nx_hal_init() == NX_OK);
        scheduler = OSAL_SCHEDULER_RUNNING;
        assert(nx_hal_deinit() == NX_ERR_BUSY);
        assert(nx_hal_get_state() == NX_HAL_READY &&
               !nx_device_shutdown_is_active());
        scheduler = OSAL_SCHEDULER_SUSPENDED;
        assert(nx_hal_deinit() == NX_ERR_BUSY);
        scheduler = OSAL_SCHEDULER_NOT_STARTED;
        assert(nx_hal_deinit() == NX_OK);
#else
        assert(!"This scenario requires the modeled FreeRTOS backend");
#endif
    } else {
        assert(!"unknown scenario");
    }
    printf("Actual production HAL/Runtime/%s hook model: %s passed; physical=false\n",
#if defined(NX_MODEL_STM32)
           "STM32",
#else
           "GD32",
#endif
           scenario);
    (void)osal_init_calls;
    (void)scheduler;
    return 0;
}
