/**
 * \file            config_display.h
 * \brief           Configuration display module for STM32 config test
 * \author          Nexus Team
 */

#ifndef CONFIG_DISPLAY_H
#define CONFIG_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Print all configuration information
 * \details         Displays platform, interrupt, and peripheral configs
 */
void config_display_print_all(void);

/**
 * \brief           Print configuration summary
 * \details         Displays brief system status information
 */
void config_display_print_summary(void);

/**
 * \brief           Print platform configuration
 * \details         Displays system clock, HSE, stack, and heap settings
 */
void config_display_print_platform(void);

/**
 * \brief           Print interrupt configuration
 * \details         Displays NVIC priority group and interrupt priorities
 */
void config_display_print_interrupt(void);

/**
 * \brief           Print peripheral configuration
 * \details         Displays UART and other peripheral settings
 */
void config_display_print_peripheral(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DISPLAY_H */
