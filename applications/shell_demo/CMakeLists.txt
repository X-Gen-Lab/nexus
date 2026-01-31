#-----------------------------------------------------------------------------
# Shell Demo Example Application
#-----------------------------------------------------------------------------
# Interactive shell demo with command-line interface.
#
# Features:
#   - Command registration and execution
#   - UART communication
#   - Built-in commands (help, version, etc.)
#
# Requirements: UART peripheral, Shell framework
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# Include common application utilities
include(ApplicationCommon)

#-----------------------------------------------------------------------------
# Application Configuration
#-----------------------------------------------------------------------------

set(APP_NAME shell_demo)

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
        shell_framework
        osal_interface
        osal
)

# Setup linker script
nexus_application_linker_script(${APP_NAME})

# Apply common application setup (definitions, post-build actions)
nexus_application_setup(${APP_NAME} VERSION "1.0.0")

#-----------------------------------------------------------------------------
# End of Shell Demo Example Application
#-----------------------------------------------------------------------------
