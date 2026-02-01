#-----------------------------------------------------------------------------
# Framework Layer Configuration (Optimized)
#-----------------------------------------------------------------------------
# Configures framework components with conditional compilation based on Kconfig.
# Uses table-driven approach for cleaner component management.
#
# Author: Nexus Team
# Version: 2.0.0 (Optimized)
# Date: 2026-02-01
#-----------------------------------------------------------------------------

message(STATUS "=== Framework Layer Configuration ===")

#-----------------------------------------------------------------------------
# Core Framework Components
#-----------------------------------------------------------------------------

# Define core component mapping: Kconfig flag -> directory -> display name -> target
set(CORE_COMPONENTS
    "CONFIG_FRAMEWORK_LOG:log:Log:log_framework"
    "CONFIG_FRAMEWORK_SHELL:shell:Shell:shell_framework"
    "CONFIG_FRAMEWORK_CONFIG:config:Config Manager:config_framework"
    "CONFIG_FRAMEWORK_INIT:init:Init:init_framework"
)

# Track enabled components
set(ENABLED_COMPONENTS "")

# Process core components
foreach(COMPONENT ${CORE_COMPONENTS})
    string(REPLACE ":" ";" COMPONENT_PARTS ${COMPONENT})
    list(GET COMPONENT_PARTS 0 CONFIG_FLAG)
    list(GET COMPONENT_PARTS 1 COMPONENT_DIR)
    list(GET COMPONENT_PARTS 2 COMPONENT_NAME)
    list(GET COMPONENT_PARTS 3 COMPONENT_TARGET)

    if(${CONFIG_FLAG})
        # Check for dependencies
        if(COMPONENT_DIR STREQUAL "shell" AND NOT CONFIG_FRAMEWORK_LOG)
            nexus_warning("${COMPONENT_NAME} framework requires Log framework, but it is disabled")
        endif()

        # Add subdirectory
        add_subdirectory(${COMPONENT_DIR})
        list(APPEND ENABLED_COMPONENTS ${COMPONENT_NAME})
        nexus_log(STATUS "${COMPONENT_NAME} framework configured")
    else()
        nexus_log(VERBOSE "${COMPONENT_NAME} framework disabled")
    endif()
endforeach()

#-----------------------------------------------------------------------------
# Optional Framework Components
#-----------------------------------------------------------------------------

# Define optional component mapping
set(OPTIONAL_COMPONENTS
    "CONFIG_FRAMEWORK_EVENT:event:Event"
    "CONFIG_FRAMEWORK_STATE_MACHINE:state_machine:State Machine"
    "CONFIG_FRAMEWORK_MIDDLEWARE:middleware:Middleware"
)

# Process optional components
foreach(COMPONENT ${OPTIONAL_COMPONENTS})
    string(REPLACE ":" ";" COMPONENT_PARTS ${COMPONENT})
    list(GET COMPONENT_PARTS 0 CONFIG_FLAG)
    list(GET COMPONENT_PARTS 1 COMPONENT_DIR)
    list(GET COMPONENT_PARTS 2 COMPONENT_NAME)

    if(${CONFIG_FLAG})
        set(COMPONENT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${COMPONENT_DIR}")
        if(EXISTS ${COMPONENT_PATH})
            add_subdirectory(${COMPONENT_DIR})
            list(APPEND ENABLED_COMPONENTS ${COMPONENT_NAME})
            nexus_log(STATUS "${COMPONENT_NAME} framework enabled")
        else()
            nexus_warning("${COMPONENT_NAME} framework enabled in Kconfig but directory not found: ${COMPONENT_PATH}")
        endif()
    endif()
endforeach()

#-----------------------------------------------------------------------------
# Framework Debug Configuration
#-----------------------------------------------------------------------------

if(CONFIG_FRAMEWORK_DEBUG)
    # Define targets that support debug mode
    set(DEBUG_TARGETS
        log_framework
        shell_framework
        config_framework
        init_framework
    )

    # Apply debug definitions
    foreach(TARGET ${DEBUG_TARGETS})
        if(TARGET ${TARGET})
            target_compile_definitions(${TARGET} PRIVATE FRAMEWORK_DEBUG=1)
        endif()
    endforeach()

    nexus_log(STATUS "Framework debug mode enabled")
endif()

#-----------------------------------------------------------------------------
# Configuration Summary
#-----------------------------------------------------------------------------

list(LENGTH ENABLED_COMPONENTS COMPONENT_COUNT)

message(STATUS "")
message(STATUS "Framework Layer Summary:")
message(STATUS "  Enabled Components: ${COMPONENT_COUNT}")
if(ENABLED_COMPONENTS)
    foreach(COMPONENT ${ENABLED_COMPONENTS})
        message(STATUS "    - ${COMPONENT}")
    endforeach()
else()
    message(STATUS "    (none)")
endif()
if(CONFIG_FRAMEWORK_DEBUG)
    message(STATUS "  Debug Mode:         Enabled")
endif()
message(STATUS "")

message(STATUS "=== Framework Layer Configuration Complete ===")

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~100 lines (manual if-endif blocks)
# Optimized: ~110 lines (table-driven + documentation)
# Net change: +10 lines (better structure and logging)
#
# Key improvements:
# - Table-driven component selection
# - Unified component processing loop
# - Better dependency checking
# - Cleaner logging with nexus_log()
# - Easier to add new components (just add to mapping table)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of Framework Layer Configuration
#-----------------------------------------------------------------------------
