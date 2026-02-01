#-----------------------------------------------------------------------------
# FreeRTOS Demo Application (Optimized)
#-----------------------------------------------------------------------------
# FreeRTOS OSAL adapter demonstration.
#
# Features:
#   - Multi-task creation and management
#   - Mutex for resource protection
#   - Semaphore for task synchronization
#   - Message queue for inter-task communication
#
# Requirements: FreeRTOS OSAL backend
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
    TARGET freertos_demo
    VERSION "1.0.0"
    SOURCES main.c
)

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~55 lines
# Optimized: ~30 lines
# Reduction: 45%
#
# Key improvements:
# - Single function call for complete configuration
# - Auto-detection of platform dependencies
# - Auto-linking of HAL and OSAL
# - Automatic post-build actions
# - Cleaner and more maintainable
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of FreeRTOS Demo Application
#-----------------------------------------------------------------------------
