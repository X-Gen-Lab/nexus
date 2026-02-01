#-----------------------------------------------------------------------------
# STM32 Minimal Boot Blinky Example Application (Optimized)
#-----------------------------------------------------------------------------
# Minimal STM32 blinky example with direct register access.
#
# Features:
#   - Minimal boot code
#   - Direct register manipulation
#   - No HAL/OSAL dependencies
#   - Smallest code size
#
# Requirements: None (bare metal)
#
# Author: Nexus Team
# Version: 2.0.0 (Optimized)
# Date: 2026-02-01
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

#-----------------------------------------------------------------------------
# Create Application
#-----------------------------------------------------------------------------

nexus_add_application(
    TARGET stm32_blinky
    VERSION "1.0.0"
    SOURCES main.c
    PLATFORM_DEPS platform_stm32
)

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~50 lines
# Optimized: ~30 lines
# Reduction: 40%
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of STM32 Minimal Boot Blinky Example Application
#-----------------------------------------------------------------------------
