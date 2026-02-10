#-----------------------------------------------------------------------------
# STM32 Configuration Test Application
#-----------------------------------------------------------------------------
# Test application to validate Kconfig configuration system.
#
# Features:
#   - Display all Kconfig settings
#   - Validate chip configuration
#   - Test GPIO and UART configuration
#   - LED blinking with SysTick
#
# Requirements: STM32 HAL library
#
# Author: Nexus Team
# Version: 1.0.0
# Date: 2026-02-08
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

#-----------------------------------------------------------------------------
# Create Application
#-----------------------------------------------------------------------------

nexus_add_application(
    TARGET stm32_config_test
    VERSION "1.0.0"
    SOURCES
        main.c
        config_display.c
        config_display.h
    PLATFORM_DEPS platform_stm32
)

#-----------------------------------------------------------------------------
# End of STM32 Configuration Test Application
#-----------------------------------------------------------------------------
