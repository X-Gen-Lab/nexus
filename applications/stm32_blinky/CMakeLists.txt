#-----------------------------------------------------------------------------
# STM32 Minimal Boot Blinky Example Application
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
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# Include common application utilities
include(ApplicationCommon)

#-----------------------------------------------------------------------------
# Application Configuration
#-----------------------------------------------------------------------------

set(APP_NAME stm32_blinky)

#-----------------------------------------------------------------------------
# Create Executable
#-----------------------------------------------------------------------------

add_executable(${APP_NAME}
    main.c
)

# Set output file suffix to .elf for ARM targets
set_target_properties(${APP_NAME}
    PROPERTIES
        SUFFIX ".elf"
)

# Include directories
target_include_directories(${APP_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with STM32 platform library (minimal dependencies)
target_link_libraries(${APP_NAME}
    PRIVATE
        platform_stm32
)

# Setup linker script
nexus_application_linker_script(${APP_NAME})

# Apply common application setup (definitions, post-build actions)
nexus_application_setup(${APP_NAME} VERSION "1.0.0")

#-----------------------------------------------------------------------------
# End of STM32 Minimal Boot Blinky Example Application
#-----------------------------------------------------------------------------
