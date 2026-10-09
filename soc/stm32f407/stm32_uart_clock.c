/** STM32F407 1 kHz HAL/SysTick acquisition clock. Task and IRQ callers share
 * an extended millisecond epoch and sampled sub-tick phase. Timing acceptance
 * requires no interrupt masking lasting one full tick and no tickless mode. */
#if !defined(NEXUS_PLATFORM_NATIVE) || defined(NEXUS_TEST_UART_CLOCK)
#include "stm32_uart_resource.h"
#include "arch/nx_arch.h"
#include "stm32f4xx_hal.h"

static uint32_t previous_tick;
static uint64_t epoch, previous_timestamp;
uint64_t stm32_uart_board_timestamp_us(void) {
    nx_arch_irq_state_t key = nx_arch_irq_save();
    uint32_t tick = HAL_GetTick();
    uint32_t load = SysTick->LOAD;
    uint32_t phase1 = SysTick->VAL;
    bool pending = (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0;
    uint32_t phase2 = SysTick->VAL;
    if (phase2 > phase1) pending = true;
    if (tick < previous_tick) epoch += UINT64_C(1) << 32;
    previous_tick = tick;
    uint64_t ticks = epoch + tick + (pending ? 1 : 0);
    uint32_t cycles = load >= phase2 ? load - phase2 : 0;
    uint64_t timestamp = ticks * 1000 +
        (load ? (uint64_t)cycles * 1000 / ((uint64_t)load + 1) : 0);
    /* HAL/platform initialization and scheduler switch can reprogram SysTick.
     * Freeze backwards samples rather than publish a false decreasing time. */
    if (timestamp < previous_timestamp) timestamp = previous_timestamp;
    previous_timestamp = timestamp;
    nx_arch_dmb();
    nx_arch_irq_restore(key);
    return timestamp;
}
uint32_t stm32_uart_board_timestamp_resolution_us(void) {
    return SysTick->LOAD && SystemCoreClock >= 1000000 ? 1 : 1000;
}
#endif
