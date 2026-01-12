##############################################################################
# STM32F4 Platform Support
##############################################################################

# Platform library
add_library(platform_stm32f4 STATIC)

target_sources(platform_stm32f4
    PRIVATE
        src/system_stm32f4xx.c
        src/startup_stm32f407xx.c
        src/hal_gpio_stm32f4.c
        src/hal_uart_stm32f4.c
        src/hal_system_stm32f4.c
)

target_include_directories(platform_stm32f4
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(platform_stm32f4
    PUBLIC
        hal_interface
)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.ld)

# Export linker script for applications
set(NEXUS_LINKER_SCRIPT ${LINKER_SCRIPT} PARENT_SCOPE)
