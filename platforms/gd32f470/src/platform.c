#include "arch/nx_arch.h"
#include "board.h"
#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include "hal/nx_status.h"
#include "nexus_config.h"
#ifdef NX_CONFIG_OSAL_BAREMETAL
#include "osal/osal_baremetal.h"
#endif

extern int nx_gd32f470_clock_validate(void);
extern int nx_gd32f470_timebase_init(void);
static bool initialized;

nx_status_t nx_platform_init(void) {
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    if (initialized) { return NX_OK; }
    /* Establish electrical idle levels while the reset IRC clock is usable;
     * a failed external-clock transition must leave CS/DE inactive. */
    nx_status_t board_status = nx_gd32_board_safe_init();
    if (board_status != NX_OK) { return board_status; }
    if (nx_gd32f470_clock_validate() != 0) { return NX_ERR_IO; }
    NVIC_SetPriorityGrouping(3u); /* Four preemption bits, no subpriority. */
    if (nx_gd32f470_timebase_init() != 0 ||
        SysTick_Config(SystemCoreClock / 1000u) != 0u) { return NX_ERR_IO; }
    NVIC_SetPriority(SysTick_IRQn, 15u);
    NVIC_SetPriority(PendSV_IRQn, 15u);
#ifdef NX_CONFIG_OSAL_BAREMETAL
    if (osal_baremetal_set_clock(nx_gd32f470_millis) != OSAL_OK) {
        SysTick->CTRL = 0u;
        return NX_ERR_IO;
    }
#endif
    initialized = true;
    return NX_OK;
}

nx_status_t nx_platform_deinit(void) {
    /* Product-wide peripheral/scheduler shutdown is not safe without a
     * quiescence protocol. Refuse success while the initialized chip is owned. */
    return initialized ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
