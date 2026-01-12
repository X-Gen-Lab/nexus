/**
 * \file            startup_stm32f407xx.c
 * \brief           STM32F407xx Startup Code (C implementation)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This is a C-based startup file for demonstration.
 *                  Production systems may use assembly startup.
 */

#include <stdint.h>

/*===========================================================================*/
/* External declarations                                                      */
/*===========================================================================*/

extern uint32_t _estack;        /**< End of stack (from linker script) */
extern uint32_t _sidata;        /**< Start of .data in flash */
extern uint32_t _sdata;         /**< Start of .data in RAM */
extern uint32_t _edata;         /**< End of .data in RAM */
extern uint32_t _sbss;          /**< Start of .bss */
extern uint32_t _ebss;          /**< End of .bss */

extern void SystemInit(void);
extern int main(void);

/*===========================================================================*/
/* Default handlers                                                           */
/*===========================================================================*/

/**
 * \brief           Default handler for unimplemented interrupts
 */
void Default_Handler(void)
{
    while (1) {
        /* Infinite loop */
    }
}

/**
 * \brief           Reset handler - entry point
 */
void Reset_Handler(void)
{
    uint32_t* src;
    uint32_t* dst;

    /* Copy .data section from flash to RAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero fill .bss section */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Call system initialization */
    SystemInit();

    /* Call main */
    main();

    /* Should never reach here */
    while (1) {
        /* Infinite loop */
    }
}

/*===========================================================================*/
/* Weak aliases for handlers                                                  */
/*===========================================================================*/

void NMI_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)          __attribute__((weak, alias("Default_Handler")));

/* STM32F4xx Peripheral Interrupts */
void WWDG_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void PVD_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TAMP_STAMP_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void RTC_WKUP_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void FLASH_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void RCC_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

void DMA1_Stream0_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream1_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream2_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream3_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream4_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream5_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream6_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void ADC_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void USART3_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIM1_UP_TIM10_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIM2_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TIM3_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TIM4_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void SPI2_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));

/*===========================================================================*/
/* Vector table                                                               */
/*===========================================================================*/

/**
 * \brief           Interrupt vector table
 */
__attribute__((section(".isr_vector")))
const void* vector_table[] = {
    &_estack,               /* Initial stack pointer */
    Reset_Handler,          /* Reset handler */
    NMI_Handler,            /* NMI handler */
    HardFault_Handler,      /* Hard fault handler */
    MemManage_Handler,      /* MPU fault handler */
    BusFault_Handler,       /* Bus fault handler */
    UsageFault_Handler,     /* Usage fault handler */
    0, 0, 0, 0,             /* Reserved */
    SVC_Handler,            /* SVCall handler */
    DebugMon_Handler,       /* Debug monitor handler */
    0,                      /* Reserved */
    PendSV_Handler,         /* PendSV handler */
    SysTick_Handler,        /* SysTick handler */

    /* External interrupts */
    WWDG_IRQHandler,        /* Window watchdog */
    PVD_IRQHandler,         /* PVD through EXTI */
    TAMP_STAMP_IRQHandler,  /* Tamper and timestamp */
    RTC_WKUP_IRQHandler,    /* RTC wakeup */
    FLASH_IRQHandler,       /* Flash */
    RCC_IRQHandler,         /* RCC */
    EXTI0_IRQHandler,       /* EXTI Line 0 */
    EXTI1_IRQHandler,       /* EXTI Line 1 */
    EXTI2_IRQHandler,       /* EXTI Line 2 */
    EXTI3_IRQHandler,       /* EXTI Line 3 */
    EXTI4_IRQHandler,       /* EXTI Line 4 */
    DMA1_Stream0_IRQHandler,
    DMA1_Stream1_IRQHandler,
    DMA1_Stream2_IRQHandler,
    DMA1_Stream3_IRQHandler,
    DMA1_Stream4_IRQHandler,
    DMA1_Stream5_IRQHandler,
    DMA1_Stream6_IRQHandler,
    ADC_IRQHandler,
    0, 0, 0, 0,             /* Reserved */
    0, 0, 0, 0, 0,          /* EXTI 5-9, TIM1 */
    TIM1_UP_TIM10_IRQHandler,
    0, 0,                   /* TIM1 */
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
};
