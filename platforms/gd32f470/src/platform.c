#include "arch/nx_arch.h"
#include "board.h"
#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include "hal/nx_status.h"
#include "hal/provider/nx_device_provider.h"
#include "osal/osal.h"
#include "nexus_config.h"
#ifdef NX_CONFIG_OSAL_BAREMETAL
#include "osal/osal_baremetal.h"
#endif

extern int nx_gd32f470_clock_validate(void);
extern int nx_gd32f470_timebase_init(void);
static bool owned;
static bool ready;
static bool timebase_owned;
#ifdef NX_CONFIG_OSAL_BAREMETAL
static bool clock_bound;
#endif

/* Private implementation hooks: only the common HAL admits these calls. */
nx_status_t nx_platform_init(void) {
    if (nx_arch_in_isr())
        return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked())
        return NX_ERR_INVALID_STATE;
    if (ready)
        return NX_OK;
    if (owned)
        return NX_ERR_INVALID_STATE;
    owned = true;
    /* Keep reviewed LED/CS/DE idle levels through clock transitions. GPIO
     * remains clocked on release; no blanket reset creates a CS/DE pulse. */
    nx_status_t board_status = nx_gd32_board_safe_init();
    if (board_status != NX_OK)
        return board_status;
    if (nx_gd32f470_clock_validate() != 0)
        return NX_ERR_IO;
    NVIC_SetPriorityGrouping(3u);
    timebase_owned = true;
    if (nx_gd32f470_timebase_init() != 0 ||
        SysTick_Config(SystemCoreClock / 1000u) != 0u)
        return NX_ERR_IO;
    NVIC_SetPriority(SysTick_IRQn, 15u);
    NVIC_SetPriority(PendSV_IRQn, 15u);
#ifdef NX_CONFIG_OSAL_BAREMETAL
    if (osal_baremetal_set_clock(nx_gd32f470_millis) != OSAL_OK)
        return NX_ERR_BUSY;
    clock_bound = true;
#endif
    ready = true;
    return NX_OK;
}

nx_status_t nx_platform_init_check(void) {
    if (nx_arch_in_isr())
        return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked())
        return NX_ERR_INVALID_STATE;
    if (osal_is_initialized())
        return NX_ERR_BUSY;
#ifdef NX_CONFIG_OSAL_FREERTOS
    osal_execution_info_t execution;
    if (osal_get_execution_info(&execution) != OSAL_OK)
        return NX_ERR_IO;
    if (execution.scheduler_state != OSAL_SCHEDULER_NOT_STARTED)
        return NX_ERR_BUSY;
#endif
    return nx_gd32f470_resources_idle();
}

nx_status_t nx_platform_shutdown_check(void) {
    if (nx_arch_in_isr())
        return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked())
        return NX_ERR_INVALID_STATE;
    if (!nx_device_shutdown_is_active())
        return NX_ERR_INVALID_STATE;
    if (osal_is_initialized())
        return NX_ERR_BUSY;
#ifdef NX_CONFIG_OSAL_FREERTOS
    osal_execution_info_t execution;
    if (osal_get_execution_info(&execution) != OSAL_OK)
        return NX_ERR_IO;
    if (execution.scheduler_state != OSAL_SCHEDULER_NOT_STARTED)
        return NX_ERR_BUSY;
#endif
    nx_status_t status = nx_device_provider_quiescence_check();
    if (status != NX_OK)
        return status;
    return nx_gd32f470_resources_idle();
}

nx_status_t nx_platform_deinit(void) {
    if (!owned)
        return NX_OK;
    if (!nx_device_shutdown_is_active())
        return NX_ERR_INVALID_STATE;
    ready = false;
#ifdef NX_CONFIG_OSAL_BAREMETAL
    if (clock_bound) {
        if (osal_baremetal_clear_clock(nx_gd32f470_millis) != OSAL_OK)
            return NX_ERR_BUSY;
        clock_bound = false;
    }
#endif
    /* All cleanup polling is bounded register observation, independent of the
     * time sources being stopped. Existing users have already been settled. */
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL = 0u;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    __DSB();
    __ISB();
    if (timebase_owned) {
        if (nx_gd32f470_timebase_deinit() != 0)
            return NX_ERR_IO;
        timebase_owned = false;
    }
    if (nx_gd32f470_clock_release() != 0)
        return NX_ERR_TIMEOUT;
    nx_status_t status = nx_gd32_board_safe_init();
    if (status != NX_OK)
        return status;
    owned = false;
    return NX_OK;
}
