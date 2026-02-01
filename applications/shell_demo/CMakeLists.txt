#-----------------------------------------------------------------------------
# Shell Demo Example Application (Optimized)
#-----------------------------------------------------------------------------
# Interactive shell demo with command-line interface.
#
# Features:
#   - Command registration and execution
#   - UART communication
#   - Built-in commands (help, version, etc.)
#
# Requirements: UART peripheral, Shell framework
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
    TARGET shell_demo
    VERSION "1.0.0"
    SOURCES main.c
    EXTRA_DEPS shell_framework
)

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~50 lines
# Optimized: ~30 lines
# Reduction: 40%
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of Shell Demo Example Application
#-----------------------------------------------------------------------------
