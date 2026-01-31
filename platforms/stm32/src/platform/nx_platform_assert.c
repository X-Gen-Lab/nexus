/**
 * \file            nx_platform_assert.c
 * \brief           STM32 platform assertion handler
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-31
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Provides assert_failed() function required by STM32 HAL
 *                  when USE_FULL_ASSERT is defined.
 */

#include <stdint.h>

#ifdef USE_FULL_ASSERT

/**
 * \brief           Reports the name of the source file and line number
 *                  where the assert_param error has occurred
 * \param[in]       file: pointer to the source file name
 * \param[in]       line: assert_param error line source number
 */
void assert_failed(uint8_t* file, uint32_t line) {
    /* User can add implementation to report the file name and line number */
    /* For example: printf("Wrong parameters value: file %s on line %d\r\n",
     * file, line) */

    /* Prevent compiler optimization */
    (void)file;
    (void)line;

    /* Infinite loop */
    while (1) {
        /* Stay here for debugging */
        __asm volatile("nop");
    }
}

#endif /* USE_FULL_ASSERT */
