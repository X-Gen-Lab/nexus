#-----------------------------------------------------------------------------
# Framework Layer CMake Configuration
#-----------------------------------------------------------------------------
# This file configures the framework layer build system with support for:
#   - Conditional compilation based on Kconfig
#   - Core framework components (log, shell, config, init)
#   - Optional framework components (event, state machine, middleware)
#
# Author: Nexus Team
#-----------------------------------------------------------------------------

message(STATUS "Configuring Framework Layer...")

#-----------------------------------------------------------------------------
# Core Framework Components
#-----------------------------------------------------------------------------

# Log framework - Logging system
if(CONFIG_FRAMEWORK_LOG)
    add_subdirectory(log)
    message(STATUS "Log Framework: Configured")
else()
    message(STATUS "Log Framework: Disabled")
endif()

# Shell framework - Command-line interface
if(CONFIG_FRAMEWORK_SHELL)
    if(NOT CONFIG_FRAMEWORK_LOG)
        message(WARNING "Shell framework requires Log framework, but it is disabled")
    endif()
    add_subdirectory(shell)
    message(STATUS "Shell Framework: Configured")
else()
    message(STATUS "Shell Framework: Disabled")
endif()

# Config Manager framework - Configuration management
if(CONFIG_FRAMEWORK_CONFIG)
    add_subdirectory(config)
    message(STATUS "Config Manager Framework: Configured")
else()
    message(STATUS "Config Manager Framework: Disabled")
endif()

# Init framework - System initialization
if(CONFIG_FRAMEWORK_INIT)
    add_subdirectory(init)
    message(STATUS "Init Framework: Configured")
else()
    message(STATUS "Init Framework: Disabled")
endif()

#-----------------------------------------------------------------------------
# Optional Framework Components
#-----------------------------------------------------------------------------

# Event framework
if(CONFIG_FRAMEWORK_EVENT)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/event)
        add_subdirectory(event)
        message(STATUS "Event Framework: Enabled")
    else()
        message(WARNING "Event framework enabled in Kconfig but directory not found")
    endif()
endif()

# State Machine framework
if(CONFIG_FRAMEWORK_STATE_MACHINE)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/state_machine)
        add_subdirectory(state_machine)
        message(STATUS "State Machine Framework: Enabled")
    else()
        message(WARNING "State Machine framework enabled in Kconfig but directory not found")
    endif()
endif()

# Middleware framework
if(CONFIG_FRAMEWORK_MIDDLEWARE)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/middleware)
        add_subdirectory(middleware)
        message(STATUS "Middleware Framework: Enabled")
    else()
        message(WARNING "Middleware framework enabled in Kconfig but directory not found")
    endif()
endif()

#-----------------------------------------------------------------------------
# Framework Debug Configuration
#-----------------------------------------------------------------------------

if(CONFIG_FRAMEWORK_DEBUG)
    # Add debug definitions to all framework components
    if(TARGET log_framework)
        target_compile_definitions(log_framework PRIVATE FRAMEWORK_DEBUG=1)
    endif()
    if(TARGET shell_framework)
        target_compile_definitions(shell_framework PRIVATE FRAMEWORK_DEBUG=1)
    endif()
    if(TARGET config_framework)
        target_compile_definitions(config_framework PRIVATE FRAMEWORK_DEBUG=1)
    endif()
    if(TARGET init_framework)
        target_compile_definitions(init_framework PRIVATE FRAMEWORK_DEBUG=1)
    endif()
    message(STATUS "Framework Debug: Enabled")
endif()

message(STATUS "Framework Layer: Configured")

#-----------------------------------------------------------------------------
# End of Framework Layer
#-----------------------------------------------------------------------------
