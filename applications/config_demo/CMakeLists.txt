#-----------------------------------------------------------------------------
# Config Demo Example Application
#-----------------------------------------------------------------------------
# Configuration system demonstration.
#
# Features:
#   - Kconfig-based configuration
#   - Runtime parameter access
#   - Configuration validation
#
# Requirements: None
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# Include common application utilities
include(ApplicationCommon)

#-----------------------------------------------------------------------------
# Application Configuration
#-----------------------------------------------------------------------------

set(APP_NAME config_demo)

#-----------------------------------------------------------------------------
# Create Executable
#-----------------------------------------------------------------------------

add_executable(${APP_NAME}
    main.c
)

# Include directories
target_include_directories(${APP_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(${APP_NAME}
    PRIVATE
        platform_stm32
        hal_interface
        hal
        config_framework
        osal_interface
        osal
)

# Setup linker script
nexus_application_linker_script(${APP_NAME})

# Apply common application setup (definitions, post-build actions)
nexus_application_setup(${APP_NAME} VERSION "1.0.0")

#-----------------------------------------------------------------------------
# End of Config Demo Example Application
#-----------------------------------------------------------------------------
