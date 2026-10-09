/** Acquisition clock math, pending-tick and epoch behavior; no physical timing. */
#include "stm32_uart_resource.h"
#include "stm32f4xx_hal.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <stdio.h>
TestSysTick fake_systick;
TestSCB fake_scb;
uint32_t SystemCoreClock=168000000,now,mask;
uint32_t __get_PRIMASK(void) {return mask;}
void __disable_irq(void) {mask=1;}
void __set_PRIMASK(uint32_t key) {mask=key;}
void __DMB(void) {}
nx_arch_irq_state_t nx_arch_irq_save(void) { nx_arch_irq_state_t old={mask}; mask=1; return old; }
void nx_arch_irq_restore(nx_arch_irq_state_t old) { mask=old.value; }
void nx_arch_dmb(void) {}
uint32_t HAL_GetTick(void) {return now;}
int main(void) {
    fake_systick.LOAD=167999;fake_systick.VAL=83999;now=10;mask=1;
    assert(stm32_uart_board_timestamp_us()==10500 && mask==1);
    assert(stm32_uart_board_timestamp_resolution_us()==1);
    fake_scb.ICSR=SCB_ICSR_PENDSTSET_Msk;fake_systick.VAL=151199;
    assert(stm32_uart_board_timestamp_us()==11100);
    now=11;fake_scb.ICSR=0;
    assert(stm32_uart_board_timestamp_us()==11100);
    now=UINT32_MAX;fake_systick.VAL=fake_systick.LOAD;
    assert(stm32_uart_board_timestamp_us()==UINT64_C(4294967295000));
    now=0;fake_systick.VAL=83999;
    assert(stm32_uart_board_timestamp_us()==UINT64_C(4294967296500));
    /* Reprogramming phase cannot publish a decreasing event timestamp. */
    fake_systick.VAL=fake_systick.LOAD;
    assert(stm32_uart_board_timestamp_us()==UINT64_C(4294967296500));
    fake_systick.LOAD=0;
    assert(stm32_uart_board_timestamp_resolution_us()==1000);
    puts("STM32 UART acquisition phase, pending tick, epoch and nested mask passed");
    return 0;
}
