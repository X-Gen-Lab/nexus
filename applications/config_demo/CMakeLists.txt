#-----------------------------------------------------------------------------
# Config Demo Example Application (Optimized)
#-----------------------------------------------------------------------------
# Configuration system demonstration.
#
# Features:
#   - Kconfig-based configuration
#   - Runtime parameter access
#   - Configuration validation
#
# Requirements: None
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
    TARGET config_demo
    VERSION "1.0.0"
    SOURCES main.c
    EXTRA_DEPS config_framework
)

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~50 lines
# Optimized: ~30 lines
# Reduction: 40%
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of Config Demo Example Application
#-----------------------------------------------------------------------------
