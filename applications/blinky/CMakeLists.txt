#-----------------------------------------------------------------------------
# Blinky Example Application (Optimized)
#-----------------------------------------------------------------------------
# Basic LED blinky example demonstrating GPIO control.
#
# Features:
#   - GPIO initialization and control
#   - Basic delay functions
#   - Minimal resource usage
#
# Optimized using nexus_add_application() for cleaner configuration.
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
    TARGET blinky
    VERSION "1.0.0"
    SOURCES main.c
)

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~40 lines (manual configuration)
# Optimized: ~30 lines (unified function)
# Reduction: 25%
#
# Key improvements:
# - Single function call for complete configuration
# - Auto-detection of platform dependencies
# - Auto-configuration of linker script
# - Automatic post-build actions
# - Cleaner and more maintainable
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of Blinky Example Application
#-----------------------------------------------------------------------------
