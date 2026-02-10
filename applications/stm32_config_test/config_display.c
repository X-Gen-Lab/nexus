/**
 * \file            config_display.c
 * \brief           Configuration display implementation
 * \author          Nexus Team
 */

#include "config_display.h"
#include "nexus_config.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Print section header
 * \param[in]       title: Section title string
 */
static void print_section_header(const char* title) {
    printf("\n=== %s ===\n", title);
}

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Print platform configuration
 */
void config_display_print_platform(void) {
    print_section_header("Platform Configuration");

#ifdef NX_CONFIG_STM32_SYSCLK_FREQ
    printf("System Clock: %lu Hz\n",
           (unsigned long)NX_CONFIG_STM32_SYSCLK_FREQ);
#else
    printf("System Clock: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_HSE_VALUE
    printf("HSE Value: %lu Hz\n", (unsigned long)NX_CONFIG_STM32_HSE_VALUE);
#else
    printf("HSE Value: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_STACK_SIZE
    printf("Stack Size: 0x%lX bytes\n",
           (unsigned long)NX_CONFIG_STM32_STACK_SIZE);
#else
    printf("Stack Size: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_HEAP_SIZE
    printf("Heap Size: 0x%lX bytes\n",
           (unsigned long)NX_CONFIG_STM32_HEAP_SIZE);
#else
    printf("Heap Size: Not configured\n");
#endif
}

/**
 * \brief           Print interrupt configuration
 */
void config_display_print_interrupt(void) {
    print_section_header("Interrupt Configuration");

#ifdef NX_CONFIG_STM32_NVIC_PRIORITY_GROUP
    printf("NVIC Priority Group: %d\n", NX_CONFIG_STM32_NVIC_PRIORITY_GROUP);
#else
    printf("NVIC Priority Group: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_SYSTICK_PRIORITY
    printf("SysTick Priority: %d\n", NX_CONFIG_STM32_SYSTICK_PRIORITY);
#else
    printf("SysTick Priority: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_PENDSV_PRIORITY
    printf("PendSV Priority: %d\n", NX_CONFIG_STM32_PENDSV_PRIORITY);
#else
    printf("PendSV Priority: Not configured\n");
#endif
}

/**
 * \brief           Print peripheral configuration
 */
void config_display_print_peripheral(void) {
    print_section_header("Peripheral Configuration");

    /* UART1 configuration */
#ifdef NX_CONFIG_STM32_UART1_BAUDRATE
    printf("UART1 Baudrate: %lu\n",
           (unsigned long)NX_CONFIG_STM32_UART1_BAUDRATE);
#else
    printf("UART1 Baudrate: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_UART1_TX_BUFFER_SIZE
    printf("UART1 TX Buffer: %lu bytes\n",
           (unsigned long)NX_CONFIG_STM32_UART1_TX_BUFFER_SIZE);
#else
    printf("UART1 TX Buffer: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_UART1_RX_BUFFER_SIZE
    printf("UART1 RX Buffer: %lu bytes\n",
           (unsigned long)NX_CONFIG_STM32_UART1_RX_BUFFER_SIZE);
#else
    printf("UART1 RX Buffer: Not configured\n");
#endif

#ifdef NX_CONFIG_STM32_UART_BUFFER_OVERFLOW_POLICY
    printf("UART Overflow Policy: %d\n",
           NX_CONFIG_STM32_UART_BUFFER_OVERFLOW_POLICY);
#else
    printf("UART Overflow Policy: Not configured\n");
#endif
}

/**
 * \brief           Print all configuration information
 */
void config_display_print_all(void) {
    printf("\n");
    printf("========================================\n");
    printf("  STM32 Configuration Test Application\n");
    printf("========================================\n");

    config_display_print_platform();
    config_display_print_interrupt();
    config_display_print_peripheral();

    printf("\n========================================\n\n");
}

/**
 * \brief           Print configuration summary
 */
void config_display_print_summary(void) {
    printf("[INFO] System running with Kconfig settings\n");

#ifdef NX_CONFIG_STM32_SYSCLK_FREQ
    printf("[INFO] SYSCLK: %lu Hz\n",
           (unsigned long)NX_CONFIG_STM32_SYSCLK_FREQ);
#endif

#ifdef NX_CONFIG_STM32_NVIC_PRIORITY_GROUP
    printf("[INFO] NVIC Group: %d\n", NX_CONFIG_STM32_NVIC_PRIORITY_GROUP);
#endif
}
